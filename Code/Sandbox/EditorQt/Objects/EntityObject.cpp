// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityObject.h"

#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include "Group.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAgent.h>
#include <CryMovie/IMovieSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IVertexAnimation.h>

#include <Cry3DEngine/IIndexedMesh.h>
#include <CryEntitySystem/IBreakableManager.h>

#include "EntityPrototype.h"
#include "Material/MaterialManager.h"
#include "Gizmos/AxisHelper.h"
#include "Dialogs/QStringDialog.h"
#include "Dialogs/GenericSelectItemDialog.h"

#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphHelpers.h"

#include "BrushObject.h"
#include "GameEngine.h"

#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>

#include "Viewport.h"
#include "ViewManager.h"
#include "IIconManager.h"
#include "Dialogs/CustomColorDialog.h"
#include "Util/MFCUtil.h"

#include <CrySerialization/Serializer.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/DynArray.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/yasli/decorators/HorizontalLine.h>
#include "Serialization/Decorators/EditorActionButton.h"
#include <Serialization/Decorators/EntityLink.h>
#include <Serialization/Decorators/EditToolButton.h>

#include "Controls/PropertyItem.h"
#include "Controls/DynamicPopupMenu.h"
#include "Util/BoostPythonHelpers.h"
#include "Objects/ObjectLayer.h"
#include "Objects/DisplayContext.h"
#include "Objects/ObjectLoader.h"
#include "Objects/ObjectPropertyWidget.h"
#include "Objects/InspectorWidgetCreator.h"

#include <Objects/EntityComponentCollapsibleFrame.h>

#include <CryRenderer/IFlares.h>
#include "LensFlareEditor/LensFlareManager.h"
#include "LensFlareEditor/LensFlareUtil.h"
#include "LensFlareEditor/LensFlareItem.h"
#include "LensFlareEditor/LensFlareLibrary.h"
#include "AI/AIManager.h"

#include "Prefabs\PrefabManager.h"
#include "Prefabs\PrefabLibrary.h"
#include "Prefabs\PrefabItem.h"

#include "UndoEntityProperty.h"
#include "UndoEntityParam.h"

#include "PickObjectTool.h"

#include <Cry3DEngine/IGeomCache.h>

#include <CrySandbox/IEditorGame.h>
#include <CrySystem/ICryLink.h>
#include "CryEditDoc.h"
#include "Util/MFCUtil.h"

#include <CryExtension/ICryFactoryRegistry.h>

#include <CrySchematyc/CoreAPI.h>
#include <CrySchematyc/IObjectProperties.h>
#include <CrySchematyc/Utils/ClassProperties.h>

#include <IGameObjectSystem.h>

#include <QToolButton.h>
#include <QCollapsibleFrame.h>
#include <Controls/DictionaryWidget.h>

REGISTER_CLASS_DESC(CEntityClassDesc);

const char* CEntityObject::s_LensFlarePropertyName("flare_Flare");
const char* CEntityObject::s_LensFlareMaterialName("%ENGINE%/EngineAssets/Materials/lens_optics");

namespace Private_EntityObject
{

//////////////////////////////////////////////////////////////////////////
//! Undo object for attach/detach changes
class CUndoAttachEntity : public IUndoObject
{
public:
	CUndoAttachEntity(CEntityObject* pAttachedObject, bool bAttach)
		: m_attachedEntityGUID(pAttachedObject->GetId())
		, m_attachmentTarget(pAttachedObject->GetAttachTarget())
		, m_attachmentType(pAttachedObject->GetAttachType())
		, m_bAttach(bAttach)
	{}

	virtual void Undo(bool bUndo) override
	{
		if (!m_bAttach) SetAttachmentTypeAndTarget();
	}

	virtual void Redo() override
	{
		if (m_bAttach) SetAttachmentTypeAndTarget();
	}

private:
	void SetAttachmentTypeAndTarget()
	{
		CObjectManager* pObjectManager = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager());
		CEntityObject* pEntity = static_cast<CEntityObject*>(pObjectManager->FindObject(m_attachedEntityGUID));

		if (pEntity)
		{
			pEntity->SetAttachType(m_attachmentType);
			pEntity->SetAttachTarget(m_attachmentTarget);
		}
	}

	virtual const char* GetDescription() { return "Attachment Changed"; }

	CryGUID                        m_attachedEntityGUID;
	CEntityObject::EAttachmentType m_attachmentType;
	string                         m_attachmentTarget;
	bool                           m_bAttach;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for adding an entity component
class CUndoAddComponent : public IUndoObject
{
	struct SComponentInstanceInfo
	{
		CryGUID owningEntityGUID;
		CryGUID instanceGUID;
	};

public:
	CUndoAddComponent(const CEntityComponentClassDesc* pClassDescription, size_t numExpectedEntities)
		: m_pClassDescription(pClassDescription)
	{
		m_affectedEntityInfo.reserve(numExpectedEntities);
	}

	virtual void Undo(bool bUndo) override
	{
		for (const SComponentInstanceInfo& componentInstanceInfo : m_affectedEntityInfo)
		{
			CEntityObject* pEntityObject = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(componentInstanceInfo.owningEntityGUID));
			if (pEntityObject == nullptr)
				continue;

			IEntity* pEntity = pEntityObject->GetIEntity();
			if (pEntity == nullptr)
				continue;

			IEntityComponent* pComponent = pEntity->GetComponentByGUID(componentInstanceInfo.instanceGUID);
			if (pComponent == nullptr)
				continue;

			pEntity->RemoveComponent(pComponent);
		}

		GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
	}

	virtual void Redo() override
	{
		for (SComponentInstanceInfo& componentInstanceInfo : m_affectedEntityInfo)
		{
			CEntityObject* pEntityObject = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(componentInstanceInfo.owningEntityGUID));
			if (pEntityObject == nullptr)
				continue;

			IEntity* pEntity = pEntityObject->GetIEntity();
			if (pEntity == nullptr)
				continue;

			componentInstanceInfo.instanceGUID = AddComponentInternal(pEntity);
		}

		GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
	}

	void AddComponent(IEntity* pEntity)
	{
		const CryGUID& componentInstanceGUID = AddComponentInternal(pEntity);
		if (!componentInstanceGUID.IsNull())
		{
			m_affectedEntityInfo.emplace_back(SComponentInstanceInfo { pEntity->GetGuid(), componentInstanceGUID });
		}
	}

	bool IsValid() const { return m_affectedEntityInfo.size() > 0; }

private:
	const CryGUID& AddComponentInternal(IEntity* pEntity)
	{
		EntityComponentFlags flags = m_pClassDescription->GetComponentFlags();
		flags.Add(EEntityComponentFlags::UserAdded);
		IEntityComponent::SInitParams initParams(pEntity, CryGUID::Create(), "", m_pClassDescription, flags, nullptr, nullptr);
		IF_LIKELY (IEntityComponent* pComponent = pEntity->CreateComponentByInterfaceID(m_pClassDescription->GetGUID(), &initParams))
		{
			return pComponent->GetGUID();
		}

		static CryGUID invalidGUID = CryGUID::Null();
		return invalidGUID;
	}

	virtual const char* GetDescription() { return "Added Entity Component"; }

	const CEntityComponentClassDesc*    m_pClassDescription;
	std::vector<SComponentInstanceInfo> m_affectedEntityInfo;
};

class EntityLinkTool : public CPickObjectTool
{
	DECLARE_DYNCREATE(EntityLinkTool)

	struct SEntityLinkPicker : IPickObjectCallback
	{
		SEntityLinkPicker()
		{
		}

		void OnPick(CBaseObject* pObj) override
		{
			if (m_owner)
			{
				CUndo undo("Add entity link");
				m_owner->AddEntityLink(pObj->GetName(), pObj->GetId());
			}
		}

		bool OnPickFilter(CBaseObject* filterObject) override
		{
			return filterObject->IsKindOf(RUNTIME_CLASS(CEntityObject));
		}

		void OnCancelPick() override
		{
		}

		CEntityObject* m_owner;
	};

public:
	EntityLinkTool()
		: CPickObjectTool(&m_picker)
	{
	}

	virtual void SetUserData(const char* key, void* userData) override
	{
		m_picker.m_owner = static_cast<CEntityObject*>(userData);
	}

private:
	SEntityLinkPicker m_picker;
};

IMPLEMENT_DYNCREATE(EntityLinkTool, CPickObjectTool)

//////////////////////////////////////////////////////////////////////////
//! Undo Entity Link
class CUndoEntityLink : public IUndoObject
{
public:
	CUndoEntityLink(const std::vector<CBaseObject*>& objects)
	{
		m_Links.reserve(objects.size());
		for (CBaseObject* pObj : objects)
		{
			if (pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				SLink link;
				link.entityID = pObj->GetId();
				link.linkXmlNode = XmlHelpers::CreateXmlNode("undo");
				((CEntityObject*)pObj)->SaveLink(link.linkXmlNode);
				m_Links.push_back(link);
			}
		}
	}

protected:
	virtual void        Release()        { delete this; }
	virtual const char* GetDescription() { return "Entity Link"; }
	virtual const char* GetObjectName()  { return ""; }

	virtual void        Undo(bool bUndo)
	{
		for (int i = 0, iLinkSize(m_Links.size()); i < iLinkSize; ++i)
		{
			SLink& link = m_Links[i];
			CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(link.entityID);
			if (pObj == NULL)
				continue;
			if (!pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
				continue;
			CEntityObject* pEntity = (CEntityObject*)pObj;
			if (link.linkXmlNode->getChildCount() == 0)
				continue;
			pEntity->LoadLink(link.linkXmlNode->getChild(0));
		}
	}
	virtual void Redo() {}

private:

	struct SLink
	{
		CryGUID    entityID;
		XmlNodeRef linkXmlNode;
	};

	std::vector<SLink> m_Links;
};

class CUndoCreateFlowGraph : public IUndoObject
{
public:
	CUndoCreateFlowGraph(CEntityObject* entity, const char* groupName)
		: m_entityGuid(entity->GetId())
		, m_groupName(groupName)
	{
	}

protected:
	virtual void Undo(bool bUndo) override
	{
		auto pEntity = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(m_entityGuid));
		if (pEntity)
		{
			pEntity->RemoveFlowGraph();
		}
	}

	virtual void Redo() override
	{
		auto pEntity = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(m_entityGuid));
		if (pEntity)
		{
			pEntity->OpenFlowGraph(m_groupName);
		}
	}

	virtual const char* GetDescription() override { return "Create Flow Graph"; }

private:
	CryGUID     m_entityGuid;
	const char* m_groupName;
};

std::map<EntityId, CEntityObject*> s_entityIdMap;

};

IMPLEMENT_DYNCREATE(CEntityObject, CBaseObject)

int CEntityObject::m_rollupId = 0;
float CEntityObject::m_helperScale = 1;

void CEntityLink::Serialize(Serialization::IArchive& ar)
{
	ar(name, "name", "Name");

	string targetName;
	if (CEntityObject* pTarget = GetTarget())
		targetName = pTarget->GetName();

	Serialization::EntityTarget target(targetId, targetName);
	ar(target, "targetName", "!Target");

	if (ar.isInput())
	{
		targetId = target.guid;

		// If we were using the default entity link name (the target object's name)
		// then enforce the new name based off the new target's name
		CEntityObject* pTarget = GetTarget();
		if (pTarget && targetName == name)
			name = pTarget->GetName();
	}
}

CEntityObject* CEntityLink::GetTarget() const
{
	return static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(targetId));
}

CEntityObject::CEntityObject() : m_listeners(1)
{
	m_pEntity = 0;
	m_bLoadFailed = false;

	m_pEntityScript = nullptr;
	m_pEntityClass = nullptr;

	m_visualObject = 0;

	m_proximityRadius = 0;
	m_innerRadius = 0;
	m_outerRadius = 0;
	m_boxSizeX = 1;
	m_boxSizeY = 1;
	m_boxSizeZ = 1;
	m_bProjectInAllDirs = false;
	m_bProjectorHasTexture = false;
	m_bInitVariablesCalled = false;

	m_bDisplayBBox = true;
	m_bBBoxSelection = false;
	m_bDisplaySolidBBox = false;
	m_bDisplayAbsoluteRadius = false;
	m_bDisplayArrow = false;
	m_entityId = 0;
	m_bVisible = true;
	m_bCalcPhysics = true;
	m_pFlowGraph = nullptr;
	m_bLight = false;
	m_bAreaLight = false;
	m_fAreaWidth = 1;
	m_fAreaHeight = 1;
	m_fAreaLightSize = 0.05f;
	m_bBoxProjectedCM = false;
	m_fBoxWidth = 1;
	m_fBoxHeight = 1;
	m_fBoxLength = 1;

	m_lightColor = Vec3(1, 1, 1);

	m_bEntityXfromValid = false;

	UseMaterialLayersMask();

	SetColor(RGB(255, 255, 0));

	// Init Variables.
	mv_castShadow = true;
	mv_castShadowMinSpec = CONFIG_LOW_SPEC;
	mv_giMode = 0;
	mv_DynamicDistanceShadows = false;
	mv_outdoor = false;
	mv_recvWind = false;
	mv_renderNearest = false;
	mv_noDecals = false;

	mv_hiddenInGame = false;
	mv_ratioLOD = 100;
	mv_ratioViewDist = 100;
	mv_ratioLOD.SetLimits(0, 255);
	mv_ratioViewDist.SetLimits(0, 255);

	m_physicsState = 0;

	m_bForceScale = false;

	m_attachmentType = eAT_Pivot;

	// Reserve some space for the icons
	m_componentIconTextureIds.reserve(4);

	m_entityDirtyFlag = UINT8_MAX;
}

void CEntityObject::InitVariables()
{
	m_bInitVariablesCalled = true;
	mv_castShadowMinSpec.AddEnumItem("Never", END_CONFIG_SPEC_ENUM);
	mv_castShadowMinSpec.AddEnumItem("Low", CONFIG_LOW_SPEC);
	mv_castShadowMinSpec.AddEnumItem("Medium", CONFIG_MEDIUM_SPEC);
	mv_castShadowMinSpec.AddEnumItem("High", CONFIG_HIGH_SPEC);
	mv_castShadowMinSpec.AddEnumItem("VeryHigh", CONFIG_VERYHIGH_SPEC);

	mv_castShadow.SetFlags(mv_castShadow.GetFlags() | IVariable::UI_INVISIBLE);
	mv_castShadowMinSpec->SetFlags(mv_castShadowMinSpec->GetFlags() | IVariable::UI_UNSORTED);

	// fill combobox depending on entity type
	if (IsLight())
	{
		mv_giMode.AddEnumItem("Disabled", IRenderNode::eGM_None);
		mv_giMode.AddEnumItem("Static light", IRenderNode::eGM_StaticVoxelization);
		mv_giMode.AddEnumItem("Dynamic light", IRenderNode::eGM_DynamicVoxelization);
		mv_giMode.AddEnumItem("Hide if GI is Active", IRenderNode::eGM_HideIfGiIsActive);
	}
	else
	{
		mv_giMode.AddEnumItem("Disabled", IRenderNode::eGM_None);
		mv_giMode.AddEnumItem("Static Voxelization", IRenderNode::eGM_StaticVoxelization);
		mv_giMode.AddEnumItem("Analytical GI Proxy Soft", IRenderNode::eGM_AnalyticalProxy_Soft);
		mv_giMode.AddEnumItem("Analytical GI Proxy Hard", IRenderNode::eGM_AnalyticalProxy_Hard);
		mv_giMode.AddEnumItem("Analytical Occluder", IRenderNode::eGM_AnalytPostOccluder);
		mv_giMode.AddEnumItem("Integrate Into Terrain", IRenderNode::eGM_IntegrateIntoTerrain);
	}

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_outdoor, "OutdoorOnly", _T("Ignore Visareas"), functor(*this, &CEntityObject::OnEntityFlagsChange));

	if (!IsLight())
	{
		// In case of light entity those castShadow flags are not used by 3dengine. Real working flags are defined by LUA script.
		m_pVarObject->AddVariable(mv_castShadow, "CastShadow", _T("Cast Shadow"), functor(*this, &CEntityObject::OnEntityFlagsChange));
		m_pVarObject->AddVariable(mv_castShadowMinSpec, "CastShadowMinspec", _T("Shadow Minimum Graphics"), functor(*this, &CEntityObject::OnEntityFlagsChange));

		m_pVarObject->AddVariable(mv_DynamicDistanceShadows, "DynamicDistanceShadows", _T("Dynamic Distance Shadows"), functor(*this, &CEntityObject::OnRenderFlagsChange));
	}

	m_pVarObject->AddVariable(mv_giMode, "GIMode", _T("GI and Usage Mode"), functor(*this, &CEntityObject::OnEntityFlagsChange));

	if (!IsLight())
	{
		m_pVarObject->AddVariable(mv_ratioLOD, "LodRatio", functor(*this, &CEntityObject::OnRenderFlagsChange));
	}

	m_pVarObject->AddVariable(mv_ratioViewDist, "ViewDistRatio", functor(*this, &CEntityObject::OnRenderFlagsChange));
	m_pVarObject->AddVariable(mv_hiddenInGame, "HiddenInGame");

	if (!IsLight())
	{
		m_pVarObject->AddVariable(mv_recvWind, "RecvWind", _T("Receive Wind"), functor(*this, &CEntityObject::OnEntityFlagsChange));

		// [artemk]: Add RenderNearest entity param because of animator request.
		// This will cause that slot zero is rendered with ENTITY_SLOT_RENDER_NEAREST flag raised.
		// Used mostly in TrackView editor.
		m_pVarObject->AddVariable(mv_renderNearest, "RenderNearest", functor(*this, &CEntityObject::OnRenderFlagsChange));
		mv_renderNearest.SetDescription("Used to eliminate z-buffer artifacts when rendering from first person view");

		m_pVarObject->AddVariable(mv_noDecals, "NoStaticDecals", functor(*this, &CEntityObject::OnRenderFlagsChange));
	}
}

void CEntityObject::SetScriptName(const string& file, CBaseObject* pPrev)
{
	// detect light source entity before InitVariables() call so we can adjust entity properties UI
	if (pPrev)
	{
		CEntityObject* pEntityObject = static_cast<CEntityObject*>(pPrev);

		m_bLight = pEntityObject->IsLight();
	}
	else
	{
		m_bLight = strcmp(file, "Entity::Light") == 0 || strcmp(file, "Light") == 0 || strstr(file, "/Lights/") != 0;
	}
}

void CEntityObject::Done()
{
	LOADING_TIME_PROFILE_SECTION;
	//Somehow this crashes the profiler, because entity names may be malformed at times. This would be very nice if it was fixed, meanwhile using the version without name.
	//LOADING_TIME_PROFILE_SECTION_NAMED(GetName().c_str());
	if (m_pFlowGraph)
	{
		GetIEditorImpl()->GetFlowGraphManager()->UnregisterGraph(m_pFlowGraph);
	}
	SAFE_RELEASE(m_pFlowGraph);

	DeleteEntity();
	UnloadScript();

	ReleaseEventTargets();
	RemoveAllEntityLinks();

	for (CListenerSet<IEntityObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnDone();
	}

	CBaseObject::Done();
}

void CEntityObject::FreeGameData()
{
	DeleteEntity();
}

bool CEntityObject::Init(CBaseObject* pPrev, const string& file)
{
	CBaseObject::Init(pPrev, file);

	if (pPrev)
	{
		CEntityObject* pPreviousEntity = (CEntityObject*)pPrev;

		// Mark this entity as having been cloned from another
		m_bCloned = true;

		// Clone Properties.
		if (pPreviousEntity->m_pLuaProperties)
		{
			m_pLuaProperties = CloneProperties(pPreviousEntity->m_pLuaProperties);
		}
		if (pPreviousEntity->m_pLuaProperties2)
		{
			m_pLuaProperties2 = CloneProperties(pPreviousEntity->m_pLuaProperties2);
		}

		// When cloning entity, do not get properties from script.
		m_entityClassGUID = pPreviousEntity->m_entityClassGUID;
		SetClass(pPreviousEntity->GetEntityClass(), false);

		mv_outdoor = pPreviousEntity->mv_outdoor;
		mv_castShadowMinSpec = pPreviousEntity->mv_castShadowMinSpec;
		mv_ratioLOD = pPreviousEntity->mv_ratioLOD;
		mv_ratioViewDist = pPreviousEntity->mv_ratioViewDist;
		mv_hiddenInGame = pPreviousEntity->mv_hiddenInGame;
		mv_recvWind = pPreviousEntity->mv_recvWind;
		mv_renderNearest = pPreviousEntity->mv_renderNearest;
		mv_noDecals = pPreviousEntity->mv_noDecals;
		mv_DynamicDistanceShadows = pPreviousEntity->mv_DynamicDistanceShadows;

		if (pPreviousEntity->GetIEntity())
		{
			// Xml data about components serialization need to be cloned from the other entity.
			XmlNodeRef entityXmlNode = XmlHelpers::CreateXmlNode("EntityObjectExport");
			pPreviousEntity->GetIEntity()->SerializeXML(entityXmlNode, false, false);
			m_loadedXmlNodeData = entityXmlNode;

			SpawnEntity();
		}
		else
		{
			// Xml data about components serialization need to be cloned from the other entity.
			m_loadedXmlNodeData = pPreviousEntity->m_loadedXmlNodeData;
		}

		if (pPreviousEntity->m_pFlowGraph)
		{
			SetFlowGraph((CHyperFlowGraph*)pPreviousEntity->m_pFlowGraph->Clone());
		}
	}
	else if (!file.IsEmpty())
	{
		int startIdx = file.ReverseFind('/') + 1;
		int endIdx = file.ReverseFind('.') - startIdx;
		if (endIdx <= 0)
			endIdx = file.GetLength() - startIdx;

		SetUniqName(file.Mid(startIdx, endIdx));
		SetClass(file, false);
	}

	ResetCallbacks();

	return true;
}

CEntityObject* CEntityObject::FindFromEntityId(EntityId id)
{
	using namespace Private_EntityObject;
	CEntityObject* pEntity = stl::find_in_map(s_entityIdMap, id, 0);
	return pEntity;
}

bool CEntityObject::IsLegacyObject() const
{
	return (m_pEntityClass != nullptr && gEnv->pGameFramework->GetIGameObjectSystem()->GetID(m_pEntityClass->GetName()) != IGameObjectSystem::InvalidExtensionID)
	       || (m_pEntityScript != nullptr && strlen(m_pEntityScript->GetClass()->GetScriptFile()) > 0)
	       || m_prototype != nullptr;
}

void CEntityObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	if (m_pLuaProperties2 != nullptr)
	{
		creator.AddPropertyTree<CEntityObject>("Lua Instance Properties", &CEntityObject::SerializeLuaInstanceProperties);
	}

	if (m_pLuaProperties != nullptr)
	{
		creator.AddPropertyTree<CEntityObject>("Lua Properties", &CEntityObject::SerializeLuaProperties);
	}

	creator.AddPropertyTree<CEntityObject>("Flowgraph", &CEntityObject::SerializeFlowgraph);
	creator.AddPropertyTree<CEntityObject>("Entity Links", &CEntityObject::SerializeLinks);

	if (IsLegacyObject())
	{
		creator.AddPropertyTree<CEntityObject>("Entity Settings", [](CEntityObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
		{
			if (!pObject->IsLight())
			{
			  pObject->m_pVarObject->SerializeVariable(&pObject->mv_outdoor, ar);
			  pObject->m_pVarObject->SerializeVariable(pObject->mv_castShadowMinSpec.GetVar(), ar);
			  pObject->m_pVarObject->SerializeVariable(&pObject->mv_DynamicDistanceShadows, ar);
			}

			pObject->m_pVarObject->SerializeVariable(pObject->mv_giMode.GetVar(), ar);

			if (!pObject->IsLight())
			{
			  pObject->m_pVarObject->SerializeVariable(&pObject->mv_ratioLOD, ar);
			}

			pObject->m_pVarObject->SerializeVariable(&pObject->mv_ratioViewDist, ar);
			pObject->m_pVarObject->SerializeVariable(&pObject->mv_hiddenInGame, ar);

			if (!pObject->IsLight())
			{
			  pObject->m_pVarObject->SerializeVariable(&pObject->mv_recvWind, ar);
			  pObject->m_pVarObject->SerializeVariable(&pObject->mv_renderNearest, ar);
			  pObject->m_pVarObject->SerializeVariable(&pObject->mv_noDecals, ar);
			}
		}, true);
	}

	if (m_prototype != nullptr)
	{
		creator.AddPropertyTree<CEntityObject>("Entity Archetype", &CEntityObject::SerializeArchetype);
	}

	if (m_pEntity != nullptr)
	{
		Schematyc::IObject* pSchematycObject = m_pEntity->GetSchematycObject();
		if (pSchematycObject && pSchematycObject->GetObjectProperties() && pSchematycObject->GetObjectProperties()->HasVariables())
		{
			creator.AddPropertyTree<CEntityObject>("Schematyc Variables", [](CEntityObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
			{
				pObject->m_pEntity->GetSchematycObject()->GetObjectProperties()->SerializeVariables(ar);
			});
		}

		CreateComponentWidgets(creator);
	}
}

static void OnBnClickedAddComponent()
{
	enum class EComponentDictionaryColumn : int32
	{
		Name = 0,
		Count
	};

	class CComponentDictionaryEntry final : public CAbstractDictionaryEntry
	{
		friend class CComponentDictionary;

	public:
		CComponentDictionaryEntry(const char* szName, const Schematyc::IEnvComponent& component, QString tooltip, const char* szIcon, bool bEnabled, const CAbstractDictionaryEntry* pParentEntry = nullptr)
			: m_name(szName)
			, m_component(component)
			, m_tooltip(tooltip)
			, m_icon(szIcon)
			, m_bEnabled(bEnabled)
			, m_pParentEntry(pParentEntry) {}
		virtual ~CComponentDictionaryEntry() {}

		// CAbstractDictionaryEntry
		virtual uint32   GetType() const override { return CAbstractDictionaryEntry::Type_Entry; }

		virtual QVariant GetColumnValue(int32 columnIndex) const override
		{
			if (columnIndex == (uint32)EComponentDictionaryColumn::Name)
			{
				return m_name;
			}
			return QVariant();
		}

		virtual QString                         GetToolTip() const override                     { return m_tooltip; }
		virtual const QIcon*                    GetColumnIcon(int32 columnIndex) const override { return &m_icon; }
		virtual const CAbstractDictionaryEntry* GetParentEntry() const override                 { return m_pParentEntry; }
		virtual bool                            IsEnabled() const override                      { return m_bEnabled; }
		// ~CAbstractDictionaryEntry

		const Schematyc::IEnvComponent& GetComponent() const { return m_component; }

	private:
		QString                         m_name;
		QString                         m_tooltip;
		CryIcon                         m_icon;
		bool                            m_bEnabled;

		const Schematyc::IEnvComponent& m_component;
		const CAbstractDictionaryEntry* m_pParentEntry;
	};

	class CComponentDictionaryCategoryEntry final : public CAbstractDictionaryEntry
	{
		friend class CComponentDictionary;

	public:
		CComponentDictionaryCategoryEntry(const char* szName, CComponentDictionaryCategoryEntry* pParent = nullptr)
			: m_name(szName)
			, m_pParentEntry(pParent) {}
		virtual ~CComponentDictionaryCategoryEntry() {}

		// CAbstractDictionaryEntry
		virtual uint32   GetType() const override { return CAbstractDictionaryEntry::Type_Folder; }
		virtual QVariant GetColumnValue(int32 columnIndex) const override
		{
			if (columnIndex == (uint32)EComponentDictionaryColumn::Name)
			{
				return m_name;
			}
			return QVariant();
		}

		virtual int32                           GetNumChildEntries() const override       { return m_entries.size(); }
		virtual const CAbstractDictionaryEntry* GetChildEntry(int32 index) const override { return m_entries[index].get(); }

		virtual const CAbstractDictionaryEntry* GetParentEntry() const override           { return m_pParentEntry; }
		// ~CAbstractDictionaryEntry

		void AddEntry(CComponentDictionaryEntry& entry)
		{
			m_entries.emplace_back(stl::make_unique<CComponentDictionaryEntry>(entry));
		}

		CComponentDictionaryCategoryEntry& AddCategoryEntry(const char* szName)
		{
			m_entries.emplace_back(stl::make_unique<CComponentDictionaryCategoryEntry>(szName, this));
			return static_cast<CComponentDictionaryCategoryEntry&>(*m_entries.back().get());
		}

	private:
		QString                                                m_name;
		std::vector<std::unique_ptr<CAbstractDictionaryEntry>> m_entries;
		const CAbstractDictionaryEntry*                        m_pParentEntry;
	};

	class CComponentDictionary final : public CAbstractDictionary
	{
	public:
		virtual ~CComponentDictionary() {}

		// CryGraphEditor::CAbstractDictionary
		virtual int32                           GetNumEntries() const override       { return m_entries.size(); }
		virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override { return (m_entries.size() > index) ? m_entries[index].get() : nullptr; }

		virtual int32                           GetNumColumns() const override       { return (uint32)EComponentDictionaryColumn::Count; };
		virtual QString                         GetColumnName(int32 index) const override
		{
			if (index == (uint32)EComponentDictionaryColumn::Name)
				return QString("Name");
			return QString();
		}

		virtual int32 GetDefaultFilterColumn() const override { return (uint32)EComponentDictionaryColumn::Name; }
		// ~CryGraphEditor::CAbstractDictionary

		void AddEntry(CComponentDictionaryEntry& entry)
		{
			m_entries.emplace_back(stl::make_unique<CComponentDictionaryEntry>(entry));
		}

		CComponentDictionaryCategoryEntry& AddCategoryEntry(const char* szName)
		{
			m_entries.emplace_back(stl::make_unique<CComponentDictionaryCategoryEntry>(szName));
			return static_cast<CComponentDictionaryCategoryEntry&>(*m_entries.back().get());
		}

	private:
		std::vector<std::unique_ptr<CAbstractDictionaryEntry>> m_entries;
	};

	// Create a temporary vector containing the class descriptions of all existing components in the currently selected entities
	// This is later used to ensure that we do not add incompatible components
	std::vector<const CEntityComponentClassDesc*> existingComponentClassDescs;
	DynArray<IEntityComponent*> existingComponentsTemp;

	const CSelectionGroup* pSelectionGroup = GetIEditor()->GetObjectManager()->GetSelection();
	for (int i = 0, n = pSelectionGroup->GetCount(); i < n; ++i)
	{
		CBaseObject* pSelectedObject = pSelectionGroup->GetObject(i);
		CRuntimeClass* pSelectedObjectClass = pSelectedObject->GetRuntimeClass();
		CRY_ASSERT(pSelectedObjectClass == RUNTIME_CLASS(CEntityObject) || pSelectedObjectClass->IsDerivedFrom(RUNTIME_CLASS(CEntityObject)));

		if (IEntity* pSelectedEntity = static_cast<CEntityObject*>(pSelectedObject)->GetIEntity())
		{
			existingComponentsTemp.clear();
			pSelectedEntity->GetComponents(existingComponentsTemp);

			for (IEntityComponent* pComponent : existingComponentsTemp)
			{
				bool bHasClassDesc = std::find_if(existingComponentClassDescs.begin(), existingComponentClassDescs.end(), [pComponent](const CEntityComponentClassDesc* pComponentClassDesc) -> bool
				{
					return pComponentClassDesc->GetGUID() == pComponent->GetClassDesc().GetGUID();
				}) != existingComponentClassDescs.end();

				if (!bHasClassDesc)
				{
					existingComponentClassDescs.push_back(&pComponent->GetClassDesc());
				}
			}
		}
	}

	std::map<string, std::vector<const Schematyc::IEnvComponent*>> componentsMap;

	// Populate all available entity components into the map, sorted by category
	auto visitComponentsLambda = [&existingComponentClassDescs, &componentsMap](const Schematyc::IEnvComponent& component)
	{
		if (strlen(component.GetDesc().GetLabel()) == 0)
		{
			return Schematyc::EVisitStatus::Continue;
		}

		if (component.GetDesc().GetComponentFlags().Check(EEntityComponentFlags::HideFromInspector))
		{
			return Schematyc::EVisitStatus::Continue;
		}

		string editorCategory = component.GetDesc().GetEditorCategory();

		componentsMap[editorCategory].push_back(&component);

		return Schematyc::EVisitStatus::Continue;
	};

	gEnv->pSchematyc->GetEnvRegistry().VisitComponents(visitComponentsLambda);

	CComponentDictionary dictionary;

	for (const std::pair<const char*, std::vector<const Schematyc::IEnvComponent*>>& componentCategoryPair : componentsMap)
	{
		CComponentDictionaryCategoryEntry* pCategoryEntry = nullptr;
		if (componentCategoryPair.first[0] != '\0')
		{
			pCategoryEntry = &dictionary.AddCategoryEntry(componentCategoryPair.first);
		}

		for (const Schematyc::IEnvComponent* pComponent : componentCategoryPair.second)
		{
			QString tooltip = pComponent->GetDesc().GetDescription();
			bool bEnabled = true;

			// Check whether or not we can add the component
			// Note that we keep the item in the list, but disable it
			for (const CEntityComponentClassDesc* pComponentClassDesc : existingComponentClassDescs)
			{
				// Check singletons
				if (pComponentClassDesc->GetComponentFlags().Check(EEntityComponentFlags::Singleton) && pComponentClassDesc->GetGUID() == pComponent->GetDesc().GetGUID())
				{
					tooltip = "DISABLED: Only one instance of this component type can be present in the same entity.";
					bEnabled = false;

					break;
				}

				// Check compatibility
				if (!pComponentClassDesc->IsCompatibleWith(pComponent->GetDesc().GetGUID()))
				{
					const char* szIncompatibleComponentName = pComponentClassDesc->GetLabel();
					if (szIncompatibleComponentName == nullptr || szIncompatibleComponentName[0] == '\0')
					{
						szIncompatibleComponentName = pComponentClassDesc->GetName().c_str();
					}

					tooltip = string().Format("DISABLED: This component is incompatible with %s", szIncompatibleComponentName);
					bEnabled = false;

					break;
				}
			}

			const char* szName = pComponent->GetDesc().GetLabel();
			if (*szName == '\0')
			{
				szName = pComponent->GetDesc().GetName().c_str();
			}

			if (pCategoryEntry != nullptr)
			{
				pCategoryEntry->AddEntry(CComponentDictionaryEntry(szName, *pComponent, tooltip, pComponent->GetDesc().GetIcon(), bEnabled, pCategoryEntry));
			}
			else
			{
				dictionary.AddEntry(CComponentDictionaryEntry(szName, *pComponent, tooltip, pComponent->GetDesc().GetIcon(), bEnabled));
			}
		}
	}

	std::unique_ptr<CModalPopupDictionary> pDictionary = stl::make_unique<CModalPopupDictionary>("Entity::AddComponent", dictionary);
	pDictionary->ExecAt(QCursor::pos());

	if (CComponentDictionaryEntry* pSelectedComponentEntry = static_cast<CComponentDictionaryEntry*>(pDictionary->GetResult()))
	{
		using namespace Private_EntityObject;
		const CSelectionGroup* pSelectionGroup = GetIEditor()->GetObjectManager()->GetSelection();
		std::unique_ptr<CUndoAddComponent> pUndoAddComponent = stl::make_unique<CUndoAddComponent>(&pSelectedComponentEntry->GetComponent().GetDesc(), pSelectionGroup->GetCount());

		for (int i = 0, n = pSelectionGroup->GetCount(); i < n; ++i)
		{
			CBaseObject* pSelectedObject = pSelectionGroup->GetObject(i);
			CRuntimeClass* pSelectedObjectClass = pSelectedObject->GetRuntimeClass();
			CRY_ASSERT(pSelectedObjectClass == RUNTIME_CLASS(CEntityObject) || pSelectedObjectClass->IsDerivedFrom(RUNTIME_CLASS(CEntityObject)));

			if (IEntity* pSelectedEntity = static_cast<CEntityObject*>(pSelectedObject)->GetIEntity())
			{
				// Code for adding component is in the undo object, this way we make sure that Redo 100% matches the initial add
				pUndoAddComponent->AddComponent(pSelectedEntity);
			}
		}

		if (pUndoAddComponent->IsValid())
		{
			CUndo undo("Add Entity Component");
			CUndo::Record(pUndoAddComponent.release());
		}

		GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
	}
}

void OnBnClickedRevertComponentProperties(IEntityComponent* pComponent)
{
	DynArray<char> variableBuffer;

	struct SPropertyReader
	{
		Schematyc::CClassProperties* pProperties;
		void                         Serialize(Serialization::IArchive& archive)
		{
			pProperties->Serialize(archive);
		}
	};

	const Schematyc::IObject* pSchematycObject = pComponent->GetEntity()->GetSchematycObject();
	const Schematyc::CClassProperties* pDefaultProperties = pSchematycObject->GetClass().GetDefaultProperties().GetComponentProperties(pComponent->GetGUID());

	// Save properties from the source to buffer
	gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(variableBuffer, Serialization::SStruct(SPropertyReader { const_cast<Schematyc::CClassProperties*>(pDefaultProperties) }));

	struct SPropertyWriter
	{
		Schematyc::CClassProperties* pProperties;
		IEntityComponent*            pComponent;
		void                         Serialize(Serialization::IArchive& archive)
		{
			pProperties->Serialize(archive);

			Schematyc::Utils::CClassSerializer classSerializer(pComponent->GetClassDesc(), pComponent);
			classSerializer.Serialize(archive);
		}
	};

	Schematyc::CClassProperties* pPerInstanceClassProperties = pSchematycObject->GetObjectProperties()->GetComponentProperties(pComponent->GetGUID());
	gEnv->pSystem->GetArchiveHost()->LoadBinaryBuffer(Serialization::SStruct(SPropertyWriter { pPerInstanceClassProperties, pComponent }), variableBuffer.data(), variableBuffer.size());

	pPerInstanceClassProperties->SetOverridePolicy(Schematyc::EOverridePolicy::Default);

	// Notify this component.
	SEntityEvent entityEvent(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
	pComponent->SendEvent(entityEvent);
}

void CreateComponentWidget(CEntityObject* pEntityObject, CryGUID componentTypeGUID, size_t typeInstanceIndex, Serialization::IArchive& archive)
{
	// Query all components of this type in the entity
	IEntity* pEntity = pEntityObject->GetIEntity();
	CRY_ASSERT(pEntity != nullptr);
	if (pEntity == nullptr)
		return;

	DynArray<IEntityComponent*> components;
	pEntity->GetComponentsByTypeId(componentTypeGUID, components);
	if (components.size() <= typeInstanceIndex)
		return;

	// Now get the component at the desired index.
	IEntityComponent* pComponent = components[typeInstanceIndex];

	// Serialize entity groups introduced with 5.3, considered legacy as this is not exposed to Schematyc
	if (IEntityPropertyGroup* pProperties = pComponent->GetPropertyGroup())
	{
		pProperties->SerializeProperties(archive);
	}

	// Serialize new unified Schematyc properties
	if (!pComponent->GetClassDesc().GetName().IsEmpty())
	{
		if (pComponent->GetComponentFlags().Check(EEntityComponentFlags::UserAdded) || pComponent->GetComponentFlags().Check(EEntityComponentFlags::SchematycEditable))
		{
			if (pComponent->GetEntitySlotId() != IEntityComponent::EmptySlotId && pComponent->GetTransform() == nullptr)
			{
				// Create transform if it doesn't exist yet
				pComponent->SetTransformMatrix(IDENTITY);
			}

			if (pComponent->GetTransform() != nullptr)
			{
				archive(*pComponent->GetTransform(), "transform", "Transform");
				if (archive.isInput())
				{
					// Apply changed transform
					pComponent->SetTransformMatrix(pComponent->GetTransform());
				}
			}
		}

		if (pComponent->GetClassDesc().GetMembers().size() > 0)
		{
			Schematyc::IObject* pSchematycObject = pComponent->GetEntity()->GetSchematycObject();

			// Get access to the custom class properties for this Schematyc component
			Schematyc::CClassProperties* pPerInstanceClassProperties = nullptr;
			if (pComponent->GetComponentFlags().Check(EEntityComponentFlags::Schematyc) &&
			    pSchematycObject != nullptr)
			{
				if (Schematyc::IObjectPropertiesPtr pObjProps = pSchematycObject->GetObjectProperties())
				{
					pPerInstanceClassProperties = pObjProps->GetComponentProperties(pComponent->GetGUID());
				}
			}

			if (pPerInstanceClassProperties != nullptr && archive.isEdit())
			{
				if (pPerInstanceClassProperties->GetOverridePolicy() != Schematyc::EOverridePolicy::Default)
				{
					// Provide a button for reverting overridden entity instance property values to the defaults specified in Schematyc
					archive(Serialization::ActionButton([pComponent]() { OnBnClickedRevertComponentProperties(pComponent); }), "revert", "^Revert to Defaults");
				}
			}

			Schematyc::CClassProperties classPropertiesBefore;
			Schematyc::CClassProperties classPropertiesAfter;

			classPropertiesBefore.Read(pComponent->GetClassDesc(), pComponent);

			Schematyc::Utils::CClassSerializer classSerializer(pComponent->GetClassDesc(), pComponent);
			if (!pComponent->GetComponentFlags().Check(EEntityComponentFlags::Schematyc) || pComponent->GetComponentFlags().Check(EEntityComponentFlags::SchematycEditable))
			{
				classSerializer.Serialize(archive);
			}
			else
			{
				// Show as read-only
				archive(classSerializer, "Serializer", "!-Private Properties");
			}

			classPropertiesAfter.Read(pComponent->GetClassDesc(), pComponent);
			bool bChangedProperties = !classPropertiesBefore.Compare(classPropertiesAfter);

			if (bChangedProperties && pPerInstanceClassProperties != nullptr)
			{
				// If any member was modified this component should be marked as override
				pPerInstanceClassProperties->SetOverridePolicy(Schematyc::EOverridePolicy::Override);

				// Read current component values into the object properties so that it is recorded for later
				pPerInstanceClassProperties->Read(pComponent->GetClassDesc(), pComponent);
			}

			if (bChangedProperties || archive.isInput())
			{
				// Notify this component.
				SEntityEvent entityEvent(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
				pComponent->SendEvent(entityEvent);
			}
		}
	}
}

void CEntityObject::CreateComponentWidgets(CInspectorWidgetCreator& creator)
{
	creator.AddWidget((uint64)'addc', [](const CInspectorWidgetCreator::SQueuedWidgetInfo& queuedWidget) -> QWidget*
	{
		QPushButton* pAddComponentButton = new QPushButton(CryIcon("icons:General/Plus.ico"), "Add Component");
		pAddComponentButton->setFixedHeight(44);
		pAddComponentButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
		QObject::connect(pAddComponentButton, &QToolButton::clicked, []()
		{
			OnBnClickedAddComponent();
		});

		return pAddComponentButton;
	});

	DynArray<IEntityComponent*> components;
	m_pEntity->GetComponents(components);

	// We store a map containing component type GUID and index of the component instance
	// This is used in order to support having multiple components of the same type, and serializing + showing them independently.
	// Otherwise they would be merged into one element or disappear altogether from UI.
	std::unordered_map<CryGUID, int> componentIndexMap;

	for (IEntityComponent* pComponent : components)
	{
		if (pComponent->GetComponentFlags().Check(IEntityComponent::EFlags::HiddenFromUser))
		{
			continue;
		}

		const char* szComponentLabel = pComponent->GetName();
		if (szComponentLabel == nullptr || szComponentLabel[0] == '\0')
		{
			szComponentLabel = pComponent->GetClassDesc().GetLabel();
		}

		if (szComponentLabel[0] == '\0')
		{
			// Support legacy components lacking names
			if (IEntityPropertyGroup* pProperties = pComponent->GetPropertyGroup())
			{
				szComponentLabel = pProperties->GetLabel();
			}
			else
			{
				continue;
			}
		}

		CryGUID componentClassGUID = pComponent->GetClassDesc().GetGUID();
		if (componentClassGUID.IsNull())
		{
			// Support legacy factories used in 5.3, entity query further below will also enumerate the factory registry
			if (ICryFactory* pFactory = pComponent->GetFactory())
			{
				componentClassGUID = pFactory->GetClassID();
			}
			else
			{
				continue;
			}
		}

		// Get the index of this component instance, out of the number of instances of this type inside the entity
		auto indexIt = componentIndexMap.find(componentClassGUID);
		int componentTypeIndex;
		if (indexIt == componentIndexMap.end())
		{
			componentTypeIndex = 0;
			componentIndexMap.emplace(componentClassGUID, componentTypeIndex);
		}
		else
		{
			componentTypeIndex = ++(indexIt->second);
		}

		CryGUID componentInstanceGUID = pComponent->GetGUID();

		std::unique_ptr<CEntityComponentCollapsibleFrame> pWidget = stl::make_unique<CEntityComponentCollapsibleFrame>(szComponentLabel,
			pComponent->GetClassDesc(), componentTypeIndex, pComponent->GetComponentFlags().Check(EEntityComponentFlags::UserAdded));

		creator.AddPropertyTree(pComponent->GetClassDesc().GetGUID().hipart + componentTypeIndex, szComponentLabel, std::move(pWidget), [componentClassGUID, componentTypeIndex](CBaseObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
		{
			CreateComponentWidget(static_cast<CEntityObject*>(pObject), componentClassGUID, componentTypeIndex, ar);
		});
	}
}

void CEntityObject::SerializeLuaInstanceProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	if (m_pLuaProperties2)
	{
		m_pLuaProperties2->Serialize(ar);
	}
}

void CEntityObject::SerializeLuaProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	if (m_pLuaProperties || GetScript())
	{
		if (GetScript())
		{
			string script = GetScript()->GetFile();

			if (!script.IsEmpty() && ar.openBlock("Script", "Script"))
			{
				ar(script, "scriptname", "!Path");
				if (ar.openBlock("script_buttons", "<Script"))
				{
					ar(Serialization::ActionButton(std::bind(&CEntityObject::OnEditScript, this)), "edit_script", "^Edit");
					ar(Serialization::ActionButton(std::bind(&CEntityObject::OnMenuReloadScripts, this)), "reload_script", "^Reload");
					ar.closeBlock();
				}
				ar.closeBlock();
			}
		}

		if (m_pLuaProperties)
		{
			m_pLuaProperties->Serialize(ar);
		}
		ar.closeBlock();
	}
}

void CEntityObject::SerializeFlowgraph(Serialization::IArchive& ar, bool bMultiEdit)
{
	if (ar.openBlock("flowgraph_operators", "<Flowgraph"))
	{
		ar(Serialization::ActionButton(std::bind(&CEntityObject::OnBnClickedOpenFlowGraph, this)), "open_flowgraph", "^Open");
		ar(Serialization::ActionButton(std::bind(&CEntityObject::OnBnClickedListFlowGraphs, this)), "list_flowgraph", "^List");
		ar(Serialization::ActionButton(std::bind(&CEntityObject::OnBnClickedRemoveFlowGraph, this)), "remove_flowgraph", m_pFlowGraph ? "^Remove" : "^!Remove");
		ar.closeBlock();
	}
}

void CEntityObject::SerializeArchetype(Serialization::IArchive& ar, bool bMultiEdit)
{
	if (m_prototype != nullptr)
	{
		const string archetype = m_prototype->GetFullName();
		ar(archetype, "entity_archetype_name", "!Name");
		ar(Serialization::ActionButton(std::bind(&CEntityObject::OnOpenArchetype, this)), "entity_archetype_open", "^Open");
	}
}

void CEntityObject::SerializeLinks(Serialization::IArchive& ar, bool bMultiEdit)
{
	using namespace Private_EntityObject;
	if (ar.isEdit())
	{
		if (ar.openBlock("linktools", "Link Tools"))
		{
			Serialization::SEditToolButton pickToolButton("");
			pickToolButton.SetToolClass(RUNTIME_CLASS(EntityLinkTool), nullptr, this);

			ar(pickToolButton, "picker", "^Pick");
			ar(Serialization::ActionButton([ = ] {
				CUndo undo("Clear entity links");
				RemoveAllEntityLinks();
			}), "picker", "^Clear");

			ar.closeBlock();
		}
	}

	// Make a copy to determine if something changed
	auto links = m_links;

	ar(links, "links_array", "Links");
	if (ar.isInput())
	{
		bool changed = false;

		// quick test to detect if serialization is really needed
		if (links.size() == m_links.size())
		{
			for (size_t i = 0; i < links.size(); ++i)
			{
				if (m_links[i].targetId != links[i].targetId
				    || m_links[i].name != links[i].name)
				{
					changed = true;
					break;
				}
			}
		}
		else
		{
			changed = true;
		}

		if (changed)
		{
			// Time to store undo
			CUndo undo("Modify Entity Links");

			RemoveAllEntityLinks();

			for (auto linkIt = links.begin(); linkIt != links.end(); ++linkIt)
			{
				AddEntityLink(linkIt->name.c_str(), linkIt->targetId);
			}
		}
	}
}

void CEntityObject::SetTransformDelegate(ITransformDelegate* pTransformDelegate, bool bInvalidateTM)
{
	CBaseObject::SetTransformDelegate(pTransformDelegate, bInvalidateTM);
	ResetCallbacks();
}

bool CEntityObject::IsSameClass(CBaseObject* obj)
{
	if (GetClassDesc() == obj->GetClassDesc())
	{
		CEntityObject* ent = (CEntityObject*)obj;
		return m_pEntityClass == ent->m_pEntityClass;
	}
	return false;
}

bool CEntityObject::ConvertFromObject(CBaseObject* object)
{
	__super::ConvertFromObject(object);

	if (object->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pObject = (CEntityObject*)object;

		mv_outdoor = pObject->mv_outdoor;
		mv_castShadowMinSpec = pObject->mv_castShadowMinSpec;
		mv_ratioLOD = pObject->mv_ratioLOD;
		mv_ratioViewDist = pObject->mv_ratioViewDist;
		mv_hiddenInGame = pObject->mv_hiddenInGame;
		mv_recvWind = pObject->mv_recvWind;
		mv_renderNearest = pObject->mv_renderNearest;
		mv_noDecals = pObject->mv_noDecals;
		mv_DynamicDistanceShadows = pObject->mv_DynamicDistanceShadows;
		return true;
	}

	return false;
}

void CEntityObject::SetLookAt(CBaseObject* target)
{
	CBaseObject::SetLookAt(target);
}

IPhysicalEntity* CEntityObject::GetCollisionEntity() const
{
	// Returns physical object of entity.
	if (m_pEntity)
	{
		return m_pEntity->GetPhysics();
	}
	return 0;
}

void CEntityObject::GetLocalBounds(AABB& box)
{
	box.min = ZERO;
	box.max = ZERO;

	if (m_pEntity != nullptr)
	{
		for (int i = 0, n = m_pEntity->GetSlotCount(); i < n; ++i)
		{
			AABB localSlotBounds;

			if (IStatObj* pStatObj = m_pEntity->GetStatObj(i))
			{
				localSlotBounds = pStatObj->GetAABB();
			}
			else if (ICharacterInstance* pCharacter = m_pEntity->GetCharacter(i))
			{
				localSlotBounds = pCharacter->GetAABB();
			}
			else if (IRenderNode* pRenderNode = m_pEntity->GetRenderNode(i))
			{
				if (pRenderNode->GetRenderNodeType() != eERType_Light
				    && pRenderNode->GetRenderNodeType() != eERType_ParticleEmitter
				    && pRenderNode->GetRenderNodeType() != eERType_FogVolume)
				{
					pRenderNode->GetLocalBounds(localSlotBounds);
				}
				else
				{
					continue;
				}
			}
			else
			{
				continue;
			}

			localSlotBounds.SetTransformedAABB(m_pEntity->GetSlotLocalTM(i, false), localSlotBounds);
			box.Add(localSlotBounds);
		}
	}
}

void CEntityObject::SetModified(bool boModifiedTransformOnly, bool bNotifyObjectManager)
{
	__super::SetModified(boModifiedTransformOnly, bNotifyObjectManager);

	if (m_pEntity)
	{
		CalcBBox();

		if (IAIObject* ai = m_pEntity->GetAI())
		{
			if (ai->GetAIType() == AIOBJECT_ACTOR)
			{
				GetIEditorImpl()->GetAI()->CalculateNavigationAccessibility();
			}
		}
	}
}

bool CEntityObject::HitTestCharacter(ICharacterInstance* pCharacter, HitContext& hc, SRayHitInfo& hitInfo, bool& bHavePhysics)
{
	if (IRenderMesh* pRenderMesh = pCharacter->GetIDefaultSkeleton().GetIRenderMesh())
	{
		if (gEnv->p3DEngine->RenderMeshRayIntersection(pRenderMesh, hitInfo))
		{
			return true;
		}
	}

	IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
	if (pAttachmentManager != nullptr)
	{
		for (int iAttachment = 0, numAttachments = pAttachmentManager->GetAttachmentCount(); iAttachment < numAttachments; ++iAttachment)
		{
			IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(iAttachment);
			IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
			if (pAttachmentObject == nullptr)
				continue;

			if (ICharacterInstance* pAttachmentCharacter = pAttachmentObject->GetICharacterInstance())
			{
				if (HitTestCharacter(pAttachmentCharacter, hc, hitInfo, bHavePhysics))
				{
					return true;
				}
			}
			else if (IStatObj* pStatObj = pAttachmentObject->GetIStatObj())
			{
				if (pStatObj->RayIntersection(hitInfo))
				{
					return true;
				}
			}
			else if (IAttachmentSkin* pIAttachmentSkin = pAttachmentObject->GetIAttachmentSkin())
			{
				ISkin* pSkin = pIAttachmentSkin->GetISkin();
				if (pSkin == nullptr)
					continue;

				IRenderMesh* pRenderMesh = pSkin->GetIRenderMesh(0);
				if (pRenderMesh == nullptr)
					continue;

				if (gEnv->p3DEngine->RenderMeshRayIntersection(pRenderMesh, hitInfo))
				{
					return true;
				}
			}
		}
	}

	IPhysicalEntity* pCharacterPhysicalEntity = pCharacter->GetISkeletonPose()->GetCharacterPhysics();
	if (pCharacterPhysicalEntity != nullptr
	    && pCharacterPhysicalEntity->GetType() != PE_NONE
	    && pCharacterPhysicalEntity->GetType() != PE_PARTICLE
	    && pCharacterPhysicalEntity->GetType() != PE_ROPE
	    && pCharacterPhysicalEntity->GetType() != PE_SOFT
	    && pCharacterPhysicalEntity->GetStatus(&pe_status_nparts()) > 0)
	{
		bHavePhysics = true;

		ray_hit hit;
		int numHits = gEnv->pPhysicalWorld->RayTraceEntity(pCharacterPhysicalEntity, hc.raySrc, hc.rayDir * 10000.0f, &hit);
		if (numHits > 0)
		{
			hc.dist = hit.dist;
			hc.object = this;
			return true;
		}
	}

	return false;
}

bool CEntityObject::HitTestEntity(HitContext& hc, bool& bHavePhysics)
{
	Matrix34 invertedWorldTransform = m_pEntity->GetWorldTM().GetInverted();

	Vec3 raySrc = invertedWorldTransform.TransformPoint(hc.raySrc);
	Vec3 rayDir = invertedWorldTransform.TransformVector(hc.rayDir).GetNormalized();

	AABB bbox;
	m_pEntity->GetLocalBounds(bbox);

	// Early exit, check bounding box
	Vec3 point;
	if (!Intersect::Ray_AABB(raySrc, rayDir, bbox, point))
	{
		return false;
	}

	SRayHitInfo hitInfo;
	hitInfo.inReferencePoint = raySrc;
	hitInfo.inRay = Ray(raySrc, rayDir);

	if (m_visualObject != nullptr)
	{
		if (m_visualObject->RayIntersection(hitInfo))
		{
			// World space distance.
			Vec3 worldHitPos = GetWorldTM().TransformPoint(hitInfo.vHitPos);
			hc.dist = hc.raySrc.GetDistance(worldHitPos);
			hc.object = this;
			return true;
		}
	}

	for (int i = 0, n = m_pEntity->GetSlotCount(); i < n; ++i)
	{
		if (IStatObj* pStatObj = m_pEntity->GetStatObj(i))
		{
			if (pStatObj->RayIntersection(hitInfo))
			{
				// World space distance.
				Vec3 worldHitPos = GetWorldTM().TransformPoint(hitInfo.vHitPos);
				hc.dist = hc.raySrc.GetDistance(worldHitPos);
				hc.object = this;
				return true;
			}
		}
		else if (ICharacterInstance* pCharacter = m_pEntity->GetCharacter(i))
		{
			if (HitTestCharacter(pCharacter, hc, hitInfo, bHavePhysics))
			{
				// World space distance.
				Vec3 worldHitPos = GetWorldTM().TransformPoint(hitInfo.vHitPos);
				hc.dist = hc.raySrc.GetDistance(worldHitPos);
				hc.object = this;
				return true;
			}
		}
		else if (IGeomCacheRenderNode* pGeomCache = m_pEntity->GetGeomCacheRenderNode(i))
		{
			if (pGeomCache->RayIntersection(hitInfo))
			{
				// World space distance.
				Vec3 worldHitPos = GetWorldTM().TransformPoint(hitInfo.vHitPos);
				hc.dist = hc.raySrc.GetDistance(worldHitPos);
				hc.object = this;
				return true;
			}
		}
	}

	IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity();

	// Now if box intersected try real geometry ray test.
	if (pPhysicalEntity != nullptr
	    && pPhysicalEntity->GetStatus(&pe_status_nparts()) > 0
	    && pPhysicalEntity->GetType() != PE_NONE
	    && pPhysicalEntity->GetType() != PE_PARTICLE
	    && pPhysicalEntity->GetType() != PE_ROPE
	    && pPhysicalEntity->GetType() != PE_SOFT)
	{
		bHavePhysics = true;

		ray_hit hit;
		int numHits = gEnv->pPhysicalWorld->RayTraceEntity(pPhysicalEntity, hc.raySrc, hc.rayDir * 10000.0f, &hit);
		if (numHits > 0)
		{
			hc.dist = hit.dist;
			hc.object = this;
			return true;
		}
	}

	return false;
}

bool CEntityObject::HitTest(HitContext& hc)
{
	if (!hc.b2DViewport)
	{
		// Test 3D viewport.
		if (m_pEntity)
		{
			bool bHavePhysics = false;
			if (HitTestEntity(hc, bHavePhysics))
			{
				hc.object = this;
				return true;
			}
			if (bHavePhysics)
			{
				return false;
			}
		}
		if (m_visualObject && !gViewportPreferences.showIcons && !gViewportPreferences.showSizeBasedIcons)
		{
			Matrix34 tm = GetWorldTM();
			float sz = m_helperScale * gGizmoPreferences.helperScale;
			tm.ScaleColumn(Vec3(sz, sz, sz));
			primitives::ray aray;
			aray.origin = hc.raySrc;
			aray.dir = hc.rayDir * 10000.0f;

			IGeomManager* pGeomMgr = GetIEditorImpl()->GetSystem()->GetIPhysicalWorld()->GetGeomManager();
			IGeometry* pRay = pGeomMgr->CreatePrimitive(primitives::ray::type, &aray);
			geom_world_data gwd;
			gwd.offset = tm.GetTranslation();
			gwd.scale = tm.GetColumn0().GetLength();
			gwd.R = Matrix33(tm);
			geom_contact* pcontacts = 0;
			WriteLockCond lock;

			int col = (m_visualObject->GetPhysGeom() && m_visualObject->GetPhysGeom()->pGeom)
			          ? m_visualObject->GetPhysGeom()->pGeom->IntersectLocked(pRay, &gwd, 0, 0, pcontacts, lock)
			          : 0;

			pGeomMgr->DestroyGeometry(pRay);
			if (col > 0)
			{
				if (pcontacts)
				{
					hc.dist = pcontacts[col - 1].t;
				}
				hc.object = this;
				return true;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if ((m_bDisplayBBox && gViewportPreferences.showTriggerBounds) || hc.b2DViewport || (m_bDisplayBBox && m_bBBoxSelection))
	{
		float hitEpsilon = hc.view->GetScreenScaleFactor(GetWorldPos()) * 0.01f;
		float hitDist;

		float fScale = GetScale().x;
		AABB boxScaled;
		GetLocalBounds(boxScaled);
		boxScaled.min *= fScale;
		boxScaled.max *= fScale;

		Matrix34 invertWTM = GetWorldTM();
		invertWTM.Invert();

		Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
		Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir);
		xformedRayDir.Normalize();

		{
			Vec3 intPnt;
			if (m_bBBoxSelection)
			{
				// Check intersection with bbox.
				if (Intersect::Ray_AABB(xformedRaySrc, xformedRayDir, boxScaled, intPnt))
				{
					hc.dist = xformedRaySrc.GetDistance(intPnt);
					hc.object = this;
					return true;
				}
			}
			else
			{
				// Check intersection with bbox edges.
				if (Intersect::Ray_AABBEdge(xformedRaySrc, xformedRayDir, boxScaled, hitEpsilon, hitDist, intPnt))
				{
					hc.dist = xformedRaySrc.GetDistance(intPnt);
					hc.object = this;
					return true;
				}
			}
		}
	}

	return false;
}

bool CEntityObject::HitHelperTest(HitContext& hc)
{
	bool bResult = CBaseObject::HitHelperTest(hc);

	if (!bResult && m_pEntity && GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
	{
		bResult = HitHelperAtTest(hc, m_pEntity->GetWorldPos());
	}

	if (bResult)
		hc.object = this;

	return bResult;
}

bool CEntityObject::HitTestRect(HitContext& hc)
{
	bool bResult = false;

	if (m_visualObject && !gViewportPreferences.showIcons && !gViewportPreferences.showSizeBasedIcons)
	{
		AABB box;
		box.SetTransformedAABB(GetWorldTM(), m_visualObject->GetAABB());
		bResult = HitTestRectBounds(hc, box);
	}
	else
	{
		bResult = CBaseObject::HitTestRect(hc);
		if (!bResult && m_pEntity && GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
		{
			AABB box;
			if (hc.bUseSelectionHelpers)
			{
				box.max = box.min = m_pEntity->GetWorldPos();
			}
			else
			{
				// Retrieve world space bound box.
				m_pEntity->GetWorldBounds(box);
			}

			bResult = HitTestRectBounds(hc, box);
		}
	}

	if (bResult)
		hc.object = this;

	return bResult;
}

float CEntityObject::GetCreationOffsetFromTerrain() const
{
	//Only check this for the legacy lights
	if (IsLight())
	{
		return 0.0f;
	}

	if (m_pEntity != nullptr)
	{
		DynArray<IEntityComponent*> components;
		m_pEntity->GetComponents(components);

		for (IEntityComponent* pComponent : components)
		{
			auto flags = pComponent->GetComponentFlags();
			if (flags.Check(EEntityComponentFlags::NoCreationOffset))
			{
				return 0.0f;
			}
		}

		AABB bounds;

		m_pEntity->GetLocalBounds(bounds);
		return -bounds.min.z;
	}

	return 0.f;
}

bool CEntityObject::CreateGameObject()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!m_pEntityScript && !m_pEntityClass)
	{
		if (!m_entityClass.IsEmpty())
		{
			SetClass(m_entityClass, true);
		}
	}
	if (!m_pEntity)
	{
		if (!m_entityClass.IsEmpty())
		{
			SpawnEntity();
		}
	}

	return true;
}

bool CEntityObject::SetClass(const string& entityClass, bool bForceReload, XmlNodeRef xmlEntityNode)
{
	LOADING_TIME_PROFILE_SECTION;
	if (entityClass == m_entityClass && (m_pEntityScript || (m_pEntityClass && strlen(m_pEntityClass->GetScriptFile()) == 0)) && !bForceReload)
	{
		return true;
	}
	if (entityClass.IsEmpty())
	{
		return false;
	}

	m_entityClass = entityClass;
	m_bLoadFailed = false;

	UnloadScript();

	if (!IsCreateGameObjects())
	{
		return false;
	}

	// If ClassGUID loaded use it to find actual entity class
	if (!m_entityClassGUID.IsNull())
	{
		m_pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClassByGUID(m_entityClassGUID);
	}
	else
	{
		m_pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass.c_str());
	}

	if (m_pEntityClass != nullptr)
	{
		if (strlen(m_pEntityClass->GetScriptFile()) > 0)
		{
			if (std::shared_ptr<CEntityScript> pEntityScript = CEntityScriptRegistry::Instance()->Find(m_entityClass))
			{
				SetEntityScript(pEntityScript, bForceReload, xmlEntityNode);
				m_pEntityClass = pEntityScript->GetClass();
			}
		}

		m_entityClassGUID = m_pEntityClass->GetGUID();
	}

	if (m_pEntityScript == nullptr && m_pEntityClass == nullptr)
	{
		m_bLoadFailed = true;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Entity creation failed, class %s was not registered with the engine!", m_entityClass.c_str());

		return false;
	}

	return true;
}

void CEntityObject::ResetEditorClassInfo()
{
	const SEditorClassInfo& editorClassInfo = m_pEntityClass->GetEditorClassInfo();

	if (!editorClassInfo.sHelper.empty())
	{
		m_visualObject = gEnv->p3DEngine->LoadStatObj(editorClassInfo.sHelper.c_str(), nullptr, nullptr, false);
		if (m_visualObject)
		{
			m_visualObject->AddRef();
		}
	}

	if (m_pEntity && m_pEntityScript != nullptr)
	{
		m_pEntityScript->UpdateTextureIcon(m_pEntity);
	}

	string classIcon = editorClassInfo.sIcon;
	int iconId = 0;
	if (gEnv->pCryPak->IsFileExist(classIcon))
	{
		iconId = GetIEditor()->GetIconManager()->GetIconTexture(classIcon);
	}

	if (iconId == 0)
	{
		if (classIcon.size() > 0)
		{
			classIcon = "%EDITOR%/ObjectIcons/" + classIcon;
			iconId = GetIEditor()->GetIconManager()->GetIconTexture(classIcon);
		}

		if (iconId == 0)
		{
			iconId = GetClassDesc()->GetTextureIconId();
			// Make sure there's always an icon
			if (iconId == 0 && m_visualObject == nullptr)
			{
				iconId = GetIEditor()->GetIconManager()->GetIconTexture("%EDITOR%/ObjectIcons/Schematyc.bmp");
			}
		}
	}

	if (iconId != 0)
	{
		SetTextureIcon(iconId);
	}

	if (editorClassInfo.bIconOnTop)
	{
		SetFlags(OBJFLAG_SHOW_ICONONTOP);
	}
	else
	{
		ClearFlags(OBJFLAG_SHOW_ICONONTOP);
	}
}

bool CEntityObject::SetEntityScript(std::shared_ptr<CEntityScript> pEntityScript, bool bForceReload, XmlNodeRef xmlEntityNode)
{
	assert(pEntityScript);

	if (pEntityScript == m_pEntityScript && !bForceReload)
	{
		return true;
	}
	m_pEntityScript = pEntityScript;

	m_bLoadFailed = false;
	// Load script if its not loaded yet.
	if (!m_pEntityScript->IsValid())
	{
		if (!m_pEntityScript->Load())
		{
			m_bLoadFailed = true;
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to load entity script for Entity %s(Script: %s) %s",
			           (const char*)GetName(),
			           (const char*)m_entityClass,
			           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));

			return false;
		}
	}

	ResetEditorClassInfo();

	GetScriptProperties(xmlEntityNode);

	// Populate empty events from the script.
	for (int i = 0, num = m_pEntityScript->GetEmptyLinkCount(); i < num; i++)
	{
		const string& linkName = m_pEntityScript->GetEmptyLink(i);
		int j = 0;
		int numlinks = (int)m_links.size();
		for (j = 0; j < numlinks; j++)
		{
			if (strcmp(m_links[j].name, linkName) == 0)
			{
				break;
			}
		}
		if (j >= numlinks)
		{
			AddEntityLink(linkName, CryGUID::Null());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Check if needs to display bbox for this entity.
	//////////////////////////////////////////////////////////////////////////
	if (m_pEntityScript->GetFlags() & ENTITY_SCRIPT_SHOWBOUNDS)
	{
		m_bDisplayBBox = true;
		m_bDisplaySolidBBox = true;
	}
	else
	{
		m_bDisplayBBox = false;
		m_bDisplaySolidBBox = false;
	}

	m_bDisplayAbsoluteRadius = (m_pEntityScript->GetFlags() & ENTITY_SCRIPT_ABSOLUTERADIUS) ? true : false;

	m_bBBoxSelection = (m_pEntityScript->GetClass()->GetFlags() & ECLF_BBOX_SELECTION) ? true : false;

	m_bDisplayArrow = (m_pEntityScript->GetFlags() & ENTITY_SCRIPT_DISPLAY_ARROW) ? true : false;

	return true;
}

void CEntityObject::GetScriptProperties(XmlNodeRef xmlEntityNode)
{
	XmlNodeRef xmlProperties;
	XmlNodeRef xmlProperties2 = xmlEntityNode ? xmlEntityNode->findChild("Properties2") : XmlNodeRef();
	if (!m_prototype && xmlEntityNode)
	{
		xmlProperties = xmlEntityNode->findChild("Properties");
	}

	if (m_prototype == nullptr)
	{
		// Create Entity properties from Script properties..
		if (m_pEntityScript && m_pEntityScript->GetDefaultProperties())
		{
			CVarBlockPtr oldProperties = m_pLuaProperties;
			m_pLuaProperties = CloneProperties(m_pEntityScript->GetDefaultProperties());

			if (xmlProperties)
			{
				m_pLuaProperties->Serialize(xmlProperties, true);
			}
			else if (oldProperties)
			{
				// If we had propertied before copy their values to new script.
				m_pLuaProperties->CopyValuesByName(oldProperties);
			}

			// High limit for snow flake count for snow entity.
			if (IVariable* pSubBlockSnowFall = m_pLuaProperties->FindVariable("SnowFall"))
			{
				if (IVariable* pSnowFlakeCount = FindVariableInSubBlock(m_pLuaProperties, pSubBlockSnowFall, "nSnowFlakeCount"))
				{
					pSnowFlakeCount->SetLimits(0, 10000, 0, true, true);
				}
			}

			// hide common params table from properties panel
			if (IVariable* pCommonParamsVar = m_pLuaProperties->FindVariable(COMMONPARAMS_TABLE))
			{
				pCommonParamsVar->SetFlags(pCommonParamsVar->GetFlags() | IVariable::UI_INVISIBLE);
			}
		}
	}

	// Create Entity properties from Script properties..
	if (m_pEntityScript && m_pEntityScript->GetDefaultProperties2())
	{
		CVarBlockPtr oldProperties = m_pLuaProperties2;
		m_pLuaProperties2 = CloneProperties(m_pEntityScript->GetDefaultProperties2());

		if (xmlProperties2)
		{
			m_pLuaProperties2->Serialize(xmlProperties2, true);
		}
		else if (oldProperties)
		{
			// If we had properties before copy their values to new script.
			m_pLuaProperties2->CopyValuesByName(oldProperties);
		}
	}

	ResetCallbacks();
}

IVariable* CEntityObject::FindVariableInSubBlock(CVarBlockPtr& properties, IVariable* pSubBlockVar, const char* pVarName)
{
	IVariable* pVar = pSubBlockVar ? pSubBlockVar->FindVariable(pVarName) : properties->FindVariable(pVarName);
	return pVar;
}

void CEntityObject::SpawnEntity()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!m_pEntityClass)
	{
		return;
	}

	// Do not spawn second time.
	if (m_pEntity)
	{
		return;
	}

	m_bLoadFailed = false;

	IEntitySystem* pEntitySystem = GetIEditorImpl()->GetSystem()->GetIEntitySystem();

	if (m_entityId != 0)
	{
		if (pEntitySystem->IsIDUsed(m_entityId))
		{
			m_entityId = 0;
		}
	}

	SEntitySpawnParams params;
	params.pClass = m_pEntityClass;
	params.nFlags = 0;
	params.nFlagsExtended = m_bCloned ? ENTITY_FLAG_EXTENDED_CLONED : 0;
	params.sName = (const char*)GetName();
	params.vPosition = GetPos();
	params.qRotation = GetRotation();
	params.id = m_entityId;
	// Give loaded xml data to the entity spawn function for loading schematyc components, and other component members
	params.entityNode = m_loadedXmlNodeData;

	m_bLight = strstr(params.pClass->GetScriptFile(), "/Lights/") ? true : false;

	if (m_prototype)
	{
		params.pArchetype = m_prototype->GetIEntityArchetype();
	}

	if (IsLegacyObject())
	{
		if (mv_castShadowMinSpec <= GetIEditorImpl()->GetEditorConfigSpec())
		{
			params.nFlags |= ENTITY_FLAG_CASTSHADOW;
		}

		if (mv_outdoor)
		{
			params.nFlags |= ENTITY_FLAG_OUTDOORONLY;
		}
		if (mv_recvWind)
		{
			params.nFlags |= ENTITY_FLAG_RECVWIND;
		}
		if (mv_noDecals)
		{
			params.nFlags |= ENTITY_FLAG_NO_DECALNODE_DECALS;
		}

		params.nFlagsExtended = (params.nFlagsExtended & ~ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK) | ((((int)mv_giMode) << ENTITY_FLAG_EXTENDED_GI_MODE_BIT_OFFSET) & ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK);
	}

	if (params.id == 0)
	{
		params.bStaticEntityId = true; // Tells to Entity system to generate new static id.
	}

	params.guid = ToEntityGuid(GetId());

	// Spawn Entity but not initialize it.
	m_pEntity = pEntitySystem->SpawnEntity(params, false);
	if (m_pEntity)
	{
		using namespace Private_EntityObject;
		m_entityId = m_pEntity->GetId();

		s_entityIdMap[m_entityId] = this;

		CopyPropertiesToScriptTables();

		if (m_pEntityScript)
		{
			m_pEntityScript->SetEventsTable(this);
		}

		// Mark this entity non destroyable.
		m_pEntity->AddFlags(ENTITY_FLAG_UNREMOVABLE);

		// Force transformation on entity.
		XFormGameEntity();

		PreInitLightProperty();

		// Set the parent entity, with the linked object having the highest priority
		if (CBaseObject* pLinkedToObject = GetLinkedTo())
		{
			if (pLinkedToObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				CEntityObject* pLinkedToEntityObject = static_cast<CEntityObject*>(pLinkedToObject);
				params.pParent = pLinkedToEntityObject->GetIEntity();
				CRY_ASSERT(params.pParent != nullptr);
			}
		}

		// Fall back to checking if a parent was set through the legacy path
		if (params.pParent == nullptr)
		{
			if (CBaseObject* pParent = GetParent())
			{
				if (pParent->IsKindOf(RUNTIME_CLASS(CEntityObject)))
				{
					CEntityObject* pParentEntityObject = static_cast<CEntityObject*>(pParent);
					params.pParent = pParentEntityObject->GetIEntity();
					CRY_ASSERT(params.pParent != nullptr);
				}
			}
		}

		// Now update the attachment parameters, if we have a parent set
		if (params.pParent != nullptr)
		{
			params.attachmentParams.m_nAttachFlags = ((m_attachmentType == eAT_GeomCacheNode) ? IEntity::ATTACHMENT_GEOMCACHENODE : 0)
			                                         | ((m_attachmentType == eAT_CharacterBone) ? IEntity::ATTACHMENT_CHARACTERBONE : 0);

			params.attachmentParams.m_target = m_attachmentTarget.GetString();
		}

		//////////////////////////////////////////////////////////////////////////
		// Now initialize entity.
		//////////////////////////////////////////////////////////////////////////
		if (!pEntitySystem->InitEntity(m_pEntity, params))
		{
			m_pEntity = 0;

			m_bLoadFailed = true;
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to initialize Entity %s(Script: %s) %s",
			           (const char*)GetName(),
			           (const char*)m_entityClass,
			           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));

			return;
		}

		BindIEntityChilds();
		UpdateIEntityLinks(false);

		// Make sure the entity is already attached to it's parent so we can update it's transform
		// to be relative to it's parent
		UpdateTransform();

		m_pEntity->Hide(!m_bVisible);

		//////////////////////////////////////////////////////////////////////////
		// If have material, assign it to the entity.
		if (GetMaterial())
		{
			UpdateMaterialInfo();
		}

		// Update render flags of entity (Must be after InitEntity).
		OnRenderFlagsChange(0);

		if (!m_physicsState)
		{
			m_pEntity->SetPhysicsState(m_physicsState);
		}

		//////////////////////////////////////////////////////////////////////////
		// Check if needs to display bbox for this entity.
		//////////////////////////////////////////////////////////////////////////
		m_bCalcPhysics = true;
		if (m_pEntity->GetPhysics() != 0)
		{
			if (m_pEntity->GetPhysics()->GetType() == PE_SOFT)
			{
				m_bCalcPhysics = false;
				//! Ignore entity being updated from physics.
				m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_IGNORE_PHYSICS_UPDATE);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Calculate entity bounding box.
		CalcBBox();

		// the entity can already have a FG if it is being reloaded
		if (m_pFlowGraph)
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Reloading Associated FlowGraph");

			// Re-apply entity for flow graph.
			m_pFlowGraph->SetEntity(this, true);

			IEntityFlowGraphComponent* pFlowGraphProxy = crycomponent_cast<IEntityFlowGraphComponent*>(m_pEntity->CreateProxy(ENTITY_PROXY_FLOWGRAPH));
			IFlowGraph* pGameFlowGraph = m_pFlowGraph->GetIFlowGraph();
			pFlowGraphProxy->SetFlowGraph(pGameFlowGraph);
			if (pGameFlowGraph)
			{
				pGameFlowGraph->SetActive(true);
			}
		}

		if (m_physicsState)
		{
			m_pEntity->SetPhysicsState(m_physicsState);
		}

		ResetEditorClassInfo();
	}
	else
	{
		m_bLoadFailed = true;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Entity %s Failed to Spawn (Script: %s) %s",
		           (const char*)GetName(),
		           (const char*)m_entityClass,
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
	}

	UpdateLightProperty();
	CheckSpecConfig();
}

