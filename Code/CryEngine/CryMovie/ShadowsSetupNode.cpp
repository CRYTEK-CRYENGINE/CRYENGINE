// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ShadowsSetupNode.h"

namespace ShadowSetupNode
{
bool s_shadowSetupParamsInit = false;
std::vector<CAnimNode::SParamInfo> s_shadowSetupParams;

void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType)
{
	CAnimNode::SParamInfo param;
	param.name = sName;
	param.paramType = paramId;
	param.valueType = valueType;
	s_shadowSetupParams.push_back(param);
}
};

CShadowsSetupNode::CShadowsSetupNode(const int id) : CAnimNode(id)
{
	CShadowsSetupNode::Initialize();
}

void CShadowsSetupNode::Initialize()
{
	if (!ShadowSetupNode::s_shadowSetupParamsInit)
	{
		ShadowSetupNode::s_shadowSetupParamsInit = true;
		ShadowSetupNode::s_shadowSetupParams.reserve(1);
		ShadowSetupNode::AddSupportedParam("GSMCache", eAnimParamType_GSMCache, eAnimValue_Bool);
	}
}

void CShadowsSetupNode::Animate(SAnimContext& animContext)
{
	IAnimTrack* pGsmCache = GetTrackForParameter(eAnimParamType_GSMCache);

	if (pGsmCache && (pGsmCache->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled) == 0)
	{
		TMovieSystemValue value = pGsmCache->GetValue(animContext.time);
		const bool boolValue = stl::get<bool>(value);
		gEnv->p3DEngine->SetShadowsGSMCache(boolValue);
	}
}

void CShadowsSetupNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_GSMCache);
}

void CShadowsSetupNode::OnReset()
{
	gEnv->p3DEngine->SetShadowsGSMCache(false);
}

unsigned int CShadowsSetupNode::GetParamCount() const
{
	return ShadowSetupNode::s_shadowSetupParams.size();
}

CAnimParamType CShadowsSetupNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < (int)ShadowSetupNode::s_shadowSetupParams.size())
	{
		return ShadowSetupNode::s_shadowSetupParams[nIndex].paramType;
	}

	return eAnimParamType_Invalid;
}

bool CShadowsSetupNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (size_t i = 0; i < ShadowSetupNode::s_shadowSetupParams.size(); ++i)
	{
		if (ShadowSetupNode::s_shadowSetupParams[i].paramType == paramId)
		{
			info = ShadowSetupNode::s_shadowSetupParams[i];
			return true;
		}
	}

	return false;
}
