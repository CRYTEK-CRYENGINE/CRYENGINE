// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ActionSetGameToken.h"
#include "ResponseInstance.h"
#include "ResponseSystem.h"

#include <CryGame/IGameTokens.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <CrySystem/ISystem.h>
#include <CryGame/IGameFramework.h>

using namespace CryDRS;

SERIALIZATION_ENUM_BEGIN_NESTED(CActionSetGameToken, EValueToSet, "OperationType")
SERIALIZATION_ENUM(CActionSetGameToken::EValueToSet::StringValue, "stringValue", "StringValue")
SERIALIZATION_ENUM(CActionSetGameToken::EValueToSet::BoolValue, "boolValue", "BooleanValue")
SERIALIZATION_ENUM(CActionSetGameToken::EValueToSet::IntValue, "intValue", "IntValue")
SERIALIZATION_ENUM(CActionSetGameToken::EValueToSet::FloatValue, "floatValue", "FloatValue")
SERIALIZATION_ENUM(CActionSetGameToken::EValueToSet::IntVariableValue, "intVariableValue", "IntVariableValue")
SERIALIZATION_ENUM(CActionSetGameToken::EValueToSet::FloatVariableValue, "floatVariableValue", "FloatVariableValue")
SERIALIZATION_ENUM(CActionSetGameToken::EValueToSet::BoolVariableValue, "boolVariableValue", "BooleanVariableValue")
SERIALIZATION_ENUM(CActionSetGameToken::EValueToSet::CurrentActorName, "currentActor", "CurrentActor")
SERIALIZATION_ENUM_END()

