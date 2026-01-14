export module graph;
export import flag;
using namespace std;
using filesystem::path;
using views::filter;
export struct FileData
{
	string module;
	path preprocessed;
	path object;
	vector<ImportUnit>depend;
	bool external;
};
export struct ForwardGraphNode
{
	string name;
	bool notInterface;
	bool header;
	bool external;
	constexpr bool operator==(const ForwardGraphNode&)const noexcept=default;
};
export namespace std
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
export struct ForwardGraphNodeData
{
	vector<ForwardGraphNode>dependent;
	uint16_t remaining;
	bool recompile;
};
export using ForwardGraph=unordered_map<ForwardGraphNode,ForwardGraphNodeData>;
export class ProjectGraph
{
	unordered_map<string,string>moduleToFile;
	unordered_map<string,FileData>files;
	const BuildConfiguration*configuration;
	const CompilerConfigurer*flagger;
public:
	ProjectGraph(const BuildConfiguration&configuration,const CompilerConfigurer&flagger)
		:moduleToFile(),files(),configuration(&configuration),flagger(&flagger)
	{}
	ProjectGraph()
		:moduleToFile(),files(),configuration(),flagger()
	{}
	void addFile(path p,bool external)
	{
		auto[it,success]=files.insert({p.string(),{}});
		if(success)
		{
			optional<path>preprocessedFile=preprocess(*configuration,p);
			if(preprocessedFile)
			{
				ModuleData moduleData=parseModuleData(*configuration,*preprocessedFile);
				if(moduleData.name.size())
				{
					moduleToFile.insert({moduleData.name,p.string()});
				}
				if(moduleData.name.size()||!external)
				{
					path object=external?path{flagger->moduleNameToFile(moduleData.name,configuration->objectDirectory())}:replaceMove(*configuration,p,path{"o"});
					it->second={std::move(moduleData.name),std::move(*preprocessedFile),std::move(object),std::move(moduleData.imports),external};
				}
			}
			else
			{
				files.erase(it);
			}
		}
	}
	void convertDependenciesToPath()
	{
		for(auto&[pathString,data]:files)
		{
			for(auto&unit:data.depend)
			{
				if(unit.type==MODULE)
				{
					auto it=moduleToFile.find(unit.name);
					if(it!=moduleToFile.end())
					{
						unit.name=it->second;
					}
					else
					{
						unit.name.clear();
					}
				}
				else
				{
					path p(pathString);
					optional<string>headerPathO=flagger->findHeader(p.parent_path(),unit.name,LOCAL_HEADER);
					if(headerPathO)
					{
						unit.name=*headerPathO;
					}
					else
					{
						unit.name.clear();
					}
				}
			}
			erase_if(data.depend,[](const ImportUnit&unit){return unit.name.size()==0;});
		}
		moduleToFile.clear();
	}
	constexpr optional<const pair<const string,FileData>*>query(const string&p)
		const noexcept
	{
		auto it=files.find(p);
		return it==files.end()?nullopt:optional<const pair<const string,FileData>*>{&*it};
	}
	constexpr auto begin()const
	{
		return files.begin();
	}
	constexpr auto end()const
	{
		return files.end();
	}
};
export queue<ForwardGraphNode>buildInitialCompileQueue(const ForwardGraph&g)
{
	queue<ForwardGraphNode>compileQueue;
	for(const auto&[node,edges]:g)
	{
		if(edges.remaining==0)
		{
			compileQueue.push(node);
		}
	}
	return compileQueue;
}
export ForwardGraph makeForwardGraph(const ProjectGraph&pg,bool forceRecompile,bool externalForceRecompile)
{
	ForwardGraph g;
	unsigned forceLevel=((unsigned)externalForceRecompile<<1)|(unsigned)forceRecompile;
	for(const auto&[pathString,fdata]:pg)
	{
		path p(pathString);
		bool updated=last_write_time(p)>last_write_time(fdata.object);
		bool toCompile=(forceLevel>>fdata.external&1)||updated;
		auto[it,succ]=g.insert({{pathString,fdata.module.size()==0,false,fdata.external},{{},static_cast<uint16_t>(fdata.depend.size()),toCompile}});
		if(!succ)
		{
			it->second.remaining=static_cast<uint16_t>(fdata.depend.size());
			it->second.recompile=toCompile;
		}
		for(const ImportUnit&iu:fdata.depend)
		{
			auto entryO=pg.query(iu.name);
			bool external=false;
			if(entryO)
			{
				auto&[_,dependData]=**entryO;
				external=dependData.external;
			}
			ForwardGraphNode node{iu.name,true,iu.type!=MODULE,external};
			auto[itN,succN]=g.insert({std::move(node),{{},0,false}});
			switch(iu.type)
			{
				case MODULE:
					break;
				case SYS_HEADER:
				case LOCAL_HEADER:
			}
		}
	}
	return g;
}
