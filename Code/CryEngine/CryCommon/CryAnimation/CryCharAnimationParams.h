// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/Tarray.h>
#include <CryNetwork/ISerialize.h> // <> required for Interfuscator
#include <CryMemory/CrySizer.h>
#include <CryCore/CryCrc32.h>

#define MAX_LMG_DIMENSIONS (4)

#if CRY_PLATFORM_WINDOWS && !defined(RELEASE)
	#define BLENDSPACE_VISUALIZATION (1)
	#define EDITOR_PCDEBUGCODE       (1)
#endif

// #define USE_PROTOTYPE_ABS_BLENDING (1)

//! The flags used in the nFlags member.
enum CA_AssetFlags : uint32
{
	//! This flags in used in RC as well
	//! If this is true, then this asset is an additive animation.
	CA_ASSET_ADDITIVE = 0x001,

	//! This flags in used in RC as well
	//! If this is true, then its possible to use this asset with the loop-flag.
	CA_ASSET_CYCLE        = 0x002,

	CA_ASSET_LOADED       = 0x004,      //!< If this is true, then the asset has valid info data (has been loaded at least once).
	CA_ASSET_LMG          = 0x008,      //!< If this is true, then the asset is a locomotion group (LMG).
	CA_ASSET_LMG_VALID    = 0x020,      //!< If this is true, then the LMG has been processed at loading time.
	CA_ASSET_CREATED      = 0x800,      //!< If this true, then the asset is created but not loaded.
	CA_ASSET_REQUESTED    = 0x1000,     //!< If this true, then the asset is already requested for loading.
	CA_ASSET_ONDEMAND     = 0x2000,     //!< If this true, then the asset has on-demand loading.
	CA_AIMPOSE            = 0x4000,     //!< If this true, then this asset is an AimPose.
	CA_AIMPOSE_UNLOADED   = 0x8000,     //!< If this true, then we had an AimPose that we removed from memory.

	CA_ASSET_NOT_FOUND    = 0x10000,
	CA_ASSET_TCB          = 0x20000,
	CA_ASSET_INTERNALTYPE = 0x40000,
	CA_ASSET_BIG_ENDIAN   = 0x80000000
};

enum CA_AnimationFlags
{

	//-------------------------------------------------------------
	//FLAGS TO HANDLE ANIMATION PLAYBACK
	//-------------------------------------------------------------
	//	All playback flags are exclusive. Only one of them can be enabled for an animation at a time. Order of priotity:
	//	CA_MANUAL_UPDATE, (disables looping and repeat)
	//	CA_LOOP_ANIMATION, (disables repeat)
	//	CA_REPEAT_LAST_KEY,

	//! With this flag the animation is not being updated automatically. The user needs to set the time manually. This is used for steering-wheels and mounted-weapons, where we convert the rotation of an object into animation-keytimes.
	CA_MANUAL_UPDATE = 0x000001,

	//! Plays an animations in an endless loop till we stop this animation or start a new animation.
	CA_LOOP_ANIMATION = 0x000002,

	//! Plays an animation once and then repeats the last keyframe. Without this flag we remove the animation from the FIFO.
	CA_REPEAT_LAST_KEY = 0x000004,

	//-------------------------------------------------------------
	//FLAGS TO HANDLE TRANSITIONS
	//-------------------------------------------------------------

	//! Linear time-warping of animations to align animation with similar properties.
	CA_TRANSITION_TIMEWARPING = 0x000008,

	//! Don't start a transition immediately. Wait till the previous animation is passing a specified key-time.
	CA_START_AT_KEYTIME = 0x000010,

	//! Can be simulated by using the flags "CA_START_AT_KEYTIME" and "CA_REPEAT_LAST_KEY".
	CA_START_AFTER = 0x000020,

	//! When playing an Idel2Move transition, we would like to find the best transition point to start a locomotion-animation.
	CA_IDLE2MOVE = 0x000040,

