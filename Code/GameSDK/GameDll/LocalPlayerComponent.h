// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CLOCALPLAYERCOMPONENT_H__
#define __CLOCALPLAYERCOMPONENT_H__

#include "Player.h"
#include "PlayerStumble.h"
#include "PlayerEntityInteraction.h"
#include "IItemSystem.h"

struct SFollowCameraSettings
{
	enum E_FollowCameraFlags
	{
		eFCF_None = 0,
		eFCF_AllowOrbitYaw		= BIT(0),				//Whether player input can control camera Yaw
		eFCF_AllowOrbitPitch	= BIT(1),				//Whether player input can control camera Pitch
		eFCF_UseEyeDir				= BIT(2),				//Whether the eye dir - rather than the entity matrix - should be used to control the camera facing direction
		eFCF_UserSelectable		= BIT(3),				//Whether the view can be picked by the user.
		eFCF_DisableHeightAdj = BIT(4),				//Do not raise up the camera when collisions are received.
	};

	typedef uint8 TFollowCameraFlags;

	SFollowCameraSettings() : m_targetOffset(ZERO), m_viewOffset(ZERO), m_offsetSpeeds(ZERO), m_desiredDistance(0.f), m_cameraHeightOffset(0.f), m_cameraYaw(0.f), m_cameraSpeed(0.f), m_cameraFov(0.f), m_crcSettingsName(0), m_flags(eFCF_None)
	{
#ifndef _RELEASE
		m_settingsName = "Uninitialized";
#endif //_RELEASE
	}

#ifndef _RELEASE
	string m_settingsName;
#endif
	Vec3 m_targetOffset; //Of camera aim position from target (in target's local space)
	Vec3 m_viewOffset; //Of camera aim position from target (in target's local space)
	Vec3 m_offsetSpeeds;//Speeds for the individual components of the offset from the target in camera space.
	float m_desiredDistance; //Of camera from target
	float m_cameraHeightOffset; //From target
	float m_cameraYaw; //Radians. 0.0f = facing same direction as target.
	float m_cameraSpeed; //When moving to follow target. 0.0f to snap.
	float m_cameraFov; //Field of View in Radians.
	uint32 m_crcSettingsName;
	TFollowCameraFlags  m_flags; //Whether player input can control cameraYaw
};

class CLocalPlayerComponent : public IInventoryListener
{
public:
	CLocalPlayerComponent(CPlayer& rParentPlayer);
	virtual ~CLocalPlayerComponent();

	//IInventoryListener
	virtual void OnAddItem(EntityId entityId){};
	virtual void OnSetAmmoCount(IEntityClass* pAmmoType, int count){};
	virtual void OnAddAccessory(IEntityClass* pAccessoryClass){};
	virtual void OnClearInventory();
	//~IInventoryListener

	void Revive();
	void OnKill();

	void InitFollowCameraSettings(const IItemParamsNode* pFollowCameraNode);
	const SFollowCameraSettings& GetCurrentFollowCameraSettings() const;
	void ChangeCurrentFollowCameraSettings(bool increment);
	bool SetCurrentFollowCameraSettings( uint32 crcName );

	void StartFreeFallDeath();
	void ResetScreenFX();
	ILINE bool IsInFreeFallDeath() const { return m_bIsInFreeFallDeath; }

	SMeleeHitParams * GetLastMeleeParams()																{ return &m_lastMeleeParams; }
	void							SetLastMeleeParams(const SMeleeHitParams& rParams)	{ m_lastMeleeParams = rParams; }

	// Allow manual trigger of fade to black. Resets when next respawn.
	void TriggerFadeToBlack(); 

	//Sound moods
	void InitSoundmoods();
	void SetClientSoundmood(CPlayer::EClientSoundmoods soundmood);
	void AddClientSoundmood(CPlayer::EClientSoundmoods soundmood);
	void RemoveClientSoundmood(CPlayer::EClientSoundmoods soundmood);
	ILINE CPlayer::EClientSoundmoods GetSoundmood() { return m_soundmood; }
	CPlayer::EClientSoundmoods FindClientSoundmoodBestFit() const;

	//Main update
	void Update( const float frameTime );

