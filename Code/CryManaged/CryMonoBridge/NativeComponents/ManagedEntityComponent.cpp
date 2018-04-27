// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
	, m_numProperties(static_cast<uint16>(factory.m_properties.size()))
{
	if (std::shared_ptr<CMonoMethod> pConstructorMethod = m_factory.m_pConstructorMethod.lock())
	{
		pConstructorMethod->Invoke(m_pMonoObject.get());
	}
	else
	{
		CRY_ASSERT(false);
	}
}

CManagedEntityComponent::CManagedEntityComponent(CManagedEntityComponent&& other)
	: m_factory(other.m_factory)
	, m_pMonoObject(other.m_pMonoObject)
	, IEntityComponent(std::move(other))
	, m_numProperties(other.m_numProperties)
{
	other.m_pMonoObject.reset();
}

void CManagedEntityComponent::PreInit(const SInitParams& params)
{
	IEntityComponent::PreInit(params);

	m_pEntity = params.pEntity;

	EntityId id = m_pEntity->GetId();

	void* pParams[2];
	pParams[0] = &m_pEntity;
	pParams[1] = &id;

	if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pInternalSetEntityMethod.lock())
	{
		pMethod->Invoke(m_pMonoObject.get(), pParams);
	}
	else
	{
		CRY_ASSERT(false);
	}
}

void CManagedEntityComponent::Initialize()
{
	// Initialize immediately if level is already loaded, otherwise wait for ENTITY_EVENT_LEVEL_LOADED event
	if (!gEnv->pSystem->IsLoading())
	{
		if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pInitializeMethod.lock())
		{
			pMethod->Invoke(m_pMonoObject.get());
		}

		if (!gEnv->IsEditing() && gEnv->pGameFramework->IsGameStarted())
		{
			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pGameStartMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get());
			}
		}
	}
}

void CManagedEntityComponent::ProcessEvent(const SEntityEvent &event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_LEVEL_LOADED:
		{
			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pInitializeMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get());
			}
		}
		break;
		case ENTITY_EVENT_START_GAME:
		{
			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pGameStartMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get());
			}
		}
		break;
		case ENTITY_EVENT_XFORM:
		{
			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pTransformChangedMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get());
			}
		}
		break;
		case ENTITY_EVENT_UPDATE:
		{
			void* pParams[1];
			pParams[0] = &((SEntityUpdateContext*)event.nParam[0])->fFrameTime;

			if (gEnv->IsEditing())
			{
				if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pUpdateMethodEditing.lock())
				{
					pMethod->Invoke(m_pMonoObject.get(), pParams);
				}
			}
			else
			{
				if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pUpdateMethod.lock())
				{
					pMethod->Invoke(m_pMonoObject.get(), pParams);
				}
			}
		}
		break;
		case ENTITY_EVENT_RESET:
		{
			void* pParams[1];
			pParams[0] = const_cast<intptr_t*>(&event.nParam[0]);

			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pGameModeChangeMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get(), pParams);
			}
		}
		break;
		case ENTITY_EVENT_HIDE:
		{
			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pHideMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get());
			}
		}
		break;
		case ENTITY_EVENT_UNHIDE:
		{
			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pUnHideMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get());
			}
		}
		break;
		case ENTITY_EVENT_COLLISION:
		{
			const EventPhysCollision* pCollision = reinterpret_cast<const EventPhysCollision*>(event.nParam[0]);
			
			// Make sure all references to this entity are slotted in the first element of the arrays.
			const int entityIndex = static_cast<int>(event.nParam[1]);
			const int otherIndex = (entityIndex + 1) % 2;

			const void* pParams[12];
			pParams[0] = &pCollision->pEntity[entityIndex]; // sourceEntityPhysics
			pParams[1] = &pCollision->pEntity[otherIndex]; // targetEntityPhysics
			pParams[2] = &pCollision->pt; // point
			pParams[3] = &pCollision->n; // normal
			pParams[4] = &pCollision->vloc[entityIndex]; // ownVelocity
			pParams[5] = &pCollision->vloc[otherIndex]; // otherVelocity
			pParams[6] = &pCollision->mass[entityIndex]; // ownMass
			pParams[7] = &pCollision->mass[otherIndex]; // otherMass
			pParams[8] = &pCollision->penetration; // penetrationDepth
			pParams[9] = &pCollision->normImpulse; // normImpulse
			pParams[10] = &pCollision->radius; // radius
			pParams[11] = &pCollision->fDecalPlacementTestMaxSize; // decalMaxSize

			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pCollisionMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get(), pParams);
			}
		}
		break;
		case ENTITY_EVENT_PREPHYSICSUPDATE:
		{
			void* pParams[1];
			pParams[0] = const_cast<float*>(&event.fParam[0]);

			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pPrePhysicsUpdateMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get(), pParams);
			}
		}
		break;
		case ENTITY_EVENT_DONE:
		{
			if (std::shared_ptr<CMonoMethod> pMethod = m_factory.m_pRemoveMethod.lock())
			{
				pMethod->Invoke(m_pMonoObject.get());
			}
		}
		break;
	}
}

