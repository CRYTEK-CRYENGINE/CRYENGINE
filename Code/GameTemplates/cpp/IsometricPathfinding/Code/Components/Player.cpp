#include "StdAfx.h"
#include "Player.h"

#include "Bullet.h"
#include "SpawnPoint.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryInput/IHardwareMouse.h>

void CPlayerComponent::Initialize()
{
	// Create the camera component, will automatically update the viewport every frame
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();

	// The character controller is responsible for maintaining player physics
	m_pCharacterController = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	// Offset the default character controller up by one unit
	m_pCharacterController->SetTransformMatrix(Matrix34::Create(Vec3(1.f), IDENTITY, Vec3(0, 0, 1.f)));

	// Create the advanced animation component, responsible for updating Mannequin and animating the player
	m_pAnimationComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CAdvancedAnimationComponent>();

	// Set the player geometry, this also triggers physics proxy creation
	m_pAnimationComponent->SetMannequinAnimationDatabaseFile("Animations/Mannequin/ADB/FirstPerson.adb");
	m_pAnimationComponent->SetCharacterFile("Objects/Characters/SampleCharacter/thirdperson.cdf");

	m_pAnimationComponent->SetControllerDefinitionFile("Animations/Mannequin/ADB/FirstPersonControllerDefinition.xml");
	m_pAnimationComponent->SetDefaultScopeContextName("FirstPersonCharacter");
	// Queue the idle fragment to start playing immediately on next update
	m_pAnimationComponent->SetDefaultFragmentName("Idle");

	// Disable movement coming from the animation (root joint offset), we control this entirely via physics
	m_pAnimationComponent->SetAnimationDrivenMotion(false);

	// Load the character and Mannequin data from file
	m_pAnimationComponent->LoadFromDisk();

	// Acquire tag identifiers to avoid doing so each update
	m_walkTagId = m_pAnimationComponent->GetTagId("Walk");

	// Get the input component, wraps access to action mapping so we can easily get callbacks when inputs are triggered
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	
	// Get and initialize the navigation component
	m_pNavigationComponent = m_pEntity->GetOrCreateComponent<IEntityNavigationComponent>();

	IEntityNavigationComponent::SMovementProperties movementProperties;
	movementProperties.normalSpeed = m_movementSpeed;
	movementProperties.maxSpeed = m_movementSpeed;
	movementProperties.maxAcceleration = m_movementSpeed * 1.5f;
	m_pNavigationComponent->SetMovementProperties(movementProperties);
	m_pNavigationComponent->SetNavigationAgentType("MediumSizedCharacters");

	m_pNavigationComponent->SetStateUpdatedCallback([this](const Vec3& recommendedVelocity)
	{
		m_pCharacterController->ChangeVelocity(recommendedVelocity, Cry::DefaultComponents::CCharacterControllerComponent::EChangeVelocityMode::SetAsTarget);
	});

	// Register the walk action
	m_pInputComponent->RegisterAction("player", "walkto", [this](int activationMode, float value)
	{
		if (m_pCursorEntity != nullptr && activationMode == eAAM_OnPress)
		{
			m_pNavigationComponent->NavigateTo(m_pCursorEntity->GetWorldPos());
		}
	});

	// Register the shoot action
	m_pInputComponent->RegisterAction("player", "shoot", [this](int activationMode, float value)
	{
		// Only fire on press, not release
		if (activationMode == eIS_Pressed)
		{
			if (ICharacterInstance *pCharacter = m_pAnimationComponent->GetCharacter())
			{
				auto *pBarrelOutAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName("barrel_out");

				if (pBarrelOutAttachment != nullptr)
				{
					QuatTS bulletOrigin = pBarrelOutAttachment->GetAttWorldAbsolute();

					SEntitySpawnParams spawnParams;
					spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

					spawnParams.vPosition = bulletOrigin.t;
					spawnParams.qRotation = bulletOrigin.q;

					const float bulletScale = 0.05f;
					spawnParams.vScale = Vec3(bulletScale);

					// Spawn the entity
					if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
					{
						// See Bullet.cpp, bullet is propelled in  the rotation and position the entity was spawned with
						pEntity->CreateComponentClass<CBulletComponent>();
					}
				}
			}
		}
	});

	// Bind the walkto action to left mouse click
	m_pInputComponent->BindAction("player", "walkto", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);
	// Bind the shoot action to the space bar
	m_pInputComponent->BindAction("player", "shoot", eAID_KeyboardMouse, EKeyId::eKI_Space);

	// Spawn the cursor if not in sandbox
	if (!gEnv->IsEditor())
	{
		SpawnCursorEntity();
	}

	Revive();
}

