// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <set>
#include "AnimNode.h"
#include <CryCore/StlUtils.h>

#include <CryAnimation/IFacialAnimation.h>

#define ENTITY_SOUNDTRACKS     3
#define ENTITY_EXPRTRACKS      3
#define MAX_CHARACTER_TRACKS   3
#define ADDITIVE_LAYERS_OFFSET 6
#define DEFAULT_LOOKIK_LAYER   15

// When preloading of a trackview sequence is triggered, the animations in
// the interval [0 ; ANIMATION_KEY_PRELOAD_INTERVAL] are preloaded into memory.
// When the animation is running Animate() makes sure that all animation keys
// between in the interval [currTime; currTime + ANIMATION_KEY_PRELOAD_INTERVAL]
// are kept in memory.
// This value should be adjusted so that the streaming system has enough
// time to stream the keys in.
#define ANIMATION_KEY_PRELOAD_INTERVAL SAnimTime(2.0f)

// remove comment to enable the check.
//#define CHECK_FOR_TOO_MANY_ONPROPERTY_SCRIPT_CALLS

typedef std::set<string>     TStringSet;
typedef TStringSet::iterator TStringSetIt;

struct ISkeletonAnim;

class CAnimationCacher
{
public:
	CAnimationCacher() { m_cachedAnims.reserve(MAX_CHARACTER_TRACKS); }
	~CAnimationCacher() { ClearCache(); }
	void AddToCache(uint32 animPathCRC);
	void RemoveFromCache(uint32 animPathCRC);
	void ClearCache();
private:
	std::vector<uint32> m_cachedAnims;
};

class CAnimEntityNode : public CAnimNode, public IAnimEntityNode
{
	struct SScriptPropertyParamInfo;
	struct SComponentPropertyParamInfo;
	struct SAnimState;
	friend class CComponentSerializer;

public:
	CAnimEntityNode(const int id);
	~CAnimEntityNode();
	static void              Initialize();

	void                     EnableEntityPhysics(bool bEnable);

	virtual EAnimNodeType    GetType() const override { return eAnimNodeType_Entity; }

	virtual void             AddTrack(IAnimTrack* pTrack) override;

	virtual void             SetEntityGuid(const EntityGUID& guid) override;
	virtual EntityGUID*      GetEntityGuid() override { return &m_entityGuid; }
	virtual void             SetEntityId(const int entityId) override;
	virtual IEntity*         GetEntity() override;

	virtual void             SetEntityGuidTarget(const EntityGUID& guid) override;
	virtual void             SetEntityGuidSource(const EntityGUID& guid) override;

	virtual void             StillUpdate() override;
	virtual void             Animate(SAnimContext& animContext) override;

	virtual void             CreateDefaultTracks() override;

	virtual void             PrecacheStatic(SAnimTime startTime) override;
	virtual void             PrecacheDynamic(SAnimTime time) override;

	virtual Vec3             GetPos() override    { return m_pos; };
	virtual Quat             GetRotate() override { return m_rotate; };
	virtual Vec3             GetScale() override  { return m_scale; };

	virtual void             Activate(bool bActivate) override;

	virtual void             Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) override;
	virtual void             OnReset() override;
	virtual void             OnStart() override;
	virtual void             OnPause() override;
	virtual void             OnStop() override;

	virtual unsigned int     GetParamCount() const override;
	virtual CAnimParamType   GetParamType(unsigned int nIndex) const override;
	virtual const char*      GetParamName(const CAnimParamType& param) const override;

	static int               GetParamCountStatic();
	static bool              GetParamInfoStatic(int nIndex, SParamInfo& info);

	ILINE void               SetSkipInterpolatedCameraNode(const bool bSkipNodeAnimation) { m_bSkipInterpolatedCameraNodeAnimation = bSkipNodeAnimation; }
	ILINE bool               IsSkipInterpolatedCameraNodeEnabled() const                  { return m_bSkipInterpolatedCameraNodeAnimation; }

	ILINE const Vec3&        GetCameraInterpolationPosition() const                       { return m_vInterpPos; }
	ILINE void               SetCameraInterpolationPosition(const Vec3& vNewPos)          { m_vInterpPos = vNewPos; }

	ILINE const Quat&        GetCameraInterpolationRotation() const                       { return m_interpRot; }
	ILINE void               SetCameraInterpolationRotation(const Quat& rotation)         { m_interpRot = rotation; }

	virtual IAnimEntityNode* QueryEntityNodeInterface() override                          { return this; }

