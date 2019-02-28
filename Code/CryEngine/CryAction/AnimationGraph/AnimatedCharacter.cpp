// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimatedCharacter.h"
#include "CryAction.h"
#include "AnimationGraphCVars.h"
#include "PersistantDebug.h"
#include "GameObjects/MannequinObject.h"
#include <CryAnimation/IFacialAnimation.h>

#include <CryExtension/CryCreateClassInstance.h>

#include "CryActionCVars.h"
#include "Mannequin/MannequinAGState.h"

#include "Animation/PoseAligner/PoseAligner.h"

#include <IViewSystem.h>

char* g_szMCMString[eMCM_COUNT] = { "Undefined", "Entity", "Animation", "DecoupledCatchUp", "ClampedEntity", "SmoothedEntity", "AnimationHCollision" };
char* g_szColliderModeString[eColliderMode_COUNT] = { "Undefined", "Disabled", "GroundedOnly", "Pushable", "NonPushable", "PushesPlayersOnly", "Spectator" };
char* g_szColliderModeLayerString[eColliderModeLayer_COUNT] = { "AG", "Game", "Script", "FG", "Anim", "Sleep", "Debug" };

#define ANIMCHAR_MEM_DEBUG // Memory Allocation Tracking
#undef  ANIMCHAR_MEM_DEBUG

#define ENABLE_NAN_CHECK

#ifndef _RELEASE
	#define VALIDATE_CHARACTER_PTRS ValidateCharacterPtrs();
#else //_RELEASE
	#define VALIDATE_CHARACTER_PTRS
#endif //_RELEASE

#undef CHECKQNAN_FLT
#if defined(ENABLE_NAN_CHECK)
	#define CHECKQNAN_FLT(x) \
	  assert(((*(unsigned*)&(x)) & 0xff000000) != 0xff000000u && (*(unsigned*)&(x) != 0x7fc00000))
#else
	#define CHECKQNAN_FLT(x) (void*)0
#endif

#ifndef CHECKQNAN_VEC
	#define CHECKQNAN_VEC(v) \
	  CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)
#endif

static CAnimationPlayerProxy s_defaultAnimPlayerProxy;
static CTimeValue lastTime;

namespace
{
struct CRY_ALIGN(8) SAnimRoot
{
	f32 m_NormalizedTimeAbs;
	f32 m_KeyTimeAbs;
	QuatT m_TransRot;

	SAnimRoot()
	{
		m_NormalizedTimeAbs = 0;
		m_KeyTimeAbs = 0;
		m_TransRot.SetIdentity();
	};

};

bool CheckNAN(const f32& f)
{
	return ((*(unsigned*)&(f)) & 0xff000000) != 0xff000000u && (*(unsigned*)&(f) != 0x7fc00000);
}

bool CheckNANVec(Vec3& v, IEntity* pEntity)
{
	if (!CheckNAN(v.x) || !CheckNAN(v.y) || !CheckNAN(v.z))
	{
		if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
		{
			int numAnims = pCharacter->GetISkeletonAnim()->GetNumAnimsInFIFO(0);
			for (int i = 0; i < numAnims; ++i)
			{
				const CAnimation& anim = pCharacter->GetISkeletonAnim()->GetAnimFromFIFO(0, i);
				CryLogAlways("NAN on AnimationID:%d, model:%s, entity:%s", anim.GetAnimationId(), pCharacter->GetFilePath(), pEntity->GetName());
			}
		}

		v.zero();
		return false;
	}
	return true;
}

bool CheckNANVec(const Vec3& v, IEntity* pEntity)
{
	Vec3 v1(v);
	return CheckNANVec(v1, pEntity);
}

template<class T, class U> bool CloneBinary(T&& outInstance, U&& inInstance)
{
	return gEnv->pSystem->GetArchiveHost()->CloneBinary(Serialization::SStruct(std::forward<T>(outInstance)), Serialization::SStruct(std::forward<U>(inInstance)));
}

struct SMannequinSettings
{
	string actionController;
	string animDatabase1P;
	string animDatabase3P;
	string soundDatabase;
	bool   useMannequinAGState;
};

//! Initializes SMannequinSettings based on existing entity components.
static SMannequinSettings RetrieveMannequinSettingsFromEntity(IEntity& entity)
{
	const auto pScriptComponent = entity.GetComponent<IEntityScriptComponent>();
	if (pScriptComponent)
	{
		const auto pScriptTable = pScriptComponent->GetScriptTable();
		if (pScriptTable)
		{
			if (pScriptTable->HaveValue("ActionController") || pScriptTable->HaveValue("fileActionController"))
			{
				SMannequinSettings settings;

				if (!pScriptTable->GetValue("ActionController", settings.actionController))
				{
					pScriptTable->GetValue("fileActionController", settings.actionController);
				}

				if (!pScriptTable->GetValue("AnimDatabase3P", settings.animDatabase3P))
				{
					pScriptTable->GetValue("fileAnimDatabase3P", settings.animDatabase3P);
				}

				if (!pScriptTable->GetValue("AnimDatabase1P", settings.animDatabase1P))
				{
					pScriptTable->GetValue("fileAnimDatabase1P", settings.animDatabase1P);
				}

				if (!pScriptTable->GetValue("SoundDatabase", settings.soundDatabase))
				{
					pScriptTable->GetValue("fileSoundDatabase", settings.soundDatabase);
				}

				pScriptTable->GetValue("UseMannequinAGState", settings.useMannequinAGState);

				return settings;
			}
		}
	}

	IEntityComponent* const pMannequinObject = entity.GetComponent<CMannequinObject>();
	if (pMannequinObject)
	{
		struct SEntityPropertyGroupSerialier
		{
			void Serialize(Serialization::IArchive& archive) { propertyGroup.SerializeProperties(archive); }

			IEntityPropertyGroup& propertyGroup;
		};

		struct SMannequinObjectPropertyGroupToMannequinSettingsSerializer
		{
			void Serialize(Serialization::IArchive& archive)
			{
				archive(mannequinSettings.actionController, "actionController");
				archive(mannequinSettings.animDatabase3P, "animationDatabase");

				if (archive.isInput())
				{
					mannequinSettings.animDatabase1P = "";
					mannequinSettings.soundDatabase = "";
					mannequinSettings.useMannequinAGState = false;
				}
			}

			SMannequinSettings& mannequinSettings;
		};

		SMannequinSettings settings;
		CloneBinary(SMannequinObjectPropertyGroupToMannequinSettingsSerializer{ settings }, SEntityPropertyGroupSerialier{ *pMannequinObject->GetPropertyGroup() });
		return settings;
	}

	return SMannequinSettings();
}

} // anonymous namespace

namespace AC
{
void RegisterEvents(IGameObjectExtension& goExt, IGameObject& gameObject)
{
	const int eventToRegister[] =
	{
		eGFE_QueueRagdollCreation,
		eGFE_OnCollision,
		eGFE_ResetAnimationGraphs,
		eGFE_QueueBlendFromRagdoll,
		eGFE_EnablePhysics,
		eGFE_DisablePhysics
	};
	gameObject.UnRegisterExtForEvents(&goExt, NULL, 0);
	gameObject.RegisterExtForEvents(&goExt, eventToRegister, sizeof(eventToRegister) / sizeof(int));
}
}

CAnimatedCharacter::CAnimatedCharacter() : m_listeners(1)
{
	InitVars();

	m_debugHistoryManager = gEnv->pGameFramework->CreateDebugHistoryManager();

	CryCreateClassInstance(CPoseAlignerC3::GetCID(), m_pPoseAligner);
}

CAnimatedCharacter::~CAnimatedCharacter()
{
	SAFE_RELEASE(m_pMannequinAGState);

	if (m_debugHistoryManager)
		m_debugHistoryManager->Release();

	DisableRigidCollider();
	DestroyExtraSolidCollider();

	CRY_ASSERT_MESSAGE(!m_pActionController, "ActionController should already be deleted. If not, the Exit of actions in the destructor could involve invalid objects.");
	DeleteActionController();
}

