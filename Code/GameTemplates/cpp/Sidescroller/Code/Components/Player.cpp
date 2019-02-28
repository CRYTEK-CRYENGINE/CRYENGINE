#include "StdAfx.h"
#include "Player.h"

#include "Bullet.h"
#include "SpawnPoint.h"

#include <CryRenderer/IRenderAuxGeom.h>

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

	// Register an action, and the callback that will be sent when it's triggered
	m_pInputComponent->RegisterAction("player", "moveleft", [this](int activationMode, float value) { HandleInputFlagChange((TInputFlags)EInputFlag::MoveLeft, activationMode);  });
	// Bind the 'A' key the "moveleft" action
	m_pInputComponent->BindAction("player", "moveleft", eAID_KeyboardMouse,	EKeyId::eKI_A);

	m_pInputComponent->RegisterAction("player", "moveright", [this](int activationMode, float value) { HandleInputFlagChange((TInputFlags)EInputFlag::MoveRight, activationMode);  });
	m_pInputComponent->BindAction("player", "moveright", eAID_KeyboardMouse, EKeyId::eKI_D);

	m_pInputComponent->RegisterAction("player", "jump", [this](int activationMode, float value)
	{
		// Only jump if the button was pressed
		if (activationMode == eIS_Pressed)
		{
			if (m_pCharacterController->IsOnGround())
				m_pCharacterController->AddVelocity(Vec3(0, 0, 5.f));
		}
	});

	m_pInputComponent->BindAction("player", "jump", eAID_KeyboardMouse, EKeyId::eKI_Space);

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

	// Bind the shoot action to left mouse click
	m_pInputComponent->BindAction("player", "shoot", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);

	Revive();
}

uint64 CPlayerComponent::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) | ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);
}

void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_START_GAME:
	{
		// Revive the entity when gameplay starts
		Revive();
	}
	break;
	case ENTITY_EVENT_UPDATE:
	{
		SEntityUpdateContext* pCtx = (SEntityUpdateContext*)event.nParam[0];

		// Start by updating the movement request we want to send to the character controller
		// This results in the physical representation of the character moving
		UpdateMovementRequest(pCtx->fFrameTime);

		// Update the animation state of the character
		UpdateAnimation(pCtx->fFrameTime);

		// Update the camera component offset
		UpdateCamera(pCtx->fFrameTime);
	}
	break;
	}
}

void CPlayerComponent::UpdateMovementRequest(float frameTime)
{
	// Don't handle input if we are in air
	if (!m_pCharacterController->IsOnGround())
		return;

	Vec3 velocity = ZERO;

	const float moveSpeed = 20.5f;

	if (m_inputFlags & (TInputFlags)EInputFlag::MoveLeft)
	{
		velocity.y -= moveSpeed * frameTime;
	}
	if (m_inputFlags & (TInputFlags)EInputFlag::MoveRight)
	{
		velocity.y += moveSpeed * frameTime;
	}

	m_pCharacterController->AddVelocity(GetEntity()->GetWorldRotation() * velocity);
}

void CPlayerComponent::UpdateAnimation(float frameTime)
{
	// Update the Mannequin tags
	m_pAnimationComponent->SetTagWithId(m_walkTagId, m_pCharacterController->IsWalking());
}

void CPlayerComponent::UpdateCamera(float frameTime)
{
	Matrix34 localTransform = IDENTITY;
	
	const float viewDistance = 5;
	const float viewOffsetUp = 2.f;

	// Offset the player along the forward axis (normally back)
	// Also offset upwards
	localTransform.SetTranslation(Vec3(viewDistance, 0, viewOffsetUp));

	localTransform.SetRotation33(Matrix33::CreateRotationZ(DEG2RAD(90)));

	m_pCameraComponent->SetTransformMatrix(localTransform);
}

void CPlayerComponent::Revive()
{
	// Find a spawn point and move the entity there
	SpawnAtSpawnPoint();

	// Unhide the entity in case hidden by the Editor
	GetEntity()->Hide(false);

	// Apply the character to the entity and queue animations
	m_pAnimationComponent->ResetCharacter();
	m_pCharacterController->Physicalize();

	// Reset input now that the player respawned
	m_inputFlags = 0;
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

void CPlayerComponent::HandleInputFlagChange(TInputFlags flags, int activationMode, EInputFlagType type)
{
	switch (type)
	{
	case EInputFlagType::Hold:
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
	case EInputFlagType::Toggle:
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