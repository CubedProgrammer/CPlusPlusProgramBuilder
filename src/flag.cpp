export module flag;
import std;
import configuration;
import utils;
using namespace std;
constexpr string_view source="int main(int,char**){return 0;}";
constexpr string_view programPath="/tmp/C++ProgramBuilderBasicProgram.c++";
constexpr string_view CBP_STDLIB_FLAG="-stdlib=libc++";
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
	args.push_back(const_cast<char*>(configuration.compiler.data()));
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
			println(cerr,"Executing {} failed with exit code {}.",configuration.compiler,data->second);
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
	return{type,{}};
}
