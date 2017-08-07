// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TiledShading.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "D3D_SVO.h"


struct STiledLightVolumeInfo
{
	Matrix44 worldMat;
	Vec4     volumeTypeInfo;
	Vec4     volumeParams0;
	Vec4     volumeParams1;
	Vec4     volumeParams2;
	Vec4     volumeParams3;
};

struct VolumeLightListGenConstants
{
	Matrix44 matViewProj;
	uint32   lightIndexOffset, numVertices;
	Vec4     screenScale;
	Vec4     viewerPos;
	Vec4     worldBasisX;
	Vec4     worldBasisY;
	Vec4     worldBasisZ;
};

CTiledShadingStage::CTiledShadingStage()
	: m_passCullingShading(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader)
{
	static_assert(sizeof(STiledLightVolumeInfo) % 16 == 0, "STiledLightVolumeInfo should be 16 byte aligned for GPU performance");
	static_assert(MaxNumTileLights <= 256 && LightTileSizeX == 8 && LightTileSizeY == 8, "Volumes don't support other settings");
}

CTiledShadingStage::~CTiledShadingStage()
{
	for (uint32 i = 0; i < eVolumeType_Count; i++)
	{
		gRenDev->m_DevBufMan.Destroy(m_volumeMeshes[i].vertexBuffer);
		gRenDev->m_DevBufMan.Destroy(m_volumeMeshes[i].indexBuffer);
	}
}


void CTiledShadingStage::Init()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	
	m_lightVolumeInfoBuf.Create(MaxNumTileLights, sizeof(STiledLightVolumeInfo), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
	
	// Create geometry for light volumes
	{
		t_arrDeferredMeshVertBuff vertices;
		t_arrDeferredMeshIndBuff indices;
		Vec4 vertexData[256];
		
		for (uint32 i = 0; i < eVolumeType_Count; i++)
		{
			switch (i)
			{
			case eVolumeType_Box:
				CDeferredRenderUtils::CreateUnitBox(indices, vertices);
				for (uint32 i = 0; i < vertices.size(); i++)
				{
					vertices[i].xyz = vertices[i].xyz * 2.0f - Vec3(1.0f, 1.0f, 1.0f);  // Centered box
				}
				break;
			case eVolumeType_Sphere:
				CDeferredRenderUtils::CreateUnitSphere(0, indices, vertices);
				break;
			case eVolumeType_Cone:
				CDeferredRenderUtils::CreateUnitSphere(0, indices, vertices);  // Currently we use a sphere proxy for cones
				break;
			}

			assert(vertices.size() < CRY_ARRAY_COUNT(vertexData));
			
			SVolumeGeometry& volumeMesh = m_volumeMeshes[i];

			volumeMesh.numIndices = indices.size();
			volumeMesh.numVertices = vertices.size();
			indices.resize(volumeMesh.numIndices * 256);
			for (uint32 i = 0; i < 256; i++)
			{
				for (uint32 j = 0; j < volumeMesh.numIndices; j++)
				{
					indices[i * volumeMesh.numIndices + j] = indices[j] + i * volumeMesh.numVertices;
				}
			}

			volumeMesh.indexBuffer = pRenderer->m_DevBufMan.Create(BBT_INDEX_BUFFER, BU_STATIC, volumeMesh.numIndices * sizeof(uint16) * 256);
			pRenderer->m_DevBufMan.UpdateBuffer(volumeMesh.indexBuffer, indices);
			volumeMesh.vertexBuffer = pRenderer->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, volumeMesh.numVertices * sizeof(vertices[0]));
			pRenderer->m_DevBufMan.UpdateBuffer(volumeMesh.vertexBuffer, vertices);

			for (uint32 i = 0; i < volumeMesh.numVertices; i++)
			{
				vertexData[i] = Vec4(vertices[i].xyz, 1);
			}
			volumeMesh.vertexDataBuf.Create(volumeMesh.numVertices, sizeof(Vec4), DXGI_FORMAT_R32G32B32A32_FLOAT, CDeviceObjectFactory::BIND_SHADER_RESOURCE, vertexData);
		}
	}

	for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_volumePasses); i++)
	{
		m_volumePasses[i].AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerBatch, sizeof(VolumeLightListGenConstants), EShaderStage_Vertex | EShaderStage_Pixel);
	}

	m_pPerViewConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerViewGlobalConstantBuffer));
}


