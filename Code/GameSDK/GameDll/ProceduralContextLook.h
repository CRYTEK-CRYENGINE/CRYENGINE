// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROCEDURAL_CONTEXT_LOOK__H__
#define __PROCEDURAL_CONTEXT_LOOK__H__

#include <ICryMannequin.h>

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>

#include <CryExtension/CryCreateClassInstance.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryExtension/RegFactoryNode.h>

class CProceduralContextLook
	: public IProceduralContext
{
public:
	PROCEDURAL_CONTEXT(CProceduralContextLook, "ProceduralContextLook", "0928592b-d916-48a5-9024-c8221945bb17"_cry_guid);

	CProceduralContextLook();
	virtual ~CProceduralContextLook() {}

	// IProceduralContext
	virtual void Initialise( IEntity& entity, IActionController& actionController ) override;
	virtual void Update( float timePassedSeconds ) override;
	// ~IProceduralContext

	void UpdateGameLookingRequest( const bool lookRequest );
	void UpdateProcClipLookingRequest( const bool lookRequest );

	void UpdateGameLookTarget( const Vec3& lookTarget );

	void SetBlendInTime( const float blendInTime );
	void SetBlendOutTime( const float blendOutTime );
	void SetFovRadians( const float fovRadians );

private:
	void InitialisePoseBlenderLook();
	void InitialiseGameLookTarget();

private:
	IAnimationPoseBlenderDir* m_pPoseBlenderLook;

	bool m_gameRequestsLooking;
	bool m_procClipRequestsLooking;
	Vec3 m_gameLookTarget;
};


#endif
