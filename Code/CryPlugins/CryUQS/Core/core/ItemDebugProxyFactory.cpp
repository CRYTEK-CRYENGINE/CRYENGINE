// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemDebugProxyFactory.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CItemDebugProxyFactory
		//
		//===================================================================================

		CItemDebugProxyFactory::CItemDebugProxyFactory()
			: m_pLastCreatedDebugItemRenderProxy()
		{}

		IItemDebugProxy_Sphere& CItemDebugProxyFactory::CreateSphere()
		{
			assert(!m_pLastCreatedDebugItemRenderProxy);
			CItemDebugProxy_Sphere* pSphere = new CItemDebugProxy_Sphere;
			m_pLastCreatedDebugItemRenderProxy.reset(pSphere);
			return *pSphere;
		}

		IItemDebugProxy_Path& CItemDebugProxyFactory::CreatePath()
		{
			assert(!m_pLastCreatedDebugItemRenderProxy);
			CItemDebugProxy_Path* pPath = new CItemDebugProxy_Path;
			m_pLastCreatedDebugItemRenderProxy.reset(pPath);
			return *pPath;
		}

		IItemDebugProxy_AABB& CItemDebugProxyFactory::CreateAABB()
		{
			assert(!m_pLastCreatedDebugItemRenderProxy);
			CItemDebugProxy_AABB* pAABB = new CItemDebugProxy_AABB;
			m_pLastCreatedDebugItemRenderProxy.reset(pAABB);
			return *pAABB;
		}

		std::unique_ptr<CItemDebugProxyBase> CItemDebugProxyFactory::GetAndForgetLastCreatedDebugItemRenderProxy()
		{
			return std::move(m_pLastCreatedDebugItemRenderProxy);
		}

		void CItemDebugProxyFactory::Serialize(Serialization::IArchive& ar)
		{
			ar(m_pLastCreatedDebugItemRenderProxy, "m_pLastCreatedDebugItemRenderProxy");
		}

	}
}
