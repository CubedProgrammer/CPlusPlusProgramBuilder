export module compiler.factory;
export import compiler.clang;
export import compiler.gcc;
using namespace std;
using filesystem::path;
constexpr string_view CBP_PROGRAM_PATH="/tmp/C++ProgramBuilderBasicProgram.c++";
constexpr string_view CBP_SAMPLE_PROGRAM="int main(int,char**){return 0;}";
constexpr string_view CBP_VERSION_FLAG="--version";
constexpr string_view CBP_VERBOSE_FLAG="-v";
export constexpr string_view CBP_NULL_DEVICE="/dev/null";
CompilerType getCompilerType(const BuildConfiguration&configuration)
{
	CompilerType type=LLVM;
	vector<char*>args{svConstCaster(configuration.compiler()),svConstCaster(CBP_VERSION_FLAG)};
	auto data=launch_program(args,PIPE_OUTPUT);
	if(data)
	{
		/*if(data->second==0)
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
		}*/
	}
	return type;
}
export unique_ptr<BaseCompilerConfigurer>getCompiler(const BuildConfiguration&configuration,ParallelProcessManager*ppm)
{
	CompilerType ct=getCompilerType(configuration);
	vector<char*>args{svConstCaster(configuration.compiler()),svConstCaster(CBP_VERBOSE_FLAG)};
	{
		ofstream fout(string{CBP_PROGRAM_PATH});
		println(fout,"{}",CBP_SAMPLE_PROGRAM);
	}
	if(ct==LLVM)
	{
		args.push_back(svConstCaster(CBP_STDLIB_FLAG));
	}
	args.append_range(views::transform(configuration.compilerOptions,svConstCaster));
	args.push_back(svConstCaster(CBP_PROGRAM_PATH));
	args.push_back(svConstCaster(CBP_OUTPUT_FLAG));
	args.push_back(svConstCaster(CBP_NULL_DEVICE));
	args.push_back(nullptr);
	//optional<pair<string,int>>dataO=launch_program(args,PIPE_ERROR);
	vector<string>directories;
	/*if(dataO&&dataO->second==0)
	{
		bool insert=false;
		for(auto sr:views::split(dataO->first,"\n"sv))
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
		println(cerr,"Executing {} with arguments {} failed with exit code {}.",configuration.compiler(),views::take(args,args.size()-1),dataO->second);
	}*/
	return ct==GNU?unique_ptr<BaseCompilerConfigurer>(make_unique<GCCConfigurer>(std::move(directories),&configuration,ppm)):unique_ptr<BaseCompilerConfigurer>(make_unique<ClangConfigurer>(std::move(directories),&configuration,ppm));
}