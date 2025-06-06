export module configuration;
import std;
using namespace std;
export constexpr char COMPILER_OPTION_FLAG='c';
export constexpr char LINKER_OPTION_FLAG='l';
export constexpr char OUTPUT_OPTION_FLAG='o';
export constexpr char DISPLAY_OPTION_FLAG='s';
export constexpr char FORCE_OPTION_FLAG='f';
export constexpr char ARTIFACT_OPTION_FLAG='a';
export constexpr char PARALLEL_OPTION_FLAG='j';
constexpr char LONG_OPTION_FLAG='-';
export constexpr string_view CBP_COMPILER_NAME="c++";
export struct BuildConfiguration
{
	string_view compiler;
	string_view objectDirectory;
	string_view artifact;
	span<string_view>compilerOptions;
	span<string_view>linkerOptions;
	uint8_t binaryOptions;
	uint8_t threadCount;
	vector<string_view>targets;
	BuildConfiguration()
		:compiler(CBP_COMPILER_NAME),objectDirectory(),artifact(),compilerOptions(),linkerOptions(),binaryOptions(),threadCount(1),targets()
	{}
	bool isDisplayCommand()
		const noexcept
	{
		return(binaryOptions&1)==1;
	}
	bool isForceRecompile()
		const noexcept
	{
		return(binaryOptions>>1&1)==1;
	}
	bool isForceRecompileEnhanced()
		const noexcept
	{
		return(binaryOptions>>2&1)==1;
	}
	bool isHelp()
		const noexcept
	{
		return(binaryOptions>>3&1)==1;
	}
	bool isVersion()
		const noexcept
	{
		return(binaryOptions>>4&1)==1;
	}
	void setDisplayCommand()
		noexcept
	{
		binaryOptions|=1;
	}
	void setForceRecompile()
		noexcept
	{
		binaryOptions|=2;
	}
	void setForceRecompileEnhanced()
		noexcept
	{
		binaryOptions|=4;
	}
	void setHelp()
		noexcept
	{
		binaryOptions|=8;
	}
	void setVersion()
		noexcept
	{
		binaryOptions|=16;
	}
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
					configuration.setDisplayCommand();
					break;
				case FORCE_OPTION_FLAG:
					if(configuration.isForceRecompile())
					{
						configuration.setForceRecompileEnhanced();
					}
					else
					{
						configuration.setForceRecompile();
					}
					break;
				case ARTIFACT_OPTION_FLAG:
					consumeInto=&configuration.artifact;
					consume=1;
					break;
				case PARALLEL_OPTION_FLAG:
					if(sv.size()>2)
					{
						uint8_t v=1;
						from_chars(sv.data()+2,sv.data()+sv.size(),v);
						configuration.threadCount=v;
					}
					break;
				case LONG_OPTION_FLAG:
					if(sv.substr(2)=="compiler")
					{
						consumeInto=&configuration.compiler;
						consume=1;
					}
					else if(sv.substr(2)=="help")
					{
						configuration.setHelp();
					}
					else if(sv.substr(2)=="version")
					{
						configuration.setVersion();
					}
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
