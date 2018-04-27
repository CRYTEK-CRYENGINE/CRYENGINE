// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimLightNode.h"
#include <CryScriptSystem/IScriptSystem.h>

namespace AnimLightNode
{
	bool s_nodeParamsInit = false;
	std::vector<CAnimNode::SParamInfo> s_nodeParameters;

	void AddSupportedParam(const char* szName, int paramId, EAnimValue valueType)
	{
		CAnimNode::SParamInfo param;
		param.name = szName;
		param.paramType = paramId;
		param.valueType = valueType;
		s_nodeParameters.push_back(param);
	}
}

CAnimLightNode::CAnimLightNode(const int id)
	: CAnimEntityNode(id),
	m_fRadius(10.0f),
	m_fDiffuseMultiplier(1),
	m_fHDRDynamic(0),
	m_fSpecularMultiplier(1),
	m_clrDiffuseColor(255.0f, 255.0f, 255.0f),
	m_fSpecularPercentage(100.0f),
	m_bJustActivated(false)
{
	CAnimLightNode::Initialize();
}

CAnimLightNode::~CAnimLightNode()
{
}

void CAnimLightNode::Initialize()
{
	if (!AnimLightNode::s_nodeParamsInit)
	{
		AnimLightNode::s_nodeParamsInit = true;
		AnimLightNode::s_nodeParameters.reserve(8);
		AnimLightNode::AddSupportedParam("Position", eAnimParamType_Position, eAnimValue_Vector);
		AnimLightNode::AddSupportedParam("Rotation", eAnimParamType_Rotation, eAnimValue_Quat);
		AnimLightNode::AddSupportedParam("Radius", eAnimParamType_LightRadius, eAnimValue_Float);
		AnimLightNode::AddSupportedParam("DiffuseColor", eAnimParamType_LightDiffuse, eAnimValue_RGB);
		AnimLightNode::AddSupportedParam("DiffuseMultiplier", eAnimParamType_LightDiffuseMult, eAnimValue_Float);
		AnimLightNode::AddSupportedParam("HDRDynamic", eAnimParamType_LightHDRDynamic, eAnimValue_Float);
		AnimLightNode::AddSupportedParam("SpecularMultiplier", eAnimParamType_LightSpecularMult, eAnimValue_Float);
		AnimLightNode::AddSupportedParam("SpecularPercentage", eAnimParamType_LightSpecPercentage, eAnimValue_Float);
		AnimLightNode::AddSupportedParam("Visibility", eAnimParamType_Visibility, eAnimValue_Bool);
		AnimLightNode::AddSupportedParam("Event", eAnimParamType_Event, eAnimValue_Unknown);
	}
}

void CAnimLightNode::Animate(SAnimContext& ec)
{
	CAnimEntityNode::Animate(ec);

	bool bNodeAnimated(false);

	float radius(m_fRadius);
	Vec3 diffuseColor(m_clrDiffuseColor);
	float diffuseMultiplier(m_fDiffuseMultiplier);
	float hdrDynamic(m_fHDRDynamic);
	float specularMultiplier(m_fSpecularMultiplier);
	float specularPercentage(m_fSpecularPercentage);

	GetValueFromTrack(eAnimParamType_LightRadius, ec.time, radius);
	GetValueFromTrack(eAnimParamType_LightDiffuse, ec.time, diffuseColor);
	GetValueFromTrack(eAnimParamType_LightDiffuseMult, ec.time, diffuseMultiplier);
	GetValueFromTrack(eAnimParamType_LightHDRDynamic, ec.time, hdrDynamic);
	GetValueFromTrack(eAnimParamType_LightSpecularMult, ec.time, specularMultiplier);
	GetValueFromTrack(eAnimParamType_LightSpecPercentage, ec.time, specularPercentage);

	if (m_bJustActivated == true)
	{
		bNodeAnimated = true;
		m_bJustActivated = false;
	}

	if (m_fRadius != radius)
	{
		bNodeAnimated = true;
		m_fRadius = radius;
	}

	if (m_clrDiffuseColor != diffuseColor)
	{
		bNodeAnimated = true;
		m_clrDiffuseColor = diffuseColor;
	}

	if (m_fDiffuseMultiplier != diffuseMultiplier)
	{
		bNodeAnimated = true;
		m_fDiffuseMultiplier = diffuseMultiplier;
	}

	if (m_fHDRDynamic != hdrDynamic)
	{
		bNodeAnimated = true;
		m_fHDRDynamic = hdrDynamic;
	}

	if (m_fSpecularMultiplier != specularMultiplier)
	{
		bNodeAnimated = true;
		m_fSpecularMultiplier = specularMultiplier;
	}

	if (m_fSpecularPercentage != specularPercentage)
	{
		bNodeAnimated = true;
		m_fSpecularPercentage = specularPercentage;
	}

	if (bNodeAnimated && m_pOwner)
	{
		m_bIgnoreSetParam = true;
		if (m_pOwner)
			m_pOwner->OnNodeAnimated(this);
		IEntity* pEntity = GetEntity();
		if (pEntity)
		{
			IScriptTable* scriptObject = pEntity->GetScriptTable();
			if (scriptObject)
			{
				scriptObject->SetValue("Radius", m_fRadius);
				scriptObject->SetValue("clrDiffuse", m_clrDiffuseColor);
				scriptObject->SetValue("fDiffuseMultiplier", m_fDiffuseMultiplier);
				scriptObject->SetValue("fSpecularMultiplier", m_fSpecularMultiplier);
				scriptObject->SetValue("fHDRDynamic", m_fHDRDynamic);
				scriptObject->SetValue("fSpecularPercentage", m_fSpecularPercentage);
			}
		}
		m_bIgnoreSetParam = false;
	}
}

