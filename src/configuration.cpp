export module configuration;
import std;
using namespace std;
using views::enumerate;
constexpr char COMPILER_OPTION_FLAG='c';
constexpr char LINKER_OPTION_FLAG='l';
constexpr char OUTPUT_OPTION_FLAG='o';
constexpr char DISPLAY_OPTION_FLAG='s';
constexpr char FORCE_OPTION_FLAG='f';
export struct BuildConfiguration
{
	string_view objectDirectory;
	span<string_view>compilerOptions;
	span<string_view>linkerOptions;
	bool displayCommand;
	bool forceCompile;
	vector<string_view>targets;
};
export BuildConfiguration parseBuildConfiguration(span<string_view>arguments)
{
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
		if(sv.size()>=2&&sv.front()=='-')
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
					break;
				case DISPLAY_OPTION_FLAG:
					configuration.displayCommand=true;
					break;
				case FORCE_OPTION_FLAG:
					configuration.forceCompile=true;
					break;
				default:
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
