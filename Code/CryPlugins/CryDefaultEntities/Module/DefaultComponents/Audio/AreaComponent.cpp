// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AreaComponent.h"

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void CAreaComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto const pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAreaComponent::SetActive, "AFA6F8A9-8D06-4C51-A1E0-E3D3FA3B9458"_cry_guid, "SetActive");
		pFunction->SetDescription("Enables/Disables the component.");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'val', "Activate");
		componentScope.Register(pFunction);
	}
	{
		auto const pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAreaComponent::SetTriggeringObjects, "DB43E668-F23B-40B1-A2C8-4801CD2CFA55"_cry_guid, "SetTriggeringObjects");
		pFunction->SetDescription("Allows for setting the object type this component reacts to.");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		pFunction->BindInput(1, 'trio', "Triggering Objects");
		componentScope.Register(pFunction);
	}

	componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(CAreaComponent::SNewFadeValueSignal));
	componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(CAreaComponent::SOnFarToNearSignal));
	componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(CAreaComponent::SOnInsideToNearSignal));
	componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(CAreaComponent::SOnNearToFarSignal));
	componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(CAreaComponent::SOnNearToInsideSignal));
}

//////////////////////////////////////////////////////////////////////////
void CAreaComponent::ReflectType(Schematyc::CTypeDesc<CAreaComponent>& desc)
{
	desc.SetGUID("E57B86BA-9F4D-4EDA-94D2-1E20E4CAE94F"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("Area");
	desc.SetDescription("Area component which delivers positional data in regards to entities interacting with the area this component is attached to.");
	desc.SetIcon("icons:Audio/component_area.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector });

	desc.AddMember(&CAreaComponent::m_fadeDistance, 'fade', "fadeDistance", "FadeDistance", "The distance over which is faded.", 0.0f);
}

//////////////////////////////////////////////////////////////////////////
static void ReflectType(Schematyc::CTypeDesc<CAreaComponent::ETriggeringObjects>& desc)
{
	desc.SetGUID("D69EC5CF-FEC0-4CE7-A1CB-EC6CDC391D34"_cry_guid);
	desc.SetLabel("Triggering Objects");
	desc.SetDescription("The type of object that is considered by this component.");
	desc.SetFlags(Schematyc::ETypeFlags::Switchable);
	desc.SetDefaultValue(CAreaComponent::ETriggeringObjects::AudioListeners);
	desc.AddConstant(CAreaComponent::ETriggeringObjects::AudioListeners, "audioListeners", "AudioListeners");
	desc.AddConstant(CAreaComponent::ETriggeringObjects::Entities, "entities", "Entities");
}

//////////////////////////////////////////////////////////////////////////
static void ReflectType(Schematyc::CTypeDesc<CAreaComponent::SNewFadeValueSignal>& desc)
{
	desc.SetGUID("B8D5D205-2DAC-47FF-B25C-FC1C9ABB78F6"_cry_guid);
	desc.SetLabel("OnNewFadeValue");
	desc.AddMember(&CAreaComponent::SNewFadeValueSignal::value, 'val', "value", "Value", "The normalized fade value.", 0.0f);
}

//////////////////////////////////////////////////////////////////////////
static void ReflectType(Schematyc::CTypeDesc<CAreaComponent::SOnFarToNearSignal>& desc)
{
	desc.SetGUID("9EB663D0-8A91-48AE-AFC2-F16FFA6DB7A7"_cry_guid);
	desc.SetLabel("OnFarToNear");
}

//////////////////////////////////////////////////////////////////////////
static void ReflectType(Schematyc::CTypeDesc<CAreaComponent::SOnInsideToNearSignal>& desc)
{
	desc.SetGUID("2E41D765-1ECC-4192-A0DB-413BAD282519"_cry_guid);
	desc.SetLabel("OnInsideToNear");
}

//////////////////////////////////////////////////////////////////////////
static void ReflectType(Schematyc::CTypeDesc<CAreaComponent::SOnNearToFarSignal>& desc)
{
	desc.SetGUID("F094BA36-9F43-4BEC-8ED1-1141588CB872"_cry_guid);
	desc.SetLabel("OnNearToFar");
}

//////////////////////////////////////////////////////////////////////////
static void ReflectType(Schematyc::CTypeDesc<CAreaComponent::SOnNearToInsideSignal>& desc)
{
	desc.SetGUID("F13D7D2D-DEF2-496F-BA05-939659118C8B"_cry_guid);
	desc.SetLabel("OnNearToInside");
}

//////////////////////////////////////////////////////////////////////////
void CAreaComponent::Initialize()
{
	auto pEAC = m_pEntity->GetOrCreateComponent<IEntityAudioComponent>();
	pEAC->SetFadeDistance(m_fadeDistance);
	gEnv->pEntitySystem->GetAreaManager()->SetAreasDirty();
}

//////////////////////////////////////////////////////////////////////////
uint64 CAreaComponent::GetEventMask() const
{
	return
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	  ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED) |
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
	  ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERNEARAREA) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_MOVENEARAREA) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVENEARAREA) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_MOVEINSIDEAREA) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA);
}

