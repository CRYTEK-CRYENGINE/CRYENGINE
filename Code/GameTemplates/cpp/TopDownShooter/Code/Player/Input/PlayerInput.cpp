#include "StdAfx.h"
#include "PlayerInput.h"

#include "Player/Player.h"

#include "Entities/Gameplay/Weapons/ISimpleWeapon.h"

#include <CryInput/IHardwareMouse.h>

#include <CryAnimation/ICryAnimation.h>

CPlayerInput::CPlayerInput()
	: m_pCursorEntity(nullptr)
{

}

void CPlayerInput::PostInit(IGameObject *pGameObject)
{
	m_pPlayer = static_cast<CPlayer *>(pGameObject->QueryExtension("Player"));

	// NOTE: Since CRYENGINE 5.3, the game is responsible to initialize the action maps
	IActionMapManager *pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();
	pActionMapManager->InitActionMaps("Libs/config/defaultprofile.xml");
	pActionMapManager->Enable(true);
	pActionMapManager->EnableActionMap("player", true);

	if (IActionMap *pActionMap = pActionMapManager->GetActionMap("player"))
	{
		pActionMap->SetActionListener(GetEntityId());
	}

	GetGameObject()->CaptureActions(this);

	// Make sure that this extension is updated regularly via the Update function below
	GetGameObject()->EnableUpdateSlot(this, 0);

	m_cursorPositionInWorld = ZERO;

	// Populate the action handler callbacks so that we get action map events
	InitializeActionHandler();

	// Spawn the cursor
	SpawnCursorEntity();
}

void CPlayerInput::ProcessEvent(SEntityEvent &event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_RESET:
		{
			// Check if we're entering game mode
			if (event.nParam[0] == 1)
			{
				SpawnCursorEntity();
			}
			else
			{
				// Removed by Sandbox
				m_pCursorEntity = nullptr;
			}
		}
		break;
	}
}

void CPlayerInput::SpawnCursorEntity()
{
	if (m_pCursorEntity)
	{
		gEnv->pEntitySystem->RemoveEntity(m_pCursorEntity->GetId());
	}

	SEntitySpawnParams spawnParams;
	// No need for a special class!
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

	// Spawn the cursor
	m_pCursorEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);

	// Load geometry
	const int geometrySlot = 0;
	m_pCursorEntity->LoadGeometry(geometrySlot, "Objects/Default/primitive_sphere.cgf");

	// Scale the cursor down a bit
	m_pCursorEntity->SetScale(Vec3(0.1f));
	m_pCursorEntity->SetViewDistRatio(255);

	// Load the custom cursor material
	auto *pCursorMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Materials/cursor");
	m_pCursorEntity->SetMaterial(pCursorMaterial);

	// Make sure that bullets are always rendered regardless of distance
	// Ratio is 0 - 255, 255 being 100% visibility
	GetEntity()->SetViewDistRatio(255);
}

void CPlayerInput::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	float mouseX, mouseY;

	gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mouseX, &mouseY);

	// Invert mouse Y
	mouseY = gEnv->pRenderer->GetHeight() - mouseY;

	Vec3 vPos0(0, 0, 0);
	gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 0, &vPos0.x, &vPos0.y, &vPos0.z);

	Vec3 vPos1(0, 0, 0);
	gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 1, &vPos1.x, &vPos1.y, &vPos1.z);

	Vec3 vDir = vPos1 - vPos0;
	vDir.Normalize();

	const auto rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit hit;

	if (gEnv->pPhysicalWorld->RayWorldIntersection(vPos0, vDir * gEnv->p3DEngine->GetMaxViewDistance(), ent_all, rayFlags, &hit, 1))
	{
		m_cursorPositionInWorld = hit.pt;

		if (m_pCursorEntity != nullptr)
		{
			m_pCursorEntity->SetPosRotScale(hit.pt, IDENTITY, m_pCursorEntity->GetScale());
		}
	}
	else
	{
		m_cursorPositionInWorld = ZERO;
	}
}

void CPlayerInput::OnPlayerRespawn()
{
	m_inputFlags = 0;
}

void CPlayerInput::HandleInputFlagChange(EInputFlags flags, int activationMode, EInputFlagType type)
{
	switch (type)
	{
		case eInputFlagType_Hold:
		{
			if (activationMode == eIS_Released)
			{
				m_inputFlags &= ~flags;
			}
			else
			{
				m_inputFlags |= flags;
			}
		}
		break;
		case eInputFlagType_Toggle:
		{
			if (activationMode == eIS_Released)
			{
				// Toggle the bit(s)
				m_inputFlags ^= flags;
			}
		}
		break;
	}
}

void CPlayerInput::InitializeActionHandler()
{
	m_actionHandler.AddHandler(ActionId("moveleft"), &CPlayerInput::OnActionMoveLeft);
	m_actionHandler.AddHandler(ActionId("moveright"), &CPlayerInput::OnActionMoveRight);
	m_actionHandler.AddHandler(ActionId("moveforward"), &CPlayerInput::OnActionMoveForward);
	m_actionHandler.AddHandler(ActionId("moveback"), &CPlayerInput::OnActionMoveBack);

	m_actionHandler.AddHandler(ActionId("shoot"), &CPlayerInput::OnActionShoot);
}

void CPlayerInput::OnAction(const ActionId &action, int activationMode, float value)
{
	// This function is called when inputs trigger action map events
	// The handler below maps the event (see 'action') to a callback, further below in this file.
	m_actionHandler.Dispatch(this, GetEntityId(), action, activationMode, value);
}

bool CPlayerInput::OnActionMoveLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(eInputFlag_MoveLeft, activationMode);
	return true;
}

bool CPlayerInput::OnActionMoveRight(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(eInputFlag_MoveRight, activationMode);
	return true;
}

bool CPlayerInput::OnActionMoveForward(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(eInputFlag_MoveForward, activationMode);
	return true;
}

bool CPlayerInput::OnActionMoveBack(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(eInputFlag_MoveBack, activationMode);
	return true;
}

bool CPlayerInput::OnActionShoot(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	// Only fire on press, not release
	if (activationMode == eIS_Pressed)
	{
		// Note that we always fire from the barrel position
		// Another approach would be to use Aim IK and adjust aim based on where the player is aiming with the cursor

		auto *pWeapon = m_pPlayer->GetCurrentWeapon();
		auto *pCharacter = GetEntity()->GetCharacter(CPlayer::eGeometry_ThirdPerson);

		if (pWeapon != nullptr && pCharacter != nullptr)
		{
			auto *pBarrelOutAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName("barrel_out");

			if (pBarrelOutAttachment != nullptr)
			{
				QuatTS bulletOrigin = pBarrelOutAttachment->GetAttWorldAbsolute();

				pWeapon->RequestFire(bulletOrigin.t, bulletOrigin.q);
			}
		}
	}

	return true;
}