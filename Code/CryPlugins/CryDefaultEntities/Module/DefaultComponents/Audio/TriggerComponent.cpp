// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TriggerComponent.h"
#include <CryAudio/IAudioSystem.h>

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	#include <CryRenderer/IRenderAuxGeom.h>
	#include <CrySerialization/Decorators/ActionButton.h>
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SPlayTriggerSerializeHelper>& desc)
{
	desc.SetGUID("ABB005B4-36C5-4120-ABFA-294F22E496C8"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void SPlayTriggerSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(m_autoPlay, "autoPlay", "AutoPlay");
	archive(Serialization::AudioTrigger<string>(m_name), "playTrigger", "PlayTrigger");

	if (archive.isInput())
	{
		m_id = CryAudio::StringToId(m_name.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SStopTriggerSerializeHelper>& desc)
{
	desc.SetGUID("0BF70B52-8DA8-41AA-9F01-1F556291871E"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void SStopTriggerSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(m_canStop, "canStop", "Can Stop");

	if (m_canStop)
	{
		archive(Serialization::AudioTrigger<string>(m_name), "stopTrigger", "StopTrigger");

		if (archive.isInput())
		{
			m_id = CryAudio::StringToId(m_name.c_str());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
static void ReflectType(Schematyc::CTypeDesc<CTriggerComponent::SFinishedSignal>& desc)
{
	desc.SetGUID("A16A29CB-8E39-42C0-88C2-33FED1680545"_cry_guid);
	desc.SetLabel("Finished");
}

//////////////////////////////////////////////////////////////////////////
void CTriggerComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTriggerComponent::Play, "B7FDCC03-6312-4795-8D00-D63F3381BFBC"_cry_guid, "Play");
		pFunction->SetDescription("Executes the PlayTrigger");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTriggerComponent::Stop, "11AB550A-A6C9-422E-8D87-8333E845C23D"_cry_guid, "Stop");
		pFunction->SetDescription("Stops the PlayTrigger if no StopTrigger assigned otherwise executes the StopTrigger.");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		componentScope.Register(pFunction);
	}

	componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(CTriggerComponent::SFinishedSignal));
}

//////////////////////////////////////////////////////////////////////////
void CTriggerComponent::ReflectType(Schematyc::CTypeDesc<CTriggerComponent>& desc)
{
	desc.SetGUID("672F0641-004E-4300-B4F7-764B70CC4DA0"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("Trigger");
	desc.SetDescription("Allows for execution of an audio trigger at provided transformation.");
	desc.SetIcon("icons:Audio/component_trigger.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

	desc.AddMember(&CTriggerComponent::m_playTrigger, 'play', "play", "Play", "This trigger gets executed when Play is called.", SPlayTriggerSerializeHelper());
	desc.AddMember(&CTriggerComponent::m_stopTrigger, 'stop', "stop", "Stop", "This trigger gets executed when Stop is called.", SStopTriggerSerializeHelper());

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	desc.AddMember(&CTriggerComponent::m_debugInfo, 'dbug', "debug", "Debug", "Debug functionality.", CDebugSerializeHelper());
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CTriggerComponent::Initialize()
{
	m_pIEntityAudioComponent = m_pEntity->GetComponent<IEntityAudioComponent>();

	if (m_pIEntityAudioComponent != nullptr)
	{
		if (m_auxObjectId != CryAudio::InvalidAuxObjectId && m_auxObjectId != CryAudio::DefaultAuxObjectId)
		{
			// Workaround for scenarios where 'Initialize' is called twice without a 'OnShutDown' in between.
			m_pIEntityAudioComponent->RemoveAudioAuxObject(m_auxObjectId);
		}

		m_auxObjectId = m_pIEntityAudioComponent->CreateAudioAuxObject();
	}
	else
	{
		m_pIEntityAudioComponent = m_pEntity->CreateComponent<IEntityAudioComponent>();
		CRY_ASSERT(m_auxObjectId == CryAudio::InvalidAuxObjectId);
		m_auxObjectId = CryAudio::DefaultAuxObjectId;
	}

	// Only play in editor. Launcher is handled via ENTITY_EVENT_START_GAME.
	if (m_playTrigger.m_autoPlay && gEnv->IsEditor())
	{
		Play();
	}

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	m_debugInfo.SetComponent(this);
	m_debugInfo.UpdateDebugInfo();
	m_previousPlayTriggerId = m_playTrigger.m_id;
	m_previousStopTriggerId = m_stopTrigger.m_id;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
Cry::Entity::EventFlags CTriggerComponent::GetEventMask() const
{
	Cry::Entity::EventFlags mask = ENTITY_EVENT_AUDIO_TRIGGER_STARTED | ENTITY_EVENT_AUDIO_TRIGGER_ENDED | ENTITY_EVENT_START_GAME | ENTITY_EVENT_DONE;

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	mask |= ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED | ENTITY_EVENT_RESET | ENTITY_EVENT_UPDATE;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE

	return mask;
}

//////////////////////////////////////////////////////////////////////////
void CTriggerComponent::ProcessEvent(const SEntityEvent& event)
{
	if (m_pIEntityAudioComponent != nullptr)
	{
		switch (event.event)
		{
		case ENTITY_EVENT_AUDIO_TRIGGER_STARTED:
			{
				++m_numActiveTriggerInstances;

				break;
			}
		case ENTITY_EVENT_AUDIO_TRIGGER_ENDED:
			{
				CRY_ASSERT(m_numActiveTriggerInstances > 0);
				--m_numActiveTriggerInstances;

				if (m_numActiveTriggerInstances == 0)
				{
					auto const pRequestInfo = reinterpret_cast<CryAudio::SRequestInfo const*>(event.nParam[0]);
					auto const auxObjectId = static_cast<CryAudio::AuxObjectId>(reinterpret_cast<uintptr_t>(pRequestInfo->pUserData));

					if (auxObjectId == m_auxObjectId)
					{
						Schematyc::IObject* const pSchematycObject = GetEntity()->GetSchematycObject();

						if (pSchematycObject != nullptr)
						{
							pSchematycObject->ProcessSignal(SFinishedSignal(), GetGUID());
						}
					}
				}

				break;
			}
		case ENTITY_EVENT_START_GAME:
			{
				// Only play in launcher. Editor is handled in Initialize()
				if (m_playTrigger.m_autoPlay && !gEnv->IsEditor())
				{
					Play();
				}

				break;
			}
		case ENTITY_EVENT_DONE:
			{
				Stop();

				if (m_pIEntityAudioComponent != nullptr && m_auxObjectId != CryAudio::InvalidAuxObjectId && m_auxObjectId != CryAudio::DefaultAuxObjectId)
				{
					m_pIEntityAudioComponent->RemoveAudioAuxObject(m_auxObjectId);
				}

				m_auxObjectId = CryAudio::InvalidAuxObjectId;

				break;
			}
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
		case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
			{
				if (m_previousPlayTriggerId != m_playTrigger.m_id)
				{
					if (m_previousPlayTriggerId != CryAudio::InvalidControlId)
					{
						m_pIEntityAudioComponent->StopTrigger(m_previousPlayTriggerId, m_auxObjectId);
					}

					if (m_playTrigger.m_autoPlay)
					{
						Play();
					}
				}
				else if (m_previousStopTriggerId != m_stopTrigger.m_id)
				{
					if (m_previousStopTriggerId != CryAudio::InvalidControlId)
					{
						m_pIEntityAudioComponent->StopTrigger(m_previousStopTriggerId, m_auxObjectId);
					}
				}

				m_previousPlayTriggerId = m_playTrigger.m_id;
				m_previousStopTriggerId = m_stopTrigger.m_id;

				m_debugInfo.UpdateDebugInfo();

				break;
			}
		case ENTITY_EVENT_RESET:
			{
				if (m_playTrigger.m_autoPlay && (m_numActiveTriggerInstances == 0))
				{
					Play();
				}
				else if (!m_playTrigger.m_autoPlay && (m_numActiveTriggerInstances > 0))
				{
					Stop();
				}

				m_debugInfo.UpdateDebugInfo();

				break;
			}
		case ENTITY_EVENT_UPDATE:
			{
				m_debugInfo.DrawDebugInfo();

				break;
			}
#endif      // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTriggerComponent::Play()
{
	if ((m_pIEntityAudioComponent != nullptr) && (m_playTrigger.m_id != CryAudio::InvalidControlId))
	{
		auto const pAuxObjectId = reinterpret_cast<void*>(static_cast<uintptr_t>(m_auxObjectId));
		CryAudio::SRequestUserData const userData(CryAudio::ERequestFlags::DoneCallbackOnExternalThread | CryAudio::ERequestFlags::CallbackOnExternalOrCallingThread, this, pAuxObjectId);
		m_pIEntityAudioComponent->ExecuteTrigger(m_playTrigger.m_id, m_auxObjectId, m_pIEntityAudioComponent->GetEntityId(), userData);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTriggerComponent::Stop()
{
	if ((m_pIEntityAudioComponent != nullptr) && m_stopTrigger.m_canStop)
	{
		if (m_stopTrigger.m_id != CryAudio::InvalidControlId)
		{
			auto const pAuxObjectId = reinterpret_cast<void*>(static_cast<uintptr_t>(m_auxObjectId));
			CryAudio::SRequestUserData const userData(CryAudio::ERequestFlags::DoneCallbackOnExternalThread | CryAudio::ERequestFlags::CallbackOnExternalOrCallingThread, this, pAuxObjectId);
			m_pIEntityAudioComponent->ExecuteTrigger(m_stopTrigger.m_id, m_auxObjectId, m_pIEntityAudioComponent->GetEntityId(), userData);
		}
		else if (m_playTrigger.m_id != CryAudio::InvalidControlId)
		{
			m_pIEntityAudioComponent->StopTrigger(m_playTrigger.m_id, m_auxObjectId);
		}
	}
}

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<CDebugSerializeHelper>& desc)
{
	desc.SetGUID("585F7020-536A-4FFD-BF62-4F7AF1054ACB"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void CDebugSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::ActionButton(functor(*this, &CDebugSerializeHelper::OnPlay)), "play", "Play");

	bool const canStop = (m_pComponent != nullptr) && (m_pComponent->CanStop());

	if (canStop)
	{
		archive(Serialization::ActionButton(functor(*this, &CDebugSerializeHelper::OnStop)), "stop", "Stop");
	}

	archive(m_drawActivityRadius, "drawPlayTriggerRadius", "Draw PlayTrigger Radius");

	if (canStop)
	{
		archive(m_drawStopTriggerRadius, "drawStopTriggerRadius", "Draw StopTrigger Radius");
	}
}

//////////////////////////////////////////////////////////////////////////
void CDebugSerializeHelper::OnPlay()
{
	if (m_pComponent != nullptr)
	{
		m_pComponent->Play();
	}
}

//////////////////////////////////////////////////////////////////////////
void CDebugSerializeHelper::OnStop()
{
	if (m_pComponent != nullptr)
	{
		m_pComponent->Stop();
	}
}

//////////////////////////////////////////////////////////////////////////
void CDebugSerializeHelper::UpdateDebugInfo()
{
	if (m_pComponent != nullptr)
	{
		CryAudio::STriggerData triggerData;
		gEnv->pAudioSystem->GetTriggerData(m_pComponent->GetPlayTriggerId(), triggerData);
		m_playTriggerRadius = triggerData.radius;

		if (m_pComponent->CanStop())
		{
			gEnv->pAudioSystem->GetTriggerData(m_pComponent->GetStopTriggerId(), triggerData);
			m_stopTriggerRadius = triggerData.radius;
		}
		else
		{
			m_stopTriggerRadius = 0.0f;
		}
	}

	m_canDrawPlayTriggerRadius = (m_drawActivityRadius && (m_playTriggerRadius > 0.0f));
	m_canDrawStopTriggerRadius = (m_drawStopTriggerRadius && (m_stopTriggerRadius > 0.0f));
}

//////////////////////////////////////////////////////////////////////////
void CDebugSerializeHelper::DrawDebugInfo()
{
	if (m_pComponent != nullptr)
	{
		if (m_canDrawPlayTriggerRadius || m_canDrawStopTriggerRadius)
		{
			IRenderAuxGeom* const pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

			if (pAuxGeom != nullptr)
			{
				SAuxGeomRenderFlags const currentRenderFlags = pAuxGeom->GetRenderFlags();
				SAuxGeomRenderFlags newRenderFlags = currentRenderFlags;
				newRenderFlags.SetCullMode(e_CullModeNone);
				newRenderFlags.SetFillMode(e_FillModeWireframe);
				newRenderFlags.SetAlphaBlendMode(e_AlphaBlended);
				pAuxGeom->SetRenderFlags(newRenderFlags);

				Vec3 const pos = m_pComponent->GetEntity()->GetWorldPos();

				if (m_canDrawPlayTriggerRadius)
				{
					pAuxGeom->DrawSphere(pos, m_playTriggerRadius, ColorB(100, 250, 100, 100), false);
				}

				if (m_canDrawStopTriggerRadius)
				{
					pAuxGeom->DrawSphere(pos, m_stopTriggerRadius, ColorB(250, 100, 100, 100), false);
				}

				pAuxGeom->SetRenderFlags(currentRenderFlags);
			}
		}
	}
}
#endif // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}      // namespace DefaultComponents
}      // namespace Audio
}      // namespace Cry
