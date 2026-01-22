export module compiler.clang;
export import compiler.base;
using namespace std;
using filesystem::path;
using views::as_rvalue,views::zip;
constexpr string_view CBP_CLANG_MODULE_PATH="-fprebuilt-module-path=.";
constexpr string_view CBP_CLANG_PRECOMPILE_FLAG="--precompile";
constexpr string_view CLANG_HEADER_ERROR_BEGIN="error: header file ";
constexpr string_view CLANG_HEADER_ERROR_END=" cannot be imported because it is not known to be a header unit";
export class ClangConfigurer:public BaseCompilerConfigurer
{
	string clangPrebuiltModuleFlag;
	string clangModulePath;
	vector<string>clangHeaderFlagStorage;
	vector<string>clangHeaderOutputs;
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
	virtual void resolveHeaders(span<ImportUnit>units,span<char>resolved)
		const
	{
		for(auto[i,r]:zip(units,resolved))
		{
			if(i.type!=MODULE)
			{
				r=true;
			}
		}
	}
	virtual void addSpecificPreprocessArguments(vector<char*>&args)
		const
	{
		args.push_back(svConstCaster(CBP_STDLIB_FLAG));
		for(const string&s:clangHeaderOutputs)
		{
			string_view sv=s;
			args.push_back(svConstCaster(sv));
		}
	}
	virtual optional<pair<ModuleData,path>>onPreprocessError(const path&file,const string&error,bool external)
	{
		optional<pair<ModuleData,path>>mdO;
		vector<ImportUnit>importedHeaders;
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
				bool external=info[0]!=info[akaIndex-3];
				string_view pathSV=info.substr(akaIndex+5,info.size()-akaIndex-7);
				//string pathS=pathSV.front()=='/'?"":"./";
				//pathS+=pathSV;
				path pathPath(pathSV);
				string pathS=absolute(pathPath).string();
				string outputfile=headerNameToOutput(pathSV);
				bool force=configuration->isForceRecompile()&&(!external||configuration->isForceRecompileEnhanced());
				bool shouldCompile=force||isMoreRecent(pathPath,outputfile);
				if(shouldCompile)
				{
					compile({pathS,outputfile,""},{},false,true);
				}
				println("clang found {} {} {}",external,pathSV,outputfile);
				clangHeaderOutputs.push_back("-fmodule-file="+outputfile);
				importedHeaders.push_back({string{pathSV},external?SYS_HEADER:LOCAL_HEADER});
				//importedHeaders.push_back({std::move(pathS),SYS_HEADER});
			}
		}
		manager->wait_remaining_processes();
		auto[pathO,_]=preprocess(file,external);
		clangHeaderOutputs.clear();
		if(pathO)
		{
			mdO={parseModuleData(*configuration,*pathO),std::move(*pathO)};
			erase_if(mdO->first.imports,[](const ImportUnit&unit){return unit.type!=MODULE;});
			mdO->first.imports.append_range(as_rvalue(importedHeaders));
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
			clangHeaderFlagStorage.push_back("-fmodule-file="+headerNameToOutput(unit.name));
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
			clangModulePath+=moduleNameToFile(moduleName);
			output.push_back(clangModulePath);
		}
	}
};