protected:
	virtual bool         GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;
	int                  GetEntityId() const { return m_EntityId; }

	void                 StopAudio();
	void                 ApplyEventKey(class CEventTrack* track, int keyIndex, SEventKey& key);
	void                 ApplyAudioTriggerKey(CryAudio::ControlId audioTriggerId, bool const bPlay = true);
	Vec3                 Adjust3DSoundOffset(bool bVoice, IEntity* pEntity, Vec3& oSoundPos) const;
	void                 AnimateCharacterTrack(class CCharacterTrack* track, SAnimContext& animContext, int layer, int trackIndex, SAnimState& animState, IEntity* pEntity, ICharacterInstance* pCharacter);
	bool                 CheckTimeJumpingOrOtherChanges(const SAnimContext& animContext, int32 activeKeys[], int32 numActiveKeys, ICharacterInstance* pCharacter, int layer, int trackIndex, SAnimState& animState);
	void                 UpdateAnimTimeJumped(int32 keyIndex, class CCharacterTrack * track, SAnimTime ectime, ICharacterInstance * pCharacter, int layer, bool bAnimEvents, int trackIndex, SAnimState & animState);
	void                 UpdateAnimRegular(int32 numActiveKeys, int32 activeKeys[], class CCharacterTrack * track, SAnimTime ectime, ICharacterInstance * pCharacter, int layer, bool bAnimEvents);
	void                 UpdateAnimBlendGap(int32 activeKeys[], class CCharacterTrack * track, SAnimTime ectime, ICharacterInstance * pCharacter, int layer);
	void                 ApplyAnimKey(int32 keyIndex, class CCharacterTrack * track, SAnimTime ectime, ICharacterInstance * pCharacter, int layer, int animIndex, bool bAnimEvents);
	void                 StopExpressions();
	void                 AnimateExpressionTrack(class CExprTrack* pTrack, SAnimContext& animContext);
	void                 AnimateFacialSequence(class CFaceSequenceTrack* pTrack, SAnimContext& animContext);
	void                 AnimateLookAt(class CLookAtTrack* pTrack, SAnimContext& animContext);
	bool                 AnimateEntityProperty(IAnimTrack* pTrack, SAnimContext& animContext, const char* name);
	bool                 AnimateLegacyScriptProperty(const SScriptPropertyParamInfo& param, IEntity* pEntity, IAnimTrack* pTrack, SAnimContext& animContext);
	bool                 AnimateEntityComponentProperty(const SComponentPropertyParamInfo& param, IEntity* pEntity, IAnimTrack* pTrack, SAnimContext& animContext);

	void                 ReleaseAllAnims();
	virtual void         OnStartAnimation(const char* sAnimation) {}
	virtual void         OnEndAnimation(const char* sAnimation);

	ILINE bool           AnimationPlaying(const SAnimState& animState) const;

	void                 PrepareAnimations();

	IFacialAnimSequence* LoadFacialSequence(const char* sequenceName);
	void                 ReleaseAllFacialSequences();
	void                 EnableProceduralFacialAnimation(bool enable);
	bool                 GetProceduralFacialAnimationEnabled();

	// functions involved in the process to parse and store lua animated properties
	virtual void UpdateDynamicParams() override;
	virtual void UpdateDynamicParams_Editor();
	virtual void UpdateDynamicParams_PureGame();

	void         FindDynamicPropertiesRec(IScriptTable* pScriptTable, const string& currentPath, unsigned int depth);
	void         AddPropertyToParamInfoMap(const char* pKey, const string& currentPath, SScriptPropertyParamInfo& paramInfo);
	bool         ObtainPropertyTypeInfo(const char* pKey, IScriptTable* pScriptTable, SScriptPropertyParamInfo& paramInfo);
	void         FindScriptTableForParameterRec(IScriptTable* pScriptTable, const string& path, string& propertyName, SmartScriptTable& propertyScriptTable);

	virtual void InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType) override;

	enum EUpdateEntityFlags
	{
		eUpdateEntity_Position  = 1 << 0,
		eUpdateEntity_Rotation  = 1 << 1,
		eUpdateEntity_Animation = 1 << 2,
	};