	//! When playing an locomotion-animation, we would like to find the best transition point to start a Move2Idle.
	CA_MOVE2IDLE = 0x000080,

	//! By default it is not possible to start the same animation twice. In some special cases this is sometimes necessary (e.g. recoil-animations). By enabling this flag we can restart the same animation using the previously described transition-rules.
	CA_ALLOW_ANIM_RESTART = 0x000100,

	//-------------------------------------------------------------
	//FLAGS TO HANDLE CONTROLLER USAGE
	//-------------------------------------------------------------
	//! Enforce 30Hz sampling.
	CA_KEYFRAME_SAMPLE_30Hz = 0x000200,    //don't interpolate between 2 keyframes (do only 30hz sampling)

	//! Don't allow multilayer animations when this flag is set in Layer0.
	CA_DISABLE_MULTILAYER = 0x000400,

	//! When an object is not visible, then we just update the animation but NOT the skeleton. The skeleton will stay in the last updated pose.
	//! This can can be a problem in some situations, for example when an animation is used to open a door, or when we use skeleton animation.
	//! to move physical objects in the game. A good example is the moving platform in MP to lift up the VTOL and the helicopters).
	//! The only way to enforce the skeleton-update is to use this flag per animation.
	CA_FORCE_SKELETON_UPDATE = 0x000800,

	//-------------------------------------------------------------
	//FLAGS FOR SPECIAL ANIMATION MODES
	//-------------------------------------------------------------
	//!	Play this animation only when you're in Track-View.
	CA_TRACK_VIEW_EXCLUSIVE = 0x001000,

	//! Usually we always update animations, even when the object is not visible.
	//! For simple objects (e.g. boids) we want to avoid even animation-update. This can lead to an overflow in the FIFO-queue.
	//! To avoid the overflow, we remove the first animation from the FIFO when there are more then 16 animation in the queue.
	CA_REMOVE_FROM_FIFO   = 0x002000,

	CA_FULL_ROOT_PRIORITY = 0x004000,

	//! An animation with this flag makes sure it's transitioned to.
	//! Usually some conditions (anims in the same queue with flags Idle2Move, Move2Idle, StartKeyAfter,
	//! StartKeyAtTime, not in memory) can delay the transition, this flag removes them all.
	CA_FORCE_TRANSITION_TO_ANIM = 0x008000,

	//! Fadeout works only for animations in higher layers and should be used together with CA_REPEAT_LAST_KEY.
	CA_FADEOUT = 0x40000000,
};

enum CA_DynAnimationFlags
{
	CA_ACTIVATED                 = 0x0001,
	CA_REMOVE_FROM_QUEUE         = 0x0002,
	CA_TW_FLAG                   = 0x0004,  //probably not needed
	CA_EOC                       = 0x0008,
	CA_LOOPED                    = 0x0010,
	CA_REPEAT                    = 0x0020,
	CA_NEGATIVE_EOC              = 0x0040,
	CA_ANIMEVENTS_EVALUATED_ONCE = 0x0080,
	CA_LOOPED_THIS_UPDATE        = 0x0100,
};

enum CA_Dimension_Flags
{
	//this is currently limited to only 8 flags
	CA_Dim_Initialized     = 0x001,
	CA_Dim_LockedParameter = 0x002,
	CA_Dim_DeltaExtraction = 0x004,
};
#define NUM_ANIMATION_USER_DATA_SLOTS 8

//--------------------------------------------------------------------------------
enum class CA_Interpolation_Type
{
	Linear = 0,
	QuadraticIn,
	QuadraticOut,
	QuadraticInOut,
	SineIn,
	SineOut,
	SineInOut
};

//--------------------------------------------------------------------------------
struct SMotionParameterDetails
{
	enum EFlags
	{
		ADDITIONAL_EXTRACTION = 1 << 0,
	};

	const char* name;
	const char* humanReadableName;
	int         flags;
};

