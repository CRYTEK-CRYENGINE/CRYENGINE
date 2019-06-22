// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "LevelEditor/Tools/EditTool.h"

struct IPhysicalEntity;

class CPhysPullTool : public CEditTool, IEditorNotifyListener
{
public:
	DECLARE_DYNAMIC(CPhysPullTool)

	CPhysPullTool();
	static void RegisterCVars();

	// Overrides from CEditTool
	virtual string GetDisplayName() const override { return "Pull Physics"; }
	virtual void   Display(SDisplayContext& dc) override;
	virtual bool   MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags) override;

	virtual void   OnEditorNotifyEvent(EEditorNotifyEvent ev) 
	{
		switch (ev)
		{
			case eNotify_OnBeginSimulationMode: case eNotify_OnBeginGameMode: 
				ClearRagdollConstraints(); break;
			case eNotify_OnEndUndoRedo: 
				SyncRagdollConstraintsMask(m_pRagdoll);
		}
	}

	void SyncCharacterToRagdoll(IPhysicalEntity *pent);

	static int OnSimFinishedGlobal(const struct EventPhysSimFinished*);
	static int OnPostStepGlobal(const struct EventPhysPostStep*);

	static float cv_HitVel0;
	static float cv_HitVel1;
	static float cv_HitProjMass;
	static float cv_HitProjVel0;
	static float cv_HitProjVel1;
	static float cv_HitExplR;
	static float cv_HitExplPress0;
	static float cv_HitExplPress1;
	static float cv_IKprec;
	static int   cv_IKStiffMode;

protected:
	virtual ~CPhysPullTool();
	void DeleteThis() { delete this; }
	void UpdateAttachPos(const struct SMiniCamera& cam, const CPoint& point);
	void ClearRagdollConstraints();
	void SyncRagdollConstraintsMask(IPhysicalEntity *pent);
	static const int m_idConstr = 2015;

	IPhysicalEntity* m_pEntPull, * m_pEntAttach, * m_pBullet, * m_pRope, * m_pRagdoll;
	CPoint           m_lastMousePos;
	Vec3             m_lastAttachPos, m_locAttachPos;
	int              m_partid;
	float            m_timeMove, m_timeHit, m_timeBullet;
	float            m_attachDist;
	int              m_nAttachPoints;
	uint64           m_maskConstr = 0;
	float            m_sizeConstr = 0.04f;
	bool             m_IKapplied = false;
	HCURSOR          m_hcur[6];

	static CPhysPullTool* g_PhysTool;

	bool IterateRagdollConstraints(std::function<bool(int)> callback)
	{
		uint64 mask = ~m_maskConstr;
		for(int id; mask != ~0ull && !callback(id = ilog2(mask ^ mask+1)); mask |= 1ull << id)
			;
		return mask != ~0ull;
	}
};
