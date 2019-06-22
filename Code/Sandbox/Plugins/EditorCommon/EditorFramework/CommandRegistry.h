// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryCore/StlUtils.h>
#include <CrySandbox/CryFunction.h>

#include <unordered_map>

namespace CommandRegistry
{

template<class T>
struct StripQualifiers
{
	using type = std::decay_t<T>;
};

template<class T>
struct StripQualifiers<T*>
{
	using type = std::add_pointer_t<std::decay_t<std::remove_pointer_t<T>>>;
};

//! Specializations of this class perform actual parsing and storing of a value.
//! It should define 2 method. First is:
//!   static int Store(STypeArg& obj, const string& str, int pos)
//! which receives the parameters string and the position where the current type's value starts.
//! The methods must return the position one passed the last character or the type's parameter,
//! e.g. the position of space char marking the end of the parameter.
//! 
//! And the second method is:
//!   T Read() const
//! which returns the stored value.
template<class T>
struct STypeArg {};

template<>
struct STypeArg<char*>
{
	static int Store(STypeArg& obj, const string& str, int pos) 
	{ 
		if (str[pos] != '\'') return -1;
		int strStart = pos + 1;
		pos = str.find('\'', strStart);
		if (pos == std::string::npos)
		{
			return -1;
		}
		obj.payload = str.substr(strStart, pos - strStart);
		return pos + 1;
	}
	const char* Read() const { return payload.c_str(); }
	string payload;
};

template<>
struct STypeArg<string> : STypeArg<char*>
{
	const string& Read() const { return payload; }
};

template<>
struct STypeArg<bool>
{
	static int Store(STypeArg& obj, const string& str, int pos) 
	{ 
		if (str.compare(pos, 4, "true") == 0)
		{
			obj.payload = true;
			return pos + 4;
		}
		if (str.compare(pos, 5, "false") == 0)
		{
			obj.payload = false;
			return pos + 5;
		}
		if (str.compare(pos, 1, "1") == 0)
		{
			obj.payload = true;
			return pos + 1;
		}
		if (str.compare(pos, 1, "0") == 0)
		{
			obj.payload = false;
			return pos + 1;
		}
		return -1;
	}
	bool Read() const { return payload; }
	bool payload;
};

template<>
struct STypeArg<int>
{
	static int Store(STypeArg& obj, const string& str, int pos) 
	{ 
		auto endPos = str.find(' ', pos);
		endPos = endPos == std::string::npos ? str.size() : endPos;
		string intStr = str.substr(pos, endPos - pos);
		obj.payload = std::atoi(intStr.c_str());
		return obj.payload == 0 && intStr != "0" ? -1 : endPos;
	}
	int Read() const { return payload; }
	int payload;
};

template<>
struct STypeArg<float>
{
	static int Store(STypeArg& obj, const string& str, int pos) 
	{ 
		auto endPos = str.find(' ', pos);
		endPos = endPos == std::string::npos ? str.size() : endPos;
		string floatStr = str.substr(pos, endPos - pos);
		obj.payload = static_cast<float>(std::atof(floatStr.c_str()));
		return obj.payload == 0 && floatStr != "0" ? -1 : endPos;
	}
	float Read() const { return payload; }
	float payload;
};

//! This is type iterators terminator.
//! It's called when there are no more types to go through and it checks if we reached 
//! the end of the arguments string or the start of the next argument. In the later 
//! case all remaining arguments are to be ignored.
template<int, class... T>
struct STypeIterator
{
	template<class Tuple>
	static bool StoreArgs(Tuple&, const string& argsStr, int pos)
	{
		return argsStr.size() == pos - 1 || argsStr[pos - 1] == ' ';
	}
};

//! This is type iterators specialization for function without parameters that don't need
//! a validity check.
template<>
struct STypeIterator<0>
{
	template<class Tuple>
	static bool StoreArgs(Tuple&, const string& argsStr, int pos) { return true; }
};

//! This type iterator goes through all the given types and calls Store static method on them.
//! The parsed value will be stored in the tuples element corresponding to the index or the type.
template<int index, class First, class... Rest>
struct STypeIterator<index, First, Rest...>
{
	template<class Tuple>
	static bool StoreArgs(Tuple& t, const string& argsStr, int pos)
	{
		pos = First::Store(std::get<index>(t), argsStr, pos);
		// we check here if there is space char between parameters.
		if (pos == -1 || (pos < static_cast<int>(argsStr.size()) && argsStr[pos] != ' '))
		{
			return false;
		}
		// recursive call for the rest of the types.
		return STypeIterator<index + 1, Rest...>::StoreArgs(t, argsStr, pos + 1);
	}
};

template<class... Params, std::size_t... I>
bool InvokeCommand(std::function<bool(Params...)> f, const string& argsStr, std::index_sequence<I...>)
{
	// Since parameters are passed as a string we use std::tuple to store temporary arg values.
	// This is necessary for pointer types like.
	std::tuple<STypeArg<typename StripQualifiers<Params>::type>...> t;
	// Pass the parameters string to the the type iterator to parse and store them recursively.
	if (!STypeIterator<0, STypeArg<typename StripQualifiers<Params>::type>...>::StoreArgs(t, argsStr, 0))
	{
		CRY_ASSERT_MESSAGE(false, "couldn't parse command arguments %s", argsStr);
		return false;
	}
	// Read all the parsed and stored parameters and pass them to the final function.
	return f(std::get<I>(t).Read()...);
}

template<class... Params, typename Indices = std::make_index_sequence<sizeof...(Params)>>
bool InvokeCommand(std::function<bool(Params...)> f, const string& argsStr)
{
	return InvokeCommand(std::move(f), argsStr, Indices{});
}

}

