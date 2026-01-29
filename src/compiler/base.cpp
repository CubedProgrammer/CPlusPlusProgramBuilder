export module compiler.base;
export import dependency.scanner;
export import utility.process;
using namespace std;
using namespace literals;
using filesystem::path;
using filesystem::recursive_directory_iterator;
using views::filter,views::zip;
export constexpr string_view CBP_COMPILE_FLAG="-c";
export constexpr string_view CBP_OUTPUT_FLAG="-o";
export constexpr string_view CBP_LANGUAGE_FLAG="-x";
export constexpr string_view CBP_MODULE_LANGUAGE="c++-module";
constexpr string_view CBP_LANGUAGE_VERSION="-std=c++20";
export constexpr string_view CBP_STDLIB_FLAG="-stdlib=libc++";
export constexpr string_view CBP_HEADER_LANGUAGE="c++-user-header";
constexpr string_view CBP_ALLOWED_EXTENSIONS="c++ c++m cc ccm cpp cppm cxx cxxm";
export enum CompilerType
{
	LLVM,GNU
};
bool hasSTDFlag(const BuildConfiguration&options)
{
	return ranges::find_if(options.compilerOptions,[](string_view sv){return sv.starts_with("-std=");})==options.compilerOptions.end();
}
export path replaceMove(const BuildConfiguration&options,path file,const path&extension)
{
	if(file.has_extension())
	{
		file.replace_extension(extension);
	}
	string_view output=options.objectDirectory().size()==0?".":options.objectDirectory();
	string pathString=file.string();
	if(options.targets.size()==1&&pathString.starts_with(options.targets[0]))
	{
		const size_t index=pathString.find_first_not_of('/',options.targets[0].size());
		if(index!=string::npos)
		{
			string_view sv{pathString.cbegin()+index,pathString.cend()};
			file=sv;
		}
	}
	return path{output}/file.relative_path();
}
string prependOutputDirectory(string file,string_view outputDirectory)
{
	if(outputDirectory.size())
	{
		string temp{outputDirectory};
		temp+='/';
		temp+=file;
		file=std::move(temp);
	}
	return file;
}
export bool isExtensionPermitted(const path&p)
	noexcept
{
	return p.extension().string().size()>1&&is_regular_file(p)&&ranges::contains(ranges::to<vector<string>>(views::split(CBP_ALLOWED_EXTENSIONS," "sv)),p.extension().string().substr(1));
}
export class BaseCompilerConfigurer
{
protected:
	CompilerType type;
	vector<string>includeDirectories;
	vector<string_view>compilerArguments;
	vector<path>potentialModuleFiles;
	const BuildConfiguration*const configuration;
	ParallelProcessManager*const manager;
public:
	BaseCompilerConfigurer(CompilerType ct,vector<string>id,const BuildConfiguration*c,ParallelProcessManager*m)
		noexcept
		:type(ct),includeDirectories(std::move(id)),compilerArguments(),configuration(c),manager(m)
	{}
	BaseCompilerConfigurer()
		noexcept
		:BaseCompilerConfigurer(LLVM,{},nullptr,nullptr)
	{}
	constexpr CompilerType getCompilerType()
		const noexcept
	{
		return type;
	}
	span<const string>getIncludeDirectories()
		const noexcept
	{
		return includeDirectories;
	}
	span<const path>getPotentialModuleFiles()
		const noexcept
	{
		return potentialModuleFiles;
	}
	virtual string getEitherSTDModulePath(const string&name)
		const=0;
	string getSTDModulePath()
	{
		return getEitherSTDModulePath("std");
	}
	string getSTDCompatModulePath()
	{
		return getEitherSTDModulePath("std.compat");
	}
	virtual string getModuleExtension()
		const noexcept
	{
		return".pcm";
	}
	string moduleNameToFile(string_view name)
		const
	{
		string file{name};
		for(char&c:filter(file,bind_front(equal_to<char>(),':')))
		{
			c='-';
		}
		if(name.size())
		{
			file+=getModuleExtension();
			file=prependOutputDirectory(file,configuration->objectDirectory());
		}
		return file;
	}
	optional<string>findHeader(path fpath,string_view include,bool searchParent)
		const
	{
		optional<string>opath;
		if(searchParent)
		{
			fpath.replace_filename(include);
			if(exists(fpath))
			{
				opath=fpath.string();
			}
		}
		for(const string&dirstr:views::filter(includeDirectories,[&opath](const string&){return!opath.has_value();}))
		{
			path headerPath{dirstr};
			headerPath/=path{include};
			if(exists(headerPath))
			{
				opath=headerPath.string();
			}
		}
		return opath;
	}
	string headerNameToOutput(string_view name)
		const
	{
		string s;
		s.reserve(name.size()+4);
		for(char c:name)
		{
			if(c=='/')
			{
				s+="%2F";
			}
			else
			{
				s.push_back(c);
			}
		}
		s+=getModuleExtension();
		return prependOutputDirectory(std::move(s),configuration->objectDirectory());
	}
	vector<path>searchForLikelyCandidates(string_view name)
	{
		vector<path>candidates;
		if(potentialModuleFiles.size()==0)
		{
			for(const string&d:includeDirectories)
			{
				recursive_directory_iterator inside(d);
				for(path en:inside)
				{
					if(isExtensionPermitted(en))
					{
						potentialModuleFiles.push_back(std::move(en));
					}
				}
			}
		}
		size_t index=name.find(':');
		size_t dot=name.find('.');
		vector<string>additionalChecks;
		if(index!=string::npos)
		{
			additionalChecks.push_back(string{name.substr(index+1)});
			additionalChecks.push_back(string{name});
			additionalChecks.back()[index]='-';
			additionalChecks.push_back(string{name});
			additionalChecks.back()[index]='_';
			additionalChecks.push_back(string{name});
			additionalChecks.back().erase(index,1);
		}
		for(const path&p:potentialModuleFiles)
		{
			string stem=p.stem().string();
			bool maybe=stem==name;
			if(!maybe&&dot!=string::npos)
			{
				path pcopy(p);
				string full=pcopy.replace_extension().string();
				ranges::fill(filter(full,bind_front(equal_to<char>(),'/')),'.');
				maybe=maybe||full.ends_with(name);
			}
			maybe=maybe||ranges::fold_left(views::transform(additionalChecks,bind_front(ranges::equal_to{},stem)),false,logical_or<bool>());
			if(maybe)
			{
				candidates.push_back(p);
			}
		}
		return candidates;
	}
	span<const path>sortPotentialModuleFiles(string_view m)
	{
		auto scores=views::transform(potentialModuleFiles,[m](const path&p){string n=p.stem().string();return similarity(m,n);});
		auto scoresVector=ranges::to<vector<ModulePathSimilarity>>(scores);
		ranges::sort(zip(scoresVector,potentialModuleFiles),ranges::greater());
		return potentialModuleFiles;
	}
	virtual void resolveHeaders(span<ImportUnit>units,span<char>resolved)
		const
	{}
	virtual void addSpecificPreprocessArguments(vector<string_view>&args)
		const
	{}
	Async<pair<optional<path>,optional<string>>>preprocess(const path&file,bool external)
	{
		optional<path>outOpt;
		optional<string>errOpt;
		path out=replaceMove(*configuration,file,path{"ii"});
		bool force=configuration->isForceRecompile()&&(!external||configuration->isForceRecompileEnhanced());
		bool shouldPreprocess=force||isMoreRecent(file,out);
		if(shouldPreprocess)
		{
			string fileString=file.string();
			string outString=out.string();
			char preprocessOption[]="-E";
			char outOption[]="-o";
			vector<string_view>preprocessCommand;
			create_directories(out.parent_path());
			preprocessCommand.reserve(configuration->compilerOptions.size()+8);
			preprocessCommand.push_back(configuration->compiler());
			if(hasSTDFlag(*configuration))
			{
				preprocessCommand.push_back(CBP_LANGUAGE_VERSION);
			}
			addSpecificPreprocessArguments(preprocessCommand);
			preprocessCommand.push_back(preprocessOption);
			preprocessCommand.append_range(configuration->compilerOptions);
			preprocessCommand.push_back(fileString);
			preprocessCommand.push_back(outOption);
			preprocessCommand.push_back(outString);
			// pipe will clog if there is too much output
			// do not await on the process, instead asynchronously read from the pipe
			// only await on the process after the pipe is finished
			// which might be unnecessary, as the process is already finished
			// implement the asynchronous IO loop, or you are gay
			auto [pid,handle]=co_await manager->runAsync(preprocessCommand,configuration->isDisplayCommand(),PIPE_ERROR);
			//auto pid=launch_program(preprocessCommand,PIPE_ERROR);
			if(pid)
			{
				outOpt=std::move(out);
				//errOpt=std::move(pid->first);
			}
			else
			{
				println(cerr,"preprocessing {} failed",fileString);
			}
		}
		else
		{
			outOpt=std::move(out);
			errOpt=string();
		}
		co_return{std::move(outOpt),std::move(errOpt)};
	}
	virtual Async<optional<pair<ModuleData,path>>>onPreprocessError(const path&file,const string&error,bool external)=0;
	Async<optional<pair<ModuleData,path>>>scanImports(const path&file,bool external)
	{
		optional<pair<ModuleData,path>>dataO;
		auto[pathO,errorO]=co_await preprocess(file,external);
		if(pathO)
		{
			dataO={parseModuleData(*configuration,*pathO),std::move(*pathO)};
		}
		else if(errorO)
		{
			dataO=co_await onPreprocessError(file,*errorO,external);
		}
		if(!dataO)
		{
			string s=external?"external":"internal";
			println(cerr,"Scanning dependencies for {} file {} failed.",s,file.string());
		}
		co_return dataO;
	}
	virtual void addCompilerSpecificArguments()=0;
	void addArguments()
	{
		compilerArguments.push_back(configuration->compiler());
		if(hasSTDFlag(*configuration))
		{
			compilerArguments.push_back(CBP_LANGUAGE_VERSION);
		}
		addCompilerSpecificArguments();
		compilerArguments.append_range(configuration->compilerOptions);
	}
	virtual void compilerSpecificArgumentsForFile(vector<string_view>&output,array<string_view,3>names,span<const ImportUnit>imports,bool notInterface,bool isHeader)=0;
	vector<string_view>getCompileCommand(array<string_view,3>names,span<const ImportUnit>imports,bool notInterface,bool isHeader)
	{
		vector<string_view>output=compilerArguments;
		auto[filename,outputname,_]=names;
		compilerSpecificArgumentsForFile(output,names,imports,notInterface,isHeader);
		output.push_back(filename.data());
		if(!isHeader)
		{
			output.push_back(CBP_COMPILE_FLAG);
		}
		output.push_back(CBP_OUTPUT_FLAG);
		output.push_back(outputname);
		return output;
	}
	optional<int>compile(array<string_view,3>names,span<const ImportUnit>imports,bool notInterface,bool isHeader)
	{
		auto command=getCompileCommand(names,imports,notInterface,isHeader);
		return manager->run(command,configuration->isDisplayCommand()).transform([](const pair<int,PipeHandle>&res){return res.first;});
	}
	vector<string_view>getLinkCommand(string_view artifact,span<const string>files)
		const
	{
		vector<string_view>trueLinkerArguments{configuration->compiler()};
		if(type==LLVM)
		{
			trueLinkerArguments.push_back(CBP_STDLIB_FLAG);
		}
		trueLinkerArguments.append_range(files);
		trueLinkerArguments.append_range(configuration->linkerOptions);
		trueLinkerArguments.push_back(CBP_OUTPUT_FLAG);
		trueLinkerArguments.push_back(artifact);
		return trueLinkerArguments;
	}
	void link(string_view artifact,span<const string>files)
	{
		auto command=getLinkCommand(artifact,files);
		manager->run(command,configuration->isDisplayCommand());
	}
	virtual~BaseCompilerConfigurer()=default;
};