void CEntityObject::DeleteEntity()
{
	using namespace Private_EntityObject;
	CRY_ASSERT_MESSAGE(!GetIEditorImpl()->GetObjectManager()->IsLoading(), "Entities should not be spawned and unloaded during loading");

	if (m_pEntity)
	{
		UnbindIEntity();
		m_pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		pEntitySystem->RemoveEntity(m_pEntity->GetId(), true);
		s_entityIdMap.erase(m_entityId);
	}
	m_pEntity = 0;
}

void CEntityObject::UnloadScript()
{
	ClearCallbacks();

	if (m_pEntity)
	{
		DeleteEntity();
	}
	if (m_visualObject)
	{
		m_visualObject->Release();
	}
	m_visualObject = 0;
	m_pEntityScript = 0;
}

void CEntityObject::XFormGameEntity()
{
	CBaseObject* pBaseObject = GetParent();
	if (pBaseObject && pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		static_cast<CEntityObject*>(pBaseObject)->XFormGameEntity();
	}

	// Make sure components are correctly calculated.
	if (m_pEntity)
	{
		if (m_pEntity->GetParent())
		{
			// If the entity is linked to its parent in game avoid precision issues from inverse/forward
			// transform calculations by setting the local matrix instead of the world matrix
			const Matrix34& tm = GetLocalTM();
			m_pEntity->SetLocalTM(tm, ENTITY_XFORM_EDITOR);
		}
		else
		{
			const Matrix34& tm = GetWorldTM();
			m_pEntity->SetWorldTM(tm, ENTITY_XFORM_EDITOR);
		}
	}
}

