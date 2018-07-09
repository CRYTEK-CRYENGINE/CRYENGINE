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
#include "StdAfx.h"
#include "NetDebug.h"

#if ENABLE_NETDEBUG

	#include <CrySystem/ITextModeConsole.h>

CNetDebug::CNetDebug()
{
	m_fLastTime = 0;
	m_varDebug = 0;
	m_varDebugRMI = 0;
	m_trapValue = 0;
	m_trapValueCount = 1;

	REGISTER_CVAR2("g_debugAspectChanges", &m_varDebug, 0, 0, "Start debugging network aspect changes");
	REGISTER_CVAR2("g_debugRMI", &m_varDebugRMI, 0, 0, "Start debugging RMI network traffic");
	REGISTER_STRING("g_debugAspectFilterClass", "*", 0, "Filter entities per class name");
	REGISTER_STRING("g_debugAspectFilterEntity", "*", 0, "Filter entities per entity name");
	REGISTER_CVAR2("g_debugAspectTrap", &m_trapValue, 0, 0, "Catch aspect change rate greater than trap value");
	REGISTER_CVAR2("g_debugAspectTrapCount", &m_trapValueCount, 1, 0, "How much time trace aspect change rate");
}

CNetDebug::~CNetDebug()
{
	gEnv->pConsole->UnregisterVariable("g_debugAspectChanges");
	gEnv->pConsole->UnregisterVariable("g_debugRMI");
	gEnv->pConsole->UnregisterVariable("g_debugAspectFilterClass");
	gEnv->pConsole->UnregisterVariable("g_debugAspectFilterEntity");
	gEnv->pConsole->UnregisterVariable("g_debugAspectTrap");
	gEnv->pConsole->UnregisterVariable("g_debugAspectTrapCount");
	m_aspects.clear();
}

void CNetDebug::DebugAspectsChange(IEntity* pEntity, uint8 aspects)
{
	if (!m_varDebug || !pEntity)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	const char* sClassName = pEntity->GetClass()->GetName();
	const char* sEntityName = pEntity->GetName();

	TDebugAspects::iterator it = m_aspects.find(CONST_TEMP_STRING(sClassName));
	if (m_aspects.end() == it)
	{
		DebugAspectsContext ctx;
		DebugAspectsEntity ent;

		ctx.sEntityClass = sClassName;
		ent.sEntityName = sEntityName;
		UpdateAspectsDebugEntity(ctx, ent, aspects);
		ctx.entities.insert(std::make_pair(ent.sEntityName, ent));
		m_aspects.insert(std::make_pair(ctx.sEntityClass, ctx));
	}
	else
	{
		DebugAspectsContext* pCtx = &(it->second);

		TDebugEntities::iterator iter = pCtx->entities.find(CONST_TEMP_STRING(sEntityName));
		if (pCtx->entities.end() == iter)
		{
			DebugAspectsEntity ent;
			ent.sEntityName = sEntityName;
			UpdateAspectsDebugEntity(it->second, ent, aspects);
			pCtx->entities.insert(std::make_pair(ent.sEntityName, ent));
		}
		else
		{
			UpdateAspectsDebugEntity(it->second, iter->second, aspects);
		}
	}
}

void CNetDebug::DebugRMI(const char* szDescription, size_t nSize)
{
	if (!m_varDebugRMI)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	TDebugRMI::iterator iter = m_rmi.find(CONST_TEMP_STRING(szDescription));
	if (m_rmi.end() == iter)
	{
		DebugRMIEntity ent;
		ent.sDesc = szDescription;
		ent.invokeCount = 1;
		ent.totalSize = nSize;
		m_rmi.insert(std::make_pair(ent.sDesc, ent));
	}
	else
	{
		DebugRMIEntity* pEnt = &(iter->second);
		pEnt->invokeCount++;
		pEnt->totalSize += nSize;
	}
}

const char* CNetDebug::IdxToAspectString(int idx)
{
	switch (idx)
	{
	case 0:
		return "Script";
	case 1:
		return "Physics";
	case 2:
		return "GameClientStatic";
	case 3:
		return "GameServerStatic";
	case 4:
		return "GameClientDynamic";
	case 5:
		return "GameServerDynamic";
	default:
		return "Unknown";
	}
}

