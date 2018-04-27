// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RECORDINGSYSTEMDEFINES_H__
#define __RECORDINGSYSTEMDEFINES_H__

#include "ICryMannequinDefs.h"

#define RECSYS_INCREASED_ACCURACY 1

#if RECSYS_INCREASED_ACCURACY
#define FP_RECORDING_BUFFER_SIZE		(40 * 1024)
#else //!RECSYS_INCREASED_ACCURACY
#define FP_RECORDING_BUFFER_SIZE		(10 * 1024)
#endif
#define FP_RECEIVE_BUFFER_SIZE			(FP_RECORDING_BUFFER_SIZE*2)


#define RECORDING_BUFFER_SIZE		(260*1024)

#define RECORDING_BUFFER_QUEUE_SIZE		(5*1024)
#define ALL_PARTS 0xFFFFFFFF
#define SOUND_NAMES_SIZE (50*1024)
#define MAX_SOUND_PARAMETERS 8
#define RECORDING_SYSTEM_MAX_PARTS 32
#define NUM_STEALTH_KILL_ANIMS 3
#define MAX_HIGHLIGHTS 4
#define MODEL_NAMES_SIZE (2*1024)
#define MAX_RECORDED_PLAYERS (MAX_PLAYER_LIMIT)
#define MAX_NUM_WEAPONS_AUDIO_CACHED 5

#if !defined(_RELEASE) && !CRY_PLATFORM_ORBIS 
#define RECSYS_DEBUG
#endif //_RELEASE

enum ERecSysDebug
{
	eRSD_None=0,
	eRSD_General,
	eRSD_Visibility,
	eRSD_Particles,
	eRSD_Sound,
	eRSD_Config,
	eRSD_Total
};

#define RecSysQuatTFormat "(%.2f, %.2f, %.2f) (%.2f, %.2f, %.2f; %.2f)"
#define RecSysQuatTValues(quat) (quat).t.x, (quat).t.y, (quat).t.z, (quat).q.v.x, (quat).q.v.y, (quat).q.v.z, (quat).q.w

#ifdef RECSYS_DEBUG
	#define RecSysDebugStr_eRSD_None ""
	#define RecSysDebugStr_eRSD_General "[DEBUG] "
	#define RecSysDebugStr_eRSD_Visibility "[VISI] "
	#define RecSysDebugStr_eRSD_Particles "[PART] "
	#define RecSysDebugStr_eRSD_Sound "[SOUND] "
	#define RecSysDebugStr_eRSD_Config "[CFG] "

    #define RecSysLog(...)								CryLog("[RECSYS] " __VA_ARGS__)
    #define RecSysLogAlways(...)					CryLogAlways("[RECSYS] " __VA_ARGS__)
	#define RecSysLogDebug(x,...)					{if(g_pGameCVars->kc_debug==(x)){CryLogAlways("[RECSYS] " RecSysDebugStr_##x __VA_ARGS__);}}
	#define RecSysLogDebugIf(x,y,...)			{if(g_pGameCVars->kc_debug==(x) &&(y)){CryLogAlways("[RECSYS] " RecSysDebugStr_##x __VA_ARGS__);}}
	#define RecSysLogFunc									do { RecSysLog("%s():%d", __FUNC__, __LINE__); } while (0);
	#define RecSysWatch(...)							CryWatch(__VA_ARGS__);
	#define RecSysWatchDebug(x,...)				{if(g_pGameCVars->kc_debug==(x)){CryWatch(__VA_ARGS__);}}
#else //RECSYS_DEBUG
	#define RecSysLog(...)								((void)0)
	#define RecSysLogAlways(...)					((void)0)
	#define RecSysLogDebug(x,...)					((void)0)
	#define RecSysLogDebugIf(x,y,...)			((void)0)
	#define RecSysLogFunc									((void)0)
	#define RecSysWatch(...)							((void)0)
	#define RecSysWatchDebug(x,...)				((void)0)
#endif //RECSYS_DEBUG

enum ERecordingState
{
	eRS_NotRecording=0,
	eRS_Recording,
};

enum EPlaybackState
{
	ePS_NotPlaying=0,
	ePS_Queued,
	ePS_Playing,
};

enum EFirstPersonFlags
{
	eFPF_OnGround = BIT(0),
	eFPF_Sprinting = BIT(1),
	eFPF_StartZoom = BIT(2),
	eFPF_ThirdPerson = BIT(3),
	eFPF_FiredShot = BIT(4),
	eFPF_NightVision = BIT(5),
};

