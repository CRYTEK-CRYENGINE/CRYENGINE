// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ClipVolume.h"
#include "ClipVolumeManager.h"
#include "LightEntity.h"
#include "FogVolumeRenderNode.h"

CClipVolumeManager::~CClipVolumeManager()
{
	assert(m_ClipVolumes.empty());
}

IClipVolume* CClipVolumeManager::CreateClipVolume()
{
	SClipVolumeInfo volumeInfo(new CClipVolume());
	m_ClipVolumes.push_back(volumeInfo);

	return m_ClipVolumes.back().m_pVolume;
}

bool CClipVolumeManager::DeleteClipVolume(IClipVolume* pClipVolume)
{
	if (m_ClipVolumes.Delete(static_cast<CClipVolume*>(pClipVolume)))
	{
		delete pClipVolume;
		return true;
	}
	return false;
}

bool CClipVolumeManager::UpdateClipVolume(IClipVolume* pClipVolume, _smart_ptr<IRenderMesh> pRenderMesh, IBSPTree3D* pBspTree, const Matrix34& worldTM, bool bActive, uint32 flags, const char* szName)
{
	int nVolumeIndex = m_ClipVolumes.Find((CClipVolume*)pClipVolume);
	if (nVolumeIndex >= 0)
	{
		SClipVolumeInfo& volumeInfo = m_ClipVolumes[nVolumeIndex];
		volumeInfo.m_pVolume->Update(pRenderMesh, pBspTree, worldTM, flags);
		volumeInfo.m_pVolume->SetName(szName);
		volumeInfo.m_bActive = bActive;

		AABB volumeBBox = pClipVolume->GetClipVolumeBBox();
		Get3DEngine()->m_pObjManager->ReregisterEntitiesInArea(volumeBBox.min, volumeBBox.max);
		return true;
	}

	return false;
}

void CClipVolumeManager::PrepareVolumesForRendering(const SRenderingPassInfo& passInfo)
{
	for (size_t i = 0; i < m_ClipVolumes.size(); ++i)
	{
		SClipVolumeInfo& volInfo = m_ClipVolumes[i];
		volInfo.m_pVolume->SetStencilRef(InactiveVolumeStencilRef);

		if (volInfo.m_bActive && passInfo.GetCamera().IsAABBVisible_F(volInfo.m_pVolume->GetClipVolumeBBox()))
		{
			uint8 nStencilRef = passInfo.GetIRenderView()->AddClipVolume(volInfo.m_pVolume);
			volInfo.m_pVolume->SetStencilRef(nStencilRef);
		}
	}
}

void CClipVolumeManager::UpdateEntityClipVolume(const Vec3& pos, IRenderNode* pRenderNode)
{
	FRAME_PROFILER("CClipVolumeManager::UpdateEntityClipVolume", GetSystem(), PROFILE_3DENGINE);

	if (!pRenderNode || !pRenderNode->m_pTempData)
		return;

	IClipVolume* pPreviousVolume = pRenderNode->m_pTempData->userData.m_pClipVolume;
	UnregisterRenderNode(pRenderNode);

	// user assigned clip volume
	CLightEntity* pLight = static_cast<CLightEntity*>(pRenderNode);
	if (pRenderNode->GetRenderNodeType() == eERType_Light && (pLight->m_light.m_Flags & DLF_HAS_CLIP_VOLUME) != 0)
	{
		for (int i = 1; i >= 0; --i)
		{
			if (CClipVolume* pVolume = static_cast<CClipVolume*>(pLight->m_light.m_pClipVolumes[i]))
				pVolume->RegisterRenderNode(pRenderNode);
		}
	}
	else // assign by position
	{
		// Check if entity is in same clip volume as before
		if (pPreviousVolume && (pPreviousVolume->GetClipVolumeFlags() & IClipVolume::eClipVolumeIsVisArea) == 0)
		{
			CClipVolume* pVolume = static_cast<CClipVolume*>(pPreviousVolume);
			if (pVolume->IsPointInsideClipVolume(pos))
			{
				pVolume->RegisterRenderNode(pRenderNode);
				return;
			}
		}

		if (CClipVolume* pVolume = GetClipVolumeByPos(pos, pPreviousVolume))
			pVolume->RegisterRenderNode(pRenderNode);
	}
}

void CClipVolumeManager::UnregisterRenderNode(IRenderNode* pRenderNode)
{
	if (!pRenderNode)
		return;

	for (size_t i = 0; i < m_ClipVolumes.size(); ++i)
		m_ClipVolumes[i].m_pVolume->UnregisterRenderNode(pRenderNode);

	if (pRenderNode->m_pTempData)
		pRenderNode->m_pTempData->userData.m_pClipVolume = NULL;
}

bool CClipVolumeManager::IsClipVolumeRequired(IRenderNode* pRenderNode) const
{
	const uint32 NoClipVolumeLights = DLF_SUN | DLF_ATTACH_TO_SUN;

	const bool bForwardObject = (pRenderNode->m_nInternalFlags & IRenderNode::REQUIRES_FORWARD_RENDERING) != 0;
	const EERType ertype = pRenderNode->GetRenderNodeType();
	const bool bIsValidLight = ertype == eERType_Light &&
	                           (static_cast<CLightEntity*>(pRenderNode)->m_light.m_Flags & NoClipVolumeLights) == 0;
	const bool bIsValidFogVolume = (ertype == eERType_FogVolume) &&
	                               static_cast<CFogVolumeRenderNode*>(pRenderNode)->IsAffectsThisAreaOnly();

	return bIsValidLight || bForwardObject || bIsValidFogVolume;
}

CClipVolume* CClipVolumeManager::GetClipVolumeByPos(const Vec3& pos, const IClipVolume* pIgnoreVolume) const
{
	for (size_t i = 0; i < m_ClipVolumes.size(); ++i)
	{
		const SClipVolumeInfo& volInfo = m_ClipVolumes[i];

		if (volInfo.m_bActive && volInfo.m_pVolume != pIgnoreVolume && volInfo.m_pVolume->IsPointInsideClipVolume(pos))
			return m_ClipVolumes[i].m_pVolume;
	}

	return NULL;
}

void CClipVolumeManager::GetMemoryUsage(class ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(this));
	for (size_t i = 0; i < m_ClipVolumes.size(); ++i)
		pSizer->AddObject(m_ClipVolumes[i].m_pVolume);
}
