// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// A helper class that is used to render a beam between the Player's 
// hand and a (boss) entity during a Mind-game button mash sequence.

#include "StdAfx.h"

#include "PlayerMindGameBeamEffect.h"

#include "Actor.h"


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- CPlayerMindGameBeamEffectLinkedEffectConfig --
//
// ============================================================================
// ============================================================================
// ============================================================================


CPlayerMindGameBeamEffectLinkedEffectConfig::CPlayerMindGameBeamEffectLinkedEffectConfig() :	
	m_EffectName()
	, m_MaterialName()
	, m_MinRestartDelay()
	, m_MaxRestartDelay()
{
}


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- CPlayerMindGameBeamEffectSimpleEffect --
//
// ============================================================================
// ============================================================================
// ============================================================================


CPlayerMindGameBeamEffectSimpleEffect::CPlayerMindGameBeamEffectSimpleEffect() :
	m_ParticleEffect()
	, m_ParticleEmitter(NULL)
{
}



CPlayerMindGameBeamEffectSimpleEffect::~CPlayerMindGameBeamEffectSimpleEffect()
{
	ReleaseAll(); // Safety code.
}


// ============================================================================
//	Update the effect (if needed).
//
void CPlayerMindGameBeamEffectSimpleEffect::Update()
{		
	if (m_ParticleEmitter != NULL)
	{
		m_ParticleEmitter->Update();
	}	
}


// ============================================================================
//	Release all resources (if needed).
//
void CPlayerMindGameBeamEffectSimpleEffect::ReleaseAll()
{
	ReleaseParticleEffect();
	ReleaseParticleEmitter();	
}


// ============================================================================
//	Set the particle effect file name (will immediately be loaded).
//
//	In:		The particle file name (NULL or an empty string will abort!)
//
void CPlayerMindGameBeamEffectSimpleEffect::SetParticleEffectName(const char* particleFileName)
{
	if (particleFileName == NULL)
	{
		return;
	}
	if (*particleFileName == '\0')
	{
		return;
	}

	IParticleEffect* particleEffect = gEnv->pParticleManager->FindEffect(particleFileName, "CEnergyBeamsBetweenPlayerAndEntitySimpleEffect::SetParticleEffect");
	if (particleEffect == NULL)
	{
		CryLog("CPlayerMindGameBeamEffectSimpleEffect::SetParticleEffect(): Unable to load particle effect '%s'!", particleFileName);
		return;
	}

	m_ParticleEffect = particleEffect;
}


// ============================================================================
//	Show the effect at a position (this will spawn the particle emitter if 
//	needed).
//
//	In:		The world-space position of the effect.
//
void CPlayerMindGameBeamEffectSimpleEffect::SetPosition(const Vec3& pos)
{
	IParticleEmitter* emitter = GetParticleEmitter(pos);
	if (emitter == NULL)
	{
		return;
	}

	Matrix34 matrix = Matrix34::CreateTranslationMat(pos);
	emitter->SetMatrix(matrix);
}


// ============================================================================
//	Stop the particle emitter (if needed).
//
//	This function will not release all resources so that we can quickly
//	start emitting again.
//
void CPlayerMindGameBeamEffectSimpleEffect::StopEmitter()
{
	ReleaseParticleEmitter();
}


// ============================================================================
//	Get the particle emitter (create one if needed).
//
//	In:		The world-space start position of the effect (will be used when we
//			create the particle emitter for the first time).
//
//	Returns:	The particle emitter or NULL if none could be obtained.
//
IParticleEmitter* CPlayerMindGameBeamEffectSimpleEffect::GetParticleEmitter(const Vec3& startPos)
{
	if (m_ParticleEmitter != NULL)
	{
		return m_ParticleEmitter;
	}

	// Create a new particle emitter now.
	if (m_ParticleEffect.get() != NULL)
	{
		Matrix34 startTransform = Matrix34::CreateTranslationMat(startPos);

		m_ParticleEmitter = m_ParticleEffect->Spawn(false, startTransform);	
		if (m_ParticleEmitter != NULL)
		{
			m_ParticleEmitter->AddRef();
			return m_ParticleEmitter;
		}		
	}

	return NULL;
}