enum EThirdPersonFlags
{
	eTPF_Dead = BIT(0),
	eTPF_Cloaked = BIT(1),
	eTPF_AimIk = BIT(2),
	eTPF_Ragdoll = BIT(3),
	eTPF_OnGround = BIT(4),
	eTPF_Invisible = BIT(5),
	eTPF_StealthKilling = BIT(6),
};

enum EPlaybackSetupStage
{
	ePSS_None,
	ePSS_AudioAndTracers,
	ePSS_HideExistingEntities,
	ePSS_SpawnNewEntities,
	ePSS_HandleParticles,
	ePSS_SpawnRecordingSystemActors,
	ePSS_StartExistingSounds,
	ePSS_ApplyBreaks,
	ePSS_ReInitHUD,
	ePSS_DONE,
	ePSS_FIRST=ePSS_AudioAndTracers,
};

enum EBulletTimePhase
{
	eBTP_Approach,
	eBTP_Hover,
	eBTP_PostHover,
	eBTP_PostHit,
};

enum EProjectileType
{
	ePT_Grenade,
	ePT_Kvolt,
	ePT_Rocket,
	ePT_C4,
};

enum EHighlightRuleMode
{
	eHRM_Stealthed,
	eHRM_Armour,
	eHRM_MaxSuit,
	eHRM_NanoVision,
	eHRM_Teabag,
	eHRM_SkillKill,
};

struct SFirstPersonDataContainer
{
	SFirstPersonDataContainer()
	{
		Reset();
	}

	void Reset()
	{
		m_firstPersonDataSize = 0;
		m_isDecompressed=false;
	}

	uint32 m_firstPersonDataSize;
	uint8 m_firstPersonData[FP_RECEIVE_BUFFER_SIZE];
	bool m_isDecompressed;
};

struct SKillCamExpectedData
{
	SKillCamExpectedData()
		: m_sender(0)
		, m_victim(0)
		, m_winningKill(false)
	{}

	SKillCamExpectedData(EntityId sender, EntityId victim, bool winningKill)
		: m_sender(sender)
		, m_victim(victim)
		, m_winningKill(winningKill)
	{}

	ILINE void Reset()
	{ 
		m_sender = 0;
		m_victim = 0;
		m_winningKill = false;
	}

	ILINE const bool operator == (const SKillCamExpectedData& o) const
	{
		return ((m_sender == o.m_sender) && (m_victim == o.m_victim) && (m_winningKill == o.m_winningKill));
	}

	EntityId m_sender;
	EntityId m_victim;
	bool m_winningKill;
};

struct SKillCamStreamData
{
	SKillCamStreamData(){Reset();}
	void Reset()
	{
		m_finalSize = 0;
		m_packetReceived[0]		=	m_packetReceived[1]		=	 0;
		m_packetTargetNum[0]	=	m_packetTargetNum[1]	=	-1;
		m_expect.Reset();
		m_validExpectationsTimer = 0.f;
	}
	bool IsUnused() const { static SKillCamExpectedData blank; return m_expect==blank; }
	ILINE void Validate() { m_validExpectationsTimer = 0.f; }
	ILINE void SetValidationTimer( const float time ) { m_validExpectationsTimer = time; }
	ILINE bool IsValidationTimerSet() const { return m_validExpectationsTimer>0.f; }
	ILINE bool UpdateValidationTimer( const float dt ) { if( m_validExpectationsTimer>0.f ) { m_validExpectationsTimer=(float)__fsel(m_validExpectationsTimer-dt,m_validExpectationsTimer-dt,0.f); return m_validExpectationsTimer>0.f; } return true; }
	ILINE size_t GetFinalSize() const { return m_finalSize; }

	uint8 m_buffer[FP_RECORDING_BUFFER_SIZE];
	size_t m_finalSize;
	int m_packetReceived[2];
	int m_packetTargetNum[2];
	SKillCamExpectedData m_expect;
	float m_validExpectationsTimer;
};

class CKillCamDataStreamer
{
public:
	CKillCamDataStreamer(){}
	~CKillCamDataStreamer(){}

	void Update(const float dt);
	SKillCamStreamData* GetExpectedStreamData(const EntityId victim);
	SKillCamStreamData* ExpectStreamData(const SKillCamExpectedData& expect, const bool canUseExistingMatchedData);
	void ClearStreamData(const EntityId victim);
	void ClearAllStreamData();

#ifndef _RELEASE
	void DebugStreamData() const;
#endif //_RELEASE

private:
	static const int kMaxSimultaneousStreams = 2;
	SKillCamStreamData m_streams[kMaxSimultaneousStreams];
};

struct SPlaybackInstanceData
{
	void Reset()
	{
		m_remoteStartTime = 0;
		m_victimPositionsOffset = 0;
		m_firstPersonActionsOffset = 0;
	}

