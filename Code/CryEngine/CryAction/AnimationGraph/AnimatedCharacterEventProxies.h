// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override;
	virtual void OnPrePhysicsUpdate(float elapseTime) = 0;

	virtual void GameSerialize(TSerialize ser) override {};

	CAnimatedCharacter* m_pAnimCharacter;
};

class CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate : public CAnimatedCharacterComponent_Base
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate,
		"CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate", "3f9d1b59-ebab-4f73-abb9-b04c675a32b9"_cry_guid)
public:
	CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate();

	ILINE void QueueRotation(const Quat& rotation) { m_queuedRotation = rotation; m_hasQueuedRotation = true; }
	ILINE void ClearQueuedRotation()               { m_hasQueuedRotation = false; }

private:

	virtual IEntityComponent::ComponentEventPriority GetEventPriority() const override;

	Quat m_queuedRotation;
	bool m_hasQueuedRotation;
	virtual void OnPrePhysicsUpdate(float elapsedTime) override;
};

class CAnimatedCharacterComponent_StartAnimProc : public CAnimatedCharacterComponent_Base
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CAnimatedCharacterComponent_StartAnimProc,
		"CAnimatedCharacterComponent_StartAnimProc", "aa2d677d-23a0-48d5-aa47-116b97dd6216"_cry_guid)

private:
	virtual IEntityComponent::ComponentEventPriority GetEventPriority() const override;
	virtual void                   OnPrePhysicsUpdate(float elapsedTime) override;
};

class CAnimatedCharacterComponent_GenerateMoveRequest : public CAnimatedCharacterComponent_Base
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CAnimatedCharacterComponent_GenerateMoveRequest,
		"CAnimatedCharacterComponent_GenerateMoveRequest", "0cc3ee7e-1ace-4bcd-9aea-e391b81b8e78"_cry_guid)

private:

	virtual IEntityComponent::ComponentEventPriority GetEventPriority() const override;
	virtual void                               OnPrePhysicsUpdate(float elapsedTime) override;
};

#endif // __AnimatedCharacterComponents_h__