//////////////////////////////////////////////////////////////////////////
void CAreaComponent::ProcessEvent(const SEntityEvent& event)
{
	EntityId const entityId = static_cast<EntityId>(event.nParam[0]);
	IEntity* const pIEntity = gEnv->pEntitySystem->GetEntity(entityId);

	if (pIEntity != nullptr)
	{
		bool const bIsAudioListener = (m_triggeringObjects == ETriggeringObjects::AudioListeners && (pIEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) != 0);
		bool const bIsAreaTriggeringEntity = (m_triggeringObjects == ETriggeringObjects::Entities && (pIEntity->GetFlags() & ENTITY_FLAG_TRIGGER_AREAS) != 0);

		if (bIsAudioListener || bIsAreaTriggeringEntity)
		{
			Schematyc::IObject* const pSchematycObject = m_pEntity->GetSchematycObject();

			switch (event.event)
			{
			case ENTITY_EVENT_ENTERNEARAREA:
				{
					m_state = EState::Near;
					m_fadeValue = 0.0f;

					if (m_bActive)
					{
						if (bIsAudioListener)
						{
							m_pEntity->SetPos(event.vec);
						}

						if (pSchematycObject != nullptr)
						{
							pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());
							pSchematycObject->ProcessSignal(SOnFarToNearSignal(), GetGUID());
						}
					}
				}
				break;
			case ENTITY_EVENT_MOVENEARAREA:
				{
					if (m_bActive)
					{
						if (bIsAudioListener)
						{
							m_pEntity->SetPos(event.vec);
						}

						if (m_fadeDistance > 0.0f)
						{
							float fadeValue = (m_fadeDistance - event.fParam[0]) / m_fadeDistance;
							fadeValue = (fadeValue > 0.0f) ? fadeValue : 0.0f;

							if (fabsf(m_fadeValue - fadeValue) > FadeValueEpsilon)
							{
								m_fadeValue = fadeValue;

								if (pSchematycObject != nullptr)
								{
									pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());
								}
							}
						}
					}
				}
				break;
			case ENTITY_EVENT_LEAVENEARAREA:
				{
					m_fadeValue = 0.0f;

					if (m_bActive)
					{
						if (pSchematycObject != nullptr)
						{
							pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());
							pSchematycObject->ProcessSignal(SOnNearToFarSignal(), GetGUID());
						}
					}
				}
				break;
			case ENTITY_EVENT_MOVEINSIDEAREA:
				{
					if (m_bActive)
					{
						EntityId const area1Id = static_cast<EntityId>(event.nParam[2]);
						IEntity* const __restrict pAreaLow = gEnv->pEntitySystem->GetEntity(area1Id);

						if (pAreaLow != nullptr)
						{
							EntityId const area2Id = static_cast<EntityId>(event.nParam[3]);
							IEntity* const __restrict pAreaHigh = gEnv->pEntitySystem->GetEntity(area2Id);

							if (pAreaHigh != nullptr)
							{
								IEntityAreaComponent const* const __restrict pAreaProxyLow = static_cast<IEntityAreaComponent const* const __restrict>(pAreaLow->GetProxy(ENTITY_PROXY_AREA));
								IEntityAreaComponent* const __restrict pAreaProxyHigh = static_cast<IEntityAreaComponent* const __restrict>(pAreaHigh->GetProxy(ENTITY_PROXY_AREA));

								if (pAreaProxyLow != nullptr && pAreaProxyHigh != nullptr)
								{
									Vec3 const pos(pIEntity->GetWorldPos());
									bool const bInsideLow = pAreaProxyLow->CalcPointWithin(entityId, pos);

									if (bInsideLow)
									{
										float const fadeValue = event.fParam[0];

										if (fadeValue == m_fadeValue)
										{
											if (bIsAudioListener)
											{
												m_pEntity->SetPos(pos);
											}
										}
										else
										{
											// Fading the outer object depending on the "InnerFadeDistance" set to an inner, higher priority area.
											Vec3 onHighHull3d(ZERO);
											pAreaProxyHigh->ClosestPointOnHullDistSq(entityId, pos, onHighHull3d);

											if (bIsAudioListener)
											{
												m_pEntity->SetPos(onHighHull3d);
											}

											if (fadeValue == 0.0f)
											{
												// Fading stopped, pretend that we have left the area even though that we're technically still in it.
												m_fadeValue = fadeValue;

												if (pSchematycObject != nullptr)
												{
													pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());
													pSchematycObject->ProcessSignal(SOnInsideToNearSignal(), GetGUID());
													pSchematycObject->ProcessSignal(SOnNearToFarSignal(), GetGUID());
												}
											}
											else if (m_fadeValue == 0.0f)
											{
												// Fading started, pretend that we have entered the area even though we technically never left it.
												m_fadeValue = fadeValue;

												if (pSchematycObject != nullptr)
												{
													pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());
													pSchematycObject->ProcessSignal(SOnFarToNearSignal(), GetGUID());
													pSchematycObject->ProcessSignal(SOnNearToInsideSignal(), GetGUID());
												}
											}

											if (fabsf(m_fadeValue - fadeValue) > FadeValueEpsilon)
											{
												m_fadeValue = fadeValue;

												if (pSchematycObject != nullptr)
												{
													pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());
												}
											}
										}
									}
								}
							}
							else
							{
								if (m_fadeValue == 0.0f)
								{
									// The listener crossed into this area from a higher priority area of same group.
									// Pretend that we have entered the area even though we technically never left it.
									m_fadeValue = 1.0f;

									if (pSchematycObject != nullptr)
									{
										pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());
										pSchematycObject->ProcessSignal(SOnFarToNearSignal(), GetGUID());
										pSchematycObject->ProcessSignal(SOnNearToInsideSignal(), GetGUID());
									}
								}

								if (bIsAudioListener)
								{
									m_pEntity->SetPos(event.vec);
								}
							}
						}
					}
				}
				break;
			case ENTITY_EVENT_ENTERAREA:
				{
					m_state = EState::Inside;
					m_fadeValue = 1.0f;

					if (m_bActive)
					{
						if (bIsAudioListener)
						{
							m_pEntity->SetPos(pIEntity->GetWorldPos());
						}

						if (pSchematycObject != nullptr)
						{
							pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());
							pSchematycObject->ProcessSignal(SOnNearToInsideSignal(), GetGUID());
						}
					}
				}
				break;
			case ENTITY_EVENT_LEAVEAREA:
				{
					m_state = EState::Near;
					bool bWasFadedOut = false;

					if (m_fadeValue == 0.0f)
					{
						bWasFadedOut = true;
						m_fadeValue = 1.0f;
					}

					if (m_bActive)
					{
						if (pSchematycObject != nullptr)
						{
							pSchematycObject->ProcessSignal(SNewFadeValueSignal(m_fadeValue), GetGUID());

							if (bWasFadedOut)
							{
								// Pretend that we just entered the near margin so that the audio gets restarted.
								pSchematycObject->ProcessSignal(SOnFarToNearSignal(), GetGUID());
							}

							pSchematycObject->ProcessSignal(SOnInsideToNearSignal(), GetGUID());
						}
					}
				}
				break;
			}
		}
	}

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		auto pEAC = m_pEntity->GetOrCreateComponent<IEntityAudioComponent>();
		pEAC->SetFadeDistance(m_fadeDistance);
		gEnv->pEntitySystem->GetAreaManager()->SetAreasDirty();
	}
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CAreaComponent::SetActive(bool const bValue)
{
	if (m_bActive != bValue)
	{
		m_bActive = bValue;

		Schematyc::IObject* const pSchematycObject = m_pEntity->GetSchematycObject();

		if (pSchematycObject != nullptr)
		{
			switch (m_state)
			{
			case EState::Near:
				{
					if (m_bActive)
					{
						if (m_previousState == EState::Inside)
						{
							pSchematycObject->ProcessSignal(SOnInsideToNearSignal(), GetGUID());
						}
						else
						{
							pSchematycObject->ProcessSignal(SOnFarToNearSignal(), GetGUID());
						}
					}
					else
					{
						// Store the state at the time of deactivation.
						m_previousState = m_state;
						pSchematycObject->ProcessSignal(SOnNearToFarSignal(), GetGUID());
					}
				}
				break;
			case EState::Inside:
				{
					if (m_bActive)
					{
						if (m_previousState == EState::Near)
						{
							pSchematycObject->ProcessSignal(SOnNearToInsideSignal(), GetGUID());
						}
						else
						{
							pSchematycObject->ProcessSignal(SOnFarToNearSignal(), GetGUID());
							pSchematycObject->ProcessSignal(SOnNearToInsideSignal(), GetGUID());
						}
					}
					else
					{
						// Store the state at the time of deactivation.
						m_previousState = m_state;
						pSchematycObject->ProcessSignal(SOnInsideToNearSignal(), GetGUID());
						pSchematycObject->ProcessSignal(SOnNearToFarSignal(), GetGUID());
					}
				}
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaComponent::SetTriggeringObjects(ETriggeringObjects const triggeringObjects)
{
	m_triggeringObjects = triggeringObjects;
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
