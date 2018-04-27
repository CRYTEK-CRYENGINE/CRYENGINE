// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MovementPlan.h"
#include <CryAISystem/MovementStyle.h>
#include "MovementHelpers.h"

namespace Movement
{
namespace MovementBlocks
{
class FollowPath : public Movement::Block, public IPlanMonitor
{
private:

	enum class EExecutionState : uint8
	{
		NotYetExecuting,
		CurrentlyExecuting,
		FinishedExecuting,

		Count
	};

	/// Tiles that were affected by NavMesh changes
	struct SMeshTileChange
	{
		enum class EChangeType : uint8
		{
			AfterGeneration = BIT(0),
			Annotation      = BIT(1),
		};
		typedef CEnumFlags<EChangeType> ChangeFlags;

		const NavigationMeshID  meshID;
		const MNM::TileID       tileID;
		CEnumFlags<EChangeType> changeFlags;

		explicit SMeshTileChange(NavigationMeshID _meshID, const MNM::TileID _tileID, const ChangeFlags& _changeFlags) : meshID(_meshID), tileID(_tileID), changeFlags(_changeFlags) {}
		bool operator==(const SMeshTileChange& rhs) const { return (this->tileID == rhs.tileID) && (this->meshID == rhs.meshID); }
	};

public:
	FollowPath(IPlan& plan, NavigationAgentTypeID navigationAgentTypeID, const CNavPath& path, const float endDistance, const MovementStyle& style, const bool endsInCover);
	~FollowPath();

	// Block
	virtual bool                    InterruptibleNow() const override { return true; }
	virtual void                    Begin(IMovementActor& actor) override;
	virtual void                    End(IMovementActor& actor) override;
	virtual Movement::Block::Status Update(const MovementUpdateContext& context) override;
	virtual const char*             GetName() const override { return "FollowPath"; }
	// ~Block

	// IPlanMonitor
	virtual bool                    CheckForReplanning(const MovementUpdateContext& context) override;
	// ~IPlanMonitor

private:
	void                            RegisterAsPlanMonitor();
	void                            UnregisterAsPlanMonitor();

	void                            OnNavigationMeshChanged(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, uint32 tileID);
	void                            OnNavigationAnnotationChanged(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, uint32 tileID);
	void                            QueueNavigationChange(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, uint32 tileID, const SMeshTileChange::ChangeFlags& changeFlag);

	bool                            IsPathOverlappingWithAnyAffectedNavMeshTileBounds() const;
	bool                            IsPathTraversableOnNavMesh(const INavMeshQueryFilter* pFilter) const;

	bool                            IsPathInterferingWithAnyQueuedNavMeshChanges(const INavMeshQueryFilter* pFilter) const;

	bool                            IsPathFollowerInterferingWithAnyQueuedNavMeshChanges(const IPathFollower& pathFollower) const;

private:
	bool                             m_bRegisteredAsPlanMonitor;
	IPlan&                           m_plan;
	NavigationAgentTypeID            m_navigationAgentTypeID;
	CNavPath                         m_path;
	MovementStyle                    m_style;
	Movement::Helpers::StuckDetector m_stuckDetector;
	std::shared_ptr<Vec3>            m_lookTarget;
	float                            m_finishBlockEndDistance;
	float                            m_accumulatedPathFollowerFailureTime;
	bool                             m_bLastFollowBlock;
	EExecutionState                  m_executionState;
	std::vector<SMeshTileChange>     m_queuedNavMeshChanges;
};
}
}