// ============================================================================
//	Release the particle effect (if needed).
//
void CPlayerMindGameBeamEffectSimpleEffect::ReleaseParticleEffect()
{
	m_ParticleEffect = NULL;
}


// ============================================================================
//	Release the particle emitter (if needed).
//
void CPlayerMindGameBeamEffectSimpleEffect::ReleaseParticleEmitter()
{	
	if (m_ParticleEmitter != NULL)
	{
		gEnv->pParticleManager->DeleteEmitter(m_ParticleEmitter);
	}

	SAFE_RELEASE(m_ParticleEmitter);
}


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- CPlayerMindGameBeamEffectLinkedEffect --
//
// ============================================================================
// ============================================================================
// ============================================================================


CPlayerMindGameBeamEffectLinkedEffect::CPlayerMindGameBeamEffectLinkedEffect() :
	m_LightningID(CLightningGameEffect::TIndex(-1))
	, m_EffectConfig(NULL)
	, m_RestartDelay(0.0f)
{
}


CPlayerMindGameBeamEffectLinkedEffect::~CPlayerMindGameBeamEffectLinkedEffect()
{
	ReleaseAll();
}


void CPlayerMindGameBeamEffectLinkedEffect::SetLightningEffect(const CPlayerMindGameBeamEffectLinkedEffectConfig* effectConfig)
{
	StopEmitter();
	assert(effectConfig != NULL);
	m_EffectConfig = effectConfig;
}


void CPlayerMindGameBeamEffectLinkedEffect::Update()
{
	if (m_LightningID != -1)
	{
		const float remainingDelay = m_RestartDelay - gEnv->pTimer->GetFrameTime();
		if (remainingDelay <= 0.0f)
		{
			m_RestartDelay = 0.0f;
			StopEmitter(); // Note: the effect will be restarted when needed again
			// (so this will be during the next call to BetweenPoints()).
		}
		else
		{
			m_RestartDelay = remainingDelay;
		}
	}
}


void CPlayerMindGameBeamEffectLinkedEffect::ReleaseAll()
{
	StopEmitter();
}


// ============================================================================
//	Show the effect between 2 points (this will spawn the particle emitter if 
//	needed).
//
//	In:		The world-space start position of the effect.
//	In:		The world-space end position of the effect.
//
void CPlayerMindGameBeamEffectLinkedEffect::BetweenPoints(const Vec3& startPos, const Vec3& endPos)
{
	IF_UNLIKELY(g_pGame == NULL)
	{
		return;
	}
	CLightningGameEffect* lightningGameEffect = g_pGame->GetLightningGameEffect();
	IF_UNLIKELY(lightningGameEffect == NULL)
	{
		return;
	}

	if (m_LightningID == -1)
	{
		assert(m_EffectConfig != NULL);
		m_LightningID = lightningGameEffect->TriggerSpark(
			m_EffectConfig->m_EffectName.c_str(),
			gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_EffectConfig->m_MaterialName),
			CLightningGameEffect::STarget(startPos), CLightningGameEffect::STarget(endPos));
		assert(m_EffectConfig->m_MinRestartDelay <= m_EffectConfig->m_MaxRestartDelay);
		m_RestartDelay = cry_random(m_EffectConfig->m_MinRestartDelay, m_EffectConfig->m_MaxRestartDelay);
	}
	else
	{
		lightningGameEffect->SetEmitter(m_LightningID, CLightningGameEffect::STarget(startPos));
		lightningGameEffect->SetReceiver(m_LightningID, CLightningGameEffect::STarget(endPos));
	}
}


void CPlayerMindGameBeamEffectLinkedEffect::StopEmitter()
{
	if (m_LightningID == -1)
	{
		return;
	}

	IF_LIKELY(g_pGame != NULL)
	{
		CLightningGameEffect* lightningGameEffect = g_pGame->GetLightningGameEffect();
		IF_LIKELY(lightningGameEffect != NULL)
		{
			lightningGameEffect->RemoveSpark(m_LightningID);
		}
	}

	m_LightningID = -1;
}


// ===========================================================================
// ===========================================================================
// ===========================================================================
//
// -- CPlayerMindGameBeamEffect -- CPlayerMindGameBeamEffect --
//
// ===========================================================================
// ===========================================================================
// ===========================================================================


