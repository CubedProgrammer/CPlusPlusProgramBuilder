export module utility.process;
export import utility.async;
export import utility.system;
using namespace std;
export class ParallelProcessManager
{
	uint8_t maximum;
	bool needToWait;
	unordered_set<int>processes;
	unordered_map<int,coroutine_handle<>>coroutines;
public:
	ParallelProcessManager(uint8_t m)
		noexcept
		:maximum(m),needToWait(false),processes(),coroutines()
	{}
	ParallelProcessManager()
		noexcept
		:ParallelProcessManager(1)
	{}
	size_t eraseProcess(int pid)
	{
		auto it=coroutines.find(pid);
		if(it!=coroutines.end())
		{
			if(it->second)
			{
				it->second.resume();
			}
			coroutines.erase(it);
		}
		return processes.erase(pid);
	}
	optional<pair<int,PipeHandle>>run(span<string_view>args,bool printCommand,unsigned pipeTarget=PIPE_NOTHING)
	{
		if(needToWait)
		{
			auto status=check_finished_process();
			size_t removed=0;
			while(status)
			{
				removed+=eraseProcess(status->first);
				status=check_finished_process();
			}
			while(removed==0)
			{
				status=wait();
				removed+=status&&eraseProcess(status->first);
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
		auto oid=launch_program(trueArgs,pipeTarget);
		if(oid)
		{
			processes.insert(oid->first);
			needToWait=(uint8_t)processes.size()==maximum;
		}
		return oid;
	}
	ProcessAwaitable runAsync(span<string_view>args,bool printCommand,unsigned pipeTarget=PIPE_NOTHING)
	{
		auto pidO=run(args,printCommand,pipeTarget);
		auto it=coroutines.end();
		if(pidO)
		{
			it=coroutines.insert({pidO->first,nullptr}).first;
		}
		return{pidO.transform([](const pair<int,PipeHandle>&x){return x.first;}),it,pidO.transform([](pair<int,PipeHandle>&x){return std::move(x.second);}).value_or(PipeHandle{})};
	}
	optional<int>wait_any_process()
	{
		optional<int>opid;
		bool keepWaiting=true;
		auto processAndResult=wait();
		while(keepWaiting&&processAndResult)
		{
			if(eraseProcess(processAndResult->first))
			{
				opid=processAndResult->first;
				needToWait=keepWaiting=false;
			}
			else
			{
				processAndResult=wait();
			}
		}
		return opid;
	}
	void wait_all_processes(span<const int>idArray)
		noexcept
	{
		for(int id:idArray)
		{
			if(eraseProcess(id))
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
				eraseProcess(status->first);
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
