export module graph.back;
export import flag;
export import graph.front;
using namespace std;
using filesystem::path;
export struct ProjectGraphIterator
{
	using difference_type=ptrdiff_t;
	using value_type=const pair<const string,FileData>;
	using pointer=value_type*;
	using reference_type=value_type&;
	using iterator_category=forward_iterator_tag;
	const array<unordered_map<string,FileData>,2>*files;
	unordered_map<string,FileData>::const_iterator base;
	bool second;
	constexpr reference_type operator*()
		const noexcept
	{
		return*base;
	}
	constexpr pointer operator->()
		const noexcept
	{
		return addressof(*base);
	}
	constexpr bool operator==(const ProjectGraphIterator&)const noexcept=default;
	constexpr ProjectGraphIterator&operator++()
		noexcept
	{
		++base;
		if(!second&&base==(*files)[0].end())
		{
			base=(*files)[1].begin();
			second=true;
		}
		return*this;
	}
	constexpr ProjectGraphIterator operator++(int)
		noexcept
	{
		ProjectGraphIterator old{*this};
		++*this;
		return old;
	}
};
export class ProjectGraph
{
	unordered_map<string,string>moduleToFile;
	array<unordered_map<string,FileData>,2>files;
	const BuildConfiguration*configuration;
	const CompilerConfigurer*flagger;
public:
	using iterator=ProjectGraphIterator;
	ProjectGraph(const BuildConfiguration&configuration,const CompilerConfigurer&flagger)
		:moduleToFile(),files(),configuration(&configuration),flagger(&flagger)
	{}
	ProjectGraph()
		:moduleToFile(),files(),configuration(),flagger()
	{}
	constexpr iterator begin()
		const
	{
		return{&files,files[files[0].size()==0].begin(),false};
	}
	constexpr iterator end()
		const
	{
		return{&files,files[1].end(),true};
	}
	void addEntry(string p,string module,path object,path preprocessed,vector<ImportUnit>&&imports,bool external)
	{
		files[external].insert({std::move(p),{std::move(module),std::move(preprocessed),std::move(object),std::move(imports),{},external}});
	}
	void addFile(path p,bool external)
	{
		auto[it,success]=files[external].insert({p.string(),{}});
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
					size_t icount=moduleData.imports.size();
					it->second={std::move(moduleData.name),std::move(*preprocessedFile),std::move(object),std::move(moduleData.imports),vector<char>(icount),external};
				}
			}
			else
			{
				files[external].erase(it);
			}
		}
	}
	void convertDependenciesToPath()
	{
		auto it=files[0].begin();
		for(;it!=files[1].end();++it)
		{
			if(it==files[0].end())
			{
				it=files[1].begin();
			}
			auto&[pathString,data]=*it;
			for(auto[unit,hasBeenResolved]:data.dependResolved())
			{
				if(unit.type==MODULE)
				{
					auto it=moduleToFile.find(unit.name);
					if(it!=moduleToFile.end())
					{
						unit.name=it->second;
						hasBeenResolved=true;
					}
				}
				else
				{
					path p(pathString);
					optional<string>headerPathO=flagger->findHeader(p.parent_path(),unit.name,unit.type==LOCAL_HEADER);
					if(headerPathO)
					{
						unit.name=*headerPathO;
						hasBeenResolved=true;
					}
				}
			}
			erase_if(data.depend,[](const ImportUnit&unit){return unit.name.size()==0;});
		}
		moduleToFile.clear();
	}
	unsigned checkForStandardModules()
		const noexcept
	{
		bool std=moduleToFile.find("std")!=moduleToFile.end();
		bool stdCompat=moduleToFile.find("std.compat")!=moduleToFile.end();
		return((unsigned)stdCompat<<1)|std;
	}
	constexpr optional<const pair<const string,FileData>*>query(string_view p)
		const noexcept
	{
		string ps{p};
		auto it=files[0].find(ps);
		if(it==files[0].end())
		{
			it=files[1].find(ps);
		}
		return it==files[1].end()?nullopt:optional<const pair<const string,FileData>*>{&*it};
	}
	const unordered_map<string,FileData>&getProjectFiles()
		const noexcept
	{
		return files[0];
	}
	pair<queue<string_view>,unordered_set<string_view>>getExternalImports()
		const
	{
		queue<string_view>q;
		unordered_set<string_view>visited;
		for(const auto&[_,fdata]:files[0])
		{
			for(const ImportUnit&unit:fdata.depend)
			{
				if(unit.type==MODULE)
				{
					auto[_,succ]=visited.insert(unit.name);
					if(succ)
					{
						q.push(unit.name);
					}
				}
			}
		}
		return{std::move(q),std::move(visited)};
	}
};
export ForwardGraph makeForwardGraph(const ProjectGraph&pg,bool forceRecompile,bool externalForceRecompile)
{
	ForwardGraph g;
	unsigned forceLevel=((unsigned)externalForceRecompile<<1)|(unsigned)forceRecompile;
	auto queryFunction=bind_front(&ProjectGraph::query,pg);
	for(const auto&[pathString,fdata]:pg.getProjectFiles())
	{
		g.insert(pathString,fdata,forceLevel,queryFunction);
	}
	auto[q,visited]=pg.getExternalImports();
	while(!q.empty())
	{
		string_view fname=q.front();
		q.pop();
		auto dataPairO=pg.query(fname);
		if(dataPairO)
		{
			const FileData&fdata=(*dataPairO)->second;
			const auto&v=fdata.depend;
			g.insert(fname,fdata,forceLevel,queryFunction);
			for(const ImportUnit&unit:v)
			{
				auto[_,succ]=visited.insert(unit.name);
				if(succ)
				{
					q.push(unit.name);
				}
			}
		}
	}
	return g;
}
