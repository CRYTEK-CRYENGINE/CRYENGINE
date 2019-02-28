// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROCEDURAL_CONTEXT_AIM__H__
#define __PROCEDURAL_CONTEXT_AIM__H__

#include <ICryMannequin.h>

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>

#include <CryExtension/CryCreateClassInstance.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryExtension/RegFactoryNode.h>

#include "ProceduralContextHelpers.h"

class CProceduralContextAim
: public IProceduralContext
{
public:
	PROCEDURAL_CONTEXT(CProceduralContextAim, "ProceduralContextAim", "4a5625bb-01d1-49c6-b563-2cf301b58e38"_cry_guid);

	CProceduralContextAim();
	virtual ~CProceduralContextAim() {}

	// IProceduralContext
	virtual void Initialise( IEntity& entity, IActionController& actionController ) override;
	virtual void Update( float timePassedSeconds ) override;
	// ~IProceduralContext

	void UpdateGameAimingRequest( const bool aimRequest );
	void UpdateProcClipAimingRequest( const bool aimRequest );

	void UpdateGameAimTarget( const Vec3& aimTarget );

	void SetBlendInTime( const float blendInTime );
	void SetBlendOutTime( const float blendOutTime );

	uint32 RequestPolarCoordinatesSmoothingParameters( const Vec2& maxSmoothRateRadiansPerSecond, const float smoothTimeSeconds );
	void CancelPolarCoordinatesSmoothingParameters( const uint32 requestId );

private:
	void InitialisePoseBlenderAim();
	void InitialiseGameAimTarget();

	void UpdatePolarCoordinatesSmoothingParameters();

private:
	IAnimationPoseBlenderDir* m_pPoseBlenderAim;

	bool m_gameRequestsAiming;
	bool m_procClipRequestsAiming;
	Vec3 m_gameAimTarget;

	Vec2 m_defaultPolarCoordinatesMaxSmoothRateRadiansPerSecond;
	float m_defaultPolarCoordinatesSmoothTimeSeconds;
	struct SPolarCoordinatesSmoothingParametersRequest
	{
		uint32 id;
		Vec2 maxSmoothRateRadiansPerSecond;
		float smoothTimeSeconds;
	};

	typedef ProceduralContextHelpers::CRequestList< SPolarCoordinatesSmoothingParametersRequest > TPolarCoordinatesSmoothingParametersRequestList;
	TPolarCoordinatesSmoothingParametersRequestList m_polarCoordinatesSmoothingParametersRequestList;
};


#endif