//--------------------------------------------------------------------------------------------------
void CActionSetGameToken::Serialize(Serialization::IArchive& ar)
{
	ar(m_tokenName, "tokenname", "^TokenName");
	ar(m_bCreateTokenIfNotExisting, "create", "^ Create if not existing");
	ar(m_typeToSet, "typeToSet", "TypeToSet");

	switch (m_typeToSet)
	{
	case CActionSetGameToken::EValueToSet::IntVariableValue:
	case CActionSetGameToken::EValueToSet::FloatVariableValue:
	case CActionSetGameToken::EValueToSet::BoolVariableValue:
		ar(m_collectionName, "collectionName", "CollectionName");
		ar(m_variableName, "variableName", "VariableName");
		break;
	case CActionSetGameToken::EValueToSet::BoolValue:
		ar(m_bBooleanValueToSet, "boolValue", "Value");
		break;
	case CActionSetGameToken::EValueToSet::IntValue:
		ar(m_intValueToSet, "intValue", "Value");
		break;
	case CActionSetGameToken::EValueToSet::FloatValue:
		ar(m_floatValueToSet, "floatValue", "Value");
		break;
	case CActionSetGameToken::EValueToSet::CurrentActorName:
		//we do not need any data for this
		break;
	default:
		ar(m_stringValueToSet, "stringValue", "Value");
		break;
	}

#if !defined(_RELEASE)
	if (ar.isEdit() && ar.isOutput())
	{
		if (!m_tokenName.empty() && (m_tokenName.front() == ' ' || m_tokenName.back() == ' '))
		{
			ar.warning(m_tokenName, "GameToken name starts or ends with a space. Check if this is really wanted.");
		}
		if (!m_stringValueToSet.empty() && (m_stringValueToSet.front() == ' ' || m_stringValueToSet.back() == ' '))
		{
			ar.warning(m_stringValueToSet, "Value starts or ends with a space. Check if this is really wanted.");
		}
		if (IGameToken* pToken = gEnv->pGameFramework->GetIGameTokenSystem()->FindToken(m_tokenName.c_str()))
		{
			if (pToken->GetType() == eFDT_Float && (m_typeToSet != CActionSetGameToken::EValueToSet::FloatVariableValue && m_typeToSet != CActionSetGameToken::EValueToSet::FloatValue))
			{
				ar.warning(m_tokenName, "GameToken is of type float, but the value to set is not.");
			}
			else if (pToken->GetType() == eFDT_Int && (m_typeToSet != CActionSetGameToken::EValueToSet::IntVariableValue && m_typeToSet != CActionSetGameToken::EValueToSet::IntValue))
			{
				ar.warning(m_tokenName, "GameToken is of type integer, but the value to set is not.");
			}
			else if (pToken->GetType() == eFDT_Bool && (m_typeToSet != CActionSetGameToken::EValueToSet::BoolVariableValue && m_typeToSet != CActionSetGameToken::EValueToSet::BoolValue))
			{
				ar.warning(m_tokenName, "GameToken is of type boolean, but the value to set is not.");
			}
			else if (pToken->GetType() == eFDT_String && (m_typeToSet != CActionSetGameToken::EValueToSet::CurrentActorName && m_typeToSet != CActionSetGameToken::EValueToSet::StringValue))
			{
				ar.warning(m_tokenName, "GameToken is of type string, but the value to set is not.");
			}
		}

		if (m_typeToSet == CActionSetGameToken::EValueToSet::IntVariableValue || m_typeToSet == CActionSetGameToken::EValueToSet::FloatVariableValue || m_typeToSet == CActionSetGameToken::EValueToSet::BoolVariableValue)
		{
			CVariableCollection* pCollection = CResponseSystem::GetInstance()->GetVariableCollectionManager()->GetCollection(m_collectionName);
			if (pCollection)
			{
				CryDRS::CVariable* pVariable = pCollection->GetVariable(m_variableName);
				if (pVariable)
				{
					if (pVariable->m_value.GetType() == eDRVT_Float && m_typeToSet != CActionSetGameToken::EValueToSet::FloatVariableValue)
					{
						ar.warning(m_tokenName, "DRS variable is of type float, but the value to set is not.");
					}
					else if (pVariable->m_value.GetType() == eDRVT_Int && m_typeToSet != CActionSetGameToken::EValueToSet::IntVariableValue)
					{
						ar.warning(m_tokenName, "DRS variable is of type integer, but the value to set is not.");
					}
					else if (pVariable->m_value.GetType() == eDRVT_Boolean && m_typeToSet != CActionSetGameToken::EValueToSet::BoolVariableValue)
					{
						ar.warning(m_tokenName, "DRS variable is of type boolean, but the value to set is not.");
					}
					else if (pVariable->m_value.GetType() == eDRVT_String && m_typeToSet != CActionSetGameToken::EValueToSet::StringValue)
					{
						ar.warning(m_tokenName, "DRS variable is of type string, but the value to set is not.");
					}
				}
			}
		}

	}
#endif
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstanceUniquePtr CActionSetGameToken::Execute(DRS::IResponseInstance* pResponseInstance)
{
	IGameTokenSystem* pTokenSystem = gEnv->pGameFramework->GetIGameTokenSystem();
	TFlowInputData dataToSet;

	switch (m_typeToSet)
	{
	case CActionSetGameToken::EValueToSet::IntVariableValue:
	case CActionSetGameToken::EValueToSet::FloatVariableValue:
	case CActionSetGameToken::EValueToSet::BoolVariableValue:
		{
			CVariableCollection* pCollection = CResponseSystem::GetInstance()->GetCollection(m_collectionName, pResponseInstance);
			if (!pCollection)
			{
				pCollection = CResponseSystem::GetInstance()->CreateVariableCollection(m_collectionName);
			}
			if (pCollection)
			{
				if (CryDRS::CVariable* pVariable = pCollection->CreateOrGetVariable(m_variableName))
				{
					if (m_typeToSet == CActionSetGameToken::EValueToSet::IntVariableValue)
						dataToSet.Set(pVariable->GetValueAsInt());
					else if (m_typeToSet == CActionSetGameToken::EValueToSet::BoolVariableValue)
						dataToSet.Set(pVariable->GetValueAsBool());
					else
						dataToSet.Set(pVariable->GetValueAsFloat());
				}
				else
					return nullptr;
			}
			else
				return nullptr;
		}
		break;
	case CActionSetGameToken::EValueToSet::BoolValue:
		dataToSet.Set(m_bBooleanValueToSet);
		break;
	case CActionSetGameToken::EValueToSet::IntValue:
		dataToSet.Set(m_intValueToSet);
		break;
	case CActionSetGameToken::EValueToSet::FloatValue:
		dataToSet.Set(m_floatValueToSet);
		break;
	case CActionSetGameToken::EValueToSet::CurrentActorName:
		dataToSet.Set(pResponseInstance->GetCurrentActor()->GetName());
		break;
	default:
		dataToSet.Set(m_stringValueToSet);
		break;
	}

	if (m_bCreateTokenIfNotExisting)
	{
		pTokenSystem->SetOrCreateToken(m_tokenName.c_str(), dataToSet);
	}
	else if (IGameToken* pToken = pTokenSystem->FindToken(m_tokenName.c_str()))
	{
		pToken->SetValue(dataToSet);
	}
	else
	{
		DrsLogWarning(string().Format("Could not find gametoken '%s'", m_tokenName.c_str()).c_str());
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------
string CryDRS::CActionSetGameToken::GetVerboseInfo() const
{
	switch (m_typeToSet)
	{
	case CActionSetGameToken::EValueToSet::IntVariableValue:
	case CActionSetGameToken::EValueToSet::BoolVariableValue:
	case CActionSetGameToken::EValueToSet::FloatVariableValue:
		return string().Format("Token: '%s' to value from variable '%s::%s'", m_tokenName.c_str(), m_collectionName.GetText().c_str(), m_variableName.GetText().c_str());
	case CActionSetGameToken::EValueToSet::BoolValue:
		return string().Format("Token: '%s' to value '%s'", m_tokenName.c_str(), (m_bBooleanValueToSet) ? "true" : "false");
	case CActionSetGameToken::EValueToSet::IntValue:
		return string().Format("Token: '%s' to value '%i'", m_tokenName.c_str(), m_intValueToSet);
	case CActionSetGameToken::EValueToSet::FloatValue:
		return string().Format("Token: '%s' to value '%f'", m_tokenName.c_str(), m_floatValueToSet);
	case CActionSetGameToken::EValueToSet::CurrentActorName:
		return string().Format("Token: '%s' to the current actor name", m_tokenName.c_str());
	default:
		return string().Format("Token: '%s' to value '%s'", m_tokenName.c_str(), m_stringValueToSet.c_str());
	}
}
