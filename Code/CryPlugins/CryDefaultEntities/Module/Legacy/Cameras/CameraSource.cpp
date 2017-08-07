#include "StdAfx.h"
#include "CameraSource.h"

class CCameraSourceRegistrator
	: public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("CameraSource") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class CameraSource, overridden by game");
			return;
		}

		RegisterEntityWithDefaultComponent<CCameraSource>("CameraSource", "Other", "Camera.bmp");
	}
};

CCameraSourceRegistrator g_cameraSourceRegistrator;

CRYREGISTER_CLASS(CCameraSource);

void CCameraSource::Initialize()
{
	GetEntity()->CreateProxy(ENTITY_PROXY_CAMERA);

	GetEntity()->SetFlags(GetEntity()->GetFlags() | ENTITY_FLAG_CLIENT_ONLY);
}