void CAnimatedCharacter::InitVars()
{
	m_fPrevInertia = 0.0f;
	m_fPrevInertiaAccel = 0.0f;
	m_fPrevTimeImpulseRecover = 0.0f;
	m_facialAlertness = 0;
	m_currentStance = -1;
	m_requestedStance = -1;
	m_stanceQuery = 0;
	m_disallowLookIKFlags = 0;
	m_allowAimIk = true;
	m_curFrameID = -1;
	m_lastResetFrameId = -1;
	m_updateGrabbedInputFrameID = -1;
	m_moveRequestFrameID = -1;
	m_lastSerializeReadFrameID = 0;
	m_lastPostProcessUpdateFrameID = 0;
	m_lastAnimProcFrameID = 0;
	m_shadowCharacterSlot = -1;
	m_hasShadowCharacter = false;
	m_bSimpleMovementSetOnce = false;
	m_curWeaponRaisedPose = eWeaponRaisedPose_None;
	m_isPlayer = false;
	m_curFrameTime = 0.0f;
	m_prevFrameTime = 0.0f;
	m_curFrameTimeOriginal = 0.0f;
	m_reqLocalEntAxxNextIndex = 0;
	m_smoothedActualEntVelo = ZERO;
	m_smoothedAmountAxx = 0.0f;
	m_avgLocalEntAxx = ZERO;
	m_requestedEntMoveDirLH4 = 0;
	m_actualEntMoveDirLH4 = 0;
	m_colliderMode = eColliderMode_Undefined;
	m_simplifiedAGSpeedInputsRequested = false;
	m_simplifiedAGSpeedInputsActual = false;
	m_requestedEntityMovementType = RequestedEntMovementType_Undefined;
	m_requestedIJump = 0;
	m_bDisablePhysicalGravity = false;
	m_simplifyMovement = false;
	m_forceDisableSlidingContactEvents = false;
	m_noMovementTimer = 0.0f;
	m_actualEntSpeed = 0.0f;
	m_actualEntSpeedHorizontal = 0.0f;
	m_actualEntMovementDirHorizontal.zero();
	m_actualEntVelocityHorizontal.zero();
	m_actualEntAccelerationHorizontal.zero();
	m_actualEntTangentialAcceleration = 0.0f;
	m_actualEntMovement.SetIdentity();
	m_entLocation.SetIdentity();
	m_entTeleportMovement.SetIdentity();
	m_pAnimTarget = NULL;
	m_pCharacter = NULL;
	m_pSkeletonAnim = NULL;
	m_pSkeletonPose = NULL;
	m_pShadowCharacter = NULL;
	m_pShadowSkeletonAnim = NULL;
	m_pShadowSkeletonPose = NULL;
	m_wasInAir = false;
	m_fallMaxHeight = 0.0f;
	m_landBobTime = 0.0f;
	m_totalLandBob = 0.0f;
	m_noMovementOverrideExternal = false;
	m_bAnimationGraphStatePaused = true;
	m_fJumpSmooth = 1.0f;
	m_fJumpSmoothRate = 1.0f;
	m_fGroundSlopeMoveDirSmooth = 0.0f;
	m_fGroundSlopeMoveDirRate = 0.0f;
	m_fGroundSlope = 0.0f;
	m_fGroundSlopeSmooth = 0.0f;
	m_fGroundSlopeRate = 0.0f;
	m_fRootHeightSmooth = 0.0f;
	m_fRootHeightRate = 0.0f;
	m_forcedRefreshColliderMode = false;
	m_prevAnimPhaseHash = 0;
	m_prevAnimEntOffsetHash = 0.0f;
	m_prevMoveVeloHash = 0.0f;
	m_prevMoveJump = 0;
	m_collisionFrameID = -1;
	m_collisionNormalCount = 0;
	m_pFeetColliderPE = NULL;
	m_debugHistoryManager = NULL;
	m_pRigidColliderPE = NULL;
	m_characterCollisionFlags = geom_colltype_player;
	m_groundAlignmentParams.InitVars();
	m_doMotionParams = true;
	m_pAnimContext = NULL;
	m_pActionController = NULL;
	m_pAnimDatabase1P = NULL;
	m_pAnimDatabase3P = NULL;
	m_pSoundDatabase = NULL;
	m_bPendingRagdoll = false;
	m_pMannequinAGState = NULL;
	m_proxiesInitialized = false;

	for (int layer = 0; layer < eAnimationGraphLayer_COUNT; ++layer)
	{
		m_pAnimationPlayerProxies[layer] = &s_defaultAnimPlayerProxy;
	}

	for (int slot = 0; slot < eMCMSlot_COUNT; ++slot)
	{
		m_movementControlMethodTags[slot] = "InitVars";
		m_movementControlMethod[eMCMComponent_Horizontal][slot] = eMCM_Entity;
		m_movementControlMethod[eMCMComponent_Vertical][slot] = eMCM_Entity;
	}
	for (int slot = eMCMSlotStack_Begin; slot < eMCMSlotStack_End; ++slot)
	{
		m_movementControlMethod[eMCMComponent_Horizontal][slot] = eMCM_Undefined;
		m_movementControlMethod[eMCMComponent_Vertical][slot] = eMCM_Undefined;
	}
	m_currentMovementControlMethodTags[eMCMComponent_Horizontal] = m_movementControlMethodTags[eMCMSlot_Cur];
	m_currentMovementControlMethodTags[eMCMComponent_Vertical] = m_movementControlMethodTags[eMCMSlot_Cur];
	m_elapsedTimeMCM[eMCMComponent_Horizontal] = 0.0f;
	m_elapsedTimeMCM[eMCMComponent_Vertical] = 0.0f;

	for (int layer = 0; layer < eColliderModeLayer_COUNT; layer++)
	{
		m_colliderModeLayersTag[layer] = "InitVars";
		m_colliderModeLayers[layer] = eColliderMode_Undefined;
	}

	for (int i = 0; i < NumAxxSamples; i++)
	{
		m_reqLocalEntAxx[i].zero();
		m_reqEntVelo[i].zero();
		m_reqEntTime[i].SetValue(0);
	}

	m_fDesiredMoveSpeedSmoothQTX = 0;
	m_fDesiredMoveSpeedSmoothRateQTX = 0;
	m_fDesiredTurnSpeedSmoothQTX = 0;
	m_fDesiredTurnSpeedSmoothRateQTX = 0;
	m_fDesiredStrafeSmoothQTX = Vec2(0, 0);
	m_fDesiredStrafeSmoothRateQTX = Vec2(0, 0);

	m_collisionNormal[0].zero();
	m_collisionNormal[1].zero();
	m_collisionNormal[2].zero();
	m_collisionNormal[3].zero();

	m_lastAnimationUpdateFrameId = 0;
	m_inGrabbedState = false;

	m_hasForcedMovement = false;
	m_forcedMovementRelative.SetIdentity();

	m_hasForcedOverrideRotation = false;
	m_forcedOverrideRotationWorld.SetIdentity();

	m_useMannequinAGState = false;
}

bool CAnimatedCharacter::Init(IGameObject* pGameObject)
{
#ifdef ANIMCHAR_MEM_DEBUG
	CCryAction::GetCryAction()->DumpMemInfo("CAnimatedCharacter::Init %p start", pGameObject);
#endif

	SetGameObject(pGameObject);

	InitializeMannequin();

#ifdef ANIMCHAR_MEM_DEBUG
	CCryAction::GetCryAction()->DumpMemInfo("CAnimatedCharacter::Init %p end", pGameObject);
#endif

	return true;
}

void CAnimatedCharacter::PostInit(IGameObject* pGameObject)
{
	AC::RegisterEvents(*this, *pGameObject);

	m_pComponentPrepareCharForUpdate = GetEntity()->GetOrCreateComponentClass<CAnimatedCharacterComponent_PrepareAnimatedCharacterForUpdate>();
	m_pComponentPrepareCharForUpdate->SetAnimatedCharacter(this);
	
	auto componentMoveRequest = GetEntity()->GetOrCreateComponentClass<CAnimatedCharacterComponent_GenerateMoveRequest>();
	componentMoveRequest->SetAnimatedCharacter(this);

	auto componentStartAnimProc = GetEntity()->GetOrCreateComponentClass<CAnimatedCharacterComponent_StartAnimProc>();
	componentStartAnimProc->SetAnimatedCharacter(this);

	m_proxiesInitialized = true;

	pGameObject->EnableUpdateSlot(this, 0);
	pGameObject->SetUpdateSlotEnableCondition(this, 0, eUEC_Visible);
	pGameObject->EnablePhysicsEvent(true, eEPE_OnCollisionLogged);
}

bool CAnimatedCharacter::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
#ifdef ANIMCHAR_MEM_DEBUG
	CCryAction::GetCryAction()->DumpMemInfo("CAnimatedCharacter::ReloadExtension %p start", pGameObject);
#endif

	AC::RegisterEvents(*this, *pGameObject);
	ResetGameObject();
	ResetInertiaCache();

	SAFE_RELEASE(m_pMannequinAGState);

	for (int layer = 0; layer < eAnimationGraphLayer_COUNT; ++layer)
	{
		m_pAnimationPlayerProxies[layer] = &s_defaultAnimPlayerProxy;
	}

	InitializeMannequin();

#ifdef ANIMCHAR_MEM_DEBUG
	CCryAction::GetCryAction()->DumpMemInfo("CAnimatedCharacter::ReloadExtension %p end", pGameObject);
#endif

	m_pComponentPrepareCharForUpdate->ClearQueuedRotation();

	return true;
}

