// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <vector>
#include "dll_string.h"
#include "Util/EditorUtils.h"
#include "KeyboardShortcut.h"
#include <CryString/CryString.h>
#include <CryCore/functor.h>

class CCommand;

class EDITOR_COMMON_API CCommandArgument
{
	friend class CCommandDescription;

public:
	CCommandArgument(CCommand* const pCommand, const size_t index, const string& name, const string& description);

	CCommand*     GetCommand() const;
	const string& GetDescription() const;
	size_t        GetIndex() const;
	const string& GetName() const;

private:
	CCommand* m_pCommand;
	size_t    m_index;
	string    m_name;
	string    m_description;
};

class CCommandDescription
{
	friend class CCommand;

public:
	typedef std::vector<CCommandArgument> TParams;

	CCommandDescription(const string& description)
		: m_pCommand(nullptr)
		, m_description(description)
	{
	}
	CCommandDescription(const CCommandDescription& desc)
		: m_pCommand(desc.m_pCommand)
		, m_description(desc.m_description)
	{
		m_params.reserve(desc.m_params.size());
		for (const auto& it : desc.m_params)
		{
			m_params.push_back(it);
			m_params.back().m_pCommand = m_pCommand;
		}
	}

	CCommandDescription& Param(const string& name, const string& description)
	{
		CRY_ASSERT_MESSAGE(!name.empty(), "Argument name must not be empty!");
		CRY_ASSERT_MESSAGE(!description.empty(), "Argument description must not be empty!");
		m_params.emplace_back(m_pCommand, m_params.size(), name, description);

		return *this;
	}
	const TParams& GetParams() const
	{
		return m_params;
	}
	CCommandDescription& SetDescription(const string& description)
	{
		m_description = description;
		return *this;
	}
	const string& GetDescription() const
	{
		return m_description;
	}

private:
	void SetCommand(CCommand* const pParent)
	{
		m_pCommand = pParent;

		for (TParams::value_type& it : m_params)
		{
			it.m_pCommand = pParent;
		}
	}

	CCommand* m_pCommand;
	TParams   m_params;
	string    m_description;
};

class CCommand
{
public:
	CCommand(const string& module, const string& name, const CCommandDescription& description)
		: m_module(module)
		, m_name(name)
		, m_commandDescription(description)
		, m_bAlsoAvailableInScripting(false)
	{
		m_commandDescription.SetCommand(this);
	}
	CCommand(const CCommand& command)
		: m_module(command.m_module)
		, m_name(command.m_name)
		, m_commandDescription(command.m_commandDescription)
		, m_bAlsoAvailableInScripting(command.m_bAlsoAvailableInScripting)
	{
		m_commandDescription.SetCommand(this);
	}

	virtual ~CCommand()
	{}

	// Class for storing function parameters as a type-erased string
	struct CArgs
	{
	public:
		CArgs()
			: m_stringFlags(0)
		{}

		template<typename T>
		void Add(T p)
		{
			assert(m_args.size() < 8 * sizeof(m_stringFlags));
			m_args.push_back(ToString(p));
		}
		void Add(const char* p)
		{
			assert(m_args.size() < 8 * sizeof(m_stringFlags));
			m_stringFlags |= 1 << m_args.size();
			m_args.push_back(p);
		}
		bool IsStringArg(int i) const
		{
			if (i < 0 || i >= GetArgCount())
				return false;

			if (m_stringFlags & (1 << i))
				return true;
			else
				return false;
		}
		int           GetArgCount() const
		{ return m_args.size(); }
		const string& GetArg(int i) const
		{
			assert(0 <= i && i < GetArgCount());
			return m_args[i];
		}
	private:
		DynArray<string> m_args;
		unsigned char    m_stringFlags; // This is needed to quote string parameters when logging a command.
	};

	const string&                       GetName() const                { return m_name; }
	const string&                       GetModule() const              { return m_module; }
	const string&                       GetDescription() const         { return m_commandDescription.GetDescription(); }
	virtual string                      GetCommandString() const       { return m_module + "." + m_name; }

	void                                SetAvailableInScripting()      { m_bAlsoAvailableInScripting = true; };
	bool                                IsAvailableInScripting() const { return m_bAlsoAvailableInScripting; }

