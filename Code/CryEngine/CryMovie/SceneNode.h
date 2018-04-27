// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AnimNode.h"

struct ISound;
class CGotoTrack;
class CAnimEntityNode;
class CCameraTrack;

class CAnimSceneNode : public CAnimNode
{
public:
	CAnimSceneNode(const int id);
	~CAnimSceneNode();
	static void            Initialize();

	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_Director; }

	virtual void           Animate(SAnimContext& animContext) override;
	virtual void           CreateDefaultTracks() override;
	virtual void           Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) override;
	virtual void           Activate(bool bActivate) override;

	bool                   GetCameraBoneLinkQuatT(IEntity* pEntity, QuatT& xform, bool bForceAnimationUpdate);

	virtual void           OnReset() override;
	virtual void           OnPause() override;

	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;

	virtual void           PrecacheStatic(SAnimTime startTime) override;
	virtual void           PrecacheDynamic(SAnimTime time) override;

protected:
	virtual bool GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;

private:
	void         ApplyCameraKey(SCameraKey& key, SAnimContext& animContext);
	void         ApplyEventKey(SEventKey& key, SAnimContext& animContext);
	void         ApplyConsoleKey(SConsoleKey& key, SAnimContext& animContext);
	void         ApplySequenceKey(IAnimTrack* pTrack, int nPrevKey, int nCurrKey, SSequenceKey& key, SAnimContext& animContext);

	void         ApplyGotoKey(CGotoTrack* poGotoTrack, SAnimContext& animContext);

	bool         GetEntityTransform(IAnimSequence* pSequence, IEntity* pEntity, SAnimTime time, Vec3& vCamPos, Quat& qCamRot);
	bool         GetEntityTransform(IEntity* pEntity, SAnimTime time, Vec3& vCamPos, Quat& qCamRot);

	virtual void InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType) override;

	// Cached parameters of node at given time.
	SAnimTime        m_time;
	CCameraTrack*    m_pCurrentCameraTrack;
	int              m_currentCameraTrackKeyNumber;
	CAnimEntityNode* m_pCamNodeOnHoldForInterp;
	float            m_backedUpFovForInterp;
	SAnimTime        m_lastPrecachePoint;

	//! Last animated key in track.
	int                     m_lastCameraKey;
	int                     m_lastEventKey;
	int                     m_lastConsoleKey;
	int                     m_lastSequenceKey;
	int                     m_nLastGotoKey;
	int                     m_lastCaptureKey;
	bool                    m_bLastCapturingEnded;
	EntityId                m_currentCameraEntityId;
	ICVar*                  m_cvar_t_FixedStep;
};