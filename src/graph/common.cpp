export module graph.common;
export import dependency;
using namespace std;
using filesystem::path;
using views::zip;
export struct FileData
{
	string module;
	path preprocessed;
	path object;
	vector<ImportUnit>depend;
	vector<char>absoluteResolved;
	bool external;
	auto dependResolved(this auto&self)
		noexcept
	{
		return zip(self.depend,self.absoluteResolved);
	}
	void eraseDuplicateImports()
		noexcept
	{
		auto it1=depend.begin();
		auto it2=it1;
		auto it3=absoluteResolved.begin();
		println("erasing for {}",module);
		for(;it2!=depend.end();++it2)
		{
			if(it1!=it2)
			{
				iter_swap(it1,it2);
			}
			if(it1==depend.begin()||!ranges::contains(ranges::subrange{depend.begin(),it1},*it1))
			{
				++it1;
				++it3;
			}
		}
		println("gap1 {}",it2-it1);
		depend.erase(it1,it2);
		println("gap2 {}",it3-absoluteResolved.begin());
		absoluteResolved.erase(it3,absoluteResolved.end());
		println("erased for {}",module);
	}
};
export using CompilerRequiredTrio=tuple<const string&,const path&,span<const ImportUnit>>;
export CompilerRequiredTrio dataToTrio(const FileData&dat)
	noexcept
{
	return{dat.module,dat.object,dat.depend};
}
