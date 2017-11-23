// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"

#include "Common/RenderView.h"
#include "GraphicsPipeline/StandardGraphicsPipeline.h"
#include "GraphicsPipeline/ShadowMap.h"

#pragma warning(push)
#pragma warning(disable: 4244)

#if CRY_PLATFORM_DURANGO
	#pragma warning(push)
	#pragma warning(disable : 4273)
BOOL InflateRect(LPRECT lprc, int dx, int dy);
	#pragma warning(pop)
#endif

bool CD3D9Renderer::FX_DeferredShadowPassSetupBlend(const Matrix44& mShadowTexGen, CRenderView* pRenderView, int nFrustumNum, float maskRTWidth, float maskRTHeight)
{
	//set ScreenToWorld Expansion Basis
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;

	CShadowUtils::ProjectScreenToWorldExpansionBasis(mShadowTexGen, pRenderView->GetCamera(CCamera::eEye_Left), pRenderView->m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, true);

	Matrix44A* mat = &gRenDev->m_TempMatrices[nFrustumNum][2];
	mat->SetRow4(0, vWBasisX);
	mat->SetRow4(1, vWBasisY);
	mat->SetRow4(2, vWBasisZ);
	mat->SetRow4(3, vCamPos);

	return true;
}

bool CD3D9Renderer::FX_DeferredShadowPassSetup(const Matrix44& mShadowTexGen, CRenderView* pRenderView, ShadowMapFrustum* pShadowFrustum, float maskRTWidth, float maskRTHeight, Matrix44& mScreenToShadow, bool bNearest)
{
	//set ScreenToWorld Expansion Basis
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
	bool bVPosSM30 = (GetFeatures() & (RFT_HW_SM30 | RFT_HW_SM40)) != 0;

	CCamera Cam = pRenderView->GetCamera(CCamera::eEye_Left);
	if (bNearest && m_drawNearFov > 1.0f && m_drawNearFov < 179.0f)
		Cam.SetFrustum(Cam.GetViewSurfaceX(), Cam.GetViewSurfaceZ(), DEG2RAD(m_drawNearFov), Cam.GetNearPlane(), Cam.GetFarPlane(), Cam.GetPixelAspectRatio());

	CShadowUtils::ProjectScreenToWorldExpansionBasis(mShadowTexGen, Cam, pRenderView->m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, bVPosSM30);

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
	/*
	if (m_RP.m_nPassGroupID == EFSLIST_TRANSP || m_RP.m_nPassGroupID == EFSLIST_HALFRES_PARTICLES)
	{
		m_cEF.m_TempVecs[13].x = CV_r_ShadowsParticleKernelSize;
		m_cEF.m_TempVecs[13].y = CV_r_ShadowsParticleJitterAmount;
		m_cEF.m_TempVecs[13].z = CV_r_ShadowsParticleAnimJitterAmount * 0.05f;
		m_cEF.m_TempVecs[13].w = CV_r_ShadowsParticleNormalEffect;
	}
	*/

	m_cEF.m_TempVecs[0].x = vCamPos.x;
	m_cEF.m_TempVecs[0].y = vCamPos.y;
	m_cEF.m_TempVecs[0].z = vCamPos.z;
	m_cEF.m_TempVecs[0].w = vCamPos.w;

	if (CV_r_ShadowsScreenSpace)
	{
		CShadowUtils::ProjectScreenToWorldExpansionBasis(Matrix44(IDENTITY), Cam, pRenderView->m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, bVPosSM30);

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
	m_renderQuality.shaderQuality = pSP->GetShaderQuality();
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
#pragma warning(pop)
