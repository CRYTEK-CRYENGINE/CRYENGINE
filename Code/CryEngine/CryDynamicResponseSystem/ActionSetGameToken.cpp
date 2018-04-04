// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "ActionSetGameToken.h"
#include "ResponseInstance.h"
#include "ResponseSystem.h"

#include <CryGame/IGameTokens.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <CrySystem/ISystem.h>
#include <CryGame/IGameFramework.h>

using namespace CryDRS;

//--------------------------------------------------------------------------------------------------
void CActionSetGameToken::Serialize(Serialization::IArchive& ar)
{
	ar(m_tokenName, "tokenname", "^TokenName");
	ar(m_valueToSet, "stringValue", "^ Value");
	ar(m_bCreateTokenIfNotExisting, "create", "^ Create");

#if !defined(_RELEASE)
	if (ar.isEdit() && ar.isOutput())
	{
		if (!m_tokenName.empty() && (m_tokenName.front() == ' ' || m_tokenName.back() == ' '))
		{
			ar.warning(m_tokenName, "GameToken name starts or ends with a space. Check if this is really wanted.");
		}
		if (!m_valueToSet.empty() && (m_valueToSet.front() == ' ' || m_valueToSet.back() == ' '))
		{
			ar.warning(m_valueToSet, "Value starts or ends with a space. Check if this is really wanted.");
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstanceUniquePtr CActionSetGameToken::Execute(DRS::IResponseInstance* pResponseInstance)
{
	IGameTokenSystem* pTokenSystem = gEnv->pGameFramework->GetIGameTokenSystem();
	if (m_bCreateTokenIfNotExisting)
	{
		pTokenSystem->SetOrCreateToken(m_tokenName.c_str(), TFlowInputData(m_valueToSet, true));
	}
	else
	{
		IGameToken* pToken = pTokenSystem->FindToken(m_tokenName.c_str());
		if (pToken)
		{
			pToken->SetValueFromString(m_valueToSet.c_str());
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_ERROR, "DRS: Could not find game token with name '%s'", m_tokenName.c_str());
		}

	}
	return nullptr;
}