int CNetDebug::AspectToIdx(EEntityAspects aspect)
{
	switch (aspect)
	{
	case eEA_Script:
		return 0;
	case eEA_Physics:
		return 1;
	case eEA_GameClientStatic:
		return 2;
	case eEA_GameServerStatic:
		return 3;
	case eEA_GameClientDynamic:
		return 4;
	case eEA_GameServerDynamic:
		return 5;
	default:
		assert(!"Not supported aspect value");
		return 6;
	}
}

void CNetDebug::UpdateAspectsDebugEntity(const DebugAspectsContext& ctx, DebugAspectsEntity& ent, uint8 aspects)
{
	if (!ProcessFilter(ctx.sEntityClass.c_str(), ent.sEntityName.c_str()))
		return;

	ProcessTrap(ent);

	if (aspects & eEA_Script) ent.aspectChangeCount[AspectToIdx(eEA_Script)]++;
	if (aspects & eEA_Physics) ent.aspectChangeCount[AspectToIdx(eEA_Physics)]++;
	if (aspects & eEA_GameClientStatic) ent.aspectChangeCount[AspectToIdx(eEA_GameClientStatic)]++;
	if (aspects & eEA_GameServerStatic) ent.aspectChangeCount[AspectToIdx(eEA_GameServerStatic)]++;
	if (aspects & eEA_GameClientDynamic) ent.aspectChangeCount[AspectToIdx(eEA_GameClientDynamic)]++;
	if (aspects & eEA_GameServerDynamic) ent.aspectChangeCount[AspectToIdx(eEA_GameServerDynamic)]++;
}

void CNetDebug::UpdateAspectsRates(float fDiffTimeMsec)
{
	for (TDebugAspects::iterator it = m_aspects.begin(); it != m_aspects.end(); ++it)
	{
		DebugAspectsContext* pCtx = &(it->second);

		for (TDebugEntities::iterator iter = pCtx->entities.begin(); iter != pCtx->entities.end(); ++iter)
		{
			DebugAspectsEntity* pEnt = &(iter->second);

			for (int i = 0; i < MAX_ASPECTS; ++i)
			{
				float fCurrRate = (float)pEnt->aspectChangeCount[i] - (float)pEnt->aspectLastCount[i];
				if (fCurrRate > 0)
					pEnt->entityTTL[i] = MAX_TTL;

				float fSec = fDiffTimeMsec / (float)UPDATE_DIFF_MSEC;
				fCurrRate = fCurrRate / fSec;
				pEnt->aspectChangeRate[i] = (pEnt->aspectChangeRate[i] + fCurrRate) / 2.0f;
				pEnt->aspectLastCount[i] = pEnt->aspectChangeCount[i];
			}
		}
	}
}

void CNetDebug::UpdateRMI(float fDiffTimeMSec)
{
	for (TDebugRMI::iterator it = m_rmi.begin(); it != m_rmi.end(); ++it)
	{
		DebugRMIEntity* pEnt = &(it->second);

		float fDiffBytes = (float)pEnt->totalSize - (float)pEnt->lastTotalSize;
		float fSec = fDiffTimeMSec / (float)UPDATE_DIFF_MSEC;
		pEnt->rate = (pEnt->rate + fDiffBytes / fSec) / 2.0f;

		if (pEnt->rate > pEnt->peakRate)
			pEnt->peakRate = pEnt->rate;
		pEnt->lastTotalSize = pEnt->totalSize;
	}
}

void CNetDebug::ProcessTrap(DebugAspectsEntity& ent)
{
	if (0 == m_trapValue)
		return;

	for (int i = 0; i < MAX_ASPECTS; ++i)
	{
		if (ent.aspectChangeRate[i] > m_trapValue)
		{
			gEnv->pSystem->debug_LogCallStack();
			m_trapValueCount--;

			if (m_trapValueCount <= 0)
			{
				m_trapValueCount = 1;
				m_trapValue = 0;
			}
		}
	}
}

