// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROCEDURAL_CONTEXT_COLLIDER_MODE__H__
#define __PROCEDURAL_CONTEXT_COLLIDER_MODE__H__

#include <ICryMannequin.h>

#include <CryExtension/CryCreateClassInstance.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryExtension/RegFactoryNode.h>

#include <IAnimatedCharacter.h>

#include "ProceduralContextHelpers.h"


#define PROCEDURAL_CONTEXT_COLLIDER_MODE_NAME "ProceduralContextColliderMode"


class CProceduralContextColliderMode
	: public IProceduralContext
{
public:
	PROCEDURAL_CONTEXT(CProceduralContextColliderMode, PROCEDURAL_CONTEXT_COLLIDER_MODE_NAME, "2857e483-964b-45e4-8e9e-6a481db8c166"_cry_guid);

	virtual ~CProceduralContextColliderMode() {}

	// IProceduralContext
	virtual void Update( float timePassedSeconds ) override;
	// ~IProceduralContext

	uint32 RequestColliderMode( const EColliderMode colliderMode );
	void CancelRequest( const uint32 requestId );

private:
	IAnimatedCharacter* GetAnimatedCharacter() const;

private:
	struct SColliderModeRequest
	{
		uint32 id;
		EColliderMode mode;
	};

	typedef ProceduralContextHelpers::CRequestList< SColliderModeRequest > TColliderModeRequestList;
	TColliderModeRequestList m_requestList;
};


#endif