void CTiledShadingStage::PrepareLightVolumeInfo()
{
	const uint32 kNumPasses = eVolumeType_Count * 2;
	uint16 volumeLightIndices[kNumPasses][MaxNumTileLights];
	
	for (uint32 i = 0; i < kNumPasses; i++)
		m_numVolumesPerPass[i] = 0;

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CTiledShading* pTiledShading = &pRenderer->GetTiledShading();
	STiledLightInfo* pLightInfo = pTiledShading->GetTiledLightInfo();

	Vec3 camPos = pRenderer->GetRCamera().vOrigin;
	
	float tileCoverageX = (float)LightTileSizeX / (float)pRenderer->GetWidth();
	float tileCoverageY = (float)LightTileSizeY / (float)pRenderer->GetHeight();
	float camFovScaleX = 0.5f * pRenderer->m_ProjMatrix.m00;
	float camFovScaleY = 0.5f * pRenderer->m_ProjMatrix.m11;

	// Classify volume type
	uint32 numLights = pTiledShading->GetValidLightCount();
	for (uint32 lightIdx = 0; lightIdx < numLights; ++lightIdx)
	{
		STiledLightInfo& lightInfo = pLightInfo[lightIdx];

		bool bInsideVolume = false;
		Vec3 lightPos = Vec3(lightInfo.posRad);
		if (lightPos.GetDistance(camPos) < lightInfo.posRad.w * 1.25f)
		{
			bInsideVolume = true;
		}

		uint32 pass = eVolumeType_Sphere;
		if (lightInfo.volumeType == CTiledShading::tlVolumeOBB)
			pass = eVolumeType_Box;
		else if (lightInfo.volumeType == CTiledShading::tlVolumeCone)
			pass = eVolumeType_Cone;

		pass += bInsideVolume ? 0 : eVolumeType_Count;
		volumeLightIndices[pass][m_numVolumesPerPass[pass]++] = lightIdx;
	}

	// Prepare buffer with light volume info
	uint32 curIndex = 0;
	STiledLightVolumeInfo volumeInfo[MaxNumTileLights];
	for (uint32 i = 0; i < kNumPasses; i++)
	{
		bool bInsideVolume = i < eVolumeType_Count; 
		
		for (uint32 j = 0; j < m_numVolumesPerPass[i]; j++)
		{	
			STiledLightInfo& lightInfo = pLightInfo[volumeLightIndices[i][j]];

			// To ensure conservative rasterization, we need to enlarge the projected volume by at least half a tile
			// Compute the size of a tile in view space at the position of the volume
			//     ScreenCoverage = FovScale * ObjectSize / Distance  =>  ObjectSize = ScreenCoverage * Distance / FovScale
			float distance = lightInfo.depthBoundsVS.y * pRenderer->GetRCamera().fFar;
			float tileSizeVS = sqrtf(sqr(tileCoverageX * distance / camFovScaleX) + sqr(tileCoverageY * distance / camFovScaleY));
			
			Vec3 lightPos = Vec3(lightInfo.posRad);
			
			Matrix44 worldMat;
			Vec4 volumeParams0(0, 0, 0, 0);
			Vec4 volumeParams1(0, 0, 0, 0);
			Vec4 volumeParams2(0, 0, 0, 0);
			Vec4 volumeParams3(0, 0, 0, 0);

			volumeInfo[curIndex].volumeTypeInfo = Vec4((float)lightInfo.volumeType, 0, 0, (float)volumeLightIndices[i][j]);

			if (lightInfo.volumeType == CTiledShading::tlVolumeOBB)
			{
				float minBoxSize = std::min(std::min(lightInfo.volumeParams0.w, lightInfo.volumeParams1.w), lightInfo.volumeParams2.w);
				float volumeScale = (minBoxSize + tileSizeVS) / minBoxSize;

				volumeParams0 = Vec4(Vec3(lightInfo.volumeParams0), lightInfo.volumeParams0.w * volumeScale);
				volumeParams1 = Vec4(Vec3(lightInfo.volumeParams1), lightInfo.volumeParams1.w * volumeScale);
				volumeParams2 = Vec4(Vec3(lightInfo.volumeParams2), lightInfo.volumeParams2.w * volumeScale);
				volumeParams3 = Vec4(volumeParams0.w, volumeParams1.w, volumeParams2.w, 0);
				
				Matrix33 rotScaleMat(Vec3(volumeParams0) * volumeParams0.w, Vec3(volumeParams1) * volumeParams1.w, Vec3(volumeParams2) * volumeParams2.w);
				Matrix34 unitVolumeToWorldMat = Matrix34::CreateTranslationMat(lightPos) * rotScaleMat;
				worldMat = unitVolumeToWorldMat.GetTransposed();

				volumeParams0 = Vec4(Vec3(volumeParams0), lightPos.x);
				volumeParams1 = Vec4(Vec3(volumeParams1), lightPos.y);
				volumeParams2 = Vec4(Vec3(volumeParams2), lightPos.z);
			}
			else if (lightInfo.volumeType == CTiledShading::tlVolumeSun)
			{
				volumeParams0 = Vec4(camPos, pRenderer->GetRCamera().fFar - 1.0f);
				
				Matrix34 unitVolumeToWorldMat = Matrix34::CreateScale(Vec3(volumeParams0.w), camPos);
				worldMat = unitVolumeToWorldMat.GetTransposed();
			}
			else if (lightInfo.volumeType == CTiledShading::tlVolumeCone)
			{
				float coneAngle = std::min(lightInfo.volumeParams1.w + 0.02f, PI / 2.0f);
				volumeInfo[curIndex].volumeTypeInfo.y = sinf(coneAngle);
				volumeInfo[curIndex].volumeTypeInfo.z = cosf(coneAngle);

				volumeParams1 = Vec4(Vec3(lightInfo.volumeParams0), cosf(coneAngle));

				Vec3 coneTip = lightPos + Vec3(volumeParams1) * tileSizeVS;
				float volumeScale = (lightInfo.posRad.w + tileSizeVS * 2.0f) / lightInfo.posRad.w;
				volumeParams0 = Vec4(coneTip, lightInfo.posRad.w * volumeScale);
				
				float volumeScaleLowRes = (lightInfo.posRad.w * 1.2f + (tileSizeVS * 2.0f)) / lightInfo.posRad.w;
				Matrix34 unitVolumeToWorldMat = Matrix34::CreateScale(Vec3(lightInfo.posRad.w * volumeScaleLowRes), coneTip);
				worldMat = unitVolumeToWorldMat.GetTransposed();
			}
			else  // CTiledShading::tlVolumeSphere
			{
				float volumeScale = (lightInfo.posRad.w + tileSizeVS) / lightInfo.posRad.w;
				volumeParams0 = Vec4(lightPos, lightInfo.posRad.w * volumeScale);
				
				float volumeScaleLowRes = (lightInfo.posRad.w * 1.2f + tileSizeVS) / lightInfo.posRad.w;
				Matrix34 unitVolumeToWorldMat = Matrix34::CreateScale(Vec3(lightInfo.posRad.w * volumeScaleLowRes), lightPos);
				worldMat = unitVolumeToWorldMat.GetTransposed();
			}

			volumeInfo[curIndex].worldMat = worldMat;
			volumeInfo[curIndex].volumeParams0 = volumeParams0;
			volumeInfo[curIndex].volumeParams1 = volumeParams1;
			volumeInfo[curIndex].volumeParams2 = volumeParams2;
			volumeInfo[curIndex].volumeParams3 = volumeParams3;
			curIndex += 1;
		}
	}

	m_lightVolumeInfoBuf.UpdateBufferContent(volumeInfo, sizeof(volumeInfo));
}


