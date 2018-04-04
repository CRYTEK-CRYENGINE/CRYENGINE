// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"

#include "Common/RenderView.h"
#include "GraphicsPipeline/StandardGraphicsPipeline.h"
#include "GraphicsPipeline/ShadowMap.h"

#pragma warning(disable: 4244)

#if CRY_PLATFORM_DURANGO
	#pragma warning(push)
	#pragma warning(disable : 4273)
BOOL InflateRect(LPRECT lprc, int dx, int dy);
	#pragma warning(pop)
#endif

bool CD3D9Renderer::FX_DeferredShadowPassSetupBlend(const Matrix44& mShadowTexGen, const CCamera& cam, int nFrustumNum, float maskRTWidth, float maskRTHeight)
{
	//set ScreenToWorld Expansion Basis
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
	CShadowUtils::ProjectScreenToWorldExpansionBasis(mShadowTexGen, cam, m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, true, &m_RenderTileInfo);

	Matrix44A* mat = &gRenDev->m_TempMatrices[nFrustumNum][2];
	mat->SetRow4(0, vWBasisX);
	mat->SetRow4(1, vWBasisY);
	mat->SetRow4(2, vWBasisZ);
	mat->SetRow4(3, vCamPos);

	return true;
}

bool CD3D9Renderer::FX_DeferredShadowPassSetup(const Matrix44& mShadowTexGen, const CCamera& cam, ShadowMapFrustum* pShadowFrustum, float maskRTWidth, float maskRTHeight, Matrix44& mScreenToShadow, bool bNearest)
{
	//set ScreenToWorld Expansion Basis
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
	bool bVPosSM30 = (GetFeatures() & (RFT_HW_SM30 | RFT_HW_SM40)) != 0;

	CCamera Cam = cam;
	if (bNearest && m_drawNearFov > 1.0f && m_drawNearFov < 179.0f)
		Cam.SetFrustum(Cam.GetViewSurfaceX(), Cam.GetViewSurfaceZ(), DEG2RAD(m_drawNearFov), Cam.GetNearPlane(), Cam.GetFarPlane(), Cam.GetPixelAspectRatio());

	CShadowUtils::ProjectScreenToWorldExpansionBasis(mShadowTexGen, Cam, m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, bVPosSM30, &m_RenderTileInfo);

	//TOFIX: create PB components for these params
	//creating common projection matrix for depth reconstruction

	mScreenToShadow = Matrix44(vWBasisX.x, vWBasisX.y, vWBasisX.z, vWBasisX.w,
	                           vWBasisY.x, vWBasisY.y, vWBasisY.z, vWBasisY.w,
	                           vWBasisZ.x, vWBasisZ.y, vWBasisZ.z, vWBasisZ.w,
	                           vCamPos.x, vCamPos.y, vCamPos.z, vCamPos.w);

	//save magnitudes separately to inrease precision
	m_cEF.m_TempVecs[14].x = vWBasisX.GetLength();
	m_cEF.m_TempVecs[14].y = vWBasisY.GetLength();
	m_cEF.m_TempVecs[14].z = vWBasisZ.GetLength();
	m_cEF.m_TempVecs[14].w = 1.0f;

	//Vec4r normalization in doubles
	vWBasisX /= vWBasisX.GetLength();
	vWBasisY /= vWBasisY.GetLength();
	vWBasisZ /= vWBasisZ.GetLength();

	m_cEF.m_TempVecs[10].x = vWBasisX.x;
	m_cEF.m_TempVecs[10].y = vWBasisX.y;
	m_cEF.m_TempVecs[10].z = vWBasisX.z;
	m_cEF.m_TempVecs[10].w = vWBasisX.w;

	m_cEF.m_TempVecs[11].x = vWBasisY.x;
	m_cEF.m_TempVecs[11].y = vWBasisY.y;
	m_cEF.m_TempVecs[11].z = vWBasisY.z;
	m_cEF.m_TempVecs[11].w = vWBasisY.w;

	m_cEF.m_TempVecs[12].x = vWBasisZ.x;
	m_cEF.m_TempVecs[12].y = vWBasisZ.y;
	m_cEF.m_TempVecs[12].z = vWBasisZ.z;
	m_cEF.m_TempVecs[12].w = vWBasisZ.w;

	m_cEF.m_TempVecs[13].x = CV_r_ShadowsAdaptionRangeClamp;
	m_cEF.m_TempVecs[13].y = CV_r_ShadowsAdaptionSize * 250.f;//to prevent akwardy high number in cvar
	m_cEF.m_TempVecs[13].z = CV_r_ShadowsAdaptionMin;

	// Particles shadow constants
	if (m_RP.m_nPassGroupID == EFSLIST_TRANSP || m_RP.m_nPassGroupID == EFSLIST_HALFRES_PARTICLES)
	{
		m_cEF.m_TempVecs[13].x = CV_r_ShadowsParticleKernelSize;
		m_cEF.m_TempVecs[13].y = CV_r_ShadowsParticleJitterAmount;
		m_cEF.m_TempVecs[13].z = CV_r_ShadowsParticleAnimJitterAmount * 0.05f;
		m_cEF.m_TempVecs[13].w = CV_r_ShadowsParticleNormalEffect;
	}

	m_cEF.m_TempVecs[0].x = vCamPos.x;
	m_cEF.m_TempVecs[0].y = vCamPos.y;
	m_cEF.m_TempVecs[0].z = vCamPos.z;
	m_cEF.m_TempVecs[0].w = vCamPos.w;

	if (CV_r_ShadowsScreenSpace)
	{
		CShadowUtils::ProjectScreenToWorldExpansionBasis(Matrix44(IDENTITY), Cam, m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, bVPosSM30, &m_RenderTileInfo);
		gRenDev->m_TempMatrices[0][3].SetRow4(0, vWBasisX);
		gRenDev->m_TempMatrices[0][3].SetRow4(1, vWBasisY);
		gRenDev->m_TempMatrices[0][3].SetRow4(2, vWBasisZ);
		gRenDev->m_TempMatrices[0][3].SetRow4(3, Vec4(0, 0, 0, 1));
	}

	return true;
}

