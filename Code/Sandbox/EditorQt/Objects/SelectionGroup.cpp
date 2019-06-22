// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectionGroup.h"

#include "Objects/BrushObject.h"
#include "Objects/EntityObject.h"
#include "Objects/GeomEntity.h"
#include "Objects/PrefabObject.h"
#include "Prefabs/PrefabManager.h"
#include "SurfaceInfoPicker.h"

#include <Objects/BaseObject.h>
#include <Objects/DisplayContext.h>
#include <Preferences/SnappingPreferences.h>
#include <Viewport.h>
#include <Util/Math.h>
#include <Cry3DEngine/IStatObj.h>
#include <queue>

void CSelectionGroup::Reserve(size_t count)
{
	m_objects.reserve(count);
}

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

bool CSelectionGroup::IsEmpty() const
{
	return m_objects.empty();
}

int CSelectionGroup::GetCount() const
{
	return m_objects.size();
}

CBaseObject* CSelectionGroup::GetObject(int index) const
{
	ASSERT(index >= 0 && index < m_objects.size());
	return m_objects[index];
}

void CSelectionGroup::GetObjects(std::vector<CBaseObject*>& objects) const
{
	for (_smart_ptr<CBaseObject> pObject : m_objects)
	{
		objects.push_back(pObject.get());
	}
}

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

void CSelectionGroup::Copy(const CSelectionGroup& from)
{
	m_name = from.m_name;
	m_objects = from.m_objects;
	m_objectsSet = from.m_objectsSet;
	m_filtered = from.m_filtered;
}

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

bool CSelectionGroup::GetManipulatorMatrix(Matrix34& tm) const
{
	int numObjects = GetCount();

	if (numObjects > 0)
	{
		CLevelEditorSharedState::CoordSystem coordSystem = GetIEditor()->GetLevelEditorSharedState()->GetCoordSystem();

		CBaseObject* pObj = GetObject(numObjects - 1);
		if (coordSystem == CLevelEditorSharedState::CoordSystem::Local || coordSystem == CLevelEditorSharedState::CoordSystem::Parent)
		{
			// attempt to get local or parent matrix from gizmo owner. If none is provided, just use world matrix instead
			if (!pObj->GetManipulatorMatrix(tm))
			{
				tm.SetIdentity();
			}
		}
		else if (coordSystem == CLevelEditorSharedState::CoordSystem::World)
		{
			tm.SetIdentity();
		}

		tm.SetTranslation(GetCenter());

		return true;
	}

	return false;
}

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

void CSelectionGroup::FilterParents() const
{
	if (!m_filtered.empty())
		return;

	GetIEditorImpl()->GetObjectManager()->FilterParents(m_objects, m_filtered);
}

void CSelectionGroup::FilterLinkedObjects() const
{
	if (!m_filtered.empty())
		return;

	GetIEditorImpl()->GetObjectManager()->FilterLinkedObjects(m_objects, m_filtered);
}

void CSelectionGroup::Move(const Vec3& offset, int moveFlags, const CPoint& point, bool bFromInitPos) const
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
		//If that object has children they need to be excluded from the search as we don't want the raycast to collide with them
		for (int i = 0; i < GetFilteredCount(); ++i)
		{
			CBaseObject* object = GetFilteredObject(i);
			//exclude the object itself
			excludeObjects.Add(object);

			//and then all the children
			if (object->HasChildren())
			{
				TBaseObjects descendants;
				object->GetAllDescendants(descendants);

				for (auto child : descendants)
				{
					excludeObjects.Add(child.get());
				}
			}
		}
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
			if (GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint() == CLevelEditorSharedState::Axis::Z)
			{
				continue;
			}

			CPoint screenSpacePos = GetIEditorImpl()->GetActiveView()->WorldToView(newPos);

			CSurfaceInfoPicker surfacePicker;
			// All movements (with snapping to geometry) should also consider frozen objects
			surfacePicker.SetPickOptionFlag(CSurfaceInfoPicker::ePickOption_IncludeFrozenObject);
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
			if (GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint() != CLevelEditorSharedState::Axis::Z)
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

void CSelectionGroup::Rotate(const Quat& qRot) const
{
	Matrix34 rotateTM = Matrix33(qRot);

	Rotate(rotateTM);
}

void CSelectionGroup::Rotate(const Ang3& angles) const
{
	//if (angles.x == 0 && angles.y == 0 && angles.z == 0)
	//	return;

	Matrix34 rotateTM = Matrix34::CreateRotationXYZ(DEG2RAD(-angles)); //NOTE: angles in radians and negated

	Rotate(rotateTM);
}

