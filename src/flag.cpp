export module flag;
import std;
import configuration;
import utils;
using namespace std;
constexpr string_view source="int main(int,char**){return 0;}";
constexpr string_view programPath="/tmp/C++ProgramBuilderBasicProgram.c++";
export constexpr string_view CBP_COMPILER_NAME="c++";
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
	string verboseFlag="-v";
	string outputFlag="-o";
	string outputFile="/dev/null";
	args.push_back(const_cast<char*>(CBP_COMPILER_NAME.data()));
	args.push_back(verboseFlag.data());
	for(string_view sv:configuration.compilerOptions)
	{
		args.push_back(const_cast<char*>(sv.data()));
	}
	args.push_back(const_cast<char*>(programPath.data()));
	args.push_back(outputFlag.data());
	args.push_back(outputFile.data());
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
			println(cerr,"Executing {} on {} failed with exit code {}.",CBP_COMPILER_NAME,source,data->second);
		}
	}
	return{type,{}};
}