void CEntityObject::CalcBBox()
{
	bool bChanged = false;
	if (m_pEntity != nullptr)
	{
		// Get Local bounding box of entity.
		AABB bbox, originalBBox;
		m_pEntity->GetLocalBounds(bbox);
		originalBBox = bbox;

		if (bbox.min.x >= bbox.max.x || bbox.min.y >= bbox.max.y || bbox.min.z >= bbox.max.z)
		{
			if (m_visualObject)
			{
				Vec3 minp = m_visualObject->GetBoxMin() * m_helperScale * gGizmoPreferences.helperScale;
				Vec3 maxp = m_visualObject->GetBoxMax() * m_helperScale * gGizmoPreferences.helperScale;
				bbox.Add(minp);
				bbox.Add(maxp);
			}
			else
			{
				bbox = AABB(ZERO);
			}
		}
		float minSize = 0.0001f;
		if (fabs(bbox.max.x - bbox.min.x) + fabs(bbox.max.y - bbox.min.y) + fabs(bbox.max.z - bbox.min.z) < minSize)
		{
			bbox.min = -Vec3(minSize, minSize, minSize);
			bbox.max = Vec3(minSize, minSize, minSize);
		}

		if (bbox.min != originalBBox.min || bbox.max != originalBBox.max)
		{
			m_pEntity->SetLocalBounds(bbox, false);
			InvalidateWorldBox();
		}
	}
}

void CEntityObject::SetName(const string& name)
{
	if (name == GetName())
	{
		return;
	}

	const string oldName = GetName();

	CBaseObject::SetName(name);
	if (m_pEntity)
	{
		m_pEntity->SetName((const char*)GetName());
	}

	CListenerSet<IEntityObjectListener*> listeners = m_listeners;
	for (CListenerSet<IEntityObjectListener*>::Notifier notifier(listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnNameChanged(name.GetString());
	}
}

void CEntityObject::SetSelected(bool bSelect)
{
	CBaseObject::SetSelected(bSelect);

	if (m_pEntity)
	{
		//m_pEntity->SetFlags()
		{
			IRenderNode* pRenderNode = m_pEntity->GetRenderNode();
			if (pRenderNode)
			{
				int flags = pRenderNode->GetRndFlags();
				if (bSelect)
				{
					flags |= ERF_SELECTED;
				}
				else
				{
					flags &= ~ERF_SELECTED;
				}
				pRenderNode->SetRndFlags(flags);
			}
		}
	}

	if (bSelect)
	{
		UpdateLightProperty();
	}

	for (CListenerSet<IEntityObjectListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnSelectionChanged(bSelect);
	}

	if (m_pEntity)
	{
		if (IEditorGame* pEditorGame = GetIEditorImpl()->GetGameEngine()->GetIEditorGame())
		{
			pEditorGame->OnEntitySelectionChanged(m_pEntity->GetId(), bSelect);
		}
	}
}

void CEntityObject::OnPropertyChange(IVariable* var)
{
	if (m_pEntityScript != 0 && m_pEntity != 0)
	{
		CopyPropertiesToScriptTables();

		m_pEntityScript->CallOnPropertyChange(m_pEntity);

		if (IsLight() && var && var->GetName())
		{
			if (strstr(var->GetName(), "Flare"))
			{
				UpdateLightProperty();
			}
			else if (!stricmp(var->GetName(), "clrDiffuse"))
			{
				IOpticsElementBasePtr pOptics = GetOpticsElement();
				if (pOptics)
					pOptics->Invalidate();
			}
		}

		// After change of properties bounding box of entity may change.
		CalcBBox();
		InvalidateTM(0);
		// Custom behavior for derived classes
		OnPropertyChangeDone(var);
		// Update prefab data if part of prefab
		UpdatePrefab();
	}
}

template<typename T> struct IVariableType {};
template<> struct IVariableType<bool>
{
	enum { value = IVariable::BOOL };
};
template<> struct IVariableType<int>
{
	enum { value = IVariable::INT };
};
template<> struct IVariableType<float>
{
	enum { value = IVariable::FLOAT };
};
template<> struct IVariableType<string>
{
	enum { value = IVariable::STRING };
};
template<> struct IVariableType<Vec3>
{
	enum { value = IVariable::VECTOR };
};

void CEntityObject::DrawProjectorPyramid(DisplayContext& dc, float dist)
{
	const int numPoints = 16; // per one arc
	const int numArcs = 6;

	if (m_projectorFOV < FLT_EPSILON)
	{
		return;
	}

	Vec3 points[numPoints * numArcs];
	{
		// generate 4 arcs on intersection of sphere with pyramid
		const float fov = DEG2RAD(m_projectorFOV);

		const Vec3 lightAxis(dist, 0.0f, 0.0f);
		const float tanA = tan(fov * 0.5f);
		const float fovProj = asinf(1.0f / sqrtf(2.0f + 1.0f / (tanA * tanA))) * 2.0f;

		const float halfFov = 0.5f * fov;
		const float halfFovProj = fovProj * 0.5f;
		const float anglePerSegmentOfFovProj = 1.0f / (numPoints - 1) * fovProj;

		const Quat yRot = Quat::CreateRotationY(halfFov);
		Vec3* arcPoints = points;
		for (int i = 0; i < numPoints; ++i)
		{
			float angle = i * anglePerSegmentOfFovProj - halfFovProj;
			arcPoints[i] = lightAxis * Quat::CreateRotationZ(angle) * yRot;
		}

		const Quat zRot = Quat::CreateRotationZ(halfFov);
		arcPoints += numPoints;
		for (int i = 0; i < numPoints; ++i)
		{
			float angle = (numPoints - i - 1) * anglePerSegmentOfFovProj - halfFovProj;
			arcPoints[i] = lightAxis * Quat::CreateRotationY(angle) * zRot;
		}

		const Quat nyRot = Quat::CreateRotationY(-halfFov);
		arcPoints += numPoints;
		for (int i = 0; i < numPoints; ++i)
		{
			float angle = (numPoints - i - 1) * anglePerSegmentOfFovProj - halfFovProj;
			arcPoints[i] = lightAxis * Quat::CreateRotationZ(angle) * nyRot;
		}

		const Quat nzRot = Quat::CreateRotationZ(-halfFov);
		arcPoints += numPoints;
		for (int i = 0; i < numPoints; ++i)
		{
			float angle = i * anglePerSegmentOfFovProj - halfFovProj;
			arcPoints[i] = lightAxis * Quat::CreateRotationY(angle) * nzRot;
		}

		// generate cross
		arcPoints += numPoints;
		const float anglePerSegmentOfFov = 1.0f / (numPoints - 1) * fov;
		for (int i = 0; i < numPoints; ++i)
		{
			float angle = i * anglePerSegmentOfFov - halfFov;
			arcPoints[i] = lightAxis * Quat::CreateRotationY(angle);
		}

		arcPoints += numPoints;
		for (int i = 0; i < numPoints; ++i)
		{
			float angle = i * anglePerSegmentOfFov - halfFov;
			arcPoints[i] = lightAxis * Quat::CreateRotationZ(angle);
		}
	}
	// draw pyramid and sphere intersection
	dc.DrawPolyLine(points, numPoints * 4, false);

	// draw cross
	dc.DrawPolyLine(points + numPoints * 4, numPoints, false);
	dc.DrawPolyLine(points + numPoints * 5, numPoints, false);

	Vec3 org(0.0f, 0.0f, 0.0f);
	dc.DrawLine(org, points[numPoints * 0]);
	dc.DrawLine(org, points[numPoints * 1]);
	dc.DrawLine(org, points[numPoints * 2]);
	dc.DrawLine(org, points[numPoints * 3]);
}

void CEntityObject::DrawProjectorFrustum(DisplayContext& dc, Vec2 size, float dist)
{
	static const Vec3 org(0.0f, 0.0f, 0.0f);
	const Vec3 corners[4] =
	{
		Vec3(dist, -size.x, -size.y),
		Vec3(dist, size.x,  -size.y),
		Vec3(dist, -size.x, size.y),
		Vec3(dist, size.x,  size.y)
	};

	dc.DrawLine(org, corners[0]);
	dc.DrawLine(org, corners[1]);
	dc.DrawLine(org, corners[2]);
	dc.DrawLine(org, corners[3]);

	dc.DrawWireBox(Vec3(dist, -size.x, -size.y), Vec3(dist, size.x, size.y));
}

void CEntityObject::DrawEntityLinks(DisplayContext& dc)
{
	if (dc.flags & DISPLAY_LINKS)
	{
		for (const auto& link : m_links)
		{
			if (CEntityObject* pTarget = link.GetTarget())
			{
				dc.DrawLine(this->GetWorldPos(), pTarget->GetWorldPos(), ColorF(0.0f, 1.0f, 0.0f), ColorF(0.0f, 1.0f, 0.0f));

				Vec3 pos = 0.5f * (this->GetWorldPos() + pTarget->GetWorldPos());

				float camDist = dc.camera->GetPosition().GetDistance(pos);
				float maxDist = gViewportPreferences.labelsDistance;
				if (camDist < gViewportPreferences.labelsDistance)
				{
					float rangeRatio = 1.0f;
					float range = maxDist / 2.0f;
					if (camDist > range)
					{
						rangeRatio = 1.0f * (1.0f - (camDist - range) / range);
					}
					dc.SetColor(0.4f, 1.0f, 0.0f, rangeRatio);
					dc.DrawTextLabel(pos + Vec3(0, 0, 0.2f), 1.2f, link.name);
				}
			}
		}
	}
}

void CEntityObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	if (!gViewportDebugPreferences.showEntityObjectHelper)
		return;

	if (!m_pEntity)
	{
		return;
	}

	float fWidth = m_fAreaWidth * 0.5f;
	float fHeight = m_fAreaHeight * 0.5f;

	Matrix34 wtm = GetWorldTM();

	COLORREF col = GetColor();
	if (IsFrozen())
	{
		col = dc.GetFreezeColor();
	}

	dc.PushMatrix(wtm);

	if (m_bDisplayArrow)
	{
		// Show direction arrow.
		Vec3 dir = FORWARD_DIRECTION;
		if (IsFrozen())
		{
			dc.SetFreezeColor();
		}
		else
		{
			dc.SetColor(1, 1, 0);
		}
		dc.DrawArrow(Vec3(0, 0, 0), FORWARD_DIRECTION * m_helperScale, m_helperScale);
	}

	bool bDisplaySolidBox = (m_bDisplaySolidBBox && gViewportPreferences.showTriggerBounds);
	if (IsSelected())
	{
		dc.SetSelectedColor(0.5f);
		if (m_bDisplayBBox || (dc.flags & DISPLAY_2D))
		{
			bDisplaySolidBox = m_bDisplaySolidBBox;
			AABB box;
			GetLocalBounds(box);
			dc.DrawWireBox(box.min, box.max);
		}
	}
	else
	{
		if ((m_bDisplayBBox && gViewportPreferences.showTriggerBounds) || (dc.flags & DISPLAY_2D))
		{
			dc.SetColor(col, 0.3f);
			AABB box;
			GetLocalBounds(box);
			dc.DrawWireBox(box.min, box.max);
		}
	}

	// Only display solid BBox if visual object is associated with the entity.
	if (bDisplaySolidBox)
	{
		dc.DepthWriteOff();
		dc.SetColor(col, 0.25f);
		AABB box;
		GetLocalBounds(box);
		dc.DrawSolidBox(box.min, box.max);
		dc.DepthWriteOn();
	}

	// Draw area light plane.
	if (IsLight() && IsSelected() && m_bAreaLight)
	{
		Vec3 org(0.0f, 0.0f, 0.0f);

		// Draw plane.
		dc.SetColor(m_lightColor, 0.25f);
		dc.DrawSolidBox(Vec3(0, -fWidth, -fHeight), Vec3(0, fWidth, fHeight));

		// Draw outline and direction line.
		dc.SetColor(m_lightColor, 0.75f);
		dc.DrawWireBox(Vec3(0, -fWidth, -fHeight), Vec3(0, fWidth, fHeight));
		dc.DrawLine(org, Vec3(m_proximityRadius, 0, 0), m_lightColor, m_lightColor);

		// Draw cross.
		dc.DrawLine(org, Vec3(0, fWidth, fHeight));
		dc.DrawLine(org, Vec3(0, -fWidth, fHeight));
		dc.DrawLine(org, Vec3(0, fWidth, -fHeight));
		dc.DrawLine(org, Vec3(0, -fWidth, -fHeight));
	}

	// Draw area light helpers.
	if (IsLight() && IsSelected())
	{
		// Draw light disk
		if (m_bProjectorHasTexture && !m_bAreaLight)
		{
			float fShapeRadius = m_fAreaHeight * 0.5f;
			dc.SetColor(m_lightColor, 0.25f);
			dc.DrawCylinder(Vec3(0, 0, 0), Vec3(1, 0, 0), fShapeRadius, 0.001f);
			dc.SetColor(m_lightColor, 0.75f);
			dc.DrawWireSphere(Vec3(0, 0, 0), Vec3(0, fShapeRadius, fShapeRadius));
			dc.SetColor(0, 1, 0, 0.7f);
			DrawProjectorFrustum(dc, Vec2(fShapeRadius, fShapeRadius), 1.0f);
		}
		else if (m_bAreaLight)  // Draw light rectangle
		{
			static const Vec3 org(0.0f, 0.0f, 0.0f);

			// Draw plane.
			dc.SetColor(m_lightColor, 0.25f);
			dc.DrawSolidBox(Vec3(0, -fWidth, -fHeight), Vec3(0, fWidth, fHeight));

			// Draw outline and frustum.
			dc.SetColor(m_lightColor, 0.75f);
			dc.DrawWireBox(Vec3(0, -fWidth, -fHeight), Vec3(0, fWidth, fHeight));

			// Draw cross.
			dc.DrawLine(org, Vec3(0, fWidth, fHeight));
			dc.DrawLine(org, Vec3(0, -fWidth, fHeight));
			dc.DrawLine(org, Vec3(0, fWidth, -fHeight));
			dc.DrawLine(org, Vec3(0, -fWidth, -fHeight));

			dc.SetColor(0, 1, 0, 0.7f);
			DrawProjectorFrustum(dc, Vec2(fWidth, fHeight), 1.0f);
		}
		else // Draw light sphere.
		{
			float fShapeRadius = m_fAreaHeight * 0.5f;
			dc.SetColor(m_lightColor, 0.25f);
			dc.DrawBall(Vec3(0, 0, 0), fShapeRadius);
			dc.SetColor(m_lightColor, 0.75f);
			dc.DrawWireSphere(Vec3(0, 0, 0), fShapeRadius);
		}
	}

	// Draw radii if present and object selected.
	if (gViewportPreferences.alwaysShowRadiuses || IsSelected())
	{
		const Vec3& scale = GetScale();
		float fScale = scale.x; // Ignore matrix scale.
		if (fScale == 0)
		{
			fScale = 1;
		}
		if (m_innerRadius > 0)
		{
			dc.SetColor(0, 1, 0, 0.3f);
			dc.DrawWireSphere(Vec3(0, 0, 0), m_innerRadius / fScale);
		}
		if (m_outerRadius > 0)
		{
			dc.SetColor(1, 1, 0, 0.8f);
			dc.DrawWireSphere(Vec3(0, 0, 0), m_outerRadius / fScale);
		}
		if (m_proximityRadius > 0)
		{
			dc.SetColor(1, 1, 0, 0.8f);

			if (IsLight() && m_bAreaLight)
			{
				float fFOVScale = max(0.001f, m_projectorFOV) / 180.0f;
				float boxWidth = m_proximityRadius + fWidth * fFOVScale;
				float boxHeight = m_proximityRadius + fHeight * fFOVScale;
				dc.DrawWireBox(Vec3(0.0, -boxWidth, -boxHeight), Vec3(m_proximityRadius, boxWidth, boxHeight));
			}

			if (IsLight() && m_bProjectorHasTexture && !m_bAreaLight)
			{
				DrawProjectorPyramid(dc, m_proximityRadius);
			}

			if (!IsLight() || (!m_bProjectorHasTexture && !m_bAreaLight) || m_bProjectInAllDirs)
			{
				if (m_bDisplayAbsoluteRadius)
				{
					AffineParts ap;
					//HINT: we need to do this because the entity class does not have a method to get final entity world scale, nice to have one in the future
					ap.SpectralDecompose(wtm);
					dc.DrawWireSphere(Vec3(0, 0, 0), Vec3(m_proximityRadius / ap.scale.x, m_proximityRadius / ap.scale.y, m_proximityRadius / ap.scale.z));
				}
				else
				{
					dc.DrawWireSphere(Vec3(0, 0, 0), m_proximityRadius);
				}
			}
		}
	}

	dc.PopMatrix();

	// Entities themselves are rendered by 3DEngine.

	if (m_visualObject && ((!gViewportPreferences.showIcons && !gViewportPreferences.showSizeBasedIcons) || !HaveTextureIcon()))
	{
		Matrix34 tm(wtm);
		float sz = m_helperScale * gGizmoPreferences.helperScale;
		tm.ScaleColumn(Vec3(sz, sz, sz));

		SRendParams rp;
		Vec3 color;
		if (IsSelected())
		{
			color = CMFCUtils::Rgb2Vec(dc.GetSelectedColor());
		}
		else
		{
			color = CMFCUtils::Rgb2Vec(col);
		}

		rp.AmbientColor = ColorF(color[0], color[1], color[2], 1);
		rp.dwFObjFlags |= FOB_TRANS_MASK;
		rp.fAlpha = 1;
		rp.pMatrix = &tm;
		rp.pMaterial = GetIEditorImpl()->GetIconManager()->GetHelperMaterial();

		SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(gEnv->p3DEngine->GetRenderingCamera(), SRenderingPassInfo::DEFAULT_FLAGS, true, dc.GetDisplayContextKey());
		m_visualObject->Render(rp, passInfo);
	}

	if (IsSelected())
	{
		if (m_pEntity)
		{
			IAIObject* pAIObj = m_pEntity->GetAI();
			if (pAIObj)
			{
				DrawAIInfo(dc, pAIObj);
			}
		}
	}

	if ((dc.flags & DISPLAY_HIDENAMES) && gViewportPreferences.drawEntityLabels)
	{
		// If labels hidden but we draw entity labels enabled, always draw them.
		CGroup* pGroup = (CGroup*)GetGroup();
		if (!pGroup || pGroup->IsOpen())
		{
			DrawLabel(dc, GetWorldPos(), col);
		}
	}

	if (m_pEntity)
	{
		SGeometryDebugDrawInfo dd;
		dd.tm = wtm;
		dd.bDrawInFront = false;
		dd.color = ColorB(CMFCUtils::Rgb2Vec(col), 120);
		dd.bNoLines = true;

		SEntityPreviewContext preview(dd);
		preview.bNoRenderNodes = true;
		preview.bSelected = IsSelected();
		preview.bRenderSlots = false;
		m_pEntity->PreviewRender(preview);
	}

	DrawDefault(dc, col);
}

void CEntityObject::GetDisplayBoundBox(AABB& box)
{
	AABB bbox;

	GetBoundBox(bbox);

	for (const auto& link : m_links)
	{
		if (CEntityObject* pTarget = link.GetTarget())
		{
			bbox.Add(pTarget->GetWorldPos());
		}
	}

	box = bbox;
}

void CEntityObject::DrawAIInfo(DisplayContext& dc, IAIObject* aiObj)
{
	assert(aiObj);

	IAIActor* pAIActor = aiObj->CastToIAIActor();
	if (!pAIActor)
	{
		return;
	}

	const AgentParameters& ap = pAIActor->GetParameters();

	// Draw ranges.
	bool bTerrainCircle = false;
	Vec3 wp = GetWorldPos();
	float z = GetIEditorImpl()->GetTerrainElevation(wp.x, wp.y);
	if (fabs(wp.z - z) < 5)
	{
		bTerrainCircle = true;
	}

	dc.SetColor(RGB(255 / 2, 0, 0));
	if (bTerrainCircle)
	{
		dc.DrawTerrainCircle(wp, ap.m_PerceptionParams.sightRange / 2, 0.2f);
	}
	else
	{
		dc.DrawCircle(wp, ap.m_PerceptionParams.sightRange / 2);
	}
}

