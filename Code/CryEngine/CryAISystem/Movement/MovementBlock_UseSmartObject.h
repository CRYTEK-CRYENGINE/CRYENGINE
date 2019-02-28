// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MovementBlock_UseSmartObject_h
	#define MovementBlock_UseSmartObject_h

	#include "MovementBlock_UseExactPositioningBase.h"

namespace Movement
{
namespace MovementBlocks
{
// If the exact positioning fails to position the character at the
// start of the smart object it reports the error through the bubbles
// and teleports to the end of it, and then proceeds with the plan.

class UseSmartObject : public UseExactPositioningBase
{
public:
	typedef UseExactPositioningBase Base;

	UseSmartObject(const CNavPath& path, const PathPointDescriptor::OffMeshLinkData& mnmData, const MovementStyle& style);
	virtual const char* GetName() const override          { return "UseSmartObject"; }
	virtual bool        InterruptibleNow() const override { return m_state == Prepare; }
	virtual void        Begin(IMovementActor& actor) override;
	virtual Status      Update(const MovementUpdateContext& context) override;
	void                SetUpcomingPath(const CNavPath& path);
	void                SetUpcomingStyle(const MovementStyle& style);

protected:
	virtual void OnTraverseStarted(const MovementUpdateContext& context) override;

private:
	virtual UseExactPositioningBase::TryRequestingExactPositioningResult TryRequestingExactPositioning(const MovementUpdateContext& context) override;
	virtual void                                                         HandleExactPositioningError(const MovementUpdateContext& context) override;

	Vec3                                                                 GetSmartObjectEndPosition() const;

private:
	CNavPath                                   m_upcomingPath;
	MovementStyle                              m_upcomingStyle;
	const PathPointDescriptor::OffMeshLinkData m_smartObjectMNMData;
	float m_timeSpentWaitingForSmartObjectToBecomeFree;
};
}
}

#endif // MovementBlock_UseSmartObject_h
