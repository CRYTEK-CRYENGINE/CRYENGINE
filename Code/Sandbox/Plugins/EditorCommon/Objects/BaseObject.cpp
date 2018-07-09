// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <Preferences/ViewportPreferences.h>
#include "Objects/BaseObject.h"
#include "Objects/ObjectLoader.h"
#include "Objects/IObjectLayer.h"
#include "Objects/IObjectLayerManager.h"
#include "IDisplayViewport.h"
#include "Objects/DisplayContext.h"
#include "Objects/ISelectionGroup.h"
#include "Objects/ObjectPropertyWidget.h"
#include "Objects/ObjectManager.h"
#include "Objects/InspectorWidgetCreator.h"
#include "IIconManager.h"
#include "IObjectManager.h"
#include "Util/Math.h"
#include "Util/AffineParts.h"
#include "Controls/DynamicPopupMenu.h"
#include "EditTool.h"
#include "UsedResources.h"
#include "IEditorMaterial.h"
#include "Grid.h"

#include "IAIManager.h"
#include "IUndoObject.h"

#include <CryMovie/IMovieSystem.h>
#include <CrySystem/ITimer.h>

// To use the Andrew's algorithm in order to make convex hull from the points, this header is needed.
#include "Util/GeometryUtil.h"

#include <CrySerialization/Serializer.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/Enum.h>
#include <Serialization/Decorators/EditorActionButton.h>

#include <CrySystem/ICryLink.h>

#include <EditorFramework/Editor.h>
#include <EditorFramework/Preferences.h>
#include <Preferences/GeneralPreferences.h>
#include <Gizmos/GizmoManager.h>
#include <AssetSystem/AssetManager.h>

SERIALIZATION_ENUM_BEGIN(ESystemConfigSpec, "SystemConfigSpec")
SERIALIZATION_ENUM(CONFIG_CUSTOM, "notset", "Any");
SERIALIZATION_ENUM(CONFIG_LOW_SPEC, "low", "Low");
SERIALIZATION_ENUM(CONFIG_MEDIUM_SPEC, "med", "Medium");
SERIALIZATION_ENUM(CONFIG_HIGH_SPEC, "high", "High");
SERIALIZATION_ENUM(CONFIG_VERYHIGH_SPEC, "veryhigh", "Very High");
SERIALIZATION_ENUM(CONFIG_DURANGO, "durango", "Xbox One");
SERIALIZATION_ENUM(CONFIG_ORBIS, "orbis", "PS4");
SERIALIZATION_ENUM(CONFIG_DETAIL_SPEC, "detail", "Detail");
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(ObjectType, "Object Type")
SERIALIZATION_ENUM(OBJTYPE_GROUP, "group", "Group")
SERIALIZATION_ENUM(OBJTYPE_TAGPOINT, "tagpoint", "Tag Point");
SERIALIZATION_ENUM(OBJTYPE_AIPOINT, "aipoint", "AI Point");
SERIALIZATION_ENUM(OBJTYPE_ENTITY, "entity", "Entity");
SERIALIZATION_ENUM(OBJTYPE_SHAPE, "shape", "Shape");
SERIALIZATION_ENUM(OBJTYPE_VOLUME, "volume", "Volume");
SERIALIZATION_ENUM(OBJTYPE_BRUSH, "brush", "Brush");
SERIALIZATION_ENUM(OBJTYPE_PREFAB, "prefab", "Prefab");
SERIALIZATION_ENUM(OBJTYPE_SOLID, "solid", "Solid");
SERIALIZATION_ENUM(OBJTYPE_CLOUD, "cloud", "Cloud");
SERIALIZATION_ENUM(OBJTYPE_CLOUDGROUP, "cloudgroup", "Cloud Group");
SERIALIZATION_ENUM(OBJTYPE_VOXEL, "voxel", "Voxel");
SERIALIZATION_ENUM(OBJTYPE_ROAD, "road", "Road");
SERIALIZATION_ENUM(OBJTYPE_OTHER, "other", "Other");
SERIALIZATION_ENUM(OBJTYPE_DECAL, "decal", "Decal");
SERIALIZATION_ENUM(OBJTYPE_DISTANCECLOUD, "distancecloud", "Distance Cloud");
SERIALIZATION_ENUM(OBJTYPE_TELEMETRY, "telemetry", "Telemetry");
SERIALIZATION_ENUM(OBJTYPE_REFPICTURE, "refpicture", "Reference Picture");
SERIALIZATION_ENUM(OBJTYPE_GEOMCACHE, "geomcache", "Geometry Cache");
SERIALIZATION_ENUM(OBJTYPE_VOLUMESOLID, "volumesolid", "Volume Solid");
//Intentionally not serializing "any" type since it was mostly used for UI and is not a valid value
SERIALIZATION_ENUM_END()

namespace BO_Private
{
COLORREF kLinkColorParent = RGB(0, 255, 255);
COLORREF kLinkColorChild = RGB(0, 0, 255);
COLORREF kLinkColorGray = RGB(128, 128, 128);

typedef std::pair<Vec2, Vec2> Edge2D;

inline Vec3 Rgb2Vec(COLORREF color)
{
	return Vec3(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f);
}

bool IsClockwiseDirection(const Vec2& p1, const Vec2& p2, const Vec3& p3)
{
	return (((p3.y - p1.y) * (p2.x - p1.x)) > ((p2.y - p1.y) * (p3.x - p1.x)));
}

bool DoTwoLineSegmentsIntersect(const Vec2& p1, const Vec2& p2, const Vec2& q1, const Vec2& q2)
{
	const bool bIsClockwise1 = IsClockwiseDirection(p1, p2, q2);
	const bool bIsClockwise2 = IsClockwiseDirection(p1, p2, q1);
	const bool bIsClockwise3 = IsClockwiseDirection(p1, q1, q2);
	const bool bIsClockwise4 = IsClockwiseDirection(p2, q1, q2);
	return bIsClockwise1 != bIsClockwise2 && bIsClockwise3 != bIsClockwise4;
}

bool DoesLineSegmentIntersectConvexHull(const Vec2& p1, const Vec2& p2, const Edge2D* edgelist, int edgeListCount)
{
	for (int i = 0; i < edgeListCount; ++i)
	{
		if (DoTwoLineSegmentsIntersect(p1, p2, edgelist[i].first, edgelist[i].second))
			return true;
	}
	return false;
}

bool IsIncludePointsInConvexHull(Edge2D* pEdgeArray0, int nEdgeArray0Size, Edge2D* pEdgeArray1, int nEdgeArray1Size)
{
	if (!pEdgeArray0 || !pEdgeArray1 || nEdgeArray0Size <= 0 || nEdgeArray1Size <= 0)
		return false;

	static const float kPointEdgeMaxInsideDistance(0.05f);

	bool bInside(true);
	for (int i = 0; i < nEdgeArray0Size; ++i)
	{
		const Vec2& v(pEdgeArray0[i].first);
		bInside = true;
		for (int k = 0; k < nEdgeArray1Size; ++k)
		{
			const Vec2& v0 = pEdgeArray1[k].first;
			const Vec2& v1 = pEdgeArray1[k].second;
			const Vec3 direction(v1.x - v0.x, v1.y - v0.y, 0);
			const Vec3 z = Vec3(0, 0, 1).Cross(direction);
			Vec2 normal = Vec2(z.x, z.y);
			normal.Normalize();
			const float distance = -normal.Dot(v0);
			const auto d = normal.Dot(v) + distance;
			if (d > kPointEdgeMaxInsideDistance)
			{
				bInside = false;
				break;
			}
		}
		if (bInside)
			break;
	}
	return bInside;
}

void ModifyConvexEdgeDirection(Edge2D* pEdgeArray, int nEdgeArraySize)
{
	if (!pEdgeArray || nEdgeArraySize < 2)
		return;
	Vec3 v0(pEdgeArray[0].first.x - pEdgeArray[0].second.x, pEdgeArray[0].first.y - pEdgeArray[0].second.y, 0);
	Vec3 v1(pEdgeArray[1].second.x - pEdgeArray[1].first.x, pEdgeArray[1].second.y - pEdgeArray[1].first.y, 0);
	Vec3 vCross = v0.Cross(v1);
	if (vCross.z < 0)
	{
		for (int i = 0; i < nEdgeArraySize; ++i)
			std::swap(pEdgeArray[i].first, pEdgeArray[i].second);
	}
}

bool CalculateConvexEdgesForOBB(const CPoint* obb_p, const int maxSizeOfEdgeList, Edge2D* edgeList, int& edgeListCount)
{
	std::vector<Vec3> pointsForRegion1;
	pointsForRegion1.reserve(maxSizeOfEdgeList);
	for (int i = 0; i < maxSizeOfEdgeList; ++i)
		pointsForRegion1.push_back(Vec3(obb_p[i].x, obb_p[i].y, 0));

	std::vector<Vec3> convexHullForRegion1;
	ConvexHull2D(convexHullForRegion1, pointsForRegion1);
	edgeListCount = convexHullForRegion1.size();
	if (edgeListCount < 3 || edgeListCount > maxSizeOfEdgeList)
		return false;

	for (int i = 0; i < edgeListCount; ++i)
	{
		int iNextI = (i + 1) % edgeListCount;
		edgeList[i] = Edge2D(Vec2(convexHullForRegion1[i].x, convexHullForRegion1[i].y), Vec2(convexHullForRegion1[iNextI].x, convexHullForRegion1[iNextI].y));
	}
	return true;
}

}

#define INVALID_POSITION_EPSILON 100000

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Undo object for CBaseObject.
class CUndoBaseObject : public IUndoObject
{
public:
	CUndoBaseObject(CBaseObject* pObj, const char* undoDescription);

protected:
	virtual int         GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() override { return m_undoDescription; };
	virtual const char* GetObjectName() override;

	virtual void        Undo(bool bUndo) override;
	virtual void        Redo() override;

protected:
	string     m_undoDescription;
	CryGUID    m_guid;
	XmlNodeRef m_undo;
	XmlNodeRef m_redo;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Undo object for CBaseObject that only stores its transform, color, area and minSpec
class CUndoBaseObjectMinimal : public IUndoObject
{
public:
	CUndoBaseObjectMinimal(CBaseObject* obj, const char* undoDescription, int flags);

protected:
	virtual int         GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() override { return m_undoDescription; };
	virtual const char* GetObjectName() override;

	virtual void        Undo(bool bUndo) override;
	virtual void        Redo() override;

private:
	struct StateStruct
	{
		Vec3     pos;
		Quat     rotate;
		Vec3     scale;
		COLORREF color;
		float    area;
		int      minSpec;
	};

	void SetTransformsFromState(CBaseObject* pObject, const StateStruct& state, bool bUndo);

	CryGUID     m_guid;
	string      m_undoDescription;
	StateStruct m_undoState;
	StateStruct m_redoState;
};

class CUndoSetLayer : public IUndoObject
{
public:
	CUndoSetLayer(CBaseObject* pObj, IObjectLayer* pOldLayer)
		: m_objGuid(pObj->GetId())
	{
		if (pOldLayer)
			m_oldLayerGuid = pOldLayer->GetGUID();
		if (pObj->GetLayer())
			m_newLayerGuid = pObj->GetLayer()->GetGUID();
	}

	virtual void Undo(bool bUndo) override
	{
		SetLayer(m_oldLayerGuid);
	}

	virtual void Redo() override
	{
		SetLayer(m_newLayerGuid);
	}

	virtual const char* GetDescription() override { return "Set object's layer"; }

private:
	void SetLayer(const CryGUID& guid)
	{
		auto obj = GetIEditor()->GetObjectManager()->FindObject(m_objGuid);
		if (obj)
		{
			auto pLayer = GetIEditor()->GetObjectManager()->GetIObjectLayerManager()->FindLayer(guid);
			obj->SetLayer(pLayer);
		}

	}

	CryGUID m_objGuid;
	CryGUID m_oldLayerGuid;
	CryGUID m_newLayerGuid;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for attach/detach changes
class CUndoAttachBaseObject : public IUndoObject
{
public:
	CUndoAttachBaseObject(CBaseObject* pAttachedObject, IObjectLayer* pOldLayer, bool bKeepPos, bool bPlaceOnRoot, bool bAttach)
		: m_attachedObjectGUID(pAttachedObject->GetId())
		, m_parentObjectGUID(pAttachedObject->GetParent()->GetId())
		, m_bKeepPos(bKeepPos)
		, m_bPlaceOnRoot(bPlaceOnRoot)
		, m_bAttach(bAttach)
	{
		if (pOldLayer)
			m_oldLayerGUID = pOldLayer->GetGUID();
	}

	virtual void Undo(bool bUndo) override
	{
		if (m_bAttach) Detach();
		else Attach();
	}

	virtual void Redo() override
	{
		if (m_bAttach) Attach();
		else Detach();
	}

private:
	void Attach()
	{
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		CBaseObject* pObject = pObjectManager->FindObject(m_attachedObjectGUID);
		CBaseObject* pParentObject = pObjectManager->FindObject(m_parentObjectGUID);

		if (pObject && pParentObject)
		{
			if (GetIEditor()->IsCGroup(pParentObject))
				pParentObject->AddMember(pObject, m_bKeepPos);
			else
				pParentObject->AttachChild(pObject, m_bKeepPos);

			pObject->UpdatePrefab();
		}
	}

	void Detach()
	{
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		CBaseObject* pObject = pObjectManager->FindObject(m_attachedObjectGUID);
		CBaseObject* pParentObject = pObjectManager->FindObject(m_parentObjectGUID);
		IObjectLayer* pOldLayer = pObjectManager->GetIObjectLayerManager()->FindLayer(m_oldLayerGUID);

		if (pObject)
		{
			if (GetIEditor()->IsCGroup(pParentObject))
				pParentObject->RemoveMember(pObject, m_bKeepPos, m_bPlaceOnRoot);
			else
				pObject->DetachThis(m_bKeepPos, m_bPlaceOnRoot);

			if (pOldLayer != pObject->GetLayer())
				pObject->SetLayer(pOldLayer);

			pObject->UpdatePrefab();
		}
	}

	virtual int         GetSize() { return sizeof(CUndoAttachBaseObject); }
	virtual const char* GetDescription() override { return "Attachment Changed"; }

	CryGUID m_attachedObjectGUID;
	CryGUID m_parentObjectGUID;
	CryGUID m_oldLayerGUID;
	bool    m_bKeepPos;
	bool    m_bPlaceOnRoot;
	bool    m_bAttach;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for attach/detach changes
class CUndoLinkBaseObject : public IUndoObject
{
public:
	enum class ActionType
	{
		Link   = 0x1,
		Unlink = 0x2
	};

	CUndoLinkBaseObject(CBaseObject* pObject, ActionType actionType = ActionType::Link)
		: m_objectGUID(pObject->GetId())
		, m_linkedObjectGUID(pObject->GetLinkedTo()->GetId())
		, m_actionType(actionType)
	{
	}

	virtual void Undo(bool bUndo) override
	{
		if (m_actionType == ActionType::Link)
			Unlink();
		else
			Link();
	}

	virtual void Redo() override
	{
		if (m_actionType == ActionType::Link)
			Link();
		else
			Unlink();
	}

private:
	void Link()
	{
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		CBaseObject* pObject = pObjectManager->FindObject(m_objectGUID);
		CBaseObject* pParentObject = pObjectManager->FindObject(m_linkedObjectGUID);

		if (pObject && pParentObject)
			pObject->LinkTo(pParentObject);
	}

	void Unlink()
	{
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		CBaseObject* pObject = pObjectManager->FindObject(m_objectGUID);
		CBaseObject* pParentObject = pObjectManager->FindObject(m_linkedObjectGUID);

		if (pObject)
			pObject->UnLink();
	}

	virtual int         GetSize()  { return sizeof(CUndoLinkBaseObject); }
	virtual const char* GetDescription() override { return m_actionType == ActionType::Link ? "Objects linked" : "Objects unlinked"; }