bool CAnimLightNode::GetValueFromTrack(CAnimParamType type, float time, float& value) const
{
	IAnimTrack* pTrack = GetTrackForParameter(type);
	if (pTrack == nullptr)
		return false;

	value = stl::get<float>(pTrack->GetValue(time));

	return true;
}

bool CAnimLightNode::GetValueFromTrack(CAnimParamType type, float time, Vec3& value) const
{
	IAnimTrack* pTrack = GetTrackForParameter(type);
	if (pTrack == nullptr)
		return false;

	value = stl::get<Vec3>(pTrack->GetValue(time));

	return true;
}

void CAnimLightNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_Position);
	CreateTrack(eAnimParamType_Rotation);
	CreateTrack(eAnimParamType_LightRadius);
	CreateTrack(eAnimParamType_LightDiffuse);
	CreateTrack(eAnimParamType_Event);
}

void CAnimLightNode::OnReset()
{
	CAnimEntityNode::OnReset();
}

void CAnimLightNode::InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType)
{
	switch (paramType.GetType())
	{
	case eAnimParamType_LightRadius:
		pTrack->SetValue(0, m_fRadius);
		break;

	case eAnimParamType_LightDiffuse:
		pTrack->SetValue(0, m_clrDiffuseColor);
		break;

	case eAnimParamType_LightDiffuseMult:
		pTrack->SetValue(0, m_fDiffuseMultiplier);
		break;

	case eAnimParamType_LightHDRDynamic:
		pTrack->SetValue(0, m_fHDRDynamic);
		break;

	case eAnimParamType_LightSpecularMult:
		pTrack->SetValue(0, m_fSpecularMultiplier);
		break;

	case eAnimParamType_LightSpecPercentage:
		pTrack->SetValue(0, m_fSpecularPercentage);
		break;
	}
}

void CAnimLightNode::Activate(bool bActivate)
{
	CAnimEntityNode::Activate(bActivate);
	m_bJustActivated = bActivate;
}

void CAnimLightNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	CAnimEntityNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);
	if (bLoading)
	{
		xmlNode->getAttr("Radius", m_fRadius);
		xmlNode->getAttr("DiffuseMultiplier", m_fDiffuseMultiplier);
		xmlNode->getAttr("DiffuseColor", m_clrDiffuseColor);
		xmlNode->getAttr("HDRDynamic", m_fHDRDynamic);
		xmlNode->getAttr("SpecularMultiplier", m_fSpecularMultiplier);
		xmlNode->getAttr("SpecularPercentage", m_fSpecularPercentage);
	}
	else
	{
		xmlNode->setAttr("Radius", m_fRadius);
		xmlNode->setAttr("DiffuseMultiplier", m_fDiffuseMultiplier);
		xmlNode->setAttr("DiffuseColor", m_clrDiffuseColor);
		xmlNode->setAttr("HDRDynamic", m_fHDRDynamic);
		xmlNode->setAttr("SpecularMultiplier", m_fSpecularMultiplier);
		xmlNode->setAttr("SpecularPercentage", m_fSpecularPercentage);
	}
}

unsigned int CAnimLightNode::GetParamCount() const
{
	return AnimLightNode::s_nodeParameters.size();
}

CAnimParamType CAnimLightNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < AnimLightNode::s_nodeParameters.size())
	{
		return AnimLightNode::s_nodeParameters[nIndex].paramType;
	}

	return eAnimParamType_Invalid;
}

bool CAnimLightNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (unsigned int i = 0; i < AnimLightNode::s_nodeParameters.size(); ++i)
	{
		if (AnimLightNode::s_nodeParameters[i].paramType == paramId)
		{
			info = AnimLightNode::s_nodeParameters[i];
			return true;
		}
	}
	return false;
}
