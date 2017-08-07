#pragma once
namespace Cry { namespace DefaultComponents { class CCameraComponent; } }

class ICameraManager
{
public:
	virtual void AddCamera(Cry::DefaultComponents::CCameraComponent* pComponent) = 0;
	virtual void SwitchCameraToActive(Cry::DefaultComponents::CCameraComponent* pComponent) = 0;
	virtual void RemvoeCamera(Cry::DefaultComponents::CCameraComponent* pComponent) = 0;
	virtual bool IsThisCameraActive(const Cry::DefaultComponents::CCameraComponent* pComponent) = 0;
};