void CTiledShadingStage::PrepareResources()
{
	if (CRenderer::CV_r_DeferredShadingTiled >= 3)
	{
		uint clearNull[4] = { 0 };
		GetDeviceObjectFactory().GetCoreCommandList().GetComputeInterface()->ClearUAV(gcpRendD3D->GetTiledShading().m_tileOpaqueLightMaskBuf.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), clearNull, 0, nullptr);
		GetDeviceObjectFactory().GetCoreCommandList().GetComputeInterface()->ClearUAV(gcpRendD3D->GetTiledShading().m_tileTranspLightMaskBuf.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), clearNull, 0, nullptr);
	}
}


bool CTiledShadingStage::ExecuteVolumeListGen(uint32 dispatchSizeX, uint32 dispatchSizeY)
{
	if (CRenderer::CV_r_DeferredShadingTiled < 3 && !CRenderer::CV_r_GraphicsPipelineMobile)
		return false;
	
	CD3D9Renderer* pRenderer = gcpRendD3D;
	CTiledShading* pTiledShading = &pRenderer->GetTiledShading();

	CTexture* pDepthRT = CTexture::s_ptexDepthBufferHalfQuarter;
	assert(CRendererCVars::CV_r_VrProjectionType > 0 || (pDepthRT->GetWidth() == dispatchSizeX && pDepthRT->GetHeight() == dispatchSizeY));

	{
		PROFILE_LABEL_SCOPE("COPY_DEPTH");

		static CCryNameTSCRC techCopy("CopyToDeviceDepth");
		m_passCopyDepth.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passCopyDepth.SetTechnique(CShaderMan::s_shPostEffects, techCopy, 0);
		m_passCopyDepth.SetRequirePerViewConstantBuffer(true);
		m_passCopyDepth.SetDepthTarget(pDepthRT);
		m_passCopyDepth.SetState(GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);
		m_passCopyDepth.SetTexture(0, CTexture::s_ptexZTargetScaled3);
		
		m_passCopyDepth.BeginConstantUpdate();
		m_passCopyDepth.Execute();
	}

	PROFILE_LABEL_SCOPE("TILED_VOLUMES");

	PrepareLightVolumeInfo();
	
	CStandardGraphicsPipeline::SViewInfo viewInfo[2];
	int viewInfoCount = pRenderer->GetGraphicsPipeline().GetViewInfo(viewInfo);
	
	Matrix44 matViewProj[2];
	Vec4 worldBasisX[2], worldBasisY[2], worldBasisZ[2], viewerPos[2];
	for (int i = 0; i < viewInfoCount; ++i)
	{
		matViewProj[i] = viewInfo[i].viewMatrix * viewInfo[i].projMatrix;

		Vec4r wBasisX, wBasisY, wBasisZ, camPos;
		CShadowUtils::ProjectScreenToWorldExpansionBasis(
			Matrix44(IDENTITY), *viewInfo[i].pCamera, pRenderer->m_vProjMatrixSubPixoffset,
			(float)pDepthRT->GetWidth(), (float)pDepthRT->GetHeight(), wBasisX, wBasisY, wBasisZ, camPos, true, NULL);

		worldBasisX[i] = wBasisX;
		worldBasisY[i] = wBasisY;
		worldBasisZ[i] = wBasisZ;
		viewerPos[i]   = camPos;
	}
	
	{
		D3DViewPort viewport;
		viewport.TopLeftX = viewport.TopLeftY = 0.0f;
		viewport.Width = (float)pDepthRT->GetWidthNonVirtual();
		viewport.Height = (float)pDepthRT->GetHeightNonVirtual();
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		
		m_passLightVolumes.SetDepthTarget(pDepthRT);
		m_passLightVolumes.SetViewport(viewport);
		m_passLightVolumes.SetOutputUAV(0, &pTiledShading->m_tileOpaqueLightMaskBuf);
		m_passLightVolumes.SetOutputUAV(1, &pTiledShading->m_tileTranspLightMaskBuf);
		m_passLightVolumes.BeginAddingPrimitives();
		
		uint32 curIndex = 0;
		for (uint32 pass = 0; pass < eVolumeType_Count * 2; pass++)
		{
			uint32 volumeType = pass % eVolumeType_Count;
			bool bInsideVolume = pass < eVolumeType_Count;
			
			if (m_numVolumesPerPass[pass] > 0)
			{
				CRenderPrimitive& primitive = m_volumePasses[pass];

				static CCryNameTSCRC techLightVolume("VolumeLightListGen");
				primitive.SetTechnique(CShaderMan::s_shDeferredShading, techLightVolume, 0);
				primitive.SetRenderState(bInsideVolume ? GS_NODEPTHTEST : GS_DEPTHFUNC_GEQUAL);
				primitive.SetEnableDepthClip(!bInsideVolume);
				primitive.SetCullMode(bInsideVolume ? eCULL_Front : eCULL_Back);
				primitive.SetTexture(3, CTexture::s_ptexZTargetScaled3, EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Pixel);
				primitive.SetBuffer(1, &m_lightVolumeInfoBuf, EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Pixel);

				SVolumeGeometry& volumeMesh = m_volumeMeshes[volumeType];
				uint32 numIndices = volumeMesh.numIndices;
				uint32 numVertices = volumeMesh.numVertices;
				uint32 numInstances = m_numVolumesPerPass[pass];
			
				primitive.SetCustomVertexStream(volumeMesh.vertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
				primitive.SetCustomIndexStream(volumeMesh.indexBuffer, Index16);
				primitive.SetDrawInfo(eptTriangleList, 0, 0, numIndices * numInstances);
				primitive.SetBuffer(0, &volumeMesh.vertexDataBuf, EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Pixel);
				primitive.Compile(m_passLightVolumes);

				{
					auto constants = primitive.GetConstantManager().BeginTypedConstantUpdate<VolumeLightListGenConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Vertex | EShaderStage_Pixel);

					constants->screenScale = Vec4((float)pDepthRT->GetWidth(), (float)pDepthRT->GetHeight(), 0, 0);

					float zn = viewInfo[0].pRenderCamera->fNear;
					float zf = viewInfo[0].pRenderCamera->fFar;
					constants->lightIndexOffset = curIndex;
					constants->numVertices = numVertices;

					constants->matViewProj = matViewProj[0];
					constants->viewerPos   = viewerPos[0];
					constants->worldBasisX = worldBasisX[0];
					constants->worldBasisY = worldBasisY[0];
					constants->worldBasisZ = worldBasisZ[0];

					if (viewInfoCount > 1)
					{
						constants.BeginStereoOverride(true);
						constants->matViewProj = matViewProj[1];
						constants->viewerPos   = viewerPos[1];
						constants->worldBasisX = worldBasisX[1];
						constants->worldBasisY = worldBasisY[1];
						constants->worldBasisZ = worldBasisZ[1];
					}

					primitive.GetConstantManager().EndTypedConstantUpdate(constants);
				}
				
				m_passLightVolumes.AddPrimitive(&primitive);
			}

			curIndex += m_numVolumesPerPass[pass];
		}
	}

	m_passLightVolumes.Execute();

	return true;
}


