// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/NavigationSystem/MNMBaseTypes.h>

namespace MNM
{
struct WayTriangleData
{
	WayTriangleData()
		: triangleID()
		, offMeshLinkID()
		, costMultiplier(1.0f)
		, incidentEdge((unsigned int)MNM::Constants::InvalidEdgeIndex)
	{}

	WayTriangleData(const TriangleID _triangleID, const OffMeshLinkID _offMeshLinkID)
		: triangleID(_triangleID)
		, offMeshLinkID(_offMeshLinkID)
		, costMultiplier(1.f)
		, incidentEdge((unsigned int)MNM::Constants::InvalidEdgeIndex)
	{}

	operator bool() const
	{
		return (triangleID.IsValid());
	}

	bool operator<(const WayTriangleData& other) const
	{
		return triangleID == other.triangleID ? offMeshLinkID < other.offMeshLinkID : triangleID.GetValue() < other.triangleID.GetValue();
	}

	bool operator==(const WayTriangleData& other) const
	{
		return ((triangleID == other.triangleID) && (offMeshLinkID == other.offMeshLinkID));
	}

	TriangleID    triangleID;
	OffMeshLinkID offMeshLinkID;
	float         costMultiplier;
	unsigned int  incidentEdge;
};

} // namespace MNM