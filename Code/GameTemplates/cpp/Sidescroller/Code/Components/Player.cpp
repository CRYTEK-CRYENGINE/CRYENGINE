#include "StdAfx.h"
#include "Player.h"
#include "Bullet.h"
#include "SpawnPoint.h"
#include "GamePlugin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CryNetwork/Rmi.h>

namespace
{
	static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}

void CPlayerComponent::Initialize()
{
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
	m_pAnimationComponent->SetAnimationDrivenMotion(true);

	// Load the character and Mannequin data from file
	m_pAnimationComponent->LoadFromDisk();

	// Acquire tag identifiers to avoid doing so each update
	m_walkTagId = m_pAnimationComponent->GetTagId("Walk");

	// Mark the entity to be replicated over the network
	m_pEntity->GetNetEntity()->BindToNetwork();

	// Register the RemoteReviveOnClient function as a Remote Method Invocation (RMI) that can be executed by the server on clients
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
}

void CPlayerComponent::InitializeLocalPlayer()
{
	// Create the camera component, will automatically update the viewport every frame
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();

	// Create the audio listener component.
	m_pAudioListenerComponent = m_pEntity->GetOrCreateComponent<Cry::Audio::DefaultComponents::CListenerComponent>();

	// Get the input component, wraps access to action mapping so we can easily get callbacks when inputs are triggered
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();

	// Register an action, and the callback that will be sent when it's triggered
	m_pInputComponent->RegisterAction("player", "moveleft", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveLeft, (EActionActivationMode)activationMode);  });
	// Bind the 'A' key the "moveleft" action
	m_pInputComponent->BindAction("player", "moveleft", eAID_KeyboardMouse, EKeyId::eKI_A);

	m_pInputComponent->RegisterAction("player", "moveright", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveRight, (EActionActivationMode)activationMode);  });
	m_pInputComponent->BindAction("player", "moveright", eAID_KeyboardMouse, EKeyId::eKI_D);

	m_pInputComponent->RegisterAction("player", "jump", [this](int activationMode, float value)
	{
		// Only jump if the button was pressed
		if (activationMode == eAAM_OnPress)
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
		if (activationMode == eAAM_OnPress)
		{
			if (ICharacterInstance *pCharacter = m_pAnimationComponent->GetCharacter())
			{
				IAttachment* pBarrelOutAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName("barrel_out");

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
}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return
		Cry::Entity::EEvent::BecomeLocalPlayer |
		Cry::Entity::EEvent::GameplayStarted |
		Cry::Entity::EEvent::Update |
		Cry::Entity::EEvent::Reset;
}

void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case Cry::Entity::EEvent::BecomeLocalPlayer:
	{
		InitializeLocalPlayer();
	}
	break;
	case Cry::Entity::EEvent::GameplayStarted:
	{
		const Matrix34 newTransform = CSpawnPointComponent::GetFirstSpawnPointTransform();

		Revive(newTransform);
	}
	break;
	case Cry::Entity::EEvent::Update:
	{
		// Don't update the player if we haven't spawned yet
		if (!m_isAlive)
			return;

		const float frameTime = event.fParam[0];

		// Start by updating the movement request we want to send to the character controller
		// This results in the physical representation of the character moving
		UpdateMovementRequest(frameTime);

		// Update the animation state of the character
		UpdateAnimation(frameTime);

		if (IsLocalClient())
		{
			// Update the camera component offset
			UpdateCamera(frameTime);
		}
	}
	break;
	case Cry::Entity::EEvent::Reset:
	{
		// Disable player when leaving game mode.
		m_isAlive = event.nParam[0] != 0;
	}
	break;
	}
}

bool CPlayerComponent::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect == InputAspect)
	{
		ser.BeginGroup("PlayerInput");

		const CEnumFlags<EInputFlag> prevInputFlags = m_inputFlags;

		ser.Value("m_inputFlags", m_inputFlags.UnderlyingValue(), 'ui8');

		if (ser.IsReading())
		{
			const CEnumFlags<EInputFlag> changedKeys = prevInputFlags ^ m_inputFlags;

			const CEnumFlags<EInputFlag> pressedKeys = changedKeys & prevInputFlags;
			if (!pressedKeys.IsEmpty())
			{
				HandleInputFlagChange(pressedKeys, eAAM_OnPress);
			}

			const CEnumFlags<EInputFlag> releasedKeys = changedKeys & prevInputFlags;
			if (!releasedKeys.IsEmpty())
			{
				HandleInputFlagChange(pressedKeys, eAAM_OnRelease);
			}
		}

		ser.EndGroup();
	}

	return true;
}

void CPlayerComponent::UpdateMovementRequest(float frameTime)
{
	// Don't handle input if we are in air
	if (!m_pCharacterController->IsOnGround())
		return;

	Vec3 velocity = ZERO;

	const float moveSpeed = 20.5f;

	if (m_inputFlags & EInputFlag::MoveLeft)
	{
		velocity.y -= moveSpeed * frameTime;
	}
	if (m_inputFlags & EInputFlag::MoveRight)
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
	m_pAudioListenerComponent->SetOffset(localTransform.GetTranslation());
}

void CPlayerComponent::OnReadyForGameplayOnServer()
{
	CRY_ASSERT(gEnv->bServer, "This function should only be called on the server!");

	const Matrix34 newTransform = CSpawnPointComponent::GetFirstSpawnPointTransform();

	Revive(newTransform);

	// Invoke the RemoteReviveOnClient function on all remote clients, to ensure that Revive is called across the network
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnOtherClients(this, RemoteReviveParams{ newTransform.GetTranslation(), Quat(newTransform) });

	// Go through all other players, and send the RemoteReviveOnClient on their instances to the new player that is ready for gameplay
	const int channelId = m_pEntity->GetNetEntity()->GetChannelId();
	CGamePlugin::GetInstance()->IterateOverPlayers([this, channelId](CPlayerComponent& player)
	{
		// Don't send the event for the player itself (handled in the RemoteReviveOnClient event above sent to all clients)
		if (player.GetEntityId() == GetEntityId())
			return;

		// Only send the Revive event to players that have already respawned on the server
		if (!player.m_isAlive)
			return;

		// Revive this player on the new player's machine, on the location the existing player was currently at
		const QuatT currentOrientation = QuatT(player.GetEntity()->GetWorldTM());
		SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnClient(&player, RemoteReviveParams{ currentOrientation.t, currentOrientation.q }, channelId);
	});
}

bool CPlayerComponent::RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel)
{
	// Call the Revive function on this client
	Revive(Matrix34::Create(Vec3(1.f), params.rotation, params.position));

	return true;
}

void CPlayerComponent::Revive(const Matrix34& transform)
{
	m_isAlive = true;

	m_pEntity->SetWorldTM(transform);

	// Apply the character to the entity and queue animations
	m_pAnimationComponent->ResetCharacter();
	m_pCharacterController->Physicalize();

	// Reset input now that the player respawned
	m_inputFlags.Clear();
	NetMarkAspectsDirty(InputAspect);
}

void CPlayerComponent::HandleInputFlagChange(const CEnumFlags<EInputFlag> flags, const CEnumFlags<EActionActivationMode> activationMode, const EInputFlagType type)
{
	switch (type)
	{
	case EInputFlagType::Hold:
	{
		if (activationMode == eAAM_OnRelease)
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
		if (activationMode == eAAM_OnRelease)
		{
			// Toggle the bit(s)
			m_inputFlags ^= flags;
		}
	}
	break;
	}

	if (IsLocalClient())
	{
		NetMarkAspectsDirty(InputAspect);
	}
}