// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameMechanismManager.h"
#include "GameMechanismBase.h"
#include "Utility/CryWatch.h"

CGameMechanismManager * CGameMechanismManager::s_instance = NULL;

#if defined(_RELEASE)
#define MechanismManagerLog(...)    (void)(0)
#define MechanismManagerWatch(...)  (void)(0)
#else
#define MechanismManagerLog(...)    do { if(m_cvarLogEnabled) CryLogAlways("[CGameMechanismManager] " __VA_ARGS__); } while(0)
#define MechanismManagerWatch(...)  do { if(m_cvarWatchEnabled) CryWatch("[CGameMechanismManager] " __VA_ARGS__); } while(0)
#endif

class CGameMechanismIterator
{
	public:
	static CGameMechanismIterator * s_firstIterator;
	CGameMechanismIterator * m_nextIterator;
	CGameMechanismBase * m_nextMechanism;

	CGameMechanismIterator(CGameMechanismBase * first)
	{
		m_nextMechanism = first;
		m_nextIterator = s_firstIterator;
		s_firstIterator = this;
	}

	~CGameMechanismIterator()
	{
		CRY_ASSERT_MESSAGE (s_firstIterator == this, "Should only free up game mechanism iterators in reverse order to their creation!");
		s_firstIterator = m_nextIterator;
	}

	CGameMechanismBase * GetNext()
	{
		CGameMechanismBase * returnThis = m_nextMechanism;
		if (returnThis)
		{
			m_nextMechanism = returnThis->GetLinkedListPointers()->m_nextMechanism;
		}
		return returnThis;
	}
};

CGameMechanismIterator * CGameMechanismIterator::s_firstIterator = NULL;

CGameMechanismManager::CGameMechanismManager()
{
	m_firstMechanism = NULL;

#if !defined(_RELEASE)
	m_cvarWatchEnabled = false;
	m_cvarLogEnabled = false;

	REGISTER_CVAR2("g_mechanismMgrWatch", & m_cvarWatchEnabled, m_cvarWatchEnabled, 0, "MECHANISM MANAGER: On-screen watches enabled");
	REGISTER_CVAR2("g_mechanismMgrLog", & m_cvarLogEnabled, m_cvarLogEnabled, 0, "MECHANISM MANAGER: Log messages enabled");
#endif

	MechanismManagerLog ("Manager created");

	assert (s_instance == NULL);
	s_instance = this;
}

CGameMechanismManager::~CGameMechanismManager()
{
	MechanismManagerLog ("Unregistering all remaining mechanisms:");

	while (m_firstMechanism)
	{
		delete m_firstMechanism;
	};

	MechanismManagerLog ("No game mechanisms remaining; game mechanism manager destroyed");

#if !defined(_RELEASE)
	IConsole * console = GetISystem()->GetIConsole();
	assert (console);

	console->UnregisterVariable("g_mechanismMgrWatch", true);
	console->UnregisterVariable("g_mechanismMgrLog", true);
#endif

	assert (s_instance == this);
	s_instance = NULL;
}

void CGameMechanismManager::RegisterMechanism(CGameMechanismBase * mechanism)
{
	MechanismManagerLog ("Registering %p: %s", mechanism, mechanism->GetName());

	if (m_firstMechanism)
	{
		CGameMechanismBase::SLinkedListPointers * oldFirstPointers = m_firstMechanism->GetLinkedListPointers();
		assert (oldFirstPointers->m_prevMechanism == NULL);
		oldFirstPointers->m_prevMechanism = mechanism;
	}

	CGameMechanismBase::SLinkedListPointers * newFirstPointers = mechanism->GetLinkedListPointers();
	newFirstPointers->m_nextMechanism = m_firstMechanism;
	m_firstMechanism = mechanism;
}

void CGameMechanismManager::UnregisterMechanism(CGameMechanismBase * removeThis)
{
	assert (removeThis);

	for (CGameMechanismIterator * eachIterator = CGameMechanismIterator::s_firstIterator; eachIterator; eachIterator = eachIterator->m_nextIterator)
	{
		if (eachIterator->m_nextMechanism == removeThis)
		{
			eachIterator->m_nextMechanism = removeThis->GetLinkedListPointers()->m_nextMechanism;
		}
	}

	CGameMechanismBase::SLinkedListPointers * removeThisPointers = removeThis->GetLinkedListPointers();
	MechanismManagerLog ("Unregistering %p: %s", removeThis, removeThis->GetName());

	if (removeThisPointers->m_nextMechanism)
	{
		CGameMechanismBase::SLinkedListPointers * afterPointers = removeThisPointers->m_nextMechanism->GetLinkedListPointers();
		assert (afterPointers->m_prevMechanism == removeThis);
		afterPointers->m_prevMechanism = removeThisPointers->m_prevMechanism;
	}

	if (removeThisPointers->m_prevMechanism)
	{
		CGameMechanismBase::SLinkedListPointers * beforePointers = removeThisPointers->m_prevMechanism->GetLinkedListPointers();
		assert (beforePointers->m_nextMechanism == removeThis);
		assert (m_firstMechanism != removeThis);
		beforePointers->m_nextMechanism = removeThisPointers->m_nextMechanism;
	}
	else
	{
		assert (m_firstMechanism == removeThis);
		m_firstMechanism = removeThisPointers->m_nextMechanism;
	}
}

void CGameMechanismManager::Update(float dt)
{
	CGameMechanismIterator iter(m_firstMechanism);

	while (CGameMechanismBase * eachMechanism = iter.GetNext())
	{
		MechanismManagerWatch ("Updating %s", eachMechanism->GetName());
		eachMechanism->Update(dt);
	}
}

void CGameMechanismManager::Inform(EGameMechanismEvent gmEvent, const SGameMechanismEventData * data)
{
#if !defined(_RELEASE)
	AUTOENUM_BUILDNAMEARRAY(names, GameMechanismEventList);
#endif

	CGameMechanismIterator iter(m_firstMechanism);

	while (CGameMechanismBase * eachMechanism = iter.GetNext())
	{
		MechanismManagerLog ("Telling %s about '%s'%s", eachMechanism->GetName(), names[gmEvent], data ? " with additional data attached" : "");
		eachMechanism->Inform(gmEvent, data);
	}
}
