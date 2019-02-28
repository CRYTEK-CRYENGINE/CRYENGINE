#pragma once

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>
#include "../../Audio/ListenerComponent.h"
#include <CrySystem/ICryPluginManager.h>

namespace Cry
{
	namespace DefaultComponents
	{
		namespace VirtualReality
		{
			class CRoomscaleCameraComponent
				: public ICameraComponent
#ifndef RELEASE
				, public IEntityComponentPreviewer
#endif
			{
			protected:
				friend CPlugin_CryDefaultEntities;
				static void Register(Schematyc::CEnvRegistrationScope& componentScope);

				// IEntityComponent
				virtual void Initialize() override;

				virtual void ProcessEvent(const SEntityEvent& event) override;
				virtual uint64 GetEventMask() const override;

#ifndef RELEASE
				virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
#endif

				virtual void OnShutDown() override;
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
				CRoomscaleCameraComponent();

				virtual ~CRoomscaleCameraComponent() = default;

				static void ReflectType(Schematyc::CTypeDesc<CRoomscaleCameraComponent>& desc)
				{
					desc.SetGUID("{F30CEF6C-C845-4833-BC58-EAF64A14FE90}"_cry_guid);
					desc.SetEditorCategory("Cameras");
					desc.SetLabel("Roomscale VR Camera");
					desc.SetDescription("Used for allowing Roomscale Virtual Reality gameplay");
					desc.SetIcon("icons:General/Camera.ico");
					desc.SetComponentFlags({ IEntityComponent::EFlags::ClientOnly });

					desc.AddMember(&CRoomscaleCameraComponent::m_bActivateOnCreate, 'actv', "Active", "Active", "Whether or not this camera should be activated on component creation", true);
					desc.AddMember(&CRoomscaleCameraComponent::m_nearPlane, 'near', "NearPlane", "Near Plane", nullptr, 0.25f);
					desc.AddMember(&CRoomscaleCameraComponent::m_farPlane, 'far', "FarPlane", "Far Plane", nullptr, 1024.f);
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

				virtual CCamera& GetCamera() { return m_camera; }
				const CCamera& GetCamera() const { return m_camera; }

			protected:
				bool m_bActivateOnCreate = true;
				Schematyc::Range<0, 32768> m_nearPlane = 0.25f;
				Schematyc::Range<0, 32768> m_farPlane = 1024;

				ICameraManager* m_pCameraManager = nullptr;

				CCamera m_camera;
				Cry::Audio::DefaultComponents::CListenerComponent* m_pAudioListener = nullptr;
			};
		}
	}
}