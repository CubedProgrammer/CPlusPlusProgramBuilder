export module cpbuild;
export import graph;
export import process;
using std::chrono::file_clock;
using std::filesystem::current_path,std::filesystem::file_time_type,std::filesystem::path,std::filesystem::recursive_directory_iterator;
using namespace std;
using namespace chrono_literals;
constexpr string_view CBP_ALLOWED_EXTENSIONS="c++ c++m cc ccm cpp cppm cxx cxxm";
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
export struct ForwardGraphNode
{
	string name;
	bool notInterface;
	bool header;
	bool external;
	constexpr bool operator==(const ForwardGraphNode&)const noexcept=default;
};
namespace std
{
	template<>
	struct hash<ForwardGraphNode>
	{
		constexpr size_t operator()(const ForwardGraphNode&object)
			const noexcept
		{
			hash<string>shasher;
			return shasher(object.name)*size_t((int)object.header+(int)object.external+1);
		}
	};
}
struct ForwardGraphNodeData
{
	vector<ForwardGraphNode>dependent;
	uint16_t remaining;
	bool recompile;
};
export using ForwardGraph=unordered_map<ForwardGraphNode,ForwardGraphNodeData>;
export class ProgramBuilder;
extern unique_ptr<ProgramBuilder>singletonPointerProgramBuilder;
struct ProgramBuilderConstructorTag{};
class ProgramBuilder
{
	BuildConfiguration options;
	vector<string>linkerArguments;
	ModuleConnection internal;
	ModuleConnection external;
	ForwardGraph graph;
	CompilerConfigurer flagger;
	ProjectGraph back;
	ParallelProcessManager pm;
public:
	ProgramBuilder(ProgramBuilderConstructorTag,pair<CompilerType,vector<string>>tai,BuildConfiguration op)
		noexcept
		:options(std::move(op)),linkerArguments(),internal(),graph(),flagger(tai.first,std::move(tai.second)),back(options,flagger),pm(op.threadCount)
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
	void add_file(path&&p,bool externalDirectory=false)
	{
		back.addFile(p,externalDirectory);
		ModuleConnection&connection=externalDirectory?external:internal;
		if(!connection.files.m.contains(p))
		{
			optional<path>preprocessed=preprocess(options,p);
			if(preprocessed)
			{
				path toscan=std::move(*preprocessed);
				ModuleData data=parseModuleData(options,toscan);
				path object=externalDirectory?path{flagger.moduleNameToFile(data.name,options.objectDirectory())}:getOutputFile(p);
				if(!object.empty())
				{
					connection.primaryModuleInterfaceUnits.emplace(data.name,p);
					connection.files.insert(std::move(p),std::move(object),std::move(toscan),std::move(data));
				}
			}
		}
	}
	void add_directory(const path&p,bool externalDirectory=false)
	{
		using namespace string_view_literals;
		auto permitted=ranges::to<vector<string>>(views::split(CBP_ALLOWED_EXTENSIONS," "sv));
		set<path>directoriesToCreate;
		if(!externalDirectory&&options.objectDirectory().size())
		{
			path toBeCreated(getOutputFile(p));
			create_directory(toBeCreated);
		}
		for(const auto&en:recursive_directory_iterator(p))
		{
			path current=en.path();
			if(current.has_extension()&&ranges::contains(permitted,current.extension().string().substr(1))&&en.is_regular_file())
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
	void parseDependencies(istream&in)
	{
		string ln;
		string current;
		while(getline(in,ln))
		{
			if(ln.size()>0)
			{
				size_t ind=ln.find(',');
				if(ind==string::npos)
				{
					current=std::move(ln);
				}
				else
				{
					string name(ln.substr(0,ind));
					string path(ln.substr(ind+1));
				}
			}
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
				parseDependencies(ifs);
			}
		}
		return loaded;
	}
	void cacheDependencies()
		const
	{
		constexpr char OPENING='[';
		constexpr char CLOSING=']';
		ofstream ofs(string{options.dependencyCache()});
		for(const auto&[filepath,mc]:internal.files.m)
		{
			println(ofs,"{}\n{}",mc.name,OPENING);
			println(ofs,"{}\nfalse",filepath.string());
			for(const auto&i:mc.dependency)
			{
				auto it = internal.primaryModuleInterfaceUnits.find(i.name);
				if(it!=internal.primaryModuleInterfaceUnits.end())
				{
					println(ofs,"{},{}",i.name,it->second.string());
				}
				else
				{
					println(ofs,"{},{}",i.name,external.primaryModuleInterfaceUnits.at(i.name).string());
				}
			}
			print(ofs,"{}\n",CLOSING);
		}
	}
	void cpbuild()
	{
		span<string_view>targets=options.targets;
		flagger.addArguments(options);
		println("{}",targets);
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
			path stdPath(flagger.getSTDModulePath());
			path stdCompatPath(flagger.getSTDCompatModulePath());
			auto[stdIt,stdInserted]=external.primaryModuleInterfaceUnits.insert({"std",stdPath});
			auto[stdCompatIt,stdCompatInserted]=external.primaryModuleInterfaceUnits.insert({"std.compat",stdPath});
			if(stdInserted)
			{
				ModuleData data=parseModuleData(options,stdPath);
				path pp=replaceMove(options,stdPath,path{"ii"});
				external.files.insert(std::move(stdPath),path{flagger.moduleNameToFile("std",options.objectDirectory())},pp,std::move(data));
			}
			if(stdCompatInserted)
			{
				ModuleData data=parseModuleData(options,stdCompatPath);
				path pp=replaceMove(options,stdCompatPath,path{"ii"});
				external.files.insert(std::move(stdCompatPath),path{flagger.moduleNameToFile("std.compat",options.objectDirectory())},pp,std::move(data));
			}
		}
		else
		{
			if(flagger.getCompilerType()==LLVM)
			{
				add_file(path{flagger.getSTDModulePath()},true);
				add_file(path{flagger.getSTDCompatModulePath()},true);
			}
			for(path t:flagger.getIncludeDirectories())
			{
				add_directory(t,true);
			}
		}
		if(options.isDumpDependencyGraph())
		{
			for(const auto&[p,mc]:internal.files.m)
			{
				auto importstr=views::join(views::transform(mc.dependency,[](const ImportUnit&unit){return unit.name+'\n';}));
				println("{}\n{}",p.string(),ranges::to<string>(importstr));
			}
		}
		if(options.dependencyCache().size())
		{
			cacheDependencies();
		}
		back.convertDependenciesToPath();
		for(const auto&[p,data]:back)
		{
			println("{} {}",p,views::transform(data.depend,&ImportUnit::name));
		}
		queue<path>externalImports;
		unordered_set<path>externalImportVisited;
		FileModuleMap headers;
		auto interfaceEndIt=internal.primaryModuleInterfaceUnits.end();
		for(const auto&[filepath,mc]:internal.files.m)
		{
			ForwardGraphNode current{filepath.string(),mc.name.size()==0,false,false};
			bool toCompile=options.isForceRecompile()||mc.source>mc.object;
			auto[itToCurrent,succ]=graph.insert({current,{{},static_cast<uint16_t>(mc.dependency.size()),toCompile}});
			if(!succ)
			{
				itToCurrent->second.remaining=mc.dependency.size();
				itToCurrent->second.recompile=toCompile;
			}
			for(const auto&i:mc.dependency)
			{
				optional<string>name;
				bool isExternal=false;
				//println("{} imports {}",filepath.string(),i.name);
				if(i.type==MODULE)
				{
					auto interfaceIt=internal.primaryModuleInterfaceUnits.find(i.name);
					isExternal=interfaceIt==interfaceEndIt;
					if(isExternal)
					{
						auto externalIt=external.primaryModuleInterfaceUnits.find(i.name);
						if(externalIt!=external.primaryModuleInterfaceUnits.end())
						{
							path namepath(externalIt->second);
							name=namepath.string();
							if(externalImportVisited.insert(namepath).second)
							{
								externalImports.push(std::move(namepath));
							}
						}
					}
					else
					{
						name=interfaceIt->second.string();
					}
				}
				else
				{
					name=flagger.findHeader(filepath,i.name,i.type==LOCAL_HEADER);
					println("{} is {}",i.name,name.value());
				}
				if(name)
				{
					auto it=graph.insert({{*name,false,i.type!=MODULE,isExternal},{}}).first;
					it->second.dependent.push_back(current);
					if(i.type!=MODULE)
					{
						path headerPath(*name);
						path modulePath(flagger.headerNameToOutput(i.name,options.objectDirectory()));
						auto headerIt=headers.m.find(headerPath);
						if(headerIt==headers.m.end())
						{
							headerIt=headers.insert(std::move(headerPath),std::move(modulePath),path{},parseModuleData(options,*name)).first;
						}
						it->second.recompile=headerIt->second.source>headerIt->second.object||options.isForceRecompile();
						it->second.remaining=headerIt->second.dependency.size();
					}
				}
			}
		}
		while(externalImports.size())
		{
			path importPath=std::move(externalImports.front());
			externalImports.pop();
			ForwardGraphNode current{importPath.string(),false,false,true};
			const ModuleCompilation&mc=external.files.m.at(importPath);
			bool toCompile=options.isForceRecompileEnhanced()||mc.source>mc.object;
			auto[itToCurrent,succ]=graph.insert({current,{{},static_cast<uint16_t>(mc.dependency.size()),toCompile}});
			if(!succ)
			{
				itToCurrent->second.remaining=mc.dependency.size();
				itToCurrent->second.recompile=toCompile;
			}
			if(options.isDumpModuleMap())
			{
				println("{}",importPath.string());
			}
			for(const auto&i:mc.dependency)
			{
				optional<string>oname;
				if(i.type==MODULE)
				{
					auto externalIt=external.primaryModuleInterfaceUnits.find(i.name);
					if(externalIt!=external.primaryModuleInterfaceUnits.end())
					{
						const path&name=externalIt->second;
						if(externalImportVisited.insert(name).second)
						{
							externalImports.push(name);
						}
						oname=name.string();
					}
				}
				else
				{
					oname=flagger.findHeader(importPath,i.name,i.type==LOCAL_HEADER);
				}
				if(oname)
				{
					auto it=graph.insert({{*oname,false,i.type!=MODULE,true},{}}).first;
					it->second.dependent.push_back(current);
					if(i.type!=MODULE)
					{
						path headerPath(*oname);
						path modulePath(flagger.headerNameToOutput(i.name,options.objectDirectory()));
						auto headerIt=headers.m.find(headerPath);
						if(headerIt==headers.m.end())
						{
							headerIt=headers.insert(std::move(headerPath),std::move(modulePath),path{},parseModuleData(options,*oname)).first;
						}
						it->second.recompile=headerIt->second.source>headerIt->second.object||options.isForceRecompile();
						it->second.remaining=headerIt->second.dependency.size();
					}
				}
			}
		}
		queue<ForwardGraphNode>compileQueue;
		for(const auto&[node,edges]:graph)
		{
			if(edges.remaining==0)
			{
				compileQueue.push(node);
			}
		}
		unordered_map<unsigned,ForwardGraphNode>processToNode;
		while(!pm.is_empty()||compileQueue.size())
		{
			if(!pm.is_full()&&compileQueue.size())
			{
				ForwardGraphNode node=std::move(compileQueue.front());
				compileQueue.pop();
				const ForwardGraphNodeData&data=graph.at(node);
				const ModuleCompilation&mc=node.header?headers.m.at({node.name}):node.external?external.files.m.at(path{node.name}):internal.files.m.at(path{node.name});
				if(!node.external&&!node.header)
				{
					linkerArguments.push_back(mc.output.string());
				}
				if(data.recompile)
				{
					if(options.isDisplayCommand())
					{
						println("Compiling {}",node.name);
					}
					string outputfile=node.external&&!node.header?"/dev/null":mc.output.string();
					path emptypath;
					const path&pp=node.header?emptypath:(node.external?external.files:internal.files).m.at(path{node.name}).preprocessed;
					string pps=pp.string();
					bool exchange=pps.size();
					if(exchange)
					{
						swap(pps,node.name);
					}
					auto arguments=flagger.compileFile(node.name,outputfile,mc.name,mc.dependency,options,node.notInterface,node.header);
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
		auto arguments=flagger.linkProgram(product,options,linkerArguments);
		pm.run(arguments,options.isDisplayCommand());
		pm.wait_remaining_processes();
	}
	~ProgramBuilder()=default;
	static ProgramBuilder&getInstance(pair<CompilerType,vector<string>>tai,BuildConfiguration options)
		noexcept
	{
		if(!singletonPointerProgramBuilder)
		{
			singletonPointerProgramBuilder=make_unique<ProgramBuilder>(ProgramBuilderConstructorTag{},std::move(tai),std::move(options));
		}
		return*singletonPointerProgramBuilder;
	}
};
unique_ptr<ProgramBuilder>singletonPointerProgramBuilder;
