#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>

#include <ICryMannequin.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Audio/ListenerComponent.h>

////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{
	enum class EInputFlagType
	{
		Hold = 0,
		Toggle
	};

	enum class EInputFlag : uint8
	{
		MoveLeft = 1 << 0,
		MoveRight = 1 << 1,
		MoveForward = 1 << 2,
		MoveBack = 1 << 3
	};

	static constexpr EEntityAspects InputAspect = eEA_GameClientD;
	
public:
	CPlayerComponent() = default;
	virtual ~CPlayerComponent() = default;

	// IEntityComponent
	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;
	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return InputAspect; }
	// ~IEntityComponent

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
	}

	void OnReadyForGameplayOnServer();
	bool IsLocalClient() const { return (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) != 0; }
	
protected:
	void Revive(const Matrix34& transform);
	void HandleInputFlagChange(CEnumFlags<EInputFlag> flags, CEnumFlags<EActionActivationMode> activationMode, EInputFlagType type = EInputFlagType::Hold);

	// Called when this entity becomes the local player, to create client specific setup such as the Camera
	void InitializeLocalPlayer();
	
	// Start remote method declarations
protected:
	// Parameters to be passed to the RemoteReviveOnClient function
	struct RemoteReviveParams
	{
		// Called once on the server to serialize data to the other clients
		// Then called once on the other side to deserialize
		void SerializeWith(TSerialize ser)
		{
			// Serialize the position with the 'wrld' compression policy
			ser.Value("pos", position, 'wrld');
			// Serialize the rotation with the 'ori0' compression policy
			ser.Value("rot", rotation, 'ori0');
		}
		
		Vec3 position;
		Quat rotation;
	};
	// Remote method intended to be called on all remote clients when a player spawns on the server
	bool RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel);
	
protected:
	bool m_isAlive = false;

	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent = nullptr;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;
	Cry::Audio::DefaultComponents::CListenerComponent* m_pAudioListenerComponent = nullptr;

	CEnumFlags<EInputFlag> m_inputFlags;
	Vec2 m_mouseDeltaRotation;
};