	CryGUID    m_objectGUID;
	CryGUID    m_linkedObjectGUID;
	ActionType m_actionType;
};

//////////////////////////////////////////////////////////////////////////
CUndoBaseObject::CUndoBaseObject(CBaseObject* obj, const char* undoDescription)
{
	// Stores the current state of this object.
	assert(obj != 0);
	m_undoDescription = undoDescription;
	m_guid = obj->GetId();

	m_redo = 0;
	m_undo = XmlHelpers::CreateXmlNode("Undo");
	CObjectArchive ar(GetIEditor()->GetObjectManager(), m_undo, false);
	ar.bUndo = true;
	obj->Serialize(ar);

	obj->SetLayerModified();
}

//////////////////////////////////////////////////////////////////////////
const char* CUndoBaseObject::GetObjectName()
{
	if (CBaseObject* obj = GetIEditor()->GetObjectManager()->FindObject(m_guid))
	{
		return obj->GetName();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
void CUndoBaseObject::Undo(bool bUndo)
{
	CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!pObject)
		return;

	GetIEditor()->GetIUndoManager()->Suspend();

	if (bUndo)
	{
		m_redo = XmlHelpers::CreateXmlNode("Redo");
		// Save current object state.
		CObjectArchive ar(GetIEditor()->GetObjectManager(), m_redo, false);
		ar.bUndo = true;
		pObject->Serialize(ar);
	}

	// Undo object state.
	CObjectArchive ar(GetIEditor()->GetObjectManager(), m_undo, true);
	ar.bUndo = true;
	IObjectLayer* pLayer = pObject->GetLayer();
	pObject->Serialize(ar);
	pObject->SetLayerModified();

	if (pLayer && pLayer != pObject->GetLayer())
	{
		pLayer->SetModified();
	}

	if (bUndo)
	{
		pObject->UpdateGroup();
		pObject->UpdatePrefab();
	}

	GetIEditor()->GetIUndoManager()->Resume();
}

//////////////////////////////////////////////////////////////////////////
void CUndoBaseObject::Redo()
{
	CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!pObject)
		return;

	GetIEditor()->GetIUndoManager()->Suspend();

	CObjectArchive ar(GetIEditor()->GetObjectManager(), m_redo, true);
	ar.bUndo = true;
	IObjectLayer* pLayer = pObject->GetLayer();
	pObject->Serialize(ar);
	pObject->SetLayerModified();

	if (pLayer && pLayer != pObject->GetLayer())
	{
		pLayer->SetModified();
	}

	pObject->UpdateGroup();
	pObject->UpdatePrefab();

	GetIEditor()->GetIUndoManager()->Resume();
}

//////////////////////////////////////////////////////////////////////////
CUndoBaseObjectMinimal::CUndoBaseObjectMinimal(CBaseObject* pObj, const char* undoDescription, int flags)
{
	// Stores the current state of this object.
	assert(pObj != nullptr);
	m_undoDescription = undoDescription;
	m_guid = pObj->GetId();

	ZeroStruct(m_redoState);

	m_undoState.pos = pObj->GetPos();
	m_undoState.rotate = pObj->GetRotation();
	m_undoState.scale = pObj->GetScale();
	m_undoState.color = pObj->GetColor();
	m_undoState.minSpec = pObj->GetMinSpec();

	pObj->SetLayerModified();
}

//////////////////////////////////////////////////////////////////////////
const char* CUndoBaseObjectMinimal::GetObjectName()
{
	CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!pObject)
		return "";

	return pObject->GetName();
}

//////////////////////////////////////////////////////////////////////////
void CUndoBaseObjectMinimal::Undo(bool bUndo)
{
	CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!pObject)
		return;

	if (bUndo)
	{
		m_redoState.pos = pObject->GetPos();
		m_redoState.scale = pObject->GetScale();
		m_redoState.rotate = pObject->GetRotation();
		m_redoState.color = pObject->GetColor();
		m_redoState.minSpec = pObject->GetMinSpec();
	}

	SetTransformsFromState(pObject, m_undoState, bUndo);

	pObject->ChangeColor(m_undoState.color);
	pObject->SetMinSpec(m_undoState.minSpec, false);
	pObject->SetLayerModified();

	if (bUndo)
		pObject->UpdateGroup();
}

//////////////////////////////////////////////////////////////////////////
void CUndoBaseObjectMinimal::Redo()
{
	CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!pObject)
		return;

	SetTransformsFromState(pObject, m_redoState, true);

	pObject->ChangeColor(m_redoState.color);
	pObject->SetMinSpec(m_redoState.minSpec, false);
	pObject->SetLayerModified();
	pObject->UpdateGroup();
}

//////////////////////////////////////////////////////////////////////////
void CUndoBaseObjectMinimal::SetTransformsFromState(CBaseObject* pObject, const StateStruct& state, bool bUndo)
{
	uint32 flags = eObjectUpdateFlags_Undo;
	if (!bUndo)
	{
		flags |= eObjectUpdateFlags_UserInputUndo;
	}

	pObject->SetPos(state.pos, flags);
	pObject->SetScale(state.scale, flags);
	pObject->SetRotation(state.rotate, flags);
}

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CBaseObject, CObject);