void CEntityObject::Serialize(CObjectArchive& ar)
{
	CBaseObject::Serialize(ar);
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		// Store xml data relevant for this node containing info about saved components.
		m_loadedXmlNodeData = xmlNode->clone();

		// Load
		string entityClass = m_entityClass;
		m_bLoadFailed = false;

		if (!m_prototype)
		{
			xmlNode->getAttr("EntityClass", entityClass);

			xmlNode->getAttr("ClassGUID", m_entityClassGUID);

			if (ar.bUndo)
			{
				XmlNodeRef propsNode = xmlNode->findChild("Properties");
				if (m_pLuaProperties)
				{
					m_pLuaProperties->Serialize(propsNode, ar.bLoading);
				}
			}
		}
		m_physicsState = xmlNode->findChild("PhysicsState");

		Vec3 angles;
		// Backward compatibility, with FarCry levels.
		if (xmlNode->getAttr("Angles", angles))
		{
			angles = DEG2RAD(angles);
			angles.z += gf_PI / 2;
			Quat quat;
			quat.SetRotationXYZ(Ang3(angles));
			SetRotation(quat);
		}

		// Load Event Targets.
		ReleaseEventTargets();
		XmlNodeRef eventTargets = xmlNode->findChild("EventTargets");
		if (eventTargets)
		{
			for (int i = 0; i < eventTargets->getChildCount(); i++)
			{
				XmlNodeRef eventTarget = eventTargets->getChild(i);
				CEntityEventTarget et;
				et.target = 0;
				CryGUID targetId = CryGUID::Null();
				eventTarget->getAttr("TargetId", targetId);
				eventTarget->getAttr("Event", et.event);
				eventTarget->getAttr("SourceEvent", et.sourceEvent);
				m_eventTargets.push_back(et);
				if (targetId != CryGUID::Null())
				{
					ar.SetResolveCallback(this, targetId, functor(*this, &CEntityObject::ResolveEventTarget), i);
				}
			}
		}

		string attachmentType;
		xmlNode->getAttr("AttachmentType", attachmentType);

		if (attachmentType == "GeomCacheNode")
		{
			m_attachmentType = eAT_GeomCacheNode;
		}
		else if (attachmentType == "CharacterBone")
		{
			m_attachmentType = eAT_CharacterBone;
		}
		else
		{
			m_attachmentType = eAT_Pivot;
		}

		xmlNode->getAttr("AttachmentTarget", m_attachmentTarget);

		bool bLoaded = SetClass(entityClass, false, ar.node);

		if (ar.bUndo)
		{
			bool bHasEntity = true;
			xmlNode->getAttr("HasEntity", bHasEntity);
			if (bHasEntity)
			{
				RemoveAllEntityLinks();
				SpawnEntity();
				PostLoad(ar);
			}
		}

		if ((mv_castShadowMinSpec == CONFIG_LOW_SPEC) && !mv_castShadow) // backwards compatibility check
		{
			mv_castShadowMinSpec = END_CONFIG_SPEC_ENUM;
			mv_castShadow = true;
		}
	}
	else
	{
		if (m_attachmentType != eAT_Pivot)
		{
			if (m_attachmentType == eAT_GeomCacheNode)
			{
				xmlNode->setAttr("AttachmentType", "GeomCacheNode");
			}
			else if (m_attachmentType == eAT_CharacterBone)
			{
				xmlNode->setAttr("AttachmentType", "CharacterBone");
			}

			xmlNode->setAttr("AttachmentTarget", m_attachmentTarget);
		}

		// Saving.
		if (!m_entityClass.IsEmpty() && m_prototype == NULL)
		{
			xmlNode->setAttr("EntityClass", m_entityClass);
		}

		if (!m_entityClassGUID.IsNull())
		{
			xmlNode->setAttr("ClassGUID", m_entityClassGUID);
		}

		if (m_physicsState)
		{
			xmlNode->addChild(m_physicsState);
		}

		XmlNodeRef propsNode = xmlNode->newChild("Properties");

		if (!m_prototype)
		{
			//! Save properties.
			if (m_pLuaProperties)
			{
				m_pLuaProperties->Serialize(propsNode, ar.bLoading);
			}
		}

		if (m_pEntity != nullptr)
		{
			m_pEntity->SerializeXML(xmlNode, false, false);
		}

		//! Save properties.
		if (m_pLuaProperties2)
		{
			XmlNodeRef propsNode = xmlNode->newChild("Properties2");
			m_pLuaProperties2->Serialize(propsNode, ar.bLoading);
		}

		// Save Event Targets.
		if (!m_eventTargets.empty())
		{
			XmlNodeRef eventTargets = xmlNode->newChild("EventTargets");
			for (int i = 0; i < m_eventTargets.size(); i++)
			{
				CEntityEventTarget& et = m_eventTargets[i];
				CryGUID targetId = CryGUID::Null();
				if (et.target != 0)
				{
					targetId = et.target->GetId();
				}

				XmlNodeRef eventTarget = eventTargets->newChild("EventTarget");
				eventTarget->setAttr("TargetId", targetId);
				eventTarget->setAttr("Event", et.event);
				eventTarget->setAttr("SourceEvent", et.sourceEvent);
			}
		}

		// Save Entity Links.
		SaveLink(xmlNode);

		xmlNode->setAttr("HasEntity", m_pEntity != nullptr);

		// Save flow graph.
		if (m_pFlowGraph && !m_pFlowGraph->IsEmpty())
		{
			XmlNodeRef graphNode = xmlNode->newChild("FlowGraph");
			m_pFlowGraph->Serialize(graphNode, false, &ar);
		}
	}
}

void CEntityObject::PostLoad(CObjectArchive& ar)
{
	LOADING_TIME_PROFILE_SECTION;
	if (m_pEntity)
	{
		// Force entities to register them-self in sectors.
		// force entity to be registered in terrain sectors again.
		XFormGameEntity();
		BindToParent();
		BindIEntityChilds();
		if (m_pEntityScript)
		{
			m_pEntityScript->SetEventsTable(this);
		}
		if (m_physicsState)
		{
			m_pEntity->SetPhysicsState(m_physicsState);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Load Links.
	XmlNodeRef linksNode = ar.node->findChild("EntityLinks");
	LoadLink(linksNode, &ar);

	//////////////////////////////////////////////////////////////////////////
	// Load flow graph after loading of everything.
	XmlNodeRef graphNode = ar.node->findChild("FlowGraph");
	if (graphNode)
	{
		if (!m_pFlowGraph)
		{
			SetFlowGraph(GetIEditorImpl()->GetFlowGraphManager()->CreateGraphForEntity(this));
		}
		if (m_pFlowGraph && m_pFlowGraph->GetIFlowGraph())
		{
			m_pFlowGraph->ExtractObjectsPrefabIdToGlobalIdMappingFromPrefab(ar);
			m_pFlowGraph->Serialize(graphNode, true, &ar);
		}
	}
	else
	{
		if (m_pFlowGraph)
		{
			SetFlowGraph(nullptr);
		}
	}

	if (m_pEntity)
	{
		SEntityEvent event;
		event.event = ENTITY_EVENT_POST_SERIALIZE;
		m_pEntity->SendEvent(event);
	}
}

void CEntityObject::CheckSpecConfig()
{
	if (m_pEntity && m_pEntity->GetAI() && GetMinSpec() != 0)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "AI entity %s ->> spec dependent", (const char*)GetName());
	}
}

XmlNodeRef CEntityObject::Export(const string& levelPath, XmlNodeRef& xmlExportNode)
{
	if (m_bLoadFailed)
	{
		return 0;
	}

	// Do not export entity with bad id.
	if (!m_entityId)
	{
		return XmlHelpers::CreateXmlNode("Temp");
	}

	CheckSpecConfig();

	// Export entities to entities.ini
	XmlNodeRef objNode = xmlExportNode->newChild("Entity");

	objNode->setAttr("Name", GetName());

	if (GetMaterial())
	{
		objNode->setAttr("Material", GetMaterial()->GetName());
	}

	if (m_prototype)
	{
		objNode->setAttr("Archetype", m_prototype->GetFullName());
	}

	Vec3 pos = GetPos(), scale = GetScale();
	Quat rotate = GetRotation();

	CBaseObject* pParent = GetLinkedTo();
	if (!pParent)
		pParent = GetParent();

	if (GetGroup() && GetGroup()->GetParent() && GetGroup()->GetParent()->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		// Store the parent entity id of the group.
		CEntityObject* parentEntity = (CEntityObject*)(GetGroup()->GetParent());
		if (parentEntity)
		{
			objNode->setAttr("ParentGuid", parentEntity->GetId());
			Matrix34 invParentTM = parentEntity->GetWorldTM().GetInverted();
			Matrix34 localTM = invParentTM * GetWorldTM();
			AffineParts ap;
			ap.SpectralDecompose(localTM);
			pos = ap.pos;
			rotate = ap.rot;
			scale = ap.scale;
		}
	}
	else if (pParent)
	{
		if (pParent->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			// Store parent entity id.
			CEntityObject* parentEntity = (CEntityObject*)pParent;
			if (parentEntity)
			{
				objNode->setAttr("ParentGuid", parentEntity->GetId());

				if (m_attachmentType != eAT_Pivot)
				{
					if (m_attachmentType == eAT_GeomCacheNode)
					{
						objNode->setAttr("AttachmentType", "GeomCacheNode");
					}
					else if (m_attachmentType == eAT_CharacterBone)
					{
						objNode->setAttr("AttachmentType", "CharacterBone");
					}

					objNode->setAttr("AttachmentTarget", m_attachmentTarget);
				}
			}
		}
		else
		{
			// Export world coordinates.
			AffineParts ap;
			ap.SpectralDecompose(GetWorldTM());
			pos = ap.pos;
			rotate = ap.rot;
			scale = ap.scale;
		}
	}

	if (!pos.IsZero())
	{
		objNode->setAttr("Pos", pos);
	}

	if (!rotate.IsIdentity())
	{
		objNode->setAttr("Rotate", rotate);
	}

	if (scale != Vec3(1, 1, 1))
	{
		objNode->setAttr("Scale", scale);
	}

	objNode->setTag("Entity");
	objNode->setAttr("EntityClass", m_entityClass);
	if (!m_entityClassGUID.IsNull())
	{
		objNode->setAttr("ClassGUID", m_entityClassGUID);
	}
	objNode->setAttr("EntityId", m_entityId);
	objNode->setAttr("EntityGuid", ToEntityGuid(GetId()));

	if (IsLegacyObject())
	{
		if (mv_ratioLOD != 100)
		{
			objNode->setAttr("LodRatio", (int)mv_ratioLOD);
		}

		if (mv_ratioViewDist != 100)
		{
			objNode->setAttr("ViewDistRatio", (int)mv_ratioViewDist);
		}

		objNode->setAttr("CastShadowMinSpec", mv_castShadowMinSpec);

		objNode->setAttr("GIMode", mv_giMode);

		if (mv_recvWind)
		{
			objNode->setAttr("RecvWind", true);
		}

		if (mv_noDecals)
		{
			objNode->setAttr("NoDecals", true);
		}

		if (mv_outdoor)
		{
			objNode->setAttr("OutdoorOnly", true);
		}

		if (mv_hiddenInGame)
		{
			objNode->setAttr("HiddenInGame", true);
		}
	}

	if (GetMinSpec() != 0)
	{
		objNode->setAttr("MinSpec", (uint32)GetMinSpec());
	}

	uint32 nMtlLayersMask = GetMaterialLayersMask();
	if (nMtlLayersMask != 0)
	{
		objNode->setAttr("MatLayersMask", nMtlLayersMask);
	}

	if (m_physicsState)
	{
		objNode->addChild(m_physicsState);
	}

	if (GetLayer()->GetName())
	{
		objNode->setAttr("Layer", GetLayer()->GetName());
	}

	// Export Event Targets.
	if (!m_eventTargets.empty())
	{
		XmlNodeRef eventTargets = objNode->newChild("EventTargets");
		for (int i = 0; i < m_eventTargets.size(); i++)
		{
			CEntityEventTarget& et = m_eventTargets[i];

			int entityId = 0;
			EntityGUID entityGuid;
			if (et.target)
			{
				if (et.target->IsKindOf(RUNTIME_CLASS(CEntityObject)))
				{
					entityId = ((CEntityObject*)et.target)->GetEntityId();
					entityGuid = ToEntityGuid(et.target->GetId());
				}
			}

			XmlNodeRef eventTarget = eventTargets->newChild("EventTarget");
			eventTarget->setAttr("Target", entityId);
			eventTarget->setAttr("Event", et.event);
			eventTarget->setAttr("SourceEvent", et.sourceEvent);
		}
	}

	// Save Entity Links.
	bool bForceDisableCheapLight = false;

	if (!m_links.empty())
	{
		XmlNodeRef linksNode = objNode->newChild("EntityLinks");
		for (auto linkIt = m_links.begin(); linkIt != m_links.end(); ++linkIt)
		{
			XmlNodeRef linkNode = linksNode->newChild("Link");
			CEntityObject* pLinkTarget = linkIt->GetTarget();
			CRY_ASSERT(pLinkTarget);

			if (pLinkTarget != nullptr)
			{
				linkNode->setAttr("TargetId", pLinkTarget->GetEntityId());
				linkNode->setAttr("TargetGuid", pLinkTarget->GetId());
				linkNode->setAttr("Name", linkIt->name);

				if (pLinkTarget->GetType() == OBJTYPE_VOLUMESOLID)
					bForceDisableCheapLight = true;
			}
		}
	}

	XmlNodeRef propsNode = objNode->newChild("Properties");

	//! Export properties.
	if (m_pLuaProperties)
	{
		m_pLuaProperties->Serialize(propsNode, false);
	}
	//! Export properties.
	if (m_pLuaProperties2)
	{
		XmlNodeRef propsNode = objNode->newChild("Properties2");
		m_pLuaProperties2->Serialize(propsNode, false);
	}

	if (m_pEntity != nullptr)
	{
		// Export internal entity data.
		m_pEntity->SerializeXML(objNode, false, false);
	}

	// Save flow graph.
	if (m_pFlowGraph && !m_pFlowGraph->IsEmpty())
	{
		XmlNodeRef graphNode = objNode->newChild("FlowGraph");
		m_pFlowGraph->Serialize(graphNode, false, 0);
	}

	return objNode;
}

void CEntityObject::OnEvent(ObjectEvent event)
{
	CBaseObject::OnEvent(event);

	switch (event)
	{
	case EVENT_PREINGAME:
		if (m_pEntity)
		{
			// Entity must be hidden when going to game.
			if (m_bVisible && mv_hiddenInGame)
			{
				m_pEntity->Hide(true);
			}
		}
		break;
	case EVENT_INGAME:
	case EVENT_OUTOFGAME:
		if (m_pEntity)
		{
			if (event == EVENT_INGAME)
			{
				if (!m_bCalcPhysics)
				{
					m_pEntity->ClearFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE);
				}
			}
			else if (event == EVENT_OUTOFGAME)
			{
				// Entity must be returned to editor visibility state.
				m_pEntity->Hide(!m_bVisible);
			}
			XFormGameEntity();
			OnRenderFlagsChange(0);

			if (event == EVENT_OUTOFGAME)
			{
				if (!m_bCalcPhysics)
				{
					m_pEntity->SetFlags(ENTITY_FLAG_IGNORE_PHYSICS_UPDATE);
				}
			}
		}
		break;
	case EVENT_REFRESH:
		if (m_pEntity)
		{
			// force entity to be registered in terrain sectors again.

			//-- little hack to force reregistration of entities
			//<<FIXME>> when issue with registration in editor is resolved
			Vec3 pos = GetPos();
			pos.z += 1.f;
			m_pEntity->SetPos(pos);
			//----------------------------------------------------

			XFormGameEntity();
		}
		break;

	case EVENT_RELOAD_ENTITY:
		if (IsLegacyObject())
		{
			if (m_pEntityScript)
			{
				m_pEntityScript->Reload();
			}
			Reload();
		}
		else
		{
			Reload();

			GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
		}
		break;

	case EVENT_RELOAD_GEOM:
		if (IsLegacyObject())
		{
			Reload();
		}
		break;

	case EVENT_PHYSICS_GETSTATE:
		AcceptPhysicsState();
		break;
	case EVENT_PHYSICS_RESETSTATE:
		ResetPhysicsState();
		break;
	case EVENT_PHYSICS_APPLYSTATE:
		if (m_pEntity && m_physicsState)
		{
			m_pEntity->SetPhysicsState(m_physicsState);
		}
		break;

	case EVENT_FREE_GAME_DATA:
		FreeGameData();
		break;

	case EVENT_CONFIG_SPEC_CHANGE:
		{
			OnEntityFlagsChange(NULL);
			break;
		}
	}
}

void CEntityObject::Reload(bool bReloadScript)
{
	if (GetIEditor()->GetDocument()->IsClosing())
		return;

	//Serialize entity before deleting, so that Entity Components are saved
	XmlNodeRef entityXmlNode;
	if (m_pEntity)
	{
		entityXmlNode = XmlHelpers::CreateXmlNode("EntityObjectExport");
		m_pEntity->SerializeXML(entityXmlNode, false, false, true);
		m_loadedXmlNodeData = entityXmlNode;
	}

	EntityId oldEntityId = m_entityId;

	if (!m_pEntityScript || bReloadScript)
	{
		// TODO: In case the entity was renamed in asset browser and therefore unregistered this
		//			 points to an released entity class.
		m_pEntityClass = nullptr;

		SetClass(m_entityClass, true);
	}

	DeleteEntity();
	SpawnEntity();

	// check if entityId changed due to reload
	// if it changed, kick off a setEntityId event
	if (m_pEntity != nullptr && m_entityId != oldEntityId)
	{
		gEnv->pFlowSystem->OnEntityIdChanged(oldEntityId, m_entityId);
	}

	UpdateUIVars();
}

void CEntityObject::UpdateVisibility(bool bVisible)
{
	CBaseObject::UpdateVisibility(bVisible);

	bool bVisibleWithSpec = bVisible && !IsHiddenBySpec();
	if (bVisibleWithSpec != m_bVisible)
	{
		m_bVisible = bVisibleWithSpec;
	}

	if (m_pEntity)
	{
		m_pEntity->Hide(!m_bVisible);
	}
};

