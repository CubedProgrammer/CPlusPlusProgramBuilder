export module utility.async;
export import utility.system;
using namespace std;
using views::zip;
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
	AsyncBase()=default;
	void resume_outer()
	{
		if(outer)
		{
			outer.resume();
			outer=nullptr;
		}
	}
	bool await_suspend(coroutine_handle<>hd)
	{
		outer=hd;
		return true;
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
	bool await_ready()
		const noexcept
	{
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
public:
	using promise_type = PromiseType<void>;
	Async(promise_type*p)
		noexcept
		:AsyncBase()
	{
		p->setTask(*this);
	}
	Async()=default;
	bool await_ready()
		const noexcept
	{
		return false;
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
	task->resume_outer();
}
void PromiseType<void>::return_void()
{
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
	for(auto[p,c]:zip(globalReadQueue.first,globalReadQueue.second))
	{
		c.first->retval=p->readInto(c.first->buffer);
		c.second.resume();
	}
}
export ReadAwaitable readAsync(PipeHandle&file,span<char>buf)
{
	using namespace chrono_literals;
	optional<size_t>retval;
	bool alreadyReady=false;
	globalReadQueue.first.push_back(&file);
	auto res=selectPipeHandles(globalReadQueue.first,50ms);
	if(res)
	{
		auto&ready=*res;
		for(size_t ind:views::reverse(ready))
		{
			if(ind==globalReadQueue.first.size()-1)
			{
				retval=globalReadQueue.first[ind]->readInto(globalReadQueue.second[ind].first->buffer);
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