HRESULT GetSampleOffsetsGaussBlur5x5Bilinear(DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, Vec4* avTexCoordOffset, Vec4* avSampleWeight, FLOAT fMultiplier)
{
	float tu = 1.0f / (float)dwD3DTexWidth;
	float tv = 1.0f / (float)dwD3DTexHeight;
	float totalWeight = 0.0f;
	Vec4 vWhite(1.f, 1.f, 1.f, 1.f);
	float fWeights[6];

	int index = 0;
	for (int x = -2; x <= 2; x++, index++)
	{
		fWeights[index] = PostProcessUtils().GaussianDistribution2D((float)x, 0.f, 4);
	}

	//  compute weights for the 2x2 taps.  only 9 bilinear taps are required to sample the entire area.
	index = 0;
	for (int y = -2; y <= 2; y += 2)
	{
		float tScale = (y == 2) ? fWeights[4] : (fWeights[y + 2] + fWeights[y + 3]);
		float tFrac = fWeights[y + 2] / tScale;
		float tOfs = ((float)y + (1.f - tFrac)) * tv;
		for (int x = -2; x <= 2; x += 2, index++)
		{
			float sScale = (x == 2) ? fWeights[4] : (fWeights[x + 2] + fWeights[x + 3]);
			float sFrac = fWeights[x + 2] / sScale;
			float sOfs = ((float)x + (1.f - sFrac)) * tu;
			avTexCoordOffset[index] = Vec4(sOfs, tOfs, 0, 1);
			avSampleWeight[index] = vWhite * sScale * tScale;
			totalWeight += sScale * tScale;
		}
	}

	for (int i = 0; i < index; i++)
	{
		avSampleWeight[i] *= (fMultiplier / totalWeight);
	}

	return S_OK;
}

int CRenderer::FX_ApplyShadowQuality()
{
	SShaderProfile* pSP = &m_cEF.m_ShaderProfiles[eST_Shadow];
	const uint64 quality = g_HWSR_MaskBit[HWSR_QUALITY];
	const uint64 quality1 = g_HWSR_MaskBit[HWSR_QUALITY1];
	m_RP.m_FlagsShader_RT &= ~(quality | quality1);

	int nQuality = (int)pSP->GetShaderQuality();
	m_RP.m_nShaderQuality = nQuality;
	switch (nQuality)
	{
	case eSQ_Medium:
		m_RP.m_FlagsShader_RT |= quality;
		break;
	case eSQ_High:
		m_RP.m_FlagsShader_RT |= quality1;
		break;
	case eSQ_VeryHigh:
		m_RP.m_FlagsShader_RT |= quality;
		m_RP.m_FlagsShader_RT |= quality1;
		break;
	}
	return nQuality;
}