const char CPlayerMindGameBeamEffect::s_PlayerLeftHandJointName[] = "Bip01 L Finger41"; // "Bip01 L Hand" is too close to the camera sometimes and causes the particles to be clipped.
const char CPlayerMindGameBeamEffect::s_PlayerRightHandJointName[] = "Bip01 R Finger41"; // "Bip01 R Hand" is too close to the camera sometimes and causes the particles to be clipped.

// The offset and spacing of the points where the 'beam' effects between are linked to the player.
const float CPlayerMindGameBeamEffect::s_PlayerWaistVerticalOffset = 1.0f;
const float CPlayerMindGameBeamEffect::s_PlayerWaistHorizontalSpacingOffset = 0.2f;


CPlayerMindGameBeamEffect::CPlayerMindGameBeamEffect()
	: m_TargetEntityID((EntityId)0)
	, m_ConnectionOnPlayer(ConnectionPositionOnPlayer_Invalid)
	, m_LinkedEffectConfig()
	, m_LeftConnectionAttachmentName()
	, m_RightConnectionAttachmentName()
	, m_UpstreamLinkedEffects()
	, m_DownstreamLinkedEffects()
	, m_PlayerHandEffects()
	, m_TargetEntityEffects()
{
}


CPlayerMindGameBeamEffect::~CPlayerMindGameBeamEffect()
{
	Disable(); // Safety code.
}


// ===========================================================================
//	Setup.
//
//	In:		The name of the particle effect on the player (NULL or empty string
//	        will ignore).
//	In:		The name of the particle effect on the target entity (NULL or empty
//	        string will ignore).
//	In:		The configuration of the linked lightning effect (NULL is invalid!)
//	In:		The name of the left attachment on the target entity to which we will
//	        connect the lightning effect from the player (NULL is invalid!)
//	In:		The name of the right attachment on the target entity to which we will
//	        connect the lightning effect from the player (NULL is invalid!)
//
void CPlayerMindGameBeamEffect::Setup(
	const char* playersEffectName, 
	const char* targetEntityEffectName, 
	const CPlayerMindGameBeamEffectLinkedEffectConfig* linkedEffectConfig,
	const char* leftBossConnectAttachmentName,
	const char* rightBossConnectAttachmentName)
{
	assert(targetEntityEffectName != NULL);
	assert(linkedEffectConfig != NULL);
	assert(leftBossConnectAttachmentName != NULL);
	assert(rightBossConnectAttachmentName != NULL);

	m_LinkedEffectConfig = *linkedEffectConfig;

	for (int beamIndex = 0 ; beamIndex < BeamsAmount ; beamIndex++)
	{
		m_UpstreamLinkedEffects[beamIndex].SetLightningEffect(&m_LinkedEffectConfig);
		m_DownstreamLinkedEffects[beamIndex].SetLightningEffect(&m_LinkedEffectConfig);
		m_PlayerHandEffects[beamIndex].SetParticleEffectName(playersEffectName);
		m_TargetEntityEffects[beamIndex].SetParticleEffectName(targetEntityEffectName);
	}

	m_LeftConnectionAttachmentName = leftBossConnectAttachmentName;
	m_RightConnectionAttachmentName = rightBossConnectAttachmentName;
}


void CPlayerMindGameBeamEffect::SetupFromLuaTable(SmartScriptTable& rootTable)
{
	string handEffectName;
	string bossConnectEffectName;
	string leftBossConnectAttachmentName;
	string rightBossConnectAttachmentName;
	CPlayerMindGameBeamEffectLinkedEffectConfig linkedEffectConfig;
	
	const char* tempString;
	rootTable->GetValue("PlayerEffectName", tempString);
	handEffectName = tempString;
	rootTable->GetValue("BossConnectEffectName", tempString);
	bossConnectEffectName = tempString;
	
	SmartScriptTable linkedEffectTable;
	if (rootTable->GetValue("LightningEffect", linkedEffectTable))
	{
		linkedEffectTable->GetValue("EffectName", tempString); 
		linkedEffectConfig.m_EffectName = tempString;
		linkedEffectTable->GetValue("MaterialName", tempString);
		linkedEffectConfig.m_MaterialName = tempString;
		linkedEffectTable->GetValue("MinRestartDelay", linkedEffectConfig.m_MinRestartDelay);
		linkedEffectTable->GetValue("MaxRestartDelay", linkedEffectConfig.m_MaxRestartDelay);
	}

	rootTable->GetValue("LeftBossConnectAttachmentName",  tempString);
	leftBossConnectAttachmentName = tempString;
	rootTable->GetValue("RightBossConnectAttachmentName", tempString);
	rightBossConnectAttachmentName = tempString;
	
	Setup(
		handEffectName.c_str(), bossConnectEffectName.c_str(), 
		&linkedEffectConfig,
		leftBossConnectAttachmentName.c_str(), rightBossConnectAttachmentName.c_str());
}