void CEntityObject::DrawDefault(DisplayContext& dc, COLORREF labelColor)
{
	CBaseObject::DrawDefault(dc, labelColor);

	bool bDisplaySelectionHelper = false;
	if (m_pEntity && CanBeDrawn(dc, bDisplaySelectionHelper))
	{
		const Vec3 wp = m_pEntity->GetWorldPos();

		if (gEnv->pAISystem)
		{
			ISmartObjectManager* pSmartObjectManager = gEnv->pAISystem->GetSmartObjectManager();
			if (!pSmartObjectManager->ValidateSOClassTemplate(m_pEntity))
			{
				DrawLabel(dc, wp, RGB(255, 0, 0), 1.f, 4);
			}
			if (IsSelected() || IsHighlighted())
			{
				pSmartObjectManager->DrawSOClassTemplate(m_pEntity);
			}
		}

		// Draw "ghosted" data around the entity's actual position in simulation mode
		if (GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
		{
			if (bDisplaySelectionHelper)
			{
				DrawSelectionHelper(dc, wp, labelColor, 0.5f);
			}
			else if (!(dc.flags & DISPLAY_HIDENAMES))
			{
				DrawLabel(dc, wp, labelColor, 0.5f);
			}

			if (GetIEditor()->IsHelpersDisplayed())
				DrawTextureIcon(dc, wp, 0.5f, bDisplaySelectionHelper);
		}
	}

	DrawEntityLinks(dc);
}

void CEntityObject::DrawTextureIcon(DisplayContext& dc, const Vec3& pos, float alpha, bool bDisplaySelectionHelper, float distanceSquared)
{
	if (!gViewportPreferences.showSizeBasedIcons && !gViewportPreferences.showIcons)
		return;

	Vec3 iconPosition = pos;

	if (bDisplaySelectionHelper)
	{
		// We need to offset the icon up to not cover text labels
		POINT iconPoint = dc.view->WorldToView(pos);
		iconPoint.y -= UIDRAW_TEXTSIZEFACTOR;
		iconPosition = dc.view->ViewToWorld(iconPoint);
	}

	SetDrawTextureIconProperties(dc, iconPosition);

	int mainIconSize = OBJECT_TEXTURE_ICON_SIZE;
	int componentIconSize = int(OBJECT_TEXTURE_ICON_SIZE * 0.75f);

	constexpr int iconDistX = 2;
	constexpr int iconDistY = 5;

	bool bDrewComponentIcon = false;

	// Draw component icons first
	if (m_pEntity != nullptr)
	{
		// Check if the components of the entity have changed
		if (m_pEntity->GetComponentChangeState() != m_entityDirtyFlag)
		{
			RenewTextureIconsBuffer();
			InvalidateWorldBox();

			// Update the dirty flag
			m_entityDirtyFlag = m_pEntity->GetComponentChangeState();
		}

		constexpr int maxColumnCount = 5;

		Vec3 iconPos = GetTextureIconDrawPos();

		bDrewComponentIcon = m_componentIconTextureIds.size() > 0;

		for (int i = 0, n = m_componentIconTextureIds.size(); i < n; ++i)
		{
			int rowIndex = i / maxColumnCount;
			int columnIndex = i - rowIndex * maxColumnCount;

			int lastRowColumnCount = n % maxColumnCount;
			int numColumnItems = i < n - lastRowColumnCount ? maxColumnCount : lastRowColumnCount;

			POINT iconPoint = dc.view->WorldToView(GetTextureIconDrawPos());

			// Move the start position towards the left, since we want to center the items
			iconPoint.x -= (float)numColumnItems * 0.5f * (componentIconSize + iconDistX) - mainIconSize / 2;

			// Offset column
			iconPoint.x += (componentIconSize + iconDistX) * columnIndex;

			// Offset row
			iconPoint.y -= mainIconSize + (componentIconSize + iconDistY) * rowIndex;

			iconPos = dc.view->ViewToWorld(iconPoint);

			dc.DrawTextureLabel(iconPos, componentIconSize, componentIconSize, m_componentIconTextureIds[i],
			                    GetTextureIconFlags(), 0, 0, OBJECT_TEXTURE_ICON_SCALE);
		}
	}

	// Draw entity class icon
	if (GetTextureIcon())
	{
		dc.DrawTextureLabel(GetTextureIconDrawPos(), mainIconSize, mainIconSize, GetTextureIcon(), GetTextureIconFlags(),
		                    0, 0, OBJECT_TEXTURE_ICON_SCALE);
	}
}

void CEntityObject::RenewTextureIconsBuffer()
{
	// Clear all icons in the buffer since we don't know which icon changed
	m_componentIconTextureIds.clear();

	DynArray<IEntityComponent*> existingComponents;
	m_pEntity->GetComponents(existingComponents);

	for (IEntityComponent* pComponent : existingComponents)
	{
		const char* szIcon = pComponent->GetClassDesc().GetIcon();

		if (szIcon != nullptr && szIcon[0] != '\0')
		{
			CryIcon icon(szIcon);
			int iconId = GetIEditor()->GetIconManager()->GetIconTexture(szIcon, icon);

			if (iconId != 0)
			{
				m_componentIconTextureIds.push_back(iconId);
			}
		}
	}
}

bool CEntityObject::IsInCameraView(const CCamera& camera)
{
	bool bResult = CBaseObject::IsInCameraView(camera);

	if (!bResult && m_pEntity && GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
	{
		AABB bbox;
		m_pEntity->GetWorldBounds(bbox);
		bResult = (camera.IsAABBVisible_F(AABB(bbox.min, bbox.max)));
	}

	return bResult;
}

float CEntityObject::GetCameraVisRatio(const CCamera& camera)
{
	float visRatio = CBaseObject::GetCameraVisRatio(camera);

	if (m_pEntity && GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
	{
		AABB bbox;
		m_pEntity->GetWorldBounds(bbox);

		float objectHeightSq = max(1.0f, (bbox.max - bbox.min).GetLengthSquared());
		float camdistSq = (bbox.min - camera.GetPosition()).GetLengthSquared();
		if (camdistSq > FLT_EPSILON)
		{
			visRatio = max(visRatio, objectHeightSq / camdistSq);
		}
	}

	return visRatio;
}

bool CEntityObject::IntersectRectBounds(const AABB& bbox)
{
	bool bResult = CBaseObject::IntersectRectBounds(bbox);

	// Check real entity in simulation mode as well
	if (!bResult && m_pEntity && GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
	{
		AABB aabb;
		m_pEntity->GetWorldBounds(aabb);

		bResult = aabb.IsIntersectBox(bbox);
	}

	return bResult;
}

bool CEntityObject::IntersectRayBounds(const Ray& ray)
{
	bool bResult = CBaseObject::IntersectRayBounds(ray);

	// Check real entity as well
	if (!bResult && m_pEntity)
	{
		Vec3 tmpPnt;
		AABB aabb;
		m_pEntity->GetWorldBounds(aabb);

		bResult = Intersect::Ray_AABB(ray, aabb, tmpPnt);
	}

	return bResult;
}

IVariable* CEntityObject::GetLightVariable(const char* name0) const
{
	if (m_pLuaProperties2)
	{
		IVariable* pLightProperties = m_pLuaProperties2->FindVariable("LightProperties_Base");

		if (pLightProperties)
		{
			for (int i = 0; i < pLightProperties->GetNumVariables(); ++i)
			{
				IVariable* pChild = pLightProperties->GetVariable(i);

				if (pChild == NULL)
				{
					continue;
				}

				string name(pChild->GetName());
				if (name == name0)
				{
					return pChild;
				}
			}
		}
	}

	return m_pLuaProperties ? m_pLuaProperties->FindVariable(name0) : nullptr;
}

string CEntityObject::GetLightAnimation() const
{
	IVariable* pStyleGroup = GetLightVariable("Style");
	if (pStyleGroup)
	{
		for (int i = 0; i < pStyleGroup->GetNumVariables(); ++i)
		{
			IVariable* pChild = pStyleGroup->GetVariable(i);

			if (pChild == nullptr)
			{
				continue;
			}

			if (strcmp(pChild->GetName(), "lightanimation_LightAnimation") == 0)
			{
				string lightAnimationName;
				pChild->Get(lightAnimationName);
				return lightAnimationName;
			}
		}
	}

	return "";
}

void CEntityObject::InvalidateTM(int nWhyFlags)
{
	CBaseObject::InvalidateTM(nWhyFlags);

	if (nWhyFlags & eObjectUpdateFlags_RestoreUndo)   // Can skip updating game object when restoring undo.
	{
		return;
	}

	// Make sure components are correctly calculated.
	const Matrix34& tm = GetWorldTM();
	if (m_pEntity)
	{
		EntityTransformationFlagsMask nWhyEntityFlag;
		if (nWhyFlags & eObjectUpdateFlags_Animated)
		{
			nWhyEntityFlag = ENTITY_XFORM_TRACKVIEW;
		}
		else
		{
			nWhyEntityFlag = ENTITY_XFORM_EDITOR;
		}

		IEntity* pParentEntity = nullptr;
		CBaseObject* pParent = GetLinkedTo();
		if (!pParent)
			pParent = GetParent();

		if (pParent && pParent->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pParentEntityObject = static_cast<CEntityObject*>(pParent);
			pParentEntity = pParentEntityObject->GetIEntity();
		}

		if (pParent && (!m_pEntity->GetParent() ||
		                (m_pEntity->GetParent() != pParentEntity) // different parent in game from parent in editor (e.g. for prefab members)
		                ))
		{
			m_pEntity->SetWorldTM(tm, nWhyEntityFlag);
		}
		else if (GetLookAt())
		{
			m_pEntity->SetWorldTM(tm, nWhyEntityFlag);
		}
		else
		{
			if (nWhyFlags & eObjectUpdateFlags_ParentChanged)
			{
				nWhyEntityFlag |= ENTITY_XFORM_FROM_PARENT;
			}

			m_pEntity->SetPosRotScale(GetPos(), GetRotation(), GetScale(), nWhyEntityFlag);
		}
	}

	m_bEntityXfromValid = false;
}

//! Attach new child node.
void CEntityObject::OnAttachChild(CBaseObject* pChild)
{
	using namespace Private_EntityObject;
	CUndo::Record(new CUndoAttachEntity(this, true));

	if (pChild && pChild->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		((CEntityObject*)pChild)->BindToParent();
	}
	else if (pChild && pChild->IsKindOf(RUNTIME_CLASS(CGroup)))
	{
		((CGroup*)pChild)->BindToParent();
	}
}

// Detach this node from parent.
void CEntityObject::OnDetachThis()
{
	using namespace Private_EntityObject;
	CUndo::Record(new CUndoAttachEntity(this, false));
	UnbindIEntity();
}

void CEntityObject::OnLink(CBaseObject* pParent)
{
	if (!m_pEntity)
	{
		return;
	}

	CBaseObject* pLinkedTo = GetLinkedTo();
	if (pLinkedTo && pLinkedTo->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pLinkedToEntity = static_cast<CEntityObject*>(pLinkedTo);
		IEntity* pLinkedToIEntity = pLinkedToEntity->GetIEntity();

		if (pLinkedToIEntity)
		{
			XFormGameEntity();
			const int flags = ((m_attachmentType == eAT_GeomCacheNode) ? IEntity::ATTACHMENT_GEOMCACHENODE : 0)
			                  | ((m_attachmentType == eAT_CharacterBone) ? IEntity::ATTACHMENT_CHARACTERBONE : 0);
			SChildAttachParams attachParams(flags | IEntity::ATTACHMENT_KEEP_TRANSFORMATION, m_attachmentTarget.GetString());
			pLinkedToIEntity->AttachChild(m_pEntity, attachParams);
			XFormGameEntity();
		}
	}
}

void CEntityObject::OnUnLink()
{
	if (!m_pEntity)
	{
		return;
	}

	CBaseObject* pLinkedTo = GetLinkedTo();
	if (pLinkedTo && pLinkedTo->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pLinkedToEntity = static_cast<CEntityObject*>(pLinkedTo);
		if (pLinkedToEntity->GetIEntity())
		{
			m_pEntity->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
		}
	}
}

Matrix34 CEntityObject::GetLinkAttachPointWorldTM() const
{
	if (m_attachmentType != eAT_Pivot)
	{
		const IEntity* pEntity = GetIEntity();
		if (pEntity && pEntity->GetParent())
		{
			return pEntity->GetParentAttachPointWorldTM();
		}
	}

	return CBaseObject::GetLinkAttachPointWorldTM();
}

bool CEntityObject::IsParentAttachmentValid() const
{
	if (m_attachmentType != eAT_Pivot)
	{
		const IEntity* pEntity = GetIEntity();
		if (pEntity)
		{
			return pEntity->IsParentAttachmentValid();
		}
	}

	return CBaseObject::IsParentAttachmentValid();
}

void CEntityObject::UpdateTransform()
{
	if (m_attachmentType == eAT_GeomCacheNode || m_attachmentType == eAT_CharacterBone)
	{
		IEntity* pEntity = GetIEntity();
		if (pEntity && pEntity->IsParentAttachmentValid())
		{
			const Matrix34& entityTM = pEntity->GetLocalTM();
			SetLocalTM(entityTM, eObjectUpdateFlags_Animated);
		}
	}
}

void CEntityObject::BindToParent()
{
	if (!m_pEntity)
	{
		return;
	}

	CBaseObject* parent = GetParent();
	if (parent)
	{
		if (parent->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* parentEntity = (CEntityObject*)parent;

			IEntity* ientParent = parentEntity->GetIEntity();
			if (ientParent)
			{
				XFormGameEntity();
				const int flags = ((m_attachmentType == eAT_GeomCacheNode) ? IEntity::ATTACHMENT_GEOMCACHENODE : 0)
				                  | ((m_attachmentType == eAT_CharacterBone) ? IEntity::ATTACHMENT_CHARACTERBONE : 0);
				SChildAttachParams attachParams(flags | IEntity::ATTACHMENT_KEEP_TRANSFORMATION, m_attachmentTarget.GetString());
				ientParent->AttachChild(m_pEntity, attachParams);
				XFormGameEntity();
			}
		}
	}
}

void CEntityObject::BindIEntityChilds()
{
	if (!m_pEntity)
	{
		return;
	}

	int numChilds = GetChildCount();
	for (int i = 0; i < numChilds; i++)
	{
		CBaseObject* pChild = GetChild(i);
		if (pChild && pChild->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pChildEntity = static_cast<CEntityObject*>(pChild);
			IEntity* ientChild = pChildEntity->GetIEntity();

			if (ientChild)
			{
				pChildEntity->XFormGameEntity();
				const int flags = ((pChildEntity->m_attachmentType == eAT_GeomCacheNode) ? IEntity::ATTACHMENT_GEOMCACHENODE : 0)
				                  | ((pChildEntity->m_attachmentType == eAT_CharacterBone) ? IEntity::ATTACHMENT_CHARACTERBONE : 0);
				SChildAttachParams attachParams(flags, pChildEntity->m_attachmentTarget.GetString());
				m_pEntity->AttachChild(ientChild, attachParams);
				pChildEntity->XFormGameEntity();
			}
		}
	}
}

void CEntityObject::UnbindIEntity()
{
	if (!m_pEntity)
	{
		return;
	}

	CBaseObject* parent = GetParent();
	if (parent && parent->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* parentEntity = (CEntityObject*)parent;

		IEntity* ientParent = parentEntity->GetIEntity();
		if (ientParent)
		{
			m_pEntity->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
		}
	}
}

void CEntityObject::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	CBaseObject::PostClone(pFromObject, ctx);

	CEntityObject* pFromEntity = (CEntityObject*)pFromObject;
	// Clone event targets.
	if (!pFromEntity->m_eventTargets.empty())
	{
		int numTargets = pFromEntity->m_eventTargets.size();
		for (int i = 0; i < numTargets; i++)
		{
			CEntityEventTarget& et = pFromEntity->m_eventTargets[i];
			CBaseObject* pClonedTarget = ctx.FindClone(et.target);
			if (!pClonedTarget)
			{
				pClonedTarget = et.target;  // If target not cloned, link to original target.
			}

			// Add cloned event.
			AddEventTarget(pClonedTarget, et.event, et.sourceEvent, true);
		}
	}

	// Clone links.
	if (!pFromEntity->m_links.empty())
	{
		int numTargets = pFromEntity->m_links.size();
		for (int i = 0; i < numTargets; i++)
		{
			CEntityLink& et = pFromEntity->m_links[i];
			CEntityObject* pFromEntityTarget = et.GetTarget();
			CBaseObject* pClonedTarget = ctx.FindClone(pFromEntityTarget);
			if (!pClonedTarget)
			{
				pClonedTarget = pFromEntityTarget;  // If target not cloned, link to original target.
			}

			// Add cloned event.
			if (pClonedTarget)
			{
				string entityLinkName = et.name;
				// If the old entity link had a default name (the target object's name) then rename it to the new target's name
				if (pFromEntityTarget->GetName() == et.name)
					entityLinkName = pClonedTarget->GetName();

				AddEntityLink(entityLinkName.c_str(), pClonedTarget->GetId());
			}
			else
			{
				AddEntityLink(et.name.c_str(), CryGUID::Null());
			}
		}
	}

	if (m_pFlowGraph)
	{
		m_pFlowGraph->PostClone(pFromObject, ctx);
	}
}

void CEntityObject::ResolveEventTarget(CBaseObject* object, unsigned int index)
{
	// Find target id.
	assert(index >= 0 && index < m_eventTargets.size());
	if (object)
	{
		if (!object->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ASSERT, "Attempting to link Object %s which is not an EntityObject to entity %s", object->GetName(), GetName());
		}
		object->AddEventListener(functor(*this, &CEntityObject::OnEventTargetEvent));
	}
	m_eventTargets[index].target = object;
	if (!m_eventTargets.empty() && m_pEntityScript != 0)
	{
		m_pEntityScript->SetEventsTable(this);
	}
}

void CEntityObject::RemoveAllEntityLinks()
{
	while (!m_links.empty())
	{
		RemoveEntityLink(m_links.size() - 1);
	}
	m_links.clear();
	SetModified(false, false);
}

void CEntityObject::ReleaseEventTargets()
{
	while (!m_eventTargets.empty())
	{
		RemoveEventTarget(m_eventTargets.size() - 1, false);
	}
	m_eventTargets.clear();
	SetModified(false, false);
}

void CEntityObject::LoadLink(XmlNodeRef xmlNode, CObjectArchive* pArchive)
{
	RemoveAllEntityLinks();

	if (!xmlNode)
	{
		return;
	}

	string name;
	CryGUID targetId;

	for (int i = 0; i < xmlNode->getChildCount(); i++)
	{
		XmlNodeRef linkNode = xmlNode->getChild(i);
		linkNode->getAttr("Name", name);

		if (linkNode->getAttr("TargetId", targetId))
		{
			int version = 0;
			linkNode->getAttr("Version", version);

			CryGUID newTargetId = pArchive ? pArchive->ResolveID(targetId) : targetId;

			// Backwards compatibility with old bone attachment system
			const char kOldBoneLinkPrefix = '@';
			if (version == 0 && name.GetLength() > 0 && name[0] == kOldBoneLinkPrefix)
			{
				CBaseObject* pObject = FindObject(newTargetId);
				if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
				{
					CEntityObject* pTargetEntity = static_cast<CEntityObject*>(pObject);

					Quat relRot(IDENTITY);
					linkNode->getAttr("RelRot", relRot);
					Vec3 relPos(IDENTITY);
					linkNode->getAttr("RelPos", relPos);

					SetAttachType(eAT_CharacterBone);
					SetAttachTarget(name.GetString() + 1);
					pTargetEntity->LinkTo(this);

					SetPos(relPos);
					SetRotation(relRot);
				}
			}
			else
			{
				AddEntityLink(name, newTargetId);
			}
		}
	}
}

void CEntityObject::SaveLink(XmlNodeRef xmlNode)
{
	if (m_links.empty())
		return;

	XmlNodeRef linksNode = xmlNode->newChild("EntityLinks");
	for (int i = 0, num = m_links.size(); i < num; i++)
	{
		if (m_links[i].targetId == CryGUID::Null())
			continue;

		XmlNodeRef linkNode = linksNode->newChild("Link");
		linkNode->setAttr("TargetId", m_links[i].targetId);
		linkNode->setAttr("Name", m_links[i].name);
		linkNode->setAttr("Version", 1);
	}
}

void CEntityObject::OnEventTargetEvent(CBaseObject* target, int event)
{
	// When event target is deleted.
	if (event == OBJECT_ON_DELETE)
	{
		// Find this target in events list and remove.
		int numTargets = m_eventTargets.size();
		for (int i = 0; i < numTargets; i++)
		{
			if (m_eventTargets[i].target == target)
			{
				RemoveEventTarget(i);
				numTargets = m_eventTargets.size();
				i--;
			}
		}
	}
	else if (event == OBJECT_ON_PREDELETE)
	{
		int numTargets = m_links.size();
		for (int i = 0; i < numTargets; i++)
		{
			if (m_links[i].targetId == target->GetId())
			{
				RemoveEntityLink(i);
				numTargets = m_eventTargets.size();
				i--;
			}
		}
	}
}

int CEntityObject::AddEventTarget(CBaseObject* target, const string& event, const string& sourceEvent, bool bUpdateScript)
{
	StoreUndo("Add EventTarget");
	CEntityEventTarget et;
	et.target = target;
	et.event = event;
	et.sourceEvent = sourceEvent;

	// Assign event target.
	if (et.target)
	{
		et.target->AddEventListener(functor(*this, &CEntityObject::OnEventTargetEvent));
	}

	m_eventTargets.push_back(et);

	// Update event table in script.
	if (bUpdateScript && m_pEntityScript != 0)
	{
		m_pEntityScript->SetEventsTable(this);
	}

	SetModified(false, false);
	return m_eventTargets.size() - 1;
}

void CEntityObject::RemoveEventTarget(int index, bool bUpdateScript)
{
	if (index >= 0 && index < m_eventTargets.size())
	{
		StoreUndo("Remove EventTarget");

		if (m_eventTargets[index].target)
		{
			m_eventTargets[index].target->RemoveEventListener(functor(*this, &CEntityObject::OnEventTargetEvent));
		}
		m_eventTargets.erase(m_eventTargets.begin() + index);

		// Update event table in script.
		if (bUpdateScript && m_pEntityScript != 0 && m_pEntity != 0)
		{
			m_pEntityScript->SetEventsTable(this);
		}

		SetModified(false, false);
	}
}

int CEntityObject::AddEntityLink(const string& name, CryGUID targetEntityId)
{
	CEntityObject* target = 0;
	if (targetEntityId != CryGUID::Null())
	{
		CBaseObject* pObject = FindObject(targetEntityId);
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			target = (CEntityObject*)pObject;

			if (target->GetEntityId() == m_pEntity->GetId())
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ASSERT, "Attempting to link Object %s with itself", m_pEntity->GetName());
				return -1;
			}
		}
		else
		{
			if (pObject)
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ASSERT, "Attempting to link Object %s which is not an EntityObject to entity %s", pObject->GetName(), GetName());
			}
			return -1;
		}
	}

	StoreUndo("Add EntityLink");

	// Assign event target.
	if (target)
	{
		target->AddEventListener(functor(*this, &CEntityObject::OnEventTargetEvent));
	}

	CEntityLink lnk;
	lnk.targetId = targetEntityId;
	lnk.name = name;
	m_links.push_back(lnk);

	if (m_pEntity != NULL && target != NULL)
	{
		// Add link to entity itself.
		m_pEntity->AddEntityLink(name, target->GetEntityId(), target->GetId());
		// tell the target about the linkage
		target->EntityLinked(name, GetId());
	}

	SetModified(false, false);
	UpdateGroup();
	UpdateUIVars();

	return m_links.size() - 1;
}

bool CEntityObject::EntityLinkExists(const string& name, CryGUID targetEntityId)
{
	for (int i = 0, num = m_links.size(); i < num; ++i)
	{
		if (m_links[i].targetId == targetEntityId && name.CompareNoCase(m_links[i].name) == 0)
		{
			return true;
		}
	}
	return false;
}

void CEntityObject::RemoveEntityLink(int index)
{
	if (index >= 0 && index < m_links.size())
	{
		CEntityLink& link = m_links[index];
		StoreUndo("Remove EntityLink");

		if (CEntityObject* pTarget = link.GetTarget())
		{
			pTarget->RemoveEventListener(functor(*this, &CEntityObject::OnEventTargetEvent));
			pTarget->EntityUnlinked(link.name.c_str(), GetId());
		}
		m_links.erase(m_links.begin() + index);

		UpdateIEntityLinks();

		SetModified(false, false);
		UpdateGroup();
		UpdateUIVars();
	}
}

void CEntityObject::RenameEntityLink(int index, const string& newName)
{
	if (index >= 0 && index < m_links.size())
	{
		StoreUndo("Rename EntityLink");

		m_links[index].name = newName;

		UpdateIEntityLinks();

		SetModified(false, false);
		UpdateGroup();
		UpdateUIVars();
	}
}

void CEntityObject::UpdateIEntityLinks(bool bCallOnPropertyChange)
{
	if (m_pEntity)
	{
		m_pEntity->RemoveAllEntityLinks();
		for (auto it = m_links.begin(); it != m_links.end(); ++it)
		{
			if (CEntityObject* pTarget = it->GetTarget())
			{
				m_pEntity->AddEntityLink(it->name, pTarget->GetEntityId(), pTarget->GetId());
			}
		}

		if (bCallOnPropertyChange && m_pEntityScript)
		{
			m_pEntityScript->CallOnPropertyChange(m_pEntity);
		}
	}
}

void CEntityObject::OnEntityFlagsChange(IVariable* var)
{
	if (m_pEntity)
	{
		int flags = m_pEntity->GetFlags();
		int flagsEx = m_pEntity->GetFlagsExtended();

		if (mv_castShadowMinSpec <= GetIEditorImpl()->GetEditorConfigSpec())
		{
			flags |= ENTITY_FLAG_CASTSHADOW;
		}
		else
		{
			flags &= ~ENTITY_FLAG_CASTSHADOW;
		}

		if (mv_outdoor)
		{
			flags |= ENTITY_FLAG_OUTDOORONLY;
		}
		else
		{
			flags &= ~ENTITY_FLAG_OUTDOORONLY;
		}

		if (mv_recvWind)
		{
			flags |= ENTITY_FLAG_RECVWIND;
		}
		else
		{
			flags &= ~ENTITY_FLAG_RECVWIND;
		}

		if (mv_noDecals)
		{
			flags |= ENTITY_FLAG_NO_DECALNODE_DECALS;
		}
		else
		{
			flags &= ~ENTITY_FLAG_NO_DECALNODE_DECALS;
		}

		// Detect disabling of IntegrateIntoTerrain feature and request terrain mesh update
		if ((flagsEx & ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK) ^ (mv_giMode << ENTITY_FLAG_EXTENDED_GI_MODE_BIT_OFFSET))
		{
			IRenderNode* pRenderNode = m_pEntity->GetRenderNode();

			if (pRenderNode && pRenderNode->GetGIMode() == IRenderNode::eGM_IntegrateIntoTerrain)
			{
				AABB nodeBox = pRenderNode->GetBBox();
				gEnv->p3DEngine->GetITerrain()->ResetTerrainVertBuffers(&nodeBox);
			}
		}

		flagsEx = (flagsEx & ~ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK) | ((mv_giMode << ENTITY_FLAG_EXTENDED_GI_MODE_BIT_OFFSET) & ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK);

		m_pEntity->SetFlags(flags);
		m_pEntity->SetFlagsExtended(flagsEx);
	}
}

void CEntityObject::OnRenderFlagsChange(IVariable* var)
{
	if (m_pEntity)
	{
		auto renderNodeParams = m_pEntity->GetRenderNodeParams();

		renderNodeParams.SetLodRatio(mv_ratioLOD);

		// With Custom view dist ratio it is set by code not UI.
		if (!(m_pEntity->GetFlags() & ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO))
		{
			renderNodeParams.SetViewDistRatio(mv_ratioViewDist);
		}
		else
		{
			// Disable UI for view distance ratio.
			mv_ratioViewDist.SetFlags(mv_ratioViewDist.GetFlags() | IVariable::UI_DISABLED);
		}

		renderNodeParams.SetRndFlags(ERF_RECVWIND, mv_recvWind);
		renderNodeParams.SetRndFlags(ERF_NO_DECALNODE_DECALS, mv_noDecals);
		renderNodeParams.SetRndFlags(ERF_DYNAMIC_DISTANCESHADOWS, mv_DynamicDistanceShadows);
		renderNodeParams.SetMinSpec((ESystemConfigSpec)GetMinSpec());
		m_pEntity->SetRenderNodeParams(renderNodeParams);

		if (var == &mv_DynamicDistanceShadows)
			gEnv->p3DEngine->SetRecomputeCachedShadows();

		SetModified(false, false);
	}
}

void CEntityObject::OnRadiusChange(IVariable* var)
{
	var->Get(m_proximityRadius);
}

void CEntityObject::OnInnerRadiusChange(IVariable* var)
{
	var->Get(m_innerRadius);
}

void CEntityObject::OnOuterRadiusChange(IVariable* var)
{
	var->Get(m_outerRadius);
}

void CEntityObject::OnBoxSizeXChange(IVariable* var)
{
	var->Get(m_boxSizeX);
}

void CEntityObject::OnBoxSizeYChange(IVariable* var)
{
	var->Get(m_boxSizeY);
}

void CEntityObject::OnBoxSizeZChange(IVariable* var)
{
	var->Get(m_boxSizeZ);
}

void CEntityObject::OnProjectorFOVChange(IVariable* var)
{
	var->Get(m_projectorFOV);
}

void CEntityObject::OnProjectInAllDirsChange(IVariable* var)
{
	int value;
	var->Get(value);
	m_bProjectInAllDirs = value;
}

void CEntityObject::OnProjectorTextureChange(IVariable* var)
{
	string texture;
	var->Get(texture);
	m_bProjectorHasTexture = !texture.IsEmpty();
}

void CEntityObject::OnAreaLightChange(IVariable* var)
{
	int value;
	var->Get(value);
	m_bAreaLight = value;
}

void CEntityObject::OnAreaWidthChange(IVariable* var)
{
	var->Get(m_fAreaWidth);
}

void CEntityObject::OnAreaHeightChange(IVariable* var)
{
	var->Get(m_fAreaHeight);
}

void CEntityObject::OnAreaLightSizeChange(IVariable* var)
{
	var->Get(m_fAreaLightSize);
}

void CEntityObject::OnColorChange(IVariable* var)
{
	var->Get(m_lightColor);
}

void CEntityObject::OnBoxProjectionChange(IVariable* var)
{
	int value;
	var->Get(value);
	m_bBoxProjectedCM = value;
}

void CEntityObject::OnBoxWidthChange(IVariable* var)
{
	var->Get(m_fBoxWidth);
}

void CEntityObject::OnBoxHeightChange(IVariable* var)
{
	var->Get(m_fBoxHeight);
}

void CEntityObject::OnBoxLengthChange(IVariable* var)
{
	var->Get(m_fBoxLength);
}

CVarBlock* CEntityObject::CloneProperties(CVarBlock* srcProperties)
{
	assert(srcProperties);
	return srcProperties->Clone(true);
}

void CEntityObject::SetMaterial(IEditorMaterial* mtl)
{
	CBaseObject::SetMaterial(mtl);
	UpdateMaterialInfo();
}

IEditorMaterial* CEntityObject::GetRenderMaterial() const
{
	if (GetMaterial())
	{
		return GetMaterial();
	}
	if (m_pEntity)
	{
		IEntityRender* pIEntityRender = m_pEntity->GetRenderInterface();

		{
			return GetIEditorImpl()->GetMaterialManager()->FromIMaterial(pIEntityRender->GetRenderMaterial());
		}
	}
	return NULL;
}

void CEntityObject::UpdateMaterialInfo()
{
	if (m_pEntity)
	{
		if (GetMaterial())
		{
			m_pEntity->SetMaterial(GetMaterial()->GetMatInfo());
		}
		else
		{
			m_pEntity->SetMaterial(0);
		}
	}
}

void CEntityObject::SetMaterialLayersMask(uint32 nLayersMask)
{
	__super::SetMaterialLayersMask(nLayersMask);
	UpdateMaterialInfo();
}

