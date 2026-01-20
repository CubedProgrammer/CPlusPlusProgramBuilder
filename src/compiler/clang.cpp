export module compiler.clang;
export import compiler.base;
using namespace std;
using filesystem::path;
constexpr string_view CBP_CLANG_MODULE_PATH="-fprebuilt-module-path=.";
constexpr string_view CBP_CLANG_PRECOMPILE_FLAG="--precompile";
export class ClangConfigurer:public BaseCompilerConfigurer
{
	string clangPrebuiltModuleFlag;
	string clangModulePath;
	vector<string>clangHeaderFlagStorage;
public:
	static constexpr CompilerType TYPE=LLVM;
	ClangConfigurer(vector<string>id)
		:BaseCompilerConfigurer(TYPE,std::move(id)),clangPrebuiltModuleFlag(),clangModulePath(),clangHeaderFlagStorage()
	{}
	ClangConfigurer()
		:ClangConfigurer(vector<string>{})
	{}
	virtual string getEitherSTDModulePath(const string&name)
		const
	{
		string first="/usr/local/share/libc++/v1/"+name+".cppm";
		string second="/usr/share/libc++/v1/"+name+".cppm";
		return exists(path{first})?first:second;
	}
	virtual void addSpecificPreprocessArguments(vector<char*>&args)
		const
	{
		args.push_back(svConstCaster(CBP_STDLIB_FLAG));
	}
	virtual void addCompilerSpecificArguments(const BuildConfiguration&configuration)
	{
		string_view flag=CBP_CLANG_MODULE_PATH;
		if(configuration.objectDirectory().size())
		{
			clangPrebuiltModuleFlag=string{CBP_CLANG_MODULE_PATH.data(),CBP_CLANG_MODULE_PATH.size()-1};
			clangPrebuiltModuleFlag.append_range(configuration.objectDirectory());
			flag=clangPrebuiltModuleFlag;
		}
		compilerArguments.push_back(CBP_STDLIB_FLAG);
		compilerArguments.push_back(flag);
	}
	virtual void compilerSpecificArgumentsForFile(vector<string_view>&output,const string&filename,const string&outputname,const string&moduleName,span<const ImportUnit>imports,const BuildConfiguration&configuration,bool notInterface,bool isHeader)
	{
		clangHeaderFlagStorage.clear();
		for(const ImportUnit&unit:views::filter(imports,[](const ImportUnit&u){return u.type!=MODULE;}))
		{
			clangHeaderFlagStorage.push_back("-fmodule-file="+headerNameToOutput(unit.name,configuration.objectDirectory()));
		}
		output.append_range(clangHeaderFlagStorage);
		if(isHeader)
		{
			output.push_back(CBP_CLANG_PRECOMPILE_FLAG);
			output.push_back(CBP_LANGUAGE_FLAG);
			output.push_back(CBP_HEADER_LANGUAGE);
		}
		else if(!notInterface)
		{
			output.push_back(CBP_LANGUAGE_FLAG);
			output.push_back(CBP_MODULE_LANGUAGE);
		}
		if(!notInterface&&!isHeader)
		{
			clangModulePath="-fmodule-output=";
			clangModulePath+=moduleNameToFile(moduleName,configuration.objectDirectory());
			output.push_back(clangModulePath);
		}
	}
};
