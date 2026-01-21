export module compiler.clang;
export import compiler.base;
using namespace std;
using filesystem::path;
constexpr string_view CBP_CLANG_MODULE_PATH="-fprebuilt-module-path=.";
constexpr string_view CBP_CLANG_PRECOMPILE_FLAG="--precompile";
constexpr string_view CLANG_HEADER_ERROR_BEGIN="error: header file ";
constexpr string_view CLANG_HEADER_ERROR_END=" cannot be imported because it is not known to be a header unit";
export class ClangConfigurer:public BaseCompilerConfigurer
{
	string clangPrebuiltModuleFlag;
	string clangModulePath;
	vector<string>clangHeaderFlagStorage;
public:
	static constexpr CompilerType TYPE=LLVM;
	ClangConfigurer(vector<string>id,const BuildConfiguration*c,ParallelProcessManager*m)
		:BaseCompilerConfigurer(TYPE,std::move(id),c,m),clangPrebuiltModuleFlag(),clangModulePath(),clangHeaderFlagStorage()
	{}
	ClangConfigurer()
		:ClangConfigurer(vector<string>{},nullptr,nullptr)
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
	virtual optional<pair<ModuleData,path>>onPreprocessError(const path&file,const string&error)
	{
		optional<pair<ModuleData,path>>mdO;
		vector<string_view>headersImported;
		size_t beginIndex=0;
		size_t endIndex=0;
		for(;beginIndex!=string::npos&&endIndex!=string::npos;beginIndex=endIndex+CLANG_HEADER_ERROR_END.size())
		{
			beginIndex=error.find(CLANG_HEADER_ERROR_BEGIN,endIndex);
			endIndex=error.find(CLANG_HEADER_ERROR_END,beginIndex);
			if(beginIndex!=string::npos&&endIndex!=string::npos)
			{
				string_view info{error.data()+beginIndex+CLANG_HEADER_ERROR_BEGIN.size(),error.data()+endIndex};
				size_t akaIndex=info.find("aka");
				string_view pathSV=info.substr(akaIndex+5,info.size()-akaIndex-7);
				headersImported.push_back(pathSV);
				println("clang found {}",pathSV);
			}
		}
		return mdO;
	}
	virtual void addCompilerSpecificArguments()
	{
		string_view flag=CBP_CLANG_MODULE_PATH;
		if(configuration->objectDirectory().size())
		{
			clangPrebuiltModuleFlag=string{CBP_CLANG_MODULE_PATH.data(),CBP_CLANG_MODULE_PATH.size()-1};
			clangPrebuiltModuleFlag.append_range(configuration->objectDirectory());
			flag=clangPrebuiltModuleFlag;
		}
		compilerArguments.push_back(CBP_STDLIB_FLAG);
		compilerArguments.push_back(flag);
	}
	virtual void compilerSpecificArgumentsForFile(vector<string_view>&output,array<string_view,3>names,span<const ImportUnit>imports,bool notInterface,bool isHeader)
	{
		string_view moduleName=names[2];
		clangHeaderFlagStorage.clear();
		for(const ImportUnit&unit:views::filter(imports,[](const ImportUnit&u){return u.type!=MODULE;}))
		{
			clangHeaderFlagStorage.push_back("-fmodule-file="+headerNameToOutput(unit.name,configuration->objectDirectory()));
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
			clangModulePath+=moduleNameToFile(moduleName,configuration->objectDirectory());
			output.push_back(clangModulePath);
		}
	}
};
