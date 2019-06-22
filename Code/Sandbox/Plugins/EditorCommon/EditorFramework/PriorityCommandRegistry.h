#pragma once

#include "CommandRegistry.h"

namespace Private_CommandRegistry
{

template<size_t dropNum, class... Types>
struct DropParams;

template<size_t dropNum, class T, class... Types>
struct DropParams<dropNum, T, Types...>
{
	using type = typename DropParams<dropNum - 1, Types...>::type;
};

template<class... Types>
struct DropParams<0, Types...>
{
	using type = std::tuple<Types...>;
};

template<size_t dropNum, class... Types>
using DropParams_t = typename DropParams<dropNum, Types...>::type;

template<size_t argInd, size_t lastArgInd, size_t totalOffset>
struct SetDefautsHelper
{
	template<class Tuple1, class Tuple2>
	static void Set(Tuple1& result, const Tuple2& defaults)
	{
		std::get<argInd>(result) = std::get<argInd + totalOffset>(defaults);
		SetDefautsHelper<argInd + 1, lastArgInd, totalOffset>::Set(result, defaults);
	}
};

template<size_t argInd, size_t totalOffset>
struct SetDefautsHelper<argInd, argInd, totalOffset>
{
	template<class T1, class T2> static void Set(T1& result, const T2& defaults) {}
};

//! Determines the rest of tuple types that are not provided by the user and sets default values to them.
//! /return New std::tuple containing only values that are not specified.
//! For example, for SPriorityCommandRegistry<int, float, char>::RegisterCommand("name", someFunc, id, 10)
//! where only first int (10) is specified returning tuple type will be std::tuple<float, char>.
template<size_t dropNum, class... Types>
auto GetDefaultRest(const std::tuple<Types...>& defaults)
{
	using RestTuple = DropParams_t<dropNum, Types...>;
	RestTuple restDefault;
	SetDefautsHelper<0, sizeof...(Types) - dropNum, dropNum>::Set(restDefault, defaults);
	return restDefault;
}

}

//! This class is responsible for registration of commands (functions, event handlers) with arbitrary number of parameters
//! as well as executing them.
//! 
//! Execution is carried out by passing a string containing the commands' id and space-separated list of 
//! arguments. Every argument is to be parsed by the corresponding type and passed to the final function.
//! 
//! Execution of a command returns true if the command (or event) was handled. Customs functions determine themselves
//! if an event is handled by returning boolean flag. If a custom command handler returns void (only bool and void are allowed)
//! the command is always considered to be handled.
//!
//! In comparison to SCommandRegistry this class also allows registration of multiple handlers for the same command.
//! The list of the handlers can be prioritized using custom comparePriorityFunc function.
//! Also every handler after execution can decide if execution of remaining handler should proceed. This might be useful
//! if you have several handlers with different permission. For example, one of the handlers can have exclusive permission
//! (and should be put first in the queue) and after execution signal about termination of further executions.
//! Override evalPriorityFunc function to customize it. This function accepts the index of the handler that is being
//! evaluated at the moment as well as list of permission data for all handlers.
//! 
//! Template types that are provided for specialization of the struct are considered as priority data. They are wrapped
//! in std::tuple and passed to aforementioned functions.
//! 
//! All the priority data parameters are optional. In order to specify default values for them override defaults member.
//!
//! enum class Perm { Exclusive, Default };
//! 
//! struct SPrioritizedRegistry : SPriorityCommandRegistry<Perm, int>
//! {
//! 	SPrioritizedRegistry()
//! 	{
//!			// add all handler with exclusive permission to the beginning of the list. If handlers have the
//!			// the same permission then we define order by priority value (int).
//! 		comparePriorityFunc = [](const std::tuple<Perm, int>& left, const std::tuple<Perm, int>& right)
//! 		{
//! 			return std::get<0>(left) == std::get<0>(right) ? std::get<1>(left) > std::get<1>(right)
//!					: std::get<0>(left) == Perm::Exclusive;
//! 		};
//! 
//!			// if the handler that is being currently processed is exclusive stop further execution.
//!			// If it has default permission (not exclusive) return true (e.g. continue execution).
//! 		evalPriorityFunc = [](size_t ind, const std::vector<std::tuple<Perm, int>>& priorities)
//! 		{
//! 			return std::get<0>(priorities[ind]) != Perm::Exclusive;
//! 		};
//!
//!         defaults = { Perm::Exclusive, 10 };
//! 	}
//! };
template<class... Types>
struct SPriorityCommandRegistry
{
	using HandlerFunc = std::function<bool(const string&)>;

	using PriorityData = std::tuple<Types...>;

	struct HandlersList
	{
		std::vector<uintptr_t> ids;
		std::vector<HandlerFunc> funcs;
		std::vector<PriorityData> priorities;
	};