//--------------------------------------------------------------------------------

enum EMotionParamID
{
	eMotionParamID_TravelSpeed = 0,
	eMotionParamID_TurnSpeed,
	eMotionParamID_TravelAngle,     //!< Forward, backwards and sidestepping.
	eMotionParamID_TravelSlope,
	eMotionParamID_TurnAngle,       //!< Idle2Move and idle-rotations.
	eMotionParamID_TravelDist,      //!< Idle-steps.
	eMotionParamID_StopLeg,         //!< Move2Idle.

	eMotionParamID_BlendWeight,     //!< Custom parameters.
	eMotionParamID_BlendWeight2,
	eMotionParamID_BlendWeight3,
	eMotionParamID_BlendWeight4,
	eMotionParamID_BlendWeight5,
	eMotionParamID_BlendWeight6,
	eMotionParamID_BlendWeight7,
	eMotionParamID_BlendWeight_Last = eMotionParamID_BlendWeight7,

	eMotionParamID_COUNT,
	eMotionParamID_INVALID = eMotionParamID_COUNT
};

//--------------------------------------------------------------------------------

struct SAnimMemoryTracker
{
	uint32 m_nMemTracker;
	uint32 m_nAnimsCurrent;
	uint32 m_nAnimsMax;
	uint64 m_nAnimsAdd;
	uint32 m_nAnimsCounter;
	uint32 m_nGlobalCAFs;
	uint32 m_nUsedGlobalCAFs;

	uint32 m_numTCharInstances;
	uint32 m_nTotalCharMemory;
	uint32 m_numTSkinInstances;
	uint32 m_nTotalSkinMemory;
	uint32 m_numModels;
	uint32 m_nTotalMMemory;

	SAnimMemoryTracker()
	{
		m_nMemTracker = 0;
		m_nAnimsCurrent = 0;
		m_nAnimsMax = 0;
		m_nAnimsAdd = 0;
		m_nAnimsCounter = 0;
		m_nGlobalCAFs = 0;
		m_nUsedGlobalCAFs = 0;

		m_numTCharInstances = 0;
		m_nTotalCharMemory = 0;
		m_numTSkinInstances = 0;
		m_nTotalSkinMemory = 0;
		m_numModels = 0;
		m_nTotalMMemory = 0;
	}

};

class CAnimEventData
{
public:
	CAnimEventData()
		: m_normalizedTime(0)
		, m_normalizedEndTime(0)
		, m_nameLowercaseCRC32(0)
		, m_vOffset(0, 0, 0)
		, m_vDir(0, 0, 0)
	{
	}

	~CAnimEventData() {}

	f32  GetNormalizedTime() const { return m_normalizedTime; }
	void SetNormalizedTime(f32 normalizedTime)
	{
		m_normalizedTime = clamp_tpl(normalizedTime, 0.f, 1.f);
		m_normalizedEndTime = max(normalizedTime, m_normalizedEndTime);
	}

	f32  GetNormalizedEndTime() const { return m_normalizedEndTime; }
	void SetNormalizedEndTime(f32 normalizedEndTime)
	{
		m_normalizedEndTime = clamp_tpl(normalizedEndTime, m_normalizedTime, 1.f);
	}

	const char* GetName() const { return m_strEventName.c_str(); }
	void        SetName(const char* name)
	{
		m_strEventName = name;
		m_nameLowercaseCRC32 = CCrc32::ComputeLowercase(name);
	}

	uint32      GetNameLowercaseCRC32() const                    { return m_nameLowercaseCRC32; }

	const char* GetCustomParameter() const                       { return m_strCustomParameter.c_str(); }
	void        SetCustomParameter(const char* customParamenter) { m_strCustomParameter = customParamenter; }

	const char* GetBoneName() const                              { return m_strBoneName.c_str(); }
	void        SetBoneName(const char* boneName)                { m_strBoneName = boneName; }

