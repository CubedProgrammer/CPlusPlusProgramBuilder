export module graph.front;
export import graph.common;
using namespace std;
using filesystem::path;
export struct ForwardGraphNode
{
	string name;
	bool notInterface;
	bool header;
	bool external;
	constexpr bool operator==(const ForwardGraphNode&)const noexcept=default;
};
export namespace std
{
	template<>
	struct hash<ForwardGraphNode>
	{
		constexpr size_t operator()(const ForwardGraphNode&object)
			const noexcept
		{
			hash<string>shasher;
			return shasher(object.name)*size_t((int)object.header+(int)object.external+1);
		}
	};
}
export struct ForwardGraphNodeData
{
	vector<ForwardGraphNode>dependent;
	uint16_t remaining;
	bool recompile;
};
export struct ForwardGraph
{
	using map_type=unordered_map<ForwardGraphNode,ForwardGraphNodeData>;
	map_type graph;
	void insert(string_view pathString,const FileData&fdata,unsigned forceLevel,function<optional<const pair<const string,FileData>*>(string_view)>query)
	{
		path p(pathString);
		bool updated=isMoreRecent(p,fdata.object);
		bool toCompile=(forceLevel>>fdata.external&1)||updated;
		auto[it,succ]=graph.insert({{string{pathString},fdata.module.size()==0,false,fdata.external},{{},static_cast<uint16_t>(fdata.depend.size()),toCompile}});
		if(!succ)
		{
			it->second.remaining=static_cast<uint16_t>(fdata.depend.size());
			it->second.recompile=toCompile;
		}
		for(const ImportUnit&iu:fdata.depend)
		{
			auto entryO=query(iu.name);
			bool external=false;
			if(entryO)
			{
				auto&[_,dependData]=**entryO;
				external=dependData.external;
			}
			println("iu.name {}",iu.name);
			ForwardGraphNode node{iu.name,false,iu.type!=MODULE,external};
			auto[itN,_]=graph.insert({std::move(node),{{},0,false}});
			itN->second.dependent.push_back(it->first);
		}
	}
	decltype(auto)at(this auto&self,const ForwardGraphNode&node)
	{
		return self.graph.at(node);
	}
	const map_type&internal()
		const noexcept
	{
		return graph;
	}
};
export queue<ForwardGraphNode>buildInitialCompileQueue(const ForwardGraph&g)
{
	queue<ForwardGraphNode>compileQueue;
	for(const auto&[node,edges]:g.graph)
	{
		if(edges.remaining==0)
		{
			compileQueue.push(node);
		}
	}
	return compileQueue;
}
