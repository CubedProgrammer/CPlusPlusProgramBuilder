export module cpbuild;
export import graph.back;
export import utility.process;
export import dependency.cache;
using std::chrono::file_clock;
using std::filesystem::current_path,std::filesystem::file_time_type,std::filesystem::path,std::filesystem::recursive_directory_iterator;
using std::views::keys,std::views::zip;
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
	ProgramBuilder(ProgramBuilderConstructorTag,unique_ptr<BaseCompilerConfigurer>&&c,BuildConfiguration op)
		noexcept
		:options(std::move(op)),linkerArguments(),graph(),compiler(std::move(c)),back(options,*compiler),pm(options.threadCount)
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
	auto add_file(path&&p,bool externalDirectory=false)
	{
		return back.addFile(p,externalDirectory);
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
				add_file(path(current),externalDirectory);
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
				parseDependencies(back,ifs);
			}
			println("loaded {}",loaded);
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
		constexpr string EMPTYSTRING="";
		span<string_view>targets=options.targets;
		compiler->addArguments(options);
		println("{}",targets);
		bool dependencyHasBeenLoaded=loadedDependencies();
		if(!dependencyHasBeenLoaded)
		{
			for(path t:targets)
			{
				if(is_directory(t))
				{
					add_directory(t);
				}
				else
				{
					add_file(std::move(t));
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
						add_file(path{line},true);
					}
				}
				unsigned stdModules=back.checkForStandardModules();
				bool needStd=!(stdModules&1);
				bool needStdCompat=!(stdModules>>1&1);
				if(needStd)
				{
					add_file(compiler->getSTDModulePath(),true);
				}
				if(needStdCompat)
				{
					add_file(compiler->getSTDCompatModulePath(),true);
				}
			}
			else
			{
				if(compiler->getCompilerType()==LLVM)
				{
					add_file(path{compiler->getSTDModulePath()},true);
					add_file(path{compiler->getSTDCompatModulePath()},true);
				}
			}
			back.convertDependenciesToPath();
		}
		unordered_set<string_view>unresolvedImports;
		for(const auto&[filepath,filedata]:back)
		{
			for(const auto&[i,resolved]:filedata.dependResolved())
			{
				if(!resolved)
				{
					vector<path>potential=compiler->searchForLikelyCandidates(i.name);
					bool found=false;
					for(path&p:potential)
					{
						auto itO=add_file(std::move(p),true);
						if(itO)
						{
							auto&it1=*itO;
							if(it1->second.module==i.name)
							{
								found=true;
								break;
							}
							else
							{
								back.erase(it1);
							}
						}
					}
					if(!found)
					{
						unresolvedImports.insert(i.name);
					}
				}
			}
		}
		auto filesToTry=ranges::to<vector<path>>(compiler->getPotentialModuleFiles());
		for(string_view sv:unresolvedImports)
		{
			println("unresolved {}",sv);
			auto scores=views::transform(filesToTry,[sv](const path&m){string n=m.stem().string();return similarity(sv,n);});
			auto scoresVector=ranges::to<vector<ModulePathSimilarity>>(scores);
			ranges::sort(zip(scoresVector,filesToTry),ranges::greater());
			for(path t:filesToTry)
			{
				string ts=t.stem().string();
				auto sim=similarity(sv,ts);
				println("{} {} {}",ts,sim.lcs,sim.remaining);
				auto iteratorOpt=add_file(std::move(t),true);
				if(iteratorOpt)
				{
					auto&it1=*iteratorOpt;
					if(it1->second.module==sv)
					{
						println("found {}",t.string());
						break;
					}
					else
					{
						back.erase(it1);
					}
				}
			}
		}
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
		for(const auto&[p,data]:back)
		{
			println("{} {}",p,views::transform(data.depend,&ImportUnit::name));
		}
		graph=makeForwardGraph(back,options.isForceRecompile(),options.isForceRecompileEnhanced());
		queue<ForwardGraphNode>compileQueue=buildInitialCompileQueue(graph);
		for(const auto&[node,data]:graph.internal())
		{
			println("node {}\n{}",node.name,views::transform(data.dependent,&ForwardGraphNode::name));
		}
		unordered_map<unsigned,ForwardGraphNode>processToNode;
		while(!pm.is_empty()||compileQueue.size())
		{
			if(!pm.is_full()&&compileQueue.size())
			{
				ForwardGraphNode node=std::move(compileQueue.front());
				compileQueue.pop();
				const ForwardGraphNodeData&data=graph.at(node);
				path cmi=replaceMove(options,path{node.name},path{"pcm"});
				auto dataPairO=back.query(node.name);
				auto requiredTrioO=dataPairO.transform([](const pair<const string,FileData>*m){return dataToTrio(m->second);});
				auto requiredTrio=requiredTrioO.value_or(tuple<const string&,const path&,span<const ImportUnit>>{EMPTYSTRING,cmi,span<const ImportUnit>{static_cast<const ImportUnit*>(nullptr),0}});
				//const ModuleCompilation&mc=node.header?headers.m.at({node.name}):node.external?external.files.m.at(path{node.name}):internal.files.m.at(path{node.name});
				if(!node.external&&!node.header)
				{
					linkerArguments.push_back(get<1>(requiredTrio).string());
				}
				if(data.recompile)
				{
					if(options.isDisplayCommand())
					{
						println("Compiling {}",node.name);
					}
					string outputfile=node.external&&!node.header?"/dev/null":get<1>(requiredTrio).string();
					path emptypath;
					const path&pp=node.header?emptypath:dataPairO.value()->second.preprocessed;
					string pps=pp.string();
					bool exchange=pps.size();
					if(exchange)
					{
						swap(pps,node.name);
					}
					auto arguments=compiler->getCompileCommand(node.name,outputfile,get<0>(requiredTrio),get<2>(requiredTrio),options,node.notInterface,node.header);
					auto opid=pm.run(arguments,options.isDisplayCommand());
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
		auto arguments=compiler->linkProgram(product,options,linkerArguments);
		pm.run(arguments,options.isDisplayCommand());
		pm.wait_remaining_processes();
	}
	~ProgramBuilder()=default;
	static ProgramBuilder&getInstance(unique_ptr<BaseCompilerConfigurer>&&compiler,BuildConfiguration options)
		noexcept
	{
		if(!singletonPointerProgramBuilder)
		{
			singletonPointerProgramBuilder=make_unique<ProgramBuilder>(ProgramBuilderConstructorTag{},std::move(compiler),std::move(options));
		}
		return*singletonPointerProgramBuilder;
	}
};
unique_ptr<ProgramBuilder>singletonPointerProgramBuilder;
