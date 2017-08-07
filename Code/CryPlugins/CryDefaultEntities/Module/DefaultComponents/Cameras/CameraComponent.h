#pragma once

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>
#include <CryExtension/ICryPluginManager.h>

#include "PluginDll.h"

namespace Cry
{
	namespace DefaultComponents
	{
		class CCameraComponent
			: public IEntityComponent
			, public IHmdDevice::IAsyncCameraCallback
			, public IEntityEventListener
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() override
			{
				m_pCameraManager->AddCamera(this);

				if (m_bActivateOnCreate)
				{
					Activate();
				}
			}

			virtual void ProcessEvent(SEntityEvent& event) override
			{
				if (event.event == ENTITY_EVENT_UPDATE)
				{
					const CCamera& systemCamera = gEnv->pSystem->GetViewCamera();

					const float farPlane = gEnv->p3DEngine->GetMaxViewDistance();

					m_camera.SetFrustum(systemCamera.GetViewSurfaceX(), systemCamera.GetViewSurfaceZ(), m_fieldOfView.ToRadians(), m_nearPlane, farPlane, systemCamera.GetPixelAspectRatio());
					m_camera.SetMatrix(GetWorldTransformMatrix());

					gEnv->pSystem->SetViewCamera(m_camera);

					if (m_bAutomaticAudioListenerPosition && m_pAudioListener)
					{
						// Make sure we update the audio listener position
						m_pAudioListener->SetWorldTM(m_camera.GetMatrix());
					}
				}
				else if (event.event == ENTITY_EVENT_START_GAME || event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
				{
					if (m_bActivateOnCreate && !IsActive())
					{
						Activate();
					}
				}
			}

			virtual uint64 GetEventMask() const override
			{
				uint64 bitFlags = IsActive() ? BIT64(ENTITY_EVENT_UPDATE) : 0;
				bitFlags |= BIT64(ENTITY_EVENT_START_GAME) | BIT64(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

				return bitFlags;
			}
			// ~IEntityComponent

			// IAsyncCameraCallback
			virtual bool OnAsyncCameraCallback(const HmdTrackingState& sensorState, IHmdDevice::AsyncCameraContext& context) override
			{
				context.outputCameraMatrix = GetWorldTransformMatrix();

				Matrix33 orientation = Matrix33(context.outputCameraMatrix);
				Vec3 position = context.outputCameraMatrix.GetTranslation();

				context.outputCameraMatrix.AddTranslation(orientation * sensorState.pose.position);
				context.outputCameraMatrix.SetRotation33(orientation * Matrix33(sensorState.pose.orientation));

				return true;
			}
			// ~IAsyncCameraCallback

			// IEntityEventListener
			virtual void OnEntityEvent(IEntity* pEntity, SEntityEvent& event) override
			{
				switch (event.event)
				{
				case ENTITY_EVENT_DONE:
				{
					// In case something destroys our listener entity before we had the chance to remove it.
					if ((m_pAudioListener != nullptr) && (pEntity->GetId() == m_pAudioListener->GetId()))
					{
						gEnv->pEntitySystem->RemoveEntityEventListener(m_pAudioListener->GetId(), ENTITY_EVENT_DONE, this);
						m_pAudioListener = nullptr;
					}

					break;
				}
				}
			}
			// ~IEntityEventListener

		public:
			CCameraComponent()
			{
				m_pCameraManager = gEnv->pSystem->GetIPluginManager()->QueryPlugin<IPlugin_CryDefaultEntities>()->GetICameraManager();
			}

			virtual ~CCameraComponent()
			{
				m_pCameraManager->RemvoeCamera(this);

				if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
				{
					pDevice->SetAsynCameraCallback(nullptr);
				}

				if (m_pAudioListener != nullptr)
				{
					gEnv->pEntitySystem->RemoveEntityEventListener(m_pAudioListener->GetId(), ENTITY_EVENT_DONE, this);
					gEnv->pEntitySystem->RemoveEntity(m_pAudioListener->GetId(), true);
					m_pAudioListener = nullptr;
				}
			}
			
			static void ReflectType(Schematyc::CTypeDesc<CCameraComponent>& desc)
			{
				desc.SetGUID("{A4CB0508-5F07-46B4-B6D4-AB76BFD550F4}"_cry_guid);
				desc.SetEditorCategory("Cameras");
				desc.SetLabel("Camera");
				desc.SetDescription("Represents a camera that can be activated to render to screen");
				desc.SetIcon("icons:General/Camera.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

				desc.AddMember(&CCameraComponent::m_bActivateOnCreate, 'actv', "Active", "Active", "Whether or not this camera should be activated on component creation", true);
				desc.AddMember(&CCameraComponent::m_nearPlane, 'near', "NearPlane", "Near Plane", nullptr, 0.25f);
				desc.AddMember(&CCameraComponent::m_fieldOfView, 'fov', "FieldOfView", "Field of View", nullptr, 75.0_degrees);
				desc.AddMember(&CCameraComponent::m_bAutomaticAudioListenerPosition, 'audi', "AutoAudioListenerPos", "Automatic Audio Listener", "If true, automatically moves the audio listener with the entity.", true);
			}

			virtual void Activate()
			{
				if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
				{
					pDevice->SetAsynCameraCallback(this);
				}

				if (m_pAudioListener == nullptr)
				{
					// Spawn the audio listener
					SEntitySpawnParams entitySpawnParams;
					entitySpawnParams.sName = "AudioListener";
					entitySpawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

					// We don't want the audio listener to serialize as the entity gets completely removed and recreated during save/load!
					// NOTE: If we set ENTITY_FLAG_NO_SAVE *after* we spawn the entity, it will make it to m_dynamicEntities in GameSerialize.cpp
					// (via CGameSerialize::OnSpawn) and GameSerialize will attempt to serialize it despite the flag with current (5.2.2) implementation
					entitySpawnParams.nFlags = ENTITY_FLAG_TRIGGER_AREAS | ENTITY_FLAG_NO_SAVE;
					entitySpawnParams.nFlagsExtended = ENTITY_FLAG_EXTENDED_AUDIO_LISTENER;

					m_pAudioListener = gEnv->pEntitySystem->SpawnEntity(entitySpawnParams);
					if (m_pAudioListener != nullptr)
					{
						gEnv->pEntitySystem->AddEntityEventListener(m_pAudioListener->GetId(), ENTITY_EVENT_DONE, this);
						CryFixedStringT<64> sTemp;
						sTemp.Format("AudioListener(%d)", static_cast<int>(m_pAudioListener->GetId()));
						m_pAudioListener->SetName(sTemp.c_str());

						IEntityAudioComponent* pIEntityAudioComponent = m_pAudioListener->GetOrCreateComponent<IEntityAudioComponent>();
						CRY_ASSERT(pIEntityAudioComponent);
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "<Audio>: Audio listener creation failed in CCameraComponent::CreateAudioListener!");
					}
				}
				else
				{
					m_pAudioListener->SetFlagsExtended(m_pAudioListener->GetFlagsExtended() | ENTITY_FLAG_EXTENDED_AUDIO_LISTENER);
					m_pAudioListener->InvalidateTM(ENTITY_XFORM_POS);
				}

				m_pCameraManager->SwitchCameraToActive(this);
			}

			virtual void OverrideAudioListenerTransform(const CryTransform::CTransform& transform)
			{
				m_pAudioListener->SetWorldTM(transform.ToMatrix34());

				m_bAutomaticAudioListenerPosition = false;
			}

			bool IsActive() const
			{
				return m_pCameraManager->IsThisCameraActive(this);
			}

			void UnregisterAudioListener()
			{
				if (m_pAudioListener != nullptr)
				{
					gEnv->pEntitySystem->GetAreaManager()->ExitAllAreas(m_pAudioListener->GetId());
					m_pAudioListener->SetFlagsExtended(m_pAudioListener->GetFlagsExtended() & ~ENTITY_FLAG_EXTENDED_AUDIO_LISTENER);
				}
			}

			virtual void EnableAutomaticActivation(bool bActivate) { m_bActivateOnCreate = bActivate; }
			bool IsAutomaticallyActivated() const { return m_bActivateOnCreate; }

			virtual void SetNearPlane(float nearPlane) { m_nearPlane = nearPlane; }
			float GetNearPlane() const { return m_nearPlane; }

			virtual void SetFieldOfView(CryTransform::CAngle angle) { m_fieldOfView = angle; }
			CryTransform::CAngle GetFieldOfView() const { return m_fieldOfView; }

			virtual void EnableAutomaticAudioListener(bool bEnable) { m_bAutomaticAudioListenerPosition = bEnable; }
			bool HasAutomaticAudioListener() const { return m_bAutomaticAudioListenerPosition; }

			virtual CCamera& GetCamera() { return m_camera; }
			const CCamera& GetCamera() const { return m_camera; }

		protected:
			bool m_bActivateOnCreate = true;
			Schematyc::Range<0, 32768> m_nearPlane = 0.25f;
			CryTransform::CClampedAngle<20, 360> m_fieldOfView = 75.0_degrees;

			bool m_bAutomaticAudioListenerPosition = true;

			ICameraManager* m_pCameraManager = nullptr;

			CCamera m_camera;
			IEntity* m_pAudioListener = nullptr;
		};
	}
}