// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AlignTool.h"

#include "Objects/BaseObject.h"
#include "Objects/ShapeObject.h"

#include "QtUtil.h"

//////////////////////////////////////////////////////////////////////////
bool CAlignPickCallback::m_bActive = false;

//////////////////////////////////////////////////////////////////////////
//! Called when object picked.
void CAlignPickCallback::OnPick(CBaseObject* picked)
{
	Matrix34 pickedTM(picked->GetWorldTM());

	AABB pickedAABB;
	picked->GetBoundBox(pickedAABB);
	pickedAABB.Move(-pickedTM.GetTranslation());
	Vec3 pickedPivot = pickedAABB.GetCenter();

	AABB pickedLocalAABB;
	picked->GetLocalBounds(pickedLocalAABB);

	const Quat& pickedRot = picked->GetRotation();
	const Vec3& pickedScale = picked->GetScale();
	const Vec3& pickedPos = picked->GetPos();

	float fPickedWidth = pickedAABB.max.x - pickedAABB.min.x;
	float fPickedHeight = pickedAABB.max.z - pickedAABB.min.z;
	float fPickedLength = pickedAABB.max.y - pickedAABB.min.y;

	bool bKeepScale = QtUtil::IsModifierKeyDown(Qt::ShiftModifier);
	bool bKeepRotation = QtUtil::IsModifierKeyDown(Qt::AltModifier);
	bool bAlignToBoundBox = QtUtil::IsModifierKeyDown(Qt::ControlModifier);
	bool bApplyTransform = !bKeepScale && !bKeepRotation && !bAlignToBoundBox;

	{
		bool bUndo = !CUndo::IsRecording();
		if (bUndo)
			GetIEditorImpl()->GetIUndoManager()->Begin();

		const CSelectionGroup* selGroup = GetIEditorImpl()->GetSelection();
		selGroup->FilterParents();

		for (int i = 0; i < selGroup->GetFilteredCount(); i++)
		{
			CBaseObject* pMovedObj = selGroup->GetFilteredObject(i);

			if (bKeepScale || bKeepRotation || bApplyTransform)
			{
				if (bKeepScale && bKeepRotation)  // Keep scale and rotation of a moved object
					pMovedObj->SetWorldTM(Matrix34::Create(pMovedObj->GetScale(), pMovedObj->GetRotation(), pickedPos));
				else if (bKeepScale)  // Keep only scale of a moved object
					pMovedObj->SetWorldTM(Matrix34::Create(pMovedObj->GetScale(), pickedRot, pickedPos));
				else if (bKeepRotation)   // Keep only rotation of a moved object
					pMovedObj->SetWorldTM(Matrix34::Create(pickedScale, pMovedObj->GetRotation(), pickedPos));
				else // Scale, Rotation and Position of a picked object are applied to a moved object.
					pMovedObj->SetWorldTM(pickedTM);
			}
			else if (bAlignToBoundBox)  // align to the bounding box.
			{
				if (pMovedObj->IsKindOf(RUNTIME_CLASS(CShapeObject)))
				{
					CVarObject* pVarObject = pMovedObj->GetVarObject();
					if (pVarObject == nullptr)
						continue;

					IVariable* varHeight = pVarObject->FindVariable("Height");
					if (varHeight == nullptr)
						continue;

					string movedObjClassName(pMovedObj->GetClassDesc()->ClassName());
					if (movedObjClassName != "Portal")
					{
						if (IDNO == MessageBox(NULL, "Aligning the shape to this bounding box will change the shape's vertex count. Do you want to continue?", "Warning", MB_YESNO))
							continue;
					}

					pMovedObj->StoreUndo("Align AreaShape");

					CShapeObject* pMovedShapeObj = (CShapeObject*)pMovedObj;
					varHeight->Set(fPickedHeight);
					pMovedShapeObj->ClearPoints();

					std::vector<Vec3> baseVertices;
					baseVertices.reserve(4);

					Vec3 pivot(pickedAABB.min.x, pickedAABB.min.y, 0);
					baseVertices.push_back(Vec3(0, 0, 0));
					baseVertices.push_back(Vec3(pickedAABB.max.x, pickedAABB.min.y, 0) - pivot);
					baseVertices.push_back(Vec3(pickedAABB.max.x, pickedAABB.max.y, 0) - pivot);
					baseVertices.push_back(Vec3(pickedAABB.min.x, pickedAABB.max.y, 0) - pivot);

					varHeight->Set(fPickedHeight);

					AABB movedShapeAABB;
					for (int a = 0; a < 4; ++a)
					{
						pMovedShapeObj->InsertPoint(-1, baseVertices[a], false);
						movedShapeAABB.Add(baseVertices[a]);
					}

					Vec3 volumePivot(fPickedWidth * 0.5f, fPickedLength * 0.5f, fPickedHeight * 0.5f);
					Matrix34 worldTM = Matrix34::CreateIdentity();
					worldTM.SetTranslation(pickedPos + (pickedPivot - volumePivot));
					pMovedObj->SetWorldTM(worldTM);
					continue;
				}

				if (pickedLocalAABB.GetVolume() == 0.0f)
					continue;

				AABB movedLocalAABB;
				pMovedObj->GetLocalBounds(movedLocalAABB);
				if (fabs(movedLocalAABB.max.x - movedLocalAABB.min.x) < VEC_EPSILON &&
				    fabs(movedLocalAABB.max.y - movedLocalAABB.min.y) < VEC_EPSILON &&
				    fabs(movedLocalAABB.max.z - movedLocalAABB.min.z) < VEC_EPSILON)
				{
					continue;
				}

				const Vec3& movedScale(pMovedObj->GetScale());
				Matrix34 movedScaleTM = Matrix34::CreateScale(movedScale);
				AABB movedLocalScaledAABB;
				movedLocalScaledAABB.min = movedScaleTM.TransformVector(movedLocalAABB.min);
				movedLocalScaledAABB.max = movedScaleTM.TransformVector(movedLocalAABB.max);

				float fMovedWidth = movedLocalScaledAABB.max.x - movedLocalScaledAABB.min.x;
				float fMovedHeight = movedLocalScaledAABB.max.z - movedLocalScaledAABB.min.z;
				float fMovedLength = movedLocalScaledAABB.max.y - movedLocalScaledAABB.min.y;

				Matrix34 pickedScaleTM = Matrix34::CreateScale(picked->GetScale());
				AABB pickedLocalScaledAABB;
				pickedLocalScaledAABB.min = pickedScaleTM.TransformVector(pickedLocalAABB.min);
				pickedLocalScaledAABB.max = pickedScaleTM.TransformVector(pickedLocalAABB.max);
				float fScaledPickedtWidth = pickedLocalScaledAABB.max.x - pickedLocalScaledAABB.min.x;
				float fScaledPickedHeight = pickedLocalScaledAABB.max.z - pickedLocalScaledAABB.min.z;
				float fScaledPickedLength = pickedLocalScaledAABB.max.y - pickedLocalScaledAABB.min.y;

				Vec3 scale((fScaledPickedtWidth / fMovedWidth) * movedScale.x, (fScaledPickedLength / fMovedLength) * movedScale.y, (fScaledPickedHeight / fMovedHeight) * movedScale.z);
				Matrix34 scaleRotTM = Matrix34::Create(scale, pickedRot, Vec3(0, 0, 0));
				Vec3 movedPivot = scaleRotTM.TransformVector(movedLocalAABB.GetCenter());

				if (pMovedObj->GetType() == OBJTYPE_VOLUME)
				{
					CVarObject* pVarObject = pMovedObj->GetVarObject();
					if (pVarObject == nullptr)
						continue;

					IVariable* varWidth = pVarObject->FindVariable("Width");
					IVariable* varLength = pVarObject->FindVariable("Length");
					IVariable* varHeight = pVarObject->FindVariable("Height");

					if (varWidth && varLength && varHeight)
					{
						pMovedObj->StoreUndo("Align AreaBox");

						Matrix34 worldTM;
						worldTM.SetIdentity();

						if (!pMovedObj->IsRotatable())
						{
							varHeight->Set(fPickedHeight);
							varWidth->Set(fPickedWidth);
							varLength->Set(fPickedLength);
							worldTM = Matrix34::Create(Vec3(1, 1, 1), Quat::CreateIdentity(), movedLocalAABB.GetCenter());
						}
						else
						{
							varHeight->Set(fScaledPickedHeight);
							varWidth->Set(fScaledPickedtWidth);
							varLength->Set(fScaledPickedLength);
							worldTM = Matrix34::Create(Vec3(1, 1, 1), pickedRot, pickedPos + (pickedPivot - movedPivot));
						}
						pMovedObj->SetWorldTM(worldTM);
					}
				}
				else
				{
					pMovedObj->SetWorldTM(Matrix34::Create(scale, pickedRot, Vec3(pickedPos + (pickedPivot - movedPivot))));
				}
			}
		}
		m_bActive = false;
		if (bUndo)
			GetIEditorImpl()->GetIUndoManager()->Accept("Align To Object");
	}
	delete this;
}

//! Called when pick mode cancelled.
void CAlignPickCallback::OnCancelPick()
{
	m_bActive = false;
	delete this;
}

//! Return true if specified object is pickable.
bool CAlignPickCallback::OnPickFilter(CBaseObject* filterObject)
{
	return true;
};

