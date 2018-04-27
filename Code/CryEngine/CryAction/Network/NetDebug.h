// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network debugging helpers for CryAction code
   -------------------------------------------------------------------------
   History:
   - 01/06/2000   11:28 : Created by Sergii Rustamov
*************************************************************************/
#ifndef __NETDEBUG_H__
#define __NETDEBUG_H__

#pragma once

#if ENABLE_NETDEBUG

class CNetDebug
{
public:
	CNetDebug();
	~CNetDebug();

	void DebugAspectsChange(IEntity* pEntity, uint8 aspects);
	void DebugRMI(const char* szDescription, size_t nSize);
	void Update();

private:
	static const int MAX_ASPECTS = 6;
	static const int MAX_TTL = 600;
	static const int UPDATE_DIFF_MSEC = 1000;

	struct DebugAspectsEntity
	{
		string sEntityName;
		uint32 aspectChangeCount[MAX_ASPECTS];
		float  aspectChangeRate[MAX_ASPECTS];
		uint32 aspectLastCount[MAX_ASPECTS];
		int    entityTTL[MAX_ASPECTS];

		DebugAspectsEntity()
		{
			memset(&aspectChangeCount, 0, sizeof(aspectChangeCount));
			memset(&aspectChangeRate, 0, sizeof(aspectChangeRate));
			memset(&aspectLastCount, 0, sizeof(aspectLastCount));
			memset(&entityTTL, 0, sizeof(entityTTL));
		}
	};
	typedef std::map<string, DebugAspectsEntity> TDebugEntities;
	struct DebugAspectsContext
	{
		string         sEntityClass;
		TDebugEntities entities;
	};
	typedef std::map<string, DebugAspectsContext> TDebugAspects;

private:
	struct DebugRMIEntity
	{
		string sDesc;
		uint32 invokeCount;
		size_t totalSize;
		size_t lastTotalSize;
		float  rate;
		float  peakRate;

		DebugRMIEntity() : invokeCount(0), totalSize(0), lastTotalSize(0), rate(0), peakRate(0) {}
	};
	typedef std::map<string, DebugRMIEntity> TDebugRMI;

private:
	float         m_fLastTime;
	TDebugAspects m_aspects;
	TDebugRMI     m_rmi;
	int           m_varDebugRMI;
	int           m_varDebug;
	int           m_trapValue;
	int           m_trapValueCount;

private:
	void        UpdateAspectsDebugEntity(const DebugAspectsContext& ctx, DebugAspectsEntity& ent, uint8 aspects);
	void        UpdateAspectsRates(float fDiffTimeMsec);
	void        DrawAspects();
	const char* IdxToAspectString(int idx);
	int         AspectToIdx(EEntityAspects aspect);
	bool        ProcessFilter(const char* szClass, const char* szEntity);
	void        ProcessTrap(DebugAspectsEntity& ent);

private:
	void UpdateRMI(float fDiffTimeMSec);
	void DrawRMI();
};

#endif // ENABLE_NETDEBUG
#endif // __NETDEBUG_H__