void CSelectionGroup::Rotate(const Matrix34& rotateTM) const
{
	// Rotate selection about selection center.
	Vec3 center = GetCenter();

	Matrix34 ToOrigin;
	Matrix34 FromOrigin;

	ToOrigin.SetIdentity();
	FromOrigin.SetIdentity();

	CLevelEditorSharedState::CoordSystem coordinateSystem = GetIEditor()->GetLevelEditorSharedState()->GetCoordSystem();

	if (coordinateSystem != CLevelEditorSharedState::CoordSystem::Local)
	{
		ToOrigin.SetTranslation(-center);
		FromOrigin.SetTranslation(center);

		if (coordinateSystem == CLevelEditorSharedState::CoordSystem::UserDefined)
		{
			Matrix34 userTM = Matrix34::CreateIdentity();
			GetManipulatorMatrix(userTM);
			userTM.SetTranslation(Vec3(0, 0, 0));

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

		auto axisConstraints = GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint();
		// Rotation around view axis should be the same regardless of space (local, parent, world)
		if (axisConstraints == CLevelEditorSharedState::Axis::View || axisConstraints == CLevelEditorSharedState::Axis::XYZ)
		{
			// Rotate objects in view space using each object's separate world position as the pivot
			if (coordinateSystem == CLevelEditorSharedState::CoordSystem::Local)
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
		else if (coordinateSystem != CLevelEditorSharedState::CoordSystem::Local)
		{
			if (coordinateSystem == CLevelEditorSharedState::CoordSystem::Parent && obj->GetParent())
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

void CSelectionGroup::Scale(const Vec3& scaleArg) const
{
	Vec3 scl = scaleArg;
	if (scl.x == 0) scl.x = 0.01f;
	if (scl.y == 0) scl.y = 0.01f;
	if (scl.z == 0) scl.z = 0.01f;

	FilterParents();

	CLevelEditorSharedState::CoordSystem coordinateSystem = GetIEditor()->GetLevelEditorSharedState()->GetCoordSystem();
	for (int i = 0; i < GetFilteredCount(); i++)
	{
		CLevelEditorSharedState::CoordSystem objCoordSystem = coordinateSystem;

		CBaseObject* const pObj = GetFilteredObject(i);
		Vec3 scale = m_initElementTransforms[i].scale;

		if (objCoordSystem == CLevelEditorSharedState::CoordSystem::Parent && pObj->GetParent() == nullptr)
		{
			objCoordSystem = CLevelEditorSharedState::CoordSystem::Local;
		}

		if (objCoordSystem == CLevelEditorSharedState::CoordSystem::Local)
		{
			scale = scale.CompMul(scl);
		}
		else if (objCoordSystem == CLevelEditorSharedState::CoordSystem::World || objCoordSystem == CLevelEditorSharedState::CoordSystem::Parent || objCoordSystem == CLevelEditorSharedState::CoordSystem::View)
		{
			Matrix33 mFromWorld;

			// scale is not in local space, so we need to project the scale vector to the local space of the object
			if (objCoordSystem == CLevelEditorSharedState::CoordSystem::Parent)
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

void CSelectionGroup::SetScale(const Vec3& scale) const
{
	Vec3 relScale = scale;

	if (GetCount() > 0 && GetObject(0))
	{
		Vec3 objScale = GetObject(0)->GetScale();
		if (relScale == objScale && (objScale.x == 0.0f || objScale.y == 0.0f || objScale.z == 0.0f))
			return;
		relScale = relScale / objScale;
	}

	Scale(relScale);
}

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
		for (int i = 0, count = pObj->GetChildCount(); i < count; ++i)
		{
			newGroup.AddObject(pObj->GetChild(i));
			RecursiveFlattenHierarchy(pObj->GetChild(i), newGroup, flattenGroups);
		}
	}
}

void CSelectionGroup::FlattenHierarchy(CSelectionGroup& newGroup, bool flattenGroups)
{
	for (int i = 0; i < GetCount(); i++)
	{
		CBaseObject* pObj = GetObject(i);
		newGroup.AddObject(pObj);
		RecursiveFlattenHierarchy(pObj, newGroup, flattenGroups);
	}
}

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
		elem.position = m_filtered[i]->GetWorldPos();
		elem.scale = m_filtered[i]->GetScale();
		elem.worldRotation = m_filtered[i]->GetWorldRotTM();
		elem.localRotation = m_filtered[i]->GetRotation();
		m_initElementTransforms.push_back(elem);
	}
}

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
