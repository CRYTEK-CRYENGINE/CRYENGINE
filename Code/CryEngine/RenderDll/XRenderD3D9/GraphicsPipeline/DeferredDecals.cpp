// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeferredDecals.h"
#include "DriverD3D.h"
#include "../../Common/Textures/TextureHelpers.h"


struct SDecalConstants
{
	Matrix44 matVolumeProj;
	Matrix44 matInvVolumeProj;
	Matrix44 matDecalTS;
	Vec4     textureRect[2];
	Vec4     diffuseCol;
	Vec4     specularCol;
	Vec4     mipLevels;
	Vec4     generalParams;
	Vec4	 opacityParams;
};


struct DecalSortComparison
{
	bool operator()(const SDeferredDecal& decal0, const SDeferredDecal& decal1) const
	{
		uint32 normalMap0 = (decal0.nFlags & DECAL_HAS_NORMAL_MAP);
		uint32 normalMap1 = (decal1.nFlags & DECAL_HAS_NORMAL_MAP);

		if (normalMap0 != normalMap1)
			return normalMap0 < normalMap1;

		return decal0.nSortOrder < decal1.nSortOrder;
	}
};


CDeferredDecalsStage::CDeferredDecalsStage()
{
}

CDeferredDecalsStage::~CDeferredDecalsStage()
{
}

void CDeferredDecalsStage::Init()
{
	// Preallocate 64 decals
	ResizeDecalBuffers(64);
}

void CDeferredDecalsStage::ResizeDecalBuffers(size_t requiredDecalCount)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	const size_t allocatedDecalCount = m_decalPrimitives.size();

	if (allocatedDecalCount < requiredDecalCount)
	{
		m_decalPrimitives.reserve(requiredDecalCount);
		m_decalShaderResources.reserve(requiredDecalCount);

		for (int i = allocatedDecalCount; i < requiredDecalCount; ++i)
		{
			CConstantBufferPtr pCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SDecalConstants));

			m_decalPrimitives.emplace_back();
			m_decalPrimitives.back().SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Pixel | EShaderStage_Vertex);

			m_decalShaderResources.emplace_back();
		}
	}
	else if (allocatedDecalCount > requiredDecalCount && allocatedDecalCount > MaxPersistentDecals)
	{
		const size_t desiredDecalCount = max(requiredDecalCount, size_t(MaxPersistentDecals));

		for (int i = allocatedDecalCount; i > desiredDecalCount; --i)
		{
			m_decalPrimitives.pop_back();
			m_decalShaderResources.pop_back();
		}
	}
}

