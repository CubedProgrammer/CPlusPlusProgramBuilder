module;
#include<cerrno>
#include<fcntl.h>
#include<sys/select.h>
#include<sys/wait.h>
#include<unistd.h>
export module utility.system;
export import std;
using namespace std;
using chrono::duration,chrono::microseconds;
using filesystem::path;
using views::take;
export constexpr unsigned PIPE_NOTHING=0;
export constexpr unsigned PIPE_OUTPUT=1;
export constexpr unsigned PIPE_ERROR=2;
export struct ModulePathSimilarity
{
	size_t lcs;
	size_t remaining;
	constexpr bool operator==(const ModulePathSimilarity&)const noexcept=default;
	constexpr std::strong_ordering operator<=>(const ModulePathSimilarity&other)const noexcept
	{
		auto first=lcs<=>other.lcs;
		return first==0?other.remaining<=>remaining:first;
	}
};
export class PipeHandle
{
	optional<int>fdO;
public:
	PipeHandle(int fd)
		noexcept
		:fdO(fd)
	{}
	PipeHandle()
		noexcept
		:fdO(nullopt)
	{}
	PipeHandle(PipeHandle&&other)
		noexcept
		:fdO(std::move(other.fdO))
	{
		other.fdO.reset();
	}
	PipeHandle&operator=(PipeHandle&&other)
		noexcept
	{
		fdO=std::move(other.fdO);
		other.fdO.reset();
		return*this;
	}
	optional<size_t>readInto(span<char>buffer)
	{
		if(fdO)
		{
			auto v=read(*fdO,static_cast<void*>(buffer.data()),buffer.size());
			return v<0?optional<size_t>{}:optional<size_t>{v};
		}
		else
		{
			return{nullopt};
		}
	}
	optional<int>&getRaw()
	{
		return fdO;
	}
	string readAll()
	{
		string txt;
		array<char,8192>buf;
		txt.reserve(8192);
		for(optional<size_t>cntO=readInto(buf);cntO&&*cntO>0;cntO=readInto(buf))
		{
			txt.append_range(take(buf,*cntO));
		}
		return txt;
	}
	~PipeHandle()
	{
		if(fdO)
		{
			close(*fdO);
		}
	}
};
export template<typename Rep,typename Period>
optional<vector<size_t>>selectPipeHandles(span<PipeHandle*>pipeHandles,duration<Rep,Period>d)
{
	optional<vector<size_t>>ready;
	auto casted=duration_cast<microseconds>(d);
	struct timeval tv;
	tv.tv_sec=casted.count()/1000000;
	tv.tv_usec=casted.count()%1000000;
	fd_set fds;
	fd_set*fdsp=&fds;
	FD_ZERO(fdsp);
	int maxi=0;
	for(PipeHandle*h:pipeHandles)
	{
		optional<int>&fdOpt=h->getRaw();
		if(fdOpt)
		{
			int fd=*fdOpt;
			maxi=max(fd,maxi);
			FD_SET(fd,fdsp);
		}
	}
	int r=select(maxi+1,fdsp,nullptr,nullptr,&tv);
	if(r>=0)
	{
		ready.emplace();
		ready->reserve(r);
		for(size_t i=0;i<pipeHandles.size();++i)
		{
			optional<int>&fdOpt=pipeHandles[i]->getRaw();
			if(fdOpt)
			{
				int fd=*fdOpt;
				if(FD_ISSET(fd,fdsp))
				{
					ready->push_back(i);
				}
			}
		}
	}
	return ready;
}
size_t longestCS(string_view x,string_view y)
{
	using ranges::fill,views::drop;
	if(x.size()>y.size())
	{
		swap(x,y);
	}
	vector<size_t>current(y.size());
	vector<size_t>next(y.size());
	size_t index=y.find(x.front());
	if(index!=string::npos)
	{
		fill(drop(current,index),1);
	}
	for(size_t i=1;i<x.size();i++)
	{
		next.front()=max(current.front(),size_t(x[i]==y[0]));
		for(size_t j=1;j<y.size();j++)
		{
			size_t addone=x[i]==y[j];
			next[j]=max({current[j],next[j-1],current[j-1]+addone});
		}
		swap(current,next);
	}
	return current.back();
}
export ModulePathSimilarity similarity(string_view x,string_view y)
{
	size_t l=longestCS(x,y);
	return{l,max(x.size(),y.size())-l};
}
export char*svConstCaster(string_view sv)
{
	return const_cast<char*>(sv.data());
}
export optional<pair<int,PipeHandle>>launch_program(span<char*>arguments,unsigned pipeTarget)
{
	optional<pair<int,PipeHandle>>oResult;
	array<int,2>fds;
	array<int,2>errorfds;
	if(pipe(errorfds.data())==0&&(pipeTarget==PIPE_NOTHING||pipe(fds.data())==0))
	{
		int pid=fork();
		if(pid>0)
		{
			long cnt;
			char c;
			close(errorfds[1]);
			cnt=read(errorfds[0],&c,1);
			if(cnt==0)
			{
				oResult.emplace(pid,PipeHandle{});
				if(pipeTarget!=PIPE_NOTHING)
				{
					close(fds[1]);
					oResult->second=PipeHandle(fds[0]);
				}
			}
		}
		else if(pid<0)
		{
			cerr<<system_error{error_code{errno,system_category()}}.what()<<endl;
		}
		else
		{
			close(errorfds[0]);
			fcntl(errorfds[1],F_SETFD,FD_CLOEXEC);
			if(pipeTarget!=PIPE_NOTHING)
			{
				if(pipeTarget==PIPE_OUTPUT)
				{
					dup2(fds[1],STDOUT_FILENO);
				}
				else
				{
					dup2(fds[1],STDERR_FILENO);
				}
				close(fds[0]);
				close(fds[1]);
			}
			if(execvp(arguments.front(),arguments.data()))
			{
				char c=31;
				write(errorfds[1],&c,1);
				close(errorfds[1]);
				exit(1);
			}
		}
	}
	return oResult;
}
export optional<pair<int,int>>waitpidpp(int pid,int flag)
{
	optional<pair<int,int>>data;
	int status;
	int r=waitpid(pid,&status,flag);
	if(r>0)
	{
		data=pair{r,WEXITSTATUS(status)};
	}
	return data;
}
export optional<pair<int,int>>waitflag(int flag)
{
	return waitpidpp(-1,flag);
}
export optional<pair<int,int>>check_finished_process(int process)
{
	return waitpidpp(process,WNOHANG);
}
export optional<pair<int,int>>check_finished_process()
{
	return waitflag(WNOHANG);
}
export optional<pair<int,int>>wait(int process)
{
	return waitpidpp(process,0);
}
export optional<pair<int,int>>wait()
{
	return waitflag(0);
}
export bool isMoreRecent(const path&a,const path&b)
{
	error_code ec;
	auto sourceTime=last_write_time(a);
	auto objectTime=last_write_time(b,ec);
	if(ec)
	{
		objectTime=sourceTime-1s;
	}
	return sourceTime>objectTime;
}
