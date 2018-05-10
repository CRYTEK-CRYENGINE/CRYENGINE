// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: Encapsulates physics proxy for improving pick and throw ragdoll
throwing behavior
-------------------------------------------------------------------------
History:
- 11:10:2010	17:17 : Created by David Ramos
*************************************************************************/
#pragma once

#ifndef __PICK_AND_THROW_PROXY
#define __PICK_AND_THROW_PROXY

#include <SharedParams/ISharedParams.h>

class CPlayer;
struct IItemParamsNode;
struct IPhysicalEntity;

class CPickAndThrowProxy;
DECLARE_SHARED_POINTERS(CPickAndThrowProxy);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CPickAndThrowProxy
{
	enum EProxyShapes
	{
		ePS_Capsule = 0,
		ePS_Sphere,
		ePS_Cylinder,
	};

public:
	BEGIN_SHARED_PARAMS(SPnTProxyParams)
	private:
		friend class CPickAndThrowProxy;
		SPnTProxyParams();

		float 						fRadius;
		float 						fHeight;
		Vec3							vPosPivot;
		EProxyShapes			proxyShape;
	END_SHARED_PARAMS

	enum { geom_colltype_proxy = geom_colltype12 };

	static CPickAndThrowProxyPtr Create(CPlayer* pPlayer, const IItemParamsNode* pParams);

	CPickAndThrowProxy(CPlayer& player);
	~CPickAndThrowProxy();

	void 				Physicalize();
	void 				Unphysicalize();

	ILINE bool	IsActive() const { return (m_pPickNThrowProxy != NULL); }

	void				OnReloadExtension();
	void				ReadXmlData(const IItemParamsNode* pRootNode);
	ILINE bool	NeedsReloading() const { return m_bNeedsReloading; }

private:
	CryFixedStringT<64>				GetSharedParamsName() const;

	SPnTProxyParamsConstPtr		m_pParams;
	IPhysicalEntity*					m_pPickNThrowProxy;
	CPlayer&									m_player;
	IPhysicalEntity*					m_pLastPlayerPhysics;
	bool											m_bNeedsReloading;
};

#endif // __PICK_AND_THROW_PROXY