void CPlayerMindGameBeamEffect::Purge()
{
	Disable(); // Safety code.

	for (int beamIndex = 0 ; beamIndex < BeamsAmount ; beamIndex++)
	{
		m_UpstreamLinkedEffects[beamIndex].ReleaseAll();
		m_DownstreamLinkedEffects[beamIndex].ReleaseAll();
		m_PlayerHandEffects[beamIndex].ReleaseAll();
		m_TargetEntityEffects[beamIndex].ReleaseAll();
	}
}


void CPlayerMindGameBeamEffect::Enable(const EntityId targetEntityID, const ConnectionPositionOnPlayer connectionPositionOnPlayer)
{
	Disable();

	if ( (connectionPositionOnPlayer == ConnectionPositionOnPlayer_Invalid) || (targetEntityID == (EntityId)0) )
	{
		return;
	}

	m_TargetEntityID = targetEntityID;
	m_ConnectionOnPlayer = connectionPositionOnPlayer;
}


void CPlayerMindGameBeamEffect::Disable()
{
	m_TargetEntityID = (EntityId)0;

	for (int beamIndex = 0 ; beamIndex < BeamsAmount ; beamIndex++)
	{
		m_UpstreamLinkedEffects[beamIndex].StopEmitter();
		m_DownstreamLinkedEffects[beamIndex].StopEmitter();
		m_PlayerHandEffects[beamIndex].StopEmitter();
		m_TargetEntityEffects[beamIndex].StopEmitter();
	}
}


bool CPlayerMindGameBeamEffect::IsEnabled() const
{
	return (m_TargetEntityID != (EntityId)0);
}


void CPlayerMindGameBeamEffect::Update()
{
	if (!IsEnabled())
	{
		return;
	}

	Vec3 leftStartPosOnPlayer, rightStartPosOnPlayer;
	const bool playerPosValidFlag = RetrieveStartPositionOnPlayer(&leftStartPosOnPlayer, &rightStartPosOnPlayer);

	Vec3 leftAttachmentPos, rightAttachmentPos;
	const bool attachmentPosValidFlag = RetrieveTargetAttachmentPositions(&leftAttachmentPos, &rightAttachmentPos);

	for (int beamIndex = 0 ; beamIndex < BeamsAmount ; beamIndex++)
	{
		Vec3 startPos, endPos;
		if (beamIndex == 0)
		{
			startPos = leftStartPosOnPlayer;
			endPos = rightAttachmentPos;
		}
		else
		{
			startPos = rightStartPosOnPlayer;
			endPos = leftAttachmentPos;
		}

		if (playerPosValidFlag && attachmentPosValidFlag)
		{
			CPlayerMindGameBeamEffectLinkedEffect& upstreamLinkEffect = m_UpstreamLinkedEffects[beamIndex];
			upstreamLinkEffect.BetweenPoints(startPos, endPos);
			upstreamLinkEffect.Update();

			CPlayerMindGameBeamEffectLinkedEffect& downstreamLinkEffect = m_DownstreamLinkedEffects[beamIndex];
			downstreamLinkEffect.BetweenPoints(endPos, startPos);
			downstreamLinkEffect.Update();
		}

		if (playerPosValidFlag)
		{
			CPlayerMindGameBeamEffectSimpleEffect& playerHandEffect = m_PlayerHandEffects[beamIndex];
			playerHandEffect.SetPosition(startPos);
			playerHandEffect.Update();
		}

		if (attachmentPosValidFlag)
		{
			CPlayerMindGameBeamEffectSimpleEffect& targetEntityEffect = m_TargetEntityEffects[beamIndex];
			targetEntityEffect.SetPosition(endPos);
			targetEntityEffect.Update();
		}
	}
}