bool CAnimatedCharacter::InitializeMannequin()
{
	SMannequinSettings mannequinSetup = RetrieveMannequinSettingsFromEntity(*GetEntity());
	if (mannequinSetup.actionController.empty())
	{
		return false;
	}

	DeleteActionController();
	SetActionController(mannequinSetup.actionController);
	if (m_pActionController)
	{
		IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
		m_pAnimDatabase3P = mannequinSys.GetAnimationDatabaseManager().Load(mannequinSetup.animDatabase3P);
		m_pAnimDatabase1P = mannequinSys.GetAnimationDatabaseManager().Load(mannequinSetup.animDatabase1P);
		m_pSoundDatabase = mannequinSys.GetAnimationDatabaseManager().Load(mannequinSetup.soundDatabase);
	}

	m_useMannequinAGState = mannequinSetup.useMannequinAGState;
	if (m_useMannequinAGState)
	{
		CRY_ASSERT(m_pActionController);

		m_pMannequinAGState = new MannequinAG::CMannequinAGState();
		m_pMannequinAGState->SetAnimatedCharacter(this, 0, NULL);
		m_pMannequinAGState->AddListener("animchar", this);
	}

	ResetVars();

	return true;
}

void CAnimatedCharacter::Release()
{
	delete this;
}

void CAnimatedCharacter::FullSerialize(TSerialize ser)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Animated character serialization");

#define SerializeMember(member)                    ser.Value( # member, member)
#define SerializeMemberType(type, member)          { type temp = member; ser.Value( # member, temp); if (isReading) member = temp; }
#define SerializeNamedType(type, name, type2, var) { type temp = var; ser.Value(name, temp); if (isReading) var = (type2)temp; }

	bool isReading = ser.IsReading();

	if (isReading)
		m_lastSerializeReadFrameID = m_curFrameID;
	/*gEnv->pRenderer->GetFrameID()*/;

	// TODO: Find out what defines an inactive character and what can be safely omitted in that case.
	// Is it safe to ignore inactive entities? Don't they have any active information?
	bool bEntityActive = true; //GetEntity()->IsActive();
	if (!ser.BeginOptionalGroup("AnimatedCharacter", bEntityActive))
		return;

	if (isReading)
		ResetVars();

	SerializeMember(m_shadowCharacterSlot);
	SerializeMember(m_hasShadowCharacter);

	SerializeMember(m_prevEntLocation.t);
	SerializeMember(m_prevEntLocation.q);

	SerializeMember(m_actualEntMovement.t);
	SerializeMember(m_actualEntMovement.q);

	SerializeMember(m_actualEntSpeed);
	SerializeMember(m_actualEntSpeedHorizontal);
	if (isReading)
	{
		m_actualEntMovementDirHorizontal = Vec2(m_actualEntMovement.t).GetNormalizedSafe(Vec2Constants<float>::fVec2_Zero);
		m_actualEntVelocityHorizontal = m_actualEntMovementDirHorizontal * m_actualEntSpeedHorizontal;
	}
	SerializeMember(m_actualEntAccelerationHorizontal);

	SerializeMember(m_requestedEntityMovement.t);
	SerializeMember(m_requestedEntityMovement.q);

	if (m_pMannequinAGState)
		m_pMannequinAGState->Serialize(ser);

	if (isReading)
		ResetInertiaCache();

	SerializeMember(m_currentStance);

	SerializeMember(m_disallowLookIKFlags);
	SerializeMemberType(bool, m_allowAimIk);

	RefreshAnimTarget();
	bool bTemp = m_groundAlignmentParams.IsFlag(eGA_Enable);
	ser.Value("m_enableGroundAlignment", bTemp);
	m_groundAlignmentParams.SetFlag(eGA_Enable, bTemp);

	string cml("m_colliderModeLayer");
	for (int layer = 0; layer < eColliderModeLayer_COUNT; layer++)
	{
		uint32 tempVal = m_colliderModeLayers[layer];
		ser.Value(cml + g_szColliderModeLayerString[layer], tempVal);
		m_colliderModeLayers[layer] = EColliderMode(tempVal);
	}

	char mcm[32] = "m_movementControlMethod";
	static const int basicStringLength = 23;
	assert(strlen(mcm) == basicStringLength);
	for (int slot = 0; slot < eMCMSlot_COUNT; ++slot)
	{
		mcm[basicStringLength] = 'H';
		itoa(slot, &mcm[basicStringLength + 1], 10);
		SerializeNamedType(uint8, mcm, EMovementControlMethod, m_movementControlMethod[eMCMComponent_Horizontal][slot]);

		mcm[basicStringLength] = 'V';
		itoa(slot, &mcm[basicStringLength + 1], 10);
		SerializeNamedType(uint8, mcm, EMovementControlMethod, m_movementControlMethod[eMCMComponent_Vertical][slot]);
	}
	ser.Value("m_elapsedTimeMCM_Horizontal", m_elapsedTimeMCM[eMCMComponent_Horizontal]);
	ser.Value("m_elapsedTimeMCM_Vertical", m_elapsedTimeMCM[eMCMComponent_Vertical]);

	ser.EndGroup(); //AnimatedCharacter
}

void CAnimatedCharacter::PostSerialize()
{
	if (m_hasShadowCharacter)
	{
		InitShadowCharacter();
	}
}

void CAnimatedCharacter::Update(SEntityUpdateContext& ctx, int slot)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	//assert(!m_simplifyMovement); // If we have simplified movement, the this GameObject extension should not be updated here.

	assert(m_entLocation.IsValid());
	//assert(m_colliderModeLayers[eColliderModeLayer_ForceSleep] == eColliderMode_Undefined);

	if (m_simplifyMovement)
	{
		GetCurrentEntityLocation();
	}

	float EntRotZ = RAD2DEG(m_entLocation.q.GetRotZ());

	assert(m_entLocation.IsValid());

	SetAnimationPostProcessParameters();

	assert(m_entLocation.IsValid());

	/*
	   This code which was around here is highly suspect; I suggest we fix 'standing on moving platforms' in a better way

	   if (GetMCMV() == eMCM_SmoothedEntity)
	   {
	   static float elevationBlendDuration = 0.1f;
	   float elevationBlend = clamp_tpl((float)m_curFrameTime / elevationBlendDuration, 0.0f, 1.0f);

	   // Blend out vertical lag when not moving (e.g. standing still on a vertically moving platform).
	   float velocityFractionH = clamp_tpl(m_actualEntSpeedHorizontal / 0.1f, 0.0f, 1.0f);
	   elevationBlend = LERP(1.0f, elevationBlend, velocityFractionH);

	   // Blend out vertical lag when not moving (e.g. standing still on a vertically moving platform).
	   float prevFrameTimeInv = ((float)m_prevFrameTime > 0.0f) ? (1.0f / (float)m_prevFrameTime) : 0.0f;
	   float actualEntVelocityZ = m_actualEntMovement.t.z * prevFrameTimeInv;
	   float velocityFractionZ = clamp_tpl(actualEntVelocityZ / 1.0f, 0.0f, 1.0f);
	   elevationBlend = LERP(elevationBlend, 1.0f, velocityFractionZ);

	   m_animLocation.q = animLocationClamped.q;
	   m_animLocation.t.x = animLocationClamped.t.x;
	   m_animLocation.t.y = animLocationClamped.t.y;
	   m_animLocation.t.z = LERP(m_animLocation.t.z, animLocationClamped.t.z, elevationBlend);
	   }
	 */

	assert(m_entLocation.IsValid());

	/*
	   CRYANIMATION2

	   Broke procedural leaning. The animrenderlocation used to contain the procedural leaning factor:

	   if (EnableProceduralLeaning()
	   && ((m_pAnimTarget == NULL) || (!m_pAnimTarget->activated))
	   && ((GetMCMH() == eMCM_DecoupledCatchUp) || CAnimationGraphCVars::Get().m_nonDecoupledLeaning)) // This was added to primarily prevent deviation while parachuting.
	   {
	   QuatT proceduralLeaning = CalculateProceduralLeaning();
	   animRenderLocation = animRenderLocation * proceduralLeaning;
	   }
	 */

	VALIDATE_CHARACTER_PTRS

	  DebugRenderCurLocations();

#ifdef _DEBUG
	DebugGraphQT(m_entLocation, "eDH_EntLocationPosX", "eDH_EntLocationPosY", "eDH_EntLocationOriZ");

	//DebugDisplayNewLocationsAndMovements(wantedEntLocationClamped, wantedEntMovement, wantedAnimLocationClamped, wantedAnimMovement, m_curFrameTime);
#endif
}

void CAnimatedCharacter::ForceRefreshPhysicalColliderMode()
{
	m_forcedRefreshColliderMode = true;
}

void CAnimatedCharacter::RequestPhysicalColliderMode(EColliderMode mode, EColliderModeLayer layer, const char* tag /* = NULL */)
{
	bool update = (m_colliderModeLayers[layer] != mode);
	if (update || m_forcedRefreshColliderMode)
	{
		m_colliderModeLayersTag[layer] = tag;
		m_colliderModeLayers[layer] = mode;
		UpdatePhysicalColliderMode();
	}
}

