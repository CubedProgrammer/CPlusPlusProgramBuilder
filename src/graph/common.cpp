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
};
export using CompilerRequiredTrio=tuple<const string&,const path&,span<const ImportUnit>>;
export CompilerRequiredTrio dataToTrio(const FileData&dat)
	noexcept
{
	return{dat.module,dat.object,dat.depend};
}