	const CCommandDescription::TParams& GetParameters() const          { return m_commandDescription.GetParams(); }

	virtual dll_string                  Execute(const CArgs& args) = 0;

	// Only a command without any arguments and return value can be a UI command.
	virtual bool CanBeUICommand() const  { return false; }
	virtual bool IsCustomCommand() const { return false; }
	virtual bool IsPolledKey() const     { return false; }

public:
	template<typename T>
	static string        ToString_(T t)                   { return ::ToString(t); }
	static inline string ToString_(const char* val)       { return val; }
	static inline string ToString_(const string& val)     { return val; }
	template<typename T>
	static bool          FromString_(T& t, const char* s) { return ::FromString(t, s); }
	static inline bool   FromString_(const char*& val, const char* s)
	{ return (val = s) != 0; }

protected:
	string              m_module;
	string              m_name;
	CCommandDescription m_commandDescription;
	bool                m_bAlsoAvailableInScripting;
};

class CUiCommand : public CCommand
{
public:

	CUiCommand(const string& module, const string& name, const CCommandDescription& description)
		: CCommand(module, name, description)
		, m_UiInfo(nullptr)
	{}

	~CUiCommand()
	{
		SAFE_DELETE(m_UiInfo);
	}

	// UI metadata for this command, if any
	struct UiInfo
	{
		string            buttonText;
		string            icon;
		CKeyboardShortcut key;

		UiInfo()
			: buttonText(nullptr)
			, icon(nullptr)
		{}

		UiInfo(const char* text, const char* iconName, const CKeyboardShortcut& shortcut)
			: buttonText(text)
			, icon(iconName)
			, key(shortcut)
		{}

		virtual ~UiInfo() {}

		virtual void operator=(const UiInfo& info)
		{
			buttonText = info.buttonText;
			icon = info.icon;
			key = info.key;
		}
	};

	void         SetDescription(const char* description) { m_commandDescription.SetDescription(description); }

	virtual bool CanBeUICommand() const override         { return true; }

	UiInfo*      GetUiInfo() const                       { return m_UiInfo; }

	void SetUiInfo(UiInfo* info)
	{
		if (m_UiInfo != info)
		{
			SAFE_DELETE(m_UiInfo);
			m_UiInfo = info;
		}
	}

private:



	UiInfo* m_UiInfo;
};

class CCommand0 : public CUiCommand
{
public:

	CCommand0(const string& module, const string& name, const CCommandDescription& description, const Functor0& functor)
		: CUiCommand(module, name, description)
		, m_functor(functor)
	{}

	inline dll_string Execute(const CArgs& args)
	{
		assert(args.GetArgCount() == 0);

		m_functor();
		return "";
	}

private:

	Functor0 m_functor;
};

template<typename RT>
class CCommand0wRet : public CUiCommand
{
public:
	CCommand0wRet(const string& module, const string& name, const CCommandDescription& description, const Functor0wRet<RT>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor0wRet<RT> m_functor;
};

template<LIST(1, typename P)>
class CCommand1 : public CCommand
{
public:
	CCommand1(const string& module, const string& name, const CCommandDescription& description, const Functor1<LIST(1, P)>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor1<LIST(1, P)> m_functor;
};

template<LIST(1, typename P), typename RT>
class CCommand1wRet : public CCommand
{
public:
	CCommand1wRet(const string& module, const string& name, const CCommandDescription& description, const Functor1wRet<LIST(1, P), RT>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor1wRet<LIST(1, P), RT> m_functor;
};

template<LIST(2, typename P)>
class CCommand2 : public CCommand
{
public:
	CCommand2(const string& module, const string& name, const CCommandDescription& description, const Functor2<LIST(2, P)>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor2<LIST(2, P)> m_functor;
};

template<LIST(2, typename P), typename RT>
class CCommand2wRet : public CCommand
{
public:
	CCommand2wRet(const string& module, const string& name, const CCommandDescription& description, const Functor2wRet<LIST(2, P), RT>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor2wRet<LIST(2, P), RT> m_functor;
};

template<LIST(3, typename P)>
class CCommand3 : public CCommand
{
public:
	CCommand3(const string& module, const string& name, const CCommandDescription& description, const Functor3<LIST(3, P)>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor3<LIST(3, P)> m_functor;
};

template<LIST(3, typename P), typename RT>
class CCommand3wRet : public CCommand
{
public:
	CCommand3wRet(const string& module, const string& name, const CCommandDescription& description, const Functor3wRet<LIST(3, P), RT>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor3wRet<LIST(3, P), RT> m_functor;
};

template<LIST(4, typename P)>
class CCommand4 : public CCommand
{
public:
	CCommand4(const string& module, const string& name, const CCommandDescription& description, const Functor4<LIST(4, P)>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor4<LIST(4, P)> m_functor;
};

template<LIST(4, typename P), typename RT>
class CCommand4wRet : public CCommand
{
public:
	CCommand4wRet(const string& module, const string& name, const CCommandDescription& description, const Functor4wRet<LIST(4, P), RT>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor4wRet<LIST(4, P), RT> m_functor;
};

template<LIST(5, typename P)>
class CCommand5 : public CCommand
{
public:
	CCommand5(const string& module, const string& name, const CCommandDescription& description, const Functor5<LIST(5, P)>& functor);