void CAnimatedCharacter::HandleEvent(const SGameObjectEvent& event)
{
	switch (event.event)
	{
	case eGFE_QueueBlendFromRagdoll:
		SetBlendFromRagdollizeParams(event);
		break;
	case eGFE_QueueRagdollCreation:
		SetRagdollizeParams(event);
		break;
	case eGFE_OnCollision:
		{
			const EventPhysCollision* pCollision = static_cast<const EventPhysCollision*>(event.ptr);

			// Ignore bullets and insignificant particles, etc.
			// TODO: This early-out condition should ideally be done on a higher level,
			// to avoid even touching this memory for all bullets and stuff.
			if (pCollision->pEntity[0]->GetType() == PE_PARTICLE)
				break;

			if (m_curFrameID > m_collisionFrameID)
			{
				m_collisionNormalCount = 0;
				m_collisionNormal[0].zero();
				m_collisionNormal[1].zero();
				m_collisionNormal[2].zero();
				m_collisionNormal[3].zero();
			}

			if ((m_curFrameID < m_collisionFrameID) || (m_collisionNormalCount >= 4) || (m_curFrameID <= 10))
				break;

			// Both entities in a collision recieve the same direction of the normal.
			// We need to flip the normal if this is the second entity.
			IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysics();
			if (pPhysEnt == pCollision->pEntity[0])
				m_collisionNormal[m_collisionNormalCount] = pCollision->n;
			else if (pPhysEnt == pCollision->pEntity[1])
				m_collisionNormal[m_collisionNormalCount] = -pCollision->n;
			/*
			      // This might happen for faked collisions, such as punches.
			      else
			        assert(!"Entity recieved collision event without being part of collision!");
			 */
			// We only care about the horizontal part, so we remove the vertical component for simplicity.
			m_collisionNormal[m_collisionNormalCount].z = 0.0f;
			if (!m_collisionNormal[m_collisionNormalCount].IsZero())
			{
				m_collisionNormal[m_collisionNormalCount].Normalize();

#if _DEBUG && defined(USER_david)
				CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
				if ((pPD != NULL) && true)
				{
					pPD->Begin(UNIQUE("AnimatedCharacter.HandleEvent.CollisionNormal"), false);
					pPD->AddSphere(pCollision->pt, 0.02f, ColorF(1, 0.75f, 0.0f, 1), 0.1f);
					pPD->AddLine(pCollision->pt, pCollision->pt + m_collisionNormal[m_collisionNormalCount] * 0.5f, ColorF(1, 0.75f, 0.0f, 1), 0.1f);
				}
#endif

				m_collisionFrameID = m_curFrameID;
				m_collisionNormalCount++;

			}
		}
		break;

	case eGFE_ResetAnimationGraphs:
		if (m_pMannequinAGState)
			m_pMannequinAGState->Reset();
		break;
	case eGFE_EnablePhysics:
		RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_ForceSleep, "eGFE_EnablePhysics");
		break;
	case eGFE_DisablePhysics:
		RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_ForceSleep, "eGFE_DisablePhysics");
		break;
	}
	// AnimationControlled/GameControlled: DEPRECATED in favor of MovementControlMethod controlled by AnimGraph.
}

void CAnimatedCharacter::ResetInertiaCache()
{
	// Synch our cached inertia values with the living entity values
	pe_player_dynamics dynParams;
	IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysics();
	if (pPhysEnt && pPhysEnt->GetParams(&dynParams))
	{
		m_fPrevInertia = dynParams.kInertia;
		m_fPrevInertiaAccel = dynParams.kInertiaAccel;
		m_fPrevTimeImpulseRecover = dynParams.timeImpulseRecover;
	}
}

void CAnimatedCharacter::ResetState()
{
	if (m_pMannequinAGState)
		m_pMannequinAGState->Reset();

	if (m_pActionController)
		m_pActionController->Reset();
	ResetVars();
	ResetInertiaCache();
}

void CAnimatedCharacter::ResetVars()
{
	IEntity* pEntity = GetEntity();
	assert(pEntity);

	m_params.Reset();

	m_facialAlertness = 0;

	m_currentStance = -1;
	m_requestedStance = -1;
	m_stanceQuery = 0;

	m_disallowLookIKFlags = 0;
	m_allowAimIk = true;

	// Why -1, why not 0?
	m_curFrameID = -1;
	m_lastResetFrameId = gEnv->nMainFrameID;
	m_updateGrabbedInputFrameID = -1;
	m_moveRequestFrameID = -1;
	m_lastSerializeReadFrameID = 0;
	m_lastPostProcessUpdateFrameID = 0;
	m_lastAnimProcFrameID = 0;

	m_shadowCharacterSlot = -1;
	m_hasShadowCharacter = false;

	m_bSimpleMovementSetOnce = false;

	m_curWeaponRaisedPose = eWeaponRaisedPose_None;

	m_moveRequest.type = eCMT_None;
	m_moveRequest.velocity.zero();
	m_moveRequest.rotation.SetIdentity();
	m_moveRequest.prediction.Reset();
	m_moveRequest.jumping = false;
	m_moveRequest.allowStrafe = false;
	m_moveRequest.proceduralLeaning = false;

	m_inGrabbedState = false;

	m_groundAlignmentParams.InitVars();

	// Init IK Disable Distance to the CVar value if specified.
	const float ikDisableDistance = CAnimationGraphCVars::Get().m_distanceForceNoIk;
	m_groundAlignmentParams.ikDisableDistanceSqr = (float)__fsel(-ikDisableDistance, m_groundAlignmentParams.ikDisableDistanceSqr, sqr(ikDisableDistance));

	m_isPlayer = false;
	if (pEntity)
	{
		IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
		assert(pActorSystem != NULL);
		IActor* pActor = pActorSystem->GetActor(pEntity->GetId());

		//No longer asserting, as the animated character is no longer just used on actors
		if (pActor != NULL)
		{
			m_isPlayer = pActor->IsPlayer();

			// Turn on by default for 3rd person in singleplayer.
			m_groundAlignmentParams.SetFlag(eGA_PoseAlignerUseRootOffset, pActor->IsThirdPerson() && !gEnv->bMultiplayer);
		}
	}

	m_curFrameStartTime.SetValue(0);

	m_curFrameTime = 0.0f;
	m_prevFrameTime = 0.0f;
	m_curFrameTimeOriginal = 0.0f;

	if (pEntity)
	{
		m_entLocation.t = pEntity->GetWorldPos();
		m_entLocation.q = pEntity->GetWorldRotation();

		m_prevEntLocation = m_entLocation;
	}
	else
	{
		m_entLocation.SetIdentity();
		m_prevEntLocation.SetIdentity();
	}

	m_expectedEntMovement.zero();
	m_actualEntMovement.SetIdentity();
	m_entTeleportMovement.SetIdentity();

	for (int i = 0; i < NumAxxSamples; i++)
	{
		m_reqLocalEntAxx[i].zero();
		m_reqEntVelo[i].zero();
		m_reqEntTime[i].SetValue(0);
	}
	m_reqLocalEntAxxNextIndex = 0;
	m_smoothedActualEntVelo.zero();
	m_smoothedAmountAxx = 0.0f;
	m_avgLocalEntAxx.zero();

	m_requestedEntMoveDirLH4 = 0;
	m_actualEntMoveDirLH4 = 0;

	for (int slot = 0; slot < eMCMSlot_COUNT; ++slot)
	{
		m_movementControlMethodTags[slot] = "ResetVars";
		m_movementControlMethod[eMCMComponent_Horizontal][slot] = eMCM_Entity;
		m_movementControlMethod[eMCMComponent_Vertical][slot] = eMCM_Entity;
	}
	for (int slot = eMCMSlotStack_Begin; slot < eMCMSlotStack_End; ++slot)
	{
		m_movementControlMethod[eMCMComponent_Horizontal][slot] = eMCM_Undefined;
		m_movementControlMethod[eMCMComponent_Vertical][slot] = eMCM_Undefined;
	}
	m_currentMovementControlMethodTags[eMCMComponent_Horizontal] = m_movementControlMethodTags[eMCMSlot_Cur];
	m_currentMovementControlMethodTags[eMCMComponent_Vertical] = m_movementControlMethodTags[eMCMSlot_Cur];
	m_elapsedTimeMCM[eMCMComponent_Horizontal] = 0.0f;
	m_elapsedTimeMCM[eMCMComponent_Vertical] = 0.0f;

	for (int layer = 0; layer < eColliderModeLayer_COUNT; layer++)
	{
		m_colliderModeLayersTag[layer] = "ResetVars";
		m_colliderModeLayers[layer] = eColliderMode_Undefined;
	}
	m_colliderMode = eColliderMode_Undefined;

	m_simplifiedAGSpeedInputsRequested = false;
	m_simplifiedAGSpeedInputsActual = false;

	m_requestedEntityMovement.SetIdentity();
	m_requestedEntityMovementType = RequestedEntMovementType_Undefined;
	m_requestedIJump = 0;
	m_bDisablePhysicalGravity = false;

	m_simplifyMovement = false;
	m_forceDisableSlidingContactEvents = false;
	m_noMovementTimer = 0.0f;

	m_actualEntSpeed = 0.0f;
	m_actualEntSpeedHorizontal = 0.0f;
	m_actualEntMovementDirHorizontal.zero();
	m_actualEntVelocityHorizontal.zero();
	m_actualEntAccelerationHorizontal.zero();
	m_actualEntTangentialAcceleration = 0.0f;

	m_pAnimTarget = NULL;
	m_pCharacter = NULL;
	m_pSkeletonAnim = NULL;
	m_pSkeletonPose = NULL;
	m_pShadowCharacter = NULL;
	m_pShadowSkeletonAnim = NULL;
	m_pShadowSkeletonPose = NULL;

	m_wasInAir = false;
	m_fallMaxHeight = 0.0f;
	m_landBobTime = 0.0f;
	m_totalLandBob = 0.0f;

	m_noMovementOverrideExternal = false;

	m_fJumpSmooth = 1.0f;
	m_fJumpSmoothRate = 1.0f;
	m_fGroundSlopeMoveDirSmooth = 0.0f;
	m_fGroundSlopeMoveDirRate = 0.0f;
	m_fGroundSlope = 0.0f;
	m_fGroundSlopeSmooth = 0.0f;
	m_fGroundSlopeRate = 0.0f;
	m_fRootHeightSmooth = 0.0f;
	m_fRootHeightRate = 0.0f;

	m_forcedRefreshColliderMode = false;
	m_bPendingRagdoll = false;
	m_ragdollParams = SRagdollizeParams();

	m_prevAnimPhaseHash = 0;
	m_prevAnimEntOffsetHash = 0.0f;
	m_prevMoveVeloHash = 0.0f;
	m_prevMoveJump = 0;

	m_moveOverride_useAnimXY = false;
	m_moveOverride_useAnimZ = false;
	m_moveOverride_useAnimRot = false;

	m_collisionFrameID = -1;
	m_collisionNormalCount = 0;
	m_collisionNormal[0].zero();
	m_collisionNormal[1].zero();
	m_collisionNormal[2].zero();
	m_collisionNormal[3].zero();

	if (m_pFeetColliderPE)
	{
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pFeetColliderPE);
		m_pFeetColliderPE = NULL;
	}

	DisableRigidCollider();

	if (m_debugHistoryManager)
		m_debugHistoryManager->Clear();

	if (pEntity)
	{
		// Disable physics as default when reviving/resetting characters.
		// Only a few frames after reset will non-disabled collider be allowed in UpdatePhysicalColliderMode().
		IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
		if (pPhysEnt)
		{
			pe_player_dynamics pd;
			pe_params_part pp;
			pd.bActive = 0;
			pp.flagsAND = ~(m_characterCollisionFlags);
			pPhysEnt->SetParams(&pd);
			pPhysEnt->SetParams(&pp);
		}
	}

	DestroyExtraSolidCollider();

	m_hasForcedMovement = false;
	m_forcedMovementRelative.SetIdentity();
	m_hasForcedOverrideRotation = false;
	m_forcedOverrideRotationWorld.SetIdentity();

	if (m_proxiesInitialized)
	{
		m_pComponentPrepareCharForUpdate->ClearQueuedRotation();
	}
}

