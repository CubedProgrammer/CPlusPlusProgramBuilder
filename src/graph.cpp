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
		}
	}
	void convertDependenciesToPath()
	{
		for(auto&[pathString,data]:files)
		{
			auto nameOnly=views::transform(data.depend,&ImportUnit::name);
			ranges::transform(filter(data.depend,[](const ImportUnit&unit){return unit.type==MODULE;}),nameOnly.begin(),[this](const string&u){return moduleToFile.at(u);},&ImportUnit::name);
		}
		moduleToFile.clear();
	}
};
