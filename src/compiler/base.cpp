export module compiler.base;
export import dependency.scanner;
export import utility.process;
using namespace std;
using namespace literals;
using filesystem::path;
using filesystem::recursive_directory_iterator;
using views::filter;
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
	string moduleNameToFile(string_view name,string_view outputDirectory)
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
			file=prependOutputDirectory(file,outputDirectory);
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
	string headerNameToOutput(string_view name,string_view outputDirectory)
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
		s+=".pcm";
		return prependOutputDirectory(std::move(s),outputDirectory);
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
	virtual void resolveHeaders(span<ImportUnit>units,span<char>resolved)
		const
	{}
	virtual void addSpecificPreprocessArguments(vector<char*>&args)
		const
	{}
	pair<optional<path>,optional<string>>preprocess(const path&file)
	{
		optional<path>outOpt;
		optional<string>errOpt;
		path out=replaceMove(*configuration,file,path{"ii"});
		string fileString=file.string();
		string outString=out.string();
		char preprocessOption[]="-E";
		char outOption[]="-o";
		vector<char*>preprocessCommand;
		create_directories(out.parent_path());
		preprocessCommand.reserve(configuration->compilerOptions.size()+7);
		preprocessCommand.push_back(svConstCaster(configuration->compiler()));
		addSpecificPreprocessArguments(preprocessCommand);
		preprocessCommand.push_back(preprocessOption);
		preprocessCommand.append_range(views::transform(configuration->compilerOptions,svConstCaster));
		preprocessCommand.push_back(svConstCaster(fileString));
		preprocessCommand.push_back(outOption);
		preprocessCommand.push_back(svConstCaster(outString));
		preprocessCommand.push_back(nullptr);
		auto pid=launch_program(preprocessCommand,PIPE_ERROR);
		if(pid)
		{
			if(pid->second!=0)
			{
				println(cerr,"preprocessing {} failed with exit code {}",fileString,pid->second);
				//preprocessCommand.pop_back();
				//println("{}",preprocessCommand);
			}
			else
			{
				outOpt=std::move(out);
			}
			errOpt=std::move(pid->first);
		}
		else
		{
			println(cerr,"preprocessing {} failed",fileString);
		}
		return{std::move(outOpt),std::move(errOpt)};
	}
	virtual optional<pair<ModuleData,path>>onPreprocessError(const path&file,const string&error)=0;
	optional<pair<ModuleData,path>>scanImports(const path&file)
	{
		optional<pair<ModuleData,path>>dataO;
		auto[pathO,errorO]=preprocess(file);
		if(pathO)
		{
			dataO={parseModuleData(*configuration,*pathO),std::move(*pathO)};
		}
		else if(errorO)
		{
			dataO=onPreprocessError(file,*errorO);
		}
		return dataO;
	}
	virtual void addCompilerSpecificArguments()=0;
	void addArguments()
	{
		compilerArguments.push_back(configuration->compiler());
		bool addLanguageVersion=ranges::find_if(configuration->compilerOptions,[](string_view sv){return sv.starts_with("-std=");})==configuration->compilerOptions.end();
		if(addLanguageVersion)
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
	optional<unsigned>compile(array<string_view,3>names,span<const ImportUnit>imports,bool notInterface,bool isHeader)
	{
		auto command=getCompileCommand(names,imports,notInterface,isHeader);
		return manager->run(command,configuration->isDisplayCommand());
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
