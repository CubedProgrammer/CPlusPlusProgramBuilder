export module utility.process;
export import utility.system;
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
	optional<unsigned>run(span<string_view>args,bool printCommand)
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
			for(string_view a:args)
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
		vector<char*>trueArgs;
		trueArgs.reserve(args.size()+1);
		trueArgs.append_range(views::transform(args,svConstCaster));
		trueArgs.push_back(nullptr);
		auto oid=launch_program(trueArgs);
		if(oid)
		{
			processes.insert((unsigned)oid->second);
			needToWait=(uint8_t)processes.size()==maximum;
		}
		return oid.transform([](const pair<string,int>&m){return(unsigned)m.second;});
	}
	optional<unsigned>wait_any_process()
	{
		optional<unsigned>opid;
		bool keepWaiting=true;
		auto processAndResult=wait();
		while(keepWaiting&&processAndResult)
		{
			if(processes.erase((unsigned)processAndResult->first))
			{
				opid=(unsigned)processAndResult->first;
				needToWait=keepWaiting=false;
			}
			else
			{
				processAndResult=wait();
			}
		}
		return opid;
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
	constexpr bool is_empty()
		const noexcept
	{
		return processes.size()==0;
	}
	constexpr bool is_full()
		const noexcept
	{
		return needToWait;
	}
};