protected:
	Vec3 m_pos;
	Quat m_rotate;
	Vec3 m_scale;

private:
	void UpdateEntityPosRotVel(const Vec3& targetPos, const Quat& targetRot, const bool initialState, const int flags, SAnimTime fTime);
	void StopEntity();
	void UpdateTargetCamera(IEntity* pEntity, const Quat& rotation);
	void RestoreEntityDefaultValues();

	//! Reference to game entity.
	EntityGUID       m_entityGuid;
	EntityId         m_EntityId;

	TStringSet       m_setAnimationSinks;

	CAnimationCacher m_animationCacher;

	//! Pointer to target animation node.
	_smart_ptr<IAnimNode> m_target;

	// Cached parameters of node at given time.
	SAnimTime m_time;
	Vec3      m_velocity;
	Vec3      m_angVelocity;

	// Camera2Camera interpolation values
	Vec3 m_vInterpPos;
	Quat m_interpRot;
	bool m_bSkipInterpolatedCameraNodeAnimation;

	//! Last animated key in Entity track.
	int m_lastEntityKey;

	struct SAnimState
	{
		int32 m_lastAnimationKeys[3][2];
		bool  m_layerPlaysAnimation[3];

		//! This is used to indicate that a time-jumped blending is currently happening in the animation track.
		bool  m_bTimeJumped[3];
		float m_jumpTime[3];
	};

	SAnimState              m_baseAnimState;

	bool                    m_bWasTransRot;
	bool                    m_bIsAnimDriven;
	bool                    m_visible;
	bool                    m_bInitialPhysicsStatus;

	std::vector<int>              m_audioSwitchTracks;
	std::vector<float>            m_audioParameterTracks;
	std::vector<SAudioInfo>       m_audioTriggerTracks;
	std::vector<SAudioInfo>       m_audioFileTracks;
	std::vector<SAudioTriggerKey> m_activeAudioTriggers;

	int                     m_lastDrsSignalKey;
	int                     m_iCurMannequinKey;

	TStringSet              m_setExpressions;

	string                  m_lookAtTarget;
	EntityId                m_lookAtEntityId;
	bool                    m_lookAtLocalPlayer;
	bool                    m_allowAdditionalTransforms;
	string                  m_lookPose;

	typedef std::map<string, _smart_ptr<IFacialAnimSequence>, stl::less_stricmp<string>> FacialSequenceMap;
	FacialSequenceMap m_facialSequences;
	bool              m_proceduralFacialAnimationEnabledOld;

	//! Reference LookAt entities.
	EntityGUID m_entityGuidTarget;
	EntityId   m_EntityIdTarget;
	EntityGUID m_entityGuidSource;
	EntityId   m_EntityIdSource;

	// Pos/rot noise parameters
	struct Noise
	{
		float m_amp;
		float m_freq;

		Vec3  Get(SAnimTime time) const;

		Noise() : m_amp(0.0f), m_freq(0.0f) {}
	};

	Noise m_posNoise;
	Noise m_rotNoise;

	struct IPropertyParamInfo
	{
		enum class EType
		{
			LegacyScriptProperty,
			ComponentProperty
		};

		virtual EType GetType() const = 0;

		SParamInfo animNodeParamInfo;
	};

	struct SScriptPropertyParamInfo final : public IPropertyParamInfo
	{
		virtual EType GetType() const override { return EType::LegacyScriptProperty; }

		string           variableName;
		string           displayName;
		SmartScriptTable scriptTable;
		bool             isVectorTable;
	};

	struct SComponentPropertyParamInfo final : public IPropertyParamInfo
	{
		virtual EType GetType() const override { return EType::ComponentProperty; }

		string uiName;
		CryGUID componentInstanceGUID;
		std::function<void(IAnimTrack*)> setDefaultValueCallback;
		std::function<bool(const TMovieSystemValue& value)> setValueCallback;
	};

	std::vector<std::unique_ptr<IPropertyParamInfo>> m_entityProperties;
	std::unordered_map<string, size_t, stl::hash_stricmp<string>, stl::hash_stricmp<string>> m_entityPropertyNameLookupMap;
#ifdef CHECK_FOR_TOO_MANY_ONPROPERTY_SCRIPT_CALLS
	uint32                                m_OnPropertyCalls;
#endif
};
