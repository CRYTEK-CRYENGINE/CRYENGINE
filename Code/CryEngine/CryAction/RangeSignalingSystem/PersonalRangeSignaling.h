// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   Range.h
   $Id$
   $DateTime$
   Description: Manager per-actor signals to other actors by range
   ---------------------------------------------------------------------
   History:
   - 09:04:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#ifndef _PERSONAL_RANGE_SIGNALING_H_
#define _PERSONAL_RANGE_SIGNALING_H_

#include "AIProxy.h"

class CRangeSignaling;
class CRange;
class CAngleAlert;

class CPersonalRangeSignaling : public IAIProxyListener
{
	typedef std::vector<CRange*>          VecRanges;
	typedef std::vector<CAngleAlert*>     VecAngles;
	typedef std::map<EntityId, VecRanges> MapTargetRanges;

public:
	// Base ----------------------------------------------------------
	CPersonalRangeSignaling(CRangeSignaling* pParent);
	virtual ~CPersonalRangeSignaling();
	bool Init(EntityId Id);
	bool Update(float fElapsedTime, uint32 uDebugOrder = 0);
	void Reset();
	void OnProxyReset();
	void SetEnabled(bool bEnable);
	bool IsEnabled() const { return m_bEnabled; }

	// Utils ---------------------------------------------------------
	bool           AddRangeSignal(float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL);
	bool           AddTargetRangeSignal(EntityId IdTarget, float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL);
	bool           AddAngleSignal(float fAngle, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL);
	EntityId       GetEntityId() const;
	IEntity*       GetEntity();
	IEntity const* GetEntity() const;
	IActor*        GetActor();

private:
	CRange const*       GetRangeTo(const Vec3& vPos, const VecRanges& vecRangeList, bool bUseBoundary = false) const;
	CRange*             SearchRange(const char* sSignal, const VecRanges& vecRangeList) const;
	CAngleAlert const*  GetAngleTo(const Vec3& vPos, bool bUseBoundary = false) const;
	CAngleAlert*        SearchAngle(const char* sSignal) const;
	void                SendSignal(IActor* pActor, const string& sSignal, IAISignalExtraData* pData = NULL) const;
	IAISignalExtraData* PrepareSignalData(IAISignalExtraData* pRequestedData) const;
	void                DebugDraw(uint32 uOrder) const;
	void                CheckActorRanges(IActor* pActor);
	void                CheckActorTargetRanges(IActor* pActor);
	void                CheckActorAngles(IActor* pActor);

	// Returns AI object of an entity
	IAIObject const* GetEntityAI(EntityId entityId) const;

	// Returns AI proxy of an entity
	IAIActorProxy* GetEntityAIProxy(EntityId entityId) const;

	// IAIProxyListener
	void         SetListener(bool bAdd);
	virtual void OnAIProxyEnabled(bool bEnabled);
	// ~IAIProxyListener

private:
	bool             m_bInit;
	bool             m_bEnabled;
	IFFont*          m_pDefaultFont;
	CRangeSignaling* m_pParent;
	EntityId         m_EntityId;

	VecRanges        m_vecRanges;
	VecAngles        m_vecAngles;

	MapTargetRanges  m_mapTargetRanges;

private:
	typedef std::map<EntityId, CRange const*>      MapRangeSignals;
	typedef std::map<EntityId, CAngleAlert const*> MapAngleSignals;

	MapRangeSignals m_mapRangeSignalsSent;
	MapRangeSignals m_mapTargetRangeSignalsSent;
	MapAngleSignals m_mapAngleSignalsSent;
};
#endif // _PERSONAL_RANGE_SIGNALING_H_
