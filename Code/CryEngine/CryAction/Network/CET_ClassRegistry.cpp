// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CET_ClassRegistry.h"
#include "GameClientChannel.h"
#include <CryNetwork/NetHelpers.h>

/*
 * Register Classes
 */

class CCET_RegisterClasses : public CCET_Base
{
public:
	CCET_RegisterClasses(CClassRegistryReplicator* pRep) : m_pRep(pRep) {}

	const char*                 GetName() { return "RegisterClasses"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		// setup our class database
		IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
		IEntityClass* pClass;
		pClassRegistry->IteratorMoveFirst();
		while (pClass = pClassRegistry->IteratorNext())
		{
			m_pRep->RegisterClassName(pClass->GetName(), ~uint16(0));
		}
		return eCETR_Ok;
	}

private:
	CClassRegistryReplicator* m_pRep;
};

void AddRegisterAllClasses(IContextEstablisher* pEst, EContextViewState state, CClassRegistryReplicator* pRep)
{
	pEst->AddTask(state, new CCET_RegisterClasses(pRep));
}

/*
 * Send Classes
 */

class CCET_SendClasses : public CCET_Base
{
public:
	CCET_SendClasses(CClassRegistryReplicator* pRep) : m_pRep(pRep)
	{
	}

	const char*                 GetName() { return "SendClasses"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		// setup a class-name to id mapping
		m_waitFor.reserve(m_pRep->NumClassIds());
		for (size_t i = 0; i < m_pRep->NumClassIds(); i++)
		{
			string name;
			if (m_pRep->ClassNameFromId(name, static_cast<uint16>(i)))
			{
				INetSendable* pMsg = new CSimpleNetMessage<SEntityClassRegistration>(SEntityClassRegistration(uint16(i), name), CGameClientChannel::RegisterEntityClass);
				pMsg->SetGroup('clas');
				SSendableHandle hdl;
				state.pSender->AddSendable(pMsg, 0, NULL, &hdl);
				m_waitFor.push_back(hdl);
			}
		}
		return eCETR_Ok;
	}

	std::vector<SSendableHandle> m_waitFor;

private:
	CClassRegistryReplicator* m_pRep;
};

void AddSendClassRegistration(IContextEstablisher* pEst, EContextViewState state, CClassRegistryReplicator* pRep, std::vector<SSendableHandle>** ppWaitFor)
{
	CCET_SendClasses* pCET = new CCET_SendClasses(pRep);
	*ppWaitFor = &pCET->m_waitFor;
	pEst->AddTask(state, pCET);
}

/*
 * Send Classes Hash
 */

class CCET_SendClassesHash : public CCET_Base
{
public:
	CCET_SendClassesHash(CClassRegistryReplicator* pRep) : m_pRep(pRep)
	{
	}

	const char*                 GetName() { return "SendClassesHash"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		uint32 crc = m_pRep->GetHash();
		INetSendable* pMsg = new CSimpleNetMessage<SEntityClassHashRegistration>(SEntityClassHashRegistration(crc), CGameClientChannel::RegisterEntityClassHash);
		pMsg->SetGroup('clsh');
		SSendableHandle hdl;
		state.pSender->AddSendable(pMsg, 0, NULL, &hdl);
		m_waitFor.push_back(hdl);
		return eCETR_Ok;
	}

	std::vector<SSendableHandle> m_waitFor;

private:
	CClassRegistryReplicator* m_pRep;
};

void AddSendClassHashRegistration(IContextEstablisher* pEst, EContextViewState state, CClassRegistryReplicator* pRep, std::vector<SSendableHandle>** ppWaitFor)
{
	CCET_SendClassesHash* pCET = new CCET_SendClassesHash(pRep);
	*ppWaitFor = &pCET->m_waitFor;
	pEst->AddTask(state, pCET);
}
