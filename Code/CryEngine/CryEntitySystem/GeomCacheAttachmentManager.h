// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// ------------------------------------------------------------------------
//  File name:   GeomCacheAttachmentManager.h
//  Created:     2013-6-12 by Axel Gneiting
//  Description: Manages geometry cache attachments
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _GEOMCACHE_ATTACHMENT_MANAGER_
	#define _GEOMCACHE_ATTACHMENT_MANAGER_

	#if defined(USE_GEOM_CACHES)
struct IGeomCacheRenderNode;
class CEntity;

		#include <CryCore/Containers/VectorMap.h>

class CGeomCacheAttachmentManager
{
public:
	void     Update();

	void     RegisterAttachment(CEntity* pChild, CEntity* pParent, const uint32 targetHash);
	void     UnregisterAttachment(const CEntity* pChild, const CEntity* pParent);

	Matrix34 GetNodeWorldTM(const CEntity* pChild, const CEntity* pParent) const;
	bool     IsAttachmentValid(const CEntity* pChild, const CEntity* pParent) const;

private:
	struct SBinding
	{
		CEntity* pChild;
		CEntity* pParent;

		bool operator<(const SBinding& rhs) const
		{
			return (pChild != rhs.pChild) ? (pChild < rhs.pChild) : (pParent < rhs.pParent);
		}
	};

	struct SAttachmentData
	{
		uint m_nodeIndex;
	};

	VectorMap<SBinding, SAttachmentData> m_attachments;
};

	#endif
#endif