void CTiledShadingStage::Execute()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CTiledShading* pTiledShading = &rd->GetTiledShading();

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	// on dx11, some input resources might still be bound as render targets.
	// dx11 will then just ignore the bind call
	ID3D11RenderTargetView* nullViews[] = { nullptr, nullptr, nullptr, nullptr };
	rd->GetDeviceContext().OMSetRenderTargets(4, nullViews, nullptr);
#endif

	int screenWidth = rd->GetWidth();
	int screenHeight = rd->GetHeight();
	int gridWidth = screenWidth;
	int gridHeight = screenHeight;

	if (CVrProjectionManager::IsMultiResEnabledStatic())
		CVrProjectionManager::Instance()->GetProjectionSize(screenWidth, screenHeight, gridWidth, gridHeight);
	
	uint32 dispatchSizeX = gridWidth / LightTileSizeX + (gridWidth % LightTileSizeX > 0 ? 1 : 0);
	uint32 dispatchSizeY = gridHeight / LightTileSizeY + (gridHeight % LightTileSizeY > 0 ? 1 : 0);

	bool bSeparateCullingPass = ExecuteVolumeListGen(dispatchSizeX, dispatchSizeY);
	
	if (CRenderer::CV_r_DeferredShadingTiled == 4 || CRenderer::CV_r_GraphicsPipelineMobile)
		return;
	
	uint64 rtFlags = 0;
	if (CRenderer::CV_r_DeferredShadingTiled > 1)   // Tiled deferred
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	if (CRenderer::CV_r_DeferredShadingTiledDebug == 2)  // Light coverage visualization
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE2];
	if (bSeparateCullingPass)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	if (CRenderer::CV_r_SSReflections)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE3];
	if (CRenderer::CV_r_DeferredShadingSSS)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE4];
	if (CRenderer::CV_r_DeferredShadingAreaLights > 0)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE5];