void CEntityObject::SetMinSpec(uint32 nMinSpec, bool bSetChildren)
{
	__super::SetMinSpec(nMinSpec, bSetChildren);
	OnRenderFlagsChange(0);
}

void CEntityObject::AcceptPhysicsState()
{
	if (m_pEntity)
	{
		StoreUndo("Accept Physics State");

		// [Anton] - StoreUndo sends EVENT_AFTER_LOAD, which forces position and angles to editor's position and
		// angles, which are not updated with the physics value
		SetWorldTM(m_pEntity->GetWorldTM());

		IPhysicalEntity* physic = m_pEntity->GetPhysics();
		if (!physic && m_pEntity->GetCharacter(0))     // for ropes
		{
			physic = m_pEntity->GetCharacter(0)->GetISkeletonPose()->GetCharacterPhysics(0);
		}
		if (physic && (physic->GetType() == PE_ARTICULATED || physic->GetType() == PE_ROPE))
		{
			IXmlSerializer* pSerializer = GetISystem()->GetXmlUtils()->CreateXmlSerializer();
			if (pSerializer)
			{
				m_physicsState = GetISystem()->CreateXmlNode("PhysicsState");
				ISerialize* ser = pSerializer->GetWriter(m_physicsState);
				physic->GetStateSnapshot(ser);
				pSerializer->Release();
			}
		}
	}
}

void CEntityObject::ResetPhysicsState()
{
	if (m_pEntity)
	{
		m_physicsState = 0;
		m_pEntity->SetPhysicsState(m_physicsState);
		Reload();
	}
}

float CEntityObject::GetHelperScale()
{
	return m_helperScale;
}

void CEntityObject::SetHelperScale(float scale)
{
	bool bChanged = m_helperScale != scale;
	m_helperScale = scale;
	if (bChanged)
	{
		CalcBBox();
	}
}

//! Analyze errors for this object.
void CEntityObject::Validate()
{
	CBaseObject::Validate();

	if (!m_pEntity && !m_entityClass.IsEmpty())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Entity %s Failed to Spawn (Script: %s) %s", (const char*)GetName(), (const char*)m_entityClass,
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
		return;
	}

	if (!m_pEntity)
	{
		return;
	}

	int slot;

	// Check Entity.
	int numObj = m_pEntity->GetSlotCount();
	for (slot = 0; slot < numObj; slot++)
	{
		IStatObj* pObj = m_pEntity->GetStatObj(slot);
		if (!pObj)
		{
			continue;
		}

		if (pObj->IsDefaultObject())
		{
			// File Not found.
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Geometry File in Slot %d for Entity %s not found %s", slot, (const char*)GetName(),
			           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
		}
	}
}

string CEntityObject::GetAssetPath() const
{
	if (m_pEntityScript != nullptr && m_pEntityScript->IsValid())
	{
		return m_pEntityScript->GetFile();
	}
	else if (m_pEntity)
	{
		if (Schematyc::IObject* pObject = m_pEntity->GetSchematycObject())
		{
			if (const char* szFileName = pObject->GetScriptFile())
			{
				return szFileName;
			}
		}
	}

	return string();
}

void CEntityObject::GatherUsedResources(CUsedResources& resources)
{
	CBaseObject::GatherUsedResources(resources);
	if (m_pLuaProperties)
	{
		m_pLuaProperties->GatherUsedResources(resources);
	}
	if (m_pLuaProperties2)
	{
		m_pLuaProperties2->GatherUsedResources(resources);
	}
	if (m_prototype != NULL && m_prototype->GetProperties())
	{
		m_prototype->GetProperties()->GatherUsedResources(resources);
	}

	if (m_pEntity)
	{
		IEntityRender* pIEntityRender = m_pEntity->GetRenderInterface();

		{
			IMaterial* pMtl = pIEntityRender->GetRenderMaterial();
			CMaterialManager::GatherResources(pMtl, resources);
		}
	}
}

bool CEntityObject::IsSimilarObject(CBaseObject* pObject)
{
	if (pObject->GetClassDesc() == GetClassDesc() && pObject->GetRuntimeClass() == GetRuntimeClass())
	{
		CEntityObject* pEntity = (CEntityObject*)pObject;
		if (m_entityClass == pEntity->m_entityClass &&
		    m_proximityRadius == pEntity->m_proximityRadius &&
		    m_innerRadius == pEntity->m_innerRadius &&
		    m_outerRadius == pEntity->m_outerRadius)
		{
			return true;
		}
	}
	return false;
}

bool CEntityObject::CreateFlowGraphWithGroupDialog()
{
	if (m_pFlowGraph)
	{
		return false;
	}

	CUndo undo("Create Flow graph");

	CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager();
	std::set<CString> groupsSet;
	pFlowGraphManager->GetAvailableGroups(groupsSet);

	string groupName;
	bool bDoNewGroup = true;
	bool bCreateGroup = false;
	bool bDoNewGraph = false;

	if (groupsSet.size() > 0)
	{
		std::vector<CString> groups;
		groups.push_back(CString(_T("<None>")));
		groups.insert(groups.end(), groupsSet.begin(), groupsSet.end());

		CGenericSelectItemDialog gtDlg(NULL);
		gtDlg.SetTitle(_T("Choose Group for the Flow Graph"));
		gtDlg.SetItems(groups);
		gtDlg.AllowNew(true);
		switch (gtDlg.DoModal())
		{
		case IDOK:
			groupName = gtDlg.GetSelectedItem();
			bCreateGroup = true;
			bDoNewGroup = false;
			bDoNewGraph = true;
			break;
		case IDNEW:
			bDoNewGroup = true;
			break;
		}
	}

	if (bDoNewGroup)
	{
		QStringDialog dlg(QObject::tr("Enter Group Name for the Flow Graph"));
		dlg.SetString(_T("<None>"));
		if (dlg.exec() == QDialog::Accepted)
		{
			bCreateGroup = true;
			groupName = dlg.GetString();
		}
	}

	if (bCreateGroup)
	{
		if (groupName == _T("<None>"))
		{
			groupName.Empty();
		}
		bDoNewGraph = false;
		OpenFlowGraph(groupName);
	}
	return bDoNewGraph;
}

void CEntityObject::SetFlowGraph(CHyperFlowGraph* pGraph)
{
	if (pGraph != m_pFlowGraph)
	{
		if (m_pFlowGraph)
		{
			m_pFlowGraph->Release();
		}
		m_pFlowGraph = pGraph;
		if (m_pFlowGraph)
		{
			m_pFlowGraph->SetEntity(this, true);   // true -> re-adjust graph entity nodes
			m_pFlowGraph->AddRef();

			if (m_pEntity)
			{
				IEntityFlowGraphComponent* pFlowGraphProxy = crycomponent_cast<IEntityFlowGraphComponent*>(m_pEntity->CreateProxy(ENTITY_PROXY_FLOWGRAPH));
				pFlowGraphProxy->SetFlowGraph(m_pFlowGraph->GetIFlowGraph());
			}
		}
		SetModified(false, false);
	}
}

void CEntityObject::OpenFlowGraph(const string& groupName)
{
	using namespace Private_EntityObject;
	if (!m_pFlowGraph)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoCreateFlowGraph(this, groupName));
		SetFlowGraph(GetIEditorImpl()->GetFlowGraphManager()->CreateGraphForEntity(this, groupName));
		UpdatePrefab();
	}
	GetIEditorImpl()->GetFlowGraphManager()->OpenView(m_pFlowGraph);
}

void CEntityObject::RemoveFlowGraph()
{
	if (m_pFlowGraph)
	{
		StoreUndo("Remove Flow Graph");

		GetIEditorImpl()->GetFlowGraphManager()->UnregisterGraph(m_pFlowGraph);
		SAFE_RELEASE(m_pFlowGraph);

		if (m_pEntity)
		{
			IEntityFlowGraphComponent* pFlowGraphProxy = (IEntityFlowGraphComponent*)m_pEntity->GetProxy(ENTITY_PROXY_FLOWGRAPH);
			if (pFlowGraphProxy)
			{
				pFlowGraphProxy->SetFlowGraph(nullptr);
			}
		}

		SetModified(false, false);
	}
}

string CEntityObject::GetSmartObjectClass() const
{
	string soClass;
	if (m_pLuaProperties)
	{
		IVariable* pVar = m_pLuaProperties->FindVariable("soclasses_SmartObjectClass");
		if (pVar)
		{
			pVar->Get(soClass);
		}
	}
	return soClass;
}

IRenderNode* CEntityObject::GetEngineNode() const
{
	if (m_pEntity)
	{
		IEntityRender* pIEntityRender = m_pEntity->GetRenderInterface();

		{
			return pIEntityRender->GetRenderNode();
		}
	}

	return NULL;
}

void CEntityObject::OnMenuCreateFlowGraph()
{
	CreateFlowGraphWithGroupDialog();
}

void CEntityObject::OnMenuScriptEvent(int eventIndex)
{
	CEntityScript* pScript = GetScript();
	if (pScript && GetIEntity())
	{
		pScript->SendEvent(GetIEntity(), pScript->GetEvent(eventIndex));
	}
}

void CEntityObject::OnMenuReloadScripts()
{
	CEntityScript* pScript = GetScript();
	if (pScript)
	{
		pScript->Reload();
	}
	Reload(true);

	// apply possible entity node change to all FG graphs that may be using it.
	// TODO: possible optimization: check if the node actually changed
	GetIEditorImpl()->GetFlowGraphManager()->ReloadGraphs();

	SEntityEvent event;
	event.event = ENTITY_EVENT_RELOAD_SCRIPT;
	gEnv->pEntitySystem->SendEventToAll(event);
}

void CEntityObject::OnMenuReloadAllScripts()
{
	CEntityScript* pScript = GetScript();
	if (pScript)
		pScript->Reload();

	CBaseObjectsArray allObjects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(allObjects);
	CBaseObjectsArray::iterator itObject, end;
	for (itObject = allObjects.begin(), end = allObjects.end(); itObject != end; ++itObject)
	{
		CBaseObject* pObject = *itObject;
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pThisEntity = (CEntityObject*)pObject;
			if (pThisEntity && pThisEntity->IsSameClass(this))
			{
				pThisEntity->Reload(true);
			}
		}
	}

	SEntityEvent event;
	event.event = ENTITY_EVENT_RELOAD_SCRIPT;
	gEnv->pEntitySystem->SendEventToAll(event);
}

void CEntityObject::OnMenuConvertToPrefab()
{
	const string& libraryFileName = this->GetEntityPropertyString("filePrefabLibrary");

	IDataBaseLibrary* pLibrary = GetIEditorImpl()->GetPrefabManager()->FindLibrary(libraryFileName);
	if (pLibrary == NULL)
		IDataBaseLibrary* pLibrary = GetIEditorImpl()->GetPrefabManager()->LoadLibrary(libraryFileName);

	if (pLibrary == NULL)
	{
		string sError = "Could not convert procedural object " + this->GetName() + "to prefab library " + libraryFileName;
		CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR, sError);
		return;
	}

	GetIEditorImpl()->GetObjectManager()->ClearSelection();

	GetIEditorImpl()->GetIUndoManager()->Suspend();
	GetIEditorImpl()->SetModifiedFlag();

	string strFullName = this->GetEntityPropertyString("sPrefabVariation");

	// check if we have the info we need from the script
	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(this->GetEntityId());
	if (pEnt)
	{
		IScriptTable* pScriptTable(pEnt->GetScriptTable());
		if (pScriptTable)
		{
			ScriptAnyValue value;
			if (pScriptTable->GetValueAny("PrefabSourceName", value))
			{
				char* szPrefabName = NULL;
				if (value.CopyTo(szPrefabName))
				{
					strFullName = string(szPrefabName);
				}
			}
		}
	}

	// strip the library name if it was added (for example it happens when automatically converting from prefab to procedural object)
	const string& sLibraryName = pLibrary->GetName();
	int nIdx = 0;
	int nLen = sLibraryName.GetLength();
	int nLen2 = strFullName.GetLength();
	if (nLen2 > nLen && strncmp(strFullName.GetString(), sLibraryName.GetString(), nLen) == 0)
		nIdx = nLen + 1;  // counts the . separating the library names

	// check if the prefab item exists inside the library
	const char* szItemName = strFullName.GetString();
	IDataBaseItem* pItem = pLibrary->FindItem(&szItemName[nIdx]);

	if (pItem)
	{
		string guid = pItem->GetGUID().ToString();
		CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->NewObject("Prefab", 0, guid);
		if (!pObject)
		{
			string sError = "Could not convert procedural object to " + strFullName;
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR, sError);
		}
		else
		{
			pObject->SetLayer(GetLayer());

			GetIEditorImpl()->SelectObject(pObject);

			pObject->SetWorldTM(this->GetWorldTM());
			GetIEditorImpl()->GetObjectManager()->DeleteObject(this);
		}
	}
	else
	{
		string sError = "Library not found " + sLibraryName;
		CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR, sError);
	}

	GetIEditorImpl()->GetIUndoManager()->Resume();
}

void CEntityObject::OnContextMenu(CPopupMenuItem* pMenu)
{
	CEntityObject* p = new CEntityObject();
	if (!pMenu->Empty())
	{
		pMenu->AddSeparator();
	}

	std::vector<CHyperFlowGraph*> flowgraphs;
	CHyperFlowGraph* pEntityFG = 0;
	FlowGraphHelpers::FindGraphsForEntity(this, flowgraphs, pEntityFG);

	if (GetFlowGraph() == 0)
	{
		pMenu->Add("Create Flow Graph", functor(*this, &CEntityObject::OnMenuCreateFlowGraph));
		pMenu->AddSeparator();
	}

	if (!flowgraphs.empty())
	{
		CPopupMenuItem& fgMenu = pMenu->Add("Flow Graphs");

		std::vector<CHyperFlowGraph*>::const_iterator iter, end;
		for (iter = flowgraphs.begin(), end = flowgraphs.end(); iter != end; ++iter)
		{
			if (!*iter)
			{
				continue;
			}
			string name;
			FlowGraphHelpers::GetHumanName(*iter, name);
			if (*iter == pEntityFG)
			{
				name += " <GraphEntity>";

				fgMenu.AddCommand(name, CommandString("flowgraph.open_view_and_select", (*iter)->GetName(), (const char*)GetName()));
				if (flowgraphs.size() > 1)
				{
					fgMenu.AddSeparator();
				}
			}
			else
			{
				fgMenu.AddCommand(name, CommandString("flowgraph.open_view_and_select", (*iter)->GetName(), (const char*)GetName()));
			}
		}
		pMenu->AddSeparator();
	}

	// Events
	CEntityScript* pScript = GetScript();
	if (pScript && pScript->GetEventCount() > 0)
	{
		CPopupMenuItem& eventMenu = pMenu->Add("Events");

		int eventCount = pScript->GetEventCount();
		for (int i = 0; i < eventCount; ++i)
		{
			string sourceEvent = pScript->GetEvent(i);
			eventMenu.Add(sourceEvent, functor(*this, &CEntityObject::OnMenuScriptEvent), i);
		}
		pMenu->AddSeparator();
	}

	if (IsLegacyObject())
	{
		pMenu->Add("Reload Script", functor(*this, &CEntityObject::OnMenuReloadScripts));
		pMenu->Add("Reload All Scripts", functor(*this, &CEntityObject::OnMenuReloadAllScripts));
	}

	if (pScript && pScript->GetClass() && stricmp(pScript->GetClass()->GetName(), "ProceduralObject") == 0)
	{
		pMenu->Add("Convert to prefab", functor(*this, &CEntityObject::OnMenuConvertToPrefab));
		//pMenu->AddSeparator();
	}
	__super::OnContextMenu(pMenu);
}

void CEntityObject::SetFrozen(bool bFrozen)
{
	__super::SetFrozen(bFrozen);
	if (m_pFlowGraph)
	{
		GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_UPDATE_FROZEN);
	}
}

bool CEntityObject::IsScalable() const
{
	if (!m_bForceScale)
	{
		return !(m_pEntityScript && (m_pEntityScript->GetFlags() & ENTITY_SCRIPT_ISNOTSCALABLE));
	}
	else
	{
		return true;
	}
}

bool CEntityObject::IsRotatable() const
{
	return !(m_pEntityScript && (m_pEntityScript->GetFlags() & ENTITY_SCRIPT_ISNOTROTATABLE));
}

void CEntityObject::InvalidateGeometryFile(const string&)
{
	if (m_pEntity)
	{
		m_pEntity->InvalidateLocalBounds();
	}
	CalcBBox();
}

IOpticsElementBasePtr CEntityObject::GetOpticsElement()
{
	SRenderLight* pLight = GetLightProperty();
	if (pLight == NULL)
		return NULL;
	return pLight->GetLensOpticsElement();
}

void CEntityObject::SetOpticsElement(IOpticsElementBase* pOptics)
{
	SRenderLight* pLight = GetLightProperty();
	if (pLight == NULL)
		return;
	pLight->SetLensOpticsElement(pOptics);
	if (GetEntityPropertyBool("bFlareEnable") && pOptics)
		CBaseObject::SetMaterial(s_LensFlareMaterialName);
	else
		SetMaterial(NULL);
}

void CEntityObject::ApplyOptics(const string& opticsFullName, IOpticsElementBasePtr pOptics)
{
	if (pOptics == NULL)
	{
		SRenderLight* pLight = GetLightProperty();
		if (pLight)
			pLight->SetLensOpticsElement(NULL);
		SetFlareName("");
		SetMaterial(NULL);
	}
	else
	{
		int nOpticsIndex(0);
		if (!gEnv->pOpticsManager->Load(opticsFullName, nOpticsIndex))
		{
			IOpticsElementBasePtr pNewOptics = gEnv->pOpticsManager->Create(eFT_Root);
			if (!gEnv->pOpticsManager->AddOptics(pNewOptics, opticsFullName, nOpticsIndex))
			{
				SRenderLight* pLight = GetLightProperty();
				if (pLight)
				{
					pLight->SetLensOpticsElement(NULL);
					SetMaterial(NULL);
				}
				return;
			}
			LensFlareUtil::CopyOptics(pOptics, pNewOptics);
		}
		SetFlareName(opticsFullName);
	}

	UpdateUIVars();
}

void CEntityObject::SetOpticsName(const string& opticsFullName)
{
	bool bUpdateOpticsProperty = true;

	if (opticsFullName.IsEmpty())
	{
		SRenderLight* pLight = GetLightProperty();
		if (pLight)
			pLight->SetLensOpticsElement(NULL);
		SetFlareName("");
		SetMaterial(NULL);
	}
	else
	{
		if (GetOpticsElement())
		{
			const string oldName(GetOpticsElement()->GetName());
			if (gEnv->pOpticsManager->Rename(oldName, opticsFullName))
				SetFlareName(opticsFullName);
		}
	}

	UpdateUIVars();
}

SRenderLight* CEntityObject::GetLightProperty() const
{
	const PodArray<ILightSource*>* pLightEntities = GetIEditorImpl()->Get3DEngine()->GetLightEntities();
	if (pLightEntities == NULL)
		return NULL;
	for (int i = 0, iLightSize(pLightEntities->Count()); i < iLightSize; ++i)
	{
		ILightSource* pLightSource = pLightEntities->GetAt(i);
		if (pLightSource == NULL)
			continue;
		SRenderLight& lightProperty = pLightSource->GetLightProperties();
		if (GetName() != lightProperty.m_sName)
			continue;
		return &lightProperty;
	}
	return NULL;
}

bool CEntityObject::GetValidFlareName(string& outFlareName) const
{
	if (!m_pLuaProperties)
		return false;

	IVariable* pFlareVar(m_pLuaProperties->FindVariable(s_LensFlarePropertyName));
	if (!pFlareVar)
		return false;

	string flareName;
	pFlareVar->Get(flareName);
	if (flareName.IsEmpty() || flareName == "@root")
		return false;

	outFlareName = flareName;

	return true;
}

void CEntityObject::PreInitLightProperty()
{
	if (!IsLight())
		return;

	string flareFullName;
	if (GetValidFlareName(flareFullName))
	{
		bool bEnableOptics = GetEntityPropertyBool("bFlareEnable");
		if (bEnableOptics)
		{
			CLensFlareManager* pLensManager = GetIEditorImpl()->GetLensFlareManager();
			CLensFlareLibrary* pLevelLib = (CLensFlareLibrary*)pLensManager->GetLevelLibrary();
			IOpticsElementBasePtr pLevelOptics = pLevelLib->GetOpticsOfItem(flareFullName);
			if (pLevelLib && pLevelOptics)
			{
				int nOpticsIndex(0);
				IOpticsElementBasePtr pNewOptics = GetOpticsElement();
				if (pNewOptics == NULL)
					pNewOptics = gEnv->pOpticsManager->Create(eFT_Root);

				if (gEnv->pOpticsManager->AddOptics(pNewOptics, flareFullName, nOpticsIndex))
				{
					LensFlareUtil::CopyOptics(pLevelOptics, pNewOptics);
					SetOpticsElement(pNewOptics);
				}
				else
				{
					SRenderLight* pLight = GetLightProperty();
					if (pLight)
					{
						pLight->SetLensOpticsElement(NULL);
						SetMaterial(NULL);
					}
				}
			}
		}
	}
}

void CEntityObject::UpdateLightProperty()
{
	if (!IsLight())
		return;

	string flareName;
	if (GetValidFlareName(flareName))
	{
		IOpticsElementBasePtr pOptics = GetOpticsElement();
		if (pOptics == NULL)
			pOptics = gEnv->pOpticsManager->Create(eFT_Root);
		bool bEnableOptics = GetEntityPropertyBool("bFlareEnable");
		if (bEnableOptics && GetIEditorImpl()->GetLensFlareManager()->LoadFlareItemByName(flareName, pOptics))
		{
			pOptics->SetName(flareName);
			SetOpticsElement(pOptics);
		}
		else
		{
			SetOpticsElement(NULL);
		}
	}

	if (m_pEntity)
	{
		IScriptTable* pScriptTable = m_pEntity->GetScriptTable();
		if (pScriptTable && GetEntityPropertyBool("bActive") && GetLayer()->IsVisible(true))
		{
			HSCRIPTFUNCTION activateLightFunction;
			if (pScriptTable->GetValue("ActivateLight", activateLightFunction))
				Script::CallMethod(pScriptTable, activateLightFunction, true);
		}
	}
}

void CEntityObject::CopyPropertiesToScriptTables()
{
	if (m_pEntityScript)
	{
		if (m_pLuaProperties && !m_prototype)
		{
			m_pEntityScript->CopyPropertiesToScriptTable(m_pEntity, m_pLuaProperties, false);
		}
		if (m_pLuaProperties2)
		{
			m_pEntityScript->CopyProperties2ToScriptTable(m_pEntity, m_pLuaProperties2, false);
		}
	}
}