void CAnimatedCharacter::SetOutput(const char* output, const char* value)
{
}

void CAnimatedCharacter::QueryComplete(TAnimationGraphQueryID queryID, bool succeeded)
{
	if (queryID == m_stanceQuery)
	{
		if (succeeded)
			m_currentStance = m_requestedStance;
		else
			CryLog("CAnimatedCharacter::QueryComplete: failed setting stance %d on %s", m_requestedStance, GetEntity()->GetName());
		m_requestedStance = -1;
		m_stanceQuery = 0;
	}
}

void CAnimatedCharacter::AddMovement(const SCharacterMoveRequest& request)
{
	if (request.type != eCMT_None)
	{
		assert(request.rotation.IsValid());
		assert(request.velocity.IsValid());

		CheckNANVec(Vec3(request.velocity), GetEntity());

		// we should have processed a move request before adding a new one
		m_moveRequest = request;
		m_moveRequestFrameID = gEnv->nMainFrameID;

		assert(m_moveRequest.rotation.IsValid());
		assert(m_moveRequest.velocity.IsValid());
	}
	else
	{
		m_moveRequest.type = eCMT_None;
		m_moveRequest.velocity.zero();
		m_moveRequest.rotation.SetIdentity();
		m_moveRequest.prediction.Reset();
		m_moveRequest.jumping = false;
		m_moveRequest.allowStrafe = false;
		m_moveRequest.proceduralLeaning = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimatedCharacter::ValidateCharacterPtrs()
{
	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	ICharacterInstance* pShadowCharacter = m_hasShadowCharacter ? GetEntity()->GetCharacter(m_shadowCharacterSlot) : NULL;

	const bool characterChanged = (pCharacter != m_pCharacter) || (m_pShadowCharacter != pShadowCharacter);
	CRY_ASSERT_TRACE(!characterChanged, ("CharacterPtrs out of date in %s. Ensure that updateCharacterPtrs is called on the animatedCharacter whenever new models are loaded", GetEntity()->GetName()));
}

//////////////////////////////////////////////////////////////////////////
void CAnimatedCharacter::UpdateCharacterPtrs()
{
	// TODO: OPT: These three will not change often (if ever), so they can be cached over many frames.
	// TODO: Though, make sure that they are refreshed if/when they actually DO change.
	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	ICharacterInstance* pShadowCharacter = m_hasShadowCharacter ? GetEntity()->GetCharacter(m_shadowCharacterSlot) : NULL;

	const bool characterChanged = (pCharacter != m_pCharacter) || (m_pShadowCharacter != pShadowCharacter);

	m_pCharacter = pCharacter;
	m_pShadowCharacter = pShadowCharacter;

	if (characterChanged)
	{
		m_pSkeletonAnim = (m_pCharacter != NULL) ? m_pCharacter->GetISkeletonAnim() : NULL;
		m_pSkeletonPose = (m_pCharacter != NULL) ? m_pCharacter->GetISkeletonPose() : NULL;

		m_pShadowSkeletonAnim = (m_pShadowCharacter != NULL) ? m_pShadowCharacter->GetISkeletonAnim() : NULL;
		m_pShadowSkeletonPose = (m_pShadowCharacter != NULL) ? m_pShadowCharacter->GetISkeletonPose() : NULL;

		DisableEntitySystemCharacterUpdate();
		UpdateSkeletonSettings();
	}

	if (m_pActionController)
	{
		IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetEntityId());
		const bool isFirstPerson = pActor && (pActor->IsThirdPerson() == false);

		if (characterChanged || (isFirstPerson != m_lastACFirstPerson))
		{
			m_lastACFirstPerson = isFirstPerson;

			const int scopeContextSound = m_pActionController->GetContext().controllerDef.m_scopeContexts.Find("Audio");
			const int scopeContext1P = m_pActionController->GetContext().controllerDef.m_scopeContexts.Find("Char1P");
			const int scopeContext3P = m_pActionController->GetContext().controllerDef.m_scopeContexts.Find("Char3P");

			if (scopeContextSound >= 0 && m_pSoundDatabase)
			{
				m_pActionController->SetScopeContext(scopeContextSound, *GetEntity(), NULL, m_pSoundDatabase);
			}

			if (isFirstPerson)
			{
				if ((scopeContext1P >= 0) && m_pAnimDatabase1P)
				{
					if (m_pCharacter)
					{
						m_pActionController->SetScopeContext(scopeContext1P, *GetEntity(), m_pCharacter, m_pAnimDatabase1P);
					}
					else
					{
						m_pActionController->ClearScopeContext(scopeContext1P);
					}
				}
				if ((scopeContext3P >= 0) && m_pAnimDatabase3P)
				{
					if (m_pShadowCharacter)
					{
						m_pActionController->SetScopeContext(scopeContext3P, *GetEntity(), m_pShadowCharacter, m_pAnimDatabase3P);
					}
					else
					{
						m_pActionController->ClearScopeContext(scopeContext3P);
					}
				}
			}
			else
			{
				if (scopeContext1P >= 0)
				{
					m_pActionController->ClearScopeContext(scopeContext1P);
				}
				if ((scopeContext3P >= 0) && m_pAnimDatabase3P)
				{
					if (m_pCharacter)
					{
						m_pActionController->SetScopeContext(scopeContext3P, *GetEntity(), m_pCharacter, m_pAnimDatabase3P);
					}
					else
					{
						m_pActionController->ClearScopeContext(scopeContext3P);
					}
				}
			}

			for (TAnimCharListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
				notifier->OnCharacterChange();

		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimatedCharacter::ProcessEvent(const SEntityEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	switch (event.event)
	{
	case ENTITY_EVENT_PRE_SERIALIZE:
		{
			if (m_pActionController)
				m_pActionController->Reset();
		}
		break;
	case ENTITY_EVENT_ANIM_EVENT:
		{
			VALIDATE_CHARACTER_PTRS

			if (m_pActionController)
			{
				const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
				ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
				if (pAnimEvent && pCharacter)
				{
					m_pActionController->OnAnimationEvent(pCharacter, *pAnimEvent);
				}
			}
		}
		break;
	case ENTITY_EVENT_XFORM:
		{
			int flags = (int)event.nParam[0];

			if (!(flags & (ENTITY_XFORM_USER | ENTITY_XFORM_PHYSICS_STEP)))
			{
				IEntity* pEntity = GetEntity();
				if (pEntity != NULL)
				{
					QuatT entLocationTeleported = m_entLocation; // maybe don't use this local variable, but instead just m_entLocation, and maybe call UpdateCurEntLocation() here as well, just in case.

					// TODO: Optimize by not doing the merge of full QuatT's twice (once for each component).
					if (flags & ENTITY_XFORM_ROT /*&& flags & (ENTITY_XFORM_TRACKVIEW|ENTITY_XFORM_EDITOR)*/)
					{
						entLocationTeleported.q = pEntity->GetWorldRotation();
					}
					if (flags & ENTITY_XFORM_POS)
					{
						entLocationTeleported.t = pEntity->GetWorldPos();

						//if (pEntity->GetParent() == NULL && m_entLocation.t != entLocationTeleported.t)
						//{
						//	// forcing MCM's to eMCM_Entity only when teleported if not inside vehicle
						//	// (inside vehicles entity can only move by teleporting)
						//	EMovementControlMethod mcmh = GetMCMH();
						//	EMovementControlMethod mcmv = GetMCMV();
						//	bool forceEntity = false;
						//	if (mcmh == eMCM_Animation || mcmh == eMCM_AnimationHCollision)
						//	{
						//		forceEntity = true;
						//		mcmh = eMCM_Entity;
						//	}
						//	if (mcmv == eMCM_Animation)
						//	{
						//		forceEntity = true;
						//		mcmv = eMCM_Entity;
						//	}
						//	if (forceEntity)
						//		SetMovementControlMethods(mcmh, mcmv);
						//}
					}

					if (!m_inGrabbedState)
					{
						// This is only for debugging, not used for anything.
						m_entTeleportMovement = ApplyWorldOffset(m_entTeleportMovement, GetWorldOffset(m_entLocation, entLocationTeleported));

						m_expectedEntMovement.zero();
						m_entLocation = entLocationTeleported;
					}
					else
					{
						// [Benito] Note:
						// When the player is grabbing an AI, the update of the character instance has to be delayed to this point
						// The player instance will update, and the AI, being attached to the player's hand will get updated its final position
						// at this point.
						// Because this happens during SynchAllAnimations at the end of the frame, SetAnimationPostProcessParameters has true as 3rd parameter
						// which means, it will immediatelly call FinishAnimations in the character (it's not ideal, but there is no other way to get the correct update order)

						VALIDATE_CHARACTER_PTRS

						  m_entLocation = entLocationTeleported;
						if (StartAnimationProcessing(m_entLocation))
							SetAnimationPostProcessParameters(m_entLocation, true);
					}

					DestroyExtraSolidCollider();

					m_pComponentPrepareCharForUpdate->ClearQueuedRotation();
				}
			}
		}
		break;
	case ENTITY_EVENT_SCRIPT_REQUEST_COLLIDERMODE:
		{
			VALIDATE_CHARACTER_PTRS

			EColliderMode mode = (EColliderMode)event.nParam[0];
			RequestPhysicalColliderMode(mode, eColliderModeLayer_Script);
		}
		break;
	case ENTITY_EVENT_DONE:
		{
			// Delete the ActionController before everything is deleted.
			DeleteActionController();
		}
		break;
	case ENTITY_EVENT_INIT:
	case ENTITY_EVENT_RESET:
		{
			if (!m_pActionController)
			{
				InitializeMannequin();
			}
		}
		break;
	}
}

uint64 CAnimatedCharacter::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_PRE_SERIALIZE) | ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT) | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) | ENTITY_EVENT_BIT(ENTITY_EVENT_SCRIPT_REQUEST_COLLIDERMODE) | ENTITY_EVENT_BIT(ENTITY_EVENT_DONE) | ENTITY_EVENT_BIT(ENTITY_EVENT_INIT) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

float CAnimatedCharacter::FilterView(SViewParams& viewParams) const
{

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	float animationControlledView = 0.0f;

	if (animationControlledView > 0.001f)
	{
		viewParams.viewID = 1;
		viewParams.nearplane = 0.1f;

		ISkeletonPose* pISkeletonPose = pCharacter->GetISkeletonPose();
		IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();

		//FIXME:keep IDs and such

		//view position, get the character position from the eyes and blend it with the game desired position
		int id_right = rIDefaultSkeleton.GetJointIDByName("eye_right_bone");
		int id_left = rIDefaultSkeleton.GetJointIDByName("eye_left_bone");
		if (id_right > -1 && id_left > -1)
		{
			Vec3 characterViewPos(pISkeletonPose->GetAbsJointByID(id_right).t);
			characterViewPos += pISkeletonPose->GetAbsJointByID(id_left).t;
			characterViewPos *= 0.5f;

			characterViewPos = GetEntity()->GetSlotWorldTM(0) * characterViewPos;

			viewParams.position = animationControlledView * characterViewPos + (1.0f - animationControlledView) * viewParams.position;
		}

		//and then, same with view rotation
		int id_head = rIDefaultSkeleton.GetJointIDByName("Bip01 Head");
		if (id_head > -1)
		{
			//	Quat characterViewQuat(Quat(GetEntity()->GetSlotWorldTM(0)) * Quat(pSkeleton->GetAbsJMatrixByID(id_head)) * Quat::CreateRotationY(gf_PI*0.5f));
			Quat characterViewQuat(Quat(GetEntity()->GetSlotWorldTM(0)) * pISkeletonPose->GetAbsJointByID(id_head).q * Quat::CreateRotationY(gf_PI * 0.5f));

			viewParams.rotation = Quat::CreateSlerp(viewParams.rotation, characterViewQuat, animationControlledView);
		}
	}

	return animationControlledView;
}

void CAnimatedCharacter::SetParams(const SAnimatedCharacterParams& params)
{
	m_params = params;
}

void CAnimatedCharacter::RequestStance(int stanceID, const char* name)
{
	m_stanceQuery = 0;
	m_requestedStance = -1;
	m_currentStance = stanceID;
}

int CAnimatedCharacter::GetCurrentStance()
{
	return m_currentStance;
}

bool CAnimatedCharacter::InStanceTransition()
{
	return m_requestedStance >= 0;
}

void CAnimatedCharacter::DestroyedState(IAnimationGraphState*)
{
	m_pMannequinAGState = NULL;
}

uint32 CAnimatedCharacter::MakeFace(const char* pExpressionName, bool usePreviewChannel, float lifeTime)
{
	uint32 channelId(~0);
	return channelId;
}

void CAnimatedCharacter::AllowLookIk(bool allow, int layer /* = -1 */)
{
	unsigned int oneBasedLayerIndex = ((layer >= 0) && (layer < eAnimationGraphLayer_COUNT)) ? layer + 1 : 0;

	if (allow)
	{
		m_disallowLookIKFlags &= ~(1 << oneBasedLayerIndex);
	}
	else
	{
		m_disallowLookIKFlags |= (1 << oneBasedLayerIndex);
	}
}

void CAnimatedCharacter::AllowAimIk(bool allow)
{
	m_allowAimIk = allow;
}

void CAnimatedCharacter::TriggerRecoil(float duration, float kinematicImpact, float kickIn /*=0.8f*/, EAnimatedCharacterArms arms /*=eACA_BothArms*/)
{
	if (IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetEntityId()))
	{
		//--- Ensure that our cached ptrs are up to date as this can be called from outside the update loop
		VALIDATE_CHARACTER_PTRS

		ISkeletonPose* pSkeletonPose = pActor->IsThirdPerson() ? m_pSkeletonPose : m_pShadowSkeletonPose; // in 1p apply recoil on shadow instead of the main skeletonpose

		if (pSkeletonPose != NULL)
			pSkeletonPose->ApplyRecoilAnimation(duration, kinematicImpact, kickIn, static_cast<uint32>(arms));
	}
}

void CAnimatedCharacter::SetWeaponRaisedPose(EWeaponRaisedPose pose)
{
	if (pose == m_curWeaponRaisedPose)
		return;

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	if (pCharacter == NULL)
		return;

	ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
	if (pSkeletonAnim == NULL)
		return;

	m_curWeaponRaisedPose = pose;

	if ((pose == eWeaponRaisedPose_None) || (pose == eWeaponRaisedPose_Fists))
	{
		pSkeletonAnim->StopAnimationInLayer(2, 0.5f);  // Stop weapon raising animation in Layer 2.
		return;
	}

	const char* anim = NULL;
	switch (pose)
	{
	case eWeaponRaisedPose_Pistol:
		anim = "combat_idleAimBlockPoses_pistol_01";
		break;
	case eWeaponRaisedPose_PistolLft:
		anim = "combat_idleAimBlockPoses_dualpistol_left_01";
		break;
	case eWeaponRaisedPose_PistolRgt:
		anim = "combat_idleAimBlockPoses_dualpistol_right_01";
		break;
	case eWeaponRaisedPose_PistolBoth:
		anim = "combat_idleAimBlockPoses_dualpistol_01";
		break;
	case eWeaponRaisedPose_Rifle:
		anim = "combat_idleAimBlockPoses_rifle_01";
		break;
	case eWeaponRaisedPose_Rocket:
		anim = "combat_idleAimBlockPoses_rocket_01";
		break;
	case eWeaponRaisedPose_MG:
		anim = "combat_idleAimBlockPoses_mg_01";
		break;
	}

	if (anim == NULL)
	{
		m_curWeaponRaisedPose = eWeaponRaisedPose_None;
		return;
	}

	// Start the weapon raising in Layer 2. This will automatically deactivate aim-poses.
	CryCharAnimationParams Params0(0);
	Params0.m_nLayerID = 2;
	Params0.m_fTransTime = 0.5f;
	Params0.m_nFlags |= CA_LOOP_ANIMATION;
	pSkeletonAnim->StartAnimation(anim, Params0);
}

SGroundAlignmentParams& CAnimatedCharacter::GetGroundAlignmentParams()
{
	return m_groundAlignmentParams;
}

void CAnimatedCharacter::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	{
		SIZER_COMPONENT_NAME(s, "MannequinAGState");
		s->AddObject(m_pMannequinAGState);
	}

	{
		SIZER_COMPONENT_NAME(s, "DebugHistory");
		s->AddObject(m_debugHistoryManager);
	}
}

