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