	const char* GetModelName() const                             { return m_strModelName.c_str(); }
	void        SetModelName(const char* modelName)              { m_strModelName = modelName; }

	const Vec3& GetOffset() const                                { return m_vOffset; }
	void        SetOffset(const Vec3& offset)                    { m_vOffset = offset; }

	const Vec3& GetDirection() const                             { return m_vDir; }
	void        SetDirection(const Vec3& direction)              { m_vDir = direction; }

	void        GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_strEventName);
		pSizer->AddObject(m_strCustomParameter);
		pSizer->AddObject(m_strBoneName);
		pSizer->AddObject(m_strModelName);
	}

	size_t GetAllocSize() const
	{
		size_t allocSize = 0;
		allocSize += m_strEventName.capacity();
		allocSize += m_strCustomParameter.capacity();
		allocSize += m_strBoneName.capacity();
		allocSize += m_strModelName.capacity();
		return allocSize;
	}

private:
	f32    m_normalizedTime;
	f32    m_normalizedEndTime;
	uint32 m_nameLowercaseCRC32;
	string m_strEventName;
	string m_strCustomParameter;
	string m_strBoneName;
	string m_strModelName;
	Vec3   m_vOffset;
	Vec3   m_vDir;
};

struct AnimEventInstance
{
	f32         m_time;
	f32         m_endTime;
	uint32      m_EventNameLowercaseCRC32;
	const char* m_EventName;
	const char* m_CustomParameter; //!< Meaning depends on event - sound: sound path, effect: effect name.
	const char* m_BonePathName;
	Vec3        m_vOffset;
	Vec3        m_vDir;

	// TODO: Use this..
	const CAnimEventData* m_pEventData;

	AnimEventInstance()
	{
		m_time = 0;
		m_EventNameLowercaseCRC32 = 0;
		m_EventName = 0;
		m_CustomParameter = 0;
		m_BonePathName = 0;
		m_vOffset = Vec3(0, 0, 0);
		m_vDir = Vec3(0, 0, 0);

		m_pEventData = NULL;
	}

	void SetAnimEventData(const CAnimEventData& animEventData)
	{
		m_time = animEventData.GetNormalizedTime();
		m_endTime = animEventData.GetNormalizedEndTime();
		m_EventName = animEventData.GetName();
		m_EventNameLowercaseCRC32 = animEventData.GetNameLowercaseCRC32();
		m_CustomParameter = animEventData.GetCustomParameter();
		m_BonePathName = animEventData.GetBoneName();
		m_vOffset = animEventData.GetOffset();
		m_vDir = animEventData.GetDirection();
		m_pEventData = &animEventData;
	}
};

#if defined(USE_PROTOTYPE_ABS_BLENDING)
struct SJointMask
{
	struct SJointWeight
	{
		SJointWeight(int _jointID, float _weight)
			:
			jointID(_jointID),
			weight(_weight)
		{
		}

		int   jointID;
		float weight;
	};
	DynArray<SJointWeight> weightList;
};
#endif //!defined(USE_PROTOTYPE_ABS_BLENDING)

//! This structure describes the parameters used to start an animation on a character.
struct CRY_ALIGN(8) CryCharAnimationParams
{
	CryCharAnimationParams(int _nLayerID = 0, uint32 _nFlags = 0)
		: m_fTransTime(-1.0f)
		  , m_fKeyTime(-1.0f)
		  , m_fPlaybackSpeed(1.0f)
		  , m_fAllowMultilayerAnim(1.0f)
		  , m_nLayerID(_nLayerID)
		  , m_nFlags(_nFlags)
		  , m_nInterpolationType(CA_Interpolation_Type::Linear)
		  , m_nUserToken(0)
		  , m_fPlaybackWeight(1.0f)
#if defined(USE_PROTOTYPE_ABS_BLENDING)
		  , m_pJointMask(NULL)
#endif //!defined(USE_PROTOTYPE_ABS_BLENDING)
	{
		for (int i = 0; i < NUM_ANIMATION_USER_DATA_SLOTS; i++)
			m_fUserData[i] = 0.0f;
	}
	//! Transition time between two animations.
	f32 m_fTransTime;

