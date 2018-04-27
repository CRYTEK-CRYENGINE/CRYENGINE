// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef SHADOW_TEXTURE_GROUP_MANAGER_H
	#define SHADOW_TEXTURE_GROUP_MANAGER_H

	#include <vector>   // STL vector<>

struct SDynTexture_Shadow;

// to combine multiple shadowmaps into one texture (e.g. GSM levels or Cubemap sides)
class CShadowTextureGroupManager
{
public:

	// Arguments
	//   pLightOwner - must not be 0
	// Returns
	//   pointer to the texture pointer, don't store this pointer
	SDynTexture_Shadow** FindOrCreateShadowTextureGroup(const IRenderNode* pLightOwner)
	{
		std::vector<SShadowTextureGroup>::iterator it, end = m_GSMGroups.end();

		for (it = m_GSMGroups.begin(); it != end; ++it)
		{
			SShadowTextureGroup& ref = *it;

			if (ref.m_pLightOwner == pLightOwner)
				return &(ref.m_pTextureGroupItem);
		}

		m_GSMGroups.push_back(SShadowTextureGroup(pLightOwner, 0));

		return &(m_GSMGroups.back().m_pTextureGroupItem);
	}

	void RemoveTextureGroupEntry(const IRenderNode* pLightOwner)
	{
		std::vector<SShadowTextureGroup>::iterator it, end = m_GSMGroups.end();

		for (it = m_GSMGroups.begin(); it != end; ++it)
		{
			SShadowTextureGroup& ref = *it;

			if (ref.m_pLightOwner == pLightOwner)
			{
				m_GSMGroups.erase(it);
				break;
			}
		}
	}

	//  TODO:  remove old entries !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

private: // ----------------------------------------------------------------------------------

	struct SShadowTextureGroup
	{
		SShadowTextureGroup(const IRenderNode* pLightOwner, SDynTexture_Shadow* pTextureGroupItem)
			: m_pLightOwner(pLightOwner), m_pTextureGroupItem(pTextureGroupItem)
		{
		}

		const IRenderNode*  m_pLightOwner;                              // key
		SDynTexture_Shadow* m_pTextureGroupItem;                        // can be extended to combine 6 cubemap sides
	};

	std::vector<SShadowTextureGroup> m_GSMGroups;         // could be a map<emitterid,m_pTextureGroupItem[]> but vector is faster for small containers
};

#endif // SHADOW_TEXTURE_GROUP_MANAGER_H