	size_t m_victimPositionsOffset;
	size_t m_firstPersonActionsOffset;
	float m_remoteStartTime;
};

struct SKillInfo
{
	SKillInfo()
		: killerId(0)
		, victimId(0)
		, projectileId(0)
		, pProjectileClass(NULL)
		, localPlayerTeam(0)
		, hitType(0)
		, impulse(ZERO)
		, deathTime(0.f)
		, bulletTimeKill(false)
		, winningKill(false)
	{}

	EntityId killerId;
	EntityId victimId;
	EntityId projectileId;
	IEntityClass* pProjectileClass;
	int localPlayerTeam;
	int hitType;
	Vec3 impulse;
	float deathTime;
	bool bulletTimeKill;
	bool winningKill;
};

struct SPlaybackRequest
{
	SPlaybackRequest()
		: playbackDelay(0.f)
		, highlights(false)
	{}

	SKillInfo kill;
	float playbackDelay;
	bool highlights;
};

struct SHighlightRules 
{
	struct SFactor
	{
		SFactor() : add(0.f), mul(1.f) {}
		SFactor( const float _add, const float _mul ) : add(_add), mul(_mul) {}
		ILINE void Apply( float& in ) const { in = (in*mul)+add; }
		float add;
		float mul;
	};

	template<typename T>
	struct SFactorMap
	{
		SFactor m_default;
		std::map<T, SFactor> m_map;
		void SetDefault( const float _add, const float _mul ) { m_default.add=_add; m_default.mul=_mul; }
		void Add( const T& value, const float add, const float mul ) { m_map[value] = SFactor(add, mul); }
		void Apply( const T& search, float& val ) const
		{
			typename std::map<T, SFactor>::const_iterator found = m_map.find(search);
			if(found!=m_map.end())
			{
				found->second.Apply(val);
			}
			else
			{
				m_default.Apply(val);
			}
		}
	};

	void Init(XmlNodeRef& node);

	SFactorMap<int> m_hitTypes;
	SFactorMap<int> m_multiKills;
	SFactorMap<IEntityClass*> m_weapons;
	SFactorMap<EHighlightRuleMode> m_specialCases;
};

struct SPlaybackInfo
{
private:
	enum
	{
		eFPWeaponLOD = 0,
	};

	enum EPrecacheFlags
	{
		ePF_StartPos				=BIT(0),
		ePF_StartPosDone		=BIT(1),
		ePF_Weapons					=BIT(2),
		ePF_WeaponsDone			=BIT(3),
		ePF_FPWeaponCached	=BIT(4),
		ePF_TPWeaponCached	=BIT(5),
	};
	typedef uint8 TPrecacheFlags;

public:
	enum EViewMode
	{
		eVM_FirstPerson,
		eVM_ThirdPerson,
		eVM_ProjectileFollow,
		eVM_BulletTime,
		eVM_Static,
	};

	struct View
	{
		View() : precacheFlags(0)
		{
			Reset();
		}
		~View() {}
		void Reset();
		void Precache( const float timeBeforePlayback, const bool bForce = false );
		void ClearPrecachedWeapons();
		CryFixedStringT<128> streamingWeaponFPModel;
		CryFixedStringT<128> streamingWeaponTPModel;
		QuatT	lastSTAPCameraDelta;
		Vec3 precacheCamPos;
		Vec3 precacheCamDir;
		Vec3 camSmoothingRate;
		float fadeOut;
		float fadeOutRate;
		EViewMode currView;
		EViewMode prevView;
		bool bWorldSpace;
		TPrecacheFlags precacheFlags;
	};

	struct Timings
	{
		Timings()
			: recStart(0.0f)
			, recEnd(0.0f)
			, relDeathTime(0.0f)
		{
			Reset();
		}
		~Timings() {}
		ILINE void Reset() { recStart=recEnd=relDeathTime=0.f; }
		ILINE void ApplyOffset( const float offset ) { recStart+=offset; recEnd+=offset; }
		ILINE void Set( const float start, const float end, const float death )
		{
			recStart = start;
			recEnd = end;
			relDeathTime = death-start;
		}
		float recStart;				// Time when recording started.
		float recEnd;					// Time when recording ended.
		float relDeathTime;		// Time from recStart to Death.
	};

	SPlaybackInfo () { Reset(); }
	~SPlaybackInfo () { Reset(); }
	void Reset( const SPlaybackRequest* pRequest = NULL )
	{
		static SPlaybackRequest blank;
		m_request = pRequest ? *pRequest : blank;
		m_view.Reset();
		m_timings.Reset();
	}
	void Init( const SPlaybackRequest& request, const struct SRecordedData& initialData, CRecordingBuffer& tpData );

