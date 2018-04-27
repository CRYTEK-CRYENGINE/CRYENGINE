#pragma once

#include <vector>
#include "ICameraManager.h"
#include "CameraComponent.h"

class CCameraManager : public ICameraManager
{
public:
	CCameraManager() = default;
	virtual ~CCameraManager() {};

	virtual void AddCamera(ICameraComponent* pComponent) override
	{
		if (pComponent->GetEntity()->GetSimulationMode() != EEntitySimulationMode::Preview)
		{
			m_Cameras.push_back(pComponent);
		}
	}
	
	virtual void SwitchCameraToActive(ICameraComponent* pComponent) override
	{
		if (pComponent->GetEntity()->GetSimulationMode() == EEntitySimulationMode::Preview)
			return;

		if (m_pActive)
		{
			m_pActive->DisableAudioListener();
		}

		m_pActive = pComponent;

		for (const ICameraComponent* component : m_Cameras)
		{
			component->GetEntity()->UpdateComponentEventMask(component);
		}
	}

	virtual void RemoveCamera(ICameraComponent* pComponent) override
	{
		if (pComponent->GetEntity()->GetSimulationMode() != EEntitySimulationMode::Preview)
		{
			if (pComponent == m_pActive)
			{
				m_pActive = nullptr;
			}

			stl::find_and_erase(m_Cameras, pComponent);
		}
	}

	virtual bool IsThisCameraActive(const ICameraComponent* pComponent) override
	{
		return (pComponent == m_pActive);
	}

private:
	std::vector<ICameraComponent*> m_Cameras;
	ICameraComponent* m_pActive = nullptr;
};