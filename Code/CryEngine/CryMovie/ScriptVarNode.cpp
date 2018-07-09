// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptVarNode.h"
#include "AnimTrack.h"

#include <CrySystem/ISystem.h>
#include <CryScriptSystem/IScriptSystem.h>

CAnimScriptVarNode::CAnimScriptVarNode(const int id) : CAnimNode(id)
{
	SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);
	m_value = -1e-20f;
}

void CAnimScriptVarNode::OnReset()
{
	m_value = -1e-20f;
}

void CAnimScriptVarNode::OnResume()
{
	OnReset();
}

void CAnimScriptVarNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_Float);
};

unsigned int CAnimScriptVarNode::GetParamCount() const
{
	return 1;
}

CAnimParamType CAnimScriptVarNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex == 0)
	{
		return eAnimParamType_Float;
	}

	return eAnimParamType_Invalid;
}

bool CAnimScriptVarNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	if (paramId.GetType() == eAnimParamType_Float)
	{
		info.flags = IAnimNode::ESupportedParamFlags(0);
		info.name = "Value";
		info.paramType = eAnimParamType_Float;
		info.valueType = eAnimValue_Float;
		return true;
	}

	return false;
}

void CAnimScriptVarNode::Animate(SAnimContext& animContext)
{
	float floatValue = m_value;

	IAnimTrack* pValueTrack = GetTrackForParameter(eAnimParamType_Float);

	if (pValueTrack)
	{
		if (pValueTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled)
		{
			return;
		}

		TMovieSystemValue value = pValueTrack->GetValue(animContext.time);
		floatValue = stl::get<float>(value);
	}

	if (floatValue != m_value)
	{
		m_value = floatValue;
		// Change console var value.
		SetScriptValue();
	}
}

void CAnimScriptVarNode::SetScriptValue()
{
	IScriptSystem* pScriptSystem = gEnv->pMovieSystem->GetSystem()->GetIScriptSystem();

	if (!pScriptSystem)
	{
		return;
	}

	const char* sVarName = GetName();
	const char* sPnt = strchr(sVarName, '.');

	if (sPnt == 0)
	{
		// Global variable.
		pScriptSystem->SetGlobalValue(sVarName, m_value);
	}
	else
	{
		char sTable[256];
		char sName[256];
		cry_strcpy(sTable, sVarName);
		sTable[sPnt - sVarName] = 0;
		cry_strcpy(sName, sPnt + 1);

		// In Table value.
		SmartScriptTable pTable;

		if (pScriptSystem->GetGlobalValue(sTable, pTable))
		{
			// Set float value inside table.
			pTable->SetValue(sName, m_value);
		}
	}
}
