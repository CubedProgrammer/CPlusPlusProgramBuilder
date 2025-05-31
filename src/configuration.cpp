export module configuration;
import std;
using namespace std;
constexpr char COMPILER_OPTION_FLAG='c';
constexpr char LINKER_OPTION_FLAG='l';
constexpr char OUTPUT_OPTION_FLAG='o';
constexpr char DISPLAY_OPTION_FLAG='s';
constexpr char FORCE_OPTION_FLAG='f';
constexpr char ARTIFACT_OPTION_FLAG='a';
export struct BuildConfiguration
{
	string_view objectDirectory;
	string_view artifact;
	span<string_view>compilerOptions;
	span<string_view>linkerOptions;
	bool displayCommand;
	bool forceCompile;
	vector<string_view>targets;
	BuildConfiguration()
		:objectDirectory(),artifact(),compilerOptions(),linkerOptions(),displayCommand(),forceCompile(),targets()
	{}
};
export BuildConfiguration parseBuildConfiguration(span<string_view>arguments)
{
	using views::enumerate;
	BuildConfiguration configuration;
	variant<monostate,string_view*,span<string_view>*>consumeInto;
	size_t consume=0;
	for(auto[index,sv]:enumerate(arguments))
	{
		if(consume)
		{
			if(consumeInto.index())
			{
				if(consumeInto.index()==2)
				{
					**get_if<2>(&consumeInto)=arguments.subspan(index,consume);
				}
				else
				{
					**get_if<1>(&consumeInto)=sv;
				}
				consumeInto=monostate{};
			}
			--consume;
		}
		else if(sv.size()>=2&&sv.front()=='-')
		{
			switch(sv[1])
			{
				case COMPILER_OPTION_FLAG:
					consumeInto=&configuration.compilerOptions;
					break;
				case LINKER_OPTION_FLAG:
					consumeInto=&configuration.linkerOptions;
					break;
				case OUTPUT_OPTION_FLAG:
					consumeInto=&configuration.objectDirectory;
					consume=1;
					break;
				case DISPLAY_OPTION_FLAG:
					configuration.displayCommand=true;
					break;
				case FORCE_OPTION_FLAG:
					configuration.forceCompile=true;
					break;
				case ARTIFACT_OPTION_FLAG:
					consumeInto=&configuration.artifact;
					consume=1;
					break;
				default:
					println("Unrecognized flag {} from argument {} will be ignored.",sv[1],sv);
					break;
			}
			if(consumeInto.index()==2)
			{
				if(sv.size()==2)
				{
					consume=1;
				}
				else
				{
					from_chars(sv.data()+2,sv.data()+sv.size(),consume);
				}
			}
		}
		else
		{
			configuration.targets.push_back(sv);
		}
	}
	return configuration;
}
