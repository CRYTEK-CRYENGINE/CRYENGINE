// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// forward declaration.
class CEntitySystem;
struct IPhysicalWorld;

//////////////////////////////////////////////////////////////////////////
class CPhysicsEventListener
{
public:
	explicit CPhysicsEventListener(IPhysicalWorld* pPhysics);
	~CPhysicsEventListener();

	static int OnStateChange(const EventPhys* pEvent);
	static int OnPostStep(const EventPhys* pEvent);
	static int OnPostStepImmediate(const EventPhys* pEvent);
	static int OnUpdateMesh(const EventPhys* pEvent);
	static int OnCreatePhysEntityPart(const EventPhys* pEvent);
	static int OnRemovePhysEntityParts(const EventPhys* pEvent);
	static int OnRevealPhysEntityPart(const EventPhys* pEvent);
	static int OnPreUpdateMesh(const EventPhys* pEvent);
	static int OnPreCreatePhysEntityPart(const EventPhys* pEvent);
	static int OnCollisionLogged(const EventPhys* pEvent);
	static int OnCollisionImmediate(const EventPhys* pEvent);
	static int OnJointBreak(const EventPhys* pEvent);
	static int OnPostPump(const EventPhys* pEvent);

	void       RegisterPhysicCallbacks();
	void       UnregisterPhysicCallbacks();

protected:
	static void SendCollisionEventToEntity(SEntityEvent& event);

private:
	static CEntity* GetEntity(void* pForeignData, int iForeignData);
	static CEntity* GetEntity(IPhysicalEntity* pPhysEntity);

	IPhysicalWorld*                      m_pPhysics;

	static std::vector<IPhysicalEntity*> m_physVisAreaUpdateVector;
	static int                           m_jointFxFrameId, m_jointFxCount;
	static int FxAllowed()
	{
		int frameId = gEnv->pRenderer->GetFrameID();
		m_jointFxCount &= -iszero(frameId - m_jointFxFrameId);
		m_jointFxFrameId = frameId;
		return m_jointFxCount < CVar::es_MaxJointFx ? ++m_jointFxCount : 0;
	}
};
