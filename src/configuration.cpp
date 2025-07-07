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
export constexpr char FILE_OPTION_FLAG='b';
constexpr char LONG_OPTION_FLAG='-';
export constexpr string_view CBP_COMPILER_NAME="c++";
export struct BuildConfiguration
{
	array<string_view,4>svOptions;
	span<string_view>compilerOptions;
	span<string_view>linkerOptions;
	uint8_t binaryOptions;
	uint8_t threadCount;
	vector<string_view>targets;
	vector<string>configurationFileStorage;
	vector<vector<string_view>>configurationSpanStorage;
	BuildConfiguration()
		:svOptions{CBP_COMPILER_NAME},compilerOptions(),linkerOptions(),binaryOptions(),threadCount(1),targets(),configurationFileStorage(),configurationSpanStorage()
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
	bool isDumpModuleMap()
		const noexcept
	{
		return(binaryOptions>>5&1)==1;
	}
	bool isDumpDependencyGraph()
		const noexcept
	{
		return(binaryOptions>>6&1)==1;
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
	void setDumpModuleMap()
		noexcept
	{
		binaryOptions|=32;
	}
	void setDumpDependencyGraph()
		noexcept
	{
		binaryOptions|=64;
	}
	constexpr decltype(auto)compiler(this auto&self)
		noexcept
	{
		return self.svOptions[0];
	}
	constexpr decltype(auto)objectDirectory(this auto&self)
		noexcept
	{
		return self.svOptions[1];
	}
	constexpr decltype(auto)artifact(this auto&self)
		noexcept
	{
		return self.svOptions[2];
	}
	constexpr decltype(auto)moduleMapCache(this auto&self)
		noexcept
	{
		return self.svOptions[3];
	}
};
string read_option_file(string_view fname)
{
	string s;
	array<char,8192>buffer;
	ifstream fin{string{fname}};
	while(fin.read(buffer.data(),buffer.size()))
	{
		s.append_range(views::take(buffer,fin.gcount()));
	}
	s.append_range(views::take(buffer,fin.gcount()));
	ranges::fill(views::filter(s,bind_front(equal_to<char>(),'\n')),'\0');
	return s;
}
export BuildConfiguration parseBuildConfiguration(span<string_view>arguments)
{
	BuildConfiguration configuration;
	variant<monostate,string_view*,span<string_view>*>consumeInto;
	const span<string_view>originalArguments=arguments;
	vector<string_view>optionFileStack;
	vector<span<string_view>>optionSpanStack;
	vector<array<size_t,2>>optionIndexStack;
	string_view flagname;
	size_t consume=0;
	bool nextOptionFile=false;
	optionIndexStack.push_back({0,arguments.size()});
	while(optionIndexStack.size())
	{
		size_t&index=optionIndexStack.back().front();
		size_t length=optionIndexStack.back().back();
		size_t stackLast=optionIndexStack.size()-1;
		if(index<length)
		{
			string_view sv=arguments[index];
			if(nextOptionFile)
			{
				configuration.configurationFileStorage.push_back(read_option_file(sv));
				optionFileStack.push_back(string_view{configuration.configurationFileStorage.back()});
				configuration.configurationSpanStorage.push_back(ranges::to<vector<string_view>>(views::transform(views::split(optionFileStack.back(),"\0"sv),[](auto rg){return string_view{rg};})));
				optionSpanStack.push_back(configuration.configurationSpanStorage.back());
				arguments=optionSpanStack.back();
				optionIndexStack.push_back({0,arguments.size()});
				nextOptionFile=false;
			}
			else if(consume)
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
						consumeInto=&configuration.objectDirectory();
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
						consumeInto=&configuration.artifact();
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
					case FILE_OPTION_FLAG:
						nextOptionFile=true;
						break;
					case LONG_OPTION_FLAG:
						flagname=sv.substr(2);
						if(flagname=="compiler")
						{
							consumeInto=&configuration.compiler();
							consume=1;
						}
						else if(flagname=="help")
						{
							configuration.setHelp();
						}
						else if(flagname=="version")
						{
							configuration.setVersion();
						}
						else if(flagname=="display-module-map")
						{
							configuration.setDumpModuleMap();
						}
						else if(flagname=="module-interface")
						{
							consumeInto=&configuration.moduleMapCache();
							consume=1;
						}
						else if(flagname=="display-dependency-graph")
						{
							configuration.setDumpDependencyGraph();
						}
						else
						{
							println("Unrecognized flag {} will be ignored.",flagname);
						}
						break;
					default:
						println("Unrecognized flag {} from argument {} will be ignored, if this was meant to be a file name, prefix with ./",sv[1],sv);
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
			else if(sv.size())
			{
				configuration.targets.push_back(sv);
			}
			++optionIndexStack[stackLast].front();
		}
		else
		{
			optionIndexStack.pop_back();
			if(optionIndexStack.size()>1)
			{
				optionFileStack.pop_back();
				optionSpanStack.pop_back();
				arguments=optionSpanStack.back();
			}
			else if(optionIndexStack.size()==1)
			{
				optionFileStack.pop_back();
				optionSpanStack.pop_back();
				arguments=originalArguments;
			}
		}
	}
	return configuration;
}