	dll_string Execute(const CArgs& args);

protected:

	Functor5<LIST(5, P)> m_functor;
};

//////////////////////////////////////////////////////////////////////////
template<typename RT>
CCommand0wRet<RT>::CCommand0wRet(const string& module, const string& name, const CCommandDescription& description, const Functor0wRet<RT>& functor)
	: CUiCommand(module, name, description), m_functor(functor)
{
}

template<typename RT>
dll_string CCommand0wRet<RT >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 0);

	RT ret = m_functor();
	return ToString_(ret).c_str();
}

//////////////////////////////////////////////////////////////////////////

template<LIST(1, typename P)>
CCommand1<LIST(1, P)>::CCommand1(const string& module, const string& name, const CCommandDescription& description, const Functor1<LIST(1, P)>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(1, typename P)>
dll_string CCommand1<LIST(1, P) >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 1);
	if (args.GetArgCount() < 1)
	{
		CryLogAlways("Cannot execute the command %s.%s! One argument required.", GetModule().c_str(), GetName().c_str());
		return "";
	}

	P1 p1;
	bool ok = FromString_(p1, args.GetArg(0).c_str());
	if (ok)
		m_functor(p1);
	else
		CryLogAlways("Cannot execute the command %s.%s(%s)! Invalid argument type.",
		             GetModule(), GetName(), args.GetArg(0).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////

template<LIST(1, typename P), typename RT>
CCommand1wRet<LIST(1, P), RT>::CCommand1wRet(const string& module, const string& name, const CCommandDescription& description, const Functor1wRet<LIST(1, P), RT>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(1, typename P), typename RT>
dll_string CCommand1wRet<LIST(1, P), RT >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 1);
	if (args.GetArgCount() < 1)
	{
		CryLogAlways("Cannot execute the command %s.%s! One argument required.", m_module.c_str(), m_name.c_str());
		return "";
	}

	P1 p1;
	bool ok = FromString_(p1, args.GetArg(0).c_str());
	if (ok)
	{
		RT ret = m_functor(p1);
		return ToString_(ret).c_str();
	}
	else
		CryLogAlways("Cannot execute the command %s.%s(%s)! Invalid argument type.",
		             GetModule(), GetName(), args.GetArg(0).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////

template<LIST(2, typename P)>
CCommand2<LIST(2, P)>::CCommand2(const string& module, const string& name, const CCommandDescription& description, const Functor2<LIST(2, P)>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(2, typename P)>
dll_string CCommand2<LIST(2, P) >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 2);
	if (args.GetArgCount() < 2)
	{
		CryLogAlways("Cannot execute the command %s.%s! Two arguments required.", GetModule().c_str(), GetName().c_str());
		return "";
	}

	P1 p1;
	P2 p2;
	bool ok = FromString_(p1, args.GetArg(0).c_str())
	          && FromString_(p2, args.GetArg(1).c_str());
	if (ok)
		m_functor(p1, p2);
	else
		CryLogAlways("Cannot execute the command %s.%s(%s,%s)! Invalid argument type(s).",
		             GetModule(), GetName(), args.GetArg(0).c_str(), args.GetArg(1).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////

template<LIST(2, typename P), typename RT>
CCommand2wRet<LIST(2, P), RT>::CCommand2wRet(const string& module, const string& name, const CCommandDescription& description, const Functor2wRet<LIST(2, P), RT>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(2, typename P), typename RT>
dll_string CCommand2wRet<LIST(2, P), RT >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 2);
	if (args.GetArgCount() < 2)
	{
		CryLogAlways("Cannot execute the command %s.%s! Two arguments required.", GetModule().c_str(), GetName().c_str());
		return "";
	}

	P1 p1;
	P2 p2;
	bool ok = FromString_(p1, args.GetArg(0).c_str())
	          && FromString_(p2, args.GetArg(1).c_str());
	if (ok)
	{
		RT ret = m_functor(p1, p2);
		return ToString_(ret).c_str();
	}
	else
		CryLogAlways("Cannot execute the command %s.%s(%s,%s)! Invalid argument type(s).",
		             GetModule(), GetName(), args.GetArg(0).c_str(), args.GetArg(1).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////

template<LIST(3, typename P)>
CCommand3<LIST(3, P)>::CCommand3(const string& module, const string& name, const CCommandDescription& description, const Functor3<LIST(3, P)>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(3, typename P)>
dll_string CCommand3<LIST(3, P) >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 3);
	if (args.GetArgCount() < 3)
	{
		CryLogAlways("Cannot execute the command %s.%s! Three arguments required.", GetModule().c_str(), GetName().c_str());
		return "";
	}

	P1 p1;
	P2 p2;
	P3 p3;
	bool ok = FromString_(p1, args.GetArg(0).c_str())
	          && FromString_(p2, args.GetArg(1).c_str())
	          && FromString_(p3, args.GetArg(2).c_str());
	if (ok)
		m_functor(p1, p2, p3);
	else
		CryLogAlways("Cannot execute the command %s.%s(%s,%s,%s)! Invalid argument type(s).",
		             GetModule(), GetName(), args.GetArg(0).c_str(), args.GetArg(1).c_str(), args.GetArg(2).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////

template<LIST(3, typename P), typename RT>
CCommand3wRet<LIST(3, P), RT>::CCommand3wRet(const string& module, const string& name, const CCommandDescription& description, const Functor3wRet<LIST(3, P), RT>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(3, typename P), typename RT>
dll_string CCommand3wRet<LIST(3, P), RT >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 3);
	if (args.GetArgCount() < 3)
	{
		CryLogAlways("Cannot execute the command %s.%s! Three arguments required.", GetModule().c_str(), GetName().c_str());
		return "";
	}

	P1 p1;
	P2 p2;
	P3 p3;
	bool ok = FromString_(p1, args.GetArg(0).c_str())
	          && FromString_(p2, args.GetArg(1).c_str())
	          && FromString_(p3, args.GetArg(2).c_str());
	if (ok)
	{
		RT ret = m_functor(p1, p2, p3);
		return ToString_(ret).c_str();
	}
	else
		CryLogAlways("Cannot execute the command %s.%s(%s,%s,%s)! Invalid argument type(s).",
		             GetModule(), GetName(), args.GetArg(0).c_str(), args.GetArg(1).c_str(), args.GetArg(2).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////

template<LIST(4, typename P)>
CCommand4<LIST(4, P)>::CCommand4(const string& module, const string& name, const CCommandDescription& description, const Functor4<LIST(4, P)>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(4, typename P)>
dll_string CCommand4<LIST(4, P) >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 4);
	if (args.GetArgCount() < 4)
	{
		CryLogAlways("Cannot execute the command %s.%s! Four arguments required.", GetModule().c_str(), GetName().c_str());
		return "";
	}

	P1 p1;
	P2 p2;
	P3 p3;
	P4 p4;
	bool ok = FromString_(p1, args.GetArg(0).c_str())
	          && FromString_(p2, args.GetArg(1).c_str())
	          && FromString_(p3, args.GetArg(2).c_str())
	          && FromString_(p4, args.GetArg(3).c_str());
	if (ok)
		m_functor(p1, p2, p3, p4);
	else
		CryLogAlways("Cannot execute the command %s.%s(%s,%s,%s,%s)! Invalid argument type(s).",
		             GetModule(), GetName(), args.GetArg(0).c_str(), args.GetArg(1).c_str(), args.GetArg(2).c_str(),
		             args.GetArg(3).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////

template<LIST(4, typename P), typename RT>
CCommand4wRet<LIST(4, P), RT>::CCommand4wRet(const string& module, const string& name, const CCommandDescription& description, const Functor4wRet<LIST(4, P), RT>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(4, typename P), typename RT>
dll_string CCommand4wRet<LIST(4, P), RT >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 4);
	if (args.GetArgCount() < 4)
	{
		CryLogAlways("Cannot execute the command %s.%s! Four arguments required.", GetModule().c_str(), GetName().c_str());
		return "";
	}

	P1 p1;
	P2 p2;
	P3 p3;
	P4 p4;
	bool ok = FromString_(p1, args.GetArg(0).c_str())
	          && FromString_(p2, args.GetArg(1).c_str())
	          && FromString_(p3, args.GetArg(2).c_str())
	          && FromString_(p4, args.GetArg(3).c_str());
	if (ok)
	{
		RT ret = m_functor(p1, p2, p3, p4);
		return ToString_(ret).c_str();
	}
	else
		CryLogAlways("Cannot execute the command %s.%s(%s,%s,%s,%s)! Invalid argument type(s).",
		             GetModule(), GetName(), args.GetArg(0).c_str(), args.GetArg(1).c_str(), args.GetArg(2).c_str(),
		             args.GetArg(3).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////

template<LIST(5, typename P)>
CCommand5<LIST(5, P)>::CCommand5(const string& module, const string& name, const CCommandDescription& description, const Functor5<LIST(5, P)>& functor)
	: CCommand(module, name, description), m_functor(functor)
{
}

template<LIST(5, typename P)>
dll_string CCommand5<LIST(5, P) >::Execute(const CCommand::CArgs& args)
{
	assert(args.GetArgCount() == 5);
	if (args.GetArgCount() < 5)
	{
		CryLogAlways("Cannot execute the command %s.%s! Five arguments required.", GetModule().c_str(), GetName().c_str());
		return "";
	}

	P1 p1;
	P2 p2;
	P3 p3;
	P4 p4;
	P5 p5;
	bool ok = FromString_(p1, args.GetArg(0).c_str())
	          && FromString_(p2, args.GetArg(1).c_str())
	          && FromString_(p3, args.GetArg(2).c_str())
	          && FromString_(p4, args.GetArg(3).c_str())
	          && FromString_(p5, args.GetArg(4).c_str());
	if (ok)
		m_functor(p1, p2, p3, p4, p5);
	else
		CryLogAlways("Cannot execute the command %s.%s(%s,%s,%s,%s,%s)! Invalid argument type(s).",
		             GetModule(), GetName(), args.GetArg(0).c_str(), args.GetArg(1).c_str(), args.GetArg(2).c_str(),
		             args.GetArg(3).c_str(), args.GetArg(4).c_str());
	return "";
}

//////////////////////////////////////////////////////////////////////////
inline string CommandString(const char* commandString)
{
	return string(commandString);
}

template<LIST(1, typename P)>
inline string CommandString(const char* commandString, P1 p1)
{
	return string(commandString) + " " + CCommand::ToString_(p1);
}

template<LIST(2, typename P)>
inline string CommandString(const char* commandString, P1 p1, P2 p2)
{
	return string(commandString) + " " + CCommand::ToString_(p1) + " " + CCommand::ToString_(p2);
}

template<LIST(3, typename P)>
inline string CommandString(const char* commandString, P1 p1, P2 p2, P3 p3)
{
	return string(commandString) + " " + CCommand::ToString_(p1) + " " + CCommand::ToString_(p2) + " " + CCommand::ToString_(p3);
}

template<LIST(4, typename P)>
inline string CommandString(const char* commandString, P1 p1, P2 p2, P3 p3, P4 p4)
{
	return string(commandString) + " " + CCommand::ToString_(p1) + " " + CCommand::ToString_(p2) + " " + CCommand::ToString_(p3) + " " + CCommand::ToString_(p4);
}

template<LIST(5, typename P)>
inline string CommandString(const char* commandString, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
{
	return string(commandString) + " " + CCommand::ToString_(p1) + " " + CCommand::ToString_(p2) + " " + CCommand::ToString_(p3) + " " + CCommand::ToString_(p4) + " " + CCommand::ToString_(p5);
}

