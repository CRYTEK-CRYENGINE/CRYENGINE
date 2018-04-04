#pragma once

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>
#include "../Audio/ListenerComponent.h"

class CPlugin_CryDefaultEntities;

namespace Cry
{
namespace DefaultComponents
{
class CCameraComponent 
	: public IEntityComponent
	, public IHmdDevice::IAsyncCameraCallback
#ifndef RELEASE
	, public IEntityComponentPreviewer
#endif
{
protected:
	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() final;

	virtual void   ProcessEvent(SEntityEvent& event) final;
	virtual uint64 GetEventMask() const final;

#ifndef RELEASE
	virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
#endif
	// ~IEntityComponent

	// IAsyncCameraCallback
	virtual bool OnAsyncCameraCallback(const HmdTrackingState& state, IHmdDevice::AsyncCameraContext& context) final;
	// ~IAsyncCameraCallback

#ifndef RELEASE
	// IEntityComponentPreviewer
	virtual void SerializeProperties(Serialization::IArchive& archive) final {}

	virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
	// ~IEntityComponentPreviewer
#endif

public:
	virtual ~CCameraComponent() override;

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
	}

	virtual void Activate()
	{
		if (s_pActiveCamera != nullptr)
		{
			CCameraComponent* pPreviousCamera = s_pActiveCamera;
			s_pActiveCamera = nullptr;
			m_pEntity->UpdateComponentEventMask(pPreviousCamera);

			pPreviousCamera->m_pAudioListener->SetActive(false);
		}

		s_pActiveCamera = this;

		if (s_pActiveCamera->m_pAudioListener != nullptr)
		{
			s_pActiveCamera->m_pAudioListener->SetActive(true);
		}
		
		m_pEntity->UpdateComponentEventMask(this);

		if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
		{
			pDevice->SetAsynCameraCallback(this);
		}
	}

	bool                 IsActive() const                           { return s_pActiveCamera == this; }

	virtual void         EnableAutomaticActivation(bool bActivate)  { m_bActivateOnCreate = bActivate; }
	bool                 IsAutomaticallyActivated() const           { return m_bActivateOnCreate; }

	virtual void         SetNearPlane(float nearPlane)              { m_nearPlane = nearPlane; }
	float                GetNearPlane() const                       { return m_nearPlane; }

	virtual void         SetFieldOfView(CryTransform::CAngle angle) { m_fieldOfView = angle; }
	CryTransform::CAngle GetFieldOfView() const                     { return m_fieldOfView; }

	virtual CCamera& GetCamera()                                { return m_camera; }
	const CCamera&   GetCamera() const                          { return m_camera; }

protected:
	bool                                               m_bActivateOnCreate = true;
	Schematyc::Range<0, 32768>                         m_nearPlane = 0.25f;
	CryTransform::CClampedAngle<20, 360>               m_fieldOfView = 75.0_degrees;

	CCamera                                            m_camera;
	Cry::Audio::DefaultComponents::CListenerComponent* m_pAudioListener = nullptr;

	static CCameraComponent*                           s_pActiveCamera;
};
} // namespace DefaultComponents
} // namespace Cry
