// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __particleitem_h__
#define __particleitem_h__
#pragma once

#include "BaseLibraryItem.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryParticleSystem/IParticles.h>

/*! CParticleItem contain definition of particle system spawning parameters.
 *
 */
class SANDBOX_API CParticleItem : public CBaseLibraryItem
{
public:
	CParticleItem();
	CParticleItem(IParticleEffect* pEffect);
	~CParticleItem();

	virtual EDataBaseItemType GetType() const { return EDB_TYPE_PARTICLE; };

	virtual void              SetName(const string& name);
	void                      Serialize(SerializeContext& ctx);

	//////////////////////////////////////////////////////////////////////////
	// Child particle systems.
	//////////////////////////////////////////////////////////////////////////
	//! Get number of sub Particles childs.
	int            GetChildCount() const;
	//! Get sub Particles child by index.
	CParticleItem* GetChild(int index) const;
	//! Remove all sub Particles.
	void           ClearChilds();
	//! Insert sub particles in between other particles.
	//void InsertChild( int slot,CParticleItem *mtl );
	//! Find slot where sub Particles stored.
	//! @retun slot index if Particles found, -1 if Particles not found.
	int            FindChild(CParticleItem* mtl);
	//! Sets new parent of item; may be 0.
	void           SetParent(CParticleItem* pParent);
	//! Returns parent Particles.
	CParticleItem* GetParent() const;

	void           GenerateIdRecursively();

	//! Called after particle parameters where updated.
	void Update();

	//! Get particle effect assigned to this particle item.
	IParticleEffect* GetEffect() const;

	void             DebugEnable(int iEnable = -1);
	int              GetEnabledState() const;

	virtual void     GatherUsedResources(CUsedResources& resources);

	void             AddAllChildren();

private:
	_smart_ptr<IParticleEffect> m_pEffect;

	//! Parent of this material (if this is sub material).
	CParticleItem*                        m_pParentParticles;
	//! Array of sub particle items.
	std::vector<TSmartPtr<CParticleItem>> m_childs;
	bool m_bSaveEnabled, m_bDebugEnabled;

	// asset resolving
	typedef std::map<IVariable::EDataType, uint32> TResolveReq;
	TResolveReq m_ResolveRequests;
};

#endif // __particleitem_h__

