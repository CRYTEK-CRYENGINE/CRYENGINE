// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 31:05:2010  Created by Jens Schöbel

*************************************************************************/

#ifndef STEREOFRAMEWORK_H
#define STEREOFRAMEWORK_H

#include "Stereo3D/StereoZoom.h"

namespace Stereo3D
{	
	void Update(float deltaTime);

	namespace Zoom
	{
		void SetFinalPlaneDist(float planeDist, float transitionTime);
		void SetFinalEyeDist(float eyeDist, float transitionTime);
		void ReturnToNormalSetting(float);
	} // namespace zoom

	namespace Weapon
	{
		const int MAX_RAY_IDS = 5;
		const QueuedRayID INVALID_RAY_ID = 0;

		class CWeaponCheck
		{
		public:
			CWeaponCheck() 
				: m_closestDist(0.0f)
				, m_closestCastDist(1000.f) 	  
				, m_numFramesWaiting(0)
				, m_numResults(0)
			{
			}

			~CWeaponCheck();
			void Update(float deltaTime);
			void OnRayCastResult(const QueuedRayID &rayID, const RayCastResult &result);
			float GetCurrentPlaneDist();

		private:
			void CastRays();

			float  m_closestDist;
			float  m_closestCastDist;
			QueuedRayID m_rayIDs[MAX_RAY_IDS];
			uint8  m_numFramesWaiting;
			uint8  m_numResults;
		};
	} // 
} // namespace Stereo3D

#endif