	//! keytime[0-1]. can be used to start a transition animation.
	f32 m_fKeyTime;

	//! Multiplier for animation-update.
	f32 m_fPlaybackSpeed;

	//! If this is '1' then we can play animation on higher layers and they overwrite the channels on lower layers.
	f32 m_fAllowMultilayerAnim;

	//! Specify the layer where to start the animation.
	int32 m_nLayerID;

	//! Animation specific weight multiplier, applied on top of the existing layer weight.
	f32 m_fPlaybackWeight;

	//! Combination of flags defined above.
	uint32 m_nFlags;

	// Transition Interpolation method.
	CA_Interpolation_Type m_nInterpolationType;

	//! Token specified by the animation calling code for it's own benefit.
	uint32 m_nUserToken;

	//! a set of weights that are blended together just like the animation is, for calling code's benefit.
	f32 m_fUserData[NUM_ANIMATION_USER_DATA_SLOTS];

#if defined(USE_PROTOTYPE_ABS_BLENDING)
	const SJointMask* m_pJointMask;
#endif //!defined(USE_PROTOTYPE_ABS_BLENDING)

	void Serialize(TSerialize ser)
	{
		if (ser.GetSerializationTarget() != eST_Network)
		{
			ser.BeginGroup("CharAnimationParams");
			for (int32 i = 0; i < NUM_ANIMATION_USER_DATA_SLOTS; i++)
			{
				string serializeName;
				serializeName.Format("fUserData%i", i);
				ser.Value(serializeName.c_str(), m_fUserData[i]);
			}

			ser.Value("fTransTime", m_fTransTime);
			ser.Value("fKeyTime", m_fKeyTime);
			ser.Value("fPlaybackSpeed", m_fPlaybackSpeed);
			ser.Value("fAllowMultilayerAnim", m_fAllowMultilayerAnim);
			ser.Value("nLayerID", m_nLayerID);
			ser.Value("nUserToken", m_nUserToken);
			ser.Value("nFlags", m_nFlags);
			ser.EndGroup();
		}
	}

};

struct SParametricSampler
{
	uint8         m_nParametricType;                                      //!< Type of Group: i.e. I2M, M2I, MOVE, Idle-Step, Idle-Rot, etc.
	uint8         m_numDimensions;                                        //!< How many dimensions are used in this Parametric Group.
	f32           m_MotionParameter[MAX_LMG_DIMENSIONS];                  //!< The motion parameter value.
	f32           m_MotionParameterForNextIteration[MAX_LMG_DIMENSIONS];  //!< This motion parameter is applied on the next iteration in case of looping animations. Has no effect otherwise.
	uint8         m_MotionParameterID[MAX_LMG_DIMENSIONS];                //!< The motion parameter id/name.
	uint8         m_MotionParameterFlags[MAX_LMG_DIMENSIONS];             //!< Flags relevant to this motion parameter. /see CA_Dimension_Flags
	virtual uint8 GetCurrentSegmentIndexBSpace() const = 0;
	virtual ~SParametricSampler() {};
};

class CRY_ALIGN(16) CAnimation
{
public:
	friend class CSkeletonAnim;
	friend class CTransitionQueue;

