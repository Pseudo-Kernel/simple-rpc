#pragma once

#include "SRPCBase.h"

namespace SRPC
{

class ThreadPool
{
public:
	using ThreadInfo = std::tuple<std::thread, uint64_t>;
	using ThreadWorkItem = std::tuple<std::function<void()>>;

	ThreadPool(uint32_t ThreadsCount) : 
		ThreadsCount_(ThreadsCount),
		NextWorkItemNumber_(0),
		ThreadsMustStop_(false)
	{
		Assert(ThreadsCount_ > 0);

		for (uint64_t i = 0; i < ThreadsCount_; i++)
		{
			ThreadMap_.try_emplace(i, [this]()
			{
				Trace("[%5d] Thread started\n", GetCurrentThreadId());

				while (!ThreadsMustStop_)
				{
					std::unique_lock<decltype(Mutex_)> Lock(Mutex_);

					ConditionVariable_.wait(Lock, [&]() {
						return ThreadsMustStop_ || WorkItemMap_.size() > 0;
					});

					if (ThreadsMustStop_)
						break;

					Assert(WorkItemMap_.size() > 0);

					auto& it = WorkItemMap_.begin();
					auto WorkItemNumber = it->first;
					auto Values = std::move(it->second);
					WorkItemMap_.erase(it);

					Lock.unlock();

					Trace("[%5d] Dispatch item %llu\n", GetCurrentThreadId(), WorkItemNumber);

					auto Function = std::get<0>(Values);

					if (Function)
						Function();

					Trace("[%5d] Item %llu done\n", GetCurrentThreadId(), WorkItemNumber);
				}

				Trace("[%5d] Thread end\n", GetCurrentThreadId());

			}, 0);
		}
	}

	~ThreadPool()
	{
		ThreadsMustStop_ = true;
		ConditionVariable_.notify_all();

		for (auto& it : ThreadMap_)
		{
			std::thread Thread;
			uint64_t Placeholder = 0;
			std::tie(Thread, Placeholder) = std::move(it.second);

			if (Thread.joinable())
				Thread.join();
		}

		// FIXME: Cancel remaining items
		// std::lock_guard<decltype(Mutex_)> Lock(Mutex_);
		// for (auto& it : WorkItemMap_)
		// {
		//   ...
		// }
	}

	template <class TFunction, class... TArg>
	auto Queue(TFunction&& Fn, TArg&&... Args)
	{
		using TReturn = std::result_of<TFunction(TArg...)>::type;

		auto Task = std::make_shared<std::packaged_task<TReturn()>>(
			std::bind(std::forward<TFunction>(Fn), std::forward<TArg>(Args)...));
		auto Future = Task->get_future();

		std::lock_guard<decltype(Mutex_)> Lock(Mutex_);

		uint64_t WorkItemNumber = NextWorkItemNumber_;

		auto ResultTuple = WorkItemMap_.try_emplace(WorkItemNumber, [Task]()
		{
			(*Task)();
		});
		Assert(ResultTuple.second);
		NextWorkItemNumber_++;

		ConditionVariable_.notify_one();

		return std::move(Future);
	}


private:
	uint32_t ThreadsCount_;

	std::mutex Mutex_;
	std::condition_variable ConditionVariable_;
	std::map<uint64_t, ThreadWorkItem> WorkItemMap_;
	std::map<uint64_t, ThreadInfo> ThreadMap_;
	uint64_t NextWorkItemNumber_;
	std::atomic_bool ThreadsMustStop_;
};

}


