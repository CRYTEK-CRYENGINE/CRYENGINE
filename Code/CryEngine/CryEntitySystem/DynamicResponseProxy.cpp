// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DynamicResponseProxy.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryNetwork/ISerialize.h>

CRYREGISTER_CLASS(CEntityComponentDynamicResponse);

//////////////////////////////////////////////////////////////////////////
CEntityComponentDynamicResponse::CEntityComponentDynamicResponse()
	: m_pResponseActor(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
CEntityComponentDynamicResponse::~CEntityComponentDynamicResponse()
{
	gEnv->pDynamicResponseSystem->ReleaseResponseActor(m_pResponseActor);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentDynamicResponse::Initialize()
{
	ReInit();
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentDynamicResponse::ProcessEvent(const SEntityEvent& event)
{
	if (m_pResponseActor && event.event == ENTITY_EVENT_RESET)
	{
		SET_DRS_USER_SCOPED("DrsProxy Reset Event");
		DRS::IVariableCollection* pVariables = m_pResponseActor->GetLocalVariables();
		if (pVariables)
		{
			pVariables->SetVariableValue("Name", CHashedString(m_pResponseActor->GetName()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 CEntityComponentDynamicResponse::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

//////////////////////////////////////////////////////////////////////////
DRS::IVariableCollection* CEntityComponentDynamicResponse::GetLocalVariableCollection() const
{
	CRY_ASSERT_MESSAGE(m_pResponseActor, "DRS Component without an Actor detected. Should never happen.");
	return m_pResponseActor->GetLocalVariables();
}

//////////////////////////////////////////////////////////////////////////
DRS::IResponseActor* CEntityComponentDynamicResponse::GetResponseActor() const
{
	CRY_ASSERT_MESSAGE(m_pResponseActor, "DRS Component without an Actor detected. Should never happen.");
	return m_pResponseActor;
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentDynamicResponse::ReInit(const char* szName, const char* szGlobalVariableCollectionToUse)
{
	if (m_pResponseActor)
	{
		gEnv->pDynamicResponseSystem->ReleaseResponseActor(m_pResponseActor);
		m_pResponseActor = nullptr;
	}

	if (!szName)
		szName = m_pEntity->GetName();

	m_pResponseActor = gEnv->pDynamicResponseSystem->CreateResponseActor(szName, m_pEntity->GetId(), szGlobalVariableCollectionToUse);
	SET_DRS_USER_SCOPED("DrsProxy Initialize");
	DRS::IVariableCollection* pVariables = m_pResponseActor->GetLocalVariables();
	if (pVariables)
	{
		pVariables->SetVariableValue("Name", CHashedString(m_pResponseActor->GetName()));
	}
}
