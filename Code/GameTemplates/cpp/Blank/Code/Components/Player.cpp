#include "StdAfx.h"
#include "Player.h"
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

	m_pInputComponent->RegisterAction("player", "moveforward", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveForward, (EActionActivationMode)activationMode);  }); 
	m_pInputComponent->BindAction("player", "moveforward", eAID_KeyboardMouse, EKeyId::eKI_W);

	m_pInputComponent->RegisterAction("player", "moveback", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveBack, (EActionActivationMode)activationMode);  }); 
	m_pInputComponent->BindAction("player", "moveback", eAID_KeyboardMouse, EKeyId::eKI_S);

	m_pInputComponent->RegisterAction("player", "mouse_rotateyaw", [this](int activationMode, float value) { m_mouseDeltaRotation.x -= value; });
	m_pInputComponent->BindAction("player", "mouse_rotateyaw", eAID_KeyboardMouse, EKeyId::eKI_MouseX);

	m_pInputComponent->RegisterAction("player", "mouse_rotatepitch", [this](int activationMode, float value) { m_mouseDeltaRotation.y -= value; });
	m_pInputComponent->BindAction("player", "mouse_rotatepitch", eAID_KeyboardMouse, EKeyId::eKI_MouseY);
}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return
		Cry::Entity::EEvent::BecomeLocalPlayer |
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
	case Cry::Entity::EEvent::Update:
	{
		// Don't update the player if we haven't spawned yet
		if(!m_isAlive)
			return;
		
		const float frameTime = event.fParam[0];

		const float moveSpeed = 20.5f;
		Vec3 velocity = ZERO;

		// Check input to calculate local space velocity
		if (m_inputFlags & EInputFlag::MoveLeft)
		{
			velocity.x -= moveSpeed * frameTime;
		}
		if (m_inputFlags & EInputFlag::MoveRight)
		{
			velocity.x += moveSpeed * frameTime;
		}
		if (m_inputFlags & EInputFlag::MoveForward)
		{
			velocity.y += moveSpeed * frameTime;
		}
		if (m_inputFlags & EInputFlag::MoveBack)
		{
			velocity.y -= moveSpeed * frameTime;
		}

		// Update the player's transformation
		Matrix34 transformation = m_pEntity->GetWorldTM();
		transformation.AddTranslation(transformation.TransformVector(velocity));

		// Update entity rotation based on latest input
		Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(transformation));

		const float rotationSpeed = 0.002f;
		ypr.x += m_mouseDeltaRotation.x * rotationSpeed;
		ypr.y += m_mouseDeltaRotation.y * rotationSpeed;

		// Disable roll
		ypr.z = 0;

		transformation.SetRotation33(CCamera::CreateOrientationYPR(ypr));

		// Reset the mouse delta since we "used" it this frame
		m_mouseDeltaRotation = ZERO;

		// Apply set position and rotation to the entity
		m_pEntity->SetWorldTM(transformation);
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
	if(aspect == InputAspect)
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

void CPlayerComponent::OnReadyForGameplayOnServer()
{
	CRY_ASSERT(gEnv->bServer, "This function should only be called on the server!");
	
	Vec3 playerScale = Vec3(1.f);
	Quat playerRotation = IDENTITY;

	// Position the player in the center of the map
	const float heightOffset = 20.f;
	const float terrainCenter = gEnv->p3DEngine->GetTerrainSize() / 2.f;
	const float height = gEnv->p3DEngine->GetTerrainZ(terrainCenter, terrainCenter);
	const Vec3 playerPosition = Vec3(terrainCenter, terrainCenter, height + heightOffset);

	const Matrix34 newTransform = Matrix34::Create(playerScale, playerRotation, playerPosition);
	
	Revive(newTransform);
	
	// Invoke the RemoteReviveOnClient function on all remote clients, to ensure that Revive is called across the network
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnOtherClients(this, RemoteReviveParams{ playerPosition, playerRotation });
	
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
	
	// Set the entity transformation, except if we are in the editor
	// In the editor case we always prefer to spawn where the viewport is
	if(!gEnv->IsEditor())
	{
		m_pEntity->SetWorldTM(transform);
	}
	
	// Reset input now that the player respawned
	m_inputFlags.Clear();
	NetMarkAspectsDirty(InputAspect);
	
	m_mouseDeltaRotation = ZERO;
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
	
	if(IsLocalClient())
	{
		NetMarkAspectsDirty(InputAspect);
	}
}