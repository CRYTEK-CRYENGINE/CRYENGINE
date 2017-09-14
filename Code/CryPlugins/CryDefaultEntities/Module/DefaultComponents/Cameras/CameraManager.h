#pragma once

#include <vector>
#include "ICameraManager.h"
#include "CameraComponent.h"

class CCameraManager : public ICameraManager
{
public:
	CCameraManager() = default;
	virtual ~CCameraManager() {};

	virtual void AddCamera(Cry::DefaultComponents::CCameraComponent* pComponent) override
	{
		if (pComponent->GetEntity()->GetSimulationMode() != EEntitySimulationMode::Preview)
		{
			m_Cameras.push_back(pComponent);
		}
	}
	
	virtual void SwitchCameraToActive(Cry::DefaultComponents::CCameraComponent* pComponent) override
	{
		if (pComponent->GetEntity()->GetSimulationMode() == EEntitySimulationMode::Preview)
			return;

		if (m_pActive)
		{
			m_pActive->DisableAudioListener();
		}

		m_pActive = pComponent;

		for (const Cry::DefaultComponents::CCameraComponent* component : m_Cameras)
		{
			component->GetEntity()->UpdateComponentEventMask(component);
		}
	}
	virtual void RemoveCamera(Cry::DefaultComponents::CCameraComponent* pComponent) override
	{
		if (pComponent->GetEntity()->GetSimulationMode() != EEntitySimulationMode::Preview)
		{
			stl::find_and_erase(m_Cameras, pComponent);
		}
	}

	virtual bool IsThisCameraActive(const Cry::DefaultComponents::CCameraComponent* pComponent) override
	{
		return (pComponent == m_pActive);
	}

private:
	std::vector<Cry::DefaultComponents::CCameraComponent*> m_Cameras;
	Cry::DefaultComponents::CCameraComponent* m_pActive = nullptr;

};