void CAnimatedCharacter::DeleteActionController()
{
	SAFE_RELEASE(m_pActionController);
	SAFE_DELETE(m_pAnimContext);
}

void CAnimatedCharacter::SetActionController(const char* filename)
{
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	const SControllerDef* contDef = mannequinSys.GetAnimationDatabaseManager().LoadControllerDef(filename);

	DeleteActionController();

	if (contDef)
	{
		m_pAnimContext = new SAnimationContext(*contDef);
		m_pActionController = mannequinSys.CreateActionController(GetEntity(), *m_pAnimContext);
	}
}

void CAnimatedCharacter::SetShadowCharacterSlot(int id)
{
	m_shadowCharacterSlot = id;
	m_hasShadowCharacter = true;

	InitShadowCharacter();
}

void CAnimatedCharacter::InitShadowCharacter()
{
	//--- Ensure that the shadow skel is set to animDriven motion, don't wait until the occassional ticked update
	IEntity* pEntity = GetEntity();
	ICharacterInstance* pCharacterInstanceShadow = m_hasShadowCharacter ? pEntity->GetCharacter(m_shadowCharacterSlot) : NULL;
	ISkeletonAnim* pShadowSkeletonAnim = pCharacterInstanceShadow ? pCharacterInstanceShadow->GetISkeletonAnim() : NULL;
	if (pShadowSkeletonAnim)
	{
		pShadowSkeletonAnim->SetAnimationDrivenMotion(1);
	}
}

