export module utility.async;
export import utility.system;
using namespace std;
using views::take,views::zip;
export template<typename T>
class Async;
class PromiseBase
{
public:
	suspend_never initial_suspend()
		const noexcept
	{
		return{};
	}
	suspend_never final_suspend()
		const noexcept
	{
		return{};
	}
	void unhandled_exception()
	{
		println(cerr,"coroutine unhandled exception");
	}
};
template<typename T>
class PromiseType:public PromiseBase
{
	Async<T>*task;
public:
	void setTask(Async<T>&t)
		noexcept
	{
		task=&t;
	}
	Async<T>get_return_object();
	void return_value(T v);
};
template<>
class PromiseType<void>:public PromiseBase
{
	Async<void>*task;
public:
	void setTask(Async<void>&t)
		noexcept
	{
		task=&t;
	}
	Async<void>get_return_object();
	void return_void();
};
export class AsyncBase
{
protected:
	coroutine_handle<>outer;
public:
	AsyncBase()
		noexcept
		:outer(nullptr)
	{
		//println("{} first outer address {}",(void*)this,outer.address());
	}
	void resume_outer()
	{
		//println("{} resuming outer address {}",(void*)this,outer.address());
		//println("resuming outer address {}",outer.address());
		if(outer)
		{
			//println("resuming outer address {}",outer.address());
			outer.resume();
			outer=nullptr;
		}
	}
	bool await_suspend(coroutine_handle<>hd)
	{
		//println("awaiting suspend");
		outer=hd;
		return true;
	}
	~AsyncBase()
	{
		//println("{}",(void*)this);
	}
};
export template<typename T=void>
class Async:public AsyncBase
{
	friend PromiseType<T>;
	optional<T>rv;
public:
	using promise_type = PromiseType<T>;
	Async(promise_type*p)
		noexcept
		:AsyncBase(),rv()
	{
		p->setTask(*this);
	}
	Async()=default;
	Async(const Async<T>&)=delete;
	bool await_ready()
		const noexcept
	{
		//println("{} awaiting ready",rv.has_value());
		return rv.has_value();
	}
	T await_resume()
	{
		return std::move(*rv);
	}
};
export template<>
class Async<void>:public AsyncBase
{
	friend PromiseType<void>;
	bool ready;
public:
	using promise_type = PromiseType<void>;
	Async(promise_type*p)
		noexcept
		:AsyncBase(),ready(false)
	{
		p->setTask(*this);
	}
	Async()=default;
	void returned()
		noexcept
	{
		ready=true;
	}
	bool await_ready()
		const noexcept
	{
		return ready;
	}
	void await_resume()
		const noexcept
	{}
};
template<typename T>
Async<T>PromiseType<T>::get_return_object()
{
	return{this};
}
Async<void>PromiseType<void>::get_return_object()
{
	return{this};
}
template<typename T>
void PromiseType<T>::return_value(T v)
{
	task->rv=std::move(v);
	//println("resuming outer task");
	task->resume_outer();
	//println("resumed outer task");
}
void PromiseType<void>::return_void()
{
	task->returned();
	task->resume_outer();
}
export struct ReadAwaitable;
pair<vector<PipeHandle*>,vector<pair<ReadAwaitable*,coroutine_handle<>>>>globalReadQueue;
export struct ProcessAwaitable
{
	optional<int>pidO;
	unordered_map<int,coroutine_handle<>>::iterator it;
	PipeHandle handle;
	bool await_ready()
	{
		bool ready=true;
		if(pidO)
		{
			ready=!it->second;
		}
		return ready;
	}
	bool await_suspend(coroutine_handle<>hd)
	{
		it->second=hd;
		return true;
	}
	pair<optional<int>,PipeHandle>await_resume()
	{
		return{std::move(pidO),std::move(handle)};
	}
};
export struct ReadAwaitable
{
	PipeHandle*handle;
	span<char>buffer;
	optional<size_t>retval;
	bool await_ready()
	{
		return buffer.size()==0||!handle->getRaw()||retval;
	}
	bool await_suspend(coroutine_handle<>hd)
	{
		globalReadQueue.first.push_back(handle);
		globalReadQueue.second.emplace_back(this,hd);
		return true;
	}
	optional<size_t>await_resume()
	{
		return std::move(retval);
	}
};
export void resumeAllAsyncRead()
{
	auto together=zip(globalReadQueue.first,globalReadQueue.second);
	while(!together.empty())
	{
		auto[p,c]=together.back();
		globalReadQueue.first.pop_back();
		globalReadQueue.second.pop_back();
		c.first->retval=p->readInto(c.first->buffer);
		c.second.resume();
	}
}
export ReadAwaitable readAsync(PipeHandle&file,span<char>buf)
{
	using namespace chrono_literals;
	optional<size_t>retval;
	bool alreadyReady=false;
	//println("sizes {} {}",globalReadQueue.first.size(),globalReadQueue.second.size());
	globalReadQueue.first.push_back(&file);
	auto res=selectPipeHandles(globalReadQueue.first,50ms);
	println("selected");
	if(res)
	{
		auto&ready=*res;
		for(size_t ind:views::reverse(ready))
		{
			//println("ind is {}",ind);
			if(ind==globalReadQueue.first.size()-1)
			{
				retval=globalReadQueue.first[ind]->readInto(buf);
				alreadyReady=true;
			}
			else
			{
				globalReadQueue.second[ind].first->retval=globalReadQueue.first[ind]->readInto(globalReadQueue.second[ind].first->buffer);
				globalReadQueue.second[ind].second.resume();
				swap(globalReadQueue.first.back(),globalReadQueue.first[ind]);
				swap(globalReadQueue.second.back(),globalReadQueue.second[ind]);
				globalReadQueue.second.pop_back();
			}
			globalReadQueue.first.pop_back();
		}
	}
	if(!alreadyReady)
	{
		globalReadQueue.first.pop_back();
	}
	return{&file,buf,std::move(retval)};
}
export Async<string>readAllAsync(PipeHandle&file)
{
	string txt;
	array<char,8192>buf;
	txt.reserve(8192);
	println(__FUNCTION__);
	for(optional<size_t>cntO=co_await readAsync(file,buf);cntO&&*cntO>0;cntO=co_await readAsync(file,buf))
	{
		txt.append_range(take(buf,*cntO));
	}
	co_return std::move(txt);
}
