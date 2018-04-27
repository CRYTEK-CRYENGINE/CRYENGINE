// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntityNode.h"

// Node for light animation set sequences
// Normal lights are handled by CEntityNode
class CAnimLightNode : public CAnimEntityNode
{
public:
	CAnimLightNode(const int id);
	~CAnimLightNode();

	static void            Initialize();
	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_Light; }
	virtual void           Animate(SAnimContext& animContext) override;
	virtual void           CreateDefaultTracks() override;
	virtual void           OnReset() override;
	virtual void           Activate(bool bActivate) override;

	virtual void           Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) override;

	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;
	virtual void            InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType) override;

private:
	virtual bool            GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;

private:
	bool GetValueFromTrack(CAnimParamType type, float time, float& value) const;
	bool GetValueFromTrack(CAnimParamType type, float time, Vec3& value) const;

private:
	float m_fRadius;
	float m_fDiffuseMultiplier;
	float m_fHDRDynamic;
	float m_fSpecularMultiplier;
	float m_fSpecularPercentage;
	Vec3 m_clrDiffuseColor;
	bool m_bJustActivated;
};
