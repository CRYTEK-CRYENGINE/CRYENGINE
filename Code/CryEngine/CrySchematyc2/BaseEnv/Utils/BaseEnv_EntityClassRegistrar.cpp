// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/Utils/BaseEnv_EntityClassRegistrar.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntityClass.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryRenderer/IRenderer.h>
#include <GameObjects/GameObject.h>
#include <GameObjects/GameObjectSystem.h>

#include "BaseEnv/BaseEnv_BaseEnv.h"
#include "CrySchematyc2/IEntityAttributesProxy.h"
#include "BaseEnv/Utils/BaseEnv_EntityFoundationProperties.h"
#include "BaseEnv/Utils/BaseEnv_EntityGameObjectExtension.h"
#include "BaseEnv/Utils/BaseEnv_EntityMap.h"
#include "BaseEnv/Utils/BaseEnv_EntityUserData.h"

namespace SchematycBaseEnv
{
	class CGameObjectExtensionCreator : public IGameObjectExtensionCreatorBase
	{
	public:

		// IGameObjectExtensionCreatorBase
			
		virtual IGameObjectExtension* Create(IEntity *pEntity)
		{
			return pEntity->GetOrCreateComponentClass<CEntityGameObjectExtension>();
		}
			
		virtual void GetGameObjectExtensionRMIData(void** ppRMI, size_t* pCount)
		{
			CEntityGameObjectExtension::GetGameObjectExtensionRMIData(ppRMI, pCount);
		}

		// ~IGameObjectExtensionCreatorBase
	};

	class CEntityFoundationPreviewExtension : public Schematyc2::IFoundationPreviewExtension
	{
	public:

		inline CEntityFoundationPreviewExtension()
			: m_entityId(0)
		{}

		// IFoundationPreviewExtension

		virtual Schematyc2::IObject* BeginPreview(const Schematyc2::SGUID& classGUID, EntityId selectedEntityId) override
		{
			Schematyc2::IObject*          pObject = nullptr;
			Schematyc2::ILibClassConstPtr pLibClass = gEnv->pSchematyc2->GetLibRegistry().GetClass(classGUID);
			if(pLibClass)
			{
				IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pLibClass->GetName());
				if(pEntityClass)
				{
					static const SEntityUserData userData(true, Schematyc2::EObjectFlags::None);
					SEntitySpawnParams           entitySpawnParams;
					entitySpawnParams.pClass    = pEntityClass;
					entitySpawnParams.sName     = SCHEMATYC2_PREVIEW_ENTITY_NAME;
					entitySpawnParams.pUserData = const_cast<SEntityUserData*>(&userData);
					IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(entitySpawnParams, true);
					if(pEntity)
					{
						m_entityId = pEntity->GetId();
						pObject    = CBaseEnv::GetInstance().GetGameEntityMap().FindObject(m_entityId);
					}
					IEntity* pSelectedEntity = gEnv->pEntitySystem->GetEntity(selectedEntityId);
					if(pSelectedEntity)
					{
						SEntityEvent entityEvent;
						entityEvent.event = ENTITY_EVENT_RESET;
						pSelectedEntity->SendEvent(entityEvent);
					}
				}
			}
			return pObject;
		}
			
