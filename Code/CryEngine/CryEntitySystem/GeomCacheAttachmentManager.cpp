// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#if defined(USE_GEOM_CACHES)
	#include "GeomCacheAttachmentManager.h"
	#include "Entity.h"

void CGeomCacheAttachmentManager::Update()
{
	for (VectorMap<SBinding, SAttachmentData>::iterator iter = m_attachments.begin(); iter != m_attachments.end(); ++iter)
	{
		iter->first.pChild->InvalidateTM(ENTITY_XFORM_FROM_PARENT);
	}
}

void CGeomCacheAttachmentManager::RegisterAttachment(CEntity* pChild, CEntity* pParent, const uint32 targetHash)
{
	IGeomCacheRenderNode* pGeomCacheRenderNode = const_cast<CEntity*>(pParent)->GetGeomCacheRenderNode(0);

	if (pGeomCacheRenderNode)
	{
		const uint nodeCount = pGeomCacheRenderNode->GetNodeCount();

		for (uint i = 0; i < nodeCount; ++i)
		{
			const uint32 nodeNameHash = pGeomCacheRenderNode->GetNodeNameHash(i);
			if (nodeNameHash == targetHash)
			{
				SBinding binding;
				binding.pChild = pChild;
				binding.pParent = pParent;

				SAttachmentData attachment;
				attachment.m_nodeIndex = i;

				m_attachments[binding] = attachment;
				break;
			}
		}
	}
}

void CGeomCacheAttachmentManager::UnregisterAttachment(const CEntity* pChild, const CEntity* pParent)
{
	SBinding binding;
	binding.pChild = const_cast<CEntity*>(pChild);
	binding.pParent = const_cast<CEntity*>(pParent);

	m_attachments.erase(binding);
}

Matrix34 CGeomCacheAttachmentManager::GetNodeWorldTM(const CEntity* pChild, const CEntity* pParent) const
{
	SBinding binding;
	binding.pChild = const_cast<CEntity*>(pChild);
	binding.pParent = const_cast<CEntity*>(pParent);

	VectorMap<SBinding, SAttachmentData>::const_iterator findIter = m_attachments.find(binding);

	if (findIter != m_attachments.end())
	{
		const SAttachmentData data = findIter->second;
		IGeomCacheRenderNode* pGeomCacheRenderNode = const_cast<CEntity*>(binding.pParent)->GetGeomCacheRenderNode(0);
		if (pGeomCacheRenderNode)
		{
			Matrix34 nodeTransform = pGeomCacheRenderNode->GetNodeTransform(data.m_nodeIndex);
			nodeTransform.OrthonormalizeFast();
			return pParent->GetWorldTM() * nodeTransform;
		}
	}

	return pParent->GetWorldTM();
}

bool CGeomCacheAttachmentManager::IsAttachmentValid(const CEntity* pChild, const CEntity* pParent) const
{
	SBinding binding;
	binding.pChild = const_cast<CEntity*>(pChild);
	binding.pParent = const_cast<CEntity*>(pParent);

	VectorMap<SBinding, SAttachmentData>::const_iterator findIter = m_attachments.find(binding);

	if (findIter != m_attachments.end())
	{
		const SAttachmentData data = findIter->second;
		IGeomCacheRenderNode* pGeomCacheRenderNode = const_cast<CEntity*>(binding.pParent)->GetGeomCacheRenderNode(0);
		return pGeomCacheRenderNode->IsNodeDataValid(data.m_nodeIndex);
	}

	return false;
}
#endif
