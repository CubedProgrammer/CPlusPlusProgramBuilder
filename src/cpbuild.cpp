export module cpbuild;
import std;
import configuration;
import dependency;
import flag;
import process;
using std::chrono::file_clock;
using std::filesystem::current_path,std::filesystem::file_time_type,std::filesystem::path,std::filesystem::recursive_directory_iterator;
using namespace std;
using namespace chrono_literals;
constexpr string_view CBP_ALLOWED_EXTENSIONS="c++ c++m cc ccm cpp cppm cxx cxxm";
constexpr string_view CBP_GCC_MODULE_FLAG="-fmodules";
constexpr string_view CBP_CLANG_MODULE_PATH="-fprebuilt-module-path=.";
constexpr string_view CBP_COMPILE_FLAG="-c";
constexpr string_view CBP_OUTPUT_FLAG="-o";
constexpr string_view CBP_LANGUAGE_FLAG="-x";
constexpr string_view CBP_MODULE_LANGUAGE="c++-module";
constexpr string_view CBP_LANGUAGE_VERSION="-std=c++20";
constexpr string_view CBP_STDLIB_FLAG="-stdlib=libc++";
constexpr string_view CBP_CLANG="clang++";
constexpr string_view CBP_GCC="g++";
export struct ModuleCompilation
{
	vector<ImportUnit>dependency;
	path output;
	file_time_type source;
	file_time_type object;
	vector<unsigned>dependentProcesses;
	string name;
	bool visited;
};
export using FileModuleMap=unordered_map<path,ModuleCompilation>;
export struct ModuleConnection
{
	FileModuleMap files;
	unordered_map<string,path>primaryModuleInterfaceUnits;
};
export class ProgramBuilder;
extern unique_ptr<ProgramBuilder>singletonPointerProgramBuilder;
struct ProgramBuilderConstructorTag{};
class ProgramBuilder
{
	string_view name;
	pair<CompilerType,vector<string>>typeAndInclude;
	BuildConfiguration options;
	vector<char*>compilerArguments;
	vector<string>linkerArguments;
	ModuleConnection connections;
	ParallelProcessManager pm;
public:
	ProgramBuilder(ProgramBuilderConstructorTag,string_view name,pair<CompilerType,vector<string>>tai,BuildConfiguration op)
		noexcept
		:name(name),typeAndInclude(std::move(tai)),options(std::move(op)),compilerArguments(),linkerArguments(),connections(),pm()
	{}
	ProgramBuilder()=delete;
	ProgramBuilder(const ProgramBuilder&)=delete;
	ProgramBuilder(ProgramBuilder&&)=delete;
	ProgramBuilder&operator=(const ProgramBuilder&)=delete;
	ProgramBuilder&operator=(ProgramBuilder&&)=delete;
	path getOutputFile(path p)
		const noexcept
	{
		if(p.has_extension())
		{
			p.replace_extension(path{"o"});
		}
		if(options.objectDirectory.size())
		{
			if(options.targets.size()==1)
			{
				string pathString=p.string();
				size_t index=pathString.find('/');
				index=index==string::npos?pathString.size()-1:index;
				string_view sv{pathString.cbegin()+index+1,pathString.cend()};
				p=sv;
			}
			p=path{options.objectDirectory}/p;
		}
		return p;
	}
	path getFinalOutputFile()
		const noexcept
	{
		if(options.artifact.size()==0)
		{
			if(options.targets.size()&&is_regular_file(path{options.targets.front()}))
			{
				path output(options.targets.front());
				output.replace_extension();
				return output;
			}
			else
			{
				return absolute(current_path()).filename();
			}
		}
		else
		{
			return path{options.artifact};
		}
	}
	void build_file(FileModuleMap::reference data)
	{
		vector<pair<FileModuleMap::pointer,vector<ImportUnit>::const_iterator>>filestack;
		size_t argumentLength=compilerArguments.size()-options.compilerOptions.size();
		CompilerType ct=typeAndInclude.first;
		println("Building {}",data.first.string());
		if(!data.second.visited)
		{
			filestack.emplace_back(&data,data.second.dependency.cbegin());
		}
		while(filestack.size())
		{
			auto&[entry,dependIt]=filestack.back();
			auto&[filepath,md]=*entry;
			if(dependIt!=md.dependency.cend())
			{
				auto it=connections.primaryModuleInterfaceUnits.find(dependIt->name);
				if(it!=connections.primaryModuleInterfaceUnits.end())
				{
					auto fileIt=connections.files.find(it->second);
					if(!fileIt->second.visited)
					{
						filestack.emplace_back(to_address(fileIt),fileIt->second.dependency.cbegin());
					}
					else if(fileIt->second.source>fileIt->second.object)
					{
						md.object=md.source-1s;
					}
				}
				++dependIt;
			}
			else
			{
				linkerArguments.push_back(md.output.string());
				if(options.forceCompile||md.source>md.object)
				{
					string filepathString=filepath.string();
					string outfileString=md.output.string();
					string moduleString;
					vector<char*>addback;
					compilerArguments.push_back(filepathString.data());
					compilerArguments.push_back(const_cast<char*>(CBP_COMPILE_FLAG.data()));
					compilerArguments.push_back(const_cast<char*>(CBP_OUTPUT_FLAG.data()));
					compilerArguments.push_back(outfileString.data());
					if(ct==LLVM)
					{
						if(md.name.size())
						{
							moduleString="-fmodule-output=";
							if(options.objectDirectory.size())
							{
								moduleString+=string{options.objectDirectory};
								moduleString+='/';
							}
							moduleString+=md.name;
							moduleString+=".pcm";
							for(char&c:moduleString)
							{
								c=c==':'?'-':c;
							}
							compilerArguments.push_back(moduleString.data());
						}
						else
						{
							addback=ranges::to<vector<char*>>(views::take(views::drop(compilerArguments,argumentLength-2),2));
							compilerArguments.erase(compilerArguments.begin()+argumentLength-2,compilerArguments.begin()+argumentLength);
						}
					}
					println("{}",views::transform(md.dependency,&ImportUnit::name));
					compilerArguments.push_back(nullptr);
					pm.run(compilerArguments,options.displayCommand);
					if(addback.size())
					{
						compilerArguments.insert_range(compilerArguments.begin()+argumentLength-2,addback);
					}
					compilerArguments.erase(compilerArguments.begin()+argumentLength+options.compilerOptions.size(),compilerArguments.end());
				}
				md.visited=true;
				filestack.pop_back();
			}
		}
	}
	void add_file(path&&p)
	{
		ModuleData data=parseModuleData(p);
		const path object=getOutputFile(p);
		const file_time_type lastModify=last_write_time(p);
		const file_time_type objectModify=exists(object)?last_write_time(object):lastModify-1s;
		connections.primaryModuleInterfaceUnits.emplace(data.name,p);
		connections.files.emplace(std::move(p),ModuleCompilation(std::move(data.imports),object,lastModify,objectModify,{},std::move(data.name),false));
	}
	void add_directory(const path&p)
	{
		using namespace string_view_literals;
		auto permitted=ranges::to<vector<string>>(views::split(CBP_ALLOWED_EXTENSIONS," "sv));
		for(const auto&en:recursive_directory_iterator(p))
		{
			const path&current=en.path();
			if(en.is_directory())
			{
				create_directory(getOutputFile(current));
			}
			else if(current.has_extension()&&ranges::contains(permitted,current.extension().string().substr(1))&&en.is_regular_file())
			{
				add_file(path(current));
			}
		}
	}
	void cpbuild()
	{
		span<string_view>targets=options.targets;
		string clangPrebuiltModuleFlag;
		CompilerType ct=typeAndInclude.first;
		const char*flagData=CBP_CLANG_MODULE_PATH.data();
		compilerArguments.push_back(const_cast<char*>(options.compiler.data()));
		linkerArguments.push_back(string{options.compiler});
		bool addLanguageVersion=ranges::find_if(options.compilerOptions,[](string_view sv){return sv.starts_with("-std=");})==options.compilerOptions.end();
		if(addLanguageVersion)
		{
			compilerArguments.push_back(const_cast<char*>(CBP_LANGUAGE_VERSION.data()));
		}
		if(options.objectDirectory.size()&&ct==GNU)
		{
			path cmcache("gcm.cache");
			path target{options.objectDirectory};
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
		switch(ct)
		{
			case LLVM:
				if(options.objectDirectory.size())
				{
					clangPrebuiltModuleFlag=string{CBP_CLANG_MODULE_PATH.data(),CBP_CLANG_MODULE_PATH.size()-1};
					clangPrebuiltModuleFlag.append_range(options.objectDirectory);
					flagData=clangPrebuiltModuleFlag.data();
				}
				compilerArguments.push_back(const_cast<char*>(CBP_STDLIB_FLAG.data()));
				compilerArguments.push_back(const_cast<char*>(flagData));
				compilerArguments.push_back(const_cast<char*>(CBP_LANGUAGE_FLAG.data()));
				compilerArguments.push_back(const_cast<char*>(CBP_MODULE_LANGUAGE.data()));
				linkerArguments.push_back(string{CBP_STDLIB_FLAG});
				break;
			case GNU:
				compilerArguments.push_back(const_cast<char*>(CBP_GCC_MODULE_FLAG.data()));
				break;
		}
		auto svConstCaster=[](string_view sv){return const_cast<char*>(sv.data());};
		compilerArguments.append_range(views::transform(options.compilerOptions,svConstCaster));
		for(path t:targets)
		{
			if(is_directory(t))
			{
				add_directory(t);
			}
			else
			{
				add_file(std::move(t));
			}
		}
		ranges::for_each(connections.files,bind_front(&ProgramBuilder::build_file,this));
		pm.wait_remaining_processes();
		const path product=getFinalOutputFile();
		linkerArguments.push_back(string{CBP_OUTPUT_FLAG});
		linkerArguments.push_back(product.string());
		auto trueLinkerArguments=ranges::to<vector<char*>>(views::transform(linkerArguments,[](string&s){return s.data();}));
		trueLinkerArguments.append_range(views::transform(options.linkerOptions,svConstCaster));
		trueLinkerArguments.push_back(nullptr);
		pm.run(trueLinkerArguments,options.displayCommand);
		pm.wait_remaining_processes();
	}
	~ProgramBuilder()=default;
	static ProgramBuilder&getInstance(string_view program,pair<CompilerType,vector<string>>tai,BuildConfiguration options)
		noexcept
	{
		if(!singletonPointerProgramBuilder)
		{
			singletonPointerProgramBuilder=make_unique<ProgramBuilder>(ProgramBuilderConstructorTag{},program,std::move(tai),std::move(options));
		}
		return*singletonPointerProgramBuilder;
	}
};
unique_ptr<ProgramBuilder>singletonPointerProgramBuilder;