		virtual void RenderPreview(const SRendParams& params, const SRenderingPassInfo& passInfo) override
		{
			if(m_entityId)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
				if(pEntity)
				{
					if(pEntity->GetRenderNode())
					{
						pEntity->GetRenderNode()->Render(params, passInfo);
					}
					gEnv->pRenderer->GetIRenderAuxGeom()->Submit();
				}
			}
		}
			
		virtual void EndPreview() override
		{
			if(m_entityId)
			{
				gEnv->pEntitySystem->RemoveEntity(m_entityId);
				m_entityId = 0;
			}
		}

		// ~IFoundationPreviewExtension

	private:

		EntityId m_entityId;
	};

	DECLARE_SHARED_POINTERS(CEntityFoundationPreviewExtension)

	IEntityComponent* CreateGameObjectEntityProxy(IEntity* pEntity, SEntitySpawnParams& spawnParams, void* pUserData)
	{
		IGameObject*    pGameObject = nullptr;
		IEntityComponent* pEntityProxy = gEnv->pGameFramework->GetIGameObjectSystem()->CreateGameObjectEntityProxy(*pEntity, &pGameObject);
		CRY_ASSERT(pGameObject);
		if(pGameObject)
		{
			static bool bHasRegistered = false;
			if (!bHasRegistered)
			{
				bHasRegistered = true;

				static CGameObjectExtensionCreator s_gameObjectExtensionCreator;
				gEnv->pGameFramework->GetIGameObjectSystem()->RegisterExtension(CEntityGameObjectExtension::s_szExtensionName, &s_gameObjectExtensionCreator, nullptr);
			}

			if(pGameObject->ActivateExtension(CEntityGameObjectExtension::s_szExtensionName))
			{
				pGameObject->SetUserData(spawnParams.pUserData);
				return pEntityProxy;
			}
		}
		return nullptr;
	}

	static const Schematyc2::SGUID s_entityFoundationGUID = "be845278-0dd2-409f-b8be-97895607c256";

	void CEntityClassRegistrar::Init()
	{
		gEnv->pSchematyc2->GetLibRegistry().Signals().classRegistration.Connect(Schematyc2::LibClassRegistrationSignal::Delegate::FromMemberFunction<CEntityClassRegistrar, &CEntityClassRegistrar::OnClassRegistration>(*this), m_connectionScope); // TODO : Can we filter by foundation guid?
	}

	void CEntityClassRegistrar::Refresh()
	{
		Schematyc2::IFoundationPtr pEntityFoundation = gEnv->pSchematyc2->GetEnvRegistry().CreateFoundation(s_entityFoundationGUID, "Entity");
		CRY_ASSERT(pEntityFoundation);
		if(pEntityFoundation)
		{
			pEntityFoundation->SetProperties(Schematyc2::Properties::MakeShared<SEntityFoundationProperties>());
			pEntityFoundation->UseNamespace("Base");
			pEntityFoundation->UseNamespace("Utils");
			//pEntityFoundation->UseNamespace("Types");
			// TODO: Workaround until recursion works.
			pEntityFoundation->UseNamespace("Types::Bool");
			pEntityFoundation->UseNamespace("Types::Int32");
			pEntityFoundation->UseNamespace("Types::UInt32");
			pEntityFoundation->UseNamespace("Types::Float");
			pEntityFoundation->UseNamespace("Types::Vector2");
			pEntityFoundation->UseNamespace("Types::Vector3");
			pEntityFoundation->UseNamespace("Types::Angle2");
			pEntityFoundation->UseNamespace("Types::QRotation");
			pEntityFoundation->UseNamespace("Types::String");
			// ~TODO
			pEntityFoundation->AddExtension<Schematyc2::IFoundationPreviewExtension>(CEntityFoundationPreviewExtensionPtr(std::make_shared<CEntityFoundationPreviewExtension>()));
		}
	}

	CEntityClassRegistrar::SEntityClass::SEntityClass()
	{
		// N.B. This is a quick work around for the problem of entities without script tables not triggering areas.
		desc.pScriptTable = gEnv->pScriptSystem->CreateTable();
		desc.pScriptTable->AddRef();
	}

	CEntityClassRegistrar::SEntityClass::SEntityClass(const SEntityClass& rhs)
		: editorCategory(rhs.editorCategory)
		, icon(rhs.icon)
		, desc(rhs.desc)
	{
		desc.pScriptTable->AddRef();
	}

	CEntityClassRegistrar::SEntityClass::~SEntityClass()
	{
		desc.pScriptTable->Release();
	}

	void CEntityClassRegistrar::RegisterEntityClass(const Schematyc2::ILibClass& libClass, SEntityClass& entityClass, bool bNewEntityClass)
	{
		Schematyc2::IPropertiesConstPtr pFoundationProperties = libClass.GetFoundationProperties();
		CRY_ASSERT(pFoundationProperties);
		if(pFoundationProperties)
		{
			const SEntityFoundationProperties& foundationProperties = *pFoundationProperties->ToPtr<SEntityFoundationProperties>();

			entityClass.libClassGUID = libClass.GetGUID();

			entityClass.editorCategory = "Schematyc";
			entityClass.editorCategory.append("/");
			//entityClass.editorCategory.append(libClass.GetLib().GetName());
			entityClass.editorCategory.append(libClass.GetName());
			string::size_type	fileNamePos = entityClass.editorCategory.find_last_of("/");
			if(fileNamePos != string::npos)
			{
				entityClass.editorCategory.erase(fileNamePos);
			}
			entityClass.editorCategory.replace("::", "/");

			entityClass.icon = foundationProperties.icon;
			const string::size_type	iconFileNamePos = entityClass.icon.find_last_of("/\\");
			if(iconFileNamePos != string::npos)
			{
				entityClass.icon.erase(0, iconFileNamePos + 1);
			}

			entityClass.desc.flags = ECLF_BBOX_SELECTION;
			if(foundationProperties.bHideInEditor)
			{
				entityClass.desc.flags |= ECLF_INVISIBLE;
			}
			if(!bNewEntityClass)
			{
				entityClass.desc.flags |= ECLF_MODIFY_EXISTING;
			}

			entityClass.desc.sName                     = libClass.GetName();
			entityClass.desc.editorClassInfo.sCategory = entityClass.editorCategory.c_str();
			entityClass.desc.editorClassInfo.sIcon     = entityClass.icon.c_str();
			entityClass.desc.pUserProxyCreateFunc      = &CreateGameObjectEntityProxy;

			// NOTE pavloi 2016.11.25: CEntityGameObjectExtension::Init expects to find this pointer to SEntityClass in the pUserProxyData
			entityClass.desc.pUserProxyData            = &entityClass;

			gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(entityClass.desc);
		}
	}

	void CEntityClassRegistrar::OnClassRegistration(const Schematyc2::ILibClassConstPtr& pLibClass)
	{
		const Schematyc2::SGUID foundationGUID = pLibClass->GetFoundationGUID();
		if(foundationGUID == s_entityFoundationGUID)
		{
			const char*             szEntityClassName = pLibClass->GetName();
			EntityClasses::iterator itEntityClass = m_entityClasses.find(szEntityClassName);
			if(itEntityClass == m_entityClasses.end())
			{
				RegisterEntityClass(*pLibClass, m_entityClasses[szEntityClassName], true);
			}
			else
			{
				RegisterEntityClass(*pLibClass, itEntityClass->second, false);
			}
		}
	}
}
