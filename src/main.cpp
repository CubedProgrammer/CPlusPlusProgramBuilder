import cpbuild;
constexpr unsigned MAJOR=0;
constexpr unsigned MINOR=3;
constexpr unsigned PATCH=3;
using namespace std;
using filesystem::current_path;
int mainpp(span<string_view>args)
	noexcept
{
	string_view program=args.front();
	string emptyPath;
	array<string_view,2>emptyArgs;
	if(args.size()==1)
	{
		println("inside if statement");
		emptyPath=absolute(current_path()).filename().string()+".conf";
		emptyArgs[0]="-b";
		emptyArgs[1]=emptyPath;
		args=emptyArgs;
	}
	else
	{
		args=args.subspan(1);
	}
	println("before parsing config");
	BuildConfiguration configuration=parseBuildConfiguration(args);
	println("{} after parsing config",configuration.targets);
	if(configuration.isHelp()||configuration.isVersion()||configuration.isInitialize())
	{
		if(configuration.isInitialize())
		{
			if(!filesystem::create_directory("bin"))
			{
				println(cerr,"Creating directory bin failed.");
			}
			if(!filesystem::create_directory("src"))
			{
				println(cerr,"Creating directory src failed.");
			}
			ofstream out(configuration.getFinalOutputFile().replace_extension("conf").string());
			out<<"-c\n{\n-Wall\n}\n--dependency-cache\n-s\n-j4\n-o\nbin\nsrc\n";
		}
		if(configuration.isVersion())
		{
			println("{} version {}.{}.{}",program,MAJOR,MINOR,PATCH);
		}
		if(configuration.isHelp())
		{
			println("{} [OPTIONS...] [DIRECTORIES...]\n",program);
			println("-{}{{N}}: Command line arguments passed to the C++ compiler, exactly N arguments must follow.",COMPILER_OPTION_FLAG);
			println("-{}{{N}}: Command line arguments passed to the C++ linker, exactly N arguments must follow.",LINKER_OPTION_FLAG);
			println("-{} DIRECTORY: Directory to store compiled module files and object files.",OUTPUT_OPTION_FLAG);
			println("-{}: Display the commands being executed.",DISPLAY_OPTION_FLAG);
			println("-{}: Compile files that have not changed, specify twice to compile external module interface units that have not changed.",FORCE_OPTION_FLAG);
			println("-{} FILE: Specify the path to the artifact, the final product of compilation.",ARTIFACT_OPTION_FLAG);
			println("-{}{{N}}: Specifies that N processess allowed to run in parallel, set this to the number of threads on this computer for maximum performance.",PARALLEL_OPTION_FLAG);
			println("-{} FILE: Read options from this file, the file must have one option per line.",FILE_OPTION_FLAG);
			println("-{}: Initialize the current directory as a project with a simple configuration file.",INITIALIZE_OPTION_FLAG);
			println("--compiler FILE: Specifies the path to the compiler to use.");
			println("--display-module-map: Print a list of values, the paths of the module interface units of the external modules imported.");
			println("--module-interface FILE: Read the specified file for the list of external module interface units to read, one file per line.");
			println("--display-dependency-graph: Print a list of values, a source file and all files it imports, each value separated by two line feeds.");
			println("--version: Displays the version of the program.");
			println("--help: Displays this wall of text.");
		}
	}
	else
	{
		if(configuration.targets.size()==0)
		{
			configuration.targets.push_back(".");
		}
		ProgramBuilder&builder=ProgramBuilder::getInstance(std::move(configuration));
		println("got instance");
		builder.cpbuild();
	}
	return 0;
}
int main(int argl,char**argv)
{
	vector<string_view>args(argv,argv+argl);
	return mainpp(args);
}
