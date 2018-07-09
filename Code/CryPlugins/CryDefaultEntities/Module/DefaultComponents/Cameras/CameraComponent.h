#pragma once

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>
#include "../Audio/ListenerComponent.h"
#include <CrySystem/ICryPluginManager.h>

#include "../../IDefaultComponentsPlugin.h"
#include "ICameraManager.h"

namespace Cry
{
	namespace DefaultComponents
	{
		enum class ECameraType
		{
			Default = 0,
			Omnidirectional
		};

		static void ReflectType(Schematyc::CTypeDesc<ECameraType>& desc)
		{
			desc.SetGUID("{E4DC6A37-2FAE-4DFF-AB21-3C4308A266D6}"_cry_guid);
			desc.SetLabel("Camera Type");
			desc.SetDefaultValue(ECameraType::Default);
			desc.AddConstant(ECameraType::Default, "Default", "Default");
			desc.AddConstant(ECameraType::Omnidirectional, "Omnidirectional", "Omnidirectional");
		}

		class CCameraComponent
			: public ICameraComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() override
			{
				m_pCameraManager->AddCamera(this);

				m_pAudioListener = m_pEntity->GetOrCreateComponent<Cry::Audio::DefaultComponents::CListenerComponent>();
				CRY_ASSERT(m_pAudioListener != nullptr);
				m_pAudioListener->SetComponentFlags(m_pAudioListener->GetComponentFlags() | IEntityComponent::EFlags::UserAdded);

				if (m_bActivateOnCreate)
				{
					Activate();
				}
			}

		virtual void ProcessEvent(const SEntityEvent& event) override
		{
			// We don't update the camera if we are in edit mode otherwise we would override the camera transformation of the editor camera.
			if (event.event == ENTITY_EVENT_UPDATE && !gEnv->IsEditing())
			{
				const CCamera& systemCamera = gEnv->pSystem->GetViewCamera();

				m_camera.SetFrustum(systemCamera.GetViewSurfaceX(), systemCamera.GetViewSurfaceZ(), m_fieldOfView.ToRadians(), m_nearPlane, m_farPlane, systemCamera.GetPixelAspectRatio());
				m_camera.SetMatrix(GetWorldTransformMatrix());

				gEnv->pSystem->SetViewCamera(m_camera);

				if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
				{
					const auto& worldTranform = GetWorldTransformMatrix();
					pDevice->EnableLateCameraInjectionForCurrentFrame(std::make_pair(Quat(worldTranform), worldTranform.GetTranslation()));
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
				uint64 bitFlags = IsActive() ? ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE) : 0;
				bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) | ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

				return bitFlags;
			}

#ifndef RELEASE
			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
#endif

			virtual void OnShutDown() final
			{
				m_pCameraManager->RemoveCamera(this);
			}
			// ~IEntityComponent

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}
			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			// ~IEntityComponentPreviewer
#endif

			// ICameraComponent
			virtual void DisableAudioListener() final
			{
				m_pAudioListener->SetActive(false);
			}
			// ~ICameraComponent

		public:
			CCameraComponent()
			{
				m_pCameraManager = gEnv->pSystem->GetIPluginManager()->QueryPlugin<IPlugin_CryDefaultEntities>()->GetICameraManager();
			}

			virtual ~CCameraComponent() = default;

			static void ReflectType(Schematyc::CTypeDesc<CCameraComponent>& desc)
			{
				desc.SetGUID("{A4CB0508-5F07-46B4-B6D4-AB76BFD550F4}"_cry_guid);
				desc.SetEditorCategory("Cameras");
				desc.SetLabel("Camera");
				desc.SetDescription("Represents a camera that can be activated to render to screen");
				desc.SetIcon("icons:General/Camera.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

				desc.AddMember(&CCameraComponent::m_type, 'type', "Type", "Type", "Method of rendering to use for the camera", ECameraType::Default);
				desc.AddMember(&CCameraComponent::m_bActivateOnCreate, 'actv', "Active", "Active", "Whether or not this camera should be activated on component creation", true);
				desc.AddMember(&CCameraComponent::m_nearPlane, 'near', "NearPlane", "Near Plane", nullptr, 0.25f);
				desc.AddMember(&CCameraComponent::m_farPlane, 'far', "FarPlane", "Far Plane", nullptr, 1024.f);
				desc.AddMember(&CCameraComponent::m_fieldOfView, 'fov', "FieldOfView", "Field of View", nullptr, 70.0_degrees);
			}

			virtual void Activate()
			{
				m_pEntity->UpdateComponentEventMask(this);
				
				if (m_pAudioListener)
				{
					m_pAudioListener->SetActive(true);
				}

				m_pCameraManager->SwitchCameraToActive(this);
			}

			bool IsActive() const
			{
				return m_pCameraManager->IsThisCameraActive(this);
			}

			virtual void EnableAutomaticActivation(bool bActivate) { m_bActivateOnCreate = bActivate; }
			bool IsAutomaticallyActivated() const { return m_bActivateOnCreate; }

			virtual void SetNearPlane(float nearPlane) { m_nearPlane = nearPlane; }
			float GetNearPlane() const { return m_nearPlane; }

			virtual void SetFarPlane(float farPlane) { m_farPlane = farPlane; }
			float GetFarPlane() const { return m_farPlane; }

			virtual void SetFieldOfView(CryTransform::CAngle angle) { m_fieldOfView = angle; }
			CryTransform::CAngle GetFieldOfView() const { return m_fieldOfView; }

			ECameraType GetType() const { return m_type; }

			virtual CCamera& GetCamera() { return m_camera; }
			const CCamera& GetCamera() const { return m_camera; }

		protected:
			ECameraType m_type = ECameraType::Default;
			bool m_bActivateOnCreate = true;
			Schematyc::Range<0, 32768> m_nearPlane = 0.25f;
			Schematyc::Range<0, 32768> m_farPlane = 1024;
			CryTransform::CClampedAngle<20, 360> m_fieldOfView = 70.0_degrees;

			ICameraManager* m_pCameraManager = nullptr;

			CCamera m_camera;
			Cry::Audio::DefaultComponents::CListenerComponent* m_pAudioListener = nullptr;
		};
	}
}