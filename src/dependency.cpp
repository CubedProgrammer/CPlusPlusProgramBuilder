export module dependency;
export import configuration;
export import utils;
using namespace std;
using std::filesystem::path;
export enum ImportType
{
	MODULE,SYS_HEADER,LOCAL_HEADER
};
export struct ImportUnit
{
	string name;
	ImportType type;
	constexpr bool operator==(const ImportUnit&)const noexcept=default;
};
export struct ModuleData
{
	string name;
	vector<ImportUnit>imports;
};
export path replaceMove(const BuildConfiguration&options,path file,const path&extension)
{
	if(file.has_extension())
	{
		file.replace_extension(extension);
	}
	if(options.objectDirectory().size())
	{
		if(options.targets.size()==1)
		{
			string pathString=file.string();
			size_t index=pathString.rfind('/')+1;
			string_view sv{pathString.cbegin()+index,pathString.cend()};
			file=sv;
		}
		file=path{options.objectDirectory()}/file.relative_path();
	}
	return file;
}
export optional<path>preprocess(const BuildConfiguration&options,const path&file)
{
	optional<path>outOpt;
	path out=replaceMove(options,file,path{"ii"});
	string fileString=file.string();
	string outString=out.string();
	char preprocessOption[]="-E";
	char outOption[]="-o";
	vector<char*>preprocessCommand;
	create_directories(out.parent_path());
	preprocessCommand.reserve(options.compilerOptions.size()+4);
	preprocessCommand.push_back(svConstCaster(options.compiler()));
	preprocessCommand.push_back(preprocessOption);
	preprocessCommand.append_range(views::transform(options.compilerOptions,svConstCaster));
	preprocessCommand.push_back(svConstCaster(fileString));
	preprocessCommand.push_back(outOption);
	preprocessCommand.push_back(svConstCaster(outString));
	preprocessCommand.push_back(nullptr);
	auto pid=launch_program(preprocessCommand);
	if(pid)
	{
		auto res=wait(*pid);
		if(res&&res->second!=0)
		{
			println(cerr,"preprocessing {} failed with exit code {}",fileString,res->second);
		}
		else
		{
			outOpt=std::move(out);
		}
	}
	else
	{
		println(cerr,"preprocessing {} failed",fileString);
	}
	return outOpt;
}
string readEntireFile(const path&name)
{
	string s;
	array<char,8192>buf;
	ifstream fin(name.string());
	while(fin)
	{
		size_t c=fin.read(buf.data(),buf.size()).gcount();
		s.append_range(views::take(buf,c));
	}
	return s;
}
vector<string>tokenizeData(const BuildConfiguration&configuration,const path&file)
{
	using namespace chrono;
	vector<string>tokens;
	bool inString=false;
	bool inChar=false;
	bool lastEscape=false;
	bool escaped=false;
	bool symbol=false;
	bool insert=false;
	bool inComment=false;
	bool inLineComment=false;
	bool isQuoted=false;
	bool inAngleBracket=false;
	bool*stringOrCharPointer=nullptr;
	string result=readEntireFile(file);
	size_t lc;
	size_t currentIndex=0;
	size_t beginIndex=0;
	for(char&c:result)
	{
		string_view current(result.begin()+beginIndex,result.begin()+currentIndex);
		insert=!inChar&&!inString&&!inComment&&!inAngleBracket&&!inLineComment;
		if(c=='_'||isalnum((unsigned char)c))
		{
			insert=(insert&&symbol)||isQuoted;
			symbol=false;
		}
		else if(c=='\\')
		{
			insert=false;
			escaped=!lastEscape;
		}
		else if(c=='>')
		{
			if(tokens.size()&&tokens.back()=="import")
			{
				inAngleBracket=false;
				isQuoted=true;
			}
			else
			{
				insert=(insert&&!symbol)||isQuoted;
				symbol=true;
			}
		}
		else if(c=='<')
		{
			if(!inString&&(current=="import"||(tokens.size()&&tokens.back()=="import")))
			{
				inAngleBracket=true;
				insert=true;
			}
			else
			{
				insert=(insert&&!symbol)||isQuoted;
				symbol=true;
			}
		}
		else if(c=='\''||c=='"')
		{
			if(!inLineComment&&!inComment)
			{
				if(inChar||c!='\''||result[currentIndex-1]>'9'||result[currentIndex-1]<'0')
				{
					insert=false;
					stringOrCharPointer=c=='\''?&inChar:&inString;
					if(((stringOrCharPointer==&inChar&&!inString)||(stringOrCharPointer==&inString&&!inChar))&&!lastEscape)
					{
						*stringOrCharPointer=!*stringOrCharPointer;
						insert=*stringOrCharPointer;
						isQuoted=!insert;
					}
				}
			}
		}
		else if(!isspace((unsigned char)c))
		{
			insert=(insert&&!symbol)||isQuoted;
			symbol=true;
		}
		if(!isQuoted&&!inString)
		{
			if(current.size()>=2)
			{
				if(current.ends_with("*/"))
				{
					insert=true;
					inComment=false;
				}
				else if(current.ends_with("/*"))
				{
					insert=true;
					inComment=true;
				}
				else if(current.ends_with("//"))
				{
					inLineComment=true;
				}
			}
			else if(current.size()>=1)
			{
				insert=insert||current.back()==';';
			}
		}
		lastEscape=escaped;
		escaped=false;
		if(c=='\n')
		{
			insert=inLineComment||insert;
			inLineComment=false;
		}
		if(insert&&current.size())
		{
			tokens.push_back(result.substr(beginIndex,currentIndex-beginIndex));
			beginIndex=currentIndex;
			insert=false;
			isQuoted=false;
		}
		if(!isspace((unsigned char)c)||inComment||inString||inChar)
		{
			if(c=='\n')
			{
				c=' ';
			}
		}
		else
		{
			++beginIndex;
		}
		++currentIndex;
	}
	if(currentIndex>beginIndex)
	{
		tokens.push_back(result.substr(beginIndex,currentIndex-beginIndex));
	}
	return tokens;
}
export ModuleData parseModuleData(const BuildConfiguration&configuration,const path&file)
{
	using namespace chrono;
	ModuleData md;
	auto start=high_resolution_clock::now();
	vector<string>ts=tokenizeData(configuration,file);
	auto end=high_resolution_clock::now();
	vector<string>importStatement;
	ImportType it;
	bool lastExport=false;
	bool grabName=false;
	bool importing=false;
	start=high_resolution_clock::now();
	for(const string&s:ts)
	{
		if(grabName)
		{
			md.name+=s;
		}
		if(s=="module")
		{
			if(lastExport)
			{
				grabName=true;
			}
		}
		lastExport=false;
		if(importing)
		{
			importStatement.push_back(s);
		}
		if(s.back()==';')
		{
			if(importStatement.size()>=2)
			{
				if(importStatement.size()==2)
				{
					if(importStatement[0].back()=='"'&&importStatement[0].front()=='"')
					{
						it=LOCAL_HEADER;
					}
					else if(importStatement[0].back()=='>'&&importStatement[0].front()=='<')
					{
						it=SYS_HEADER;
					}
					else
					{
						it=MODULE;
					}
				}
				else
				{
					it=MODULE;
				}
				if(it!=MODULE)
				{
					importStatement[0].erase(importStatement[0].end()-1);
					importStatement[0].erase(0,1);
				}
				if(importStatement.size()==2)
				{
					md.imports.push_back({std::move(importStatement[0]),it});
				}
				else
				{
					importStatement.pop_back();
					auto concatenated=views::join(importStatement);
					string partitionOf;
					if(importStatement.front()==":")
					{
						size_t p=md.name.find(":");
						p=p==string::npos?md.name.size():p;
						partitionOf=md.name.substr(0,p);
					}
					md.imports.push_back({partitionOf+ranges::to<string>(concatenated),it});
				}
				importStatement.clear();
				importing=false;
			}
			if(grabName&&!md.name.empty())
			{
				md.name.pop_back();
			}
			grabName=false;
		}
		if(s=="export")
		{
			lastExport=true;
		}
		if(s=="import")
		{
			importing=true;
		}
	}
	end=high_resolution_clock::now();
	return md;
}
