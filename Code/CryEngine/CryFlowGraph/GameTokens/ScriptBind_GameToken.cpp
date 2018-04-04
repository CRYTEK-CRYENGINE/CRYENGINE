// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "ScriptBind_GameToken.h"
#include <CryGame/IGameTokens.h>

namespace
{
// Returns literal representation of the type value
const char* ScriptAnyTypeToString(ScriptAnyType type)
{
	switch (type)
	{
	case ANY_ANY:
		return "Any";
	case ANY_TNIL:
		return "Null";
	case ANY_TBOOLEAN:
		return "Boolean";
	case ANY_TSTRING:
		return "String";
	case ANY_TNUMBER:
		return "Number";
	case ANY_TFUNCTION:
		return "Function";
	case ANY_TTABLE:
		return "Table";
	case ANY_TUSERDATA:
		return "UserData";
	case ANY_THANDLE:
		return "Pointer";
	case ANY_TVECTOR:
		return "Vec3";
	default:
		return "Unknown";
	}
}
};

void CScriptBind_GameToken::Release()
{
	delete this;
};

int CScriptBind_GameToken::SetToken(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	const char* szTokenName = nullptr;
	if (pH->GetParams(szTokenName) == false)
	{
		GameWarning("[GameToken.SetToken] Usage: GameToken.SetToken TokenName TokenValue]");
		return pH->EndFunction();
	}

	ScriptAnyValue val;
	if (pH->GetParamAny(2, val) == false)
	{
		GameWarning("[GameToken.SetToken(%s)] Usage: GameToken.SetToken TokenName TokenValue]", szTokenName);
		return pH->EndFunction();
	}

	TFlowInputData data;
	switch (val.type)
	{
	case ANY_TBOOLEAN:
		{
			bool v;
			val.CopyTo(v);

			data = TFlowInputData::CreateDefaultInitializedForTag(eFDT_Bool, true);
			data.SetValueWithConversion(v);
		}
		break;

	case ANY_TNUMBER:
		{
			float v;
			val.CopyTo(v);

			data = TFlowInputData::CreateDefaultInitializedForTag(eFDT_Float, true);
			data.SetValueWithConversion(v);
		}
		break;

	case ANY_TSTRING:
		{
			string v;
			val.CopyTo(v);

			data = TFlowInputData::CreateDefaultInitializedForTag(eFDT_String, true);
			data.SetValueWithConversion(v);
		}
		break;

	case ANY_TVECTOR:
		{
			Vec3 v;
			val.CopyTo(v);

			data = TFlowInputData::CreateDefaultInitializedForTag(eFDT_Vec3, true);
			data.SetValueWithConversion(v);
		}

	case ANY_TTABLE:
		{
			Vec3 v;
			val.CopyFromTableTo(v);

			data = TFlowInputData::CreateDefaultInitializedForTag(eFDT_Vec3, true);
			data.SetValueWithConversion(v);
		}
		break;

	case ANY_THANDLE:
		{
			ScriptHandle handle;
			val.CopyTo(handle);
			const EntityId v = static_cast<EntityId>(handle.n);

			data = TFlowInputData::CreateDefaultInitializedForTag(eFDT_Int, true);
			data.SetValueWithConversion(v);
		}
		break;

	default:
		GameWarning("[GameToken.SetToken(%s)] Unsupported type '%s']", szTokenName, ScriptAnyTypeToString(val.type));
		return pH->EndFunction();
	}

	m_pTokenSystem->SetOrCreateToken(szTokenName, data);

	return pH->EndFunction();
}

int CScriptBind_GameToken::GetToken(IFunctionHandler* pH, const char* sTokenName)
{
	IGameToken* pToken = m_pTokenSystem->FindToken(sTokenName);
	if (!pToken)
	{
		GameWarning("[GameToken.GetToken] Cannot find token '%s'", sTokenName);
		return pH->EndFunction();
	}

	return pH->EndFunction(pToken->GetValueAsString());
}

int CScriptBind_GameToken::DumpAllTokens(IFunctionHandler* pH)
{
	m_pTokenSystem->DumpAllTokens();
	return pH->EndFunction();
}

void CScriptBind_GameToken::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}