#if defined(FEATURE_SVO_GI)
	if (CSvoRenderer::GetInstance()->IsActive())
	{
		int nModeGI = CSvoRenderer::GetInstance()->GetIntegratioMode();

		if (nModeGI == 0 && CSvoRenderer::GetInstance()->GetUseLightProbes())
		{
			// AO modulates diffuse and specular
			rtFlags |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
		}
		else if (nModeGI <= 1)
		{
			// GI replaces diffuse and modulates specular
			rtFlags |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];
		}
		else if (nModeGI == 2)
		{
			// GI replaces diffuse and specular
			rtFlags |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
			rtFlags |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];
		}
	}
#endif

	rtFlags |= CVrProjectionManager::Instance()->GetRTFlags();

	CTexture* texClipVolumeIndex = CTexture::s_ptexVelocity;
	CTexture* pTexCaustics = pTiledShading->m_bApplyCaustics ? CTexture::s_ptexSceneTargetR11G11B10F[1] : CTexture::s_ptexBlack;
	CTexture* pTexAOColorBleed = CRenderer::CV_r_ssdoColorBleeding ? CTexture::s_ptexAOColorBleed : CTexture::s_ptexBlack;

	CTexture* pTexGiDiff = CTexture::s_ptexBlack;
	CTexture* pTexGiSpec = CTexture::s_ptexBlack;

