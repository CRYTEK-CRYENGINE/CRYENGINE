// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   EntityClassRegistry.cpp
//  Version:     v1.00
//  Created:     3/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EntityClassRegistry.h"
#include "EntityClass.h"
#include "EntityScript.h"
#include <CrySystem/File/CryFile.h>

//////////////////////////////////////////////////////////////////////////
CEntityClassRegistry::CEntityClassRegistry()
	: m_pDefaultClass(nullptr)
	, m_listeners(2)
{
	m_pSystem = GetISystem();
}

//////////////////////////////////////////////////////////////////////////
CEntityClassRegistry::~CEntityClassRegistry()
{
}

//////////////////////////////////////////////////////////////////////////
bool CEntityClassRegistry::RegisterEntityClass(IEntityClass* pClass)
{
	assert(pClass != NULL);

	bool newClass = false;
	if ((pClass->GetFlags() & ECLF_MODIFY_EXISTING) == 0)
	{
		IEntityClass* pOldClass = FindClass(pClass->GetName());
		if (pOldClass)
		{
			EntityWarning("CEntityClassRegistry::RegisterEntityClass failed, class with name %s already registered",
			              pOldClass->GetName());
			return false;
		}
		newClass = true;
	}
	m_mapClassName[pClass->GetName()] = pClass;
	NotifyListeners(newClass ? ECRE_CLASS_REGISTERED : ECRE_CLASS_MODIFIED, pClass);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityClassRegistry::UnregisterEntityClass(IEntityClass* pClass)
{
	assert(pClass != NULL);
	if (FindClass(pClass->GetName()))
	{
		m_mapClassName.erase(pClass->GetName());
		NotifyListeners(ECRE_CLASS_UNREGISTERED, pClass);
		pClass->Release();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
IEntityClass* CEntityClassRegistry::FindClass(const char* sClassName) const
{
	ClassNameMap::const_iterator it = m_mapClassName.find(CONST_TEMP_STRING(sClassName));

	if (it == m_mapClassName.end())
		return 0;

	return it->second;
}

//////////////////////////////////////////////////////////////////////////
IEntityClass* CEntityClassRegistry::GetDefaultClass() const
{
	return m_pDefaultClass;
}

//////////////////////////////////////////////////////////////////////////
IEntityClass* CEntityClassRegistry::RegisterStdClass(const SEntityClassDesc& entityClassDesc)
{
	// Creates a new entity class.
	CEntityClass* pClass = new CEntityClass;
	pClass->SetName(entityClassDesc.sName);
	pClass->SetFlags(entityClassDesc.flags);
	pClass->SetScriptFile(entityClassDesc.sScriptFile);
	pClass->SetUserProxyCreateFunc(entityClassDesc.pUserProxyCreateFunc, entityClassDesc.pUserProxyData);
	pClass->SetPropertyHandler(entityClassDesc.pPropertyHandler);
	pClass->SetEventHandler(entityClassDesc.pEventHandler);
	pClass->SetScriptFileHandler(entityClassDesc.pScriptFileHandler);
	pClass->SetEditorClassInfo(entityClassDesc.editorClassInfo);
	pClass->SetClassAttributes(entityClassDesc.classAttributes);
	pClass->SetEntityAttributes(entityClassDesc.entityAttributes);

	// Check if need to create entity script.
	if (entityClassDesc.sScriptFile[0] || entityClassDesc.pScriptTable)
	{
		// Create a new entity script.
		CEntityScript* pScript = new CEntityScript;
		bool ok = false;
		if (entityClassDesc.sScriptFile[0])
			ok = pScript->Init(entityClassDesc.sName, entityClassDesc.sScriptFile);
		else
			ok = pScript->Init(entityClassDesc.sName, entityClassDesc.pScriptTable);

		if (!ok)
		{
			EntityWarning("EntityScript %s failed to initialize", entityClassDesc.sScriptFile);
			pScript->Release();
			pClass->Release();
			return NULL;
		}
		pClass->SetEntityScript(pScript);
	}

	if (!RegisterEntityClass(pClass))
	{
		// Register class failed.
		pClass->Release();
		return NULL;
	}
	return pClass;
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::RegisterListener(IEntityClassRegistryListener* pListener)
{
	if ((pListener != NULL) && (pListener->m_pRegistry == NULL))
	{
		m_listeners.Add(pListener);
		pListener->m_pRegistry = this;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::UnregisterListener(IEntityClassRegistryListener* pListener)
{
	if ((pListener != NULL) && (pListener->m_pRegistry == this))
	{
		m_listeners.Remove(pListener);
		pListener->m_pRegistry = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::IteratorMoveFirst()
{
	m_currentMapIterator = m_mapClassName.begin();
}

//////////////////////////////////////////////////////////////////////////
IEntityClass* CEntityClassRegistry::IteratorNext()
{
	IEntityClass* pClass = NULL;
	if (m_currentMapIterator != m_mapClassName.end())
	{
		pClass = m_currentMapIterator->second;
		++m_currentMapIterator;
	}
	return pClass;
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::InitializeDefaultClasses()
{
	LoadClasses("entities.xml");

	LoadArchetypes("Libs/EntityArchetypes", false);

	SEntityClassDesc stdClass;
	stdClass.flags |= ECLF_INVISIBLE | ECLF_DEFAULT;
	stdClass.sName = "Default";
	m_pDefaultClass = RegisterStdClass(stdClass);

	SEntityClassDesc stdGeomClass;
	stdGeomClass.flags |= ECLF_INVISIBLE | ECLF_DEFAULT;
	stdGeomClass.sName = "GeomEntity";
	stdGeomClass.sScriptFile = "Scripts/Entities/Default/GeomEntity.lua";
	RegisterStdClass(stdGeomClass);

	SEntityClassDesc stdRopeClass;
	stdRopeClass.flags |= ECLF_INVISIBLE | ECLF_DEFAULT;
	stdRopeClass.sName = "RopeEntity";
	stdRopeClass.sScriptFile = "Scripts/Entities/Default/RopeEntity.lua";
	RegisterStdClass(stdRopeClass);

	SEntityClassDesc stdFlowgraphClass;
	stdFlowgraphClass.flags |= ECLF_DEFAULT;
	stdFlowgraphClass.sName = "FlowgraphEntity";
	stdFlowgraphClass.editorClassInfo.sIcon = "FlowgraphEntity.bmp";
	RegisterStdClass(stdFlowgraphClass);

	SEntityClassDesc stdAudioListenerClass;
	stdAudioListenerClass.flags |= ECLF_INVISIBLE | ECLF_DEFAULT;
	stdAudioListenerClass.sName = "AudioListener";
	RegisterStdClass(stdAudioListenerClass);
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::LoadClasses(const char* szFilename, bool bOnlyNewClasses)
{
	const XmlNodeRef root = m_pSystem->LoadXmlFromFile(szFilename);

	if (root && root->isTag("Entities"))
	{
		int childCount = root->getChildCount();
		for (int i = 0; i < childCount; ++i)
		{
			LoadClassDescription(root->getChild(i), bOnlyNewClasses);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::LoadArchetypes(const char* libPath, bool reload)
{
	if (reload)
	{
		std::vector<IEntityClass*> archetypeClasses;
		archetypeClasses.reserve(m_mapClassName.size());

		ClassNameMap::const_iterator it = m_mapClassName.begin();
		ClassNameMap::const_iterator end = m_mapClassName.end();
		for (; it != end; ++it)
		{
			if (it->second->GetFlags() & ECLF_ENTITY_ARCHETYPE)
			{
				archetypeClasses.push_back(it->second);
			}
		}

		for (IEntityClass* pArchetype : archetypeClasses)
		{
			UnregisterEntityClass(pArchetype);
		}
	}

	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	char filename[_MAX_PATH];

	string sPath = libPath;
	sPath.TrimRight("/\\");
	string sSearch = sPath + "/*.xml";
	intptr_t handle = pCryPak->FindFirst(sSearch, &fd, 0);
	if (handle != -1)
	{
		int res = 0;
		do
		{
			// Animation file found, load it.
			cry_strcpy(filename, sPath);
			cry_strcat(filename, "/");
			cry_strcat(filename, fd.name);

			// Load xml file.
			XmlNodeRef root = m_pSystem->LoadXmlFromFile(filename);
			if (root)
			{
				LoadArchetypeDescription(root);
			}

			res = pCryPak->FindNext(handle, &fd);
		}
		while (res >= 0);
		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::LoadArchetypeDescription(const XmlNodeRef& root)
{
	for (int i = 0, childCount = root->getChildCount(); i < childCount; ++i)
	{
		XmlNodeRef child = root->getChild(i);
		if (child->isTag("EntityPrototype"))
		{
			string lib = child->getAttr("Library");
			string name = child->getAttr("Name");
			if (lib.empty() || name.empty())
			{
				return;
			}

			string fullName = lib + "." + name;

			IEntityClass* pClass = FindClass(fullName.c_str());
			if (!pClass)
			{
				SEntityClassDesc cd;
				cd.flags = ECLF_INVISIBLE | ECLF_ENTITY_ARCHETYPE;
				cd.sName = fullName.c_str();
				RegisterStdClass(cd);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::LoadClassDescription(const XmlNodeRef& root, bool bOnlyNewClasses)
{
	assert(root != (IXmlNode*)NULL);
	if (root->isTag("Entity"))
	{
		const char* sName = root->getAttr("Name");
		if (*sName == 0)
			return; // Empty name.

		const char* sScript = root->getAttr("Script");

		IEntityClass* pClass = FindClass(sName);
		if (!pClass)
		{
			// New class.
			SEntityClassDesc cd;
			cd.flags = 0;
			cd.sName = sName;
			cd.sScriptFile = sScript;

			bool bInvisible = false;
			if (root->getAttr("Invisible", bInvisible))
			{
				if (bInvisible)
					cd.flags |= ECLF_INVISIBLE;
			}

			bool bBBoxSelection = false;
			if (root->getAttr("BBoxSelection", bBBoxSelection))
			{
				if (bBBoxSelection)
					cd.flags |= ECLF_BBOX_SELECTION;
			}

			RegisterStdClass(cd);
		}
		else
		{
			// This class already registered.
			if (!bOnlyNewClasses)
			{
				EntityWarning("[CEntityClassRegistry] LoadClassDescription failed, Entity Class name %s already registered", sName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::NotifyListeners(EEntityClassRegistryEvent event, const IEntityClass* pEntityClass)
{
	CRY_ASSERT(m_listeners.IsNotifying() == false);
	for (TListenerSet::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		(*notifier)->OnEntityClassRegistryEvent(event, pEntityClass);
	}
}
