import std;
import configuration;
import cpbuild;
using namespace std;
int mainpp(span<string_view>args)
	noexcept
{
	string_view program=args.front();
	CompilerType compilertype=args[1]=="GNU"?GNU:LLVM;
	BuildConfiguration configuration=parseBuildConfiguration(args.subspan(2));
	ProgramBuilder&builder=ProgramBuilder::getInstance(program,compilertype,std::move(configuration));
	cout.write(program.data(),program.size());
	cout.put('\n');
	builder.cpbuild();
	return 0;
}
int main(int argl,char**argv)
{
	vector<string_view>args(argv,argv+argl);
	return mainpp(args);
}