uint64 CPlayerComponent::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) | ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);
}

void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
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
	case ENTITY_EVENT_START_GAME:
	{
		// Revive the entity when gameplay starts
		Revive();
	}
	break;
	case ENTITY_EVENT_UPDATE:
	{
		SEntityUpdateContext* pCtx = (SEntityUpdateContext*)event.nParam[0];

		// Update the in-world cursor position
		UpdateCursor(pCtx->fFrameTime);

		// Update the animation state of the character
		UpdateAnimation(pCtx->fFrameTime);

		// Update the camera component offset
		UpdateCamera(pCtx->fFrameTime);
	}
	break;
	}
}

void CPlayerComponent::SpawnCursorEntity()
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
	m_pCursorEntity->LoadGeometry(geometrySlot, "%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf");

	// Scale the cursor down a bit
	m_pCursorEntity->SetScale(Vec3(0.1f));
	m_pCursorEntity->SetViewDistRatio(255);

	// Load the custom cursor material
	auto *pCursorMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Materials/cursor");
	m_pCursorEntity->SetMaterial(pCursorMaterial);
}

void CPlayerComponent::UpdateAnimation(float frameTime)
{
	ICharacterInstance *pCharacter = m_pAnimationComponent->GetCharacter();

	// Update the Mannequin tags
	m_pAnimationComponent->SetTagWithId(m_walkTagId, m_pCharacterController->IsWalking());

	if (m_pCharacterController->IsWalking())
	{
		Quat newRotation = Quat::CreateRotationVDir(m_pCharacterController->GetMoveDirection());

		Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(newRotation));

		// We only want to affect Z-axis rotation, zero pitch and roll
		ypr.y = 0;
		ypr.z = 0;

		// Re-calculate the quaternion based on the corrected yaw
		newRotation = Quat(CCamera::CreateOrientationYPR(ypr));

		// Send updated transform to the entity, only orientation changes
		m_pEntity->SetPosRotScale(m_pEntity->GetWorldPos(), newRotation, Vec3(1, 1, 1));
	}
}

void CPlayerComponent::UpdateCamera(float frameTime)
{
	// Start with rotating the camera isometric style
	Matrix34 localTransform = IDENTITY;
	localTransform.SetRotation33(Matrix33(m_pEntity->GetWorldRotation().GetInverted()) * Matrix33::CreateRotationXYZ(Ang3(DEG2RAD(-45), 0, DEG2RAD(-45))));

	const float viewDistanceFromPlayer = 10.f;

	// Offset the player along the forward axis (normally back)
	// Also offset upwards
	localTransform.SetTranslation(-localTransform.GetColumn1() * viewDistanceFromPlayer);

	m_pCameraComponent->SetTransformMatrix(localTransform);
}

void CPlayerComponent::UpdateCursor(float frameTime)
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
		if (m_pCursorEntity != nullptr)
		{
			m_pCursorEntity->SetPosRotScale(hit.pt, IDENTITY, m_pCursorEntity->GetScale());
		}
	}
}

void CPlayerComponent::Revive()
{
	// Find a spawn point and move the entity there
	SpawnAtSpawnPoint();

	// Unhide the entity in case hidden by the Editor
	GetEntity()->Hide(false);

	// Make sure that the player spawns upright
	GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1, 1, 1), IDENTITY, GetEntity()->GetWorldPos()));

	// Apply the character to the entity and queue animations
	m_pAnimationComponent->ResetCharacter();
	m_pCharacterController->Physicalize();
}

void CPlayerComponent::SpawnAtSpawnPoint()
{
	// Spawn at first default spawner
	IEntityItPtr pEntityIterator = gEnv->pEntitySystem->GetEntityIterator();
	pEntityIterator->MoveFirst();

	while (!pEntityIterator->IsEnd())
	{
		IEntity *pEntity = pEntityIterator->Next();

		if (auto* pSpawner = pEntity->GetComponent<CSpawnPointComponent>())
		{
			pSpawner->SpawnEntity(m_pEntity);
			break;
		}
	}
}