void CD3D9Renderer::FX_StencilTestCurRef(bool bEnable, bool bNoStencilClear, bool bStFuncEqual)
{
	if (bEnable)
	{
		int nStencilState =
		  STENC_FUNC(bStFuncEqual ? FSS_STENCFUNC_EQUAL : FSS_STENCFUNC_NOTEQUAL) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		  STENCOP_PASS(FSS_STENCOP_KEEP);

		FX_SetStencilState(nStencilState, m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFFFFFF);
		FX_SetState(m_RP.m_CurState | GS_STENCIL);
	}
	else
	{
	}
}

bool CD3D9Renderer::CreateAuxiliaryMeshes()
{
	t_arrDeferredMeshIndBuff arrDeferredInds;
	t_arrDeferredMeshVertBuff arrDeferredVerts;

	uint nProjectorMeshStep = 10;

	//projector frustum mesh
	for (int i = 0; i < 3; i++)
	{
		uint nFrustTess = 11 + nProjectorMeshStep * i;
		CDeferredRenderUtils::CreateUnitFrustumMesh(nFrustTess, nFrustTess, arrDeferredInds, arrDeferredVerts);
		SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_PROJECTOR + i]);
		SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_PROJECTOR + i]);
		static_assert(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]), "Invalid type size!");
		CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_PROJECTOR + i], m_pUnitFrustumVB[SHAPE_PROJECTOR + i]);
		m_UnitFrustVBSize[SHAPE_PROJECTOR + i] = arrDeferredVerts.size();
		m_UnitFrustIBSize[SHAPE_PROJECTOR + i] = arrDeferredInds.size();
	}

	//clip-projector frustum mesh
	for (int i = 0; i < 3; i++)
	{
		uint nClipFrustTess = 41 + nProjectorMeshStep * i;
		CDeferredRenderUtils::CreateUnitFrustumMesh(nClipFrustTess, nClipFrustTess, arrDeferredInds, arrDeferredVerts);
		SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_CLIP_PROJECTOR + i]);
		SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_CLIP_PROJECTOR + i]);
		static_assert(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]), "Invalid type size!");
		CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_CLIP_PROJECTOR + i], m_pUnitFrustumVB[SHAPE_CLIP_PROJECTOR + i]);
		m_UnitFrustVBSize[SHAPE_CLIP_PROJECTOR + i] = arrDeferredVerts.size();
		m_UnitFrustIBSize[SHAPE_CLIP_PROJECTOR + i] = arrDeferredInds.size();
	}

	//omni-light mesh
	//Use tess3 for big lights
	CDeferredRenderUtils::CreateUnitSphere(2, arrDeferredInds, arrDeferredVerts);
	SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_SPHERE]);
	SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_SPHERE]);
	static_assert(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]), "Invalid type size!");
	CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_SPHERE], m_pUnitFrustumVB[SHAPE_SPHERE]);
	m_UnitFrustVBSize[SHAPE_SPHERE] = arrDeferredVerts.size();
	m_UnitFrustIBSize[SHAPE_SPHERE] = arrDeferredInds.size();

	//unit box
	CDeferredRenderUtils::CreateUnitBox(arrDeferredInds, arrDeferredVerts);
	SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_BOX]);
	SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_BOX]);
	static_assert(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]), "Invalid type size!");
	CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_BOX], m_pUnitFrustumVB[SHAPE_BOX]);
	m_UnitFrustVBSize[SHAPE_BOX] = arrDeferredVerts.size();
	m_UnitFrustIBSize[SHAPE_BOX] = arrDeferredInds.size();

	//frustum approximated with 8 vertices
	CDeferredRenderUtils::CreateSimpleLightFrustumMesh(arrDeferredInds, arrDeferredVerts);
	SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_SIMPLE_PROJECTOR]);
	SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_SIMPLE_PROJECTOR]);
	static_assert(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]), "Invalid type size!");
	CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_SIMPLE_PROJECTOR], m_pUnitFrustumVB[SHAPE_SIMPLE_PROJECTOR]);
	m_UnitFrustVBSize[SHAPE_SIMPLE_PROJECTOR] = arrDeferredVerts.size();
	m_UnitFrustIBSize[SHAPE_SIMPLE_PROJECTOR] = arrDeferredInds.size();

	// FS quad
	CDeferredRenderUtils::CreateQuad(arrDeferredInds, arrDeferredVerts);
	SAFE_RELEASE(m_pQuadVB);
	D3DIndexBuffer* pDummyQuadIB = 0; // reusing CreateUnitVolumeMesh.
	CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, pDummyQuadIB, m_pQuadVB);
	m_nQuadVBSize = int16(arrDeferredVerts.size());

	return true;
}

