#include "StdAfx.h"
#include "Rifle.h"

#include "GamePlugin.h"

class CRifleRegistrator
	: public IEntityRegistrator
{
	virtual void Register() override
	{
		CGamePlugin::RegisterEntityWithDefaultComponent<CRifle>("Rifle");

		RegisterCVars();
	}

	virtual void Unregister() override
	{
		UnregisterCVars();
	}

	void RegisterCVars()
	{
		REGISTER_CVAR2("w_rifleBulletScale", &m_bulletScale, 0.05f, VF_CHEAT, "Determines the scale of the bullet geometry");
	}

	void UnregisterCVars()
	{
		IConsole* pConsole = gEnv->pConsole;
		if (pConsole)
		{
			pConsole->UnregisterVariable("w_rifleBulletScale");
		}
	}

public:
	float m_bulletScale;
};

CRifleRegistrator g_rifleRegistrator;

void CRifle::RequestFire(const Vec3 &initialBulletPosition, const Quat &initialBulletRotation)
{
	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Bullet");

	spawnParams.vPosition = initialBulletPosition;
	spawnParams.qRotation = initialBulletRotation;

	spawnParams.vScale = Vec3(g_rifleRegistrator.m_bulletScale);

	// Spawn the entity, bullet is propelled in CBullet based on the rotation and position here
	gEnv->pEntitySystem->SpawnEntity(spawnParams);
}