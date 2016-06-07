// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   EntityClassRegistry.h
//  Version:     v1.00
//  Created:     3/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __EntityClassRegistry_h__
#define __EntityClassRegistry_h__
#pragma once

#include <CryCore/Containers/CryListenerSet.h>
#include <CryEntitySystem/IEntityClass.h>

//////////////////////////////////////////////////////////////////////////
// Description:
//    Standard implementation of the IEntityClassRegistry interface.
//////////////////////////////////////////////////////////////////////////
class CEntityClassRegistry : public IEntityClassRegistry
{
public:
	CEntityClassRegistry();
	~CEntityClassRegistry();

	bool          RegisterEntityClass(IEntityClass* pClass) override;
	bool          UnregisterEntityClass(IEntityClass* pClass) override;

	IEntityClass* FindClass(const char* sClassName) const override;
	IEntityClass* GetDefaultClass() const override;

	IEntityClass* RegisterStdClass(const SEntityClassDesc& entityClassDesc) override;

	void          RegisterListener(IEntityClassRegistryListener* pListener) override;
	void          UnregisterListener(IEntityClassRegistryListener* pListener) override;

	void          LoadClasses(const char* szFilename, bool bOnlyNewClasses = false) override;
	void          LoadArchetypes(const char* libPath, bool reload);

	//////////////////////////////////////////////////////////////////////////
	// Registry iterator.
	//////////////////////////////////////////////////////////////////////////
	void          IteratorMoveFirst() override;
	IEntityClass* IteratorNext() override;
	int           GetClassCount() const override { return m_mapClassName.size(); };

	void          InitializeDefaultClasses();

	void          GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_pDefaultClass);
		pSizer->AddContainer(m_mapClassName);
	}

private:
	void LoadArchetypeDescription(const XmlNodeRef& root);
	void LoadClassDescription(const XmlNodeRef& root, bool bOnlyNewClasses);

	void NotifyListeners(EEntityClassRegistryEvent event, const IEntityClass* pEntityClass);

	typedef std::map<string, IEntityClass*> ClassNameMap;
	ClassNameMap           m_mapClassName;

	IEntityClass*          m_pDefaultClass;

	ISystem*               m_pSystem;
	ClassNameMap::iterator m_currentMapIterator;

	typedef CListenerSet<IEntityClassRegistryListener*> TListenerSet;
	TListenerSet m_listeners;
};

#endif // __EntityClassRegistry_h__