	CAnimation()
		: m_pParametricSampler(NULL)
		  , m_nStaticFlags(0)
		  , m_animationId(0)
		  , m_currentSegmentExpectedDurationSeconds(-1.f)
		  , m_expectedTotalDurationSeconds(-1.f)
		  , m_fCurrentDeltaTime(-1.f)
		  , m_fStartTime(0.0f)
		  , m_nInterpolationType(CA_Interpolation_Type::Linear)
		  , m_fTransitionTime(1.0f)
		  , m_fTransitionPriority(0.f)
		  , m_fTransitionWeight(-1.f)
		  , m_fPlaybackScale(1.0f)
		  , m_fPlaybackWeight(1.0f)
		  , m_nUserToken(0)
	{
		m_fAnimTimePrev[0] = 0.0f;
		m_fAnimTimePrev[1] = 0.0f;
		m_fAnimTime[0] = 0.0f;
		m_fAnimTime[1] = 0.0f;
		m_currentSegmentIndexPrev[0] = 0;
		m_currentSegmentIndexPrev[1] = 0;
		m_currentSegmentIndex[0] = 0;
		m_currentSegmentIndex[1] = 0;
		m_DynFlags[0] = 0;
		m_DynFlags[1] = 0;
		for (int i = 0; i < NUM_ANIMATION_USER_DATA_SLOTS; i++)
			m_fUserData[i] = 0.0f;
	}

	virtual ~CAnimation()
	{
	}

	void Serialize(TSerialize ser)
	{
		if (ser.GetSerializationTarget() != eST_Network)
		{
			ser.BeginGroup("CAnimation");
			{
				ser.Value("animationId", m_animationId);

				ser.Value("segmentNormalizedTimeDelta", m_fCurrentDeltaTime);

				ser.Value("transitionPriority", m_fTransitionPriority);
				ser.Value("transitionWeight", m_fTransitionWeight);

				ser.Value("currentSegmentExpectedDurationSeconds", m_currentSegmentExpectedDurationSeconds);
				ser.Value("expectedTotalDurationSeconds", m_currentSegmentExpectedDurationSeconds);

				ser.Value("currentSegmentIndexPrev0", m_currentSegmentIndexPrev[0]);
				ser.Value("currentSegmentIndexPrev1", m_currentSegmentIndexPrev[1]);
				ser.Value("currentSegmentIndex0", m_currentSegmentIndex[0]);
				ser.Value("currentSegmentIndex1", m_currentSegmentIndex[1]);
				ser.Value("segmentNormalizedTimePrev0", m_fAnimTimePrev[0]);
				ser.Value("segmentNormalizedTimePrev1", m_fAnimTimePrev[1]);
				ser.Value("segmentNormalizedTime0", m_fAnimTime[0]);
				ser.Value("segmentNormalizedTime1", m_fAnimTime[1]);
				ser.Value("DynFlags0", m_DynFlags[0]);
				ser.Value("DynFlags1", m_DynFlags[1]);

				ser.Value("nFlags", m_nStaticFlags);
				ser.Value("fTransitionTime", m_fTransitionTime);
				ser.Value("fStartTime", m_fStartTime);
				ser.Value("fPlaybackSpeed", m_fPlaybackScale);
				ser.Value("nUserToken", m_nUserToken);
				for (int32 i = 0; i < NUM_ANIMATION_USER_DATA_SLOTS; i++)
				{
					string serializeName;
					serializeName.Format("fUserData%i", i);
					ser.Value(serializeName.c_str(), m_fUserData[i]);

				}
			}
			ser.EndGroup();
		}
	}

	void                      GetMemoryUsage(ICrySizer* pSizer) const {}

	SParametricSampler*       GetParametricSampler()                  { return m_pParametricSampler; }
	const SParametricSampler* GetParametricSampler() const            { return m_pParametricSampler; }

	int16                     GetAnimationId() const                  { return m_animationId; }
	virtual uint8             GetCurrentSegmentIndex() const
	{
		if (m_pParametricSampler)
			return m_pParametricSampler->GetCurrentSegmentIndexBSpace();
		return m_currentSegmentIndex[0];
	}

