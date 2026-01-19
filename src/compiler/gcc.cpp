export module compiler.gcc;
export import compiler.base;
using namespace std;
constexpr string_view CBP_GCC_MODULE_FLAG="-fmodules";
export class GCCConfigurer:public BaseCompilerConfigurer
{
public:
	static constexpr CompilerType TYPE=GNU;
	GCCConfigurer(vector<string>id)
		:BaseCompilerConfigurer(TYPE,std::move(id))
	{}
	GCCConfigurer()
		:GCCConfigurer(vector<string>{})
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
	virtual void addCompilerSpecificArguments(const BuildConfiguration&configuration)
	{
		compilerArguments.push_back(CBP_GCC_MODULE_FLAG);
	}
	virtual void compilerSpecificArgumentsForFile(vector<string_view>&output,const string&filename,const string&outputname,const string&moduleName,span<const ImportUnit>imports,const BuildConfiguration&configuration,bool notInterface,bool isHeader)
	{
		if(isHeader)
		{
			output.push_back(CBP_LANGUAGE_FLAG);
			output.push_back(CBP_HEADER_LANGUAGE);
		}
	}
};
