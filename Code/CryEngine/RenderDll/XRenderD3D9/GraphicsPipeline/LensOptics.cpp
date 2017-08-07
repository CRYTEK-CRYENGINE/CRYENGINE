#include "StdAfx.h"
#include "DriverD3D.h"
#include "LensOptics.h"
#include "Common/RendElements/RootOpticsElement.h"
#include "Common/RendElements/FlareSoftOcclusionQuery.h"
#include "Common/RenderView.h"

void CLensOpticsStage::Init()
{
	m_softOcclusionManager.Init();

	CFlareSoftOcclusionQuery::InitGlobalResources();
}

void CLensOpticsStage::Execute(CRenderView* pRenderView)
{
	auto& lensOpticsElements = pRenderView->GetRenderItems(EFSLIST_LENSOPTICS);
	m_primitivesRendered = 0;

	if (!lensOpticsElements.empty())
	{
		CTexture* pDestRT = CTexture::s_ptexSceneTargetR11G11B10F[0];

		D3DViewPort viewport;
		viewport.TopLeftX = viewport.TopLeftY = 0.0f;
		viewport.Width = (float)pDestRT->GetWidth();
		viewport.Height = (float)pDestRT->GetHeight();
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		CStandardGraphicsPipeline::SViewInfo viewInfo[CCamera::eEye_eCount];
		int viewInfoCount = gcpRendD3D->GetGraphicsPipeline().GetViewInfo(viewInfo, &viewport);

		const int  frameID          = gcpRendD3D.GetFrameID(false);
		const bool bUpdateOcclusion = m_occlusionUpdateFrame != frameID;

		if (bUpdateOcclusion)
		{
			UpdateOcclusionQueries(viewInfo, viewInfoCount);
			m_occlusionUpdateFrame = frameID;
		}
		GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearSurface(pDestRT->GetSurface(0, 0), Clr_Transparent);

		m_passLensOptics.SetRenderTarget(0, pDestRT);
		m_passLensOptics.SetViewport(viewport);

		for (auto& re : lensOpticsElements)
		{
			m_passLensOptics.BeginAddingPrimitives();
			std::vector<CPrimitiveRenderPass*> prePasses;

			CD3D9Renderer* pRD = gcpRendD3D;
			CRenderObject* pObj = re.pObj;
			SRenderObjData* pOD = pObj->GetObjData();
			const CRenderCamera* cam = viewInfo[0].pRenderCamera;

			SRenderLight* pLight = pOD ? &pRenderView->GetLight(eDLT_DeferredLight, pOD->m_nLightID) : nullptr;
			RootOpticsElement* pRootElem = pLight ? (RootOpticsElement*)pLight->GetLensOpticsElement() : nullptr;

			if (!pRootElem || pRootElem->GetType() != eFT_Root)
				continue;

			CFlareSoftOcclusionQuery* pOcc = static_cast<CFlareSoftOcclusionQuery*>(pLight->m_pSoftOccQuery);
			if (!pOcc)
				continue;

			{
				PROFILE_LABEL_SCOPE(pLight->m_sName);

				pRootElem->SetOcclusionQuery(pOcc);
				pOcc->SetOccPlaneSizeRatio(pRootElem->GetOccSize());

				RootOpticsElement::SFlareLight flareLight;
				if (pLight->m_Flags & DLF_ATTACH_TO_SUN)
				{
					const float sunHeight = 20000.0f;
					Vec3 sunDirNorm = gEnv->p3DEngine->GetSunDir();
					sunDirNorm.Normalize();

					flareLight.m_vPos = cam->vOrigin + sunDirNorm * sunHeight;
					flareLight.m_cLdrClr = gEnv->p3DEngine->GetSunColor();
					flareLight.m_fRadius = sunHeight;
					flareLight.m_bAttachToSun = true;
					pLight->SetPosition(flareLight.m_vPos);
				}
				else
				{
					flareLight.m_vPos = pObj->GetTranslation();
					ColorF& c = pLight->m_Color;
					flareLight.m_cLdrClr.set(c.r, c.g, c.b, 1.0f);
					flareLight.m_fRadius = pLight->m_fRadius;
					flareLight.m_bAttachToSun = false;
				}

				ColorF cNorm;
				flareLight.m_fClrMultiplier = flareLight.m_cLdrClr.NormalizeCol(cNorm);
				flareLight.m_cLdrClr = cNorm;

				flareLight.m_fViewAngleFalloff = 1.0;
				if (pLight->m_LensOpticsFrustumAngle != 0)
				{
					Vec3 vDirLight2Camera = (cam->vOrigin - flareLight.m_vPos).GetNormalizedFast();
					float viewCos = pLight->m_ProjMatrix.GetColumn(0).Dot(vDirLight2Camera) * 0.5f + 0.5f;
					if (pLight->m_LensOpticsFrustumAngle < 255)
					{
						int angle = (int)((float)pLight->m_LensOpticsFrustumAngle * (360.0f / 255.0f));
						float radAngle = DEG2RAD(angle);
						float halfFrustumCos = cos_tpl(radAngle * 0.5f) * 0.5f + 0.5f;
						float quarterFrustumCos = cos_tpl(radAngle * 0.25f) * 0.5f + 0.5f;
						if (viewCos < quarterFrustumCos)
							flareLight.m_fViewAngleFalloff = max(1.0f - (float)((quarterFrustumCos - viewCos) / (quarterFrustumCos - halfFrustumCos)), 0.0f);
						else
							flareLight.m_fViewAngleFalloff = 1;
					}
				}
				else
				{
					flareLight.m_fViewAngleFalloff = 0.0;
				}

				if (pRootElem->ProcessAll(m_passLensOptics, prePasses, flareLight, viewInfo, viewInfoCount, false, bUpdateOcclusion))
				{
					if (bUpdateOcclusion)
					{
						m_softOcclusionManager.AddSoftOcclusionQuery(pOcc, pLight->GetPosition());
					}
				}

				pRootElem->SetOcclusionQuery(nullptr);

				// execute prepasses first
				for (auto pPass : prePasses)
					pPass->Execute();

				m_passLensOptics.Execute();
				m_primitivesRendered += m_passLensOptics.GetPrimitiveCount();
			}
		}
	}
}

void CLensOpticsStage::UpdateOcclusionQueries(CStandardGraphicsPipeline::SViewInfo* pViewInfo, int viewInfoCount)
{
	PROFILE_LABEL_SCOPE("Soft Occlusion Query");

	CFlareSoftOcclusionQuery::BatchReadResults(); // copy to system memory previous frame

	m_softOcclusionManager.Update(pViewInfo, viewInfoCount);

	CFlareSoftOcclusionQuery::ReadbackSoftOcclQuery(); // update current frame to staging buffer
	for (int i = 0, iSize(m_softOcclusionManager.GetSize()); i < iSize; ++i)
	{
		CFlareSoftOcclusionQuery* pSoftOcclusion = m_softOcclusionManager.GetSoftOcclusionQuery(i);
		if (pSoftOcclusion == NULL)
			continue;
		pSoftOcclusion->UpdateCachedResults();
	}

	m_softOcclusionManager.Reset();
}