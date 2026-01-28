export module cpbuild;
export import utility.process;
export import dependency.cache;
using std::chrono::file_clock;
using std::filesystem::current_path,std::filesystem::file_time_type,std::filesystem::path,std::filesystem::recursive_directory_iterator;
using namespace std;
using namespace chrono_literals;
export struct ModuleCompilation
{
	vector<ImportUnit>dependency;
	path preprocessed;
	path output;
	file_time_type source;
	file_time_type object;
	vector<unsigned>dependentProcesses;
	string name;
	ModuleCompilation()=default;
	ModuleCompilation(ModuleData data,file_time_type sourceModify,path preprocessed,path out)
		:dependency(std::move(data.imports)),preprocessed(std::move(preprocessed)),output(std::move(out)),source(sourceModify),object(exists(output)?last_write_time(output):sourceModify-1s),dependentProcesses(),name(std::move(data.name))
	{}
};
export struct FileModuleMap
{
	unordered_map<path,ModuleCompilation>m;
	auto insert(path in,path out,path preprocessed,ModuleData data)
	{
		const file_time_type lastModify=last_write_time(in);
		return m.emplace(std::move(in),ModuleCompilation(std::move(data),lastModify,std::move(preprocessed),std::move(out)));
	}
};
export struct ModuleConnection
{
	FileModuleMap files;
	unordered_map<string,path>primaryModuleInterfaceUnits;
};
export class ProgramBuilder;
extern unique_ptr<ProgramBuilder>singletonPointerProgramBuilder;
struct ProgramBuilderConstructorTag{};
class ProgramBuilder
{
	BuildConfiguration options;
	vector<string>linkerArguments;
	ForwardGraph graph;
	unique_ptr<BaseCompilerConfigurer>compiler;
	ProjectGraph back;
	ParallelProcessManager pm;
public:
	ProgramBuilder(ProgramBuilderConstructorTag,BuildConfiguration op)
		noexcept
		:options(std::move(op)),linkerArguments(),graph(),compiler(getCompiler(options,&pm)),back(options,*compiler),pm(options.threadCount)
	{}
	ProgramBuilder()=delete;
	ProgramBuilder(const ProgramBuilder&)=delete;
	ProgramBuilder(ProgramBuilder&&)=delete;
	ProgramBuilder&operator=(const ProgramBuilder&)=delete;
	ProgramBuilder&operator=(ProgramBuilder&&)=delete;
	path getOutputFile(path p)
		const noexcept
	{
		return replaceMove(options,std::move(p),path{"o"});
	}
	path getFinalOutputFile()
		const noexcept
	{
		if(options.artifact().size()==0)
		{
			if(options.targets.size()&&is_regular_file(path{options.targets.front()}))
			{
				path output(options.targets.front());
				output.replace_extension();
				return output;
			}
			else
			{
				return absolute(current_path()).filename();
			}
		}
		else
		{
			return path{options.artifact()};
		}
	}
	void add_directory(const path&p,bool externalDirectory=false)
	{
		using namespace string_view_literals;
		set<path>directoriesToCreate;
		if(!externalDirectory&&options.objectDirectory().size())
		{
			path toBeCreated(getOutputFile(p));
			create_directory(toBeCreated);
		}
		for(const auto&en:recursive_directory_iterator(p))
		{
			path current=en.path();
			if(current.has_extension()&&isExtensionPermitted(current))
			{
				back.addFile(path(current),externalDirectory);
				if(!externalDirectory&&options.objectDirectory().size())
				{
					directoriesToCreate.insert(getOutputFile(current.remove_filename()));
				}
			}
		}
		for(const path&d:directoriesToCreate)
		{
			create_directory(d);
		}
	}
	bool loadedDependencies()
	{
		bool loaded=false;
		if(options.dependencyCache().size())
		{
			ifstream ifs(static_cast<string>(options.dependencyCache()));
			loaded=ifs.is_open();
			if(loaded)
			{
				if(parseDependencies(back,ifs))
				{
					back.buildModuleMap();
					back.convertDependenciesToPath();
					resolveUnresolvedDependencies(back);
				}
			}
			//println("loaded {}",loaded);
		}
		return loaded;
	}
	void cacheDependencies()
		const
	{
		ofstream ofs(string{options.dependencyCache()});
		dumpDependencies(back,ofs);
	}
	void cpbuild()
	{
		const string EMPTYSTRING="";
		compiler->addArguments();
		//println("{}",targets);
		bool dependencyHasBeenLoaded=loadedDependencies();
		if(!dependencyHasBeenLoaded)
		{
			for(path t:options.targets)
			{
				if(is_directory(t))
				{
					add_directory(t);
				}
				else
				{
					back.addFile(std::move(t),false);
				}
			}
			if(options.moduleMapCache().size())
			{
				ifstream fin(string{options.moduleMapCache()});
				string line;
				while(getline(fin,line))
				{
					if(line.size())
					{
						back.addFile(path{line},true);
					}
				}
				unsigned stdModules=back.checkForStandardModules();
				bool needStd=!(stdModules&1);
				bool needStdCompat=!(stdModules>>1&1);
				if(needStd)
				{
					back.addFile(compiler->getSTDModulePath(),true);
				}
				if(needStdCompat)
				{
					back.addFile(compiler->getSTDCompatModulePath(),true);
				}
			}
			else
			{
				if(compiler->getCompilerType()==LLVM)
				{
					back.addFile(path{compiler->getSTDModulePath()},true);
					back.addFile(path{compiler->getSTDCompatModulePath()},true);
				}
			}
			back.convertDependenciesToPath();
			resolveUnresolvedDependencies(back);
		}
		back.convertDependenciesToPath(true);
		if(options.dependencyCache().size())
		{
			cacheDependencies();
		}
		if(options.isDumpDependencyGraph())
		{
			for(const auto&[p,data]:back)
			{
				auto importstr=views::join(views::transform(data.depend,[](const ImportUnit&unit){return unit.name+'\n';}));
				println("{}\n{}",p,ranges::to<string>(importstr));
			}
		}
		/*for(const auto&[p,data]:back)
		{
			println("{} {}",p,views::transform(data.depend,&ImportUnit::name));
		}*/
		graph=makeForwardGraph(back,options.isForceRecompile(),options.isForceRecompileEnhanced());
		queue<ForwardGraphNode>compileQueue=buildInitialCompileQueue(graph);
		/*for(const auto&[node,data]:graph.internal())
		{
			println("node {}\n{}",node.name,views::transform(data.dependent,&ForwardGraphNode::name));
		}*/
		unordered_map<unsigned,ForwardGraphNode>processToNode;
		while(!pm.is_empty()||compileQueue.size())
		{
			if(!pm.is_full()&&compileQueue.size())
			{
				ForwardGraphNode node=std::move(compileQueue.front());
				compileQueue.pop();
				const ForwardGraphNodeData&data=graph.at(node);
				path cmi=compiler->headerNameToOutput(node.name);
				//path cmi=replaceMove(options,path{node.name},path{"pcm"});
				auto dataPairO=back.query(node.name);
				auto requiredTrioO=dataPairO.transform([](const pair<const string,FileData>*m){return dataToTrio(m->second);});
				auto requiredTrio=requiredTrioO.value_or(tuple<const string&,const path&,span<const ImportUnit>>{EMPTYSTRING,cmi,span<const ImportUnit>{static_cast<const ImportUnit*>(nullptr),0}});
				if(!node.external&&!node.header)
				{
					linkerArguments.push_back(get<1>(requiredTrio).string());
				}
				if(data.recompile)
				{
					string outputfile=node.external&&!node.header?"/dev/null":get<1>(requiredTrio).string();
					path emptypath;
					const path&pp=node.header?emptypath:dataPairO.value()->second.preprocessed;
					string pps=pp.string();
					bool exchange=pps.size();
					if(exchange)
					{
						swap(pps,node.name);
					}
					auto opid=compiler->compile({node.name,outputfile,get<0>(requiredTrio)},get<2>(requiredTrio),node.notInterface,node.header);
					if(exchange)
					{
						swap(pps,node.name);
					}
					if(opid)
					{
						processToNode.insert({*opid,std::move(node)});
					}
					else
					{
						println(cerr,"Compiling {} failed",node.name);
					}
				}
				else if(options.isDisplayCommand())
				{
					println("{} does not need to be recompiled",node.name);
				}
				for(const auto&other:data.dependent)
				{
					ForwardGraphNodeData&otherData=graph.at(other);
					otherData.recompile=otherData.recompile||data.recompile;
					otherData.remaining-=!data.recompile;
					if(otherData.remaining==0)
					{
						compileQueue.push(other);
					}
				}
			}
			else
			{
				auto opid=pm.wait_any_process();
				if(opid)
				{
					const ForwardGraphNode&node=processToNode.at(*opid);
					const ForwardGraphNodeData&data=graph.at(node);
					for(const auto&other:data.dependent)
					{
						ForwardGraphNodeData&otherData=graph.at(other);
						if(--otherData.remaining==0)
						{
							compileQueue.push(other);
						}
					}
				}
			}
		}
		pm.wait_remaining_processes();
		string product=getFinalOutputFile().string();
		compiler->link(product,linkerArguments);
		pm.wait_remaining_processes();
	}
	~ProgramBuilder()=default;
	static ProgramBuilder&getInstance(BuildConfiguration options)
		noexcept
	{
		if(!singletonPointerProgramBuilder)
		{
			singletonPointerProgramBuilder=make_unique<ProgramBuilder>(ProgramBuilderConstructorTag{},std::move(options));
		}
		return*singletonPointerProgramBuilder;
	}
};
unique_ptr<ProgramBuilder>singletonPointerProgramBuilder;
