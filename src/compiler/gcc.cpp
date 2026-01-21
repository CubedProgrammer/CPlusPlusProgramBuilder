export module compiler.gcc;
export import compiler.base;
using namespace std;
using filesystem::path;
constexpr string_view CBP_GCC_MODULE_FLAG="-fmodules";
export class GCCConfigurer:public BaseCompilerConfigurer
{
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
	virtual optional<pair<ModuleData,path>>onPreprocessError(const path&file,const string&error)
	{
		print("{}",error);
		return nullopt;
	}
	virtual void addCompilerSpecificArguments()
	{
		compilerArguments.push_back(CBP_GCC_MODULE_FLAG);
	}
	virtual void compilerSpecificArgumentsForFile(vector<string_view>&output,array<string_view,3>names,span<const ImportUnit>imports,bool notInterface,bool isHeader)
	{
		if(isHeader)
		{
			output.push_back(CBP_LANGUAGE_FLAG);
			output.push_back(CBP_HEADER_LANGUAGE);
		}
	}
};
