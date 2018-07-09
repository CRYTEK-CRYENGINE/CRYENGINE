// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ActionSetActor.h"
#include "ResponseInstance.h"
#include "ResponseSystem.h"
#include "Variable.h"
#include "VariableCollection.h"

namespace
{
static const CHashedString s_specialResponderNameSender("Sender");
}

using namespace CryDRS;

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstanceUniquePtr CActionSetActor::Execute(DRS::IResponseInstance* pResponseInstance)
{
	CResponseInstance* pResponseInstanceImpl = static_cast<CResponseInstance*>(pResponseInstance);
	CHashedString newResponderName = m_newResponderName;

	if (m_newResponderName == s_specialResponderNameSender)
	{
		newResponderName = pResponseInstanceImpl->GetOriginalSender()->GetName();
	}
	//else if (m_newResponderName == "Player")
	//	newResponderName = Game::getPlayer
	//more specials to add if needed

	CResponseActor* pNewResponder = CResponseSystem::GetInstance()->GetResponseActor(newResponderName);

	if (pNewResponder)
	{
		pResponseInstanceImpl->SetCurrentActor(pNewResponder);
	}
	else
	{
		DrsLogWarning((string("Could not set new Responder, no Gameobject with name ") + newResponderName.GetText() + " was not existing").c_str());
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------
void CActionSetActor::Serialize(Serialization::IArchive& ar)
{
	ar(m_newResponderName, "ActorName", "^Actor Name");

#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit() && !m_newResponderName.IsValid())
	{
		ar.warning(m_newResponderName.m_textCopy, "No actor to set specified");
	}
#endif
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

DRS::IResponseActionInstanceUniquePtr CActionSetActorByVariable::Execute(DRS::IResponseInstance* pResponseInstance)
{
	CVariable* pVariable = GetCurrentVariable(static_cast<CResponseInstance*>(pResponseInstance));

	if (pVariable)
	{
		CResponseActor* pNewResponder = CResponseSystem::GetInstance()->GetResponseActor(pVariable->GetValueAsHashedString());
		if (pNewResponder)
		{
			pResponseInstance->SetCurrentActor(pNewResponder);
		}
		else
		{
			DrsLogWarning((string("Could not set new Responder, no actor found with name '") + pVariable->GetValueAsHashedString().GetText() + "' was existing").c_str());
		}
	}
	else
	{
		DrsLogWarning((string("Could not set new Responder, no Variable with name ") + GetVariableVerboseName() + " was existing").c_str());
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
void CActionSetActorByVariable::Serialize(Serialization::IArchive& ar)
{
	_Serialize(ar, "^VariableWhichHoldsActorName");

#if defined (ENABLE_VARIABLE_VALUE_TYPE_CHECKINGS)
	CVariableCollection* pCollection = CResponseSystem::GetInstance()->GetCollection(m_collectionName);
	if (pCollection)
	{
		if (pCollection->GetVariableValue(m_variableName).GetType() != eDRVT_String && pCollection->GetVariableValue(m_variableName).GetType() != eDRVT_Undefined)
		{
			ar.warning(m_collectionName.m_textCopy, "Variable to receive the new actor name from, needs to be a string variable");
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------

