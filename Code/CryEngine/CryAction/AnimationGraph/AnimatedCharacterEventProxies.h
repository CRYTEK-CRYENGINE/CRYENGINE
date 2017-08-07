// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef __AnimatedCharacterComponents_h__
	#define __AnimatedCharacterComponents_h__

	#include <CryEntitySystem/IEntityComponent.h>

class CAnimatedCharacter;
class CAnimatedCharacterComponent_Base : public IEntityComponent
{
public:
	void SetAnimatedCharacter( CAnimatedCharacter* pAnimChar ) { m_pAnimCharacter = pAnimChar; }

protected:

	typedef CAnimatedCharacterComponent_Base SuperClass;

	explicit CAnimatedCharacterComponent_Base();

	virtual void ProcessEvent(SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override;
	virtual void OnPrePhysicsUpdate(float elapseTime) = 0;

	virtual void GameSerialize(TSerialize ser) override {};

	CAnimatedCharacter* m_pAnimCharacter;
};

class CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate : public CAnimatedCharacterComponent_Base
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS(CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate,
		"CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate", 0x3F9D1B59EBAB4F73, 0xABB9B04C675A32B9)
public:
	CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate();

	ILINE void QueueRotation(const Quat& rotation) { m_queuedRotation = rotation; m_hasQueuedRotation = true; }
	ILINE void ClearQueuedRotation()               { m_hasQueuedRotation = false; }

protected:

	virtual IEntityComponent::ComponentEventPriority GetEventPriority(const int eventID) const override;

private:
	Quat m_queuedRotation;
	bool m_hasQueuedRotation;
	virtual void OnPrePhysicsUpdate(float elapsedTime) override;
};

class CAnimatedCharacterComponent_StartAnimProc : public CAnimatedCharacterComponent_Base
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS(CAnimatedCharacterComponent_StartAnimProc,
		"CAnimatedCharacterComponent_StartAnimProc", 0xAA2D677D23A048D5, 0xAA47116B97DD6216)

protected:
	virtual ComponentEventPriority GetEventPriority(const int eventID) const override;
	virtual void                   OnPrePhysicsUpdate(float elapsedTime) override;
};

class CAnimatedCharacterComponent_GenerateMoveRequest : public CAnimatedCharacterComponent_Base
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS(CAnimatedCharacterComponent_GenerateMoveRequest,
		"CAnimatedCharacterComponent_GenerateMoveRequest", 0x0CC3EE7E1ACE4BCD, 0x9AEAE391B81B8E78)

protected:

	virtual IEntityComponent::ComponentEventPriority GetEventPriority(const int eventID) const override;
	virtual void                               OnPrePhysicsUpdate(float elapsedTime) override;
};

#endif // __AnimatedCharacterComponents_h__