	SPlaybackRequest m_request;
	View m_view;
	Timings m_timings;
};

class IRecordingSystemListener
{
public:
	virtual ~IRecordingSystemListener() {}

	virtual void OnPlaybackRequested( const SPlaybackRequest& info ) {}
	virtual void OnPlaybackStarted( const SPlaybackInfo& info ) {}
	virtual void OnPlaybackEnd( const SPlaybackInfo& info ) {}
};

struct AimIKInfo
{
	bool aimIKEnabled;
	Vec3 aimIKTarget;

	AimIKInfo() : aimIKEnabled(false), aimIKTarget(0, 0, 1) {};
	AimIKInfo( bool enabled, Vec3 &target ) : aimIKEnabled(enabled), aimIKTarget(target) {};

	void GetMemoryUsage(ICrySizer *pSizer) const{}
};

struct SWeaponInfo
{
	EntityId weaponId;
	bool isMounted;

	SWeaponInfo() : weaponId(0), isMounted(false) {}
	SWeaponInfo(EntityId _weaponId, bool mounted) : weaponId(_weaponId), isMounted(mounted) {}

	void GetMemoryUsage(ICrySizer *pSizer) const{}
};

struct SSoundParamters
{
	SSoundParamters()
		: /*soundId(0)
		, */numParameters(0)
		, ignoredParameters(0)
	{
		memset(parameters, 0, sizeof(parameters));
	}
	//tSoundID soundId;
	float parameters[MAX_SOUND_PARAMETERS];
	int numParameters;
	uint32 ignoredParameters;
};

struct SRecordingEntity
{
	enum EEntityType
	{
		eET_Generic,
		eET_Item,
		eET_Actor,
	};
	enum EHiddenState
	{
		eHS_None,
		eHS_Hide,
		eHS_Unhide
	};
	SRecordingEntity()
		: location(IDENTITY)
		, entityId(0)
		, type(eET_Generic)
		, hiddenState(eHS_None)
	{}

	QuatT location;
	EntityId entityId;
	EEntityType type;
	EHiddenState hiddenState;
};

struct SRecordingAnimObject
{
	SRecordingAnimObject()
		: entityId(0)
		, fTime(0.f)
		, animId(-1)
	{
	}
	EntityId entityId;
	float fTime;
	int16 animId;
};

struct STrackedReplayGrenade
{
	STrackedReplayGrenade(EntityId projId, Vec3 initialPos, int8 team)
		: m_projId(projId)
		, m_team(team)
	{
	}

	EntityId m_projId;
	int8 m_team;
};

struct STrackedCorpse
{
	STrackedCorpse()
		: m_numJoints(0)
		, m_corpseId(0)
	{
	}

  enum {k_maxJoints=137};

	QuatT m_jointAbsPos[k_maxJoints];
	int32 m_numJoints;
	EntityId m_corpseId;
};

struct SInteractiveObjectAnimation
{
	SInteractiveObjectAnimation() : m_interactionTypeTag(TAG_ID_INVALID), m_interactionIndex(0) {}
	SInteractiveObjectAnimation(TagID interactionTypeTag, uint8 interactionIndex) : m_interactionTypeTag(interactionTypeTag), m_interactionIndex(interactionIndex) {}

	TagID m_interactionTypeTag;
	uint8 m_interactionIndex;
};


typedef std::map<EntityId, Vec3> ChrVelocityMap;
typedef std::map<EntityId,AimIKInfo> EntityAimIKMap;
typedef std::map<EntityId,int> EntityIntMap;
typedef std::map<EntityId,SWeaponInfo> EntityWeaponMap;
typedef std::map<IParticleEffect*, IParticleEffect*> TParticleEffectMap;
typedef std::map<EntityId, EntityId> TReplayEntityMap;
typedef std::map<IParticleEmitter*, IParticleEmitter*> TReplayParticleMap;
//typedef std::map<tSoundID, tSoundID> TReplaySoundMap;
typedef std::map<IStatObj*, IStatObj*> TStatObjMap;
typedef std::map<uint32, Vec3> TPendingRaycastMap;
typedef std::map<EntityId,SInteractiveObjectAnimation> TInteractiveObjectAnimationMap;
typedef std::vector<IRecordingSystemListener*> TRecordingSystemListeners;
typedef std::vector<uint32> TCorpseADIKJointsVec;
typedef std::map<EntityId, EntityId> TReplayActorMap;
typedef std::set<const IEntityClass*> TEntityClassSet;
typedef std::map<EntityId, SRecordingEntity> TRecEntitiesMap;

#endif //__RECORDINGSYSTEMDEFINES_H__
