// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "IBackgroundTaskManager.h"

#include <future>
#include <tuple>
#include <type_traits>

namespace ThreadingUtils
{

namespace Private_ThreadingUtils
{

template<typename Tuple, size_t... Idxs>
inline decltype(auto) Invoke(Tuple& tuple, std::integer_sequence<size_t, Idxs...>)
{
	return std::invoke(std::move(std::get<Idxs>(tuple))...);
}

template<typename Tuple>
inline decltype(auto) Invoke(Tuple& tuple)
{
	return Invoke(tuple, std::make_integer_sequence<size_t, std::tuple_size<Tuple>::value>());
}

template<typename T, typename Tuple>
inline void SetPromise(std::promise<T>& p, Tuple& tuple)
{
	p.set_value(Invoke(tuple));
}

template<typename Tuple>
inline void SetPromise(std::promise<void>& p, Tuple& tuple)
{
	Invoke(tuple);
	p.set_value();
}

template<typename Tuple>
struct CBaseTask
{
private:
	typedef decltype(Invoke(std::declval<Tuple&>())) Result;

public:
	template<typename... T>
	CBaseTask(T&&... other)
		: m_tuple(std::forward<T>(other)...)
	{
	}

	std::future<Result> GetFuture()
	{
		return m_promise.get_future();
	}

	void Execute()
	{
		SetPromise(m_promise, m_tuple);
	}

private:
	Tuple m_tuple;
	std::promise<Result> m_promise;
};

template<typename Tuple>
class CAsyncTask : public CBaseTask<Tuple>, public IBackgroundTask
{
public:
	template<typename... T>
	CAsyncTask(T&&... other)
		: CBaseTask<Tuple>(std::forward<T>(other)...)
	{
	}

	// IBackgroundTask implementation.

	virtual void Finalize() override
	{
	}

	virtual void Delete() override
	{
		delete this;
	}

	virtual ETaskResult Work() override
	{
		Execute();
		return eTaskResult_Completed;
	}
};

} // namespace Private_ThreadingUtils

template<typename Fn, typename... Args>
std::future<std::result_of_t<std::decay_t<Fn>()>> Async(Fn&& fn, Args&&... args)
{
	using namespace Private_ThreadingUtils;

	typedef std::tuple<std::decay_t<Fn>, std::decay_t<Args>...> Tuple;
	typedef CAsyncTask<Tuple> Task;

	_smart_ptr<Task> pTask = new Task(std::forward<Fn>(fn), std::forward<Args>(args)...);
	GetIEditor()->GetBackgroundTaskManager()->AddTask(pTask, eTaskPriority_FileUpdate, eTaskThreadMask_Any);
	return pTask->GetFuture();
}

// There is only one queue so all this calls are run in a sequence.
template<typename Fn, typename... Args>
std::future<std::result_of_t<std::decay_t<Fn>()>> AsyncQueue(Fn&& fn, Args&&... args)
{
	using namespace Private_ThreadingUtils;

	typedef std::tuple<std::decay_t<Fn>, std::decay_t<Args>...> Tuple;
	typedef CAsyncTask<Tuple> Task;

	_smart_ptr<Task> pTask = new Task(std::forward<Fn>(fn), std::forward<Args>(args)...);
	GetIEditor()->GetBackgroundTaskManager()->AddTask(pTask, eTaskPriority_FileUpdate, eTaskThreadMask_IO);
	return pTask->GetFuture();
}


template<typename Fn, typename... Args>
std::future<std::result_of_t<std::decay_t<Fn>(std::decay_t<Args>...)>> PostOnMainThread(Fn&& fn, Args&&... args)
{
	using namespace Private_ThreadingUtils;

	typedef std::tuple<std::decay_t<Fn>, std::decay_t<Args>...> Tuple;

	auto pTask = new CBaseTask<Tuple>(std::forward<Fn>(fn), std::forward<Args>(args)...);
	auto future = pTask->GetFuture();
	GetIEditor()->PostOnMainThread([pTask]()
	{
		pTask->Execute();
		delete pTask;
	});
	return future;
}

//! Convenience function template that runs one callable asynchronously, and passes the result to another callable on the main thread.
//! \param workFn Callable with signature equivalent to T w(). (the 'work' function)
//! \param finalizeFn Callable with signature equivalent to void f(T). (the 'finalize' function)
//! \result Returns future<T>, where T is the type returned by the work function.
//! \sa AsyncNotify
template<typename WorkFn, typename FinalizeFn>
std::future<std::result_of_t<std::decay_t<WorkFn>()>> AsyncFinalize(WorkFn&& workFn, FinalizeFn&& finalizeFn)
{
	return Async([workFn, finalizeFn]() -> std::result_of_t<std::decay_t<WorkFn>()>
	{
		auto&& res = workFn();
		PostOnMainThread(finalizeFn, res);
		return res;
	});
}

//! Convenience function template that runs one callable asynchronously, and then runs another callable on the main thread.
//! \param workFn Callable with signature equivalent to void T w(). (the 'work' function)
//! \param finalizeFn Callable with signature equivalent to void f(). (the 'finalize' function)
//! \result Returns future<T>, where T is the type returned by the work function.
//! \remark In contrast to AsyncFinalize, the result of the work function is not passed to the finalize function.
//! \sa AsyncFinalize
template<typename WorkFn, typename FinalizeFn>
std::future<std::result_of_t<std::decay_t<WorkFn>()>> AsyncNotify(WorkFn&& workFn, FinalizeFn&& finalizeFn)
{
	return Async([workFn, finalizeFn]() -> std::result_of_t<std::decay_t<WorkFn>()>
	{
		auto&& ret = workFn();
		PostOnMainThread(finalizeFn);
		return ret;
	});
}

} // namespace ThreadingUtils

