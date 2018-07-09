// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectionGroup.h"
#include "Grid.h"
#include "Viewport.h"
#include "Objects/BaseObject.h"
#include "Objects/DisplayContext.h"
#include "GeomEntity.h"
#include "PrefabObject.h"
#include "BrushObject.h"
#include "EntityObject.h"
#include "SurfaceInfoPicker.h"
#include "Prefabs/PrefabManager.h"

//////////////////////////////////////////////////////////////////////////
CSelectionGroup::CSelectionGroup()
{

}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Reserve(size_t count)
{
	m_objects.reserve(count);
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::AddObject(CBaseObject* obj)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (!IsContainObject(obj))
	{
		m_objects.push_back(obj);
		m_objectsSet.insert(obj);
		m_filtered.clear();
		m_initElementTransforms.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::RemoveObject(CBaseObject* obj)
{
	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		if (*it == obj)
		{
			m_objects.erase(it);
			m_objectsSet.erase(obj);
			m_filtered.clear();
			m_initElementTransforms.clear();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::RemoveAll()
{
	m_objects.clear();
	m_objectsSet.clear();
	m_filtered.clear();
	//This should notify
}

bool CSelectionGroup::IsContainObject(CBaseObject* obj) const
{
	return (m_objectsSet.find(obj) != m_objectsSet.end());
}

//////////////////////////////////////////////////////////////////////////
bool CSelectionGroup::IsEmpty() const
{
	return m_objects.empty();
}

//////////////////////////////////////////////////////////////////////////
int CSelectionGroup::GetCount() const
{
	return m_objects.size();
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CSelectionGroup::GetObject(int index) const
{
	ASSERT(index >= 0 && index < m_objects.size());
	return m_objects[index];
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CSelectionGroup::GetObjectByGuid(CryGUID guid) const
{
	for (size_t i = 0, count(m_objects.size()); i < count; ++i)
	{
		if (m_objects[i]->GetId() == guid)
		{
			return m_objects[i];
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CSelectionGroup::GetObjectByGuidInPrefab(CryGUID guid) const
{
	std::queue<CBaseObject*> groups;
	for (size_t i = 0, count(m_objects.size()); i < count; ++i)
	{
		//Recurse through the groups
		if (m_objects[i]->IsKindOf(RUNTIME_CLASS(CGroup)))
		{
			groups.push(m_objects[i]);
			while (!groups.empty())
			{
				CBaseObject* pGroup = groups.front();
				if (pGroup->GetIdInPrefab() == guid)
					return pGroup;

				groups.pop();

				for (int j = 0, childCount = pGroup->GetChildCount(); j < childCount; ++j)
				{
					CBaseObject* pChild = pGroup->GetChild(j);

					if (pChild->GetIdInPrefab() == guid)
						return pChild;
					else if (pChild->IsKindOf(RUNTIME_CLASS(CGroup)))
						groups.push(pChild);
				}
			}
		}
		else if (m_objects[i]->GetIdInPrefab() == guid)
		{
			return m_objects[i];
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Copy(const CSelectionGroup& from)
{
	m_name = from.m_name;
	m_objects = from.m_objects;
	m_objectsSet = from.m_objectsSet;
	m_filtered = from.m_filtered;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CSelectionGroup::GetCenter() const
{
	Vec3 c(0, 0, 0);
	for (int i = 0; i < GetCount(); i++)
	{
		c += GetObject(i)->GetWorldPos();
	}
	if (GetCount() > 0)
		c /= GetCount();
	return c;
}

bool CSelectionGroup::GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm) const
{
	int numObjects = GetCount();

	if (numObjects > 0)
	{
		CBaseObject* pObj = GetObject(numObjects - 1);
		if (coordSys == COORDS_LOCAL || coordSys == COORDS_PARENT)
		{
			// attempt to get local or parent matrix from gizmo owner. If none is provided, just use world matrix instead
			if (!pObj->GetManipulatorMatrix(coordSys, tm))
			{
				tm.SetIdentity();
			}
		}
		else if (coordSys == COORDS_WORLD)
		{
			tm.SetIdentity();
		}

		tm.SetTranslation(GetCenter());

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
AABB CSelectionGroup::GetBounds() const
{
	AABB b;
	AABB box;
	box.Reset();
	for (int i = 0; i < GetCount(); i++)
	{
		GetObject(i)->GetBoundBox(b);
		box.Add(b.min);
		box.Add(b.max);
	}
	return box;
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::FilterParents() const
{
	if (!m_filtered.empty())
		return;

	GetIEditorImpl()->GetObjectManager()->FilterParents(m_objects, m_filtered);
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::FilterLinkedObjects() const
{
	if (!m_filtered.empty())
		return;

	GetIEditorImpl()->GetObjectManager()->FilterLinkedObjects(m_objects, m_filtered);
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Move(const Vec3& offset, int moveFlags, int referenceCoordSys, const CPoint& point, bool bFromInitPos) const
{
	// [MichaelS - 17/3/2005] Removed this code from the three edit functions (move,
	// rotate and scale). This was causing a bug where the render node of objects
	// was not being updated when objects were dragged away from their position
	// and then back again, since movement is re-calculated from the initial position
	// each mouse message (ie first the previous movement is undone and then the
	// movement is applied). This meant that when moving back to the start position
	// it appeared like no movement was applied, although it was still necessary to
	// update the graphics resources. The object transform is explicitly reset
	// below.

	//if (offset.x == 0 && offset.y == 0 && offset.z == 0)
	//	return;

	FilterParents();
	Vec3 newPos;

	bool bValidFollowGeometryMode(true);
	if (point.x == -1 || point.y == -1)
		bValidFollowGeometryMode = false;

	SRayHitInfo pickedInfo;
	CSurfaceInfoPicker::CExcludedObjects excludeObjects;

	if ((moveFlags & eMS_FollowGeometry) && bValidFollowGeometryMode)
	{
		for (int i = 0; i < GetFilteredCount(); ++i)
			excludeObjects.Add(GetFilteredObject(i));
	}

	for (int i = 0; i < GetFilteredCount(); i++)
	{
		CBaseObject* obj = GetFilteredObject(i);
		Vec3 wp;

		if (bFromInitPos)
		{
			if (m_initElementTransforms.empty())
			{
				CRY_ASSERT_MESSAGE(0, "Initial position expected from transform but not provided. Use SaveFilteredPositions before moving");
				// use incorrect fallback
				Matrix34 wtm = obj->GetWorldTM();
				wp = wtm.GetTranslation();
			}
			else
			{
				wp = m_initElementTransforms[i].position;
			}
		}
		else
		{
			Matrix34 wtm = obj->GetWorldTM();
			wp = wtm.GetTranslation();
		}

		newPos = wp + offset;

		if ((moveFlags & eMS_FollowGeometry) && bValidFollowGeometryMode)
		{
			if (GetIEditorImpl()->GetActiveView()->GetAxisConstrain() == AXIS_Z)
			{
				continue;
			}

			CPoint screenSpacePos = GetIEditorImpl()->GetActiveView()->WorldToView(newPos);

			CSurfaceInfoPicker surfacePicker;
			bValidFollowGeometryMode = surfacePicker.Pick(screenSpacePos, pickedInfo, &excludeObjects);

			obj->SetWorldPos(pickedInfo.vHitPos);

			if (moveFlags & eMS_SnapToNormal)
			{
				Quat rotation = obj->GetRotation();
				Vec3 zaxis = rotation * Vec3(0, 0, 1);
				zaxis.Normalize();
				Quat nq;
				nq.SetRotationV0V1(zaxis, pickedInfo.vHitNormal);
				obj->SetRotation(nq * rotation);
			}
			continue;
		}

		if (moveFlags & eMS_FollowTerrain)
		{
			if (GetIEditorImpl()->GetActiveView()->GetAxisConstrain() != AXIS_Z)
			{
				// Make sure object keeps it height.
				float height = wp.z - GetIEditorImpl()->GetTerrainElevation(wp.x, wp.y);
				newPos.z = GetIEditorImpl()->GetTerrainElevation(newPos.x, newPos.y) + height;

				if (moveFlags & eMS_SnapToNormal)
				{
					Quat rotation = obj->GetRotation();
					Vec3 zaxis = rotation * Vec3(0, 0, 1);
					zaxis.Normalize();
					Quat nq;
					nq.SetRotationV0V1(zaxis, GetIEditorImpl()->GetTerrainNormal(newPos));
					obj->SetRotation(nq * rotation);
				}
			}
		}

		obj->SetWorldPos(newPos, eObjectUpdateFlags_UserInput | eObjectUpdateFlags_PositionChanged | eObjectUpdateFlags_MoveTool);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::MoveTo(const Vec3& pos, EMoveSelectionFlag moveFlag, int referenceCoordSys, const CPoint& point) const
{
	FilterParents();
	if (GetFilteredCount() < 1)
		return;

	CBaseObject* refObj = GetFilteredObject(0);
	CSelectionGroup::Move(pos - refObj->GetWorldTM().GetTranslation(), moveFlag, referenceCoordSys, point);
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Rotate(const Quat& qRot, int referenceCoordSys) const
{
	Matrix34 rotateTM = Matrix33(qRot);

	Rotate(rotateTM, referenceCoordSys);
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Rotate(const Ang3& angles, int referenceCoordSys) const
{
	//if (angles.x == 0 && angles.y == 0 && angles.z == 0)
	//	return;

	Matrix34 rotateTM = Matrix34::CreateRotationXYZ(DEG2RAD(-angles)); //NOTE: angles in radians and negated

	Rotate(rotateTM, referenceCoordSys);
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Rotate(const Matrix34& rotateTM, int referenceCoordSys) const
{
	// Rotate selection about selection center.
	Vec3 center = GetCenter();

	Matrix34 ToOrigin;
	Matrix34 FromOrigin;

	ToOrigin.SetIdentity();
	FromOrigin.SetIdentity();

	if (referenceCoordSys != COORDS_LOCAL)
	{
		ToOrigin.SetTranslation(-center);
		FromOrigin.SetTranslation(center);

		if (referenceCoordSys == COORDS_USERDEFINED)
		{
			Matrix34 userTM = gSnappingPreferences.GetMatrix();
			Matrix34 invUserTM = userTM.GetInvertedFast();

			ToOrigin = invUserTM * ToOrigin;
			FromOrigin = FromOrigin * userTM;
		}
	}

	FilterParents();

	for (int i = 0; i < GetFilteredCount(); i++)
	{
		CBaseObject* obj = GetFilteredObject(i);

		Matrix34 m = obj->GetWorldTM();

		auto axisConstraints = GetIEditor()->GetAxisConstrains();
		// Rotation around view axis should be the same regardless of space (local, parent, world)
		if (axisConstraints == AXIS_VIEW || axisConstraints == AXIS_XYZ)
		{
			// Rotate objects in view space using each object's separate world position as the pivot
			if (referenceCoordSys == COORDS_LOCAL)
			{
				CBaseObject* obj = GetFilteredObject(i);
				Matrix34 m = obj->GetWorldTM();

				// Reset matrices to be identity
				ToOrigin.SetIdentity();
				FromOrigin.SetIdentity();

				// These matrices should be based off the object's world position rather than the center of selection
				Vec3 objWorld = obj->GetWorldPos();
				ToOrigin.SetTranslation(-objWorld);
				FromOrigin.SetTranslation(objWorld);

				m = FromOrigin * rotateTM * ToOrigin * m;
				obj->SetWorldTM(m, eObjectUpdateFlags_UserInput);
			}
			else
			{
				// Rotate in view space using the center of the selection as the pivot
				m = FromOrigin * rotateTM * ToOrigin * m;
				obj->SetWorldTM(m, eObjectUpdateFlags_UserInput);
			}
		}
		else if (referenceCoordSys != COORDS_LOCAL)
		{
			if (referenceCoordSys == COORDS_PARENT && obj->GetParent())
			{
				Matrix34 parentTM = obj->GetParent()->GetWorldTM();
				parentTM.OrthonormalizeFast();
				parentTM.SetTranslation(Vec3(0, 0, 0));
				Matrix34 invParentTM = parentTM.GetInvertedFast();

				m = FromOrigin * parentTM * rotateTM * invParentTM * ToOrigin * m;
			}
			else
			{
				m = FromOrigin * rotateTM * ToOrigin * m;
			}

			obj->SetWorldTM(m, eObjectUpdateFlags_UserInput);
		}
		else
		{
			obj->SetRotation(obj->GetRotation() * Quat(rotateTM), eObjectUpdateFlags_UserInput);
		}

		obj->InvalidateTM(eObjectUpdateFlags_UserInput);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Scale(const Vec3& scaleArg, int referenceCoordSys) const
{
	Vec3 scl = scaleArg;
	if (scl.x == 0) scl.x = 0.01f;
	if (scl.y == 0) scl.y = 0.01f;
	if (scl.z == 0) scl.z = 0.01f;

	FilterParents();

	for (int i = 0; i < GetFilteredCount(); i++)
	{
		int objCoordSystem = referenceCoordSys;

		CBaseObject* const pObj = GetFilteredObject(i);
		Vec3 scale = m_initElementTransforms[i].scale;

		if (objCoordSystem == COORDS_PARENT && pObj->GetParent() == nullptr)
		{
			objCoordSystem = COORDS_LOCAL;
		}

		if (objCoordSystem == COORDS_LOCAL)
		{
			scale = scale.CompMul(scl);
		}
		else if (objCoordSystem == COORDS_WORLD || objCoordSystem == COORDS_PARENT || objCoordSystem == COORDS_VIEW)
		{
			Matrix33 mFromWorld;

			// scale is not in local space, so we need to project the scale vector to the local space of the object
			if (objCoordSystem == COORDS_PARENT)
			{
				mFromWorld = Matrix33(m_initElementTransforms[i].localRotation.GetInverted());
			}	
			else
			{
				mFromWorld = m_initElementTransforms[i].worldRotation.GetTransposed();
			}

			// Since the scale is always in the local space, we can not scale in directions that are not parallel to the local coordinate axes.
			// Therefore we snap the mFromWorld orientation to the nearest 90 degrees in the local space.
			mFromWorld = SnapToNearest(mFromWorld, gf_PI * 0.5f);

			// all components of the matrix should be positive since we are only interested in positive contributions to the object scale
			mFromWorld.Fabs();

			// by transforming the difference to the new space instead of the original vector, we
			// can avoid scale getting zeroed-out in perpendicular to the original vector directions
			const Vec3 scaleDiff = scl - Vec3(1.0f, 1.0f, 1.0f);
			scale = scale.CompMul(Vec3(1.0f, 1.0f, 1.0f) + mFromWorld * scaleDiff);
		}

		pObj->SetScale(scale, eObjectUpdateFlags_UserInput | eObjectUpdateFlags_ScaleTool);
	}
}

void CSelectionGroup::SetScale(const Vec3& scale, int referenceCoordSys) const
{
	Vec3 relScale = scale;

	if (GetCount() > 0 && GetObject(0))
	{
		Vec3 objScale = GetObject(0)->GetScale();
		if (relScale == objScale && (objScale.x == 0.0f || objScale.y == 0.0f || objScale.z == 0.0f))
			return;
		relScale = relScale / objScale;
	}

	Scale(relScale, referenceCoordSys);
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Align() const
{
	for (int i = 0; i < GetFilteredCount(); ++i)
	{
		bool terrain = false;
		CBaseObject* obj = GetFilteredObject(i);
		Vec3 pos = obj->GetPos();
		Quat rot = obj->GetRotation();
		CPoint point = GetIEditorImpl()->GetActiveView()->WorldToView(pos);
		Vec3 normal = GetIEditorImpl()->GetActiveView()->ViewToWorldNormal(point, false, true);
		pos = GetIEditorImpl()->GetActiveView()->ViewToWorld(point, &terrain, false, false, true);
		Vec3 zaxis = rot * Vec3(0, 0, 1);
		normal.Normalize();
		zaxis.Normalize();
		Quat nq;
		nq.SetRotationV0V1(zaxis, normal);
		obj->SetRotation(nq * rot);
		obj->SetPos(pos);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::Transform(const Vec3& offset, EMoveSelectionFlag moveFlag, const Ang3& angles, const Vec3& scale, int referenceCoordSys) const
{
	if (offset != Vec3(0))
		Move(offset, moveFlag, referenceCoordSys);

	if (!(angles == Ang3(ZERO)))
		Rotate(angles, referenceCoordSys);

	if (scale != Vec3(0))
		Scale(scale, referenceCoordSys);
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::ResetTransformation() const
{
	FilterParents();
	Quat qIdentity;
	qIdentity.SetIdentity();
	Vec3 vScale(1.0f, 1.0f, 1.0f);

	for (int i = 0, n = GetFilteredCount(); i < n; ++i)
	{
		CBaseObject* pObj = GetFilteredObject(i);
		pObj->SetRotation(qIdentity);
		pObj->SetScale(vScale);
	}
}

//////////////////////////////////////////////////////////////////////////
static void RecursiveFlattenHierarchy(CBaseObject* pObj, CSelectionGroup& newGroup, bool flattenGroups)
{
	if (!pObj->IsKindOf(RUNTIME_CLASS(CGroup)))
	{
		for (int i = 0; i < pObj->GetChildCount(); i++)
		{
			CBaseObject* pChild = pObj->GetChild(i);
			newGroup.AddObject(pChild);
			RecursiveFlattenHierarchy(pChild, newGroup, flattenGroups);
		}
		for (int i = 0; i < pObj->GetLinkedObjectCount(); i++)
		{
			RecursiveFlattenHierarchy(pObj->GetLinkedObject(i), newGroup, flattenGroups);
		}
	}
	else if (flattenGroups)
	{
		const TBaseObjects& groupMembers = static_cast<CGroup*>(pObj)->GetMembers();
		for (int i = 0, count = groupMembers.size(); i < count; ++i)
		{
			newGroup.AddObject(groupMembers[i]);
			RecursiveFlattenHierarchy(groupMembers[i], newGroup, flattenGroups);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::FlattenHierarchy(CSelectionGroup& newGroup, bool flattenGroups)
{
	for (int i = 0; i < GetCount(); i++)
	{
		CBaseObject* pObj = GetObject(i);
		newGroup.AddObject(pObj);
		RecursiveFlattenHierarchy(pObj, newGroup, flattenGroups);
	}
}

//////////////////////////////////////////////////////////////////////////
class CAttachToGroup : public IPickObjectCallback
{
public:
	//! Called when object picked.
	virtual void OnPick(CBaseObject* picked)
	{
		assert(picked->GetType() == OBJTYPE_PREFAB || picked->GetType() == OBJTYPE_GROUP);

		IObjectManager* objManager = GetIEditorImpl()->GetObjectManager();

		if (picked->GetType() == OBJTYPE_GROUP)
		{
			CUndo undo("Attach to Group");
			CGroup* gpicked = static_cast<CGroup*>(picked);
			const CSelectionGroup* selGroup = GetIEditorImpl()->GetSelection();
			selGroup->FilterParents();
			std::vector<CBaseObject*> filteredObjects;
			filteredObjects.reserve(selGroup->GetFilteredCount());

			// Save filtered list locally because we will be doing selections, which will invalidate the filtered group
			for (int i = 0; i < selGroup->GetFilteredCount(); i++)
				filteredObjects.push_back(selGroup->GetFilteredObject(i));

			for (CBaseObject* selectedObj : filteredObjects)
			{
				if (ChildIsValid(picked, selectedObj))
				{
					gpicked->AddMember(selectedObj);

					// If this is not an open group we are adding to, we must also update the selection
					if (!gpicked->IsOpen())
						objManager->UnselectObject(selectedObj);
				}
			}

			if (!gpicked->IsOpen() && !gpicked->IsSelected())
				objManager->SelectObject(gpicked);
		}
		else if (picked->GetType() == OBJTYPE_PREFAB)
		{
			CUndo undo("Attach to Prefab");

			CPrefabObject* gpicked = static_cast<CPrefabObject*>(picked);
			GetIEditor()->GetPrefabManager()->AddSelectionToPrefab(gpicked);
		}

		delete this;
	}
	//! Called when pick mode cancelled.
	virtual void OnCancelPick()
	{
		delete this;
	}
	//! Return true if specified object is pickable.
	virtual bool OnPickFilter(CBaseObject* filterObject)
	{
		if (filterObject->GetType() == OBJTYPE_PREFAB || filterObject->GetType() == OBJTYPE_GROUP)
			return true;
		else
			return false;
	}
private:
	bool ChildIsValid(CBaseObject* pParent, CBaseObject* pChild, int nDir = 3)
	{
		if (pParent == pChild)
			return false;

		CBaseObject* pObj;
		if (nDir & 1)
		{
			if (pObj = pChild->GetParent())
			{
				if (!ChildIsValid(pParent, pObj, 1))
				{
					return false;
				}
			}
		}
		if (nDir & 2)
		{
			for (int i = 0; i < pChild->GetChildCount(); i++)
			{
				if (pObj = pChild->GetChild(i))
				{
					if (!ChildIsValid(pParent, pObj, 2))
					{
						return false;
					}
				}
			}
		}
		return true;
	}
};

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::AttachToGroup() const
{
	CAttachToGroup* pCallback = new CAttachToGroup;
	GetIEditorImpl()->PickObject(pCallback);
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::SendEvent(ObjectEvent event) const
{
	for (int i = 0; i < m_objects.size(); i++)
	{
		CBaseObject* obj = m_objects[i];
		obj->OnEvent(event);
	}
}

void CSelectionGroup::SaveFilteredTransform() const
{
	size_t nfiltered = m_filtered.size();
	if (nfiltered == 0)
	{
		return;
	}

	m_initElementTransforms.reserve(m_filtered.size());

	for (size_t i = 0; i < nfiltered; ++i)
	{
		// Here copied what was there in Move function instead of GetPos due to high risk time for the fix.
		// TODO: explore if SetPos works correctly even with object hierarchies
		STransformElementInit elem;
		elem.position      = m_filtered[i]->GetWorldPos();
		elem.scale         = m_filtered[i]->GetScale();
		elem.worldRotation = m_filtered[i]->GetWorldRotTM();
		elem.localRotation = m_filtered[i]->GetRotation();
		m_initElementTransforms.push_back(elem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSelectionGroup::ObjectModified() const
{
	//Object has been moved/scaled/rotated, call reset
	//some objects script code may rely on the pos/dir of the object and need to be reinitialized

	for (int i = 0; i < m_objects.size(); i++)
	{
		CBaseObject* obj = m_objects[i];
		if (obj->GetType() == OBJTYPE_ENTITY)
		{
			CEntityObject* pEnt = (CEntityObject*)obj;
			if (pEnt)
			{
				IEntity* pIEnt = pEnt->GetIEntity();
				if (pIEnt)
				{
					SEntityEvent xform_finished_event(ENTITY_EVENT_XFORM_FINISHED_EDITOR);
					pIEnt->SendEvent(xform_finished_event);
				}
			}
		}
	}
}

void CSelectionGroup::FinishChanges() const
{
	for (auto& pObject : m_objects)
	{
		if (pObject)
		{
			pObject->OnEvent(EVENT_TRANSFORM_FINISHED);
			pObject->UpdateGroup();
		}
	}

	m_initElementTransforms.clear();
}