	//STAP & FPIK
	void UpdateFPIKTorso(float fFrameTime, IItem * pCurrentItem, const Vec3& cameraPosition);
	ILINE const QuatT	&GetLastSTAPCameraDelta() const { return m_lastSTAPCameraDelta; }

	//Breathing code
	void UpdateSwimming(float fFrameTime, float breathingInterval);

	void UpdatePlayerLowHealthStatus( const float oldHealth );
	float GetTimeEnteredLowHealth() const { return m_timeEnteredLowHealth.GetSeconds(); };

	void UpdateStumble( const float frameTime );
	void EnableStumbling(PlayerActor::Stumble::StumbleParameters* stumbleParameters)
				{ m_playerStumble.Start(stumbleParameters); }
	void DisableStumbling()
				{ m_playerStumble.Disable(); }

	//FP visibility
	void RefreshVisibilityState ( const bool isThirdPerson )
	{
		m_fpCompleteBodyVisible = isThirdPerson;
	}

	void SetHasUsedAutoArmour() { m_autoArmourFlags |= EAAF_UsedAutoArmour; }
	void SetHasTakenDamageWithArmour() { m_autoArmourFlags |= EAAF_TakenDamageWithArmor; }
	void SetHasEnteredSpectatorMode() { m_autoArmourFlags |= EAAF_EnteredSpectatorMode; }
	bool CanCompleteTrainingWheelsAchievement() const { return m_autoArmourFlags == EAAF_TakenDamageWithArmor; }

	ILINE CPlayerEntityInteraction& GetPlayerEntityInteraction() {return m_playerEntityInteraction;}

protected:
	CPlayer& m_rPlayer;

	//Health feedback related variables
	CTimeValue	m_timeEnteredLowHealth;
	bool        m_playedMidHealthSound;

	//FP visibility
	bool        m_fpCompleteBodyVisible;

	enum EAutoArmorFlags
	{
		EAAF_UsedAutoArmour					= BIT(0),
		EAAF_TakenDamageWithArmor		= BIT(1),
		EAAF_EnteredSpectatorMode		= BIT(2),
	};

	uint8				m_autoArmourFlags;

	//Freefall related vars
	bool					m_bIsInFreeFallDeath;
	Vec3					m_freeFallLookTarget;
	float					m_freeFallDeathFadeTimer;
	TMFXEffectId	m_screenFadeEffectId;	

	SMeleeHitParams m_lastMeleeParams;
	PlayerActor::Stumble::CPlayerStumble  m_playerStumble;


	//Sound moods
	struct SSoundmood
	{
		void Set(const char* enterSignal, const char* exitSignal)
		{
			m_enter.SetSignal(enterSignal);
			m_exit.SetSignal(exitSignal);
		}
		void Empty()
		{
			m_enter.SetSignal(INVALID_AUDIOSIGNAL_ID);
			m_exit.SetSignal(INVALID_AUDIOSIGNAL_ID);
		}
		CAudioSignalPlayer m_enter;
		CAudioSignalPlayer m_exit;
	};
	SSoundmood m_soundmoods[CPlayer::ESoundmood_Last];
	CPlayer::EClientSoundmoods m_soundmood;

	std::vector<SFollowCameraSettings> m_followCameraSettings;
	int m_currentFollowCameraSettingsIndex;
#ifndef _RELEASE
	mutable SFollowCameraSettings m_followCameraDebugSettings;
#endif //_RELEASE

	float m_timeForBreath;


	//STAP & FPIK
	QuatT	m_lastSTAPCameraDelta;
	float m_stapElevLimit;	

private:
	void UpdateItemUsage( const float frameTime );
	void UpdateFreeFallDeath( const float frameTime );

	void UpdateFPBodyPartsVisibility();

	void UpdateScreenFadeEffect();
	void PlayBreathingUnderwater(float breathingInterval);
	
	void AdjustTorsoAimDir(float fFrameTime, Vec3 &aimDir);
	void GetFPTotalTorsoOffset(QuatT &offset, IItem * pCurrentItem) const;
	bool ShouldUpdateTranslationPinning() const;

	CPlayerEntityInteraction m_playerEntityInteraction;
};


#endif //__CLOCALPLAYERCOMPONENT_H__
