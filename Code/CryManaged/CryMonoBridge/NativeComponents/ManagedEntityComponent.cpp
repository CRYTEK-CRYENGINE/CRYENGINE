#include "StdAfx.h"
#include "ManagedEntityComponent.h"

#include "MonoRuntime.h"

#include "Wrappers/MonoString.h"
#include "Wrappers/MonoDomain.h"
#include "Wrappers/MonoClass.h"
#include "Wrappers/MonoMethod.h"

#include <CryPhysics/physinterface.h>

CManagedEntityComponent::CManagedEntityComponent(const CManagedEntityComponentFactory& factory)
	: m_factory(factory)
	, m_pMonoObject(m_factory.m_pClass->CreateUninitializedInstance())
{
	m_factory.m_pConstructorMethod->Invoke(m_pMonoObject.get());
}

void CManagedEntityComponent::PreInit(const SInitParams& params)
{
	IEntityComponent::PreInit(params);

	m_pEntity = params.pEntity;

	EntityId id = m_pEntity->GetId();

	void* pParams[2];
	pParams[0] = &m_pEntity;
	pParams[1] = &id;

	m_factory.m_pInternalSetEntityMethod->Invoke(m_pMonoObject.get(), pParams);
}

void CManagedEntityComponent::Initialize()
{
	if (m_factory.m_pInitializeMethod != nullptr)
	{
		m_factory.m_pInitializeMethod->Invoke(m_pMonoObject.get());
	}

	if (m_factory.m_pGameStartMethod != nullptr && !gEnv->IsEditing() && gEnv->pGameFramework->IsGameStarted())
	{
		m_factory.m_pGameStartMethod->Invoke(m_pMonoObject.get());
	}
}

void CManagedEntityComponent::ProcessEvent(SEntityEvent &event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_START_GAME:
			{
				m_factory.m_pGameStartMethod->Invoke(m_pMonoObject.get());
			}
			break;
		case ENTITY_EVENT_XFORM:
			{
				m_factory.m_pTransformChangedMethod->Invoke(m_pMonoObject.get());
			}
			break;
		case ENTITY_EVENT_UPDATE:
			{
				void* pParams[1];
				pParams[0] = &((SEntityUpdateContext*)event.nParam[0])->fFrameTime;

				if (gEnv->IsEditing())
				{
					if (m_factory.m_pUpdateMethodEditing != nullptr)
					{
						m_factory.m_pUpdateMethodEditing->Invoke(m_pMonoObject.get(), pParams);
					}
				}
				else
				{
					if (m_factory.m_pUpdateMethod != nullptr)
					{
						m_factory.m_pUpdateMethod->Invoke(m_pMonoObject.get(), pParams);
					}
				}
			}
			break;
		case ENTITY_EVENT_RESET:
			{
				void* pParams[1];
				pParams[0] = &event.nParam[0];

				m_factory.m_pGameModeChangeMethod->Invoke(m_pMonoObject.get(), pParams);
			}
			break;
		case ENTITY_EVENT_HIDE:
			{
				m_factory.m_pHideMethod->Invoke(m_pMonoObject.get());
			}
			break;
		case ENTITY_EVENT_UNHIDE:
			{
				m_factory.m_pUnHideMethod->Invoke(m_pMonoObject.get());
			}
			break;
		case ENTITY_EVENT_COLLISION:
			{
				EventPhysCollision* pCollision = (EventPhysCollision*)event.nParam[0];

				void* pParams[2];
				pParams[0] = &pCollision->pEntity[0];
				pParams[1] = &pCollision->pEntity[1];

				m_factory.m_pCollisionMethod->Invoke(m_pMonoObject.get(), pParams);
			}
			break;
		case ENTITY_EVENT_PREPHYSICSUPDATE:
			{
				void* pParams[1];
				pParams[0] = &event.fParam[0];

				m_factory.m_pPrePhysicsUpdateMethod->Invoke(m_pMonoObject.get(), pParams);
			}
			break;
		case ENTITY_EVENT_DONE:
			{
				m_factory.m_pRemoveMethod->Invoke(m_pMonoObject.get());
			}
			break;
	}
}
