export module flag;
export import dependency;
using namespace std;
using namespace literals;
using filesystem::path;
constexpr string_view source="int main(int,char**){return 0;}";
constexpr string_view programPath="/tmp/C++ProgramBuilderBasicProgram.c++";
constexpr string_view CBP_GCC_MODULE_FLAG="-fmodules";
constexpr string_view CBP_CLANG_MODULE_PATH="-fprebuilt-module-path=.";
constexpr string_view CBP_COMPILE_FLAG="-c";
constexpr string_view CBP_OUTPUT_FLAG="-o";
constexpr string_view CBP_LANGUAGE_FLAG="-x";
constexpr string_view CBP_MODULE_LANGUAGE="c++-module";
constexpr string_view CBP_LANGUAGE_VERSION="-std=c++20";
constexpr string_view CBP_STDLIB_FLAG="-stdlib=libc++";
constexpr string_view CBP_CLANG_PRECOMPILE_FLAG="--precompile";
constexpr string_view CBP_HEADER_LANGUAGE="c++-user-header";
string clangPrebuiltModuleFlag;
string clangModulePath;
vector<string>clangHeaderFlagStorage;
string prependOutputDirectory(string file,string_view outputDirectory, bool isGCC)
{
	if(isGCC)
	{
		outputDirectory=outputDirectory.size()>0?outputDirectory:"gcm.cache";
	}
	if(outputDirectory.size())
	{
		string temp{outputDirectory};
		temp+='/';
		temp+=file;
		file=std::move(temp);
	}
	return file;
}
export enum CompilerType
{
	LLVM,GNU
};
export pair<CompilerType,vector<string>>getCompilerInformation(const BuildConfiguration&configuration)
{
	{
		ofstream fout(string{programPath});
		println(fout,"{}",source);
	}
	vector<char*>args;
	string versionFlag="--version";
	string verboseFlag="-v";
	string outputFlag="-o";
	string outputFile="/dev/null";
	args.push_back(const_cast<char*>(configuration.compiler().data()));
	args.push_back(versionFlag.data());
	args.push_back(nullptr);
	CompilerType type=LLVM;
	auto data=run_and_get_output(args);
	if(data)
	{
		if(data->second==0)
		{
			string&s=data->first;
			if(!s.starts_with("clang"))
			{
				type=GNU;
			}
		}
		else
		{
			println(cerr,"Executing {} failed with exit code {}.",configuration.compiler(),data->second);
		}
	}
	args.erase(args.begin()+1,args.end());
	args.push_back(verboseFlag.data());
	if(type==LLVM)
	{
		args.push_back(const_cast<char*>(CBP_STDLIB_FLAG.data()));
	}
	for(string_view sv:configuration.compilerOptions)
	{
		args.push_back(const_cast<char*>(sv.data()));
	}
	args.push_back(const_cast<char*>(programPath.data()));
	args.push_back(outputFlag.data());
	args.push_back(outputFile.data());
	args.push_back(nullptr);
	data=run_and_get_output(args);
	vector<string>directories;
	if(data)
	{
		if(data->second==0)
		{
			bool insert=false;
			for(auto sr:views::split(data->first,"\n"sv))
			{
				string_view sv{sr};
				if(sv=="End of search list.")
				{
					insert=false;
				}
				if(insert)
				{
					directories.push_back(canonical(path{sv.substr(1)}).string());
				}
				if(sv=="#include <...> search starts here:")
				{
					insert=true;
				}
			}
		}
		else
		{
			println(cerr,"Executing {} with arguments {} failed with exit code {}.",configuration.compiler(),views::take(args,args.size()-1),data->second);
		}
	}
	return{type,std::move(directories)};
}
export class CompilerConfigurer
{
	CompilerType type;
	vector<string>includeDirectories;
	vector<char*>compilerArguments;
public:
	CompilerConfigurer(CompilerType ct,vector<string>id)
		noexcept
		:type(ct),includeDirectories(std::move(id)),compilerArguments()
	{}
	CompilerConfigurer()
		noexcept
		:CompilerConfigurer(LLVM,{})
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
	string getEitherSTDModulePath(string name)
		const
	{
		if(type==GNU)
		{
			return includeDirectories.front()+"/bits/"+name+".cc";
		}
		else
		{
			string first="/usr/local/share/libc++/v1/"+name+".cppm";
			string second="/usr/share/libc++/v1/"+name+".cppm";
			return exists(path{first})?first:second;
		}
	}
	string getSTDModulePath()
	{
		return getEitherSTDModulePath("std");
	}
	string getSTDCompatModulePath()
	{
		return getEitherSTDModulePath("std.compat");
	}
	string getModuleExtension()
		const noexcept
	{
		return type==GNU?".gcm":".pcm";
	}
	string moduleNameToFile(string_view name,string_view outputDirectory)
		const
	{
		string file{name};
		for(char&c:views::filter(file,bind_front(equal_to<char>(),':')))
		{
			c='-';
		}
		if(name.size())
		{
			file+=type==GNU?".gcm":".pcm";
			file=prependOutputDirectory(file,outputDirectory,type==GNU);
		}
		return file;
	}
	string headerNameToOutput(string_view name,string_view outputDirectory)
		const
	{
		string s(name);
		s+=".pcm";
		return prependOutputDirectory(std::move(s),outputDirectory,type==GNU);
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
	void addArguments(const BuildConfiguration&configuration)
	{
		const char*flagData=CBP_CLANG_MODULE_PATH.data();
		compilerArguments.push_back(const_cast<char*>(configuration.compiler().data()));
		bool addLanguageVersion=ranges::find_if(configuration.compilerOptions,[](string_view sv){return sv.starts_with("-std=");})==configuration.compilerOptions.end();
		if(addLanguageVersion)
		{
			compilerArguments.push_back(const_cast<char*>(CBP_LANGUAGE_VERSION.data()));
		}
		if(configuration.objectDirectory().size()&&type==GNU)
		{
			path cmcache("gcm.cache");
			path target{configuration.objectDirectory()};
			if(!exists(cmcache))
			{
				create_directory_symlink(target,cmcache);
			}
			else if(is_symlink(cmcache))
			{
				if(read_symlink(cmcache)!=target)
				{
					remove(cmcache);
					create_directory_symlink(target,cmcache);
				}
			}
		}
		switch(type)
		{
			case LLVM:
				if(configuration.objectDirectory().size())
				{
					clangPrebuiltModuleFlag=string{CBP_CLANG_MODULE_PATH.data(),CBP_CLANG_MODULE_PATH.size()-1};
					clangPrebuiltModuleFlag.append_range(configuration.objectDirectory());
					flagData=clangPrebuiltModuleFlag.data();
				}
				compilerArguments.push_back(const_cast<char*>(CBP_STDLIB_FLAG.data()));
				compilerArguments.push_back(const_cast<char*>(flagData));
				break;
			case GNU:
				compilerArguments.push_back(const_cast<char*>(CBP_GCC_MODULE_FLAG.data()));
				break;
		}
		compilerArguments.append_range(views::transform(configuration.compilerOptions,svConstCaster));
	}
	vector<char*>compileFile(string&filename,string&outputname,const string&moduleName,span<const ImportUnit>imports,const BuildConfiguration&configuration,bool notInterface,bool isHeader)
		const
	{
		vector<char*>output=compilerArguments;
		if(type==LLVM)
		{
			clangHeaderFlagStorage.clear();
			for(const ImportUnit&unit:views::filter(imports,[](const ImportUnit&u){return u.type!=MODULE;}))
			{
				clangHeaderFlagStorage.push_back("-fmodule-file="+headerNameToOutput(unit.name,configuration.objectDirectory()));
			}
			output.append_range(views::transform(clangHeaderFlagStorage,[](string&s){return s.data();}));
			if(!notInterface&&!isHeader)
			{
				output.push_back(const_cast<char*>(CBP_LANGUAGE_FLAG.data()));
				output.push_back(const_cast<char*>(CBP_MODULE_LANGUAGE.data()));
			}
		}
		if(isHeader)
		{
			if(type==LLVM)
			{
				output.push_back(svConstCaster(CBP_CLANG_PRECOMPILE_FLAG));
			}
			output.push_back(svConstCaster(CBP_LANGUAGE_FLAG));
			output.push_back(svConstCaster(CBP_HEADER_LANGUAGE));
		}
		output.push_back(filename.data());
		if(!isHeader)
		{
			output.push_back(const_cast<char*>(CBP_COMPILE_FLAG.data()));
		}
		if(type==LLVM||!isHeader)
		{
			output.push_back(const_cast<char*>(CBP_OUTPUT_FLAG.data()));
			output.push_back(outputname.data());
		}
		if(type==LLVM&&!notInterface&&!isHeader)
		{
			clangModulePath="-fmodule-output=";
			clangModulePath+=moduleNameToFile(moduleName,configuration.objectDirectory());
			output.push_back(clangModulePath.data());
		}
		output.push_back(nullptr);
		return output;
	}
	vector<char*>linkProgram(string&artifact,BuildConfiguration&configuration,span<string>files)
		const
	{
		vector<char*>trueLinkerArguments{const_cast<char*>(configuration.compiler().data())};
		if(type==LLVM)
		{
			trueLinkerArguments.push_back(svConstCaster(CBP_STDLIB_FLAG));
		}
		trueLinkerArguments.append_range(views::transform(files,[](string&s){return s.data();}));
		trueLinkerArguments.append_range(views::transform(configuration.linkerOptions,svConstCaster));
		trueLinkerArguments.push_back(svConstCaster(CBP_OUTPUT_FLAG));
		trueLinkerArguments.push_back(artifact.data());
		trueLinkerArguments.push_back(nullptr);
		return trueLinkerArguments;
	}
};
