export module dependency.cache;
export import graph.back;
using namespace std;
using filesystem::path;
using views::zip;
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
export void resolveUnresolvedDependencies(ProjectGraph&g)
{
	unordered_set<string_view>unresolvedImports;
	for(const auto&[filepath,filedata]:g)
	{
		for(const auto&[i,resolved]:filedata.dependResolved())
		{
			if(!resolved)
			{
				vector<path>potential=g.getCompiler()->searchForLikelyCandidates(i.name);
				bool found=false;
				for(path&p:potential)
				{
					auto itO=g.addFile(std::move(p),true);
					if(itO)
					{
						auto&it1=*itO;
						if(it1->second.module==i.name)
						{
							found=true;
							break;
						}
						else
						{
							g.erase(it1);
						}
					}
				}
				if(!found)
				{
					unresolvedImports.insert(i.name);
				}
			}
		}
	}
	auto filesToTry=ranges::to<vector<path>>(g.getCompiler()->getPotentialModuleFiles());
	for(string_view sv:unresolvedImports)
	{
		println("unresolved {}",sv);
		auto scores=views::transform(filesToTry,[sv](const path&m){string n=m.stem().string();return similarity(sv,n);});
		auto scoresVector=ranges::to<vector<ModulePathSimilarity>>(scores);
		ranges::sort(zip(scoresVector,filesToTry),ranges::greater());
		for(path t:filesToTry)
		{
			string ts=t.stem().string();
			auto sim=similarity(sv,ts);
			println("{} {} {}",ts,sim.lcs,sim.remaining);
			auto iteratorOpt=g.addFile(std::move(t),true);
			if(iteratorOpt)
			{
				auto&it1=*iteratorOpt;
				if(it1->second.module==sv)
				{
					println("found {}",t.string());
					break;
				}
				else
				{
					g.erase(it1);
				}
			}
		}
	}
}