void CEntityObject::ResetCallbacks()
{
	//TODO: This seems very dangerous, setting arbitrary properties and removing callbacks without knowledge of the observers...
	//Only observers should detach or attach, the only other time when a callback should be detached is on destruction of the object
	//My assumption is that this was made to set the track view properties back to original values, and this should be removed

	//CE-12395: Don't clear m_pLuaProperties and m_pLuaProperties2 &CEntityObject::OnPropertyChange callback. This is needed
	//because the order of those callbacks registration is important!!!!
	{
		for (auto iter = m_callbacks.begin(); iter != m_callbacks.end(); ++iter)
		{
			iter->first->RemoveOnSetCallback(iter->second);
		}
		m_callbacks.clear();
	}

	if (m_pLuaProperties)
	{
		m_callbacks.reserve(6);

		//@FIXME Hack to display radii of properties.
		// wires properties from param block, to this entity internal variables.
		IVariable* var = 0;
		var = m_pLuaProperties->FindVariable("Radius", false);
		if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
		{
			var->Get(m_proximityRadius);
			SetVariableCallback(var, functor(*this, &CEntityObject::OnRadiusChange));
		}
		else
		{
			var = m_pLuaProperties->FindVariable("radius", false);
			if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
			{
				var->Get(m_proximityRadius);
				SetVariableCallback(var, functor(*this, &CEntityObject::OnRadiusChange));
			}
		}

		var = m_pLuaProperties->FindVariable("InnerRadius", false);
		if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
		{
			var->Get(m_innerRadius);
			SetVariableCallback(var, functor(*this, &CEntityObject::OnInnerRadiusChange));
		}
		var = m_pLuaProperties->FindVariable("OuterRadius", false);
		if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
		{
			var->Get(m_outerRadius);
			SetVariableCallback(var, functor(*this, &CEntityObject::OnOuterRadiusChange));
		}

		var = m_pLuaProperties->FindVariable("BoxSizeX", false);
		if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
		{
			var->Get(m_boxSizeX);
			SetVariableCallback(var, functor(*this, &CEntityObject::OnBoxSizeXChange));
		}
		var = m_pLuaProperties->FindVariable("BoxSizeY", false);
		if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
		{
			var->Get(m_boxSizeY);
			SetVariableCallback(var, functor(*this, &CEntityObject::OnBoxSizeYChange));
		}
		var = m_pLuaProperties->FindVariable("BoxSizeZ", false);
		if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
		{
			var->Get(m_boxSizeZ);
			SetVariableCallback(var, functor(*this, &CEntityObject::OnBoxSizeZChange));
		}

		var = m_pLuaProperties->FindVariable("fAttenuationBulbSize");
		if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
		{
			var->Get(m_fAreaLightSize);
			SetVariableCallback(var, functor(*this, &CEntityObject::OnAreaLightSizeChange));
		}

		IVariable* pProjector = m_pLuaProperties->FindVariable("Projector");
		if (pProjector)
		{
			var = pProjector->FindVariable("fProjectorFov");
			if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
			{
				var->Get(m_projectorFOV);
				SetVariableCallback(var, functor(*this, &CEntityObject::OnProjectorFOVChange));
			}
			var = pProjector->FindVariable("bProjectInAllDirs");
			if (var && var->GetType() == IVariable::BOOL)
			{
				int value;
				var->Get(value);
				m_bProjectInAllDirs = value;
				SetVariableCallback(var, functor(*this, &CEntityObject::OnProjectInAllDirsChange));
			}
			var = pProjector->FindVariable("texture_Texture");
			if (var && var->GetType() == IVariable::STRING)
			{
				string projectorTexture;
				var->Get(projectorTexture);
				m_bProjectorHasTexture = !projectorTexture.IsEmpty();
				SetVariableCallback(var, functor(*this, &CEntityObject::OnProjectorTextureChange));
			}
		}

		IVariable* pColorGroup = m_pLuaProperties->FindVariable("Color", false);
		if (pColorGroup)
		{
			const int nChildCount = pColorGroup->GetNumVariables();
			for (int i = 0; i < nChildCount; ++i)
			{
				IVariable* pChild = pColorGroup->GetVariable(i);
				if (!pChild)
					continue;

				string name(pChild->GetName());
				if (name == "clrDiffuse")
				{
					pChild->Get(m_lightColor);
					SetVariableCallback(pChild, functor(*this, &CEntityObject::OnColorChange));
					break;
				}
			}
		}

		IVariable* pType = m_pLuaProperties->FindVariable("Shape");
		if (pType)
		{
			var = pType->FindVariable("bAreaLight");
			if (var && var->GetType() == IVariable::BOOL)
			{
				int value;
				var->Get(value);
				m_bAreaLight = value;
				SetVariableCallback(var, functor(*this, &CEntityObject::OnAreaLightChange));
			}
			var = pType->FindVariable("fPlaneWidth");
			if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
			{
				var->Get(m_fAreaWidth);
				SetVariableCallback(var, functor(*this, &CEntityObject::OnAreaWidthChange));
			}
			var = pType->FindVariable("fPlaneHeight");
			if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
			{
				var->Get(m_fAreaHeight);
				SetVariableCallback(var, functor(*this, &CEntityObject::OnAreaHeightChange));
			}
		}

		IVariable* pProjection = m_pLuaProperties->FindVariable("Projection");
		if (pProjection)
		{
			var = pProjection->FindVariable("bBoxProject");
			if (var && var->GetType() == IVariable::BOOL)
			{
				int value;
				var->Get(value);
				m_bBoxProjectedCM = value;
				SetVariableCallback(var, functor(*this, &CEntityObject::OnBoxProjectionChange));
			}
			var = pProjection->FindVariable("fBoxWidth");
			if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
			{
				var->Get(m_fBoxWidth);
				SetVariableCallback(var, functor(*this, &CEntityObject::OnBoxWidthChange));
			}
			var = pProjection->FindVariable("fBoxHeight");
			if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
			{
				var->Get(m_fBoxHeight);
				SetVariableCallback(var, functor(*this, &CEntityObject::OnBoxHeightChange));
			}
			var = pProjection->FindVariable("fBoxLength");
			if (var && (var->GetType() == IVariable::FLOAT || var->GetType() == IVariable::INT))
			{
				var->Get(m_fBoxLength);
				SetVariableCallback(var, functor(*this, &CEntityObject::OnBoxLengthChange));
			}
		}

		// Each property must have callback to our OnPropertyChange.
		//CE-12395: in the case &CEntityObject::OnPropertyChange is already present in  m_pLuaProperties
		//it won't be added again, because there is a check inside
		m_pLuaProperties->AddOnSetCallback(functor(*this, &CEntityObject::OnPropertyChange));
	}

	if (m_pLuaProperties2)
	{
		//CE-12395: in the case &CEntityObject::OnPropertyChange is already present in  m_pLuaProperties2
		//it won't be added again, because there is a check inside
		m_pLuaProperties2->AddOnSetCallback(functor(*this, &CEntityObject::OnPropertyChange));
	}

}

void CEntityObject::SetVariableCallback(IVariable* pVar, IVariable::OnSetCallback func)
{
	pVar->AddOnSetCallback(func);
	m_callbacks.push_back(std::make_pair(pVar, func));
}

void CEntityObject::ClearCallbacks()
{
	if (m_pLuaProperties)
	{
		m_pLuaProperties->RemoveOnSetCallback(functor(*this, &CEntityObject::OnPropertyChange));
	}

	if (m_pLuaProperties2)
	{
		m_pLuaProperties2->RemoveOnSetCallback(functor(*this, &CEntityObject::OnPropertyChange));
	}

	for (auto iter = m_callbacks.begin(); iter != m_callbacks.end(); ++iter)
	{
		iter->first->RemoveOnSetCallback(iter->second);
	}

	m_callbacks.clear();
}

void CEntityObject::StoreUndoEntityLink(const std::vector<CBaseObject*>& objects)
{
	using namespace Private_EntityObject;
	if (objects.size() > 0 && CUndo::IsRecording())
		CUndo::Record(new CUndoEntityLink(objects));
}

IStatObj* CEntityObject::GetIStatObj()
{
	if (m_pEntity == NULL)
		return NULL;

	for (int i = 0, iSlotCount(m_pEntity->GetSlotCount()); i < iSlotCount; ++i)
	{
		if (!m_pEntity->IsSlotValid(i))
			continue;
		IStatObj* pStatObj = m_pEntity->GetStatObj(i);
		if (pStatObj == NULL)
			continue;
		return pStatObj;
	}

	return NULL;
}

void CEntityObject::UpdateHighlightPassState(bool bSelected, bool bHighlighted)
{
	if (m_pEntity)
	{
		m_pEntity->SetEditorObjectInfo(bSelected, bHighlighted);
	}
}

void CEntityObject::RegisterListener(IEntityObjectListener* pListener)
{
	m_listeners.Add(pListener);
}

void CEntityObject::UnregisterListener(IEntityObjectListener* pListener)
{
	m_listeners.Remove(pListener);
}

template<typename T>
T CEntityObject::GetEntityProperty(const char* pName, T defaultvalue) const
{
	IVariable* pVariable = NULL;
	if (m_pLuaProperties2)
	{
		pVariable = m_pLuaProperties2->FindVariable(pName);
	}

	if (!pVariable)
	{
		if (m_pLuaProperties)
		{
			pVariable = m_pLuaProperties->FindVariable(pName);
		}

		if (!pVariable)
		{
			return defaultvalue;
		}
	}

	if (pVariable->GetType() != IVariableType<T>::value)
	{
		return defaultvalue;
	}

	T value;
	pVariable->Get(value);
	return value;
}

template<typename T>
void CEntityObject::SetEntityProperty(const char* pName, T value)
{
	CVarBlock* pProperties = GetProperties2();
	IVariable* pVariable = NULL;
	if (pProperties)
	{
		pVariable = pProperties->FindVariable(pName);
	}

	if (!pVariable)
	{
		pProperties = GetProperties();
		if (pProperties)
		{
			pVariable = pProperties->FindVariable(pName);
		}

		if (!pVariable)
		{
			throw std::runtime_error(string("\"") + pName + "\" is an invalid property.");
		}
	}

	if (pVariable->GetType() != IVariableType<T>::value)
	{
		throw std::logic_error("Data type is invalid.");
	}
	pVariable->Set(value);
}

bool CEntityObject::GetEntityPropertyBool(const char* name) const
{
	return GetEntityProperty<bool>(name, false);
}

int CEntityObject::GetEntityPropertyInteger(const char* name) const
{
	return GetEntityProperty<int>(name, 0);
}

float CEntityObject::GetEntityPropertyFloat(const char* name) const
{
	return GetEntityProperty<float>(name, 0.0f);
}

string CEntityObject::GetMouseOverStatisticsText() const
{
	if (m_pEntity)
	{
		string statsText;
		string lodText;

		const unsigned int slotCount = m_pEntity->GetSlotCount();
		for (unsigned int slot = 0; slot < slotCount; ++slot)
		{
			IStatObj* pStatObj = m_pEntity->GetStatObj(slot);

			if (pStatObj)
			{
				IStatObj::SStatistics stats;
				pStatObj->GetStatistics(stats);

				for (int i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
				{
					if (stats.nIndicesPerLod[i] > 0)
					{
						lodText.Format("\n  Slot%d, LOD%d:   ", slot, i);
						statsText = statsText + lodText +
						            FormatWithThousandsSeperator(stats.nIndicesPerLod[i] / 3) + " Tris,   "
						            + FormatWithThousandsSeperator(stats.nVerticesPerLod[i]) + " Verts";
					}
				}
			}

			IGeomCacheRenderNode* pGeomCacheRenderNode = m_pEntity->GetGeomCacheRenderNode(slot);

			if (pGeomCacheRenderNode)
			{
				IGeomCache* pGeomCache = pGeomCacheRenderNode->GetGeomCache();
				if (pGeomCache)
				{
					IGeomCache::SStatistics stats = pGeomCache->GetStatistics();

					string averageDataRate;
					FormatFloatForUI(averageDataRate, 2, stats.m_averageAnimationDataRate);

					statsText += "\n  " + (stats.m_bPlaybackFromMemory ? string("Playback from Memory") : string("Playback from Disk"));
					statsText += "\n  " + averageDataRate + " MiB/s Average Animation Data Rate";
					statsText += "\n  " + FormatWithThousandsSeperator(stats.m_numStaticTriangles) + " Static Tris, "
					             + FormatWithThousandsSeperator(stats.m_numStaticVertices) + " Static Verts";
					statsText += "\n  " + FormatWithThousandsSeperator(stats.m_numAnimatedTriangles) + " Animated Tris, "
					             + FormatWithThousandsSeperator(stats.m_numAnimatedVertices) + " Animated Verts";
					statsText += "\n  " + FormatWithThousandsSeperator(stats.m_numMaterials) + " Materials";
					statsText += "\n  " + FormatWithThousandsSeperator(stats.m_numAnimatedMeshes) + " Animated Meshes";
					statsText += "\n  " + FormatWithThousandsSeperator(stats.m_numStaticMeshes) + " Static Meshes";
					statsText += "\n  " + FormatWithThousandsSeperator(stats.m_staticDataSize) + " Bytes Memory Static Data";
					statsText += "\n  " + FormatWithThousandsSeperator(stats.m_diskAnimationDataSize) + " Bytes Disk Animation Data";
					statsText += "\n  " + FormatWithThousandsSeperator(stats.m_memoryAnimationDataSize) + " Bytes Memory Animation Data";
				}
			}

			ICharacterInstance* pCharacter = m_pEntity->GetCharacter(slot);
			if (pCharacter)
			{
				IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
				if (pAttachmentManager)
				{
					const int32 count = pAttachmentManager->GetAttachmentCount();
					for (int32 i = 0; i < count; ++i)
					{
						if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(i))
						{
							IAttachmentSkin* pAttachmentSkin = pAttachment->GetIAttachmentSkin();
							if (pAttachmentSkin)
							{
								ISkin* pSkin = pAttachmentSkin->GetISkin();
								if (pSkin)
								{
									const uint32 numLods = pSkin->GetNumLODs();
									const char* szFilename = PathUtil::GetFile(pSkin->GetModelFilePath());

									statsText += "\n  " + string(szFilename);
									for (int j = 0; j < numLods; ++j)
									{
										IRenderMesh* pRenderMesh = pSkin->GetIRenderMesh(j);
										if (pRenderMesh)
										{
											const int32 vertCount = pRenderMesh->GetVerticesCount();
											statsText += ", LOD" + FormatWithThousandsSeperator(j) + ": " + FormatWithThousandsSeperator(vertCount) + " Vertices";
										}
									}

									const uint32 vertexFrameCount = pSkin->GetVertexFrames()->GetCount();
									statsText += ", " + FormatWithThousandsSeperator(vertexFrameCount) + " Vertex Frames";
								}
							}
						}
					}
				}
			}
		}

		return statsText;
	}

	return "";
}

void CEntityObject::OnEditScript()
{
	if (GetScript())
	{
		CFileUtil::EditTextFile(GetScript()->GetFile());
	}
}

void CEntityObject::OnOpenArchetype()
{
	if (m_prototype)
	{
		GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_ENTITY_ARCHETYPE, m_prototype);
	}
}

void CEntityObject::OnBnClickedOpenFlowGraph()
{
	if (!GetFlowGraph())
	{
		CreateFlowGraphWithGroupDialog();
	}
	else
	{
		// Flow graph already present.
		OpenFlowGraph("");
	}
}

void CEntityObject::OnBnClickedListFlowGraphs()
{
	FlowGraphHelpers::ListFlowGraphsForEntity(this);
}

void CEntityObject::OnBnClickedRemoveFlowGraph()
{
	if (GetFlowGraph())
	{
		CUndo undo("Remove Flow graph");
		string str;
		str.Format("Remove Flow Graph for Entity %s?", (const char*)GetName());
		if (MessageBox(0, str, "Confirm", MB_OKCANCEL) == IDOK)
		{
			RemoveFlowGraph();
		}
	}
}

string CEntityObject::GetEntityPropertyString(const char* name) const
{
	return GetEntityProperty<string>(name, "");
}

void CEntityObject::SetEntityPropertyBool(const char* name, bool value)
{
	SetEntityProperty<bool>(name, value);
}

void CEntityObject::SetEntityPropertyInteger(const char* name, int value)
{
	SetEntityProperty<int>(name, value);
}

void CEntityObject::SetEntityPropertyFloat(const char* name, float value)
{
	SetEntityProperty<float>(name, value);
}

void CEntityObject::SetEntityPropertyString(const char* name, const string& value)
{
	SetEntityProperty<string>(name, value);
}

SPyWrappedProperty CEntityObject::PyGetEntityProperty(const char* pName) const
{
	IVariable* pVariable = NULL;
	if (m_pLuaProperties2)
	{
		pVariable = m_pLuaProperties2->FindVariable(pName, true, true);
	}

	if (!pVariable)
	{
		if (m_pLuaProperties)
		{
			pVariable = m_pLuaProperties->FindVariable(pName, true, true);
		}

		if (!pVariable)
		{
			throw std::runtime_error(string("\"") + pName + "\" is an invalid property.");
		}
	}

	SPyWrappedProperty value;
	if (pVariable->GetType() == IVariable::BOOL)
	{
		value.type = SPyWrappedProperty::eType_Bool;
		pVariable->Get(value.property.boolValue);
		return value;
	}
	else if (pVariable->GetType() == IVariable::INT)
	{
		value.type = SPyWrappedProperty::eType_Int;
		pVariable->Get(value.property.intValue);
		return value;
	}
	else if (pVariable->GetType() == IVariable::FLOAT)
	{
		value.type = SPyWrappedProperty::eType_Float;
		pVariable->Get(value.property.floatValue);
		return value;
	}
	else if (pVariable->GetType() == IVariable::STRING)
	{
		value.type = SPyWrappedProperty::eType_String;
		pVariable->Get(value.stringValue);
		return value;
	}
	else if (pVariable->GetType() == IVariable::VECTOR)
	{
		Vec3 tempVec3;
		pVariable->Get(tempVec3);

		if (pVariable->GetDataType() == IVariable::DT_COLOR)
		{
			value.type = SPyWrappedProperty::eType_Color;
			COLORREF col = CMFCUtils::ColorLinearToGamma(ColorF(
			                                               tempVec3[0],
			                                               tempVec3[1],
			                                               tempVec3[2]));
			value.property.colorValue.r = (int)GetRValue(col);
			value.property.colorValue.g = (int)GetGValue(col);
			value.property.colorValue.b = (int)GetBValue(col);
		}
		else
		{
			value.type = SPyWrappedProperty::eType_Vec3;
			value.property.vecValue.x = tempVec3[0];
			value.property.vecValue.y = tempVec3[1];
			value.property.vecValue.z = tempVec3[2];
		}

		return value;
	}

	throw std::runtime_error("Data type is invalid.");
}

void CEntityObject::PySetEntityProperty(const char* pName, const SPyWrappedProperty& value)
{
	IVariable* pVariable = NULL;
	if (m_pLuaProperties2)
	{
		pVariable = m_pLuaProperties2->FindVariable(pName, true, true);
	}

	if (!pVariable)
	{
		if (m_pLuaProperties)
		{
			pVariable = m_pLuaProperties->FindVariable(pName, true, true);
		}

		if (!pVariable)
		{
			throw std::runtime_error(string("\"") + pName + "\" is an invalid property.");
		}
	}

	if (value.type == SPyWrappedProperty::eType_Bool)
	{
		pVariable->Set(value.property.boolValue);
	}
	else if (value.type == SPyWrappedProperty::eType_Int)
	{
		pVariable->Set(value.property.intValue);
	}
	else if (value.type == SPyWrappedProperty::eType_Float)
	{
		pVariable->Set(value.property.floatValue);
	}
	else if (value.type == SPyWrappedProperty::eType_String)
	{
		pVariable->Set(value.stringValue);
	}
	else if (value.type == SPyWrappedProperty::eType_Vec3)
	{
		pVariable->Set(Vec3(value.property.vecValue.x, value.property.vecValue.y, value.property.vecValue.z));
	}
	else if (value.type == SPyWrappedProperty::eType_Color)
	{
		pVariable->Set(Vec3(value.property.colorValue.r, value.property.colorValue.g, value.property.colorValue.b));
	}
	else
	{
		throw std::runtime_error("Data type is invalid.");
	}

	UpdateUIVars();
}

SPyWrappedProperty PyGetEntityProperty(const char* pObjName, const char* pPropName)
{
	CBaseObject* pObject;
	if (GetIEditorImpl()->GetObjectManager()->FindObject(pObjName))
		pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pObjName);
	else if (GetIEditorImpl()->GetObjectManager()->FindObject(CryGUID::FromString(pObjName)))
		pObject = GetIEditorImpl()->GetObjectManager()->FindObject(CryGUID::FromString(pObjName));
	else
	{
		throw std::logic_error(string("\"") + pObjName + "\" is an invalid object.");
	}

	if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntityObject = static_cast<CEntityObject*>(pObject);
		return pEntityObject->PyGetEntityProperty(pPropName);
	}
	else
		throw std::logic_error(string("\"") + pObjName + "\" is an invalid entity.");
}

void PySetEntityProperty(const char* entityName, const char* propName, SPyWrappedProperty value)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(entityName);
	if (!pObject || !pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		throw std::logic_error(string("\"") + entityName + "\" is an invalid entity.");
	}

	CUndo undo("Set Entity Property");
	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoEntityProperty(entityName, propName));
	}

	CEntityObject* pEntityObject = static_cast<CEntityObject*>(pObject);
	return pEntityObject->PySetEntityProperty(propName, value);
}

SPyWrappedProperty PyGetObjectVariableRec(IVariableContainer* pVariableContainer, std::deque<string>& path)
{
	SPyWrappedProperty value;
	string currentName = path.back();
	string currentPath = path.front();

	IVariableContainer* pSubVariableContainer = pVariableContainer;
	if (path.size() != 1)
	{
		pSubVariableContainer = pSubVariableContainer->FindVariable(currentPath, false, true);
		if (!pSubVariableContainer)
		{
			throw std::logic_error("Path is invalid.");
		}
		path.pop_front();
		PyGetObjectVariableRec(pSubVariableContainer, path);
	}

	IVariable* pVariable = pSubVariableContainer->FindVariable(currentName, false, true);

	if (pVariable)
	{
		if (pVariable->GetType() == IVariable::BOOL)
		{
			value.type = SPyWrappedProperty::eType_Bool;
			pVariable->Get(value.property.boolValue);
			return value;
		}
		else if (pVariable->GetType() == IVariable::INT)
		{
			value.type = SPyWrappedProperty::eType_Int;
			pVariable->Get(value.property.intValue);
			return value;
		}
		else if (pVariable->GetType() == IVariable::FLOAT)
		{
			value.type = SPyWrappedProperty::eType_Float;
			pVariable->Get(value.property.floatValue);
			return value;
		}
		else if (pVariable->GetType() == IVariable::STRING)
		{
			value.type = SPyWrappedProperty::eType_String;
			pVariable->Get(value.stringValue);
			return value;
		}
		else if (pVariable->GetType() == IVariable::VECTOR)
		{
			value.type = SPyWrappedProperty::eType_Vec3;
			Vec3 tempVec3;
			pVariable->Get(tempVec3);
			value.property.vecValue.x = tempVec3[0];
			value.property.vecValue.y = tempVec3[1];
			value.property.vecValue.z = tempVec3[2];
			return value;
		}

		throw std::logic_error("Data type is invalid.");
	}

	throw std::logic_error(string("\"") + currentName + "\" is an invalid parameter.");
}

SPyWrappedProperty PyGetEntityParam(const char* pObjName, const char* pVarPath)
{
	SPyWrappedProperty result;
	string varPath = pVarPath;
	std::deque<string> splittedPath;
	int currentPos = 0;
	for (string token = varPath.Tokenize("/\\", currentPos); !token.IsEmpty(); token = varPath.Tokenize("/\\", currentPos))
	{
		splittedPath.push_back(token);
	}

	CBaseObject* pObject;
	if (GetIEditorImpl()->GetObjectManager()->FindObject(pObjName))
		pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pObjName);
	else if (GetIEditorImpl()->GetObjectManager()->FindObject(CryGUID::FromString(pObjName)))
		pObject = GetIEditorImpl()->GetObjectManager()->FindObject(CryGUID::FromString(pObjName));
	else
	{
		throw std::logic_error(string("\"") + pObjName + "\" is an invalid object.");
		return result;
	}

	if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		if (CVarObject* pVarObject = pObject->GetVarObject())
		{
			return PyGetObjectVariableRec(pVarObject, splittedPath);
		}
	}
	else
		throw std::logic_error(string("\"") + pObjName + "\" is an invalid entity.");

	return result;
}

void PySetEntityParam(const char* pObjectName, const char* pVarPath, SPyWrappedProperty value)
{
	string varPath = pVarPath;
	std::deque<string> splittedPath;
	int currentPos = 0;
	for (string token = varPath.Tokenize("/\\", currentPos); !token.IsEmpty(); token = varPath.Tokenize("/\\", currentPos))
	{
		splittedPath.push_back(token);
	}

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pObjectName);
	if (pObject)
	{
		if (CVarObject* pVarObject = pObject->GetVarObject())
		{
			IVariable* pVariable = pVarObject->FindVariable(splittedPath.back(), false, true);

			CUndo undo("Set Entity Param");
			if (CUndo::IsRecording())
			{
				CUndo::Record(new CUndoEntityParam(pObjectName, pVarPath));
			}

			if (pVariable)
			{
				if (value.type == SPyWrappedProperty::eType_Bool)
				{
					pVariable->Set(value.property.boolValue);
				}
				else if (value.type == SPyWrappedProperty::eType_Int)
				{
					pVariable->Set(value.property.intValue);
				}
				else if (value.type == SPyWrappedProperty::eType_Float)
				{
					pVariable->Set(value.property.floatValue);
				}
				else if (value.type == SPyWrappedProperty::eType_String)
				{
					pVariable->Set(value.stringValue);
				}
				else if (value.type == SPyWrappedProperty::eType_Vec3)
				{
					Vec3 tempVec3;
					tempVec3[0] = value.property.vecValue.x;
					tempVec3[1] = value.property.vecValue.y;
					tempVec3[2] = value.property.vecValue.z;
					pVariable->Set(tempVec3);
				}
				else
				{
					throw std::logic_error("Data type is invalid.");
				}
			}
			else
			{
				throw std::logic_error(string("\"") + pVarPath + "\"" + " is an invalid parameter.");
			}
		}
	}
	else
	{
		throw std::logic_error(string("\"") + pObjectName + "\" is an invalid entity.");
	}
}

static void PyOpenEntityArchetype(const char* archetype)
{

}

DECLARE_PYTHON_MODULE(entity);

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetEntityParam, general, get_entity_param,
                                          "Gets an object param",
                                          "general.get_entity_param(str entityName, str paramName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetEntityParam, general, set_entity_param,
                                          "Sets an object param",
                                          "general.set_entity_param(str entityName, str paramName, [ bool || int || float || str || (float, float, float) ] paramValue)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetEntityProperty, general, get_entity_property,
                                          "Gets an entity property or property2",
                                          "general.get_entity_property(str entityName, str propertyName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetEntityProperty, general, set_entity_property,
                                          "Sets an entity property or property2",
                                          "general.set_entity_property(str entityName, str propertyName, [ bool || int || float || str || (float, float, float) ] propertyValue)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyOpenEntityArchetype, entity, open_archetype,
                                     "Open archetype in a Entity Archetype editor",
                                     "entity.open_archetype archetypeName");