bool CD3D9Renderer::ReleaseAuxiliaryMeshes()
{

	for (int i = 0; i < SHAPE_MAX; i++)
	{
		SAFE_RELEASE(m_pUnitFrustumVB[i]);
		SAFE_RELEASE(m_pUnitFrustumIB[i]);
	}

	SAFE_RELEASE(m_pQuadVB);
	m_nQuadVBSize = 0;

	return true;
}

bool CD3D9Renderer::CreateUnitVolumeMesh(t_arrDeferredMeshIndBuff& arrDeferredInds, t_arrDeferredMeshVertBuff& arrDeferredVerts, D3DIndexBuffer*& pUnitFrustumIB, D3DVertexBuffer*& pUnitFrustumVB)
{
	HRESULT hr = S_OK;

	//FIX: try default pools

	if (!arrDeferredVerts.empty())
	{
		hr = GetDeviceObjectFactory().CreateBuffer(arrDeferredVerts.size(), sizeof(SDeferMeshVert), 0, CDeviceObjectFactory::BIND_VERTEX_BUFFER, &pUnitFrustumVB, &arrDeferredVerts[0]);
		assert(SUCCEEDED(hr));

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
		pUnitFrustumVB->SetPrivateData(WKPDID_D3DDebugObjectName, strlen("Unit Frustum"), "Unit Frustum");
#endif
	}

	if (!arrDeferredInds.empty())
	{
		hr = GetDeviceObjectFactory().CreateBuffer(arrDeferredInds.size(), sizeof(arrDeferredInds[0]), 0, CDeviceObjectFactory::BIND_INDEX_BUFFER, &pUnitFrustumIB, &arrDeferredInds[0]);
		assert(SUCCEEDED(hr));

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
		pUnitFrustumVB->SetPrivateData(WKPDID_D3DDebugObjectName, strlen("Unit Frustum"), "Unit Frustum");
#endif
	}

	return SUCCEEDED(hr);
}

