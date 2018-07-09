// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementBlock_SetupPipeUserCoverInformation.h"
#include "Movement/MovementActor.h"
#include "Cover/CoverSystem.h"
#include "PipeUser.h"

namespace Movement
{
namespace MovementBlocks
{
void SetupPipeUserCoverInformation::Begin(IMovementActor& actor)
{
	CoverID nextCoverID = m_pipeUser.GetCoverRegister();
	if (!nextCoverID)
	{
		// PipeUser is dead or for some other reason it has reset requested cover
		return;
	}
	
	ICoverUser::StateFlags coverState = ICoverUser::EStateFlags::MovingToCover;

	// The cover we are in or moving towards
	CoverID currCoverID = m_pipeUser.GetCoverID();

	if (currCoverID && m_pipeUser.IsInCover())
	{
		CCoverSystem& coverSystem = *gAIEnv.pCoverSystem;

		if (coverSystem.GetSurfaceID(currCoverID) == coverSystem.GetSurfaceID(nextCoverID))
		{
			coverState.Add(ICoverUser::EStateFlags::InCover);
		}
		else
		{
			// Check if surfaces are neighbors (edges are close to each other)
			const CoverSurface& currSurface = coverSystem.GetCoverSurface(currCoverID);
			const CoverSurface& nextSurface = coverSystem.GetCoverSurface(nextCoverID);

			const float neighborDistSq = square(0.3f);

			ICoverSystem::SurfaceInfo currSurfaceInfo;
			ICoverSystem::SurfaceInfo nextSurfaceInfo;

			if (currSurface.GetSurfaceInfo(&currSurfaceInfo) && nextSurface.GetSurfaceInfo(&nextSurfaceInfo))
			{
				const Vec3& currLeft = currSurfaceInfo.samples[0].position;
				const Vec3& currRight = currSurfaceInfo.samples[currSurfaceInfo.sampleCount - 1].position;
				const Vec3& nextLeft = nextSurfaceInfo.samples[0].position;
				const Vec3& nextRight = nextSurfaceInfo.samples[nextSurfaceInfo.sampleCount - 1].position;

				if (Distance::Point_Point2DSq(currLeft, nextRight) < neighborDistSq ||
					Distance::Point_Point2DSq(currRight, nextLeft) < neighborDistSq)
				{
					coverState.Add(ICoverUser::EStateFlags::InCover);
				}
			}
		}
	}

	m_pipeUser.SetCoverID(nextCoverID);
	m_pipeUser.SetCoverState(coverState);
}
}
}
