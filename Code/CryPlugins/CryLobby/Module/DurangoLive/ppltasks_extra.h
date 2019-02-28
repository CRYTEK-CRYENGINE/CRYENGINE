// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ppltasks.h>

#pragma warning(push)
#pragma warning(disable : 28204)

#include <agents.h>

#pragma warning(pop)

#include <memory>
#include <chrono>

namespace Concurrency
{
namespace extras
{

template<typename Func>
auto create_delayed_task(std::chrono::milliseconds delay, Func func, concurrency::cancellation_token token = concurrency::cancellation_token::none())->decltype(create_task(func))
{
	concurrency::task_completion_event<void> tce;

	auto pTimer = std::make_shared<concurrency::timer<int>>(static_cast<int>(delay.count()), 0, nullptr, false);
	auto pCallback = std::make_shared<concurrency::call<int>>([tce](int)
			{
				tce.set();
	    });

	pTimer->link_target(pCallback.get());
	pTimer->start();

	return create_task(tce).then([pCallback, pTimer]()
			{
	    }).then(func, token);
}

enum class iterative_task_status
{
	complete   = 0,
	incomplete = 1
};

namespace details
{
static void iterative_task_impl(concurrency::task_completion_event<HRESULT> finished, std::function<concurrency::task<std::tuple<iterative_task_status, HRESULT>>()> body, concurrency::cancellation_token ct)
{
	enum
	{
		Status = 0,
		Result = 1
	};
	body().then([=](concurrency::task<std::tuple<iterative_task_status, HRESULT>> previous)
				{
					try
					{
					  std::tuple<iterative_task_status, HRESULT> result(previous.get());
					  if (std::get<Status>(result) == iterative_task_status::incomplete)
							iterative_task_impl(finished, body, ct);
					  else
							finished.set(std::get<Result>(result));
					}
					catch (...)
					{
					  finished.set_exception(std::current_exception());
					}
	      }, ct);
}

}   // namespace details

inline auto create_iterative_task(std::function<concurrency::task<std::tuple<iterative_task_status, HRESULT>>()> body, cancellation_token ct = cancellation_token::none())->concurrency::task<HRESULT>
{
	concurrency::task_completion_event<HRESULT> finished;
	create_task([ = ] {
				try
				{
				  details::iterative_task_impl(finished, body, ct);
				}
				catch (...)
				{
				  finished.set_exception(std::current_exception());
				}
	    }, ct);
	return create_task(finished, ct);
}

} // } namespace extras
} // } namespace concurrency
