// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "EntityClassRegistry.h"
#include "EntityClass.h"
#include "EntityScript.h"
#include <CrySystem/File/CryFile.h>
#include <CrySchematyc/CoreAPI.h>

struct SSchematycEntityClassProperties
{
	SSchematycEntityClassProperties()
		: icon("%EDITOR%/objecticons/schematyc.bmp")
		, bHideInEditor(false)
		, bTriggerAreas(true)
	{}

	void Serialize(Serialization::IArchive& archive)
	{
		archive(Serialization::ObjectIconPath(icon), "icon", "Icon");
		archive.doc("Icon");
		archive(bHideInEditor, "bHideInEditor", "Hide In Editor");
		archive.doc("Hide entity class in editor");
		archive(bTriggerAreas, "bTriggerAreas", "Trigger Areas");
		archive.doc("Entity can enter and trigger areas");
	}

	static void ReflectType(Schematyc::CTypeDesc<SSchematycEntityClassProperties>& desc)
	{
		desc.SetGUID("cb7311ea-9a07-490a-a351-25c298a91550"_cry_guid);
	}

	// class properties members
	string icon;
	bool   bHideInEditor;
	bool   bTriggerAreas;
};

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

	CryGUID guid = pClass->GetGUID();
	if (!guid.IsNull())
	{
		m_mapClassGUIDs[guid] = pClass;
	}

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
		m_mapClassGUIDs.erase(pClass->GetGUID());
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
IEntityClass* CEntityClassRegistry::FindClassByGUID(const CryGUID& guid) const
{
	auto it = m_mapClassGUIDs.find(guid);
	if (it == m_mapClassGUIDs.end())
		return nullptr;
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

	pClass->SetClassDesc(entityClassDesc);

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
			EntityWarning("EntityScript %s failed to initialize", entityClassDesc.sScriptFile.c_str());
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
bool CEntityClassRegistry::UnregisterStdClass(const CryGUID& classGUID)
{
	IEntityClass* pClass = FindClassByGUID(classGUID);
	if (pClass)
	{
		return UnregisterEntityClass(pClass);
	}
	return false;
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
	stdClass.sName = "Entity";
	stdClass.flags |= ECLF_INVISIBLE;
	m_pDefaultClass = RegisterStdClass(stdClass);

	// Same as empty entity above, kept for legacy reasons
	SEntityClassDesc stdFlowgraphClass;
	stdFlowgraphClass.flags |= ECLF_INVISIBLE;
	stdFlowgraphClass.sName = "FlowgraphEntity";
	stdFlowgraphClass.editorClassInfo.sIcon = "FlowgraphEntity.bmp";
	RegisterStdClass(stdFlowgraphClass);

	RegisterSchematycEntityClass();
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::LoadClasses(const char* szFilename, bool bOnlyNewClasses)
{
	// Skip if file does not exist
	if (!gEnv->pCryPak->IsFileExist(szFilename))
		return;

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

class CSchematycEntityClassPreviewer : public Schematyc::IObjectPreviewer
{
public:
	CSchematycEntityClassPreviewer()
	{
	}

	//~IObjectPreviewer
	virtual void SerializeProperties(Serialization::IArchive& archive) override
	{
		Schematyc::IObject* pObject = gEnv->pSchematyc->GetObject(m_objectId);
		if (pObject && pObject->GetEntity())
		{
			auto visitor = [&](IEntityComponent* pComponent)
			{
				if (pComponent->GetPreviewer())
				{
					pComponent->GetPreviewer()->SerializeProperties(archive);
				}
			};
			pObject->GetEntity()->VisitComponents(visitor);
		}
	};

	virtual Schematyc::ObjectId CreateObject(const CryGUID& classGUID) const override
	{
		IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClassByGUID(classGUID);
		if (pEntityClass)
		{
			// Spawn entity for preview
			SEntitySpawnParams params;
			params.pClass = pEntityClass;
			params.sName = "Schematyc Preview Entity";
			params.nFlagsExtended |= ENTITY_FLAG_EXTENDED_PREVIEW;
			IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(params);
			if (pEntity && pEntity->GetSchematycObject())
			{
				m_objectId = pEntity->GetSchematycObject()->GetId();
			}
		}
		return m_objectId;
	};

	virtual Schematyc::ObjectId ResetObject(Schematyc::ObjectId objectId) const override
	{
		Schematyc::IObject* pObject = gEnv->pSchematyc->GetObject(objectId);
		if (pObject)
		{
			if (!pObject->SetSimulationMode(Schematyc::ESimulationMode::Preview, Schematyc::EObjectSimulationUpdatePolicy::Always, false))
			{
				CRY_ASSERT_MESSAGE(0, "Failed to reset Schematyc Preview.");
				DestroyObject(m_objectId);
			}
		}

		return m_objectId;
	}

	virtual void DestroyObject(Schematyc::ObjectId objectId) const override
	{
		Schematyc::IObject* pObject = gEnv->pSchematyc->GetObject(objectId);
		if (!pObject)
			return;
		if (pObject->GetEntity())
		{
			gEnv->pEntitySystem->RemoveEntity(pObject->GetEntity()->GetId());
		}
		m_objectId = Schematyc::ObjectId::Invalid;
	};

	virtual Sphere GetObjectBounds(Schematyc::ObjectId objectId) const override
	{
		Schematyc::IObject* pObject = gEnv->pSchematyc->GetObject(objectId);
		if (pObject && pObject->GetEntity())
		{
			AABB bbox;
			pObject->GetEntity()->GetLocalBounds(bbox);
			return Sphere(bbox.GetCenter(), bbox.GetRadius());
		}
		return Sphere(Vec3(0, 0, 0), 1.f);
	};

	virtual void RenderObject(const Schematyc::IObject& object, const SRendParams& params, const SRenderingPassInfo& passInfo) const override
	{
		IEntity* pEntity = object.GetEntity();
		if (pEntity)
		{
			SGeometryDebugDrawInfo debugDrawInfo;
			SEntityPreviewContext ctx(debugDrawInfo);
			ctx.pPassInfo = &passInfo;
			ctx.pRenderParams = &params;
			pEntity->PreviewRender(ctx);
		}
	}
	//~IObjectPreviewer

private:
	mutable Schematyc::ObjectId m_objectId = Schematyc::ObjectId::Invalid;
};

static constexpr CryGUID EntityModuleGUID = "{FB618A28-5A7A-4940-8424-D34A9B034155}"_cry_guid;
static constexpr CryGUID EntityPackageGUID = "{FAA7837E-1310-454D-808B-99BDDC6954A7}"_cry_guid;

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::RegisterSchematycEntityClass()
{
	assert(gEnv->pSchematyc);
	if (!gEnv->pSchematyc)
		return;

	auto staticAutoRegisterLambda = [](Schematyc::IEnvRegistrar& registrar)
	{
		registrar.RootScope().Register(SCHEMATYC_MAKE_ENV_MODULE(EntityModuleGUID, "EntitySystem"));

		Schematyc::CEnvRegistrationScope scope = registrar.Scope(EntityModuleGUID);
		{
			auto pClass = std::make_shared<Schematyc::CEnvClass>(IEntity::GetEntityScopeGUID(), "Entity", SCHEMATYC_SOURCE_FILE_INFO);
			pClass->SetProperties(SSchematycEntityClassProperties());
			pClass->SetPreviewer(CSchematycEntityClassPreviewer());
			// Add EntityUtilsComponent by default. It contains things like EntityId
			pClass->AddComponent("e88093df-904f-4c52-af38-911e26777cdc"_cry_guid);
			scope.Register(pClass);
		}
	};

	gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(
	  stl::make_unique<Schematyc::CEnvPackage>(
		EntityPackageGUID,
	    "EntitySystem",
	    "Crytek GmbH",
	    "CRYENGINE EntitySystem Package",
	    staticAutoRegisterLambda
	    )
	  );

	// TODO : Can we filter by class guid?
	gEnv->pSchematyc->GetCompiler().GetClassCompilationSignalSlots().Connect(
	  [this](const Schematyc::IRuntimeClass& runtimeClass) { this->OnSchematycClassCompilation(runtimeClass); },
	  m_connectionScope);
}

//////////////////////////////////////////////////////////////////////////
void CEntityClassRegistry::OnSchematycClassCompilation(const Schematyc::IRuntimeClass& runtimeClass)
{
	if (runtimeClass.GetEnvClassGUID() == IEntity::GetEntityScopeGUID())
	{
		string className = "Schematyc";
		className.append("::");
		className.append(runtimeClass.GetName());

		className.MakeLower();

		bool bModifyExisting = false;

		IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
		if (pEntityClass)
		{
			if (pEntityClass->GetGUID() != runtimeClass.GetGUID())
			{
				SCHEMATYC_ENV_CRITICAL_ERROR("Duplicate entity class name '%s'!", className.c_str());
				return;
			}
		}

		Schematyc::CAnyConstPtr pEnvClassProperties = runtimeClass.GetEnvClassProperties();
		SCHEMATYC_ENV_ASSERT(pEnvClassProperties);
		if (pEnvClassProperties)
		{
			const SSchematycEntityClassProperties& classProperties = Schematyc::DynamicCast<SSchematycEntityClassProperties>(*pEnvClassProperties);

			IEntityClassRegistry::SEntityClassDesc entityClassDesc;

			entityClassDesc.sName = className;
			entityClassDesc.guid = runtimeClass.GetGUID();
			entityClassDesc.schematycRuntimeClassGuid = runtimeClass.GetGUID();

			string icon = classProperties.icon;

			const string::size_type iconFileNamePos = icon.find_last_of("/\\");
			if (iconFileNamePos != string::npos)
			{
				icon.erase(0, iconFileNamePos + 1);
			}

			entityClassDesc.flags = ECLF_BBOX_SELECTION;
			entityClassDesc.flags |= ECLF_MODIFY_EXISTING;

			if (classProperties.bHideInEditor)
			{
				entityClassDesc.flags |= ECLF_INVISIBLE;
			}

			entityClassDesc.editorClassInfo.sCategory = "Schematyc";
			entityClassDesc.editorClassInfo.sIcon = icon.c_str();
			gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(entityClassDesc);
		}
	}

	const Schematyc::IEnvClass* pEnvClass = gEnv->pSchematyc->GetEnvRegistry().GetClass(runtimeClass.GetEnvClassGUID());
	if (!pEnvClass)
	{
		//pEnvClass->SetPreviewer();
	}
}

void CEntityClassRegistry::UnregisterSchematycEntityClass()
{
	if (gEnv->pSchematyc)
	{
		gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(EntityPackageGUID);
	}
}
