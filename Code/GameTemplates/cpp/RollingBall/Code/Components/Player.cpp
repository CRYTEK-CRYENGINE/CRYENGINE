#include "StdAfx.h"
#include "Player.h"
#include "SpawnPoint.h"
#include "GamePlugin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryPhysics/IPhysics.h>
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
	// CryNetwork/CryPhysics: the entity has to be physicalized on both sides
	// *prior* to binding to network, so the physical state is synced properly.
	int slot = m_pEntity->LoadGeometry(GetOrMakeEntitySlotId(), "%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf");
	if (slot != -1)
	{
		IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("%ENGINE%/EngineAssets/TextureMsg/DefaultSolids");
		if (pMaterial != nullptr)
		{
			m_pEntity->SetMaterial(pMaterial);
		}
	}
	
	SEntityPhysicalizeParams physParams;
	physParams.type = PE_RIGID;
	physParams.mass = 90.f;
	m_pEntity->Physicalize(physParams);

	// By default, the server delegates authority to a single Player-entity on the client.
	// However, we want the player physics to be simulated server-side, so we need
	// to prevent physics aspect delegation.
	// This should be done on both sides.
	m_pEntity->GetNetEntity()->EnableDelegatableAspect(eEA_Physics, false);

	// Mark the entity to be replicated over the network
	m_pEntity->GetNetEntity()->BindToNetwork();

	// Indicate the physics type, this will be synchronized across all clients
	m_pEntity->GetNetEntity()->SetAspectProfile(eEA_Physics, physParams.type);

	// Register the RemoteReviveOnClient function as a Remote Method Invocation (RMI) that can be executed on clients from the server
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
	// Register the RemoteRequestJumpOnServer as an RMI that can be executed on the server from clients
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteRequestJumpOnServer)>::Register(this, eRAT_NoAttach, true, eNRT_ReliableOrdered);
}

void CPlayerComponent::InitializeLocalPlayer()
{
	// Create the camera component, will automatically update the viewport every frame
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();
	// Get the input component, wraps access to action mapping so we can easily get callbacks when inputs are m_pEntity
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	
	// Register an action, and the callback that will be sent when it's m_pEntity
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

	// Register the shoot action
	m_pInputComponent->RegisterAction("player", "jump", [this](int activationMode, float value)
	{
		if (activationMode != eAAM_OnPress)
			return;

		// Flags for the ray cast
		const unsigned int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;

		// Container which holds the ray hit information
		ray_hit hit;

		// Get the player bbox
		AABB bbox;
		m_pEntity->GetLocalBounds(bbox);

		// Ray cast direction which is down for our case.
		const Vec3 rayDirection = Vec3(0.0f, 0.0f, -1.0f) * bbox.GetRadius();

		// Max hit is 1 since we only want to know the closest collision to the player.
		const int maxHits = 1;

		// Find out if the player is in the air
		// Ray cast the world and add the player to the skip entity list so we don't hit the player itself.
		const bool canJump = gEnv->pPhysicalWorld->RayWorldIntersection(m_pEntity->GetWorldPos(), rayDirection, ent_all, rayFlags, &hit, maxHits, m_pEntity->GetPhysicalEntity()) > 0;

		if (canJump)
		{
			// RMI is used here only for demo purposes, the actual jump action
			// can be sent with input flags as well.
			SRmi<RMI_WRAP(&CPlayerComponent::RemoteRequestJumpOnServer)>::InvokeOnServer(this, RemoteRequestJumpParams{});
		}
	});

	// Bind the jump action to the space bar
	m_pInputComponent->BindAction("player", "jump", eAID_KeyboardMouse, EKeyId::eKI_Space);
}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return Cry::Entity::EEvent::BecomeLocalPlayer |
			Cry::Entity::EEvent::Update;
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

		if (IsLocalClient())
		{
			// Update the camera component offset
			UpdateCamera(frameTime);
		}

		if (gEnv->bServer) // Simulate physics only on the server for now.
		{
			// Start by updating the movement request we want to send to the character controller
			// This results in the physical representation of the character moving
			UpdateMovementRequest(frameTime);
		}
	}
	break;
	}
}

