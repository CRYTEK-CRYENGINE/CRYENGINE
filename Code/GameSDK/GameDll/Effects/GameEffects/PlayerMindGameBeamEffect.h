// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// A helper class that is used to render a beam between the Player's 
// hand and a (boss) entity during a Mind-game button mash sequence.

#if !defined(PlayerMindGameEffect_h)
#define PlayerMindGameEffect_h

#include <CryCore/smartptr.h>

#include "LightningBolt.h"


// A simple effect that will be rendered at a position in the world.
class CPlayerMindGameBeamEffectSimpleEffect
{
public:

	CPlayerMindGameBeamEffectSimpleEffect();
	~CPlayerMindGameBeamEffectSimpleEffect();

	// Configuration:
	void                                   SetParticleEffectName(const char* particleFileName);

	// Life-cycle:
	void                                   Update();
	void                                   ReleaseAll();

	// Control:
	void                                   SetPosition(const Vec3& pos);
	void                                   StopEmitter();


private:

	// Pointer to the particle effect (NULL if none).
	typedef _smart_ptr<IParticleEffect> TParticleEffectPtr;
	TParticleEffectPtr                     m_ParticleEffect;

	// Pointer to the particle emitter (will be manually reference counted)
	// (NULL if there is none).
	IParticleEmitter*                      m_ParticleEmitter;


private:

	// Resource management:
	IParticleEmitter*                      GetParticleEmitter(const Vec3& startPos);
	inline void                            ReleaseParticleEffect();
	inline void                            ReleaseParticleEmitter();	
};


// The configuration of the linked effect between player and entity.
class CPlayerMindGameBeamEffectLinkedEffectConfig
{
public:

	// The name of the lightning effect.	
	CryFixedStringT<128>                m_EffectName;

	// The material name of the lightning effect.
	CryFixedStringT<128>                m_MaterialName;

	// Minimum time to delay before restarting the effect while it is enabled 
	// (>= 0.0f) (in seconds).
	float                               m_MinRestartDelay;

	// Maximum time to delay before restarting the effect while it is enabled 
	// (>= 0.0f) (in seconds).
	float                               m_MaxRestartDelay;


public:

	CPlayerMindGameBeamEffectLinkedEffectConfig();
};


// An effect that will be rendered between 2 entities.
class CPlayerMindGameBeamEffectLinkedEffect
{
public:

	CPlayerMindGameBeamEffectLinkedEffect();
	~CPlayerMindGameBeamEffectLinkedEffect();

	// Configuration:
	void                                   SetLightningEffect(const CPlayerMindGameBeamEffectLinkedEffectConfig* effectConfig);

	// Life-cycle:
	void                                   Update();
	void                                   ReleaseAll();

	// Control:
	void                                   BetweenPoints(const Vec3& startPos, const Vec3& endPos);
	void                                   StopEmitter();


private:

	// The lightning spark ID (-1 if none).
	CLightningGameEffect::TIndex           m_LightningID;

	// The lighting effect configuration (NULL if not initialized yet).
	const CPlayerMindGameBeamEffectLinkedEffectConfig* m_EffectConfig;

	// The amount of time left before we should restart the particle (>= 0.0f) (in seconds).
	float                                  m_RestartDelay;
};


// Support class for rendering an 'energy beam' between the player's hands and a target entity
// (currently designed for System-X).
class CPlayerMindGameBeamEffect
{
public:

	// The various ways how to connect the beams to the player.
	enum ConnectionPositionOnPlayer
	{
		ConnectionPositionOnPlayer_Invalid = 0, // Only used for debugging and error handling.
		ConnectionPositionOnPlayer_Hands,       // Connect them to the player's hands.
		ConnectionPositionOnPlayer_WaistArea    // Somewhere around the waist area.
	};


public:

	CPlayerMindGameBeamEffect();
	~CPlayerMindGameBeamEffect();

	// Setup:	
	void                                   Setup(const char* playersHandEffectName, const char* targetEntityEffectName, const CPlayerMindGameBeamEffectLinkedEffectConfig* linkedEffectConfig, const char* leftBossConnectAttachmentName, const char* rightBossConnectAttachmentName);
	void                                   SetupFromLuaTable(SmartScriptTable& rootTable);
	void                                   Purge();

	// Life-time:
	void                                   Enable(const EntityId entityID, const ConnectionPositionOnPlayer connectionPositionOnPlayer);
	void                                   Disable();
	bool                                   IsEnabled() const;
	
	void                                   Update();


private:

	// Joint/attachment related queries;
	bool                                   RetrieveStartPositionOnPlayer(Vec3* leftStartPosOnPlayer, Vec3* rightStartPosOnPlayer);
	bool                                   RetrievePlayerHandPositions(Vec3* playerLeftHandPos, Vec3* playerRightHandPos) const;
	bool                                   RetrievePlayerWaistPositions(Vec3* leftStartPosOnPlayer, Vec3* rightStartPosOnPlayer) const;
	bool                                   RetrieveTargetAttachmentPositions(Vec3* targetLeftAttachmentPos, Vec3* targetRightAttachmentPos) const;


private:

	static const int                       BeamsAmount = 2;

	static const char                      s_PlayerLeftHandJointName[];
	static const char                      s_PlayerRightHandJointName[];

	static const float                     s_PlayerWaistVerticalOffset;
	static const float                     s_PlayerWaistHorizontalSpacingOffset;


private:

	// The entity ID of the target with which to connect the energy beams or 0
	// if none.
	EntityId                               m_TargetEntityID;

	// How to connect the beams to the player (ConnectionPositionOnPlayer_Invalid if not initialized).
	ConnectionPositionOnPlayer             m_ConnectionOnPlayer;

	// The linked effect configuration.
	CPlayerMindGameBeamEffectLinkedEffectConfig m_LinkedEffectConfig;

	// The name of the left connection attachment on the target entity.
	CryFixedStringT<64>                    m_LeftConnectionAttachmentName;

	// The name of the left connection attachment on the target entity.
	CryFixedStringT<64>                    m_RightConnectionAttachmentName;

	// The linked effect between the player's hand and the target entity.
	CPlayerMindGameBeamEffectLinkedEffect  m_UpstreamLinkedEffects[BeamsAmount];
	CPlayerMindGameBeamEffectLinkedEffect  m_DownstreamLinkedEffects[BeamsAmount];

	// The effects on the player's hand.
	CPlayerMindGameBeamEffectSimpleEffect  m_PlayerHandEffects[BeamsAmount];

	// The effects on the target entity.
	CPlayerMindGameBeamEffectSimpleEffect  m_TargetEntityEffects[BeamsAmount];
};


#endif // !defined(PlayerMindGameEffect_h)
