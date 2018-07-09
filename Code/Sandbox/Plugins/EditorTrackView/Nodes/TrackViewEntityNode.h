// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.
#pragma once

#include "TrackViewAnimNode.h"
#include "Objects/EntityObject.h"
#include "Objects/TrackGizmo.h"

// This class represents an IAnimNode that is an entity
class CTrackViewEntityNode : public CTrackViewAnimNode, public ITransformDelegate, public IEntityObjectListener
{
public:
	CTrackViewEntityNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode, CEntityObject* pEntityObject);

	virtual void             Animate(const SAnimContext& animContext);

	virtual CTrackViewTrack* CreateTrack(const CAnimParamType& paramType) override;

	// Node owner setter/getter
	virtual void SetNodeEntity(CEntityObject* pEntity);
	virtual CEntityObject* GetNodeEntity(const bool bSearch = true) override;

	// Entity setters/getters
	void        SetEntityGuid(const EntityGUID& guid);
	EntityGUID* GetEntityGuid() const;
	IEntity*    GetEntity() const;

	// Set/get source/target GUIDs
	void SetEntityGuidSource(const EntityGUID& guid);
	void SetEntityGuidTarget(const EntityGUID& guid);

	// Rotation/Position & Scale
	void         SetPos(const Vec3& position, bool ignoreSelection);
	Vec3         GetPos() const;
	void         SetScale(const Vec3& scale, bool ignoreSelection);
	Vec3         GetScale() const;
	void         SetRotation(const Quat& rotation, bool ignoreSelection);
	Quat         GetRotation() const;
	bool         IsBoneLinkTransformEnabled() const;

	virtual void OnAdded() override;
	virtual void OnRemoved() override;

	// Check if this node may be moved to new parent
	virtual bool IsValidReparentingTo(CTrackViewAnimNode* pNewParent) override;

	// Sync from/to base transform. Returns false if nothing was synced.
	virtual bool SyncToBase();
	virtual bool SyncFromBase();

protected:
	// IAnimNodeOwner
	virtual void OnNodeAnimated(IAnimNode* pNode) override;
	// ~IAnimNodeOwner

private:
	virtual void Bind() override;
	virtual void UnBind() override;
	virtual void ConsoleSync(const SAnimContext& animContext) override;
	virtual bool IsNodeOwner(const CEntityObject* pOwner) override;

	void         UpdateTrackGizmo();

	void         SetPosRotScaleTracksDefaultValues();

	// ITransformDelegate
	virtual void MatrixInvalidated() override;

	virtual Vec3 GetTransformDelegatePos(const Vec3& realPos) const override;
	virtual Quat GetTransformDelegateRotation(const Quat& realRotation) const override;
	virtual Vec3 GetTransformDelegateScale(const Vec3& realScale) const override;

	virtual void SetTransformDelegatePos(const Vec3& position, bool ignoreSelection) override;
	virtual void SetTransformDelegateRotation(const Quat& rotation, bool ignoreSelection) override;
	virtual void SetTransformDelegateScale(const Vec3& scale, bool ignoreSelection) override;

	// If those return true the base object uses its own transform instead
	virtual bool IsPositionDelegated(bool ignoreSelection) const override;
	virtual bool IsRotationDelegated(bool ignoreSelection) const override;
	virtual bool IsScaleDelegated(bool ignoreSelection) const override;
	// ~ITransformDelegate

	// IEntityObjectListener
	virtual void OnNameChanged(const char* pName) override;
	virtual void OnSelectionChanged(const bool bSelected) override {}
	virtual void OnDone() override;
	// ~IEntityObjectListener

	// IAnimNodeOwner
	virtual void OnNodeVisibilityChanged(IAnimNode* pNode, const bool bHidden) override;
	virtual void OnNodeReset(IAnimNode* pNode) override;
	// ~IAnimNodeOwner

	IAnimEntityNode*   m_pAnimEntityNode;
	CEntityObject*     m_pNodeEntity;
	_smart_ptr<CTrackGizmo> m_trackGizmo;
};

