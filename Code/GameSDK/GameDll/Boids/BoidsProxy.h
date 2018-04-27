// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BoidsProxy.h
//  Version:     v1.00
//  Created:     2/10/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BoidsProxy_h__
#define __BoidsProxy_h__
#pragma once

#include <CryEntitySystem/IEntityComponent.h>

class CFlock;
class CBoidObject;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
class CBoidsProxy : public IEntityComponent
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CBoidsProxy, "CBoidsProxy", "382d896a-7c22-4637-b323-21c67ec5f588"_cry_guid)

		CBoidsProxy();
	virtual ~CBoidsProxy();

public:

	IEntity* GetEntity() const { return m_pEntity; };

	// IEntityComponent.h interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void Initialize() final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetProxyType() const final { return ENTITY_PROXY_BOIDS; }
	virtual void Release() final;
	virtual	void ProcessEvent(const SEntityEvent& event) final;
	virtual uint64 GetEventMask() const final;
	virtual bool Init(IEntity *pEntity, SEntitySpawnParams &params) final { return true; }
	virtual void Reload(IEntity *pEntity, SEntitySpawnParams &params) final;
	virtual void GameSerialize(TSerialize ser) final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void SetFlock(CFlock *pFlock);
	CFlock* GetFlock() { return m_pFlock; }
	void OnTrigger(bool bEnter, const SEntityEvent &event);

	virtual void GetMemoryUsage(ICrySizer *pSizer)const final
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_pFlock);
	}
private:
	void OnMove();

private:
	//////////////////////////////////////////////////////////////////////////
	// Private member variables.
	//////////////////////////////////////////////////////////////////////////
	// Flock of items.
	CFlock *m_pFlock;

	int m_playersInCount;
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
struct CBoidObjectProxy : public IEntityComponent
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CBoidObjectProxy, "CBoidObjectProxy", "191dc4b2-c222-4e1e-81bd-fe1d882c9f3e"_cry_guid)

		CBoidObjectProxy();
	virtual ~CBoidObjectProxy() {}

public:
	IEntity* GetEntity() const { return m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityEvent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void Initialize() override;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetProxyType() const override { return ENTITY_PROXY_BOID_OBJECT; }
	virtual	void ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const final;
	virtual void GameSerialize(TSerialize ser) override;
	virtual bool NeedGameSerialize() override { return false; };
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void SetBoid(CBoidObject *pBoid) { m_pBoid = pBoid; };
	CBoidObject* GetBoid() { return m_pBoid; }

	virtual void GetMemoryUsage(ICrySizer *pSizer)const override
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_pBoid);
	}
private:
	//////////////////////////////////////////////////////////////////////////
	// Private member variables.
	//////////////////////////////////////////////////////////////////////////
	// Host entity.
	IEntity *m_pEntity;
	// Host Flock.
	CBoidObject *m_pBoid;
};

#endif //__BoidsProxy_h__