bool CPlayerMindGameBeamEffect::RetrieveStartPositionOnPlayer(Vec3* leftStartPosOnPlayer, Vec3* rightStartPosOnPlayer)
{
	switch (m_ConnectionOnPlayer)
	{
	case ConnectionPositionOnPlayer_Hands:
		IF_UNLIKELY (!RetrievePlayerHandPositions(leftStartPosOnPlayer, rightStartPosOnPlayer))
		{
			CRY_ASSERT_MESSAGE(false, "CPlayerMindGameBeamEffect::RetrieveStartPositionOnPlayer(): Failed to obtain player's hand position in world-space.");
			return false;
		}
		return true;

	case ConnectionPositionOnPlayer_WaistArea:
		IF_UNLIKELY (!RetrievePlayerWaistPositions(leftStartPosOnPlayer, rightStartPosOnPlayer))
		{
			CRY_ASSERT_MESSAGE(false, "CPlayerMindGameBeamEffect::RetrieveStartPositionOnPlayer(): Failed to obtain player's waist position in world-space.");
			return false;
		}
		return true;

	case ConnectionPositionOnPlayer_Invalid:
	default:
		break;
	}
	
	// We should never get here!
	assert(false);
	return false;
}


// ===========================================================================
//	Retrieve the player's hand positions.
//
//	Out:	The player's left hand position in world-space (NULL is invalid!)
//	Out:	The player's right hand position in world-space (NULL is invalid!)
//
//	Returns:	True if the positions were returned; false if not.
//
bool CPlayerMindGameBeamEffect::RetrievePlayerHandPositions(
	Vec3* playerLeftHandPos, Vec3* playerRightHandPos) const
{
	assert(playerLeftHandPos != NULL);
	assert(playerRightHandPos != NULL);

	CActor* clientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	IF_UNLIKELY (clientActor == NULL)
	{
		return false;
	}
	IEntity* clientEntity = clientActor->GetEntity();		
	IF_UNLIKELY (clientEntity == NULL)
	{
		return false;
	}
	ICharacterInstance* clientCharacter = clientEntity->GetCharacter(0);
	IF_UNLIKELY (clientCharacter == NULL)
	{
		return false;
	}
	ISkeletonPose* skeletonPose = clientCharacter->GetISkeletonPose();
	IDefaultSkeleton& rIDefaultSkeleton = clientCharacter->GetIDefaultSkeleton();
	IF_UNLIKELY (skeletonPose == NULL)
	{
		return false;
	}

	const Matrix34& worldMatrix = clientEntity->GetWorldTM();

	const int16 leftHandJointID = rIDefaultSkeleton.GetJointIDByName(s_PlayerLeftHandJointName);
	IF_UNLIKELY (leftHandJointID < 0)
	{
		CryLog("CPlayerMindGameBeamEffect::RetrievePlayerHandPositions(): Unable to find joint '%s'.", s_PlayerLeftHandJointName);
		return false;
	}
	const QuatT& leftHandBonePose = skeletonPose->GetAbsJointByID(leftHandJointID);
	IF_LIKELY (leftHandBonePose.IsValid())
	{
		*playerLeftHandPos = worldMatrix.TransformPoint(leftHandBonePose.t);			
	}
	else
	{
		return false;
	}

	const int16 rightHandJointID = rIDefaultSkeleton.GetJointIDByName(s_PlayerRightHandJointName);
	IF_UNLIKELY (rightHandJointID < 0)
	{
		CryLog("CPlayerMindGameBeamEffect::RetrievePlayerHandPositions(): Unable to find joint '%s'.", s_PlayerRightHandJointName);
		return false;
	}
	const QuatT& rightHandBonePose = skeletonPose->GetAbsJointByID(rightHandJointID);
	IF_LIKELY (rightHandBonePose.IsValid())
	{
		*playerRightHandPos = worldMatrix.TransformPoint(rightHandBonePose.t);			
	}
	else
	{
		return false;
	}

	return true;
}