void CAnimatedCharacter::SetAnimationPlayerProxy(CAnimationPlayerProxy* proxy, int layer)
{
	m_pAnimationPlayerProxies[layer] = proxy;
}

CAnimationPlayerProxy* CAnimatedCharacter::GetAnimationPlayerProxy(int layer)
{
	CRY_ASSERT(layer < eAnimationGraphLayer_COUNT);
	return m_pAnimationPlayerProxies[layer];
}

void CAnimatedCharacter::DisableEntitySystemCharacterUpdate()
{
	if (m_pCharacter)
	{
		// Turn off auto updating on this character,
		// AnimatedCharacter will be forcing Pre and Post update manually.
		m_pCharacter->SetFlags(m_pCharacter->GetFlags() & (~CS_FLAG_UPDATE));

		if (m_pShadowCharacter)
		{
			m_pShadowCharacter->SetFlags(m_pShadowCharacter->GetFlags() & (~CS_FLAG_UPDATE));
		}
	}
}

void CAnimatedCharacter::EnableLandBob(const SLandBobParams& landBobParams)
{
	CRY_ASSERT(landBobParams.IsValid());

	m_landBobParams = landBobParams;
}

void CAnimatedCharacter::DisableLandBob()
{
	m_landBobParams.Invalidate();
}

void CAnimatedCharacter::UseAnimationMovementForEntity(bool xyMove, bool zMove, bool rotation)
{
	m_moveOverride_useAnimXY = xyMove;
	m_moveOverride_useAnimZ = zMove;
	m_moveOverride_useAnimRot = rotation;
}

void CAnimatedCharacter::SetInGrabbedState(bool bEnable)
{
	m_noMovementOverrideExternal = bEnable;
	m_inGrabbedState = bEnable;
}

void CAnimatedCharacter::SetBlendFromRagdollizeParams(const SGameObjectEvent& event)
{
	CRY_ASSERT(event.event == eGFE_QueueBlendFromRagdoll);

	m_blendFromRagollizeParams.m_bPendingBlend = event.paramAsBool;
}

void CAnimatedCharacter::SetRagdollizeParams(const SGameObjectEvent& event)
{
	CRY_ASSERT(event.event == eGFE_QueueRagdollCreation);

	m_ragdollParams = *static_cast<const SRagdollizeParams*>(event.ptr);

	m_bPendingRagdoll = true;
}

void CAnimatedCharacter::KickOffRagdoll()
{
	if (m_bPendingRagdoll)
	{
		m_groundAlignmentParams.SetFlag(eGA_Enable, false);

		SEntityPhysicalizeParams pp;

		pp.type = PE_ARTICULATED;
		pp.nSlot = 0;
		pp.bCopyJointVelocities = true;

		pp.mass = m_ragdollParams.mass;

		//never ragdollize without mass [Anton]
		pp.mass = (float)__fsel(-pp.mass, 80.0f, pp.mass);

		pp.fStiffnessScale = m_ragdollParams.stiffness;

		pe_player_dimensions playerDim;
		pe_player_dynamics playerDyn;

		playerDyn.gravity.z = 15.0f;
		playerDyn.kInertia = 5.5f;

		pp.pPlayerDimensions = &playerDim;
		pp.pPlayerDynamics = &playerDyn;

		IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
		if (!pPhysicalEntity || pPhysicalEntity->GetType() != PE_LIVING)
			pp.nLod = 1;

		// Joints velocities are copied by default for now
		pp.bCopyJointVelocities = !gEnv->pSystem->IsSerializingFile();
		pp.nFlagsOR = pef_log_poststep;
		GetEntity()->Physicalize(pp);

		SGameObjectEvent triggeredRagdoll(eGFE_RagdollPhysicalized, eGOEF_ToExtensions);
		triggeredRagdoll.ptr = &m_ragdollParams;
		GetGameObject()->SendEvent(triggeredRagdoll);
	}
	m_bPendingRagdoll = false;

	if (m_blendFromRagollizeParams.m_bPendingBlend)
	{
		m_groundAlignmentParams.SetFlag(eGA_Enable, true);

		SGameObjectEvent event(eGFE_DisableBlendRagdoll, eGOEF_ToExtensions);
		GetGameObject()->SendEvent(event);

		/*		ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0);
		    if (pCharacter && pCharacter->GetISkeletonAnim())
		    {
		      IPhysicalEntity *pPhysicalEntity=0;
		      Matrix34 delta(IDENTITY);

		      pCharacter->GetISkeletonPose()->StandUp( Matrix34(m_entLocation), false, pPhysicalEntity, delta);

		      if (pPhysicalEntity)
		      {
		        {
		          GetEntity()->SetWorldTM(delta);
		          m_entLocation = QuatT(delta);
		          GetEntity()->AssignPhysicalEntity(pPhysicalEntity);

		          GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
		        }
		      }
		    }*/
	}
	m_blendFromRagollizeParams.m_bPendingBlend = false;
}