void CDeferredDecalsStage::SetupDecalPrimitive(const SDeferredDecal& decal, CRenderPrimitive& primitive, _smart_ptr<IRenderShaderResources>& pShaderResources)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	IMaterial* pDecalMaterial = decal.pMaterial;
	if (!pDecalMaterial)
		return;

	SShaderItem& shaderItem = pDecalMaterial->GetShaderItem(0);
	if (!shaderItem.m_pShaderResources)
		return;

	uint64 rtFlags = g_HWSR_MaskBit[HWSR_CUBEMAP0];
	uint32 mdFlags = 0;

	ITexture* pNormalMap = TextureHelpers::LookupTexDefault(EFTT_NORMALS);
	if (SEfResTexture* pNormalRes = shaderItem.m_pShaderResources->GetTexture(EFTT_NORMALS))
	{
		if (pNormalRes->m_Sampler.m_pITex)
		{
			pNormalMap = pNormalRes->m_Sampler.m_pITex;
			rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		}
	}

	ITexture* pSmoothnessMap = TextureHelpers::LookupTexDefault(EFTT_SMOOTHNESS);
	if (SEfResTexture* pSmoothnessRes = shaderItem.m_pShaderResources->GetTexture(EFTT_SMOOTHNESS))
	{
		if (pSmoothnessRes->m_Sampler.m_pITex)
		{
			pSmoothnessMap = pSmoothnessRes->m_Sampler.m_pITex;
			rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE4];
		}
	}

	ITexture* pOpacityMap = TextureHelpers::LookupTexDefault(EFTT_OPACITY);
	if (SEfResTexture* pOpacityRes = shaderItem.m_pShaderResources->GetTexture(EFTT_OPACITY))
	{
		if (pOpacityRes->m_Sampler.m_pITex)
		{
			pOpacityMap = pOpacityRes->m_Sampler.m_pITex;
			rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE5];
		}
	}

	if (decal.fGrowAlphaRef > 0.0f)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	static CCryNameTSCRC techDefDecalVolume = "DeferredDecalVolumeMerged";
	primitive.SetTechnique(CShaderMan::s_shDeferredShading, techDefDecalVolume, rtFlags);
	primitive.SetPrimitiveType(CRenderPrimitive::ePrim_UnitBox);
	primitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, gcpRendD3D->GetGraphicsPipeline().GetMainViewConstantBuffer(), EShaderStage_Pixel | EShaderStage_Vertex);

	ITexture* pDiffuseMap = TextureHelpers::LookupTexDefault(EFTT_DIFFUSE);
	Matrix44 texMatrix(IDENTITY);

	SamplerStateHandle diffuseMapSampler = EDefaultSamplerStates::TrilinearClamp;
	if (SEfResTexture* pDiffuseRes = shaderItem.m_pShaderResources->GetTexture(EFTT_DIFFUSE))
	{
		if (pDiffuseRes->m_Sampler.m_pITex)
		{
			pDiffuseMap = pDiffuseRes->m_Sampler.m_pITex;
			diffuseMapSampler = (pDiffuseRes->m_bUTile || pDiffuseRes->m_bVTile) ? EDefaultSamplerStates::TrilinearWrap : EDefaultSamplerStates::TrilinearClamp;

			if (pDiffuseRes->IsHasModificators())
			{
				pDiffuseRes->UpdateWithModifier(EFTT_MAX, mdFlags);
				SEfTexModificator* mod = pDiffuseRes->m_Ext.m_pTexModifier;
				texMatrix = pDiffuseRes->m_Ext.m_pTexModifier->m_TexMatrix;
			}
		}
	}

	primitive.SetSampler(0, EDefaultSamplerStates::TrilinearWrap);
	primitive.SetSampler(1, diffuseMapSampler);
	primitive.SetSampler(9, EDefaultSamplerStates::PointClamp);  // Used by gbuffer normal encoding

	primitive.SetTexture(0, CRendererResources::s_ptexLinearDepth);
	primitive.SetTexture(1, (CTexture*)CRendererResources::s_ptexBackBuffer);  // Contains copy of scene normals
	primitive.SetTexture(2, (CTexture*)pDiffuseMap);
	primitive.SetTexture(3, (CTexture*)pNormalMap);
	primitive.SetTexture(4, (CTexture*)pSmoothnessMap);
	primitive.SetTexture(5, (CTexture*)pOpacityMap);
	primitive.SetTexture(30, CRendererResources::s_ptexNormalsFitting);

	bool bCameraInVolume;

	// Update constants
	{
		auto& constantManager = primitive.GetConstantManager();
		auto constants = constantManager.BeginTypedConstantUpdate<SDecalConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Pixel | EShaderStage_Vertex);

		SRenderViewInfo viewInfo[2];
		size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfo);

		const Vec3 vBasisX = decal.projMatrix.GetColumn0();
		const Vec3 vBasisY = decal.projMatrix.GetColumn1();
		const Vec3 vBasisZ = decal.projMatrix.GetColumn2();

		Vec3  camFront = viewInfo[0].cameraVZ.normalized();
		Vec3  camPos   = viewInfo[0].cameraOrigin;
		float camFar   = viewInfo[0].farClipPlane;
		float camNear  = viewInfo[0].nearClipPlane;
	
		const float r = fabs(vBasisX.dot(camFront)) + fabs(vBasisY.dot(camFront)) + fabs(vBasisZ.dot(camFront));
		const float s = camFront.dot(decal.projMatrix.GetTranslation() - camPos);
		bCameraInVolume = fabs(s) - camNear <= r;  // OBB-Plane via separating axis test, to check if camera near plane intersects decal volume

		assert(decal.rectTexture.w * decal.rectTexture.h > 0.f);
		constants->textureRect[0] = Vec4(decal.rectTexture.w * texMatrix.m00, decal.rectTexture.h * texMatrix.m10, decal.rectTexture.x * texMatrix.m00 + decal.rectTexture.y * texMatrix.m10 + texMatrix.m30, 0);;
		constants->textureRect[1] = Vec4(decal.rectTexture.w * texMatrix.m01, decal.rectTexture.h * texMatrix.m11, decal.rectTexture.x * texMatrix.m01 + decal.rectTexture.y * texMatrix.m11 + texMatrix.m31, 0);;

		constants->diffuseCol = shaderItem.m_pShaderResources->GetColorValue(EFTT_DIFFUSE).toVec4();
		constants->diffuseCol.w = shaderItem.m_pShaderResources->GetStrengthValue(EFTT_OPACITY) * decal.fAlpha;

		constants->specularCol = shaderItem.m_pShaderResources->GetColorValue(EFTT_SPECULAR).toVec4();
		constants->specularCol.w = shaderItem.m_pShaderResources->GetStrengthValue(EFTT_SMOOTHNESS);

		Vec4 decalParams(1, 1, decal.fGrowAlphaRef, 1);
		Vec4 decalOpacityParams(1, 1, 1, 1);
		DynArrayRef<SShaderParam>& shaderParams = shaderItem.m_pShaderResources->GetParameters();
		for (uint32 i = 0, si = shaderParams.size(); i < si; ++i)
		{
			const char* name = shaderParams[i].m_Name;
			if (strcmp(name, "DecalAlphaMult") == 0)
				decalParams.x = shaderParams[i].m_Value.m_Float;
			else if (strcmp(name, "DecalFalloff") == 0)
				decalParams.y = shaderParams[i].m_Value.m_Float;
			else if (strcmp(name, "DecalAngleBasedFading") == 0 && !decal.fGrowAlphaRef)
				decalParams.z = shaderParams[i].m_Value.m_Float;
			else if (strcmp(name, "DecalDiffuseOpacity") == 0)
				decalOpacityParams.x = shaderParams[i].m_Value.m_Float;
			else if (strcmp(name, "DecalNormalOpacity") == 0)
				decalOpacityParams.y = shaderParams[i].m_Value.m_Float;
			else if (strcmp(name, "DecalSpecularOpacity") == 0)
				decalOpacityParams.z = shaderParams[i].m_Value.m_Float;
		}
		constants->generalParams = decalParams;
		constants->opacityParams = decalOpacityParams;
		
		const float zNear = -0.3f;
		const float zFar = 0.5f;
		const Matrix44A matTextureAndDepth(
			0.5f,  0.0f, 0.0f,                  0.5f,
			0.0f, -0.5f, 0.0f,                  0.5f,
			0.0f,  0.0f, 1.0f / (zNear - zFar), zNear / (zNear - zFar),
			0.0f,  0.0f, 0.0f,                  1.0f);
		
		// Transformation from world coords to decal texture coords
		constants->matVolumeProj = matTextureAndDepth * decal.projMatrix.GetInverted();
		constants->matInvVolumeProj = constants->matVolumeProj.GetInverted();

		const Vec3 vNormX = vBasisX.GetNormalized();
		const Vec3 vNormY = vBasisY.GetNormalized();
		const Vec3 vNormZ = vBasisZ.GetNormalized();

		// Decal normal map to world normal transformation
		const Matrix44A matDecalTS(
			 vNormX.x,  vNormX.y,  vNormX.z, 0.0f,
			-vNormY.x, -vNormY.y, -vNormY.z, 0.0f,
			 vNormZ.x,  vNormZ.y,  vNormZ.z, 0.0f,
			 0.0f ,     0.0f,      0.0f,     1.0f);
		constants->matDecalTS = matDecalTS;

		// Manual mip level computation
		//
		//                 tan(fov) * (textureSize * tiling / decalSize) * distance
		// MipLevel = log2 --------------------------------------------------------
		//                 screenResolution * dot(viewVector, decalNormal)

		const float screenRes = (float)CRendererResources::s_ptexSceneNormalsMap->GetWidth() * 0.5f + (float)CRendererResources::s_ptexSceneNormalsMap->GetHeight() * 0.5f;
		const float decalSize = max(vBasisX.GetLength() * 2.0f, vBasisY.GetLength() * 2.0f);
		const float texScale = max(
			texMatrix.GetColumn(0).GetLength() * decal.rectTexture.w,
			texMatrix.GetColumn(1).GetLength() * decal.rectTexture.h);
		const float mipLevelFactor = (tan(viewInfo[0].pCamera->GetFov()) * texScale / decalSize) / screenRes;

		Vec4 vMipLevels;
		vMipLevels.x = pDiffuseMap ? mipLevelFactor * (float)max(pDiffuseMap->GetWidth(), pDiffuseMap->GetHeight()) : 0.0f;
		vMipLevels.y = pNormalMap ? mipLevelFactor * (float)max(pNormalMap->GetWidth(), pNormalMap->GetHeight()) : 0.0f;
		vMipLevels.z = pSmoothnessMap ? mipLevelFactor * (float)max(pSmoothnessMap->GetWidth(), pSmoothnessMap->GetHeight()) : 0.0f;
		vMipLevels.w = pOpacityMap ? mipLevelFactor * (float)max(pOpacityMap->GetWidth(), pOpacityMap->GetHeight()) : 0.0f;
		constants->mipLevels = vMipLevels;

		constantManager.EndTypedConstantUpdate(constants);
	}

	ECull cullMode = eCULL_Back;
	int renderState = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
	renderState |= GS_DEPTHFUNC_GREAT | GS_STENCIL | GS_NOCOLMASK_GBUFFER_OVERLAY;

	if (bCameraInVolume)
	{
		renderState &= ~(GS_NODEPTHTEST | GS_DEPTHFUNC_MASK);
		renderState |= GS_DEPTHFUNC_LEQUAL;
		cullMode = eCULL_Front;
	}

	primitive.SetRenderState(renderState);
	primitive.SetCullMode(cullMode);

	primitive.SetStencilState(
		STENC_FUNC(FSS_STENCFUNC_EQUAL) |
		STENCOP_FAIL(FSS_STENCOP_KEEP)  |
		STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		STENCOP_PASS(FSS_STENCOP_KEEP),
		BIT_STENCIL_RESERVED, BIT_STENCIL_RESERVED, 0xFF);

	primitive.Compile(m_decalPass);
	m_decalPass.AddPrimitive(&primitive);

	pShaderResources = shaderItem.m_pShaderResources;
}


