export module cpbuild;
import std;
import configuration;
import dependency;
import flag;
import process;
using std::chrono::file_clock;
using std::filesystem::current_path,std::filesystem::file_time_type,std::filesystem::path,std::filesystem::recursive_directory_iterator;
using namespace std;
using namespace chrono_literals;
constexpr string_view CBP_ALLOWED_EXTENSIONS="c++ c++m cc ccm cpp cppm cxx cxxm";
constexpr string_view CBP_CLANG="clang++";
constexpr string_view CBP_GCC="g++";
export struct ModuleCompilation
{
	vector<ImportUnit>dependency;
	path output;
	file_time_type source;
	file_time_type object;
	vector<unsigned>dependentProcesses;
	string name;
	bool visited;
};
export using FileModuleMap=unordered_map<path,ModuleCompilation>;
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
	string_view name;
	BuildConfiguration options;
	vector<string>linkerArguments;
	ModuleConnection internal;
	ModuleConnection external;
	ForwardGraph graph;
	CompilerConfigurer flagger;
	ParallelProcessManager pm;
public:
	ProgramBuilder(ProgramBuilderConstructorTag,string_view name,pair<CompilerType,vector<string>>tai,BuildConfiguration op)
		noexcept
		:name(name),options(std::move(op)),linkerArguments(),internal(),graph(),flagger(tai.first,std::move(tai.second)),pm(op.threadCount)
	{}
	ProgramBuilder()=delete;
	ProgramBuilder(const ProgramBuilder&)=delete;
	ProgramBuilder(ProgramBuilder&&)=delete;
	ProgramBuilder&operator=(const ProgramBuilder&)=delete;
	ProgramBuilder&operator=(ProgramBuilder&&)=delete;
	path getOutputFile(path p)
		const noexcept
	{
		if(p.has_extension())
		{
			p.replace_extension(path{"o"});
		}
		if(options.objectDirectory.size())
		{
			if(options.targets.size()==1)
			{
				string pathString=p.string();
				size_t index=pathString.find('/');
				index=index==string::npos?pathString.size()-1:index;
				string_view sv{pathString.cbegin()+index+1,pathString.cend()};
				p=sv;
			}
			p=path{options.objectDirectory}/p;
		}
		return p;
	}
	path getFinalOutputFile()
		const noexcept
	{
		if(options.artifact.size()==0)
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
			return path{options.artifact};
		}
	}
	void add_file(path&&p,bool externalDirectory=false)
	{
		ModuleData data=parseModuleData(p);
		const path object=externalDirectory?path{flagger.moduleNameToFile(data.name,options.objectDirectory)}:getOutputFile(p);
		if(!object.empty())
		{
			ModuleConnection&connection=externalDirectory?external:internal;
			const file_time_type lastModify=last_write_time(p);
			const file_time_type objectModify=exists(object)?last_write_time(object):lastModify-1s;
			connection.primaryModuleInterfaceUnits.emplace(data.name,p);
			connection.files.emplace(std::move(p),ModuleCompilation(std::move(data.imports),object,lastModify,objectModify,{},std::move(data.name),false));	
		}
	}
	void add_directory(const path&p,bool externalDirectory=false)
	{
		using namespace string_view_literals;
		auto permitted=ranges::to<vector<string>>(views::split(CBP_ALLOWED_EXTENSIONS," "sv));
		for(const auto&en:recursive_directory_iterator(p))
		{
			const path&current=en.path();
			if(en.is_directory())
			{
				if(!externalDirectory)
				{
					create_directory(getOutputFile(current));
				}
			}
			else if(current.has_extension()&&ranges::contains(permitted,current.extension().string().substr(1))&&en.is_regular_file())
			{
				add_file(path(current),externalDirectory);
			}
		}
	}
	void cpbuild()
	{
		span<string_view>targets=options.targets;
		flagger.addArguments(options);
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
		add_file(path{flagger.getSTDModulePath()},true);
		add_file(path{flagger.getSTDCompatModulePath()},true);
		println("external {}",external.files.size());
		for(path t:flagger.getIncludeDirectories())
		{
			println("{}",t.string());
			add_directory(t,true);
		}
		println("external {}",external.files.size());
		for(const auto&[s,p]:external.primaryModuleInterfaceUnits)
		{
			println("module {} {}",s,p.string());
		}
		queue<path>externalImports;
		auto interfaceEndIt=internal.primaryModuleInterfaceUnits.end();
		for(const auto&[filepath,mc]:internal.files)
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
				auto interfaceIt=internal.primaryModuleInterfaceUnits.find(i.name);
				optional<string>name;
				bool isExternal=interfaceIt==interfaceEndIt;
				if(isExternal)
				{
					auto externalIt=external.primaryModuleInterfaceUnits.find(i.name);
					if(externalIt!=external.primaryModuleInterfaceUnits.end())
					{
						name=externalIt->second.string();
						externalImports.push(path{*name});
					}
				}
				else
				{
					name=interfaceIt->second.string();
				}
				if(name)
				{
					auto it=graph.insert({{*name,false,i.type!=MODULE,isExternal},{}}).first;
					it->second.dependent.push_back(current);
				}
			}
		}
		while(externalImports.size())
		{
			path importPath=std::move(externalImports.front());
			externalImports.pop();
			println("externally importing {}",importPath.string());
			ForwardGraphNode current{importPath.string(),false,false,true};
			const ModuleCompilation&mc=external.files.at(importPath);
			bool toCompile=options.isForceRecompileEnhanced()||mc.source>mc.object;
			auto[itToCurrent,succ]=graph.insert({current,{{},static_cast<uint16_t>(mc.dependency.size()),toCompile}});
			if(!succ)
			{
				itToCurrent->second.remaining=mc.dependency.size();
				itToCurrent->second.recompile=toCompile;
			}
			for(const auto&i:mc.dependency)
			{
				auto externalIt=external.primaryModuleInterfaceUnits.find(i.name);
				println("depends on {}",i.name);
				if(externalIt!=external.primaryModuleInterfaceUnits.end())
				{
					path name=externalIt->second;
					println("{} is {}",i.name,name.string());
					externalImports.push(name);
					auto it=graph.insert({{name.string(),false,i.type!=MODULE,true},{}}).first;
					it->second.dependent.push_back(current);
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
			println("{} {} {} {} {}",node.name,node.notInterface,edges.remaining,edges.recompile,views::transform(edges.dependent,&ForwardGraphNode::name));
		}
		unordered_map<unsigned,ForwardGraphNode>processToNode;
		while(!pm.is_empty()||compileQueue.size())
		{
			if(!pm.is_full()&&compileQueue.size())
			{
				ForwardGraphNode node=std::move(compileQueue.front());
				compileQueue.pop();
				const ForwardGraphNodeData&data=graph.at(node);
				const ModuleCompilation&mc=node.external?external.files.at(path{node.name}):internal.files.at(path{node.name});
				if(!node.external)
				{
					linkerArguments.push_back(mc.output.string());
				}
				if(data.recompile)
				{
					println("Compiling {}",node.name);
					string outputfile=node.external?"/dev/null":mc.output.string();
					auto arguments=flagger.compileFile(node.name,outputfile,mc.name,options,node.notInterface,node.header);
					auto opid=pm.run(arguments,options.isDisplayCommand());
					if(opid)
					{
						processToNode.insert({*opid,std::move(node)});
					}
					else
					{
						println("Compiling {} failed",node.name);
					}
				}
				else
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
	static ProgramBuilder&getInstance(string_view program,pair<CompilerType,vector<string>>tai,BuildConfiguration options)
		noexcept
	{
		if(!singletonPointerProgramBuilder)
		{
			singletonPointerProgramBuilder=make_unique<ProgramBuilder>(ProgramBuilderConstructorTag{},program,std::move(tai),std::move(options));
		}
		return*singletonPointerProgramBuilder;
	}
};
unique_ptr<ProgramBuilder>singletonPointerProgramBuilder;