bool CAnimatedCharacter::StartAnimationProcessing(const QuatT& entityLocation) const
{
	const int currentFrameId = gEnv->nMainFrameID;

	if ((m_pCharacter != NULL) && (m_lastAnimationUpdateFrameId != currentFrameId))
	{
		// calculate the approximate distance from camera
		const CCamera& camera = GetISystem()->GetViewCamera();
		const float fDistance = (camera.GetPosition() - entityLocation.t).GetLength();
		const float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

		SAnimationProcessParams params;
		params.locationAnimation = entityLocation;
		params.bOnRender = 0;
		params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;
		params.locationAnimation = QuatTS(entityLocation.q, entityLocation.t, m_pCharacter->GetUniformScale());

		if (m_pShadowCharacter)
			m_pShadowCharacter->StartAnimationProcessing(params);

		m_pCharacter->StartAnimationProcessing(params);

		m_lastAnimationUpdateFrameId = currentFrameId;

		return true;
	}

	return false;
}

void CAnimatedCharacter::SetAnimationPostProcessParameters(const QuatT& entityLocation, const bool finishImmediate) const
{
	if (m_pCharacter != NULL)
	{
		const CCamera& viewCamera = GetISystem()->GetViewCamera();
		const float scale = GetEntity()->GetWorldTM().GetColumn(0).GetLength();
		const float fDistance = (viewCamera.GetPosition() - entityLocation.t).GetLength();
		const float fZoomFactor = 0.001f + 0.999f * (RAD2DEG((viewCamera.GetFov())) / 60.f);

		m_pCharacter->SetAttachmentLocation_DEPRECATED(QuatTS(entityLocation.q, entityLocation.t, scale));
		if (m_pShadowCharacter)
		{
			m_pShadowCharacter->SetAttachmentLocation_DEPRECATED(QuatTS(entityLocation.q, entityLocation.t, scale));
		}

		if (finishImmediate)
		{
			m_pCharacter->FinishAnimationComputations();

			//This path won't trigger, but for consistency
			if (m_pShadowCharacter)
			{
				m_pShadowCharacter->FinishAnimationComputations();
			}
		}
	}
}

void CAnimatedCharacter::StartAnimationProcessing() const
{
	m_lastAnimProcFrameID = gEnv->nMainFrameID;

	if (UseNormalAnimationProcessing())
	{
		StartAnimationProcessing(m_entLocation);
	}
}

void CAnimatedCharacter::SetAnimationPostProcessParameters() const
{
	if (UseNormalAnimationProcessing())
	{
		SetAnimationPostProcessParameters(m_entLocation);
	}
}

void CAnimatedCharacter::PrepareAndStartAnimProc()
{
	if (m_curFrameTime <= 0.0f)
		return;

	UpdateCharacterPtrs();

	KickOffRagdoll();

	UpdateGroundAlignment();

	StartAnimationProcessing();

	UpdatePhysicalColliderMode();

	UpdatePhysicsInertia();
}

void CAnimatedCharacter::ForceMovement(const QuatT& relativeMovement)
{
	if (!m_hasForcedMovement)
		m_forcedMovementRelative = relativeMovement;
	else
		m_forcedMovementRelative = ApplyWorldOffset(m_forcedMovementRelative, relativeMovement);
	m_hasForcedMovement = true;
}

void CAnimatedCharacter::ForceOverrideRotation(const Quat& qWorldRotation)
{
	m_forcedOverrideRotationWorld = qWorldRotation;
	m_hasForcedOverrideRotation = true;
}

void CAnimatedCharacter::UpdateGroundAlignment()
{
	//use ground alignment only when character is close to the camera
	if (m_pSkeletonPose)
	{
		bool bNeedUpdate = true;

		if (m_simplifyMovement)
		{
			bNeedUpdate = false;
		}
		else if (!m_groundAlignmentParams.IsFlag(eGA_Enable))
		{
			bNeedUpdate = false;
		}
		else
		{
			//check if player is close enough
			const CCamera& camera = gEnv->pSystem->GetViewCamera();
			const float fDistanceSq = (camera.GetPosition() - m_entLocation.t).GetLengthSquared();

			// check if the character is using an animAction
			// because these should allow groundAlignment even in animation driven mode
			const int currentStance = GetCurrentStance();

			if (fDistanceSq > m_groundAlignmentParams.ikDisableDistanceSqr)
			{
				bNeedUpdate = false;
			}
			else if (GetEntity()->GetParent() != NULL)
			{
				bNeedUpdate = false;
			}
			else if ((currentStance == STANCE_SWIM) ||
			         (currentStance == STANCE_ZEROG) ||
			         (currentStance == STANCE_PRONE))
			{
				bNeedUpdate = false;
			}
			else if (NoMovementOverride() && !m_groundAlignmentParams.IsFlag(eGA_AllowWithNoCollision))
			{
				bNeedUpdate = false;
			}
			else if ((m_colliderMode == eColliderMode_Disabled) && !m_groundAlignmentParams.IsFlag(eGA_AllowWithNoCollision))
			{
				bNeedUpdate = false;
			}
			else if (GetMCMV() == eMCM_Animation)
			{
				bNeedUpdate = false;
			}
			else if (!m_groundAlignmentParams.IsFlag(eGA_AllowWhenHasGroundCollider))
			{
				if (IPhysicalEntity* piPhysics = GetEntity()->GetPhysics())
				{
					// Can't get access to whether the actor has a ground collider here,
					// so querying physics directly. This is really a bug :(
					pe_status_living peLiv;
					piPhysics->GetStatus(&peLiv);
					if (peLiv.pGroundCollider)
					{
						bNeedUpdate = false;
					}
				}
			}
		}

		if (bNeedUpdate)
		{
			PostProcessingUpdate();
		}
	}
}

void CAnimatedCharacter::PerformSimpleMovement()
{
	return;
	//this function doesn't make any sense here. We ALWAYS update entities in CryAnimation
	CryFatalError("no need to update entities here");
	/*	if (m_simplifyMovement && m_pCharacter)
	   {
	    //if a character is not visible on screen, we still have to update the world position of the attached entity
	    QuatT EntLocation;
	    EntLocation.q=GetEntity()->GetWorldRotation();
	    EntLocation.t=GetEntity()->GetWorldPos();
	    m_pCharacter->UpdateAttachedObjectsFast(EntLocation,0.0f);
	   }*/
}

void CAnimatedCharacter::GenerateMovementRequest()
{
	// 21/09/2012: Workaround for cases in which ResetVars() is called after PrepareAnimatedCharacterForUpdate() but before GenerateMovementRequest()
	// This could turn into a proper 'cache' of useful variables like curFrameTime, velocity, ... which gets properly invalidated
	// whenever ResetVars() is called.
	IF_UNLIKELY (m_curFrameTime == 0.0f)
	{
		UpdateTime();
	}

	UpdateCharacterPtrs();

	RefreshAnimTarget();

	UpdateSimpleMovementConditions();

	PreAnimationUpdate();

	// Update actioncontroller after the graph if we push fragments
	// (fragments started by the graph should start as soon as possible)
	if (m_pActionController)
	{
		m_pActionController->Update((float) m_curFrameTime);
	}

	CalculateParamsForCurrentMotions();

	UpdateMCMs();

	CalculateAndRequestPhysicalEntityMovement();
}

void CAnimatedCharacter::PrepareAnimatedCharacterForUpdate()
{
#ifdef DEBUGHISTORY
	SetupDebugHistories();
#endif

#ifdef _DEBUG
	RunTests();
#endif

	UpdateTime();
	GetCurrentEntityLocation();
	RefreshAnimTarget();
}

namespace animatedcharacter
{

void Preload(struct IScriptTable* pEntityScript)
{
	// Cache Mannequin related files
	bool hasActionController = false;
	{
		IMannequin& mannequinSystem = gEnv->pGameFramework->GetMannequinInterface();
		IAnimationDatabaseManager& animationDatabaseManager = mannequinSystem.GetAnimationDatabaseManager();

		const char* szAnimationDatabase1p = 0;
		if (pEntityScript->GetValue("AnimDatabase1P", szAnimationDatabase1p) &&
		    szAnimationDatabase1p && szAnimationDatabase1p[0])
		{
			animationDatabaseManager.Load(szAnimationDatabase1p);
		}

		const char* szAnimationDatabase3p = 0;
		if (pEntityScript->GetValue("AnimDatabase3P", szAnimationDatabase3p) &&
		    szAnimationDatabase3p && szAnimationDatabase3p[0])
		{
			animationDatabaseManager.Load(szAnimationDatabase3p);
		}

		const char* szSoundDatabase = 0;
		if (pEntityScript->GetValue("SoundDatabase", szSoundDatabase) &&
		    szSoundDatabase && szSoundDatabase[0])
		{
			animationDatabaseManager.Load(szSoundDatabase);
		}

		const char* szControllerDef = 0;
		if (pEntityScript->GetValue("ActionController", szControllerDef) &&
		    szControllerDef && szControllerDef[0])
		{
			const SControllerDef* pControllerDef = animationDatabaseManager.LoadControllerDef(szControllerDef);
			hasActionController = (pControllerDef != NULL);
		}
	}
}

} // namespace animatedcharacter
