export module process;
import std;
import utils;
using namespace std;
export class ParallelProcessManager
{
	uint8_t maximum;
	bool needToWait;
	unordered_set<int>processes;
public:
	ParallelProcessManager(uint8_t m)
		noexcept
		:maximum(m),needToWait(false),processes()
	{}
	ParallelProcessManager()
		noexcept
		:ParallelProcessManager(1)
	{}
	optional<int>run(span<char*>args)
	{
		if(needToWait)
		{
			auto status=check_finished_process();
			size_t removed=0;
			while(status)
			{
				removed+=processes.erase(status->first);
				status=check_finished_process();
			}
			while(removed==0)
			{
				status=wait();
				removed+=status&&processes.erase(status->first);
			}
			needToWait=false;
		}
		auto oid=launch_program(args);
		if(oid)
		{
			processes.insert(*oid);
			needToWait=(uint8_t)processes.size()==maximum;
		}
		return oid;
	}
	void wait_all_processes(span<int>idArray)
		noexcept
	{
		for(int id:idArray)
		{
			if(processes.erase(id))
			{
				wait(id);
				needToWait=false;
			}
		}
	}
	void wait_remaining_processes()
		noexcept
	{
		while(!processes.empty())
		{
			auto status=wait();
			if(status)
			{
				processes.erase(status->first);
			}
		}
		needToWait=false;
	}
	constexpr bool is_full()
		const noexcept
	{
		return needToWait;
	}
};
