#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>

#include <ICryMannequin.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>

#include <CrySchematyc/CoreAPI.h>

struct MovementParams
{
	Vec3 pos;
	void SerializeWith(TSerialize ser)
	{
		ser.Value("pos", pos, 'wrld');
	}
};

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
		MoveBack = 1 << 3,
		Jump = 1 << 4
	};

	using TInputFlags = std::underlying_type<EInputFlag>::type;

	const EEntityAspects kInputAspect = eEA_GameClientD;

public:
	CPlayerComponent() = default;
	virtual ~CPlayerComponent() {}

	// IEntityComponent
	virtual void Initialize() override;

	virtual uint64 GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;
	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return kInputAspect | eEA_Physics; };
	// ~IEntityComponent

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{B08F2F41-F02E-48B5-921A-3FF857F19ED6}"_cry_guid);
	}

	void Revive();

protected:
	void UpdateMovementRequest(float frameTime);
	void UpdateCamera(float frameTime);

	void HandleInputFlagChange(TInputFlags flags, int activationMode, EInputFlagType type = EInputFlagType::Hold);

	// Additional initialization called only for the entity representing the current player.
	void InitializeLocalPlayer();

	bool SvJump(MovementParams&& p, INetChannel *);

protected:
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent = nullptr;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;

	TInputFlags m_inputFlags = 0;
	Vec2 m_mouseDeltaRotation = ZERO;
	// Should translate to head orientation in the future
	Quat m_lookOrientation = IDENTITY;
};
