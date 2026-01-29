export module compiler.gcc;
export import compiler.base;
using namespace std;
using filesystem::directory_iterator,filesystem::path;
using views::drop,views::take;
constexpr string_view CBP_GCC_MODULE_FLAG="-fmodules";
constexpr string_view CBP_GCC_MAPPER_FLAG="-fmodule-mapper";
export class GCCConfigurer:public BaseCompilerConfigurer
{
	string moduleMapperFlag;
public:
	static constexpr CompilerType TYPE=GNU;
	GCCConfigurer(vector<string>id,const BuildConfiguration*c,ParallelProcessManager*m)
		:BaseCompilerConfigurer(TYPE,std::move(id),c,m)
	{}
	GCCConfigurer()
		:GCCConfigurer(vector<string>{},nullptr,nullptr)
	{}
	virtual string getEitherSTDModulePath(const string&name)
		const
	{
		return includeDirectories.front()+"/bits/"+name+".cc";
	}
	virtual string getModuleExtension()
		const noexcept
	{
		return".gcm";
	}
	virtual Async<optional<pair<ModuleData,path>>>onPreprocessError(const path&file,const string&error,bool external)
	{
		print(cerr,"{}",error);
		co_return nullopt;
	}
	virtual void addCompilerSpecificArguments()
	{
		string_view dir=".";
		compilerArguments.push_back(CBP_GCC_MODULE_FLAG);
		moduleMapperFlag=CBP_GCC_MAPPER_FLAG;
		moduleMapperFlag.push_back('=');
		moduleMapperFlag+=configuration->objectDirectory();
		if(configuration->objectDirectory().size()>0)
		{
			moduleMapperFlag.push_back('/');
			dir=configuration->objectDirectory();
		}
		moduleMapperFlag+="module.map";
		compilerArguments.push_back(moduleMapperFlag);
		directory_iterator it(dir);
		path extension(getModuleExtension());
		ofstream fout(moduleMapperFlag.substr(CBP_GCC_MAPPER_FLAG.size()+1));
		for(path p:it)
		{
			if(p.extension()==extension)
			{
				string name=p.stem().string();
				string demangled;
				string_view search="%2F";
				size_t index=0;
				size_t start=0;
				demangled.reserve(name.size());
				while(index!=string::npos)
				{
					index=name.find(search,start);
					if(index!=string::npos)
					{
						demangled.append_range(drop(take(name,index),start));
						demangled.push_back('/');
						start=index+search.size();
					}
					else
					{
						demangled.append_range(drop(name,start));
					}
				}
				println(fout,"{} {}",demangled,p.string());
			}
		}
	}
	virtual void compilerSpecificArgumentsForFile(vector<string_view>&output,array<string_view,3>names,span<const ImportUnit>imports,bool notInterface,bool isHeader)
	{
		auto[fname,oname,mname]=names;
		string_view use;
		string cmi;
		if(isHeader)
		{
			output.push_back(CBP_LANGUAGE_FLAG);
			output.push_back(CBP_HEADER_LANGUAGE);
			cmi=headerNameToOutput(use=fname);
		}
		else
		{
			cmi=moduleNameToFile(use=mname);
		}
		ofstream fout(moduleMapperFlag.substr(CBP_GCC_MAPPER_FLAG.size()+1),ios::app);
		//println("gcc module map {} {}",use,cmi);
		println(fout,"{} {}",use,cmi);
	}
};
