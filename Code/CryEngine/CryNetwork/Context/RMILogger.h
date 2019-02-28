// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RMILOGGER_H__
#define __RMILOGGER_H__

#pragma once

#include "Config.h"
#if INCLUDE_DEMO_RECORDING

	#include <CryNetwork/SimpleSerialize.h>
	#include "INetContextListener.h"

template<class T>
class CRMILoggerValue : public IRMILoggerValue
{
public:
	CRMILoggerValue(const T& value) : m_value(value) {}

	void WriteStream(CSimpleOutputStream& os, const char* key)
	{
		os.Put(key, m_value);
	}

private:
	const T& m_value;
};

	#define NOTIMPLEMENTED_RMI_LOGGER(type)                           \
	  template<>                                                      \
	  class CRMILoggerValue<type> : public IRMILoggerValue            \
	  {                                                               \
	  public:                                                         \
	    CRMILoggerValue(const type &value) {}                         \
	    void WriteStream(CSimpleOutputStream & os, const char* key)   \
	    {                                                             \
	      NetWarning("Not implemented: CRMILoggerValue<" # type ">"); \
	    }                                                             \
	  }

NOTIMPLEMENTED_RMI_LOGGER(ScriptAnyValue);
NOTIMPLEMENTED_RMI_LOGGER(CTimeValue);

class CRMILoggerImpl : public CSimpleSerializeImpl<true, eST_Network>
{
public:
	CRMILoggerImpl(const char* name, std::vector<IRMILogger*>& children) : m_children(children)
	{
		for (size_t i = 0; i < m_children.size(); ++i)
			m_children[i]->BeginFunction(name);
	}
	~CRMILoggerImpl()
	{
		for (size_t i = 0; i < m_children.size(); ++i)
			m_children[i]->EndFunction();
	}

	template<class T_Value>
	void Value(const char* name, T_Value& value)
	{
		CRMILoggerValue<T_Value> loggerValue(value);
		for (size_t i = 0; i < m_children.size(); ++i)
			m_children[i]->OnProperty(name, &loggerValue);
	}
	template<class T_Value, class T_Policy>
	ILINE void Value(const char* szName, T_Value& value, const T_Policy& policy)
	{
		Value(szName, value);
	}
	bool BeginOptionalGroup(const char* szName, bool group)
	{
		NetWarning("Not implemented: %s", __FUNCTION__);
		return false;
	}

private:
	std::vector<IRMILogger*>& m_children;
};

#endif

#endif