	f32 Interpolate(f32 value, CA_Interpolation_Type interpolationType)
	{
		switch (interpolationType)
		{
		case CA_Interpolation_Type::QuadraticIn:
		{
			return clamp_tpl(value * value, 0.f, 1.f);
		}
		case CA_Interpolation_Type::QuadraticOut:
		{
			return clamp_tpl(-value * (value - 2.0f), 0.f, 1.f);
		}
		case CA_Interpolation_Type::QuadraticInOut:
		{
			value = clamp_tpl(value, 0.f, 1.f) - 0.5f;
			return value / (0.5f + 2.0f * value * value) + 0.5f;
		}
		case CA_Interpolation_Type::SineIn:
		{
			return clamp_tpl(-cosf(value * gf_PI * 0.5f) + 1.0f, 0.f, 1.f);
		}
		case CA_Interpolation_Type::SineOut:
		{
			return clamp_tpl(sinf(value * gf_PI * 0.5f), 0.f, 1.f);
		}
		case CA_Interpolation_Type::SineInOut:
		{
			return clamp_tpl(-0.5f * (cosf(value * gf_PI) - 1.0f), 0.f, 1.f);
		}
		default:
		{
			return clamp_tpl(value, 0.0f, 1.0f);
		}
		}

		return value;
	}

	bool HasStaticFlag(uint32 animationFlag) const { return ((m_nStaticFlags & animationFlag) == animationFlag); }
	void SetStaticFlag(uint32 nStaticFlags)        { m_nStaticFlags |= nStaticFlags; }  // TODO: check with game-team if really needed.
	void ClearStaticFlag(uint32 nStaticFlags)      { m_nStaticFlags &= ~nStaticFlags; } // TODO: check with game-team if really needed.

	//! See Get/SetAnimationNormalizedTime and Get/SetLayerNormalizedTime functions in ISkeletonAnim to get or set the real normalized time of an animation taking its segmentation into account.
	f32  GetCurrentSegmentNormalizedTimePrevious() const                    { return m_fAnimTimePrev[0]; }
	void SetCurrentSegmentNormalizedTimePrevious(f32 normalizedSegmentTime) { m_fAnimTimePrev[0] = clamp_tpl(normalizedSegmentTime, 0.f, 1.f); }
	f32  GetCurrentSegmentNormalizedTime() const                            { return m_fAnimTime[0]; }
	void SetCurrentSegmentNormalizedTime(f32 normalizedSegmentTime)         { m_fAnimTime[0] = clamp_tpl(normalizedSegmentTime, 0.f, 1.f); }
	f32  GetCurrentSegmentNormalizedTimeDelta() const                       { return m_fCurrentDeltaTime; }

	CA_Interpolation_Type GetTransitionInterpolationType() const            { return m_nInterpolationType; }
	void SetTransitionInterpolationType(CA_Interpolation_Type nInterpolationType) { m_nInterpolationType = nInterpolationType; }

	f32  GetTransitionPriority() const                                      { return m_fTransitionPriority; }
	void SetTransitionPriority(f32 transitionPriority)                      { m_fTransitionPriority = clamp_tpl(transitionPriority, 0.f, 1.f); }

	f32  GetTransitionWeight() const                                        { return m_fTransitionWeight; }
	void SetTransitionWeight(f32 transitionWeight)                          { m_fTransitionWeight = transitionWeight; }
	void SetTransitionWeightRequested(f32 transitionWeight)                 { m_fTransitionWeight = Interpolate(transitionWeight, m_nInterpolationType); }

	f32  GetTransitionTime() const                                          { return m_fTransitionTime; }
	void SetTransitionTime(f32 transitionTime)                              { m_fTransitionTime = transitionTime; }

	f32  GetPlaybackWeight() const                                          { return m_fPlaybackWeight; }
	void SetPlaybackWeight(f32 playbackWeight)
	{
		assert(playbackWeight >= 0.0f);
		assert(playbackWeight <= 1.0f);
		m_fPlaybackWeight = playbackWeight;
	}

