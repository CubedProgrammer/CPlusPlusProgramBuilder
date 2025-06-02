import std;
import configuration;
import cpbuild;
import flag;
using namespace std;
int mainpp(span<string_view>args)
	noexcept
{
	string_view program=args.front();
	BuildConfiguration configuration=parseBuildConfiguration(args.subspan(1));
	auto ci=getCompilerInformation(configuration);
	ProgramBuilder&builder=ProgramBuilder::getInstance(program,std::move(ci),std::move(configuration));
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