template<class T>
using StringMap = std::unordered_map<string, T, stl::hash_strcmp<string>, stl::hash_strcmp<string>>;

//! This class is responsible for registration of commands (functions, event handlers) with arbitrary number of parameters
//! as well as executing them.
//! 
//! Execution is carried out by passing a string containing the commands' id and space-separated list of 
//! arguments. Every argument is to be parsed by the corresponding type and passed to the final function.
//! 
//! Execution of a command returns true if the command (or event) was handled. Customs functions determine themselves
//! if an event is handled by returning boolean flag. If a custom command handler returns void (only bool and void are allowed)
//! the command is always considered to be handled.
struct SCommandRegistry
{
	StringMap<std::function<bool(const string&)>> commands;

	//! Registers a std::function as a command.
	template<class... Params>
	void RegisterCommand(const string& id, std::function<bool(Params...)> f)
	{
		auto func = [f = std::move(f)](const string& argsStr) -> bool
		{
			return CommandRegistry::InvokeCommand(std::move(f), argsStr);
		};

		commands[id] = std::move(func);
	}

	//! Registers a void returning std::function as a command.
	template<class... Params>
	void RegisterCommand(const string& id, std::function<void(Params...)> f)
	{
		std::function<bool(Params...)> func = [f = std::move(f)](Params&&... params) -> bool
		{
			f(std::forward<Params>(params)...);
			return true;
		};
		RegisterCommand(id, std::move(func));
	}

	//! Registers a lambda or a function as a command.
	template<class Func>
	void RegisterCommand(const string& id, Func&& f)
	{
		RegisterCommand(id, ToStdFunction(std::forward<Func>(f)));
	}

	//! Registers a member function pointer as a command.
	template<class T, class Func>
	void RegisterCommand(const string& id, Func&& f, T* pObj)
	{
		RegisterCommand(id, ToStdFunction(std::forward<Func>(f), pObj));
	}

	void UnregisterCommand(const string& id)
	{
		commands.erase(id);
	}

	//! Executes a command.
	//! /param commandStr A string containing the id of the command and space-separated list of arguments.
	//! If the number of arguments exceeds the number of parameters the corresponding command (or event handler)
	//! accepts all extra arguments will be ignored and the handler will receive only relevant arguments.
	//! If the number of arguments is fewer the execution will fail.
	//! /return true if the command was handled.
	bool ExecuteCommand(const string& commandStr)
	{
		string commandId = commandStr.substr(0, commandStr.find(' '));
		const auto it = commands.find(commandId);
		if (it != commands.cend())
		{
			string argsStr = commandStr.size() > commandId.size() + 1 ? commandStr.substr(commandId.size() + 1) : "";
			return it->second(argsStr);
		}
		return false;
	}
};