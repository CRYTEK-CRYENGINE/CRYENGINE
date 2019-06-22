// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/NavigationSystem/MNMBaseTypes.h>

namespace MNM
{

//! Structure for storing agent setting used for NavMesh generation
struct SAgentSettings
{
	//! Returns horizontal distance from any feature in voxels that could be affected during the generation process
	size_t GetPossibleAffectedSizeHorizontal() const
	{
		// inclineTestCount = (height + 1) comes from FilterWalkable in TileGenerator
		const size_t inclineTestCount = climbableHeight + 1;
		return radius + inclineTestCount + 1;
	}

	//! Returns vertical distance from any feature in voxels that could be affected during the generation process
	size_t GetPossibleAffectedSizeVertical() const
	{
		// inclineTestCount = (height + 1) comes from FilterWalkable in TileGenerator
		const size_t inclineTestCount = climbableHeight + 1;
		const size_t maxZDiffInWorstCase = inclineTestCount * climbableHeight;

		// TODO pavloi 2016.03.16: agent.height is not applied here, because it's usually applied additionally in other places.
		// Or such places just don't care.
		// +1 just in case, I'm not fully tested this formula.
		return maxZDiffInWorstCase + 1;
	}

	uint16 radius = 4;          //!< Agent radius in voxels count
	uint16 height = 18;         //!< Agent height in voxels count
	uint16 climbableHeight = 4; //!< Maximum step height that the agent can still walk through in voxels count
	uint16 maxWaterDepth = 8;   //!< Maximum walkable water depth in voxels count

	float  climbableInclineGradient = 0.0f; //!< The steepness of a surface to still be climbable
	float  climbableStepRatio = 0.0f;

	struct SLowerHeightArea
	{
		uint16         height = 12;     //!< Minimal height for the area in voxels count
		uint16         minAreaSize = 0; //!< Minimal size of the area in voxels count
		AreaAnnotation annotation;      //!< Annotation applied to generated area
	};
	static const int kMaxLowerHeightAreas = 4;
	CryFixedArray<SLowerHeightArea, kMaxLowerHeightAreas> lowerHeightAreas; //!< Areas that can be generated in places where walkable height is lower than agent's height
};

} // namespace MNM