void CD3D9Renderer::FX_StencilCullPass(int nStencilID, int nNumVers, int nNumInds)
{
	int newState = m_RP.m_CurState;

	// Set LS colormask
	// debug states
	if (CV_r_DebugLightVolumes /*&& m_RP.m_TI.m_PersFlags2 & d3dRBPF2_ENABLESTENCILPB*/)
	{
		newState &= ~GS_COLMASK_NONE;
		newState &= ~GS_NODEPTHTEST;
		//	newState |= GS_NODEPTHTEST;
		newState |= GS_DEPTHWRITE;

		if (CV_r_DebugLightVolumes > 1)
		{
			newState |= GS_WIREFRAME;
		}
	}
	else
	{
		//	// Disable color writes
		//	if (CV_r_ShadowsStencilPrePass == 2)
		//	{
		//	  newState &= ~GS_COLMASK_NONE;
		//	  newState |= GS_COLMASK_A;
		//	}
		//	else
		{
			newState |= GS_COLMASK_NONE;
		}

		//setup depth test and enable stencil
		newState &= ~(GS_NODEPTHTEST | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK);
		newState |= GS_DEPTHFUNC_LEQUAL | GS_STENCIL;
	}

	//////////////////////////////////////////////////////////////////////////
	// draw back faces - inc when depth fail
	//////////////////////////////////////////////////////////////////////////
	int stencilFunc = FSS_STENCFUNC_ALWAYS;
	uint32 nCurrRef = 0;
	if (nStencilID >= 0)
	{
		D3DSetCull(eCULL_Front);

		//	if (nStencilID > 0)
		stencilFunc = m_RP.m_CurStencilCullFunc;

		FX_SetStencilState(
		  STENC_FUNC(stencilFunc) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		  STENCOP_PASS(FSS_STENCOP_KEEP),
		  nStencilID, 0xFFFFFFFF, 0xFFFF
		  );

		//	uint32 nStencilWriteMask = 1 << nStencilID; //0..7
		//	FX_SetStencilState(
		//		STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
		//		STENCOP_FAIL(FSS_STENCOP_KEEP) |
		//		STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		//		STENCOP_PASS(FSS_STENCOP_KEEP),
		//		0xFF, 0xFFFFFFFF, nStencilWriteMask
		//	);
	}
	else if (nStencilID == -4) // set all pixels with nCurRef within clip volume to nCurRef-1
	{
		stencilFunc = FSS_STENCFUNC_EQUAL;
		nCurrRef = m_nStencilMaskRef;

		FX_SetStencilState(
		  STENC_FUNC(stencilFunc) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_DECR) |
		  STENCOP_PASS(FSS_STENCOP_KEEP),
		  nCurrRef, 0xFFFFFFFF, 0xFFFF
		  );

		m_nStencilMaskRef--;
		D3DSetCull(eCULL_Front);
	}
	else
	{
		if (nStencilID == -3) // TD: Fill stencil by values=1 for drawn volumes in order to avoid overdraw
		{
			stencilFunc = FSS_STENCFUNC_LEQUAL;
			m_nStencilMaskRef--;
		}
		else if (nStencilID == -2)
		{
			stencilFunc = FSS_STENCFUNC_GEQUAL;
			m_nStencilMaskRef--;
		}
		else
		{
			stencilFunc = FSS_STENCFUNC_GEQUAL;
			m_nStencilMaskRef++;
			//	m_nStencilMaskRef = m_nStencilMaskRef % STENC_MAX_REF + m_nStencilMaskRef / STENC_MAX_REF;
			if (m_nStencilMaskRef > STENC_MAX_REF)
			{
				EF_ClearTargetsImmediately(FRT_CLEAR_STENCIL, Clr_Unused.r, 1);
				m_nStencilMaskRef = 2;
			}
		}

		nCurrRef = m_nStencilMaskRef;
		assert(m_nStencilMaskRef > 0 && m_nStencilMaskRef <= STENC_MAX_REF);

		D3DSetCull(eCULL_Front);
		FX_SetStencilState(
		  STENC_FUNC(stencilFunc) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		  STENCOP_PASS(FSS_STENCOP_KEEP),
		  nCurrRef, 0xFFFFFFFF, 0xFFFF
		  );
	}

	FX_SetState(newState);
	FX_Commit();

	// Don't clip pixels beyond far clip plane
	SStateRaster PreviousRS = m_StatesRS[m_nCurStateRS];
	SStateRaster NoDepthClipRS = PreviousRS;
	NoDepthClipRS.Desc.DepthClipEnable = false;

	SetRasterState(&NoDepthClipRS);
	FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, nNumVers, 0, nNumInds);
	SetRasterState(&PreviousRS);

	//////////////////////////////////////////////////////////////////////////
	// draw front faces - decr when depth fail
	//////////////////////////////////////////////////////////////////////////
	if (nStencilID >= 0)
	{
		//	uint32 nStencilWriteMask = 1 << nStencilID; //0..7
		//	D3DSetCull(eCULL_Back);
		//	FX_SetStencilState(
		//		STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
		//		STENCOP_FAIL(FSS_STENCOP_KEEP) |
		//		STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		//		STENCOP_PASS(FSS_STENCOP_KEEP),
		//		0x0, 0xFFFFFFFF, nStencilWriteMask
		//	);
	}
	else
	{
		D3DSetCull(eCULL_Back);
		// TD: deferred meshed should have proper front facing on dx10
		FX_SetStencilState(
		  STENC_FUNC(stencilFunc) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_ZERO) |
		  STENCOP_PASS(FSS_STENCOP_KEEP),
		  m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFF
		  );
	}

	// skip front faces when nStencilID is specified
	if (nStencilID <= 0)
	{
		FX_SetState(newState);
		FX_Commit();

		FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, nNumVers, 0, nNumInds);
	}

	return;
}

