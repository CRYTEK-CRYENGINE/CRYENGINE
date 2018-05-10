// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimEnvironmentNode.h"
#include "AnimSplineTrack.h"
#include <Cry3DEngine/ITimeOfDay.h>

namespace AnimEnvironmentNode
{
bool s_environmentNodeParamsInit = false;
std::vector<CAnimNode::SParamInfo> s_environmentNodeParams;

void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType)
{
	CAnimNode::SParamInfo param;
	param.name = sName;
	param.paramType = paramId;
	param.valueType = valueType;
	s_environmentNodeParams.push_back(param);
}
}

CAnimEnvironmentNode::CAnimEnvironmentNode(const int id) : CAnimNode(id), m_oldSunLongitude(0.0f), m_oldSunLatitude(0.0f)
{
	CAnimEnvironmentNode::Initialize();
	StoreCelestialPositions();
}

void CAnimEnvironmentNode::Initialize()
{
	if (!AnimEnvironmentNode::s_environmentNodeParamsInit)
	{
		AnimEnvironmentNode::s_environmentNodeParamsInit = true;
		AnimEnvironmentNode::s_environmentNodeParams.reserve(4);
		AnimEnvironmentNode::AddSupportedParam("Sun Longitude", eAnimParamType_SunLongitude, eAnimValue_Float);
		AnimEnvironmentNode::AddSupportedParam("Sun Latitude", eAnimParamType_SunLatitude, eAnimValue_Float);
		AnimEnvironmentNode::AddSupportedParam("Moon Longitude", eAnimParamType_MoonLongitude, eAnimValue_Float);
		AnimEnvironmentNode::AddSupportedParam("Moon Latitude", eAnimParamType_MoonLatitude, eAnimValue_Float);
	}
}

void CAnimEnvironmentNode::Animate(SAnimContext& ac)
{
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();

	float sunLongitude = pTimeOfDay->GetSunLongitude();
	float sunLatitude = pTimeOfDay->GetSunLatitude();

	Vec3 v;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
	float& moonLongitude = v.y;
	float& moonLatitude = v.x;

	IAnimTrack* pSunLongitudeTrack = GetTrackForParameter(eAnimParamType_SunLongitude);
	IAnimTrack* pSunLatitudeTrack = GetTrackForParameter(eAnimParamType_SunLatitude);
	IAnimTrack* pMoonLongitudeTrack = GetTrackForParameter(eAnimParamType_MoonLongitude);
	IAnimTrack* pMoonLatitudeTrack = GetTrackForParameter(eAnimParamType_MoonLatitude);

	bool bUpdateSun = false;
	bool bUpdateMoon = false;

	if (pSunLongitudeTrack && pSunLongitudeTrack->GetNumKeys() > 0)
	{
		bUpdateSun = true;
		sunLongitude = stl::get<float>(pSunLongitudeTrack->GetValue(ac.time));
	}

	if (pSunLatitudeTrack && pSunLatitudeTrack->GetNumKeys() > 0)
	{
		bUpdateSun = true;
		sunLatitude = stl::get<float>(pSunLatitudeTrack->GetValue(ac.time));
	}

	if (pMoonLongitudeTrack && pMoonLongitudeTrack->GetNumKeys() > 0)
	{
		bUpdateMoon = true;
		moonLongitude = stl::get<float>(pMoonLongitudeTrack->GetValue(ac.time));
	}

	if (pMoonLatitudeTrack && pMoonLatitudeTrack->GetNumKeys() > 0)
	{
		bUpdateMoon = true;
		moonLatitude = stl::get<float>(pMoonLatitudeTrack->GetValue(ac.time));
	}

	if (bUpdateSun)
	{
		pTimeOfDay->SetSunPos(sunLongitude, sunLatitude);
	}

	if (bUpdateMoon)
	{
		gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
	}

	if (bUpdateSun || bUpdateMoon)
	{
		pTimeOfDay->Update(true, false);
	}
}

void CAnimEnvironmentNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_SunLatitude);
	CreateTrack(eAnimParamType_SunLongitude);
}

void CAnimEnvironmentNode::Activate(bool bActivate)
{
	if (bActivate)
	{
		StoreCelestialPositions();
	}
	else
	{
		RestoreCelestialPositions();
	}
}

unsigned int CAnimEnvironmentNode::GetParamCount() const
{
	return AnimEnvironmentNode::s_environmentNodeParams.size();
}

CAnimParamType CAnimEnvironmentNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < AnimEnvironmentNode::s_environmentNodeParams.size())
	{
		return AnimEnvironmentNode::s_environmentNodeParams[nIndex].paramType;
	}

	return eAnimParamType_Invalid;
}

bool CAnimEnvironmentNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (size_t i = 0; i < AnimEnvironmentNode::s_environmentNodeParams.size(); ++i)
	{
		if (AnimEnvironmentNode::s_environmentNodeParams[i].paramType == paramId)
		{
			info = AnimEnvironmentNode::s_environmentNodeParams[i];
			return true;
		}
	}

	return false;
}

void CAnimEnvironmentNode::InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType)
{
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	const float sunLongitude = pTimeOfDay->GetSunLongitude();
	const float sunLatitude = pTimeOfDay->GetSunLatitude();

	Vec3 v;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
	const float moonLongitude = v.y;
	const float moonLatitude = v.x;

	CAnimSplineTrack* pFloatTrack = static_cast<CAnimSplineTrack*>(pTrack);

	if (pFloatTrack)
	{
		if (paramType == eAnimParamType_SunLongitude)
		{
			pFloatTrack->SetDefaultValue(TMovieSystemValue(sunLongitude));
		}
		else if (paramType == eAnimParamType_SunLatitude)
		{
			pFloatTrack->SetDefaultValue(TMovieSystemValue(sunLatitude));
		}
		else if (paramType == eAnimParamType_MoonLongitude)
		{
			pFloatTrack->SetDefaultValue(TMovieSystemValue(moonLongitude));
		}
		else if (paramType == eAnimParamType_MoonLatitude)
		{
			pFloatTrack->SetDefaultValue(TMovieSystemValue(moonLatitude));
		}
	}
}

void CAnimEnvironmentNode::StoreCelestialPositions()
{
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	m_oldSunLongitude = pTimeOfDay->GetSunLongitude();
	m_oldSunLatitude = pTimeOfDay->GetSunLatitude();

	Vec3 v;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);
	m_oldMoonLongitude = v.y;
	m_oldMoonLatitude = v.x;
}

void CAnimEnvironmentNode::RestoreCelestialPositions()
{
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	pTimeOfDay->SetSunPos(m_oldSunLongitude, m_oldSunLatitude);

	Vec3 v;
	v.y = m_oldMoonLongitude;
	v.x = m_oldMoonLatitude;
	gEnv->p3DEngine->SetGlobalParameter(E3DPARAM_SKY_MOONROTATION, v);

	pTimeOfDay->Update(true, false);
}
