#include "StdAfx.h"
#include "PlayerInput.h"

#include "Player/Player.h"
#include "Player/PathFinding/PlayerPathFinding.h"

#include "Entities/Gameplay/Weapons/ISimpleWeapon.h"

#include <CryAnimation/IAttachment.h>
#include <CryInput/IHardwareMouse.h>

CPlayerInput::CPlayerInput()
	: m_pCursorEntity(nullptr)
{
	if (gEnv->pGameFramework)
		gEnv->pGameFramework->RegisterListener(this, "PlayerInput", FRAMEWORKLISTENERPRIORITY_DEFAULT);
}

CPlayerInput::~CPlayerInput()
{
	if (gEnv->pGameFramework)
		gEnv->pGameFramework->UnregisterListener(this);
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
		case ENTITY_EVENT_DONE:
		{
			GetGameObject()->ReleaseActions(this);
		}
		break;
	}
}

void CPlayerInput::SpawnCursorEntity()
{
	if (gEnv->IsEditing())
		return;

	CRY_ASSERT(m_pCursorEntity == nullptr);

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

void CPlayerInput::OnPostUpdate(float fDeltaTime)
{
	float mouseX, mouseY;
	gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mouseX, &mouseY);

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

void CPlayerInput::InitializeActionHandler()
{
	m_actionHandler.AddHandler(ActionId("shoot"), &CPlayerInput::OnActionShoot);
	m_actionHandler.AddHandler(ActionId("navigate_to"), &CPlayerInput::OnActionNavigateTo);
}

void CPlayerInput::OnAction(const ActionId &action, int activationMode, float value)
{
	// This function is called when inputs trigger action map events
	// The handler below maps the event (see 'action') to a callback, further below in this file.
	m_actionHandler.Dispatch(this, GetEntityId(), action, activationMode, value);
}


bool CPlayerInput::OnActionShoot(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	// Only fire on press, not release
	if (activationMode == eIS_Pressed)
	{
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

bool CPlayerInput::OnActionNavigateTo(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode != eIS_Pressed)
		return true;

	auto &pathFinding = *m_pPlayer->GetPathFinding();

	if (pathFinding.IsProcessingRequest())
	{
		pathFinding.CancelCurrentRequest();
	}

	if(!m_cursorPositionInWorld.IsZero())
	{
		pathFinding.RequestMoveTo(m_cursorPositionInWorld);
	}

	return true;
}