module;
#include<cerrno>
#include<fcntl.h>
#include<sys/wait.h>
#include<unistd.h>
export module utility.system;
export import std;
using namespace std;
using filesystem::path;
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
export optional<int>launch_program(span<char*>arguments)
{
	optional<int>opid;
	array<int,2>fds;
	if(pipe(fds.data())==0)
	{
		int pid=fork();
		if(pid>0)
		{
			close(fds[1]);
			char c=0;
			if(read(fds[0],&c,1)<=0)
			{
				opid=pid;
			}
			close(fds[0]);
		}
		else if(pid<0)
		{
			cerr<<system_error{error_code{errno,system_category()}}.what()<<endl;
		}
		else
		{
			close(fds[0]);
			fcntl(fds[1],F_SETFD,FD_CLOEXEC);
			char c=1;
			if(execvp(arguments.front(),arguments.data()))
			{
				write(fds[1],&c,1);
				close(fds[1]);
				exit(1);
			}
		}
	}
	return opid;
}
export optional<pair<string,int>>run_and_get_output(span<char*>arguments)
{
	array<int,2>fds;
	optional<pair<string,int>>oResult;
	if(pipe(fds.data())==0)
	{
		int pid=fork();
		if(pid>0)
		{
			int status;
			array<char,8192>buffer;
			long cnt;
			oResult=pair<string,int>({},0);
			close(fds[1]);
			while((cnt=read(fds[0],buffer.data(),buffer.size()))>0)
			{
				oResult->first.append_range(span(buffer.data(),buffer.data()+cnt));
			}
			waitpid(pid,&status,0);
			close(fds[0]);
			oResult->second=WEXITSTATUS(status);
		}
		else if(pid<0)
		{
			cerr<<system_error{error_code{errno,system_category()}}.what()<<endl;
		}
		else
		{
			dup2(fds[1],STDOUT_FILENO);
			dup2(fds[1],STDERR_FILENO);
			close(fds[0]);
			close(fds[1]);
			if(execvp(arguments.front(),arguments.data()))
			{
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