#if defined(FEATURE_SVO_GI)
	if (CSvoRenderer::GetInstance()->IsActive() && CSvoRenderer::GetInstance()->GetSpecularFinRT())
	{
		pTexGiDiff = (CTexture*)CSvoRenderer::GetInstance()->GetDiffuseFinRT();
		pTexGiSpec = (CTexture*)CSvoRenderer::GetInstance()->GetSpecularFinRT();
	}
#endif

	static int s_prevTexAOColorBleed = pTexAOColorBleed->GetID();

	if (m_passCullingShading.InputChanged(int(rtFlags >> 32), int(rtFlags), pTexCaustics->GetID(), pTexGiDiff->GetID()) || s_prevTexAOColorBleed != pTexAOColorBleed->GetID())
	{
		static CCryNameTSCRC techTiledShading("TiledDeferredShading");
		m_passCullingShading.SetTechnique(CShaderMan::s_shDeferredShading, techTiledShading, rtFlags);

		m_passCullingShading.SetOutputUAV(0, &pTiledShading->m_tileTranspLightMaskBuf);
		m_passCullingShading.SetOutputUAV(1, CTexture::s_ptexHDRTarget);
		m_passCullingShading.SetOutputUAV(2, CTexture::s_ptexSceneTargetR11G11B10F[0]);

		m_passCullingShading.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);

		m_passCullingShading.SetTexture(0, CTexture::s_ptexZTarget);
		m_passCullingShading.SetTexture(1, CTexture::s_ptexSceneNormalsMap);
		m_passCullingShading.SetTexture(2, CTexture::s_ptexSceneSpecular);
		m_passCullingShading.SetTexture(3, CTexture::s_ptexSceneDiffuse);
		m_passCullingShading.SetTexture(4, CTexture::s_ptexShadowMask);
		m_passCullingShading.SetTexture(5, CTexture::s_ptexSceneNormalsBent);
		m_passCullingShading.SetTexture(6, CTexture::s_ptexHDRTargetScaledTmp[0]);
		m_passCullingShading.SetTexture(7, CTexture::s_ptexEnvironmentBRDF);
		m_passCullingShading.SetTexture(8, texClipVolumeIndex);
		m_passCullingShading.SetTexture(9, pTexAOColorBleed);
		m_passCullingShading.SetTexture(10, pTexGiDiff);
		m_passCullingShading.SetTexture(11, pTexGiSpec);
		m_passCullingShading.SetTexture(12, pTexCaustics);

		m_passCullingShading.SetBuffer(16, bSeparateCullingPass ? &pTiledShading->m_tileOpaqueLightMaskBuf : &pTiledShading->m_lightCullInfoBuf);
		m_passCullingShading.SetBuffer(17, &pTiledShading->m_lightShadeInfoBuf);
		m_passCullingShading.SetBuffer(18, &pTiledShading->m_clipVolumeInfoBuf);
		m_passCullingShading.SetTexture(19, pTiledShading->m_specularProbeAtlas.texArray);
		m_passCullingShading.SetTexture(20, pTiledShading->m_diffuseProbeAtlas.texArray);
		m_passCullingShading.SetTexture(21, pTiledShading->m_spotTexAtlas.texArray);

		s_prevTexAOColorBleed = pTexAOColorBleed->GetID();
		m_pTexGiDiff = pTexGiDiff;
		m_pTexGiSpec = pTexGiSpec;
	}

	D3DViewPort viewport = { 0.f, 0.f, float(screenWidth), float(screenHeight), 0.f, 1.f };
	CStandardGraphicsPipeline::SViewInfo viewInfo[2];
	int viewInfoCount = rd->GetGraphicsPipeline().GetViewInfo(viewInfo, &viewport);
	rd->GetGraphicsPipeline().UpdatePerViewConstantBuffer(viewInfo, viewInfoCount, m_pPerViewConstantBuffer);
	m_passCullingShading.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_pPerViewConstantBuffer);

	m_passCullingShading.BeginConstantUpdate();
	{
		static CCryNameR projParamsName("ProjParams");
		Vec4 projParams(rd->m_ProjMatrix.m00, rd->m_ProjMatrix.m11, rd->m_ProjMatrix.m20, rd->m_ProjMatrix.m21);
		m_passCullingShading.SetConstant(projParamsName, projParams);

		static CCryNameR screenSizeName("ScreenSize");
		float fScreenWidth = (float)screenWidth;
		float fScreenHeight = (float)screenHeight;
		Vec4 screenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		m_passCullingShading.SetConstant(screenSizeName, screenSize);

		static CCryNameR worldViewPosName("WorldViewPos");
		Vec4 worldViewPosParam(rd->GetRCamera().vOrigin, 0);
		m_passCullingShading.SetConstant(worldViewPosName, worldViewPosParam);

		SD3DPostEffectsUtils::UpdateFrustumCorners();
		static CCryNameR frustumTLName("FrustumTL");
		Vec4 frustumTL(SD3DPostEffectsUtils::m_vLT.x, SD3DPostEffectsUtils::m_vLT.y, SD3DPostEffectsUtils::m_vLT.z, 0);
		m_passCullingShading.SetConstant(frustumTLName, frustumTL);
		static CCryNameR frustumTRName("FrustumTR");
		Vec4 frustumTR(SD3DPostEffectsUtils::m_vRT.x, SD3DPostEffectsUtils::m_vRT.y, SD3DPostEffectsUtils::m_vRT.z, 0);
		m_passCullingShading.SetConstant(frustumTRName, frustumTR);
		static CCryNameR frustumBLName("FrustumBL");
		Vec4 frustumBL(SD3DPostEffectsUtils::m_vLB.x, SD3DPostEffectsUtils::m_vLB.y, SD3DPostEffectsUtils::m_vLB.z, 0);
		m_passCullingShading.SetConstant(frustumBLName, frustumBL);

		static CCryNameR sunDirName("SunDir");
		Vec4 sunDirParam(gEnv->p3DEngine->GetSunDirNormalized(), TiledShading_SunDistance);
		m_passCullingShading.SetConstant(sunDirName, sunDirParam);

		static CCryNameR sunColorName("SunColor");
		Vec3 sunColor;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_COLOR, sunColor);
		sunColor *= rd->m_fAdaptedSceneScaleLBuffer;  // Apply LBuffers range rescale
		Vec4 sunColorParam(sunColor.x, sunColor.y, sunColor.z, gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER));
		m_passCullingShading.SetConstant(sunColorName, sunColorParam);

		static CCryNameR ssdoParamsName("SSDOParams");
		Vec4 ssdoParams(CRenderer::CV_r_ssdoAmountDirect, CRenderer::CV_r_ssdoAmountAmbient, CRenderer::CV_r_ssdoAmountReflection, 0);
