// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewEntityNode.h"
#include "TrackViewSequence.h"
#include "TrackViewPlugin.h"
#include "AnimationContext.h"
#include "TrackViewUndo.h"
#include "Gizmos/GizmoManager.h"
#include "Objects/ObjectManager.h"
#include "ViewManager.h"
#include "Util/Math.h"

CTrackViewEntityNode::CTrackViewEntityNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode, CEntityObject* pEntityObject)
	: CTrackViewAnimNode(pSequence, pAnimNode, pParentNode)
	, m_pAnimEntityNode(nullptr)
	, m_pNodeEntity(nullptr)
	, m_trackGizmo(nullptr)
{
	if (pAnimNode)
	{
		m_pAnimEntityNode = pAnimNode->QueryEntityNodeInterface();
		SetNodeEntity(pEntityObject ? pEntityObject : GetNodeEntity());
	}
}

void CTrackViewEntityNode::Animate(const SAnimContext& animContext)
{
	CTrackViewAnimNode::Animate(animContext);
	UpdateTrackGizmo();
}

CTrackViewTrack* CTrackViewEntityNode::CreateTrack(const CAnimParamType& paramType)
{
	CTrackViewTrack* pTrack = CTrackViewAnimNode::CreateTrack(paramType);
	SetPosRotScaleTracksDefaultValues();
	return pTrack;
}

void CTrackViewEntityNode::Bind()
{
	CEntityObject* pEntity = GetNodeEntity();
	if (pEntity)
	{
		pEntity->SetTransformDelegate(this, true);
		pEntity->RegisterListener(this);
		SetNodeEntity(pEntity);
	}

	UpdateTrackGizmo();
}

void CTrackViewEntityNode::UnBind()
{
	CEntityObject* pEntity = GetNodeEntity();
	if (pEntity)
	{
		pEntity->SetTransformDelegate(nullptr, true);
		pEntity->UnregisterListener(this);
	}

	GetIEditor()->GetGizmoManager()->RemoveGizmo(m_trackGizmo);
	m_trackGizmo = nullptr;
}

void CTrackViewEntityNode::ConsoleSync(const SAnimContext& animContext)
{
	switch (GetType())
	{
	case eAnimNodeType_Camera:
		{
			IEntity* pEntity = GetEntity();
			if (pEntity)
			{
				CBaseObject* pCameraObject = GetIEditor()->GetObjectManager()->FindObject(GetIEditor()->GetViewManager()->GetCameraObjectId());
				IEntity* pCameraEntity = pCameraObject ? ((CEntityObject*)pCameraObject)->GetIEntity() : NULL;
				if (pCameraEntity && pEntity->GetId() == pCameraEntity->GetId())
				// If this camera is currently active,
				{
					Matrix34 viewTM = pEntity->GetWorldTM();
					Vec3 oPosition(viewTM.GetTranslation());
					Vec3 oDirection(viewTM.TransformVector(FORWARD_DIRECTION));
				}
			}
		}
		break;
	}
}

bool CTrackViewEntityNode::IsNodeOwner(const CEntityObject* pOwner)
{
	return GetNodeType() == eTVNT_AnimNode && GetNodeEntity() == pOwner;
}

void CTrackViewEntityNode::SetEntityGuidTarget(const EntityGUID& guid)
{
	if (m_pAnimEntityNode)
	{
		m_pAnimEntityNode->SetEntityGuidTarget(guid);
	}
}

void CTrackViewEntityNode::SetEntityGuid(const EntityGUID& guid)
{
	if (m_pAnimEntityNode)
	{
		m_pAnimEntityNode->SetEntityGuid(guid);
	}
}

EntityGUID* CTrackViewEntityNode::GetEntityGuid() const
{
	return m_pAnimEntityNode ? m_pAnimEntityNode->GetEntityGuid() : nullptr;
}

IEntity* CTrackViewEntityNode::GetEntity() const
{
	return m_pAnimEntityNode ? m_pAnimEntityNode->GetEntity() : nullptr;
}

void CTrackViewEntityNode::SetEntityGuidSource(const EntityGUID& guid)
{
	if (m_pAnimEntityNode)
	{
		m_pAnimEntityNode->SetEntityGuidSource(guid);
	}
}