// ===========================================================================
//	Retrieve the player's waist area positions.
//
//	Out:	The player's left waist position in world-space (NULL is invalid!)
//	Out:	The player's right waist position in world-space (NULL is invalid!)
//
//	Returns:	True if the positions were returned; false if not.
//
bool CPlayerMindGameBeamEffect::RetrievePlayerWaistPositions(
	Vec3* leftStartPosOnPlayer, Vec3* rightStartPosOnPlayer) const
{
	assert(leftStartPosOnPlayer != NULL);
	assert(rightStartPosOnPlayer != NULL);

	CActor* clientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	IF_UNLIKELY (clientActor == NULL)
	{
		return false;
	}
	IEntity* clientEntity = clientActor->GetEntity();		

	const Vec3& entityWorldPos = clientEntity->GetWorldPos();

	const Vec3 verticalOffset(0.0f, 0.0f, s_PlayerWaistVerticalOffset);

	const Vec3& forwardNormal = clientEntity->GetForwardDir();
	const Vec3 upNormal(0.0f, 0.0f, 1.0f);
	Vec3 leftwardsNormal = upNormal.Cross(forwardNormal);
	leftwardsNormal.normalize();
	if (leftwardsNormal.IsZero())
	{
		*leftStartPosOnPlayer  = entityWorldPos + verticalOffset;
		*rightStartPosOnPlayer = entityWorldPos + verticalOffset;
	}
	else
	{
		*leftStartPosOnPlayer  = entityWorldPos + verticalOffset + (leftwardsNormal * s_PlayerWaistHorizontalSpacingOffset);
		*rightStartPosOnPlayer = entityWorldPos + verticalOffset - (leftwardsNormal * s_PlayerWaistHorizontalSpacingOffset);
	}

	return true;
}


// ===========================================================================
//	Retrieve the target's effect attachment positions.
//
//	Out:	The target's left attachment position in world-space (NULL is invalid!)
//	Out:	The target's right attachment position in world-space (NULL is invalid!)
//
//	Returns:	True if the positions were returned; false if not.
//
bool CPlayerMindGameBeamEffect::RetrieveTargetAttachmentPositions(
	Vec3* targetLeftAttachmentPos, Vec3* targetRightAttachmentPos) const
{
	assert(targetLeftAttachmentPos != NULL);
	assert(targetRightAttachmentPos != NULL);

	if (m_LeftConnectionAttachmentName.empty() || m_RightConnectionAttachmentName.empty())
	{
		return false;
	}

	IEntity* entity = gEnv->pEntitySystem->GetEntity(m_TargetEntityID);
	IF_UNLIKELY (entity == NULL)
	{
		return false;
	}
	ICharacterInstance* characterInstance = entity->GetCharacter(0);
	IF_UNLIKELY (characterInstance == NULL)
	{
		return false;
	}
	IAttachmentManager* attachmentManager = characterInstance->GetIAttachmentManager();
	IF_UNLIKELY (attachmentManager == NULL)
	{
		return false;
	}

	IAttachment* leftAttachment = attachmentManager->GetInterfaceByName(m_LeftConnectionAttachmentName.c_str());
	IF_UNLIKELY (leftAttachment == NULL)
	{
		CryLog("CPlayerMindGameBeamEffect::RetrieveTargetAttachmentPositions(): Unable to find attachment '%s'.", m_LeftConnectionAttachmentName.c_str());
		return false;
	}
	const QuatTS& leftPose = leftAttachment->GetAttWorldAbsolute();
	*targetLeftAttachmentPos = leftPose.t;

	IAttachment* rightAttachment = attachmentManager->GetInterfaceByName(m_RightConnectionAttachmentName.c_str());
	IF_UNLIKELY (rightAttachment == NULL)
	{
		CryLog("CPlayerMindGameBeamEffect::RetrieveTargetAttachmentPositions(): Unable to find attachment '%s'.", m_RightConnectionAttachmentName.c_str());
		return false;
	}
	const QuatTS& rightPose = rightAttachment->GetAttWorldAbsolute();
	*targetRightAttachmentPos = rightPose.t;

	return true;
}

