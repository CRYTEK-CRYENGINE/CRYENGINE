// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/RenderElements/RendElement.h>
#include "XRenderD3D9/DriverD3D.h"


CREOcclusionQuery::~CREOcclusionQuery()
{
	mfReset();
}

void CREOcclusionQuery::mfReset()
{
	D3DOcclusionQuery* pVizQuery = (D3DOcclusionQuery*)m_nOcclusionID;
	SAFE_RELEASE(pVizQuery);

	m_nOcclusionID = 0;
	m_nDrawFrame = 0;
	m_nCheckFrame = 0;
	m_nVisSamples = 0;
	m_bSucceeded = false;
}

uint32 CREOcclusionQuery::m_nQueriesPerFrameCounter = 0;
uint32 CREOcclusionQuery::m_nReadResultNowCounter = 0;
uint32 CREOcclusionQuery::m_nReadResultTryCounter = 0;

// OLD PIPELINE
/*
bool CREOcclusionQuery::mfDraw(CShader* ef, SShaderPass* sfm)
{
	PROFILE_FRAME(CREOcclusionQuery::mfDraw);

	CD3D9Renderer* r = gcpRendD3D;

	gRenDev->m_cEF.mfRefreshSystemShader("OcclusionTest", CShaderMan::s_ShaderOcclTest);

	CShader* pSh = CShaderMan::s_ShaderOcclTest;
	if (!pSh || pSh->m_HWTechniques.empty())
		return false;

	if (!(r->m_Features & RFT_OCCLUSIONTEST))
	{
		// If not supported
		m_nVisSamples = r->GetWidth() * r->GetHeight();
		return true;
	}

	int w = r->GetWidth();
	int h = r->GetHeight();

	if (!m_nOcclusionID)
	{
		D3DOcclusionQuery* pVizQuery = GetDeviceObjectFactory().CreateOcclusionQuery();
		if (pVizQuery)
			m_nOcclusionID = (UINT_PTR)pVizQuery;
	}

	int nFrame = r->GetFrameID();

	if (!m_nDrawFrame) // only allow queries update, if finished already with previous query
	{
		// draw test box
		if (m_nOcclusionID)
		{
			D3DOcclusionQuery* pVizQuery = (D3DOcclusionQuery*)m_nOcclusionID;
			CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
			commandList.GetGraphicsInterface()->BeginOcclusionQuery(pVizQuery);

			const t_arrDeferredMeshIndBuff& arrDeferredInds = r->GetDeferredUnitBoxIndexBuffer();
			const t_arrDeferredMeshVertBuff& arrDeferredVerts = r->GetDeferredUnitBoxVertexBuffer();

			//allocate vertices
			TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(&arrDeferredVerts[0], arrDeferredVerts.size(), 0);

			//allocate indices
			TempDynIB16::CreateFillAndBind(&arrDeferredInds[0], arrDeferredInds.size());

			gRenDev->m_renderThreadInfo[gRenDev->GetRenderThreadID()].m_matView->Push();
			Matrix34 mat;
			mat.SetIdentity();
			mat.SetScale(m_vBoxMax - m_vBoxMin, m_vBoxMin);

			const Matrix44 cTransPosed = Matrix44(mat).GetTransposed();
			gRenDev->m_renderThreadInfo[gRenDev->GetRenderThreadID()].m_matView->MultMatrixLocal(&cTransPosed);
			r->EF_DirtyMatrix();

			uint32 nPasses;
			pSh->FXSetTechnique("General");
			pSh->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			pSh->FXBeginPass(0);

			int nPersFlagsSave = gRenDev->m_renderThreadInfo[gRenDev->GetRenderThreadID()].m_PersFlags;
			uint64 nObjFlagsSave = gRenDev->m_RP.m_ObjFlags;
			CRenderObject* pCurObjectSave = gRenDev->m_RP.m_pCurObject;
			CShader* pShaderSave = gRenDev->m_RP.m_pShader;
			SShaderTechnique* pCurTechniqueSave = gRenDev->m_RP.m_pCurTechnique;

			if (r->FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F) == S_OK)
			{
				gRenDev->m_renderThreadInfo[gRenDev->GetRenderThreadID()].m_PersFlags &= ~RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY;
				gRenDev->m_RP.m_ObjFlags &= ~FOB_TRANS_MASK;
				gRenDev->m_RP.m_pCurObject = gRenDev->m_RP.m_pIdendityRenderObject;
				gRenDev->m_RP.m_pShader = pSh;
				gRenDev->m_RP.m_pCurTechnique = pSh->m_HWTechniques[0];
				r->FX_SetState(GS_NOCOLMASK_RGBA | GS_DEPTHFUNC_LEQUAL);
				r->SetCullMode(R_CULL_NONE);

				r->FX_Commit();

				r->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, arrDeferredVerts.size(), 0, arrDeferredInds.size());
			}

			pSh->FXEndPass();
			pSh->FXEnd();

			gRenDev->m_renderThreadInfo[gRenDev->GetRenderThreadID()].m_matView->Pop();
			gRenDev->m_renderThreadInfo[gRenDev->GetRenderThreadID()].m_PersFlags = nPersFlagsSave;
			gRenDev->m_RP.m_ObjFlags = nObjFlagsSave;
			gRenDev->m_RP.m_pCurObject = pCurObjectSave;
			gRenDev->m_RP.m_pShader = pShaderSave;
			gRenDev->m_RP.m_pCurTechnique = pCurTechniqueSave;

			commandList.GetGraphicsInterface()->EndOcclusionQuery(pVizQuery);

			CREOcclusionQuery::m_nQueriesPerFrameCounter++;
			m_nDrawFrame = 1;
		}

		m_bSucceeded = false;
		// m_nDrawFrame = nFrame;
	}

	return true;
}
*/

void CREOcclusionQuery::mfReadResult_Try(uint32 nDefaultNumSamples)
{
	gRenDev->ExecuteRenderThreadCommand( [=]{ this->RT_ReadResult_Try(nDefaultNumSamples); },
		ERenderCommandFlags::SkipDuringLoading
	);
}

bool CREOcclusionQuery::RT_ReadResult_Try(uint32 nDefaultNumSamples)
{
	PROFILE_FRAME(CREOcclusionQuery::RT_ReadResult_Try);

	int nFrame = gcpRendD3D->GetRenderFrameID();

	D3DOcclusionQuery* pVizQuery = (D3DOcclusionQuery*)m_nOcclusionID;
	if (pVizQuery)
	{
		uint64 samplesPassed;
		HRESULT hRes = GetDeviceObjectFactory().GetOcclusionQueryResults(pVizQuery, samplesPassed) ? S_OK : S_FALSE;

		if (hRes == S_OK)
		{
			m_nVisSamples = (int)samplesPassed;
			m_nDrawFrame = 0;
			m_nCheckFrame = nFrame;
		}
	}

	m_nReadResultTryCounter++;

#ifdef DO_RENDERLOG
	if (!m_nVisSamples)
	{
		if (CRenderer::CV_r_log)
			gRenDev->Logv("OcclusionQuery: Water is not visible\n");
	}
	else
	{
		if (CRenderer::CV_r_log)
			gRenDev->Logv("OcclusionQuery: Water is visible (%d samples)\n", m_nVisSamples);
	}
#endif

	m_bSucceeded = (m_nCheckFrame == nFrame);
	return m_bSucceeded;
}
