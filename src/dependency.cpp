export module dependency;
import std;
import configuration;
import utils;
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
};
export struct ModuleData
{
	string name;
	vector<ImportUnit>imports;
};
vector<string>tokenizeData(const BuildConfiguration&configuration,const path&file)
{
	using namespace chrono;
	vector<string>tokens;
	vector<char*>preprocessCommand;
	string fileString=file.string();
	char preprocessOption[]="-E";
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
	size_t lc;
	preprocessCommand.reserve(configuration.compilerOptions.size()+4);
	preprocessCommand.push_back(svConstCaster(configuration.compiler()));
	preprocessCommand.push_back(preprocessOption);
	preprocessCommand.append_range(views::transform(configuration.compilerOptions, svConstCaster));
	preprocessCommand.push_back(svConstCaster(fileString));
	println("{}",preprocessCommand);
	preprocessCommand.push_back(nullptr);
	auto start=high_resolution_clock::now();
	auto result= run_and_get_output(preprocessCommand).value().first;
	auto end=high_resolution_clock::now();
	println("preprocessing {}\ndependency {} {}",(end-start).count(),fileString,result.size());
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
	println("tokenize {}\n{}",(end-start).count(),ts.size());
	start=high_resolution_clock::now();
	/*if(file.filename()=="test-tokenize.txt")
	{
		for(const string&s:ts)
		{
			println("{}",s);
		}
	}*/
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
	println("parse {}",(end-start).count());
	println("{} imports {}",md.name,views::transform(md.imports,&ImportUnit::name));
	return md;
}