void CD3D9Renderer::FX_StencilFrustumCull(int nStencilID, const SRenderLight* pLight, ShadowMapFrustum* pFrustum, int nAxis)
{
	EShapeMeshType nPrimitiveID = m_RP.m_nDeferredPrimitiveID; //SHAPE_PROJECTOR;
	uint32 nPassCount = 0;
	CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;
	static CCryNameTSCRC StencilCullTechName = "DeferredShadowPass";

	float Z = 1.0f;

	Matrix44A mProjection = m_IdentityMatrix;
	Matrix44A mView = m_IdentityMatrix;

	CRY_ASSERT(pLight != NULL);
	bool bAreaLight = (pLight->m_Flags & DLF_AREA_LIGHT) && pLight->m_fAreaWidth && pLight->m_fAreaHeight && pLight->m_fLightFrustumAngle && CRenderer::CV_r_DeferredShadingAreaLights;

	Vec3 vOffsetDir(0, 0, 0);

	//un-projection matrix calc
	if (pFrustum == NULL)
	{
		ITexture* pLightTexture = pLight->GetLightTexture();
		if (pLight->m_fProjectorNearPlane < 0)
		{
			SRenderLight instLight = *pLight;
			vOffsetDir = (-pLight->m_fProjectorNearPlane) * (pLight->m_ObjMatrix.GetColumn0().GetNormalized());
			instLight.SetPosition(instLight.m_Origin - vOffsetDir);
			instLight.m_fRadius -= pLight->m_fProjectorNearPlane;
			CShadowUtils::GetCubemapFrustumForLight(&instLight, nAxis, 160.0f, &mProjection, &mView, false); // 3.0f -  offset to make sure that frustums are intersected
		}
		else if ((pLight->m_Flags & DLF_PROJECT) && pLightTexture && !(pLightTexture->GetFlags() & FT_REPLICATE_TO_ALL_SIDES))
		{
			//projective light
			//refactor projective light

			//for light source
			CShadowUtils::GetCubemapFrustumForLight(pLight, nAxis, pLight->m_fLightFrustumAngle * 2.f /*CShadowUtils::g_fOmniLightFov+3.0f*/, &mProjection, &mView, false); // 3.0f -  offset to make sure that frustums are intersected
		}
		else //omni/area light
		{
			//////////////// light sphere/box processing /////////////////////////////////
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Push();
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Push();

			float fExpensionRadius = pLight->m_fRadius * 1.08f;

			Vec3 vScale(fExpensionRadius, fExpensionRadius, fExpensionRadius);

			Matrix34 mLocal;
			if (bAreaLight)
			{
				mLocal = CShadowUtils::GetAreaLightMatrix(pLight, vScale);
			}
			else if (pLight->m_Flags & DLF_DEFERRED_CUBEMAPS)
			{
				Matrix33 rotMat(pLight->m_ObjMatrix.GetColumn0().GetNormalized() * pLight->m_ProbeExtents.x,
				                pLight->m_ObjMatrix.GetColumn1().GetNormalized() * pLight->m_ProbeExtents.y,
				                pLight->m_ObjMatrix.GetColumn2().GetNormalized() * pLight->m_ProbeExtents.z);
				mLocal = Matrix34::CreateTranslationMat(pLight->m_Origin) * rotMat * Matrix34::CreateScale(Vec3(2, 2, 2), Vec3(-1, -1, -1));
			}
			else
			{
				mLocal.SetIdentity();
				mLocal.SetScale(vScale, pLight->m_Origin);

				mLocal.m00 *= -1.0f;
			}

			Matrix44 mLocalTransposed = mLocal.GetTransposed();
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->LoadMatrix(&mLocalTransposed);
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->LoadIdentity();

			pShader->FXSetTechnique(StencilCullTechName);
			pShader->FXBegin(&nPassCount, FEF_DONTSETSTATES);

			// Vertex/index buffer
			const EShapeMeshType meshType = (bAreaLight || (pLight->m_Flags & DLF_DEFERRED_CUBEMAPS)) ? SHAPE_BOX : SHAPE_SPHERE;

			FX_SetVStream(0, m_pUnitFrustumVB[meshType], 0, sizeof(SVF_P3F_C4B_T2F));
			FX_SetIStream(m_pUnitFrustumIB[meshType], 0, (kUnitObjectIndexSizeof == 2 ? Index16 : Index32));

			//  shader pass
			pShader->FXBeginPass(DS_SHADOW_CULL_PASS);

			if (!FAILED(FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
				FX_StencilCullPass(nStencilID == -4 ? -4 : -1, m_UnitFrustVBSize[meshType], m_UnitFrustIBSize[meshType]);

			pShader->FXEndPass();

			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Pop();
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Pop();

			pShader->FXEnd();

			return;
			//////////////////////////////////////////////////////////////////////////
		}
	}
	else
	{
		if (!pFrustum->bOmniDirectionalShadow)
		{
			//temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
			//pmProjection = &(pFrustum->mLightProjMatrix);
			mProjection = gRenDev->m_IdentityMatrix;
			mView = pFrustum->mLightViewMatrix;
		}
		else
		{
			//calc one of cubemap's frustums
			Matrix33 mRot = (Matrix33(pLight->m_ObjMatrix));
			//rotation for shadow frustums is disabled
			CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, pFrustum, nAxis, &mProjection, &mView /*, &mRot*/);
		}
	}

	//matrix concanation and inversion should be computed in doubles otherwise we have precision problems with big coords on big levels
	//which results to the incident frustum's discontinues for omni-lights
	Matrix44r mViewProj = Matrix44r(mView) * Matrix44r(mProjection);
	Matrix44A mViewProjInv = mViewProj.GetInverted();

	gRenDev->m_TempMatrices[0][0] = mViewProjInv.GetTransposed();

	//setup light source pos/radius
	m_cEF.m_TempVecs[5] = Vec4(pLight->m_Origin, pLight->m_fRadius * 1.1f); //increase radius slightly
	if (pLight->m_fProjectorNearPlane < 0)
	{
		m_cEF.m_TempVecs[5].x -= vOffsetDir.x;
		m_cEF.m_TempVecs[5].y -= vOffsetDir.y;
		m_cEF.m_TempVecs[5].z -= vOffsetDir.z;
		nPrimitiveID = SHAPE_CLIP_PROJECTOR;
	}

	//if (m_pUnitFrustumVB==NULL || m_pUnitFrustumIB==NULL)
	//  CreateUnitFrustumMesh();

	FX_SetVStream(0, m_pUnitFrustumVB[nPrimitiveID], 0, sizeof(SVF_P3F_C4B_T2F));
	FX_SetIStream(m_pUnitFrustumIB[nPrimitiveID], 0, (kUnitObjectIndexSizeof == 2 ? Index16 : Index32));

	pShader->FXSetTechnique(StencilCullTechName);
	pShader->FXBegin(&nPassCount, FEF_DONTSETSTATES);

	//  shader pass
	pShader->FXBeginPass(DS_SHADOW_FRUSTUM_CULL_PASS);

	if (!FAILED(FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
		FX_StencilCullPass(nStencilID, m_UnitFrustVBSize[nPrimitiveID], m_UnitFrustIBSize[nPrimitiveID]);

	pShader->FXEndPass();
	pShader->FXEnd();

	return;
}

void CD3D9Renderer::FX_CreateDeferredQuad(const SRenderLight* pLight, float maskRTWidth, float maskRTHeight, Matrix44A* pmCurView, Matrix44A* pmCurComposite, float fCustomZ)
{
	//////////////////////////////////////////////////////////////////////////
	// Create FS quad
	//////////////////////////////////////////////////////////////////////////

	Vec2 vBoundRectMin(0.0f, 0.0f), vBoundRectMax(1.0f, 1.0f);
	Vec3 vCoords[8];
	Vec3 vRT, vLT, vLB, vRB;

	//calc screen quad
	if (!(pLight->m_Flags & DLF_DIRECTIONAL))
	{
		if (CV_r_ShowLightBounds)
			CShadowUtils::CalcLightBoundRect(pLight, GetRCamera(), *pmCurView, *pmCurComposite, &vBoundRectMin, &vBoundRectMax, gRenDev->GetIRenderAuxGeom());
		else
			CShadowUtils::CalcLightBoundRect(pLight, GetRCamera(), *pmCurView, *pmCurComposite, &vBoundRectMin, &vBoundRectMax, NULL);

		if (m_RenderTileInfo.nGridSizeX > 1.f || m_RenderTileInfo.nGridSizeY > 1.f)
		{

			//always render full-screen quad for hi-res screenshots
			gcpRendD3D->GetRCamera().CalcTileVerts(vCoords,
			                                       m_RenderTileInfo.nGridSizeX - 1 - m_RenderTileInfo.nPosX,
			                                       m_RenderTileInfo.nPosY,
			                                       m_RenderTileInfo.nGridSizeX,
			                                       m_RenderTileInfo.nGridSizeY);
			vBoundRectMin = Vec2(0.0f, 0.0f);
			vBoundRectMax = Vec2(1.0f, 1.0f);

			vRT = vCoords[4] - vCoords[0];
			vLT = vCoords[5] - vCoords[1];
			vLB = vCoords[6] - vCoords[2];
			vRB = vCoords[7] - vCoords[3];

			// Swap order when mirrored culling enabled
			if (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_MIRRORCULL)
			{
				vLT = vCoords[4] - vCoords[0];
				vRT = vCoords[5] - vCoords[1];
				vRB = vCoords[6] - vCoords[2];
				vLB = vCoords[7] - vCoords[3];
			}

		}
		else
		{
			GetRCamera().CalcRegionVerts(vCoords, vBoundRectMin, vBoundRectMax);
			vRT = vCoords[4] - vCoords[0];
			vLT = vCoords[5] - vCoords[1];
			vLB = vCoords[6] - vCoords[2];
			vRB = vCoords[7] - vCoords[3];
		}
	}
	else //full screen case for directional light
	{
		if (m_RenderTileInfo.nGridSizeX > 1.f || m_RenderTileInfo.nGridSizeY > 1.f)
			gcpRendD3D->GetRCamera().CalcTileVerts(vCoords,
			                                       m_RenderTileInfo.nGridSizeX - 1 - m_RenderTileInfo.nPosX,
			                                       m_RenderTileInfo.nPosY,
			                                       m_RenderTileInfo.nGridSizeX,
			                                       m_RenderTileInfo.nGridSizeY);
		else
			gcpRendD3D->GetRCamera().CalcVerts(vCoords);

		vRT = vCoords[4] - vCoords[0];
		vLT = vCoords[5] - vCoords[1];
		vLB = vCoords[6] - vCoords[2];
		vRB = vCoords[7] - vCoords[3];

	}

	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVector(SVF_P3F_T2F_T3F, 4, vQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);

	vQuad[0].p.x = vBoundRectMin.x;
	vQuad[0].p.y = vBoundRectMin.y;
	vQuad[0].p.z = fCustomZ;
	vQuad[0].st0[0] = vBoundRectMin.x;
	vQuad[0].st0[1] = 1 - vBoundRectMin.y;
	vQuad[0].st1 = vLB;

	vQuad[1].p.x = vBoundRectMax.x;
	vQuad[1].p.y = vBoundRectMin.y;
	vQuad[1].p.z = fCustomZ;
	vQuad[1].st0[0] = vBoundRectMax.x;
	vQuad[1].st0[1] = 1 - vBoundRectMin.y;
	vQuad[1].st1 = vRB;

	vQuad[3].p.x = vBoundRectMax.x;
	vQuad[3].p.y = vBoundRectMax.y;
	vQuad[3].p.z = fCustomZ;
	vQuad[3].st0[0] = vBoundRectMax.x;
	vQuad[3].st0[1] = 1 - vBoundRectMax.y;
	vQuad[3].st1 = vRT;

	vQuad[2].p.x = vBoundRectMin.x;
	vQuad[2].p.y = vBoundRectMax.y;
	vQuad[2].p.z = fCustomZ;
	vQuad[2].st0[0] = vBoundRectMin.x;
	vQuad[2].st0[1] = 1 - vBoundRectMax.y;
	vQuad[2].st1 = vLT;

	TempDynVB<SVF_P3F_T2F_T3F>::CreateFillAndBind(vQuad, 4, 0);
}

