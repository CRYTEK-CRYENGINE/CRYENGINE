// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CVarNode.h"
#include "AnimTrack.h"

#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>

CAnimCVarNode::CAnimCVarNode(const int id) : CAnimNode(id)
{
	SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);
	m_value = -1e-20f; //-1e-28;
}

void CAnimCVarNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_Float);
}

void CAnimCVarNode::OnReset()
{
	m_value = -1e-20f;
}

void CAnimCVarNode::OnResume()
{
	OnReset();
}

unsigned int CAnimCVarNode::GetParamCount() const
{
	return 1;
}

CAnimParamType CAnimCVarNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex == 0)
	{
		return eAnimParamType_Float;
	}

	return eAnimParamType_Invalid;
}

bool CAnimCVarNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
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

void CAnimCVarNode::SetName(const char* name)
{
	// Name of node is used as a name of console var.
	CAnimNode::SetName(name);
	ICVar* pVar = gEnv->pConsole->GetCVar(GetName());

	if (pVar)
	{
		m_value = pVar->GetFVal();
	}
}

void CAnimCVarNode::Animate(SAnimContext& animContext)
{
	if (animContext.bResetting)
	{
		return;
	}

	float floatValue = m_value;

	IAnimTrack* pValueTrack = GetTrackForParameter(eAnimParamType_Float);

	if (!pValueTrack || (pValueTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled))
	{
		return;
	}

	TMovieSystemValue value = pValueTrack->GetValue(animContext.time);
	floatValue = stl::get<float>(value);

	if (floatValue != m_value)
	{
		m_value = floatValue;
		// Change console var value.
		ICVar* pVar = gEnv->pConsole->GetCVar(GetName());

		if (pVar)
		{
			pVar->Set(m_value);
		}
	}
}