void CPlayerComponent::UpdateMovementRequest(float frameTime)
{
	if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
	{
		Vec3 velocity = ZERO;

		const float moveImpulseStrength = 800.f;

		// Update movement
		Vec3 direction = ZERO;

		if (m_inputFlags & EInputFlag::MoveLeft)
		{
			direction -= m_lookOrientation.GetColumn0();
		}
		if (m_inputFlags & EInputFlag::MoveRight)
		{
			direction += m_lookOrientation.GetColumn0();
		}
		if (m_inputFlags & EInputFlag::MoveForward)
		{
			direction += m_lookOrientation.GetColumn1();
		}
		if (m_inputFlags & EInputFlag::MoveBack)
		{
			direction -= m_lookOrientation.GetColumn1();
		}
		
		direction.z = 0.0f;

		// Only dispatch the impulse to physics if one was provided
		if (!direction.IsZero())
		{
			pe_action_impulse impulseAction;

			// Multiply by frame time to keep consistent across machines
			impulseAction.impulse = direction.GetNormalized() * moveImpulseStrength * frameTime;

			pPhysicalEntity->Action(&impulseAction);
		}
	}
}

void CPlayerComponent::UpdateCamera(float frameTime)
{
	// Start with updating look orientation from the latest input
	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));

	if (!m_mouseDeltaRotation.IsZero())
	{
		const float rotationSpeed = 0.002f;

		ypr.x += m_mouseDeltaRotation.x * rotationSpeed;

		const float rotationLimitsMinPitch = -1.2;
		const float rotationLimitsMaxPitch = 0.05f;

		// TODO: Perform soft clamp here instead of hard wall, should reduce rot speed in this direction when close to limit.
		ypr.y = CLAMP(ypr.y + m_mouseDeltaRotation.y * rotationSpeed, rotationLimitsMinPitch, rotationLimitsMaxPitch);

		// Look direction needs to be synced to server to calculate the movement in
		// the right direction.
		m_lookOrientation = Quat(CCamera::CreateOrientationYPR(ypr));
		NetMarkAspectsDirty(InputAspect);

		// Reset every frame
		m_mouseDeltaRotation = ZERO;
	}

	// Start with changing view rotation to the requested mouse look orientation
	Matrix34 localTransform = IDENTITY;
	localTransform.SetRotation33(Matrix33(m_pEntity->GetWorldRotation().GetInverted()) * CCamera::CreateOrientationYPR(ypr));

	const float viewDistance = 10.f;

	// Offset the player along the forward axis (normally back)
	// Also offset upwards
	localTransform.SetTranslation(-localTransform.GetColumn1() * viewDistance);

	m_pCameraComponent->SetTransformMatrix(localTransform);
}

void CPlayerComponent::OnReadyForGameplayOnServer()
{
	CRY_ASSERT_MESSAGE(gEnv->bServer, "This function should only be called on the server!");
	
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
	m_inputFlags.Clear();
	
	// Set the entity transformation, except if we are in the editor
	// In the editor case we always prefer to spawn where the viewport is
	if(!gEnv->IsEditor())
	{
		m_pEntity->SetWorldTM(transform);
	}
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

	// Input is replicated from the client to the server.
	if (IsLocalClient())
	{
		NetMarkAspectsDirty(InputAspect);
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

		// Serialize the player look orientation
		ser.Value("m_lookOrientation", m_lookOrientation, 'ori3');

		ser.EndGroup();
	}
	else if (aspect == eEA_Physics)
	{
		// Determine the physics type used by the remote side
		pe_type remotePhysicsType = static_cast<pe_type>(profile);

		// Nothing can be serialized if we are not physicalized
		if (remotePhysicsType == PE_NONE)
			return true;

		if (ser.IsWriting())
		{
			// If the remote physics type does not match our local state, serialize a dummy snapshot
			// Note that this might indicate a game code error, please ensure that the remote and local states match
			IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity();
			if (pPhysicalEntity == nullptr || pPhysicalEntity->GetType() != remotePhysicsType)
			{
				gEnv->pPhysicalWorld->SerializeGarbageTypedSnapshot(ser, remotePhysicsType, 0);
				return true;
			}
		}

		// Serialize physics state in order to keep the clients in sync
		m_pEntity->PhysicsNetSerializeTyped(ser, remotePhysicsType, flags);
	}

	return true;
}


bool CPlayerComponent::RemoteRequestJumpOnServer(RemoteRequestJumpParams&& p, INetChannel *pChannel)
{
	if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
	{
		pe_action_impulse impulse;
		impulse.impulse = Vec3(0, 0, 800);
		pPhysicalEntity->Action(&impulse);
	}
	return true;
}