void CManagedEntityComponent::SendSignal(int signalId, MonoInternals::MonoArray* pParams)
{
	if (Schematyc::IObject* pSchematycObject = m_pEntity->GetSchematycObject())
	{
		CManagedEntityComponentFactory::CSchematycSignal& signal = *m_factory.m_schematycSignals[signalId].get();
		Schematyc::SObjectSignal objectSignal(signal.GetGUID(), GetGUID());

		size_t numParams = MonoInternals::mono_array_length(pParams);
		objectSignal.params.ReserveInputs(numParams);

		std::vector<std::shared_ptr<Schematyc::CAnyValue>> inputValues;
		inputValues.resize(numParams);

		for (size_t i = 0; i < numParams; ++i)
		{
			MonoInternals::MonoObject* pObject = *(MonoInternals::MonoObject**)MonoInternals::mono_array_addr_with_size(pParams, sizeof(void*), i);
			MonoInternals::MonoType* pType = MonoInternals::mono_class_get_type(MonoInternals::mono_object_get_class(pObject));
			MonoInternals::MonoTypeEnum type = (MonoInternals::MonoTypeEnum)MonoInternals::mono_type_get_type(pType);

			switch (type)
			{
			case MonoInternals::MONO_TYPE_BOOLEAN:
			{
				inputValues[i] = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<bool>(), (bool*)mono_object_unbox(pObject));
			}
			break;
			case MonoInternals::MONO_TYPE_STRING:
			{
				char* szString = MonoInternals::mono_string_to_utf8((MonoInternals::MonoString*)pObject);
				Schematyc::CSharedString sharedString(szString);

				inputValues[i] = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<Schematyc::CSharedString>(), &sharedString);

				MonoInternals::mono_free(szString);
			}
			break;
			case MonoInternals::MONO_TYPE_U1:
			case MonoInternals::MONO_TYPE_CHAR: // Char is unsigned by default for .NET
			case MonoInternals::MONO_TYPE_U2:
			case MonoInternals::MONO_TYPE_U4:
			case MonoInternals::MONO_TYPE_U8:  // Losing precision! TODO: Add Schematyc support for 64?
			{
				inputValues[i] = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<uint32>(), (uint32*)mono_object_unbox(pObject));
			}
			break;
			case MonoInternals::MONO_TYPE_I1:
			case MonoInternals::MONO_TYPE_I2:
			case MonoInternals::MONO_TYPE_I4:
			case MonoInternals::MONO_TYPE_I8:  // Losing precision! TODO: Add Schematyc support for 64?
			{
				inputValues[i] = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<int32>(), (int32*)mono_object_unbox(pObject));
			}
			break;
			
			case MonoInternals::MONO_TYPE_R4:
			case MonoInternals::MONO_TYPE_R8:  // Losing precision! TODO: Add Schematyc support for 64?
			{
				inputValues[i] = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<float>(), (float*)mono_object_unbox(pObject));
			}
			break;

			default:
				CRY_ASSERT_MESSAGE(false, "Tried to send Schematyc signal with non-primitive parameter type!");
				return;
			}

			objectSignal.params.BindInput(Schematyc::CUniqueId::FromUInt32(i), inputValues[i]);
		}

		pSchematycObject->ProcessSignal(objectSignal);
	}
}