	StringMap<HandlersList> commands;
	std::function<bool(const PriorityData& left, const PriorityData& right)> comparePriorityFunc = [](auto, auto) { return true; };
	std::function<bool(size_t, const std::vector<PriorityData>&)> evalPriorityFunc = [](auto, auto) { return true; };
	std::tuple<Types...> defaults;

	//! Registers a std::function as a command.
	template<class... Params, class... Args>
	void RegisterCommand(const string& commandId, std::function<bool(Params...)> f, uintptr_t id, Args&&... args)
	{
		using namespace Private_CommandRegistry;
		auto func = [f = std::move(f)](const string& argsStr) -> bool
		{
			return CommandRegistry::InvokeCommand(std::move(f), argsStr);
		};

		auto commandIt = commands.find(commandId);
		PriorityData priorityData = std::tuple_cat(std::make_tuple(std::forward<Args>(args)...),
			GetDefaultRest<sizeof...(Args)>(defaults));
		if (commandIt == commands.end())
		{
			auto newIt = commands.emplace(commandId, HandlersList());
			HandlersList& handlers = newIt.first->second;
			handlers.ids.push_back(id);
			handlers.funcs.push_back(std::move(func));
			handlers.priorities.push_back(std::move(priorityData));
		}
		else
		{
			HandlersList& handlers = commandIt->second;
			CRY_ASSERT_MESSAGE(std::find(handlers.ids.cbegin(), handlers.ids.cend(), id) == handlers.ids.cend()
				, "Id %d for command %s is already registered", id, commandId.c_str());
			auto it = std::lower_bound(handlers.priorities.cbegin(), handlers.priorities.cend(), priorityData, comparePriorityFunc);
			auto offset = std::distance(handlers.priorities.cbegin(), it);
			handlers.ids.insert(handlers.ids.cbegin() + offset, id);
			handlers.funcs.insert(handlers.funcs.cbegin() + offset, std::move(func));
			handlers.priorities.insert(it, std::move(priorityData));
		}
	}

	//! Registers a void returning std::function as a command.
	template<class... Params, class... Args>
	void RegisterCommand(const string& commandId, std::function<void(Params...)> f, uintptr_t id, Args&&... args)
	{
		std::function<bool(Params...)> func = [f = std::move(f)](Params&&... params) -> bool
		{
			f(std::forward<Params>(params)...);
			return true;
		};
		RegisterCommand(commandId, std::move(func), id, std::forward<Args>(args)...);
	}

	//! Registers a lambda or a function as a command.
	template<class Func, class... Args>
	void RegisterCommand(const string& commandId, Func&& f, uintptr_t id, Args&&... args)
	{
		RegisterCommand(commandId, ToStdFunction(std::forward<Func>(f)), id, std::forward<Args>(args)...);
	}

	//! Registers a member function pointer as a command.
	template<class T, class Func, class... Args>
	void RegisterCommand(const string& commandId, Func&& f, T* pObj, uintptr_t id = 0, Args&&... args)
	{
		RegisterCommand(commandId, ToStdFunction(std::forward<Func>(f), pObj), id ? id : reinterpret_cast<uintptr_t>(pObj),
			std::forward<Args>(args)...);
	}

	void UnregisterCommand(const string& commandId)
	{
		commands.erase(commandId);
	}

	void UnregisterCommand(const string& commandId, uintptr_t id)
	{
		auto commandIt = commands.find(commandId);
		if (commandIt != commands.end())
		{
			HandlersList& list = commandIt->second;
			auto it = std::find(list.ids.cbegin(), list.ids.cend(), id);
			if (it == list.ids.cend())
			{
				CRY_ASSERT_MESSAGE(false, "Id %d for command %s is not registered", id, commandId.c_str());
				return;
			}
			auto offset = it - list.ids.cbegin();
			list.ids.erase(list.ids.cbegin() + offset);
			list.funcs.erase(list.funcs.cbegin() + offset);
			list.priorities.erase(list.priorities.cbegin() + offset);
		}
	}

	bool ExecuteCommand(const string& commandStr)
	{
		string commandId = commandStr.substr(0, commandStr.find(' '));
		const auto it = commands.find(commandId);
		bool result = false;
		if (it != commands.cend())
		{
			string argsStr = commandStr.size() > commandId.size() + 1 ? commandStr.substr(commandId.size() + 1) : "";
			HandlersList& handlers = it->second;
			for (size_t i = 0; i < handlers.funcs.size(); ++i)
			{
				result |= handlers.funcs[i](argsStr);
				if (!evalPriorityFunc(i, handlers.priorities))
				{
					break;
				}
			}
		}
		return result;
	}
};