//////////////////////////////////////////////////////////////////////////
void CObjectCloneContext::AddClone(CBaseObject* pFromObject, CBaseObject* pToObject)
{
	m_objectsMap[pFromObject] = pToObject;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectCloneContext::FindClone(CBaseObject* pFromObject) const
{
	CBaseObject* pTarget = stl::find_in_map(m_objectsMap, pFromObject, (CBaseObject*) NULL);
	return pTarget;
}

//////////////////////////////////////////////////////////////////////////
// CBaseObject implementation.
//////////////////////////////////////////////////////////////////////////
CBaseObject::CBaseObject()
	: m_pLinkedTo(nullptr)
	, m_bSuppressUpdatePrefab(false)
{
	m_pos(0, 0, 0);
	m_rotate.SetIdentity();
	m_scale(1, 1, 1);

	m_color = RGB(255, 255, 255);
	m_pMaterial = NULL;

	m_flags = 0;

	m_numRefs = 0;
	m_guid = CryGUID::Null();
	m_guidInPrefab = CryGUID::Null();

	m_layer = 0;

	m_parent = 0;

	m_lookat = 0;
	m_lookatSource = 0;

	m_bMatrixInWorldSpace = false;
	m_bMatrixValid = false;
	m_bWorldBoxValid = false;
	m_bUseMaterialLayersMask = false;

	m_nTextureIcon = 0;
	m_nMaterialLayersMask = 0;
	m_nMinSpec = 0;

	m_worldTM.SetIdentity();
	m_worldBounds.min.Set(0, 0, 0);
	m_worldBounds.max.Set(0, 0, 0);

	m_vDrawIconPos = Vec3(0, 0, 0);
	m_nIconFlags = 0;
	m_bInSelectionBox = false;

	m_pTransformDelegate = nullptr;
	m_bSupportsBoxHighlight = true;
}

//////////////////////////////////////////////////////////////////////////
IObjectManager* CBaseObject::GetObjectManager() const
{
	return GetIEditor()->GetObjectManager();
};

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetClassDesc(CObjectClassDesc* classDesc)
{
	m_classDesc = classDesc;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::Init(CBaseObject* prev, const string& file)
{
	ClearFlags(OBJFLAG_DELETED);

	if (prev != 0)
	{
		// Same layer.
		SetLayer(prev->GetLayer());

		SetUniqName(prev->GetName());
		SetLocalTM(prev->GetPos(), prev->GetRotation(), prev->GetScale());
		SetColor(prev->GetColor());
		m_nMaterialLayersMask = prev->m_nMaterialLayersMask;
		SetMaterial(prev->GetMaterial());
		SetMinSpec(prev->GetMinSpec(), false);

		// Copy all basic variables.
		if (m_pVarObject != nullptr)
		{
			if (CVarObject* pPrevVariables = prev->GetVarObject())
			{
				m_pVarObject->EnableUpdateCallbacks(false);
				m_pVarObject->CopyVariableValues(pPrevVariables);
				m_pVarObject->EnableUpdateCallbacks(true);
				m_pVarObject->OnSetValues();
			}
		}
	}

	m_nTextureIcon = m_classDesc->GetTextureIconId();
	if (m_classDesc->RenderTextureOnTop())
	{
		SetFlags(OBJFLAG_SHOW_ICONONTOP);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject::~CBaseObject()
{
	for (BaseObjectArray::iterator c = m_children.begin(); c != m_children.end(); c++)
	{
		CBaseObject* child = *c;
		child->m_parent = 0;
	}
	m_children.clear();

	for (auto pLinkedObject : m_linkedObjects)
		pLinkedObject->m_parent = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(m_name.c_str());
	CBaseObject* pGroup = GetGroup();

	// Must unlink all objects before removing from group or prefab
	// that way we can still have them attached to the parent
	UnLinkAll();

	// From parent
	if (pGroup)
	{
		bool bSuspended(GetIEditor()->SuspendUpdateCGroup(pGroup));
		GetIEditor()->RemoveMemberCGroup(pGroup, this);
		if (bSuspended)
			GetIEditor()->ResumeUpdateCGroup(pGroup);
	}
	else
	{
		DetachThis();
	}

	if (GetLinkedTo())
		UnLink();

	// From children
	DetachAll(true, true);

	SetLookAt(0);
	if (m_lookatSource)
	{
		m_lookatSource->SetLookAt(0);
	}
	SetFlags(OBJFLAG_DELETED);

	NotifyListeners(OBJECT_ON_DELETE);
	m_eventListeners.clear();

	if (m_pMaterial)
	{
		m_pMaterial->Release();
		m_pMaterial = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetName(const string& name)
{
	if (name == m_name)
		return;

	StoreUndo("Name");

	m_name = name;
	GetObjectManager()->RegisterObjectName(name);
	SetModified(false, false);

	NotifyListeners(OBJECT_ON_RENAME);
	GetIEditor()->GetObjectManager()->NotifyObjectListeners(this, OBJECT_ON_RENAME);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetUniqName(const string& name)
{
	if (name == m_name)
		return;
	SetName(GetObjectManager()->GenUniqObjectName(name));
}

//////////////////////////////////////////////////////////////////////////
const string& CBaseObject::GetName() const
{
	return m_name;
}

//////////////////////////////////////////////////////////////////////////
string CBaseObject::GetWarningsText() const
{
	string warnings;

	if (gViewportDebugPreferences.showScaleWarnings)
	{
		const EScaleWarningLevel scaleWarningLevel = GetScaleWarningLevel();
		if (scaleWarningLevel == eScaleWarningLevel_Rescaled)
		{
			warnings += "\\n  Warning: Object Scale is not 100%.";
		}
		else if (scaleWarningLevel == eScaleWarningLevel_RescaledNonUniform)
		{
			warnings += "\\n  Warning: Object has non-uniform scale.";
		}
	}

	if (gViewportDebugPreferences.showRotationWarnings)
	{
		const ERotationWarningLevel rotationWarningLevel = GetRotationWarningLevel();

		if (rotationWarningLevel == eRotationWarningLevel_Rotated)
		{
			warnings += "\\n  Warning: Object is rotated.";
		}
		else if (rotationWarningLevel == eRotationWarningLevel_RotatedNonRectangular)
		{
			warnings += "\\n  Warning: Object is rotated non-orthogonally.";
		}
	}

	return warnings;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsSameClass(CBaseObject* obj)
{
	return GetClassDesc() == obj->GetClassDesc();
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::SetPos(const Vec3& pos, int flags)
{
	auto currentPos = GetPos();

	bool equal = false;
	if (flags & eObjectUpdateFlags_MoveTool) // very sensitive in case of the move tool
	{
		equal = IsVectorsEqual(currentPos, pos, 0.0f);
	}
	else // less sensitive for others
	{
		equal = IsVectorsEqual(currentPos, pos);
	}

	if (equal)
	{
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Check if position is bad.
	if (fabs(pos.x) > INVALID_POSITION_EPSILON ||
	    fabs(pos.y) > INVALID_POSITION_EPSILON ||
	    fabs(pos.z) > INVALID_POSITION_EPSILON ||
	    !_finite(pos.x) || !_finite(pos.y) || !_finite(pos.z))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,
		           "Object %s, SetPos called with invalid position: (%f,%f,%f) %s",
		           (const char*)GetName(), pos.x, pos.y, pos.z,
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
		return false;
	}

	if (ShouldNotifyOfUpcomingAABBChanges())
	{
		OnBeforeAreaChange();
	}

	const bool ignoreSelection = (flags & eObjectUpdateFlags_IgnoreSelection) != 0;
	const bool bPositionDelegated = m_pTransformDelegate && m_pTransformDelegate->IsPositionDelegated(ignoreSelection);

	if (m_pTransformDelegate && (flags & eObjectUpdateFlags_Animated) == 0)
	{
		m_pTransformDelegate->SetTransformDelegatePos(pos, ignoreSelection);
	}

	//////////////////////////////////////////////////////////////////////////
	if (!bPositionDelegated && (flags & eObjectUpdateFlags_RestoreUndo) == 0 && (flags & eObjectUpdateFlags_Animated) == 0)
	{
		StoreUndo("Position", true, flags);
	}

	if (!bPositionDelegated)
	{
		m_pos = pos;
	}

	if (!(flags & eObjectUpdateFlags_DoNotInvalidate))
	{
		InvalidateTM(flags | eObjectUpdateFlags_PositionChanged);
	}

	SetModified(true, false);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::SetRotation(const Quat& rotate, int flags)
{
	if (!IsRotatable())
	{
		return false;
	}

	const Quat currentRotate = GetRotation();

	if (currentRotate.w == rotate.w && currentRotate.v.x == rotate.v.x
	    && currentRotate.v.y == rotate.v.y && currentRotate.v.z == rotate.v.z)
	{
		return false;
	}

	if (flags & eObjectUpdateFlags_ScaleTool)
	{
		return false;
	}

	if (ShouldNotifyOfUpcomingAABBChanges())
	{
		OnBeforeAreaChange();
	}

	const bool ignoreSelection = (flags & eObjectUpdateFlags_IgnoreSelection) != 0;
	const bool bRotationDelegated = m_pTransformDelegate && m_pTransformDelegate->IsRotationDelegated(ignoreSelection);
	if (m_pTransformDelegate && (flags & eObjectUpdateFlags_Animated) == 0)
	{
		m_pTransformDelegate->SetTransformDelegateRotation(rotate, ignoreSelection);
	}

	if (!bRotationDelegated && (flags & eObjectUpdateFlags_RestoreUndo) == 0 && (flags & eObjectUpdateFlags_Animated) == 0)
	{
		StoreUndo("Rotate", true, flags);
	}

	if (!bRotationDelegated)
	{
		m_rotate = rotate;
	}

	if (m_bMatrixValid && !(flags & eObjectUpdateFlags_DoNotInvalidate))
	{
		InvalidateTM(flags | eObjectUpdateFlags_RotationChanged);
	}

	SetModified(true, false);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::SetScale(const Vec3& scale, int flags)
{
	if (!IsScalable())
	{
		return false;
	}

	if (IsVectorsEqual(GetScale(), scale))
	{
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Check if position is bad.
	if (scale.x < 0.01 || scale.y < 0.01 || scale.z < 0.01)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Object %s, SetScale called with invalid scale: (%f,%f,%f) %s",
		           (const char*)GetName(), scale.x, scale.y, scale.z,
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
		return false;
	}
	//////////////////////////////////////////////////////////////////////////

	if (ShouldNotifyOfUpcomingAABBChanges())
	{
		OnBeforeAreaChange();
	}

	const bool ignoreSelection = (flags & eObjectUpdateFlags_IgnoreSelection) != 0;
	const bool bScaleDelegated = m_pTransformDelegate && m_pTransformDelegate->IsScaleDelegated(ignoreSelection);
	if (m_pTransformDelegate && (flags & eObjectUpdateFlags_Animated) == 0)
	{
		m_pTransformDelegate->SetTransformDelegateScale(scale, ignoreSelection);
	}

	if (!bScaleDelegated && (flags & eObjectUpdateFlags_RestoreUndo) == 0 && (flags & eObjectUpdateFlags_Animated) == 0)
	{
		StoreUndo("Scale", true, flags);
	}

	if (!bScaleDelegated)
	{
		m_scale = scale;
	}

	if (m_bMatrixValid && !(flags & eObjectUpdateFlags_DoNotInvalidate))
	{
		InvalidateTM(flags | eObjectUpdateFlags_ScaleChanged);
	}

	SetModified(true, false);

	return true;
}

//////////////////////////////////////////////////////////////////////////
const Vec3 CBaseObject::GetPos() const
{
	if (!m_pTransformDelegate)
	{
		return m_pos;
	}

	return m_pTransformDelegate->GetTransformDelegatePos(m_pos);
}

//////////////////////////////////////////////////////////////////////////
const Quat CBaseObject::GetRotation() const
{
	if (!m_pTransformDelegate)
	{
		return m_rotate;
	}

	return m_pTransformDelegate->GetTransformDelegateRotation(m_rotate);
}

//////////////////////////////////////////////////////////////////////////
const Vec3 CBaseObject::GetScale() const
{
	if (!m_pTransformDelegate)
	{
		return m_scale;
	}

	return m_pTransformDelegate->GetTransformDelegateScale(m_scale);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::ChangeColor(COLORREF color)
{
	if (color == m_color)
		return;

	StoreUndo("Color", true);

	SetColor(color);
	SetModified(false, false);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetColor(COLORREF color)
{
	m_color = color;
}

void CBaseObject::GetDisplayBoundBox(AABB& box)
{
	GetBoundBox(box);
}

//! Get bounding box of object in world coordinate space.
void CBaseObject::GetBoundBox(AABB& box)
{
	if (!m_bWorldBoxValid)
	{
		GetLocalBounds(m_worldBounds);
		if (!m_worldBounds.IsReset())
		{
			m_worldBounds.SetTransformedAABB(GetWorldTM(), m_worldBounds);
			m_bWorldBoxValid = true;
		}
	}
	box = m_worldBounds;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::GetLocalBounds(AABB& box)
{
	box.min.Set(0, 0, 0);
	box.max.Set(0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::UpdateUIVars()
{
	// Just notify inspector, he'll take care of the rest
	NotifyListeners(OBJECT_ON_UI_PROPERTY_CHANGED);
}
//////////////////////////////////////////////////////////////////////////

void CBaseObject::SetModified(bool boModifiedTransformOnly, bool bNotifyObjectManager)
{
	if(bNotifyObjectManager)
		GetObjectManager()->OnObjectModified(this, false, boModifiedTransformOnly);

	// Send signal to prefab if this object is part of any
	if (boModifiedTransformOnly)
		UpdatePrefab(eOCOT_ModifyTransform);
	else
		UpdatePrefab();
}

void CBaseObject::DrawDefault(DisplayContext& dc, COLORREF labelColor)
{
	using namespace BO_Private;
	Vec3 wp = GetWorldPos();

	bool bDisplaySelectionHelper = false;
	if (!CanBeDrawn(dc, bDisplaySelectionHelper))
		return;

	// Draw link between parent and child.
	if (dc.flags & DISPLAY_LINKS)
	{
		bool prefabOpenedCheck = true;
		// If we are part of a prefab draw links only if the prefab is in opened state
		if (CheckFlags(OBJFLAG_PREFAB))
		{
			CBaseObject* pPrefab = GetPrefab();
			prefabOpenedCheck = pPrefab && GetIEditor()->IsGroupOpen(pPrefab);
		}

		if (GetLinkedTo())
		{
			dc.DrawLine(GetLinkAttachPointWorldTM().GetTranslation(), wp, IsFrozen() ? kLinkColorGray : kLinkColorParent, IsFrozen() ? kLinkColorGray : kLinkColorChild);
		}
	}

	// Draw Bounding box
	if (dc.flags & DISPLAY_BBOX)
	{
		AABB box;
		GetBoundBox(box);
		dc.SetColor(Vec3(1, 1, 1));
		dc.DrawWireBox(box.min, box.max);
	}

	if (gViewportPreferences.displaySelectedObjectOrientation && IsSelected())
	{
		float textSize = 1.4;
		const Matrix34& m = GetWorldTM();
		float scale = dc.view->GetScreenScaleFactor(m.GetTranslation()) * gGizmoPreferences.axisGizmoSize * 0.5f;
		Vec3 xVec = m.GetTranslation() + scale * m.GetColumn0().GetNormalized();
		Vec3 yVec = m.GetTranslation() + scale * m.GetColumn1().GetNormalized();
		Vec3 zVec = m.GetTranslation() + scale * m.GetColumn2().GetNormalized();

		unsigned int oldFlags = dc.GetState();
		dc.DepthTestOff();
		dc.SetLineWidth(2.0f);

		dc.SetColor(Vec3(1, 0, 0));
		dc.DrawLine(m.GetTranslation(), xVec);

		dc.SetColor(Vec3(0, 1, 0));
		dc.DrawLine(m.GetTranslation(), yVec);

		dc.SetColor(Vec3(0, 0, 1));
		dc.DrawLine(m.GetTranslation(), zVec);

		if (gGizmoPreferences.axisGizmoText)
		{
			dc.SetColor(Vec3(1, 1, 1));
			dc.DrawTextLabel(ConvertToTextPos(xVec, Matrix34::CreateIdentity(), dc.view, dc.flags & DISPLAY_2D), textSize, "x");
			dc.DrawTextLabel(ConvertToTextPos(yVec, Matrix34::CreateIdentity(), dc.view, dc.flags & DISPLAY_2D), textSize, "y");
			dc.DrawTextLabel(ConvertToTextPos(zVec, Matrix34::CreateIdentity(), dc.view, dc.flags & DISPLAY_2D), textSize, "z");
		}

		dc.SetState(oldFlags);
	}

	if (IsHighlighted())
	{
		DrawHighlight(dc);
	}

	if (IsSelected())
	{
		const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();

		// If the number of selected object is over 2, the merged boundbox should be used to render the measurement axis.
		if (!pSelection || (pSelection && pSelection->GetCount() == 1))
		{
			DrawDimensions(dc);
		}
	}

	float objectToCameraDistanceSquared = GetIEditor()->GetSystem()->GetViewCamera().GetPosition().GetSquaredDistance(wp);
	if (bDisplaySelectionHelper && objectToCameraDistanceSquared < gViewportPreferences.selectionHelperDisplayThresholdSquared)
	{
		DrawSelectionHelper(dc, wp, labelColor, 1.0f);
	}
	else if (!(dc.flags & DISPLAY_HIDENAMES))
	{
		DrawLabel(dc, wp, labelColor);
	}

	if (!bDisplaySelectionHelper)
	{
		SetDrawTextureIconProperties(dc, wp);
		DrawTextureIcon(dc, wp, 1.f, bDisplaySelectionHelper, objectToCameraDistanceSquared);
		DrawWarningIcons(dc, wp);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawDimensions(DisplayContext& dc, AABB* pMergedBoundBox)
{
	if (HasMeasurementAxis() && gViewportPreferences.displayDimension)
	{
		AABB localBoundBox;
		GetLocalBounds(localBoundBox);
		DrawDimensionsImpl(dc, localBoundBox, pMergedBoundBox);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawDimensionsImpl(DisplayContext& dc, const AABB& localBoundBox, AABB* pMergedBoundBox)
{
	AABB boundBox;
	Matrix34 rotatedTM;
	bool bHave2Axis(false);
	float width(0);
	float height(0);
	float depth(0);

	if (pMergedBoundBox)
	{
		rotatedTM = Matrix34::CreateIdentity();
		boundBox = *pMergedBoundBox;
		width = boundBox.max.x - boundBox.min.x;
		height = boundBox.max.z - boundBox.min.z;
		depth = boundBox.max.y - boundBox.min.y;
	}
	else
	{
		rotatedTM = GetWorldRotTM();
		Matrix34 scaledTranslatedTM = GetWorldScaleTM();
		scaledTranslatedTM.SetTranslation(GetWorldPos());
		boundBox.SetTransformedAABB(scaledTranslatedTM, localBoundBox);

		IVariable* pVarWidth(NULL);
		IVariable* pVarDepth(NULL);
		IVariable* pVarHeight(NULL);

		IVariable* pVarDimX(NULL);
		IVariable* pVarDimY(NULL);
		IVariable* pVarDimZ(NULL);

		auto scale = GetScale();

		if (m_pVarObject != nullptr)
		{
			pVarWidth = m_pVarObject->FindVariable("Width");
			pVarDepth = m_pVarObject->FindVariable("Length");
			pVarHeight = m_pVarObject->FindVariable("Height");
			pVarDimX = m_pVarObject->FindVariable("DimX");
			pVarDimY = m_pVarObject->FindVariable("DimY");
			pVarDimZ = m_pVarObject->FindVariable("DimZ");
		}

		width = boundBox.max.x - boundBox.min.x;
		height = boundBox.max.z - boundBox.min.z;
		depth = boundBox.max.y - boundBox.min.y;

		if (pVarDimX && pVarDimY && pVarDimZ)
		{
			pVarDimX->Get(width);
			pVarDimZ->Get(height);
			pVarDimY->Get(depth);
			width *= scale.x;
			height *= scale.z;
			depth *= scale.y;
		}
		else if (pVarWidth && pVarDepth && pVarHeight)
		{
			// A case of an area box.
			pVarWidth->Get(width);
			pVarHeight->Get(height);
			pVarDepth->Get(depth);
			width *= scale.x;
			height *= scale.z;
			depth *= scale.y;
		}
		else if (!pVarWidth && !pVarDepth && pVarHeight)
		{
			// A case of an area shape.
			pVarHeight->Get(height);
			height *= scale.z;
		}
	}

	const float kMinimumLimitation(0.4f);
	if (width < kMinimumLimitation && height < kMinimumLimitation && depth < kMinimumLimitation)
		return;

	const float kEpsilon(0.001f);
	bHave2Axis = GetType() == OBJTYPE_DECAL || fabs(height) < kEpsilon;

	Vec3 basePoints[] = {
		Vec3(boundBox.min.x, boundBox.min.y, boundBox.min.z),
		Vec3(boundBox.min.x, boundBox.max.y, boundBox.min.z),
		Vec3(boundBox.max.x, boundBox.max.y, boundBox.min.z),
		Vec3(boundBox.max.x, boundBox.min.y, boundBox.min.z),
		Vec3(boundBox.min.x, boundBox.min.y, boundBox.max.z),
		Vec3(boundBox.min.x, boundBox.max.y, boundBox.max.z),
		Vec3(boundBox.max.x, boundBox.max.y, boundBox.max.z),
		Vec3(boundBox.max.x, boundBox.min.y, boundBox.max.z)
	};
	const int kElementSize(sizeof(basePoints) / sizeof(*basePoints));
	Vec3 axisDirections[kElementSize] = { Vec3(1, 1, 1), Vec3(1, -1, 1), Vec3(-1, -1, 1), Vec3(-1, 1, 1), Vec3(1, 1, -1), Vec3(1, -1, -1), Vec3(-1, -1, -1), Vec3(-1, 1, -1) };
	int nLoopCount = bHave2Axis ? (kElementSize / 2) : kElementSize;
	if (bHave2Axis)
	{
		for (int i = 0; i < nLoopCount; ++i)
			basePoints[i].z = 0.5f * (boundBox.min.z + boundBox.max.z);
	}

	// Find out the nearest base point of a boundbox from a camera position and use it as a pivot.
	const CCamera& camera = dc.GetCamera();
	Vec3 cameraPos(camera.GetPosition());
	Vec3 pivot(rotatedTM.TransformVector(basePoints[0] - GetWorldPos()) + GetWorldPos());
	float fNearestDist = (cameraPos - pivot).GetLength();
	int nNearestAxisIndex(0);
	bool bPrevVisible(camera.IsPointVisible(pivot));

	for (int i = 1; i < nLoopCount; ++i)
	{
		Vec3 candidatePivot(rotatedTM.TransformVector(basePoints[i] - GetWorldPos()) + GetWorldPos());
		float candidateLength = (candidatePivot - cameraPos).GetLength();
		bool bVisible = camera.IsPointVisible(candidatePivot);
		if (bVisible)
		{
			if (!bPrevVisible || candidateLength < fNearestDist)
			{
				fNearestDist = candidateLength;
				pivot = candidatePivot;
				nNearestAxisIndex = i;
			}
			bPrevVisible = bVisible;
		}
	}

	float fOriginArrowScale = dc.view->GetScreenScaleFactor(pivot) * 0.04f;

	Vec3 vX(width, 0, 0);
	Vec3 vY(0, depth, 0);
	Vec3 vZ(0, 0, height);

	vX = vX * axisDirections[nNearestAxisIndex].x;
	vY = vY * axisDirections[nNearestAxisIndex].y;
	vZ = vZ * axisDirections[nNearestAxisIndex].z;
	vX = rotatedTM.TransformVector(vX);
	vY = rotatedTM.TransformVector(vY);
	vZ = rotatedTM.TransformVector(vZ);

	const float kArrowPivotOffset = 0.1f;
	pivot = pivot + (-(vX + vY + vZ)).GetNormalized() * kArrowPivotOffset;

	Vec3 centerPt(boundBox.GetCenter());

	// Display texts of width, height and depth
	float fTextScale(1.3f);
	dc.SetColor(RGB(200, 200, 200));
	string str;

	const float kBrightness(0.35f);
	const float kTextBrightness(0.7f);
	const ColorF kXColor(1.0f, kBrightness, kBrightness, 0.9f);
	const ColorF kYColor(kBrightness, 1.0f, kBrightness, 0.9f);
	const ColorF kZColor(kBrightness, kBrightness, 1.0f, 0.9f);
	const ColorF kTextXColor(1.0f, kTextBrightness, kTextBrightness, 0.9f);
	const ColorF kTextYColor(kTextBrightness, 1.0f, kTextBrightness, 0.9f);
	const ColorF kTextZColor(kTextBrightness, kTextBrightness, 1.0f, 0.9f);
	const ColorF TextBoxColor(0, 0, 0, 0.75f);

	ColorB backupcolor = dc.GetColor();
	uint32 backupstate = dc.GetState();
	int backupThickness = dc.GetLineWidth();

	dc.SetState(backupstate | e_DepthTestOff);

	Vec3 vNX = vX.GetNormalized();
	Vec3 vNY = vY.GetNormalized();
	Vec3 vNZ = vZ.GetNormalized();

	const float kMinimumOffset(0.20f);
	const float kMaximumOffset(30.0f);
	float fMaximumOffset[3] = { kMaximumOffset, kMaximumOffset, kMaximumOffset };
	if (width > kMaximumOffset * 0.5f)
		fMaximumOffset[0] = width * 3.0f;
	if (depth > kMaximumOffset * 0.5f)
		fMaximumOffset[1] = depth * 3.0f;
	if (height > kMaximumOffset * 0.5f)
		fMaximumOffset[2] = height * 3.0f;
	Vec3 textPos[3] = { pivot, pivot, pivot };
	Vec3 textMinPos[3] = { pivot + vNX * kMinimumOffset, pivot + vNY * kMinimumOffset, pivot + vNZ * kMinimumOffset };
	Vec3 textCenterPos[3] = { pivot + vX * 0.5f, pivot + vY * 0.5f, pivot + vZ * 0.5f };
	Vec3 textMaxPos[3] = { pivot + vNX * fMaximumOffset[0], pivot + vNY * fMaximumOffset[1], pivot + vNZ * fMaximumOffset[2] };

	const Vec3& cameraDir(camera.GetViewdir());

	for (int i = 0; i < 3; ++i)
	{
		Vec3 d = (textMaxPos[i] - cameraPos).GetNormalized();
		float fCameraDir = d.Dot(cameraDir);
		if (fCameraDir < 0)
			fCameraDir = 0;
		textPos[i] = textMinPos[i] + (textCenterPos[i] - textPos[i]) * fCameraDir;
	}

	SRayHitInfo hitInfo;
	bool bVisiableText[3] = { true, true, true };
	for (int i = 0; i < 3; ++i)
	{
		if (GetIEditor()->PickObject(cameraPos, textPos[i] - cameraPos, hitInfo, this))
		{
			if (hitInfo.fDistance < cameraPos.GetDistance(textPos[i]))
				bVisiableText[i] = false;
		}
	}

	if (bVisiableText[0])
	{
		str.Format("%.3f", width);
		DrawTextOn2DBox(dc, textPos[0], str, fTextScale, kTextXColor, TextBoxColor);
	}
	if (!bHave2Axis)
	{
		if (bVisiableText[2])
		{
			str.Format("%.3f", height);
			DrawTextOn2DBox(dc, textPos[2], str, fTextScale, kTextZColor, TextBoxColor);
		}
	}
	if (bVisiableText[1])
	{
		str.Format("%.3f", depth);
		DrawTextOn2DBox(dc, textPos[1], str, fTextScale, kTextYColor, TextBoxColor);
	}

	dc.SetState(backupstate | e_DepthTestOn);
	dc.SetLineWidth(4);

	// Draw arrows of each axis.
	dc.SetColor(kXColor);
	Vec3 xVec = pivot + vX;
	float fxArrowScale = dc.view->GetScreenScaleFactor(xVec) * 0.04f;
	dc.DrawArrow(pivot, xVec, fxArrowScale);
	dc.DrawArrow(xVec, pivot, fOriginArrowScale);
	if (!bHave2Axis)
	{
		dc.SetColor(kZColor);
		Vec3 zVec = pivot + vZ;
		float fzArrowScale = dc.view->GetScreenScaleFactor(zVec) * 0.04f;
		dc.DrawArrow(pivot, zVec, fzArrowScale);
		dc.DrawArrow(zVec, pivot, fOriginArrowScale);
	}
	dc.SetColor(kYColor);
	Vec3 yVec = pivot + vY;
	float fyArrowScale = dc.view->GetScreenScaleFactor(yVec) * 0.04f;
	dc.DrawArrow(pivot, yVec, fyArrowScale);
	dc.DrawArrow(yVec, pivot, fOriginArrowScale);

	dc.SetState(backupstate);
	dc.SetColor(backupcolor);
	dc.SetLineWidth(backupThickness);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawTextOn2DBox(DisplayContext& dc, const Vec3& pos, const char* text, float textScale, const ColorF& TextColor, const ColorF& TextBackColor)
{
	Vec3 worldPos = dc.ToWorldPos(pos);
	int vx=0, vy=0, vw=dc.GetWidth(), vh=dc.GetHeight();

	const CCamera& camera = dc.GetCamera();
	Vec3 screenPos;
	camera.Project(worldPos, screenPos, Vec2i(0, 0), Vec2i(0, 0));

	//! Font size information doesn't seem to exist so the proper size is used
	int textlen = strlen(text);
	float fontsize = 7.5f;
	float textwidth = fontsize * textlen;
	float textheight = 16.0f;

	screenPos.x = screenPos.x - textwidth * 0.5f;

	Vec3 textregion[4] = {
		Vec3(screenPos.x,             screenPos.y,              screenPos.z),
		Vec3(screenPos.x + textwidth, screenPos.y,              screenPos.z),
		Vec3(screenPos.x + textwidth, screenPos.y + textheight, screenPos.z),
		Vec3(screenPos.x,             screenPos.y + textheight, screenPos.z)
	};

	Vec3 textworldreign[4];
	Matrix34 dcInvTm = dc.GetMatrix().GetInverted();

	Matrix44A mProj, mView;
	mathMatrixPerspectiveFov(&mProj, camera.GetFov(), camera.GetProjRatio(), camera.GetNearPlane(), camera.GetFarPlane());
	mathMatrixLookAt(&mView, camera.GetPosition(), camera.GetPosition() + camera.GetViewdir(), Vec3(0, 0, 1));
	Matrix44A mInvViewProj = (mView * mProj).GetInverted();

	for (int i = 0; i < 4; ++i)
	{
		Vec4 projectedpos = Vec4((textregion[i].x - vx) / vw * 2.0f - 1.0f,
		                         -((textregion[i].y - vy) / vh) * 2.0f + 1.0f,
		                         textregion[i].z,
		                         1.0f);

		Vec4 wp = projectedpos * mInvViewProj;
		wp.x /= wp.w;
		wp.y /= wp.w;
		wp.z /= wp.w;
		textworldreign[i] = dcInvTm.TransformPoint(Vec3(wp.x, wp.y, wp.z));
	}

	ColorB backupcolor = dc.GetColor();
	uint32 backupstate = dc.GetState();

	dc.SetColor(TextBackColor);
	dc.SetDrawInFrontMode(true);

	dc.DrawQuad(textworldreign[3], textworldreign[2], textworldreign[1], textworldreign[0]);
	dc.SetColor(TextColor);
	dc.DrawTextLabel(pos, textScale, text);

	dc.SetDrawInFrontMode(false);
	dc.SetColor(backupcolor);
	dc.SetState(backupstate);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawSelectionHelper(DisplayContext& dc, const Vec3& pos, COLORREF labelColor, float alpha)
{
	// Magic Number to offset the text from the selection box
	// Can't use fixed value as we move in world space, so we need to use a fracture of the screenScaleFactor to always have a certain distance from the selection box in the viewport
	float r = dc.view->GetScreenScaleFactor(pos) * 0.006f;
	//Draw the Text somewhat below the selection box
   	Vec3 adjustedPos = pos;
	adjustedPos[2] -= 2*r;
	DrawLabel(dc, adjustedPos, labelColor);

	dc.SetColor(GetColor());
	if (IsHighlighted() || IsSelected() || IsInSelectionBox())
		dc.SetColor(dc.GetSelectedColor());

	uint32 nPrevState = dc.GetState();
	dc.DepthTestOff();
	dc.DrawWireBox(pos - Vec3(r, r, r), pos + Vec3(r, r, r));
	dc.SetState(nPrevState);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetDrawTextureIconProperties(DisplayContext& dc, const Vec3& pos, float alpha)
{
	if (gViewportPreferences.showIcons || gViewportPreferences.showSizeBasedIcons)
	{
		if (IsHighlighted())
			dc.SetColor(RGB(255, 120, 0), 0.8f * alpha);
		else if (IsSelected())
			dc.SetSelectedColor(alpha);
		else if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor(RGB(255, 255, 255), alpha);

		m_vDrawIconPos = pos;

		int nIconFlags = 0;
		if (CheckFlags(OBJFLAG_SHOW_ICONONTOP))
		{
			Vec3 objectPos = GetWorldPos();

			AABB box;
			GetBoundBox(box);
			m_vDrawIconPos.z = (m_vDrawIconPos.z - objectPos.z) + box.max.z;
			nIconFlags = DisplayContext::TEXICON_ALIGN_BOTTOM;
		}

		m_nIconFlags = nIconFlags;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawTextureIcon(DisplayContext& dc, const Vec3& pos, float alpha, bool bDisplaySelectionHelper, float distanceSquared)
{
	if (m_nTextureIcon && (gViewportPreferences.showIcons || gViewportPreferences.showSizeBasedIcons))
	{
		dc.DrawTextureLabel(GetTextureIconDrawPos(), OBJECT_TEXTURE_ICON_SIZE, OBJECT_TEXTURE_ICON_SIZE,
		                    GetTextureIcon(), GetTextureIconFlags(), 0, 0, OBJECT_TEXTURE_ICON_SCALE, distanceSquared);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawWarningIcons(DisplayContext& dc, const Vec3& pos)
{
	// Don't draw warning icons if they are beyond draw distance
	if ((dc.camera->GetPosition() - pos).GetLength() > gViewportDebugPreferences.warningIconsDrawDistance)
		return;

	if (gViewportPreferences.showIcons || gViewportPreferences.showSizeBasedIcons)
	{
		const int warningIconSize = OBJECT_TEXTURE_ICON_SIZE / 2;
		const int iconOffset = m_nTextureIcon ? (-OBJECT_TEXTURE_ICON_SIZE / 2) : 0;

		if (gViewportDebugPreferences.showScaleWarnings)
		{
			const EScaleWarningLevel scaleWarningLevel = GetScaleWarningLevel();

			if (scaleWarningLevel != eScaleWarningLevel_None)
			{
				dc.SetColor(RGB(255, scaleWarningLevel == eScaleWarningLevel_RescaledNonUniform ? 50 : 255, 50), 1.0f);
				dc.DrawTextureLabel(GetTextureIconDrawPos(), warningIconSize, warningIconSize,
				                    GetIEditor()->GetIconManager()->GetIconTexture(eIcon_ScaleWarning), GetTextureIconFlags(),
				                    -warningIconSize / 2, iconOffset - (warningIconSize / 2));
			}
		}

		if (gViewportDebugPreferences.showRotationWarnings)
		{
			const ERotationWarningLevel rotationWarningLevel = GetRotationWarningLevel();
			if (rotationWarningLevel != eRotationWarningLevel_None)
			{
				dc.SetColor(RGB(255, rotationWarningLevel == eRotationWarningLevel_RotatedNonRectangular ? 50 : 255, 50), 1.0f);
				dc.DrawTextureLabel(GetTextureIconDrawPos(), warningIconSize, warningIconSize,
				                    GetIEditor()->GetIconManager()->GetIconTexture(eIcon_RotationWarning), GetTextureIconFlags(),
				                    warningIconSize / 2, iconOffset - (warningIconSize / 2));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawLabel(DisplayContext& dc, const Vec3& pos, COLORREF labelColor, float alpha, float size)
{
	// Check if our group is not closed.
	if (!GetGroup() || GetIEditor()->IsGroupOpen(GetGroup()))
	{
		AABB box;
		GetBoundBox(box);

		//p.z = box.max.z + 0.2f;
		if ((dc.flags & DISPLAY_2D) && labelColor == RGB(255, 255, 255))
		{
			labelColor = RGB(0, 0, 0);
		}
	}

	float camDist = dc.camera->GetPosition().GetDistance(pos);
	float maxDist = gViewportPreferences.labelsDistance;
	if (camDist < gViewportPreferences.labelsDistance || (dc.flags & DISPLAY_SELECTION_HELPERS))
	{
		float range = maxDist / 2.0f;
		Vec3 c = BO_Private::Rgb2Vec(labelColor);
		if (IsSelected())
			c = BO_Private::Rgb2Vec(dc.GetSelectedColor());

		float col[4] = { c.x, c.y, c.z, 1 };
		if (dc.flags & DISPLAY_SELECTION_HELPERS)
		{
			if (IsHighlighted())
				c = BO_Private::Rgb2Vec(dc.GetSelectedColor());
			col[0] = c.x;
			col[1] = c.y;
			col[2] = c.z;
		}
		else if (camDist > range)
		{
			col[3] = col[3] * (1.0f - (camDist - range) / range);
		}

		dc.SetColor(col[0], col[1], col[2], col[3] * alpha);
		dc.DrawTextLabel(pos, size, GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DrawHighlight(DisplayContext& dc)
{
	if (!m_nTextureIcon && m_bSupportsBoxHighlight)
	{
		AABB box;
		GetLocalBounds(box);

		dc.PushMatrix(GetWorldTM());
		dc.DrawWireBox(box.min, box.max);
		dc.SetLineWidth(1);
		dc.PopMatrix();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::CanBeDrawn(const DisplayContext& dc, bool& outDisplaySelectionHelper) const
{
	bool bResult = true;
	outDisplaySelectionHelper = false;

	if (dc.flags & DISPLAY_SELECTION_HELPERS)
	{
		// Check if this object type is masked for selection.
		if ((GetType() & gViewportSelectionPreferences.objectSelectMask) && !IsFrozen())
		{
			if (IsSkipSelectionHelper())
				return bResult;
			if (CanBeHightlighted())
				outDisplaySelectionHelper = true;
		}
		else
		{
			// Object helpers should not be displayed when object is not for selection.
			bResult = false;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsInCameraView(const CCamera& camera)
{
	AABB bbox;
	GetDisplayBoundBox(bbox);
	return (camera.IsAABBVisible_F(AABB(bbox.min, bbox.max)));
}

//////////////////////////////////////////////////////////////////////////
float CBaseObject::GetCameraVisRatio(const CCamera& camera)
{
	AABB bbox;
	GetBoundBox(bbox);

	static const float defaultVisRatio = 1000.0f;

	const float objectHeightSq = max(1.0f, (bbox.max - bbox.min).GetLengthSquared());
	const float camdistSq = (bbox.min - camera.GetPosition()).GetLengthSquared();
	float visRatio = defaultVisRatio;
	if (camdistSq > FLT_EPSILON)
		visRatio = objectHeightSq / camdistSq;

	return visRatio;
}

//////////////////////////////////////////////////////////////////////////
int CBaseObject::MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseMove || event == eMouseLDown)
	{
		Vec3 pos = view->MapViewToCP(point, 0, true, GetCreationOffsetFromTerrain());
		SetPos(pos);

		if (event == eMouseLDown)
		{
			return MOUSECREATE_OK;
		}
	}
	else if (event == eMouseWheel)
	{
		double angle = 1;
		if (gSnappingPreferences.angleSnappingEnabled())
			angle = gSnappingPreferences.angleSnap();

		Quat rot = GetRotation();
		rot.SetRotationXYZ(Ang3(0, 0, rot.GetRotZ() + DEG2RAD(point.x > 0 ? angle * (-1) : angle)));
		SetRotation(rot);
	}

	return MOUSECREATE_CONTINUE;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnEvent(ObjectEvent event)
{
	switch (event)
	{
	case EVENT_CONFIG_SPEC_CHANGE:
		UpdateVisibility(!IsHidden());
		break;
	case EVENT_TRANSFORM_FINISHED:
		GetObjectManager()->OnObjectModified(this, false, false);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetShared(bool bShared)
{

}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetHidden(bool bHidden, bool bAnimated)
{
	// test first if the current layer is hidden beforehand AND we're not currently loading a level
	if (!GetLayer()->IsVisible() && !GetIEditor()->GetObjectManager()->IsLoading())
	{
		CUndo undo("Show Object Along With Layer");
		// collect children of the same layer
		CBaseObjectsArray children;
		GetIEditor()->GetObjectManager()->GetObjects(children, BaseObjectFilterFunctor(&CBaseObject::FilterByLayer, GetLayer()));
		for (auto child : children)
		{
			if (child == this)
				// show myself
				child->ClearFlags(OBJFLAG_HIDDEN);
			else
				// hide all other children
				child->SetFlags(OBJFLAG_HIDDEN);
		}
		// then show the layer
		IObjectLayer* layer = GetLayer();
		do
		{
			layer->SetVisible(true, false);
			layer = layer->GetParentIObjectLayer();
		}
		while (layer);
		for (auto child : children)
			child->UpdateVisibility(!child->IsHidden());
	}
	else if (CheckFlags(OBJFLAG_HIDDEN) != bHidden)
	{
		if (!bAnimated)
		{
			StoreUndo("Hide Object");
		}

		if (bHidden)
			SetFlags(OBJFLAG_HIDDEN);
		else
			ClearFlags(OBJFLAG_HIDDEN);
		UpdateVisibility(!IsHidden());
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::FilterByLayer(CBaseObject const& obj, void* pLayer)
{
	if (obj.CheckFlags(OBJFLAG_DELETED))
		return false;

	if (obj.GetLayer() == pLayer)
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetFrozen(bool bFrozen)
{
	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();

	// test first if the current layer is actually frozen beforehand AND we're not currently loading a level
	if (GetLayer()->IsFrozen() && !pObjectManager->IsLoading())
	{
		// the only case this situation is currently valid is when a child
		// object is frozen because of the parent layer - and the object's
		// actual "frozen" state might be masked off by the layer

		CUndo undo("Unfreeze Object Along With Layer");
		// collect children of the same layer
		CBaseObjectsArray children;
		pObjectManager->GetObjects(children, BaseObjectFilterFunctor(&CBaseObject::FilterByLayer, GetLayer()));
		for (auto child : children)
		{
			if (child == this)
				// unfreeze myself
				child->ClearFlags(OBJFLAG_FROZEN);
			else
				// freeze all other children
				child->SetFlags(OBJFLAG_FROZEN);
		}
		// then unfreeze the layer alone
		GetLayer()->SetFrozen(false, false);
	}
	else if (CheckFlags(OBJFLAG_FROZEN) != bFrozen)
	{
		// First deselect, so the selection state is saved before freezing. If we don't do that
		// selection won't be restored properly if we are restoring from undo,
		// since undos get restored in reverse order and object will still think it's frozen
		if (bFrozen && IsSelected())
		{
			pObjectManager->UnselectObject(this);
		}

		StoreUndo("Freeze Object");
		if (bFrozen)
			SetFlags(OBJFLAG_FROZEN);
		else
			ClearFlags(OBJFLAG_FROZEN);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetHighlight(bool bHighlight)
{
	if (bHighlight)
		SetFlags(OBJFLAG_HIGHLIGHT);
	else
		ClearFlags(OBJFLAG_HIGHLIGHT);

	UpdateHighlightPassState(IsSelected(), bHighlight);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetSelected(bool bSelect)
{
	if (bSelect)
	{
		SetFlags(OBJFLAG_SELECTED);
		NotifyListeners(OBJECT_ON_SELECT);
	}
	else
	{
		ClearFlags(OBJFLAG_SELECTED);
		NotifyListeners(OBJECT_ON_UNSELECT);
	}

	UpdateHighlightPassState(bSelect, IsHighlighted());
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsHiddenBySpec() const
{
	if (!gViewportPreferences.applyConfigSpec)
		return false;

	// Check against min spec.
	auto configSpec = GetIEditor()->GetEditorConfigSpec();
	if (m_nMinSpec == CONFIG_DETAIL_SPEC && configSpec == CONFIG_LOW_SPEC)
		return true;

	return (m_nMinSpec != 0 && configSpec != 0 && m_nMinSpec > configSpec);
}

//////////////////////////////////////////////////////////////////////////
//! Returns true if object hidden.
bool CBaseObject::IsHidden() const
{
	if (!m_layer)
		return false;
	return (CheckFlags(OBJFLAG_HIDDEN)) || 
	       (!m_layer->IsVisible()) ||
	       (gViewportDebugPreferences.GetObjectHideMask() & GetType());
}

//////////////////////////////////////////////////////////////////////////
//! Returns true if object frozen.
bool CBaseObject::IsFrozen() const
{
	if (!m_layer)
		return false;

	return CheckFlags(OBJFLAG_FROZEN) || m_layer->IsFrozen(false);
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsSelectable() const
{
	// Not selectable if hidden.
	if (IsHidden())
		return false;

	// Not selectable if frozen.
	if (IsFrozen())
		return false;

	// Not selectable if in closed group.
	CBaseObject* group = GetGroup();
	if (group)
	{
		if (!GetIEditor()->IsGroupOpen(group))
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::Serialize(CObjectArchive& ar)
{
	XmlNodeRef xmlNode = ar.node;

	ITransformDelegate* pTransformDelegate = m_pTransformDelegate;
	m_pTransformDelegate = nullptr;

	if (ar.bLoading)
	{
		// Loading.
		if (ar.ShouldResetInternalMembers())
		{
			m_flags = 0;
			m_nMinSpec = 0;
			m_scale.Set(1.0f, 1.0f, 1.0f);
		}

		int flags = 0;
		int oldFlags = m_flags;

		IObjectLayer* pLayer = 0;
		// Find layer name.
		CryGUID layerGUID;
		string layerName;

		if (xmlNode->getAttr("LayerGUID", layerGUID))
		{
			pLayer = GetObjectManager()->GetIObjectLayerManager()->FindLayer(layerGUID);
		}
		else if (xmlNode->getAttr("Layer", layerName))
		{
			pLayer = GetObjectManager()->GetIObjectLayerManager()->FindLayerByName(layerName);
		}


		if (!pLayer && ar.IsLoadingToCurrentLayer())
			pLayer = GetObjectManager()->GetIObjectLayerManager()->GetCurrentLayer();

		string name = m_name;
		string mtlName;

		// In Serialize do not use accessors bellow (GetPos(), .. ), as this will modify the level when saving, due to the potential delegated mode being activated
		Vec3 pos = m_pos;
		Vec3 scale = m_scale;
		Quat quat = m_rotate;
		Ang3 angles(0, 0, 0);
		uint32 nMinSpec = m_nMinSpec;

		COLORREF color = m_color;

		CryGUID parentId = CryGUID::Null();
		CryGUID linkToId = CryGUID::Null();
		CryGUID idInPrefab = CryGUID::Null();
		CryGUID lookatId = CryGUID::Null();

		xmlNode->getAttr("Name", name);
		xmlNode->getAttr("Pos", pos);
		if (!xmlNode->getAttr("Rotate", quat))
		{
			// Backward compatability.
			if (xmlNode->getAttr("Angles", angles))
			{
				angles = DEG2RAD(angles);
				//angles.z += gf_PI/2;
				quat.SetRotationXYZ(angles);
			}
		}

		xmlNode->getAttr("Scale", scale);
		xmlNode->getAttr("ColorRGB", color);
		xmlNode->getAttr("Flags", flags);
		xmlNode->getAttr("Parent", parentId);
		xmlNode->getAttr("LinkedTo", linkToId);
		xmlNode->getAttr("IdInPrefab", idInPrefab);
		xmlNode->getAttr("LookAt", lookatId);
		xmlNode->getAttr("Material", mtlName);
		xmlNode->getAttr("MinSpec", nMinSpec);

		if (nMinSpec <= CONFIG_VERYHIGH_SPEC) // Ignore invalid values.
			m_nMinSpec = nMinSpec;

		if (m_bUseMaterialLayersMask)
		{
			uint32 nMask = 0;
			xmlNode->getAttr("MatLayersMask", nMask);
			m_nMaterialLayersMask = nMask;
		}

		bool bHidden = flags & OBJFLAG_HIDDEN;
		bool bFrozen = flags & OBJFLAG_FROZEN;

		m_flags = flags;
		m_flags &= ~OBJFLAG_PERSISTMASK;
		m_flags |= (oldFlags) & (~OBJFLAG_PERSISTMASK);
		//SetFlags( flags & OBJFLAG_PERSISTMASK );
		m_flags &= ~OBJFLAG_SHARED;  // clear shared flag
		m_flags &= ~OBJFLAG_DELETED; // clear deleted flag

		if (ar.bUndo)
		{
			DetachThis(false, true);
			UnLink();
		}

		if (name != m_name)
		{
			// This may change object id.
			SetName(name);
		}

		//////////////////////////////////////////////////////////////////////////
		// Check if position is bad.
		if (fabs(pos.x) > INVALID_POSITION_EPSILON ||
		    fabs(pos.y) > INVALID_POSITION_EPSILON ||
		    fabs(pos.z) > INVALID_POSITION_EPSILON)
		{
			// File Not found.
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Object %s have invalid position (%f,%f,%f) %s", (const char*)GetName(), pos.x, pos.y, pos.z,
			           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
		}
		//////////////////////////////////////////////////////////////////////////

		if (!ar.bUndo)
			SetLocalTM(pos, quat, scale);
		else
			SetLocalTM(pos, quat, scale, eObjectUpdateFlags_Undo);

		if (idInPrefab != CryGUID::Null())
		{
			SetIdInPrefab(idInPrefab);
		}

		SetColor(color);

		if (pLayer)
		{
			SetLayer(pLayer);
		}

		SetFrozen(bFrozen);
		SetHidden(bHidden);

		//////////////////////////////////////////////////////////////////////////
		// Load material.
		//////////////////////////////////////////////////////////////////////////
		SetMaterial(mtlName);

		ar.SetResolveCallback(this, linkToId, functor(*this, &CBaseObject::ResolveLinkedTo));
		ar.SetResolveCallback(this, parentId, functor(*this, &CBaseObject::ResolveParent));
		ar.SetResolveCallback(this, lookatId, functor(*this, &CBaseObject::SetLookAt));

		InvalidateTM(0);
		SetModified(false, true);

		//////////////////////////////////////////////////////////////////////////

		if (ar.bUndo)
		{
			// If we are selected update UI Panel.
			UpdateUIVars();
		}

		// We reseted the min spec and deserialized it so set it internally
		if (ar.ShouldResetInternalMembers())
		{
			SetMinSpec(m_nMinSpec);
		}
	}
	else
	{
		// Saving.
		const bool isPartOfPrefab = IsPartOfPrefab();

		// This attributed only readed by ObjectManager.
		xmlNode->setAttr("Type", GetTypeName());

		if (m_layer)
		{
			xmlNode->setAttr("Layer", m_layer->GetName());
			xmlNode->setAttr("LayerGUID", m_layer->GetGUID());
		}

		xmlNode->setAttr("Id", m_guid);

		if (isPartOfPrefab && !ar.IsSavingInPrefab())
		{
			xmlNode->setAttr("IdInPrefab", m_guidInPrefab);
		}

		xmlNode->setAttr("Name", GetName());

		if (m_parent)
			xmlNode->setAttr("Parent", m_parent->GetId());

		if (m_pLinkedTo)
			xmlNode->setAttr("LinkedTo", m_pLinkedTo->GetId());

		if (m_lookat)
			xmlNode->setAttr("LookAt", m_lookat->GetId());

		if (isPartOfPrefab || !GetPos().IsZero())
			xmlNode->setAttr("Pos", GetPos());

		xmlNode->setAttr("Rotate", m_rotate);

		auto scale = GetScale();

		if (scale != Vec3(1, 1, 1))
			xmlNode->setAttr("Scale", scale);

		xmlNode->setAttr("ColorRGB", GetColor());

		int flags = m_flags & OBJFLAG_PERSISTMASK;
		if (flags != 0)
			xmlNode->setAttr("Flags", flags);

		if (m_pMaterial)
			xmlNode->setAttr("Material", GetMaterialName());

		if (m_nMinSpec != 0)
			xmlNode->setAttr("MinSpec", (uint32)m_nMinSpec);

		if (m_bUseMaterialLayersMask)
		{
			uint32 nMask = m_nMaterialLayersMask;
			xmlNode->setAttr("MatLayersMask", nMask);
		}
	}

	// Serialize variables after default entity parameters.
	if (m_pVarObject != nullptr)
	{
		m_pVarObject->Serialize(xmlNode, ar.bLoading);
	}

	m_pTransformDelegate = pTransformDelegate;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CBaseObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	// This function exports object to the Engine

	XmlNodeRef objNode = xmlNode->newChild("Object");

	objNode->setAttr("Type", GetTypeName());
	objNode->setAttr("Name", GetName());

	if (m_pMaterial)
		objNode->setAttr("Material", m_pMaterial->GetName());

	Vec3 pos;
	Vec3 scale;
	Quat rotate;

	if (m_parent)
	{
		// Export world coordinates.
		AffineParts ap;
		ap.SpectralDecompose(GetWorldTM());

		pos = ap.pos;
		rotate = ap.rot;
		scale = ap.scale;
	}
	else
	{
		// Not using acccessors(GetPos()...) here as we want to save the base object position and ignore delegates
		pos = m_pos;
		rotate = m_rotate;
		scale = m_scale;
	}

	if (!pos.IsZero())
		objNode->setAttr("Pos", pos);

	if (!rotate.IsIdentity())
		objNode->setAttr("Rotate", rotate);

	if (!scale.IsZero())
		objNode->setAttr("Scale", scale);

	if (m_nMinSpec != 0)
		objNode->setAttr("MinSpec", (uint32)m_nMinSpec);

	// Save variables.
	if (m_pVarObject != nullptr)
	{
		m_pVarObject->Serialize(objNode, false);
	}

	return objNode;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CBaseObject::FindObject(CryGUID id) const
{
	return GetObjectManager()->FindObject(id);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::StoreUndo(const char* UndoDescription, bool minimal, int flags)
{
	if (CUndo::IsRecording())
	{
		if (minimal)
			CUndo::Record(new CUndoBaseObjectMinimal(this, UndoDescription, flags));
		else
			CUndo::Record(new CUndoBaseObject(this, UndoDescription));
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsCreateGameObjects() const
{
	return GetObjectManager()->IsCreateGameObjects();
}

//////////////////////////////////////////////////////////////////////////
string CBaseObject::GetTypeName() const
{
	const char* className = m_classDesc->ClassName();
	const char* subClassName = strstr(className, "::");
	if (!subClassName)
		return className;

	string name;
	name.Append(className, subClassName - className);
	return name;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLayer(string layerFullName)
{
	auto pLayer = GetIEditor()->GetObjectManager()->GetIObjectLayerManager()->FindLayerByFullName(layerFullName);
	if (pLayer)
	{
		SetLayer(pLayer);
	}
}

void CBaseObject::SetLayer(IObjectLayer* pLayer)
{
	// TODO: set children's layer when added
	if (pLayer == m_layer && (m_children.empty() || m_linkedObjects.empty()))
		return;

	IObjectManager* iObjMng = GetIEditor()->GetObjectManager();

	bool bShouldUpdateLayerState = !iObjMng->IsLayerChanging();
	iObjMng->SetLayerChanging(true);

	// guard against same layer (we might have children!)
	if (pLayer != m_layer)
	{
		auto pOldLayer = m_layer;
		m_layer = pLayer;
		IObjectManager* iObjMng = GetIEditor()->GetObjectManager();
		if (CUndo::IsRecording())
		{
			CUndo::Record(new CUndoSetLayer(this, pOldLayer));
		}

		if (pLayer)
			pLayer->SetModified();
		if (pOldLayer)
			pOldLayer->SetModified();

		CObjectLayerChangeEvent changeEvt(this, pOldLayer);
		iObjMng->signalObjectChanged(changeEvt);

		if (pLayer)
			UpdateVisibility(pLayer->IsVisible());
	}

	// Set layer for all childs.
	for (int i = 0; i < m_children.size(); i++)
	{
		m_children[i]->SetLayer(pLayer);
	}

	// If object have target, target must also go into this layer.
	if (m_lookat)
	{
		m_lookat->SetLayer(pLayer);
	}

	if (bShouldUpdateLayerState)
		iObjMng->SetLayerChanging(false);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetDescendantsLayer(IObjectLayer* pLayer)
{
	SetLayer(pLayer);

	for (CBaseObject* pLinkedObj : m_linkedObjects)
		pLinkedObj->SetDescendantsLayer(pLayer);

	for (CBaseObject* pChild : m_children)
	{
		if (pChild->GetLayer() != pLayer)
			pChild->SetLayer(pLayer);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IntersectRectBounds(const AABB& bbox)
{
	AABB aabb;
	GetBoundBox(aabb);

	return aabb.IsIntersectBox(bbox);
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IntersectRayBounds(const Ray& ray)
{
	Vec3 tmpPnt;
	AABB aabb;
	GetBoundBox(aabb);

	return Intersect::Ray_AABB(ray, aabb, tmpPnt);
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::HitTestRectBounds(HitContext& hc, const AABB& box)
{
	using namespace BO_Private;

	if (hc.bUseSelectionHelpers)
	{
		if (IsSkipSelectionHelper())
			return false;
	}

	static const int kNumberOfBoundBoxPt(8);

	// transform all 8 vertices into view space.
	CPoint p[kNumberOfBoundBoxPt] =
	{
		hc.view->WorldToView(Vec3(box.min.x, box.min.y, box.min.z)),
		hc.view->WorldToView(Vec3(box.min.x, box.max.y, box.min.z)),
		hc.view->WorldToView(Vec3(box.max.x, box.min.y, box.min.z)),
		hc.view->WorldToView(Vec3(box.max.x, box.max.y, box.min.z)),
		hc.view->WorldToView(Vec3(box.min.x, box.min.y, box.max.z)),
		hc.view->WorldToView(Vec3(box.min.x, box.max.y, box.max.z)),
		hc.view->WorldToView(Vec3(box.max.x, box.min.y, box.max.z)),
		hc.view->WorldToView(Vec3(box.max.x, box.max.y, box.max.z))
	};

	CRect objrc, temprc;
	objrc.left = 10000;
	objrc.top = 10000;
	objrc.right = -10000;
	objrc.bottom = -10000;

	// find new min/max values
	for (int i = 0; i < 8; i++)
	{
		objrc.left = min(objrc.left, p[i].x);
		objrc.right = max(objrc.right, p[i].x);
		objrc.top = min(objrc.top, p[i].y);
		objrc.bottom = max(objrc.bottom, p[i].y);
	}
	if (objrc.IsRectEmpty())
	{
		// Make objrc at least of size 1.
		objrc.bottom += 1;
		objrc.right += 1;
	}

	if (hc.rect.PtInRect(CPoint(objrc.left, objrc.top)) &&
	    hc.rect.PtInRect(CPoint(objrc.left, objrc.bottom)) &&
	    hc.rect.PtInRect(CPoint(objrc.right, objrc.top)) &&
	    hc.rect.PtInRect(CPoint(objrc.right, objrc.bottom)))
	{
		hc.object = this;
		return true;
	}

	if (temprc.IntersectRect(objrc, hc.rect))
	{
		AABB localAABB;
		GetLocalBounds(localAABB);
		CBaseObject* pOldObj = hc.object;
		hc.object = this;
		if (localAABB.IsEmpty())
			return true;

		const int kMaxSizeOfEdgeList1(8);

		const Matrix34& worldTM(GetWorldTM());
		OBB obb = OBB::CreateOBBfromAABB(Matrix33(worldTM), localAABB);
		Vec3 ax = obb.m33.GetColumn0() * obb.h.x;
		Vec3 ay = obb.m33.GetColumn1() * obb.h.y;
		Vec3 az = obb.m33.GetColumn2() * obb.h.z;
		auto tr = worldTM * obb.c;
		CPoint obb_p[kMaxSizeOfEdgeList1] =
		{
			hc.view->WorldToView(-ax - ay - az + tr),
			hc.view->WorldToView(-ax - ay + az + tr),
			hc.view->WorldToView(-ax + ay - az + tr),
			hc.view->WorldToView(-ax + ay + az + tr),
			hc.view->WorldToView(ax - ay - az + tr),
			hc.view->WorldToView(ax - ay + az + tr),
			hc.view->WorldToView(ax + ay - az + tr),
			hc.view->WorldToView(ax + ay + az + tr)
		};

		int nEdgeList1Count(kMaxSizeOfEdgeList1);
		Edge2D edgelist1[kMaxSizeOfEdgeList1];
		if (!CalculateConvexEdgesForOBB(obb_p, kMaxSizeOfEdgeList1, edgelist1, nEdgeList1Count))
			return true;

		const int kMaxSizeOfEdgeList0(4);
		Edge2D edgelist0[kMaxSizeOfEdgeList0] = {
			Edge2D(Vec2(hc.rect.left,  hc.rect.top),    Vec2(hc.rect.right, hc.rect.top)),
			Edge2D(Vec2(hc.rect.right, hc.rect.top),    Vec2(hc.rect.right, hc.rect.bottom)),
			Edge2D(Vec2(hc.rect.right, hc.rect.bottom), Vec2(hc.rect.left,  hc.rect.bottom)),
			Edge2D(Vec2(hc.rect.left,  hc.rect.bottom), Vec2(hc.rect.left,  hc.rect.top))
		};

		ModifyConvexEdgeDirection(edgelist0, kMaxSizeOfEdgeList0);
		ModifyConvexEdgeDirection(edgelist1, nEdgeList1Count);

		bool bInside = IsIncludePointsInConvexHull(edgelist0, kMaxSizeOfEdgeList0, edgelist1, nEdgeList1Count);
		if (!bInside)
			bInside = IsIncludePointsInConvexHull(edgelist1, nEdgeList1Count, edgelist0, kMaxSizeOfEdgeList0);
		if (!bInside)
			bInside = DoesLineSegmentIntersectConvexHull(Vec2(hc.rect.left, hc.rect.top), Vec2(hc.rect.right, hc.rect.bottom), edgelist1, nEdgeList1Count);
		if (!bInside)
		{
			hc.object = pOldObj;
			return false;
		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::HitTestRect(HitContext& hc)
{
	AABB box;

	if (hc.bUseSelectionHelpers)
	{
		if (IsSkipSelectionHelper())
			return false;
		box.min = GetWorldPos();
		box.max = box.min;
	}
	else
	{
		// Retrieve world space bound box.
		GetBoundBox(box);
	}

	bool bHit = HitTestRectBounds(hc, box);
	m_bInSelectionBox = bHit;
	if (bHit)
		hc.object = this;
	return bHit;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::HitHelperTest(HitContext& hc)
{
	return HitHelperAtTest(hc, GetWorldPos());
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::HitHelperAtTest(HitContext& hc, const Vec3& pos)
{
	bool bResult = false;

	if (m_nTextureIcon && (gViewportPreferences.showIcons || gViewportPreferences.showSizeBasedIcons) && !hc.bUseSelectionHelpers)
	{
		int iconSize = OBJECT_TEXTURE_ICON_SIZE;

		if (gViewportPreferences.distanceScaleIcons)
		{
			iconSize *= OBJECT_TEXTURE_ICON_SCALE / hc.view->GetScreenScaleFactor(pos);
		}

		// Hit Test icon of this object.
		Vec3 testPos = pos;
		int y0 = -(iconSize / 2);
		int y1 = +(iconSize / 2);
		if (CheckFlags(OBJFLAG_SHOW_ICONONTOP))
		{
			Vec3 objectPos = GetWorldPos();

			AABB box;
			GetBoundBox(box);
			testPos.z = (pos.z - objectPos.z) + box.max.z;
			y0 = -iconSize;
			y1 = 0;
		}
		CPoint pnt = hc.view->WorldToView(testPos);

		if (hc.point2d.x >= pnt.x - (iconSize / 2) && hc.point2d.x <= pnt.x + (iconSize / 2) &&
		    hc.point2d.y >= pnt.y + y0 && hc.point2d.y <= pnt.y + y1)
		{
			hc.dist = hc.raySrc.GetDistance(testPos) - 0.2f;
			bResult = true;
		}
	}
	else if (hc.bUseSelectionHelpers)
	{
		// Check potentially children first
		bResult = HitHelperTestForChildObjects(hc);

		// If no hit check this object
		if (!bResult)
		{
			// Hit test helper.
			Vec3 w = pos - hc.raySrc;
			w = hc.rayDir.Cross(w);
			float d = w.GetLengthSquared();

			static const float screenScaleToRadiusFactor = 0.008f;
			const float radius = hc.view->GetScreenScaleFactor(pos) * screenScaleToRadiusFactor;
			const float pickDistance = hc.raySrc.GetDistance(pos);
			if (d < radius * radius + hc.distanceTolerance && hc.dist >= pickDistance)
			{
				hc.dist = pickDistance;
				hc.object = this;
				bResult = true;
			}
		}
	}

	return bResult;
}

CBaseObject* CBaseObject::GetLinkedObject(size_t i) const
{
	assert(i >= 0 && i < m_linkedObjects.size());
	return m_linkedObjects[i];
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CBaseObject::GetChild(size_t const i) const
{
	assert(i >= 0 && i < m_children.size());
	return m_children[i];
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsDescendantOf(const CBaseObject* pObject) const
{
	CBaseObject* pParentObject = m_parent;
	if (!pParentObject)
		pParentObject = m_pLinkedTo;

	while (pParentObject)
	{
		if (pParentObject == pObject)
			return true;

		if (pParentObject->GetParent())
			pParentObject = pParentObject->GetParent();
		else
			pParentObject = pParentObject->GetLinkedTo();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsChildOf(CBaseObject* node)
{
	CBaseObject* p = m_parent;
	while (p && p != node)
	{
		p = p->m_parent;
	}
	if (p == node)
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsLinkedDescendantOf(CBaseObject* pObject)
{
	CBaseObject* pLinkedTo = pObject->GetLinkedTo();
	while (pLinkedTo)
	{
		if (pLinkedTo == this)
			return true;

		pLinkedTo = pLinkedTo->GetLinkedTo();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::GetAllChildren(TBaseObjects& outAllChildren, CBaseObject* pObj) const
{
	const CBaseObject* pBaseObj = pObj ? pObj : this;

	for (int i = 0, iChildCount(pBaseObj->GetChildCount()); i < iChildCount; ++i)
	{
		CBaseObject* pChild = pBaseObj->GetChild(i);
		if (pChild == NULL)
			continue;
		outAllChildren.push_back(pChild);
		GetAllChildren(outAllChildren, pChild);
	}
}

void CBaseObject::GetAllChildren(DynArray<_smart_ptr<CBaseObject>>& outAllChildren, CBaseObject* pObj) const
{
	const CBaseObject* pBaseObj = pObj ? pObj : this;

	for (int i = 0, iChildCount(pBaseObj->GetChildCount()); i < iChildCount; ++i)
	{
		CBaseObject* pChild = pBaseObj->GetChild(i);
		if (pChild == NULL)
			continue;
		outAllChildren.push_back(pChild);
		GetAllChildren(outAllChildren, pChild);
	}
}

void CBaseObject::GetAllChildren(ISelectionGroup& outAllChildren, CBaseObject* pObj) const
{
	const CBaseObject* pBaseObj = pObj ? pObj : this;

	for (int i = 0, iChildCount(pBaseObj->GetChildCount()); i < iChildCount; ++i)
	{
		CBaseObject* pChild = pBaseObj->GetChild(i);
		if (pChild == NULL)
			continue;
		outAllChildren.AddObject(pChild);
		GetAllChildren(outAllChildren, pChild);
	}
}

void CBaseObject::GetAllPrefabFlagedChildren(std::vector<CBaseObject*>& outAllChildren, CBaseObject* pObj) const
{
	const CBaseObject* pBaseObj = pObj ? pObj : this;

	for (int i = 0, iChildCount(pBaseObj->GetChildCount()); i < iChildCount; ++i)
	{
		CBaseObject* pChild = pBaseObj->GetChild(i);
		if (pChild == NULL || !pChild->CheckFlags(OBJFLAG_PREFAB))
			continue;
		outAllChildren.push_back(pChild);

		if (GetIEditor()->IsCPrefabObject(pChild))
			continue;

		GetAllPrefabFlagedChildren(outAllChildren, pChild);
	}

	for (int i = 0, iLinkedCount(pBaseObj->GetLinkedObjectCount()); i < iLinkedCount; ++i)
	{
		CBaseObject* pLinkedObject = pBaseObj->GetLinkedObject(i);
		if (pLinkedObject == NULL || !pLinkedObject->CheckFlags(OBJFLAG_PREFAB))
			continue;
		outAllChildren.push_back(pLinkedObject);

		if (GetIEditor()->IsCPrefabObject(pLinkedObject))
			continue;

		GetAllPrefabFlagedChildren(outAllChildren, pLinkedObject);
	}
}

void CBaseObject::GetAllPrefabFlagedChildren(ISelectionGroup& outAllChildren, CBaseObject* pObj) const
{
	const CBaseObject* pBaseObj = pObj ? pObj : this;

	for (int i = 0, iChildCount(pBaseObj->GetChildCount()); i < iChildCount; ++i)
	{
		CBaseObject* pChild = pBaseObj->GetChild(i);
		if (pChild == NULL || !pChild->CheckFlags(OBJFLAG_PREFAB))
			continue;
		outAllChildren.AddObject(pChild);

		if (GetIEditor()->IsCPrefabObject(pChild))
			continue;

		GetAllPrefabFlagedChildren(outAllChildren, pChild);
	}

	for (int i = 0, iLinkedCount(pBaseObj->GetLinkedObjectCount()); i < iLinkedCount; ++i)
	{
		CBaseObject* pLinkedObject = pBaseObj->GetLinkedObject(i);
		if (pLinkedObject == NULL || !pLinkedObject->CheckFlags(OBJFLAG_PREFAB)) // check howw to set this flag, or is it needed
			continue;
		outAllChildren.AddObject(pLinkedObject);

		if (GetIEditor()->IsCPrefabObject(pLinkedObject))
			continue;

		GetAllPrefabFlagedChildren(outAllChildren, pLinkedObject);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::AttachChild(CBaseObject* pChild, bool shouldKeepPos, bool bInvalidateTM)
{
	std::vector<CBaseObject*> children = { pChild };
	AttachChildren(children, shouldKeepPos, bInvalidateTM);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::DetachThis(bool bKeepPos, bool bPlaceOnRoot)
{
	if (m_parent)
	{
		std::vector<CBaseObject*> children = { this };
		m_parent->DetachChildren(children, bKeepPos, bPlaceOnRoot);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::RemoveChild(CBaseObject* pChild)
{
	std::vector<CBaseObject*> children = { pChild };
	RemoveChildren(children);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::ResolveParent(CBaseObject* parent)
{
	// even though parent is same as m_parent, adding the member to the parent must be done.
	if (parent)
	{
		bool bDerivedFromGroup = GetIEditor()->IsCGroup(parent);
		bool bSuspended(false);
		if (bDerivedFromGroup)
		{
			bSuspended = GetIEditor()->SuspendUpdateCGroup(parent);
			parent->AddMember(this, false);
			if (bSuspended)
				GetIEditor()->ResumeUpdateCGroup(parent);
		}
		else // backwards compatibility for linking
			ResolveLinkedTo(parent);
	}
	else
	{
		if (GetGroup())
			GetIEditor()->RemoveMemberCGroup(GetGroup(), this);
	}
}

void CBaseObject::ResolveLinkedTo(CBaseObject* object)
{
	if (object) // Link
	{
		if (object == m_pLinkedTo)
			return;

		LinkTo(object, false);
	}
	else if (m_pLinkedTo) // Unlink
	{
		BaseObjectArray::iterator it = std::find(m_pLinkedTo->m_linkedObjects.begin(), m_pLinkedTo->m_linkedObjects.end(), this);
		if (it != m_pLinkedTo->m_linkedObjects.end())
			m_pLinkedTo->m_linkedObjects.erase(it);
		m_pLinkedTo = object;
		OnUnLink();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::CalcLocalTM(Matrix34& tm) const
{
	tm.SetIdentity();

	if (m_lookat)
	{
		auto pos = GetPos();

		// Check if our pos is relative to another object, either parent or linked
		CBaseObject* pRelativeTo = GetLinkedTo();
		if (!pRelativeTo)
			pRelativeTo = m_parent;

		// Get our world position.
		if (pRelativeTo)
			pos = pRelativeTo->GetWorldTM().TransformPoint(pos);

		// Calculate local TM matrix differently.
		Vec3 lookatPos = m_lookat->GetWorldPos();
		if (lookatPos == pos) // if target at object pos
		{
			tm.SetTranslation(pos);
		}
		else
		{
			tm = Matrix34(Matrix33::CreateRotationVDir((lookatPos - pos).GetNormalized()), pos);
		}

		if (pRelativeTo)
		{
			Matrix34 invParentTM = pRelativeTo->GetWorldTM();
			invParentTM.Invert();
			tm = invParentTM * tm;
		}
	}
	else
	{
		tm = Matrix34::Create(GetScale(), GetRotation(), GetPos());
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnChildModified()
{
	if (m_parent)
		m_parent->OnChildModified();

	if (m_pLinkedTo)
		m_pLinkedTo->OnChildModified();
};

//////////////////////////////////////////////////////////////////////////
Matrix34 CBaseObject::GetParentWorldTM() const
{
	if (m_parent)
	{
		return m_parent->GetWorldTM();
	}
	
	// If the object doesn't have a parent, use it's attachment world transform
	return GetLinkAttachPointWorldTM();
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CBaseObject::GetWorldTM() const
{
	if (!m_bMatrixValid)
	{
		m_worldTM = GetLocalTM();
		m_bMatrixValid = true;
		m_bMatrixInWorldSpace = false;
		m_bWorldBoxValid = false;
	}
	if (!m_bMatrixInWorldSpace)
	{
		// If linked to an object we need to get the attach point rather than the parent's world transform because
		// there are cases in which we're not linked to the parent directly but actually linked to one of it's bones
		if (m_pLinkedTo)
			m_worldTM = GetLinkAttachPointWorldTM() * m_worldTM;
		else if (m_parent)
			m_worldTM = m_parent->GetWorldTM() * m_worldTM;

		m_bMatrixInWorldSpace = true;
		m_bWorldBoxValid = false;
	}
	return m_worldTM;
}

bool CBaseObject::GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm) const
{
	if (coordSys == COORDS_LOCAL)
	{
		tm = GetWorldTM();
		return true;
	}
	else if (coordSys == COORDS_PARENT)
	{
		if (GetLinkedTo())
		{
			tm = GetLinkedTo()->GetWorldTM();
		}
		else if (GetParent())
		{
			tm = GetParent()->GetWorldTM();
		}
		else
		{
			return false;
		}
		return true;
	}
	else if (coordSys == COORDS_WORLD)
	{
		tm = GetWorldTM();
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
Matrix34 CBaseObject::GetLinkAttachPointWorldTM() const
{
	CBaseObject* pLinkedTo = GetLinkedTo();
	if (pLinkedTo)
	{
		return pLinkedTo->GetWorldTM();
	}

	return Matrix34(IDENTITY);
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsParentAttachmentValid() const
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::InvalidateTM(int flags)
{
	LOADING_TIME_PROFILE_SECTION

	bool bMatrixWasValid = m_bMatrixValid;

	m_bMatrixInWorldSpace = false;
	m_bMatrixValid = false;
	m_bWorldBoxValid = false;

	// If matrix was valid, invalidate all childs.
	if (bMatrixWasValid)
	{
		if (m_lookatSource)
			m_lookatSource->InvalidateTM(eObjectUpdateFlags_ParentChanged);

		// Invalidate matrices off all child objects.
		for (int i = 0; i < m_children.size(); i++)
		{
			if (m_children[i] != 0 && m_children[i]->m_bMatrixValid)
			{
				m_children[i]->InvalidateTM(eObjectUpdateFlags_ParentChanged);
			}
		}

		for (int i = 0; i < m_linkedObjects.size(); ++i)
		{
			if (m_linkedObjects[i] != 0 && m_linkedObjects[i]->m_bMatrixValid)
			{
				m_linkedObjects[i]->InvalidateTM(eObjectUpdateFlags_ParentChanged);
			}
		}

		NotifyListeners(OBJECT_ON_TRANSFORM);

		// Notify parent that we were modified.
		if (m_parent)
		{
			m_parent->OnChildModified();
		}

		if (m_pLinkedTo)
			m_pLinkedTo->OnChildModified();
	}

	if (m_pTransformDelegate)
	{
		m_pTransformDelegate->MatrixInvalidated();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLocalTM(const Vec3& pos, const Quat& rotate, const Vec3& scale, int flags)
{
	const bool b1 = SetPos(pos, flags | eObjectUpdateFlags_DoNotInvalidate);
	const bool b2 = SetRotation(rotate, flags | eObjectUpdateFlags_DoNotInvalidate);
	const bool b3 = SetScale(scale, flags | eObjectUpdateFlags_DoNotInvalidate);

	if (b1 || b2 || b3 || flags == eObjectUpdateFlags_Animated)
	{
		flags = (b1) ? (flags | eObjectUpdateFlags_PositionChanged) : (flags & (~eObjectUpdateFlags_PositionChanged));
		flags = (b2) ? (flags | eObjectUpdateFlags_RotationChanged) : (flags & (~eObjectUpdateFlags_RotationChanged));
		flags = (b3) ? (flags | eObjectUpdateFlags_ScaleChanged) : (flags & (~eObjectUpdateFlags_ScaleChanged));
		InvalidateTM(flags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLocalTM(const Matrix34& tm, int flags)
{
	if (m_lookat)
	{
		bool b1 = SetPos(tm.GetTranslation(), eObjectUpdateFlags_DoNotInvalidate);
		flags = (b1) ? (flags | eObjectUpdateFlags_PositionChanged) : (flags & (~eObjectUpdateFlags_PositionChanged));
		InvalidateTM(flags);
	}
	else
	{
		AffineParts affineParts;
		affineParts.SpectralDecompose(tm);
		SetLocalTM(affineParts.pos, affineParts.rot, affineParts.scale, flags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetWorldPos(const Vec3& pos, int flags)
{
	if (m_parent || m_pLinkedTo)
	{
		Matrix34 parentWorld = GetParentWorldTM();
		parentWorld.Invert();
		Vec3 posLocal = parentWorld * pos;
		SetPos(posLocal, flags);
	}
	else
	{
		SetPos(pos, flags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetWorldTM(const Matrix34& tm, int flags)
{
	if (m_parent || m_pLinkedTo)
	{
		Matrix34 parentWorld = GetParentWorldTM();
		parentWorld.Invert();
		Matrix34 localTM = parentWorld * tm;
		SetLocalTM(localTM, flags);
	}
	else
	{
		SetLocalTM(tm, flags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::UpdateVisibility(bool bVisible)
{
	bool bVisibleWithSpec = bVisible && !IsHiddenBySpec();
	if (bVisible == CheckFlags(OBJFLAG_INVISIBLE))
	{
		GetObjectManager()->InvalidateVisibleList();
		if (!bVisible)
			m_flags |= OBJFLAG_INVISIBLE;
		else
			m_flags &= ~OBJFLAG_INVISIBLE;

		NotifyListeners(OBJECT_ON_VISIBILITY);

		IPhysicalEntity* pPhEn = GetCollisionEntity();
		if (pPhEn && gEnv->pPhysicalWorld)
			gEnv->pPhysicalWorld->DestroyPhysicalEntity(pPhEn, bVisibleWithSpec ? 2 : 1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLookAt(CBaseObject* target)
{
	if (m_lookat == target)
		return;

	StoreUndo("Change LookAt");

	if (m_lookat)
	{
		// Unbind current lookat.
		m_lookat->m_lookatSource = 0;
	}
	m_lookat = target;
	if (m_lookat)
	{
		m_lookat->m_lookatSource = this;
	}

	InvalidateTM(0);
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsLookAtTarget() const
{
	return m_lookatSource != 0;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::AddEventListener(const EventCallback& cb)
{
	if (find(m_eventListeners.begin(), m_eventListeners.end(), cb) == m_eventListeners.end())
		m_eventListeners.push_back(cb);
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::RemoveEventListener(const EventCallback& cb)
{
	std::vector<EventCallback>::iterator cbFound = find(m_eventListeners.begin(), m_eventListeners.end(), cb);
	if (cbFound != m_eventListeners.end())
	{
		(*cbFound) = EventCallback();
	}
}

//////////////////////////////////////////////////////////////////////////
bool IsBaseObjectEventCallbackNULL(const CBaseObject::EventCallback& cb) { return cb.getFunc() == NULL; }

//////////////////////////////////////////////////////////////////////////
void CBaseObject::NotifyListeners(EObjectListenerEvent event)
{
	for (std::vector<EventCallback>::iterator it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it)
	{
		// Call listener callback.
		if ((*it).getFunc() != NULL)
			(*it)(this, event);
	}

	m_eventListeners.erase(remove_if(m_eventListeners.begin(), m_eventListeners.end(), IsBaseObjectEventCallbackNULL), std::end(m_eventListeners));
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::ConvertFromObject(CBaseObject* object)
{
	SetLocalTM(object->GetLocalTM());
	SetName(object->GetName());
	SetLayer(object->GetLayer());
	SetColor(object->GetColor());

	if (object->GetParent())
	{
		object->GetParent()->AttachChild(this);
	}
	SetMaterial(object->GetMaterial());
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsPotentiallyVisible() const
{
	if (!m_layer->IsVisible())
		return false;
	if (CheckFlags(OBJFLAG_HIDDEN))
		return false;
	if (gViewportDebugPreferences.GetObjectHideMask() & GetType())
		return false;

	CBaseObject* pGroup = GetGroup();
	if (pGroup)
	{
		if (!pGroup->IsPotentiallyVisible())
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
//! Analyze errors for this object.
void CBaseObject::Validate()
{
	// Not using acccessors(GetPos(), ... ) here as we want to validate the object position, scale and ignore delegates

	// Checks for invalid values in base object.

	//////////////////////////////////////////////////////////////////////////
	// Check if position is bad.
	auto pos = m_pos;

	if (fabs(pos.x) > INVALID_POSITION_EPSILON ||
	    fabs(pos.y) > INVALID_POSITION_EPSILON ||
	    fabs(pos.z) > INVALID_POSITION_EPSILON)
	{
		// File Not found.
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Object %s have invalid position (%f,%f,%f) %s", (const char*)GetName(), pos.x, pos.y, pos.z,
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
	}
	//////////////////////////////////////////////////////////////////////////

	float minScale = 0.01f;
	float maxScale = 1000.0f;
	//////////////////////////////////////////////////////////////////////////
	// Check if scale is out of range.
	if (m_scale.x < minScale || m_scale.x > maxScale ||
	    m_scale.y < minScale || m_scale.y > maxScale ||
	    m_scale.z < minScale || m_scale.z > maxScale)
	{
		// File Not found.
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Object %s have invalid scale (%f,%f,%f) %s", (const char*)GetName(), m_scale.x, m_scale.y, m_scale.z,
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
	} //////////////////////////////////////////////////////////////////////////

	if (GetMaterial() != NULL && GetMaterial()->IsDummy())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Material: %s for object: %s not found %s", GetMaterial()->GetName(), (const char*)GetName(),
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
	}

};

//////////////////////////////////////////////////////////////////////////
Ang3 CBaseObject::GetWorldAngles() const
{
	// Not using acccessors(GetScale()...) here as we want to validate the object scale and ignore delegates

	if (m_scale == Vec3(1, 1, 1))
	{
		Quat q = Quat(GetWorldTM());
		Ang3 angles = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(q)));
		return angles;
	}
	else
	{
		Matrix34 tm = GetWorldTM();
		tm.OrthonormalizeFast();
		Quat q = Quat(tm);
		Ang3 angles = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(q)));
		return angles;
	}
};

//////////////////////////////////////////////////////////////////////////
void CBaseObject::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	CBaseObject* pFromParent = pFromObject->GetParent();
	if (pFromParent)
	{
		CBaseObject* pFromParentInContext = ctx.FindClone(pFromParent);
		if (pFromParentInContext)
			pFromParentInContext->AddMember(this, false);
		else
			pFromParent->AddMember(this, false);
	}
	for (int i = 0; i < pFromObject->GetChildCount(); i++)
	{
		CBaseObject* pChildObject = pFromObject->GetChild(i);
		CBaseObject* pClonedChild = GetObjectManager()->CloneObject(pChildObject);
		ctx.AddClone(pChildObject, pClonedChild);
	}
	for (int i = 0; i < pFromObject->GetChildCount(); i++)
	{
		CBaseObject* pChildObject = pFromObject->GetChild(i);
		CBaseObject* pClonedChild = ctx.FindClone(pChildObject);
		if (pClonedChild)
		{
			pClonedChild->PostClone(pChildObject, ctx);
		}
	}

	CBaseObject* pLinkedTo = pFromObject->GetLinkedTo();
	if (pLinkedTo)
	{
		CBaseObject* pLinkedToInContext = ctx.FindClone(pLinkedTo);
		if (pLinkedToInContext)
			LinkTo(pLinkedToInContext);
		else
			LinkTo(pLinkedTo, false);
	}
	for (int i = 0; i < pFromObject->GetLinkedObjectCount(); ++i)
	{
		CBaseObject* pLinkedObject = pFromObject->GetLinkedObject(i);
		CBaseObject* pClonedLink = GetObjectManager()->CloneObject(pLinkedObject);
		pClonedLink->SetWorldTM(pLinkedObject->GetWorldTM());
		ctx.AddClone(pLinkedObject, pClonedLink);
	}
	for (int i = 0; i < pFromObject->GetLinkedObjectCount(); i++)
	{
		CBaseObject* pLinkedObject = pFromObject->GetLinkedObject(i);
		CBaseObject* pClonedLink = ctx.FindClone(pLinkedObject);
		if (pClonedLink)
		{
			pClonedLink->PostClone(pLinkedObject, ctx);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::GatherUsedResources(CUsedResources& resources)
{
	if (m_pVarObject != nullptr)
	{
		m_pVarObject->GatherUsedResources(resources);
	}

	if (m_pMaterial)
	{
		m_pMaterial->GatherUsedResources(resources);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IsSimilarObject(CBaseObject* pObject)
{
	if (pObject->GetClassDesc() == GetClassDesc() && pObject->GetRuntimeClass() == GetRuntimeClass())
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetMaterial(IEditorMaterial* mtl)
{
	if (m_pMaterial == mtl)
		return;

	StoreUndo("Assign Material");
	if (m_pMaterial)
	{
		m_pMaterial->Release();
	}
	m_pMaterial = mtl;
	if (m_pMaterial)
	{
		m_pMaterial->AddRef();
	}

	// Not sure if really needed, but add just in case
	UpdateUIVars();
}

//////////////////////////////////////////////////////////////////////////
string CBaseObject::GetMaterialName() const
{
	if (m_pMaterial)
		return m_pMaterial->GetName();
	return "";
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetMaterial(const string& materialName)
{
	IEditorMaterial* pMaterial = NULL;
	if (!materialName.IsEmpty())
		pMaterial = GetIEditor()->LoadMaterial(materialName);
	SetMaterial(pMaterial);
}

void CBaseObject::OnMtlResolved(uint32 id, bool success, const char* orgName, const char* newName)
{
	if (success)
	{
		string materialName = newName;
		IEditorMaterial* pMaterial = NULL;
		if (!materialName.IsEmpty())
			pMaterial = GetIEditor()->LoadMaterial(materialName);
		SetMaterial(pMaterial);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetLayerModified()
{
	if (m_layer)
		m_layer->SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetMinSpec(uint32 nSpec, bool bSetChildren)
{
	m_nMinSpec = nSpec;
	UpdateVisibility(!IsHidden());

	// Set min spec for all childs.
	if (bSetChildren)
	{
		for (int i = m_children.size() - 1; i >= 0; --i)
		{
			m_children[i]->SetMinSpec(nSpec, true);
		}

		for (auto pLinkedObject : m_linkedObjects)
			pLinkedObject->SetMinSpec(nSpec, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnPropertyChanged(IVariable*)
{
	SetLayerModified();
	UpdateGroup();
	UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnMultiSelPropertyChanged(IVariable*)
{
	const ISelectionGroup* grp = GetIEditor()->GetISelectionGroup();
	for (int i = 0; i < grp->GetCount(); i++)
	{
		grp->GetObject(i)->SetLayerModified();
		;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnMenuProperties()
{
	if (!IsSelected())
	{
		CUndo undo("Select Object");
		GetIEditor()->GetObjectManager()->ClearSelection();
		GetIEditor()->SelectObject(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::OnContextMenu(CPopupMenuItem* menu)
{
	if (!menu->Empty())
	{
		menu->AddSeparator();
	}

	// Group and prefab menu items
	menu->Add("Create Group", [=] {
		BeginUndoAndEnsureSelection();
		GetIEditor()->OnGroupMake();
		GetIEditor()->GetIUndoManager()->Accept("Create Group");
	});
	menu->Add("Create Prefab", [=] {
		BeginUndoAndEnsureSelection();
		GetIEditor()->OnPrefabMake();
		GetIEditor()->GetIUndoManager()->Accept("Create Prefab From Selection");
	});
	menu->Add("Add to...", [=]
	{
		BeginUndoAndEnsureSelection();
		GetIEditor()->GetIUndoManager()->Accept("Select Object");
		GetIEditor()->OnGroupAttach();
	});

	CPopupMenuItem& detachItem = menu->Add("Detach", [=] {
		BeginUndoAndEnsureSelection();
		GetIEditor()->OnGroupDetach();
		GetIEditor()->GetIUndoManager()->Accept("Detach From Group");
	});
	CPopupMenuItem& detachToRootItem = menu->Add("Detach from Hierarchy", [=] {
		BeginUndoAndEnsureSelection();
		GetIEditor()->OnGroupDetachToRoot();
		GetIEditor()->GetIUndoManager()->Accept("Detach from Hierarchy");
	});

	// Linking menu items
	menu->AddSeparator();

	menu->Add("Link to...", [=]
	{
		BeginUndoAndEnsureSelection();
		GetIEditor()->GetIUndoManager()->Accept("Select Object");
		GetIEditor()->OnLinkTo();
	});

	CPopupMenuItem& unlinkItem = menu->Add("Unlink", [=]
	{
		BeginUndoAndEnsureSelection();
		GetIEditor()->GetIUndoManager()->Accept("Select Object");
		GetIEditor()->OnUnLink();
	});

	CBaseObject* pGroup = GetGroup();
	bool bEnableDetach = pGroup && GetIEditor()->IsGroupOpen(pGroup);
	detachItem.Enable(bEnableDetach);
	detachToRootItem.Enable(bEnableDetach);
	unlinkItem.Enable(m_pLinkedTo != nullptr);

	// Other object menu items
	menu->AddSeparator();

	menu->Add("Delete", [ = ]
	{
		IObjectManager* manager = GetObjectManager();
		BeginUndoAndEnsureSelection();
		manager->DeleteSelection();
		GetIEditor()->GetIUndoManager()->Accept("Delete Selection");
	});

	CPopupMenuItem& transformMenu = menu->Add("Transform");
	transformMenu.Add("Clear Rotation", [ = ]
	{
		BeginUndoAndEnsureSelection();
		const ISelectionGroup* pGroup = GetIEditor()->GetISelectionGroup();
		Quat q(IDENTITY);
		for (int i = 0; i < pGroup->GetCount(); ++i)
		{
		  CBaseObject* pObj = pGroup->GetObject(i);
		  pObj->SetRotation(q);
		}
		GetIEditor()->GetIUndoManager()->Accept("Clear Rotation");
	});

	transformMenu.Add("Clear Scale", [ = ]
	{
		BeginUndoAndEnsureSelection();
		const ISelectionGroup* pGroup = GetIEditor()->GetISelectionGroup();
		Vec3 s(1.0f, 1.0f, 1.0f);
		for (int i = 0; i < pGroup->GetCount(); ++i)
		{
		  CBaseObject* pObj = pGroup->GetObject(i);
		  pObj->SetScale(s);
		}
		GetIEditor()->GetIUndoManager()->Accept("Clear Scale");
	});

	transformMenu.Add("Clear All", [ = ]
	{
		BeginUndoAndEnsureSelection();
		const ISelectionGroup* pGroup = GetIEditor()->GetISelectionGroup();
		Quat q(IDENTITY);
		Vec3 s(1.0f, 1.0f, 1.0f);
		for (int i = 0; i < pGroup->GetCount(); ++i)
		{
		  CBaseObject* pObj = pGroup->GetObject(i);
		  pObj->SetRotation(q);
		  pObj->SetScale(s);
		}
		GetIEditor()->GetIUndoManager()->Accept("Clear Rotation/Scale");
	});

	// Register converters
	bool bAddedSeparator = false;

	GetIEditor()->GetObjectManager()->IterateTypeConverters(GetRuntimeClass(), [menu, this, &bAddedSeparator](const char* szTargetName, std::function<void(CBaseObject* pObject)> conversionFunc)
	{
		if (!bAddedSeparator)
		{
			menu->AddSeparator();
			bAddedSeparator = true;
		}

		string sMenuTitle;
		sMenuTitle.Format("Convert To %s", szTargetName);

		menu->Add(sMenuTitle, [conversionFunc, this, sMenuTitle]
		{
			CUndo undo(sMenuTitle);
			conversionFunc(this);
		});
	});

	menu->AddSeparator();

	GetIEditor()->OnObjectContextMenuOpened(menu, this);

	string assetPath = GetAssetPath();
	CPopupMenuItem& showInAssetBrowser = menu->Add("Show in Asset Browser", [=]
	{
		string command("asset.show_in_browser ");
		command += assetPath;
		GetIEditor()->ExecuteCommand(command.c_str());
	});

	if (!GetIEditor()->GetAssetManager()->FindAssetForFile(assetPath))
		showInAssetBrowser.Enable(false);
}

//////////////////////////////////////////////////////////////////////////
bool CBaseObject::IntersectRayMesh(const Vec3& raySrc, const Vec3& rayDir, SRayHitInfo& outHitInfo) const
{
	const float fRenderMeshTestDistance = 0.2f;
	IRenderNode* pRenderNode = GetEngineNode();
	if (!pRenderNode)
		return false;

	Matrix34A worldTM;
	IStatObj* pStatObj = pRenderNode->GetEntityStatObj(0, 0, &worldTM);
	if (!pStatObj)
		return false;

	// transform decal into object space
	Matrix34A worldTM_Inverted = worldTM.GetInverted();
	Matrix33 worldRot(worldTM_Inverted);
	worldRot.Transpose();
	// put hit direction into the object space
	Vec3 vRayDir = rayDir.GetNormalized() * worldRot;
	// put hit position into the object space
	Vec3 vHitPos = worldTM_Inverted.TransformPoint(raySrc);
	Vec3 vLineP1 = vHitPos - vRayDir * fRenderMeshTestDistance;

	memset(&outHitInfo, 0, sizeof(outHitInfo));
	outHitInfo.inReferencePoint = vHitPos;
	outHitInfo.inRay.origin = vLineP1;
	outHitInfo.inRay.direction = vRayDir;
	outHitInfo.bInFirstHit = false;
	outHitInfo.bUseCache = false;

	return pStatObj->RayIntersection(outHitInfo, 0);
}

//////////////////////////////////////////////////////////////////////////
EScaleWarningLevel CBaseObject::GetScaleWarningLevel() const
{
	EScaleWarningLevel scaleWarningLevel = eScaleWarningLevel_None;

	const float kScaleThreshold = 0.001f;
	auto scale = GetScale();

	if (fabs(scale.x - 1.0f) > kScaleThreshold
	    || fabs(scale.y - 1.0f) > kScaleThreshold
	    || fabs(scale.z - 1.0f) > kScaleThreshold)
	{
		if (fabs(scale.x - scale.y) < kScaleThreshold
		    && fabs(scale.y - scale.z) < kScaleThreshold)
		{
			scaleWarningLevel = eScaleWarningLevel_Rescaled;
		}
		else
		{
			scaleWarningLevel = eScaleWarningLevel_RescaledNonUniform;
		}
	}

	return scaleWarningLevel;
}

//////////////////////////////////////////////////////////////////////////
ERotationWarningLevel CBaseObject::GetRotationWarningLevel() const
{
	ERotationWarningLevel rotationWarningLevel = eRotationWarningLevel_None;

	const float kRotationThreshold = 0.01f;

	Ang3 eulerRotation = Ang3(GetRotation());

	if (fabs(eulerRotation.x) > kRotationThreshold
	    || fabs(eulerRotation.y) > kRotationThreshold
	    || fabs(eulerRotation.z) > kRotationThreshold)
	{
		const float xRotationMod = fabs(fmod(eulerRotation.x, gf_PI / 2.0f));
		const float yRotationMod = fabs(fmod(eulerRotation.y, gf_PI / 2.0f));
		const float zRotationMod = fabs(fmod(eulerRotation.z, gf_PI / 2.0f));

		if ((xRotationMod < kRotationThreshold || xRotationMod > (gf_PI / 2.0f - kRotationThreshold))
		    && (yRotationMod < kRotationThreshold || yRotationMod > (gf_PI / 2.0f - kRotationThreshold))
		    && (zRotationMod < kRotationThreshold || zRotationMod > (gf_PI / 2.0f - kRotationThreshold)))
		{
			rotationWarningLevel = eRotationWarningLevel_Rotated;
		}
		else
		{
			rotationWarningLevel = eRotationWarningLevel_RotatedNonRectangular;
		}
	}

	return rotationWarningLevel;
}

bool CBaseObject::IsSkipSelectionHelper() const
{
	CEditTool* pEditTool(GetIEditor()->GetEditTool());
	if (pEditTool && pEditTool->IsNeedToSkipPivotBoxForObjects())
		return true;
	return false;
}

CBaseObject* CBaseObject::GetGroup() const
{
	// Make sure that if we return a group it's the actual owning group. If we were to just loop through
	// objects we're linked to and parents then we could potentially find a group that is not really the owning group.
	// The case for this happening is if we have an object that is linked to the group itself and not that it's linked
	// with a member of the group
	CBaseObject* pParentObject = GetParent();
	if (!pParentObject)
	{
		pParentObject = GetLinkedTo();
		if (pParentObject && GetIEditor()->IsCGroup(pParentObject))
		{
			if (pParentObject->GetParent())
				pParentObject = pParentObject->GetParent();
			else
				pParentObject = pParentObject->GetLinkedTo();
		}
	}

	while (pParentObject)
	{
		if (GetIEditor()->IsCGroup(pParentObject))
			return pParentObject;

		if (pParentObject->GetParent())
			pParentObject = pParentObject->GetParent();
		else
			pParentObject = pParentObject->GetLinkedTo();
	}

	return nullptr;
}

CBaseObject* CBaseObject::GetPrefab() const
{
	CBaseObject* pParentObject = GetParent();
	if (!pParentObject)
	{
		pParentObject = GetLinkedTo();
		if (pParentObject && GetIEditor()->IsCPrefabObject(pParentObject))
		{
			if (pParentObject->GetParent())
				pParentObject = pParentObject->GetParent();
			else
				pParentObject = pParentObject->GetLinkedTo();
		}
	}

	while (pParentObject)
	{
		if (GetIEditor()->IsCPrefabObject(pParentObject))
			return pParentObject;

		if (pParentObject->GetParent())
			pParentObject = pParentObject->GetParent();
		else
			pParentObject = pParentObject->GetLinkedTo();
	}

	return nullptr;
}

bool CBaseObject::IsPartOfPrefab() const
{
	return CheckFlags(OBJFLAG_PREFAB);
}

void CBaseObject::UpdateGroup()
{
	CBaseObject* pParentObject = GetParent();
	if (pParentObject && GetIEditor()->IsCGroup(pParentObject))
	{
		pParentObject->UpdateGroup();
	}
}

void CBaseObject::UpdatePrefab(EObjectChangedOpType updateReason)
{
	CBaseObject* pPrefab = GetPrefab();
	if (!pPrefab || m_bSuppressUpdatePrefab || GetIEditor()->IsModifyInProgressCPrefabObject(pPrefab))
		return;

	SObjectChangedContext context;
	context.m_modifiedObjectGlobalId = GetId();
	context.m_modifiedObjectGuidInPrefab = GetIdInPrefab();
	context.m_operation = updateReason;

	GetIEditor()->SyncPrefabCPrefabObject(pPrefab, context);
}

bool CBaseObject::CanBeHightlighted() const
{
	CBaseObject* pGroup = GetGroup();
	return !pGroup || GetIEditor()->IsGroupOpen(pGroup);
}

Matrix33 CBaseObject::GetWorldRotTM() const
{
	if (GetParent())
		return GetParent()->GetWorldRotTM() * Matrix33(GetRotation());
	return Matrix33(GetRotation());
}

void CBaseObject::BeginUndoAndEnsureSelection()
{
	IObjectManager* manager = GetObjectManager();

	GetIEditor()->GetIUndoManager()->Begin();
	if (!CheckFlags(OBJFLAG_SELECTED) || GetIEditor()->GetISelectionGroup()->GetCount() == 0)
	{
		// Select just this object, so we need to clear selection
		manager->ClearSelection();
		manager->SelectObject(this);
	}
}

Matrix33 CBaseObject::GetWorldScaleTM() const
{
	if (GetParent())
		return GetParent()->GetWorldScaleTM() * Matrix33::CreateScale(GetScale());
	return Matrix33::CreateScale(GetScale());
}

void CBaseObject::SetTransformDelegate(ITransformDelegate* pTransformDelegate, bool bInvalidateTM)
{
	LOADING_TIME_PROFILE_SECTION
	if(m_pTransformDelegate != pTransformDelegate)
	{
		m_pTransformDelegate = pTransformDelegate;
		if (bInvalidateTM)
		{
			InvalidateTM(0);
		}
	}
}

void CBaseObject::AddMember(CBaseObject* pMember, bool bKeepPos /*=true */)
{
	LOADING_TIME_PROFILE_SECTION;
	AttachChild(pMember, bKeepPos);
	if (CBaseObject* pPrefab = GetPrefab())
		pPrefab->AddMember(pMember, bKeepPos);
}

bool CBaseObject::CanLinkTo(CBaseObject* pLinkTo) const
{
	if (!pLinkTo)
		return false;

	// Cannot link to self
	if (this == pLinkTo)
		return false;

	if (GetGroup() != pLinkTo->GetGroup())
	{
		// TODO: Re-evaluate this case, we should be able to handle this now, but differs from regular linking because we would also need to:
		// 1.- Unlink if we're currently linked to something else
		// 2.- Detach from current group and attach to future group (need to switch layers if necessary)
		// 3.- Link to new target
		return false;
	}

	// Cannot link to a descendant
	if (pLinkTo->IsDescendantOf(this))
		return false;

	return true;
}

void CBaseObject::LinkTo(CBaseObject* pLinkTo, bool bKeepPos/* = true*/)
{
	if (pLinkTo->IsDescendantOf(this))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cannot link %s to a direct descendant", pLinkTo->GetName().c_str());
		return;
	}

	if (!pLinkTo)
		return;

	{
		CScopedSuspendUndo suspendUndo;

		// If we're attaching to an object within a prefab, make sure we're a part of the prefab first
		if (pLinkTo->GetPrefab() && !GetPrefab())
			pLinkTo->GetPrefab()->AddMember(this, true);

		// If not already attached to this node.
		if (m_pLinkedTo == pLinkTo)
			return;

		if (m_pLinkedTo)
			UnLink();
		if (m_parent)
			DetachThis(true, true);

		m_bSuppressUpdatePrefab = true;
		CObjectPreLinkEvent preLinkEvent(this, pLinkTo);
		GetObjectManager()->signalObjectChanged(preLinkEvent);
		NotifyListeners(OBJECT_ON_PRELINKED);
		const Matrix34& xform = GetWorldTM();

		pLinkTo->m_linkedObjects.push_back(this);
		m_pLinkedTo = pLinkTo;

		// Execute any special handling for linking. Used specifically in entity objects to
		// provide options for linking to any bones or attachments if available. This must always
		// execute before setting the object's new position, since the new position can be relative
		// to a bone and this transform is currently available only through the game-side entity
		OnLink(m_pLinkedTo);

		// Set the object's local transform relative to it's new parent
		if (bKeepPos)
		{
			Matrix34 invParentTM = GetLinkAttachPointWorldTM();
			invParentTM.Invert();
			Matrix34 localTM = invParentTM * xform;
			SetLocalTM(localTM);
		}
		else
		{
			InvalidateTM(eObjectUpdateFlags_ParentChanged);
		}

		GetObjectManager()->NotifyObjectListeners(this, OBJECT_ON_LINKED);
		NotifyListeners(OBJECT_ON_LINKED);

		m_bSuppressUpdatePrefab = false;
		UpdatePrefab(eOCOT_ModifyTransform);
	}

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoLinkBaseObject(this));
	}
}

// Unlink this object from parent
void CBaseObject::UnLink(bool placeOnRoot /*= false*/)
{
	if (m_pLinkedTo)
	{
		if (CUndo::IsRecording())
		{
			CUndo::Record(new CUndoLinkBaseObject(this, CUndoLinkBaseObject::ActionType::Unlink));
		}

		Matrix34 worldTM;
		ITransformDelegate* pTransformDelegate;
		CBaseObject* pGrandParent = m_pLinkedTo->GetGroup();

		{
			CScopedSuspendUndo suspendUndo;
			GetObjectManager()->NotifyObjectListeners(this, OBJECT_ON_PREUNLINKED);
			NotifyListeners(OBJECT_ON_PREUNLINKED);

			pTransformDelegate = m_pTransformDelegate;
			SetTransformDelegate(nullptr, true);

			ITransformDelegate* pParentTransformDelegate = m_pLinkedTo->m_pTransformDelegate;
			m_pLinkedTo->SetTransformDelegate(nullptr, true);
			worldTM = GetWorldTM();
			m_pLinkedTo->SetTransformDelegate(pParentTransformDelegate, true);
		}

		OnUnLink();

		{
			CScopedSuspendUndo suspendUndo;

			// Copy parent to temp var, erasing child from parent may delete this node if child referenced only from parent.
			CBaseObject* pLinkedTo = m_pLinkedTo;
			m_pLinkedTo = 0;

			BaseObjectArray::iterator it = std::find(pLinkedTo->m_linkedObjects.begin(), pLinkedTo->m_linkedObjects.end(), this);
			if (it != pLinkedTo->m_linkedObjects.end())
			{
				pLinkedTo->m_linkedObjects.erase(it);
			}

			// Keep old world space transformation.
			SetWorldTM(worldTM);

			SetTransformDelegate(pTransformDelegate, true);

			CObjectUnLinkEvent unLinkEvent(this, pLinkedTo);
			GetObjectManager()->signalObjectChanged(unLinkEvent);
			NotifyListeners(OBJECT_ON_UNLINKED);
		}

		// If the object we were linked to has a parent, then attach to it
		if (pGrandParent && !placeOnRoot)
			pGrandParent->AddMember(this, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::UnLinkAll()
{
	while (!m_linkedObjects.empty())
	{
		CBaseObject* pLinkedObj = *m_linkedObjects.begin();
		pLinkedObj->UnLink();
	}
}

bool CBaseObject::AreLinkedDescendantsInLayer(const IObjectLayer* pLayer) const
{
	if (m_layer != pLayer)
		return false;

	for (auto i = 0; i < m_linkedObjects.size(); ++i)
	{
		if (!m_linkedObjects[i]->AreLinkedDescendantsInLayer(pLayer))
			return false;
	}

	return true;
}

bool CBaseObject::IsLinkedAncestryInLayer(const IObjectLayer* pLayer) const
{
	if (m_layer != pLayer)
		return false;

	CBaseObject* pLinkedTo = m_pLinkedTo;
	while (pLinkedTo)
	{
		if (pLinkedTo->GetLayer() != pLayer)
			return false;

		pLinkedTo = pLinkedTo->GetLinkedTo();
	}

	return true;
}

bool CBaseObject::IsAnyLinkedAncestorInLayer(const IObjectLayer* pLayer) const
{
	if (m_layer == pLayer)
		return true;

	CBaseObject* pLinkedTo = m_pLinkedTo;
	while (pLinkedTo)
	{
		if (pLinkedTo->GetLayer() == pLayer)
			return true;

		pLinkedTo = pLinkedTo->GetLinkedTo();
	}

	return false;
}

void CBaseObject::OnBeforeAreaChange()
{
	AABB aabb;
	GetBoundBox(aabb);
	GetIEditor()->GetAIManager()->OnAreaModified(aabb, this);
}
//////////////////////////////////////////////////////////////////////////
void CBaseObject::SetMaterialByName(const char* mtlName)
{

}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	creator.AddPropertyTree<CBaseObject>("General", [](CBaseObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->SerializeGeneralProperties(ar, bMultiEdit);
	});
	creator.AddPropertyTree<CBaseObject>("Transform", [](CBaseObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->SerializeTransformProperties(ar);
	});
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SerializeGeneralProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	string typeDescr = GetTypeDescription();
	ar(typeDescr, "type_name", "!Type");
	string name = (const char*)m_name;
	ar(name, "name", "Name");

	int abgr = m_color;
	ColorB color = ColorB((uint32)abgr);
	ar(color, "color", "Color");

	string layer = (const char*)m_layer->GetFullName();
	ar(Serialization::LevelLayerPicker(layer), "layer", "Layer");

	string mtl = GetMaterialName();
	bool bHadCustomMaterial = mtl.size() > 0;
	if (!bHadCustomMaterial)
	{
		if (IEditorMaterial* pMaterial = GetRenderMaterial())
		{
			mtl = pMaterial->GetName();
		}
	}

	ar(Serialization::MaterialPicker(mtl), "mtl", "Material");
	// Special case: Clear the custom material if it is identical to the default render one
	if (!bHadCustomMaterial)
	{
		if (IEditorMaterial* pMaterial = GetRenderMaterial())
		{
			if (mtl == pMaterial->GetName())
			{
				mtl.clear();
			}
		}
	}

	ESystemConfigSpec minSpec = (ESystemConfigSpec)m_nMinSpec;
	ar(minSpec, "MinSpec", "Minimum Graphics");

	if (ar.isInput())
	{
		bool objectChanged = false;
		abgr = color.pack_abgr8888();
		if (abgr != m_color)
		{
			objectChanged = true;
			SetColor(abgr);
		}

		if (!name.empty() && strcmp(name.c_str(), (LPCSTR)m_name) != 0)
		{
			// This may also change object id.
			GetObjectManager()->ChangeObjectName(this, name.c_str(), bMultiEdit); // Force a unique name if doing a multi-select change
			objectChanged = true;
		}

		if (layer != m_layer->GetFullName())
		{
			SetLayer(layer);
			objectChanged = true;
		}

		if (m_pMaterial)
		{
			if (mtl.empty())
			{
				SetMaterial(0);
				objectChanged = true;
			}
			else if (strcmp(mtl.c_str(), (LPCSTR)m_pMaterial->GetName()) != 0)
			{
				SetMaterial(mtl.c_str());
				objectChanged = true;
			}
		}
		else if (!mtl.empty())
		{
			SetMaterial(mtl.c_str());
			objectChanged = true;
		}

		if (minSpec != GetMinSpec())
		{
			StoreUndo("Change MinSpec", true);
			SetMinSpec(minSpec);
			objectChanged = true;
		}

		// if we had a change make sure to propagate it to the prefab
		if (objectChanged)
		{
			UpdatePrefab();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseObject::SerializeTransformProperties(Serialization::IArchive& ar)
{
	using namespace BO_Private;

	// Using delegate here as we want the properties to display and edit the delegate transform
	Vec3 pos = GetPos();
	Quat rot = GetRotation();
	Vec3 scl = GetScale();

	ar(Serialization::SPosition(pos), "xform.pos", "Position");
	ar(Serialization::SRotation(rot), "xform.rot", "Rotation");
	ar(Serialization::SUniformScale(scl), "xform.scl", "Scale");
	if (ar.isInput())
	{
		SetPos(pos);

		// If rotation changed, then set the new rotation. If there's a transform delegate, let it handle 
		// rotation if necessary
		if (m_pTransformDelegate || !Quat::IsEquivalent(m_rotate, rot, 0.0001f))
			SetRotation(rot);

		SetScale(scl);
	}
}

void CBaseObject::RegisterOnEngine()
{
	IRenderNode* pRenderNode = GetEngineNode();
	if (pRenderNode && pRenderNode->GetEntityStatObj())
	{
		GetIEditor()->RegisterEntity(pRenderNode);
	}
}

void CBaseObject::UnRegisterFromEngine()
{
	IRenderNode* pRenderNode = GetEngineNode();
	if (pRenderNode && pRenderNode->GetEntityStatObj())
	{
		GetIEditor()->UnRegisterEntityAsJob(pRenderNode);
	}
}

