// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProfileLogSystem.h"

//////////////////////////////////////////////////////////////////////////
// class CLogElement
//////////////////////////////////////////////////////////////////////////

CLogElement::CLogElement()
	: m_pParent(NULL)
	, m_time(0)
{
}

CLogElement::CLogElement(CLogElement* pParent)
	: m_pParent(pParent)
	, m_time(0)
{
}

CLogElement::CLogElement(CLogElement* pParent, const char* name, const char* message)
	: m_pParent(pParent)
	, m_strName(name)
	, m_strMessage(message)
	, m_time(0)
{
}

void CLogElement::Flush(stack_string& indent)
{
	if (m_logElements.empty())
	{
		CryLog("%s%s [%.3f ms] %s", indent.c_str(), m_strName.c_str(), m_time, m_strMessage.c_str());
		return;
	}

	CryLog("%s+%s [%.3f ms] %s", indent.c_str(), m_strName.c_str(), m_time, m_strMessage.c_str());

	indent += "  ";
	for (std::list<CLogElement>::iterator it = m_logElements.begin(); it != m_logElements.end(); ++it)
	{
		(*it).Flush(indent);
	}
	indent.erase(0, 2);

	CryLog("%s-%s", indent.c_str(), m_strName.c_str());
}

ILogElement* CLogElement::Log(const char* name, const char* message)
{
	m_logElements.push_back(CLogElement(this));
	m_logElements.back().m_strName = name;
	m_logElements.back().m_strMessage = message;

	return &m_logElements.back();
}

ILogElement* CLogElement::SetTime(float time)
{
	m_time = time;

	return m_pParent;
}

void CLogElement::Clear()
{
	m_logElements.resize(0);
}

//////////////////////////////////////////////////////////////////////////
// class CProfileLogSystem
//////////////////////////////////////////////////////////////////////////

CProfileLogSystem::CProfileLogSystem()
	: m_rootElelent(NULL)
	, m_pLastElelent(NULL)
{
}

CProfileLogSystem::~CProfileLogSystem()
{

}

ILogElement* CProfileLogSystem::Log(const char* name, const char* message)
{
	if (m_pLastElelent)
	{
		m_pLastElelent = m_pLastElelent->Log(name, message);
	}
	else
	{
		m_rootElelent.Clear();
		m_rootElelent.SetName(name);
		m_rootElelent.SetMessage(message);
		m_pLastElelent = &m_rootElelent;
	}

	return m_pLastElelent;
}

void CProfileLogSystem::SetTime(ILogElement* pElement, float time)
{
	if (pElement == NULL)
	{
		return;
	}

	if (m_pLastElelent = pElement->SetTime(time))
	{
		return;
	}

	stack_string indent;
	m_rootElelent.Flush(indent);
	m_rootElelent.Clear();
}

void CProfileLogSystem::Release()
{
	delete this;
}