bool CNetDebug::ProcessFilter(const char* szClass, const char* szEntity)
{
	const char* szFilterClass = gEnv->pConsole->GetCVar("g_debugAspectFilterClass")->GetString();
	const char* szFilterEntity = gEnv->pConsole->GetCVar("g_debugAspectFilterEntity")->GetString();

	if (*szFilterClass != '*' && strcmp(szClass, szFilterClass) != 0)
		return false;

	if (*szFilterEntity != '*' && strcmp(szEntity, szFilterEntity) != 0)
		return false;

	return true;
}

void CNetDebug::DrawAspects()
{
	float xPos, yPos;
	int row = 0;
	int maxRow = 40;
	float cellSize = 15.0f;
	float color[] = { 0, 1, 0, 1 };

	xPos = cellSize;
	yPos = cellSize * 5;

	for (TDebugAspects::iterator it = m_aspects.begin(); it != m_aspects.end(); ++it)
	{
		DebugAspectsContext* pCtx = &(it->second);

		for (TDebugEntities::iterator iter = pCtx->entities.begin(); iter != pCtx->entities.end(); ++iter)
		{
			DebugAspectsEntity* pEnt = &(iter->second);

			if (!ProcessFilter(pCtx->sEntityClass.c_str(), pEnt->sEntityName.c_str()))
				continue;

			for (int i = 0; i < MAX_ASPECTS; ++i)
			{
				if (pEnt->entityTTL[i] > 0)
				{
					char msg[256];
					ITextModeConsole* pTextModeConsole = gEnv->pSystem->GetITextModeConsole();

					if (yPos > cellSize * maxRow)
					{
						cry_sprintf(msg, "There are more messages to track. Please use filter.");
						color[3] = color[0] = 1;
						IRenderAuxText::Draw2dLabel(xPos, yPos, 1.2f, &color[0], false, "%s", msg);
					}
					else
					{
						cry_sprintf(msg, "Class [%s]: entity [%s] aspect [%s], total [%u], rate [%.2f]",
						            pCtx->sEntityClass.c_str(),
						            pEnt->sEntityName.c_str(),
						            IdxToAspectString(i),
						            pEnt->aspectChangeCount[i],
						            pEnt->aspectChangeRate[i]);

						color[3] = (pEnt->entityTTL[i]) / (float)MAX_TTL;
						IRenderAuxText::Draw2dLabel(xPos, yPos, 1.2f, &color[0], false, "%s", msg);
						if (pTextModeConsole)
							pTextModeConsole->PutText(0, row, msg);

						row++;
						yPos += cellSize;
					}
					if (m_varDebug != 2)
						pEnt->entityTTL[i]--;
				}
			}
		}
	}
}

void CNetDebug::DrawRMI()
{
	float xPos, yPos;
	int row = 0;
	int maxRow = 40;
	float cellSize = 15.0f;
	float color[] = { 0, 1, 0, 1 };

	xPos = cellSize;
	yPos = cellSize * 5;

	for (TDebugRMI::iterator it = m_rmi.begin(); it != m_rmi.end(); ++it)
	{
		DebugRMIEntity* pEnt = &(it->second);

		char msg[256];

		ITextModeConsole* pTextModeConsole = gEnv->pSystem->GetITextModeConsole();

		if (yPos < cellSize * maxRow)
		{
			cry_sprintf(msg, "[%s]: invoke [%u] total [%" PRISIZE_T "], peak [%.2f], rate [%.2f]",
			            pEnt->sDesc.c_str(),
			            pEnt->invokeCount,
			            pEnt->totalSize,
			            pEnt->peakRate,
			            pEnt->rate);

			IRenderAuxText::Draw2dLabel(xPos, yPos, 1.2f, &color[0], false, "%s", msg);
			if (pTextModeConsole)
				pTextModeConsole->PutText(0, row, msg);

			row++;
			yPos += cellSize;
		}
	}
}

void CNetDebug::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!m_varDebug && !m_varDebugRMI)
		return;

	float fFrameTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
	float fDiffTime = fFrameTime - m_fLastTime;

	if (fDiffTime > UPDATE_DIFF_MSEC)
	{
		m_fLastTime = fFrameTime;
		if (m_varDebug) UpdateAspectsRates(fDiffTime);
		if (m_varDebugRMI) UpdateRMI(fDiffTime);
	}
	if (m_varDebug) DrawAspects();
	if (m_varDebugRMI) DrawRMI();
}

#endif // ENABLE_NETDEBUG
