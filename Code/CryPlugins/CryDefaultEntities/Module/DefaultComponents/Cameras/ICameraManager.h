#pragma once

struct ICameraComponent : public IEntityComponent
{
	static void ReflectType(Schematyc::CTypeDesc<ICameraComponent>& desc)
	{
		desc.SetGUID("{42D1F269-CED6-4504-8092-1651B7645594}"_cry_guid);
	}

	virtual void DisableAudioListener() = 0;
};

class ICameraManager
{
public:
	virtual void AddCamera(ICameraComponent* pComponent) = 0;
	virtual void SwitchCameraToActive(ICameraComponent* pComponent) = 0;
	virtual void RemoveCamera(ICameraComponent* pComponent) = 0;
	virtual bool IsThisCameraActive(const ICameraComponent* pComponent) = 0;
};