void CDeferredDecalsStage::Execute()
{
	if (!CRenderer::CV_r_deferredDecals)
		return;

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	auto& deferredDecals = RenderView()->GetDeferredDecals();

	ResizeDecalBuffers(deferredDecals.size());

#if !CRY_PLATFORM_ORBIS
	// Want the buffer cleared or we'll just get black out
	if (deferredDecals.empty())
#endif
		return;

	PROFILE_LABEL_SCOPE("DEFERRED_DECALS");

	// Create temporary copy to enable reads from normal target
	GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(CRendererResources::s_ptexSceneNormalsMap->GetDevTexture(), CRendererResources::s_ptexBackBuffer->GetDevTexture());
	
	// Sort decals
	std::stable_sort(deferredDecals.begin(), deferredDecals.end(), DecalSortComparison());

	CTexture* pSceneSpecular = CRendererResources::s_ptexSceneSpecular;
#if defined(DURANGO_USE_ESRAM)
	pSceneSpecular = CRendererResources::s_ptexSceneSpecularESRAM;
#endif

	m_decalPass.SetRenderTarget(0, CRendererResources::s_ptexSceneNormalsMap);
	m_decalPass.SetRenderTarget(1, CRendererResources::s_ptexSceneDiffuse);
	m_decalPass.SetRenderTarget(2, pSceneSpecular);
	m_decalPass.SetDepthTarget(RenderView()->GetDepthTarget());
	
	m_decalPass.SetViewport(RenderView()->GetViewport());
		
	m_decalPass.BeginAddingPrimitives();
		
	for (uint32 i = 0, count = deferredDecals.size(); i < count; i++)
	{
		SetupDecalPrimitive(deferredDecals[i], m_decalPrimitives[i], m_decalShaderResources[i]);
	}

	m_decalPass.Execute();
}