#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer::GetInstance()->IsActive())
			ssdoParams *= CSvoRenderer::GetInstance()->GetSsaoAmount();
#endif
		Vec4 ssdoNullParams(0, 0, 0, 0);
		m_passCullingShading.SetConstant(ssdoParamsName, CRenderer::CV_r_ssdo ? ssdoParams : ssdoNullParams);
	}

	if (CVrProjectionManager::IsMultiResEnabledStatic())
	{
		auto constantBuffer = CVrProjectionManager::Instance()->GetProjectionConstantBuffer(screenWidth, screenHeight);
		m_passCullingShading.SetInlineConstantBuffer(eConstantBufferShaderSlot_VrProjection, constantBuffer);
	}

	m_passCullingShading.SetDispatchSize(dispatchSizeX, dispatchSizeY, 1);

	{
		PROFILE_LABEL_SCOPE("SHADING");		
		// Prepare buffers and textures which have been used in the z-pass and post-processes for use in the compute queue
		// Reduce resource state switching by requesting the most inclusive resource state
		m_passCullingShading.PrepareResourcesForUse(GetDeviceObjectFactory().GetCoreCommandList());

		{
			const bool bAsynchronousCompute = CRenderer::CV_r_D3D12AsynchronousCompute & BIT((eStage_TiledShading - eStage_FIRST_ASYNC_COMPUTE)) ? true : false;
			SScopedComputeCommandList computeCommandList(bAsynchronousCompute);

			m_passCullingShading.Execute(computeCommandList, EShaderStage_All);
		}
	}

	// Output debug information
	if (CRenderer::CV_r_DeferredShadingTiledDebug == 1)
	{
		IRenderAuxText::Draw2dLabel(20, 60, 2.0f, Col_Blue, false, "Tiled Shading Debug");
		IRenderAuxText::Draw2dLabel(20, 95, 1.7f, pTiledShading->m_numSkippedLights > 0 ? Col_Red : Col_Blue, false, "Skipped Lights: %i", pTiledShading->m_numSkippedLights);
		IRenderAuxText::Draw2dLabel(20, 120, 1.7f, Col_Blue, false, "Atlas Updates: %i", pTiledShading->m_numAtlasUpdates);
	}

	pTiledShading->m_bApplyCaustics = false;  // Reset flag
}