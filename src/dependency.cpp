export module dependency;
import std;
using namespace std;
using std::filesystem::path;
//bool first=true;
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
vector<string>tokenizeData(const path&file)
{
	vector<string>tokens;
	string current;
	ifstream fin(file.string());
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
	char c;
	while(fin.get(c))
	{
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
		else if(!isspace((unsigned char)c))
		{
			insert=(insert&&!symbol)||isQuoted;
			symbol=true;
		}
		if(!isQuoted&&!inString)
		{
			if(current.size()>=2)
			{
				if(current.find("*/")==current.size()-2)
				{
					insert=true;
					inComment=false;
				}
				else if(current.find("/*")==current.size()-2)
				{
					insert=true;
					inComment=true;
				}
				else if(current.find("//")==current.size()-2)
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
			insert=inLineComment;
			inLineComment=false;
		}
		if(insert&&current.size())
		{
			tokens.push_back(std::move(current));
			current=string();
			insert=false;
			isQuoted=false;
		}
		if(!isspace((unsigned char)c)||inComment||inString||inChar)
		{
			if(c=='\n')
			{
				current+="\\n";
			}
			else
			{
				current+=c;
			}
		}
	}
	if(current.size())
	{
		tokens.push_back(std::move(current));
	}
	/*if(first)
	{
		for(const string&s:tokens)
		{
			println("{}",s);
		}
		first=false;
	}*/
	return tokens;
}
export ModuleData parseModuleData(const path&file)
{
	ModuleData md;
	vector<string>ts=tokenizeData(file);
	vector<string>importStatement;
	ImportType it;
	bool lastExport=false;
	bool grabName=false;
	bool importing=false;
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
	return md;
}
