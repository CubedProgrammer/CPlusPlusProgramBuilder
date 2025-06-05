export module process;
import std;
import utils;
using namespace std;
export class ParallelProcessManager
{
	uint8_t maximum;
	bool needToWait;
	unordered_set<unsigned>processes;
public:
	ParallelProcessManager(uint8_t m)
		noexcept
		:maximum(m),needToWait(false),processes()
	{}
	optional<unsigned>run(span<char*>args,bool printCommand)
	{
		if(needToWait)
		{
			auto status=check_finished_process();
			size_t removed=0;
			while(status)
			{
				removed+=processes.erase((unsigned)status->first);
				status=check_finished_process();
			}
			while(removed==0)
			{
				status=wait();
				removed+=status&&processes.erase((unsigned)status->first);
			}
			needToWait=false;
		}
		if(printCommand)
		{
			bool notFirst=false;
			for(char*a:args.first(args.size()-1))
			{
				if(notFirst)
				{
					print(" {}",a);
				}
				else
				{
					print("{}",a);
				}
				notFirst=true;
			}
			print("\n");
		}
		auto oid=launch_program(args);
		if(oid)
		{
			processes.insert((unsigned)*oid);
			needToWait=(uint8_t)processes.size()==maximum;
		}
		return oid;
	}
	void wait_all_processes(span<unsigned>idArray)
		noexcept
	{
		for(unsigned id:idArray)
		{
			if(processes.erase(id))
			{
				wait((int)id);
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
				processes.erase((unsigned)status->first);
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