	f32  GetPlaybackScale() const { return m_fPlaybackScale; }
	void SetPlaybackScale(f32 playbackScale)
	{
		assert(playbackScale >= 0.0f);
		m_fPlaybackScale = max(0.0f, playbackScale);
	}

	bool   HasUserToken(uint32 userToken) const                                  { return (m_nUserToken == userToken); }
	uint32 GetUserToken() const                                                  { return m_nUserToken; }
	void   SetUserToken(uint32 nUserToken)                                       { m_nUserToken = nUserToken; }

	f32    GetExpectedTotalDurationSeconds() const                               { return m_expectedTotalDurationSeconds; }
	void   SetExpectedTotalDurationSeconds(f32 expectedDurationSeconds)          { m_expectedTotalDurationSeconds = expectedDurationSeconds; }

	f32    GetCurrentSegmentExpectedDurationSeconds() const                      { return m_currentSegmentExpectedDurationSeconds; }
	void   SetCurrentSegmentExpectedDurationSeconds(f32 expectedDurationSeconds) { m_currentSegmentExpectedDurationSeconds = expectedDurationSeconds; }

	// return RT-flags.
	void   ClearActivated()           { m_DynFlags[0] &= ~CA_ACTIVATED; } // TODO: check with game-team if really needed.
	void   ClearAnimEventsEvaluated() { m_DynFlags[0] &= ~CA_ANIMEVENTS_EVALUATED_ONCE; }
	uint32 IsActivated()        const { return m_DynFlags[0] & CA_ACTIVATED;         }
	uint32 GetRepeat()          const { return m_DynFlags[0] & CA_REPEAT;            }
	uint32 GetLoop()            const { return m_DynFlags[0] & CA_LOOPED;            }
	uint32 GetRemoveFromQueue() const { return m_DynFlags[0] & CA_REMOVE_FROM_QUEUE; }
	uint32 GetEndOfCycle()      const { return m_DynFlags[0] & CA_EOC;               }
	uint32 GetUseTimeWarping()  const { return m_DynFlags[0] & CA_TW_FLAG;           }

protected:
	SParametricSampler* m_pParametricSampler;

	int16 m_animationId;
	uint8 m_currentSegmentIndexPrev[2];
	uint8 m_currentSegmentIndex[2];

	f32 m_fCurrentDeltaTime;        //!< This is the delta in normalized segment time.

	f32 m_fStartTime;               //!< keytime[0-1]. can be used to start a transition animation.
	f32 m_fTransitionTime;          //!< Transition time between two animations.
	f32 m_fTransitionPriority;      //!< Motion priority: a value of '1' is overwriting all other motion in the queue.
	f32 m_fTransitionWeight;        //!< This is the real percentage value for all active motions in the transition queue.
	f32 m_fPlaybackScale;           //!< Multiplier for animation-update.
	f32 m_fPlaybackWeight;          //!< Multiplier for the additive weight

	f32 m_currentSegmentExpectedDurationSeconds;
	f32 m_expectedTotalDurationSeconds;

	f32 m_fAnimTimePrev[2];         //!< This is a percentage value between 0-1 for the current segment.
	f32 m_fAnimTime[2];             //!< This is a percentage value between 0-1 for the current segment.
	uint32 m_nStaticFlags;          //!< Static animation-flags (needs to be a 32-bit register).
	uint16 m_DynFlags[2];
	CA_Interpolation_Type m_nInterpolationType;                           // defines the interpolation curve

	uint32 m_nUserToken;                              //!< Token specified by the animation calling code for it's own benefit.
	f32 m_fUserData[NUM_ANIMATION_USER_DATA_SLOTS];   //!< A set of weights that are blended together just like the animation is, for calling code's benefit.

#if defined(USE_PROTOTYPE_ABS_BLENDING)
	const SJointMask* m_pJointMask;
#endif //!defined(USE_PROTOTYPE_ABS_BLENDING)

};