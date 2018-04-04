// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "Shadow_Renderer.h"

#include "RenderView.h"

#include "CompiledRenderObject.h"

bool SRendItem::IsListEmpty(int nList)
{
	return gRenDev->m_RP.m_pCurrentRenderView->GetRenderItems(nList).empty();
}

uint32 SRendItem::BatchFlags(int nList)
{
	uint32 nBatchFlags = gRenDev->m_RP.m_pCurrentRenderView->GetBatchFlags(nList);
	return nBatchFlags;
}

CRenderObjectsPools* CPermanentRenderObject::s_pPools;

SPipeStat* SPipeStat::s_pCurrentOutput = nullptr;

//=================================================================

SRenderPipeline::SRenderPipeline()
	: m_pShader(0)
	, m_nShaderTechnique(-1)
	, m_pCurTechnique(NULL)
	, m_CurDownscaleFactor(Vec2(1.0f, 1.0f))
	, m_IndexStreamOffset(~0)
	, m_IndexStreamType(Index16)
	, m_pCurrentRenderView(nullptr)
	, m_ObjectsPool(0)
{
}

SRenderPipeline::~SRenderPipeline()
{
}

//////////////////////////////////////////////////////////////////////////
// Global function used in logging
bool IsRecursiveRenderView()
{
	return (gRenDev->m_RP.m_pCurrentRenderView) ? gRenDev->m_RP.m_pCurrentRenderView->IsRecursive() : false;
}
