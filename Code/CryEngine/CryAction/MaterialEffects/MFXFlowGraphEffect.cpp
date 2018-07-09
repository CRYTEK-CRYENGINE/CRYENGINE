// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MFXFlowGraphEffect.cpp
//  Version:     v1.00
//  Created:     29/11/2006 by AlexL-Benito GR
//  Compilers:   Visual Studio.NET
//  Description: IMFXEffect implementation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "MFXFlowGraphEffect.h"
#include "MaterialEffects.h"
#include "MaterialFGManager.h"
#include "MaterialEffectsCVars.h"
#include "CryAction.h"

namespace
{
CMaterialFGManager* GetFGManager()
{
	CMaterialEffects* pMFX = static_cast<CMaterialEffects*>(CCryAction::GetCryAction()->GetIMaterialEffects());
	if (pMFX == 0)
		return 0;
	CMaterialFGManager* pMFXFGMgr = pMFX->GetFGManager();
	assert(pMFXFGMgr != 0);
	return pMFXFGMgr;
}
};

CMFXFlowGraphEffect::CMFXFlowGraphEffect()
	: CMFXEffectBase(eMFXPF_Flowgraph)
{
}

CMFXFlowGraphEffect::~CMFXFlowGraphEffect()
{
	CMaterialFGManager* pMFXFGMgr = GetFGManager();
	if (pMFXFGMgr && m_flowGraphParams.fgName.empty() == false)
		pMFXFGMgr->EndFGEffect(m_flowGraphParams.fgName);
}

void CMFXFlowGraphEffect::LoadParamsFromXml(const XmlNodeRef& paramsNode)
{
	// Xml data format
	/*
	   <FlowGraph name="..." maxdist="..." param1="..." param2="..." />
	 */

	m_flowGraphParams.fgName = paramsNode->getAttr("name");
	if (paramsNode->haveAttr("maxdist"))
	{
		paramsNode->getAttr("maxdist", m_flowGraphParams.maxdistSq);
		m_flowGraphParams.maxdistSq *= m_flowGraphParams.maxdistSq;
	}
	paramsNode->getAttr("param1", m_flowGraphParams.params[0]); //MFX custom param 1
	paramsNode->getAttr("param2", m_flowGraphParams.params[1]); //MFX custom param 2
	m_flowGraphParams.params[2] = 1.0f;                         //Intensity (set dynamically from game code)
	m_flowGraphParams.params[3] = 0.0f;                         //Blend out time
}

void CMFXFlowGraphEffect::Execute(const SMFXRunTimeEffectParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (CMaterialEffectsCVars::Get().mfx_EnableFGEffects == 0)
		return;

	const float distToCameraSq = (gEnv->pSystem->GetViewCamera().GetPosition() - params.pos).GetLengthSquared();

	//Check max distance
	if (m_flowGraphParams.maxdistSq == 0.f || distToCameraSq <= m_flowGraphParams.maxdistSq)
	{
		CMaterialFGManager* pMFXFGMgr = GetFGManager();
		pMFXFGMgr->StartFGEffect(m_flowGraphParams, sqrt_tpl(distToCameraSq));
		//if(pMFXFGMgr->StartFGEffect(m_flowGraphParams.fgName))
		//	CryLogAlways("Starting FG HUD effect %s (player distance %f)", m_flowGraphParams.fgName.c_str(),distToPlayer);
	}
}

void CMFXFlowGraphEffect::SetCustomParameter(const char* customParameter, const SMFXCustomParamValue& customParameterValue)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (CMaterialEffectsCVars::Get().mfx_EnableFGEffects == 0)
		return;

	CMaterialFGManager* pMFXFGMgr = GetFGManager();
	pMFXFGMgr->SetFGCustomParameter(m_flowGraphParams, customParameter, customParameterValue);
}

void CMFXFlowGraphEffect::GetResources(SMFXResourceList& resourceList) const
{
	SMFXFlowGraphListNode* listNode = SMFXFlowGraphListNode::Create();
	listNode->m_flowGraphParams.name = m_flowGraphParams.fgName;

	SMFXFlowGraphListNode* next = resourceList.m_flowGraphList;

	if (!next)
		resourceList.m_flowGraphList = listNode;
	else
	{
		while (next->pNext)
			next = next->pNext;

		next->pNext = listNode;
	}
}

void CMFXFlowGraphEffect::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_flowGraphParams.fgName);
}
