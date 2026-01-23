export module dependency.cache;
export import graph.back;
using namespace std;
using filesystem::path;
using views::filter;
constexpr char OPENING='[';
constexpr char CLOSING=']';
export bool parseDependencies(ProjectGraph&g,istream&in)
{
	string ln;
	string current;
	array<string,3>stringFields;
	bool external=false;
	vector<ImportUnit>imports;
	size_t index=0;
	bool inside=false;
	bool atLeastOneUpdated=false;
	while(!getline(in,ln).eof())
	{
		if(ln.front()==OPENING)
		{
			imports.clear();
			index=0;
			inside=true;
		}
		else if(ln.front()==CLOSING)
		{
			path source(current);
			path preprocessed(stringFields[1]);
			bool updated=isMoreRecent(source,preprocessed);
			if(updated)
			{
				atLeastOneUpdated=true;
				g.addFile(std::move(source),external);
			}
			else
			{
				g.addEntry(std::move(current),std::move(stringFields[0]),std::move(preprocessed),path{stringFields[2]},std::move(imports),external);
			}
			inside=false;
		}
		else if(inside)
		{
			if(index<stringFields.size())
			{
				stringFields[index]=std::move(ln);
			}
			else if(index==stringFields.size())
			{
				external=ln!="false";
			}
			else
			{
				const size_t ind=ln.find_last_of(',');
				ImportType type=MODULE;
				if(ind!=string::npos)
				{
					type=static_cast<ImportType>(stoi(ln.substr(ind+1)));
				}
				imports.emplace_back(ln.substr(0,ind),type);
			}
			++index;
		}
		else
		{
			current=std::move(ln);
		}
	}
	return atLeastOneUpdated;
}
export void dumpDependencies(const ProjectGraph&g,ostream&out)
{
	for(const auto&[filepath,filedata]:g)
	{
		if(!filedata.external||filedata.module.size())
		{
			println(out,"{}\n{}",filepath,OPENING);
			println(out,"{}\n{}\n{}\n{}",filedata.module,filedata.preprocessed.string(),filedata.object.string(),filedata.external);
			for(const ImportUnit&i:filedata.depend)
			{
				println(out,"{},{}",i.name,to_underlying(i.type));
			}
			print(out,"{}\n",CLOSING);
		}
	}
}
bool attemptToResolveUsing(ProjectGraph&g,string_view module,unordered_set<string_view>&visited,queue<string_view>&q,span<const path>likely)
{
	bool found=false;
	for(const path&p:likely)
	{
		//println("checking {} against {}",module,p.string());
		auto itO=g.addFile(std::move(p),true);
		if(itO)
		{
			auto&it1=*itO;
			if(it1->second.module==module)
			{
				for(const ImportUnit&unit:filter(it1->second.depend,[](const ImportUnit&u){return u.type==MODULE;}))
				{
					auto[_,success]=visited.insert(unit.name);
					if(success)
					{
						q.push(unit.name);
					}
				}
				//println("found {}",p.string());
				found=true;
				break;
			}
			else
			{
				g.erase(it1);
			}
		}
	}
	return found;
}
export void resolveUnresolvedDependencies(ProjectGraph&g)
{
	unordered_set<string_view>unresolvedImports;
	for(const auto&[filepath,filedata]:g)
	{
		for(const auto&[i,resolved]:filedata.dependResolved())
		{
			if(!resolved)
			{
				//vector<path>potential=g.getCompiler()->searchForLikelyCandidates(i.name);
				unresolvedImports.insert(i.name);
				/*if(!found)
				{
					unresolvedImports.insert(i.name);
				}*/
			}
		}
	}
	queue<string_view>q=ranges::to<queue<string_view>>(unresolvedImports);
	while(q.size()>0)
	{
		string_view sv=q.front();
		q.pop();
		//println("unresolved {}",sv);
		vector<path>likely=g.getCompiler()->searchForLikelyCandidates(sv);
		//println("likely size {}",likely.size());
		if(!attemptToResolveUsing(g,sv,unresolvedImports,q,likely))
		{
			auto filesToTry=g.getCompiler()->sortPotentialModuleFiles(sv);
			//println("filesToTry size {}",filesToTry.size());
			bool found=attemptToResolveUsing(g,sv,unresolvedImports,q,filesToTry);
			if(!found)
			{
				println(cerr,"fatal error: module {} not found",sv);
			}
		}
	}
}