void CTrackViewEntityNode::SetPos(const Vec3& position, bool ignoreSelection)
{
	const SAnimTime time = GetSequence()->GetTime();
	CTrackViewTrack* pTrack = GetTrackForParameter(eAnimParamType_Position);

	if (pTrack)
	{
		Vec3 posPrev = stl::get<Vec3>(pTrack->GetValue(time));

		// Do not affect changes if the position slightly changed.
		// It could happen because of floating point calculations 
		// that calculated position is slightly differs from actual one 
		// but not changed by means of user interaction        
		if (!IsVectorsEqual(position, posPrev, 0.0001f))
		{
			if (!CTrackViewPlugin::GetAnimationContext()->IsRecording())
			{
				// Offset all keys by move amount.
				Vec3 offset = position - posPrev;
				pTrack->OffsetKeys(TMovieSystemValue(offset));
				GetSequence()->OnNodeChanged(pTrack, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
			}
			else if (m_pNodeEntity->IsSelected() || ignoreSelection)
			{
				CUndo::Record(new CUndoTrackObject(pTrack, GetSequence() != nullptr));
				pTrack->SetValue(GetSequence()->GetTime(), TMovieSystemValue(position));
				GetSequence()->OnNodeChanged(pTrack, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
			}
		}
	}
}

Vec3 CTrackViewEntityNode::GetPos() const
{
	return m_pAnimEntityNode->GetPos();
}

void CTrackViewEntityNode::SetScale(const Vec3& scale, bool ignoreSelection)
{
	CTrackViewTrack* pTrack = GetTrackForParameter(eAnimParamType_Scale);

	if (pTrack)
	{
		const SAnimTime time = GetSequence()->GetTime();
		if (!CTrackViewPlugin::GetAnimationContext()->IsRecording())
		{
			// Offset all keys by move amount.
			Vec3 scalePrev = stl::get<Vec3>(pTrack->GetValue(time));
			Vec3 offset = scale - scalePrev;
			pTrack->OffsetKeys(TMovieSystemValue(offset));
			GetSequence()->OnNodeChanged(pTrack, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
		}
		else if (m_pNodeEntity->IsSelected() || ignoreSelection)
		{
			CUndo::Record(new CUndoTrackObject(pTrack, GetSequence() != nullptr));
			pTrack->SetValue(GetSequence()->GetTime(), TMovieSystemValue(scale));
			GetSequence()->OnNodeChanged(pTrack, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
		}
	}
}

Vec3 CTrackViewEntityNode::GetScale() const
{
	return m_pAnimEntityNode->GetScale();
}

void CTrackViewEntityNode::SetRotation(const Quat& rotation, bool ignoreSelection)
{
	CTrackViewTrack* pTrack = GetTrackForParameter(eAnimParamType_Rotation);

	if (pTrack)
	{
		const SAnimTime time = GetSequence()->GetTime();
		if (!CTrackViewPlugin::GetAnimationContext()->IsRecording())
		{
			Quat prevRotation = stl::get<Quat>(pTrack->GetValue(time));
			// calculate difference
			Quat finalRotation = prevRotation.GetInverted() * rotation;

			pTrack->OffsetKeys(TMovieSystemValue(finalRotation));
			GetSequence()->OnNodeChanged(pTrack, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
		}
		else if (m_pNodeEntity->IsSelected() || ignoreSelection)
		{
			CUndo::Record(new CUndoTrackObject(pTrack, GetSequence() != nullptr));
			pTrack->SetValue(GetSequence()->GetTime(), TMovieSystemValue(rotation));
			GetSequence()->OnNodeChanged(pTrack, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
		}
	}
}

Quat CTrackViewEntityNode::GetRotation() const
{
	return m_pAnimEntityNode->GetRotate();
}

void CTrackViewEntityNode::UpdateTrackGizmo()
{
	if (IsActive() && m_pNodeEntity && !m_pNodeEntity->IsHidden())
	{
		if (!m_trackGizmo)
		{
			CTrackGizmo* pTrackGizmo = new CTrackGizmo;
			pTrackGizmo->SetAnimNode(this);
			m_trackGizmo = pTrackGizmo;
			GetIEditor()->GetGizmoManager()->AddGizmo(m_trackGizmo);
		}
	}
	else
	{
		GetIEditor()->GetGizmoManager()->RemoveGizmo(m_trackGizmo);
		m_trackGizmo = nullptr;
	}

	if (m_pNodeEntity && m_trackGizmo)
	{
		const Matrix34 gizmoMatrix = m_pNodeEntity->GetLinkAttachPointWorldTM();
		m_trackGizmo->SetMatrix(gizmoMatrix);
	}
}

void CTrackViewEntityNode::SetPosRotScaleTracksDefaultValues()
{
	const CEntityObject* pOwner = GetNodeEntity(false);

	CTrackViewTrack* pPosTrack = GetTrackForParameter(eAnimParamType_Position);
	CTrackViewTrack* pRotTrack = GetTrackForParameter(eAnimParamType_Rotation);
	CTrackViewTrack* pScaleTrack = GetTrackForParameter(eAnimParamType_Scale);

	if (pOwner && IsBoundToEditorObjects())
	{
		const SAnimTime time = GetSequence()->GetTime();
		if (pPosTrack)
		{
			pPosTrack->SetDefaultValue(TMovieSystemValue(pOwner->GetPos()));
		}

		if (pRotTrack)
		{
			pRotTrack->SetDefaultValue(TMovieSystemValue(pOwner->GetRotation()));
		}

		if (pScaleTrack)
		{
			pScaleTrack->SetDefaultValue(TMovieSystemValue(pOwner->GetScale()));
		}
	}
}

void CTrackViewEntityNode::SetNodeEntity(CEntityObject* pEntity)
{
	m_pNodeEntity = pEntity;

	if (GetAnimNode())
	{
		GetAnimNode()->SetNodeOwner(this);
	}

	if (pEntity)
	{
		const EntityGUID guid = ToEntityGuid(pEntity->GetId());
		SetEntityGuid(guid);

		if (pEntity->GetLookAt() && pEntity->GetLookAt()->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* target = static_cast<CEntityObject*>(pEntity->GetLookAt());
			SetEntityGuidTarget(ToEntityGuid(target->GetId()));
		}
		if (pEntity->GetLookAtSource() && pEntity->GetLookAtSource()->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* source = static_cast<CEntityObject*>(pEntity->GetLookAtSource());
			SetEntityGuidSource(ToEntityGuid(source->GetId()));
		}

		SetPosRotScaleTracksDefaultValues();

		OnSelectionChanged(pEntity->IsSelected());
	}

	GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_NodeOwnerChanged);
}

CEntityObject* CTrackViewEntityNode::GetNodeEntity(const bool bSearch)
{
	if (m_pAnimEntityNode)
	{
		if (m_pNodeEntity)
		{
			return m_pNodeEntity;
		}

		if (bSearch)
		{
			IEntity* pIEntity = GetEntity();
			if (pIEntity)
			{
				// Find owner editor entity.
				return CEntityObject::FindFromEntityId(pIEntity->GetId());
			}
		}
	}

	return nullptr;
}

void CTrackViewEntityNode::OnAdded()
{
	CEntityObject* pEntityObject = GetNodeEntity();
	if (pEntityObject && IsActive())
	{
		pEntityObject->SetTransformDelegate(this, true);
		pEntityObject->RegisterListener(this);
	}
}

void CTrackViewEntityNode::OnRemoved()
{
	CEntityObject* pEntityObject = GetNodeEntity();
	if (pEntityObject && IsActive())
	{
		pEntityObject->SetTransformDelegate(nullptr, true);
		pEntityObject->UnregisterListener(this);
	}
}

// Check if this node may be moved to new parent
bool CTrackViewEntityNode::IsValidReparentingTo(CTrackViewAnimNode* pNewParent)
{
	if (!CTrackViewAnimNode::IsValidReparentingTo(pNewParent))
	{
		return false;
	}

	// Check if another node already owns this entity in the new parent's tree
	CEntityObject* pOwner = GetNodeEntity();
	if (pOwner)
	{
		CTrackViewAnimNodeBundle ownedNodes = pNewParent->GetAllOwnedNodes(pOwner);
		if (ownedNodes.GetCount() > 0 && ownedNodes.GetNode(0) != this)
		{
			return false;
		}
	}

	return true;
}

void CTrackViewEntityNode::OnNodeAnimated(IAnimNode* pNode)
{
	if (m_pNodeEntity)
	{
		// By invalidating the transform on the object it will update its transform through the ITransformDelegate (this object)
		m_pNodeEntity->InvalidateTM(0);
	}
}

void CTrackViewEntityNode::OnNameChanged(const char* pName)
{
	SetName(pName);
}

void CTrackViewEntityNode::OnNodeVisibilityChanged(IAnimNode* pNode, const bool bHidden)
{
	if (m_pNodeEntity)
	{
		m_pNodeEntity->SetHidden(bHidden);

		// Need to do this to force recreation of gizmos
		bool bUnhideSelected = !m_pNodeEntity->IsHidden() && m_pNodeEntity->IsSelected();
		if (bUnhideSelected)
		{
			GetIEditor()->GetObjectManager()->UnselectObject(m_pNodeEntity);
			GetIEditor()->GetObjectManager()->SelectObject(m_pNodeEntity);
		}

		UpdateTrackGizmo();
	}
}

void CTrackViewEntityNode::OnNodeReset(IAnimNode* pNode)
{
	if (gEnv->IsEditing() && m_pNodeEntity)
	{
		// If the node has an event track, one should also reload the script when the node is reset.
		CTrackViewTrack* pAnimTrack = GetTrackForParameter(eAnimParamType_Event);
		if (pAnimTrack && pAnimTrack->GetKeyCount())
		{
			CEntityScript* script = m_pNodeEntity->GetScript();
			if (script)
			{
				script->Reload();
			}
			m_pNodeEntity->Reload(true);
		}
	}
}

void CTrackViewEntityNode::MatrixInvalidated()
{
	UpdateTrackGizmo();
}

Vec3 CTrackViewEntityNode::GetTransformDelegatePos(const Vec3& basePos) const
{
	const Vec3 position = GetPos();

	return Vec3(CheckTrackAnimated(eAnimParamType_PositionX) ? position.x : basePos.x,
	            CheckTrackAnimated(eAnimParamType_PositionY) ? position.y : basePos.y,
	            CheckTrackAnimated(eAnimParamType_PositionZ) ? position.z : basePos.z);
}

Quat CTrackViewEntityNode::GetTransformDelegateRotation(const Quat& baseRotation) const
{
	if (!CheckTrackAnimated(eAnimParamType_Rotation))
	{
		return baseRotation;
	}

	return GetRotation();
}

Vec3 CTrackViewEntityNode::GetTransformDelegateScale(const Vec3& baseScale) const
{
	const Vec3 scale = GetScale();

	return Vec3(CheckTrackAnimated(eAnimParamType_ScaleX) ? scale.x : baseScale.x,
	            CheckTrackAnimated(eAnimParamType_ScaleY) ? scale.y : baseScale.y,
	            CheckTrackAnimated(eAnimParamType_ScaleZ) ? scale.z : baseScale.z);
}

void CTrackViewEntityNode::SetTransformDelegatePos(const Vec3& position, bool ignoreSelection)
{
	SetPos(position, ignoreSelection);
}

void CTrackViewEntityNode::SetTransformDelegateRotation(const Quat& rotation, bool ignoreSelection)
{
	SetRotation(rotation, ignoreSelection);
}

void CTrackViewEntityNode::SetTransformDelegateScale(const Vec3& scale, bool ignoreSelection)
{
	SetScale(scale, ignoreSelection);
}

bool CTrackViewEntityNode::IsPositionDelegated(bool ignoreSelection) const
{
	const bool bDelegated = (CTrackViewPlugin::GetAnimationContext()->IsRecording() &&
	                         (m_pNodeEntity->IsSelected() || ignoreSelection) && GetTrackForParameter(eAnimParamType_Position)) || CheckTrackAnimated(eAnimParamType_Position);
	return bDelegated;
}

bool CTrackViewEntityNode::IsRotationDelegated(bool ignoreSelection) const
{
	const bool bDelegated = (CTrackViewPlugin::GetAnimationContext()->IsRecording() &&
	                         (m_pNodeEntity->IsSelected() || ignoreSelection) && GetTrackForParameter(eAnimParamType_Rotation)) || CheckTrackAnimated(eAnimParamType_Rotation);
	return bDelegated;
}

bool CTrackViewEntityNode::IsScaleDelegated(bool ignoreSelection) const
{
	const bool bDelegated = (CTrackViewPlugin::GetAnimationContext()->IsRecording() &&
	                         (m_pNodeEntity->IsSelected() || ignoreSelection) && GetTrackForParameter(eAnimParamType_Scale)) || CheckTrackAnimated(eAnimParamType_Scale);
	return bDelegated;
}

void CTrackViewEntityNode::OnDone()
{
	SetNodeEntity(nullptr);
	UpdateTrackGizmo();
}

bool CTrackViewEntityNode::SyncToBase()
{
	CEntityObject* pEntityObject = GetNodeEntity();
	if (pEntityObject && IsActive())
	{
		ITransformDelegate* pDelegate = pEntityObject->GetTransformDelegate();
		pEntityObject->SetTransformDelegate(nullptr, true);

		const Vec3 position = GetPos();
		pEntityObject->SetPos(position);

		const Quat rotation = GetRotation();
		pEntityObject->SetRotation(rotation);

		const Vec3 scale = GetScale();
		pEntityObject->SetScale(scale);

		pEntityObject->SetTransformDelegate(pDelegate, true);

		return true;
	}

	return false;
}

bool CTrackViewEntityNode::SyncFromBase()
{
	CEntityObject* pEntityObject = GetNodeEntity();
	if (pEntityObject && IsActive())
	{
		ITransformDelegate* pDelegate = pEntityObject->GetTransformDelegate();
		pEntityObject->SetTransformDelegate(nullptr, true);

		const Vec3 position = pEntityObject->GetPos();
		SetPos(position, false);

		const Quat rotation = pEntityObject->GetRotation();
		SetRotation(rotation, false);

		const Vec3 scale = pEntityObject->GetScale();
		SetScale(scale, false);

		pEntityObject->SetTransformDelegate(pDelegate, true);

		return true;
	}

	return false;
}

