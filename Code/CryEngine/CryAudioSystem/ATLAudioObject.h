// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"
#include <CryPhysics/IPhysics.h>
#include <CrySystem/TimeValue.h>

struct IRenderAuxGeom;

enum EAudioTriggerStatus : AudioEnumFlagsType
{
	eAudioTriggerStatus_None                  = 0,
	eAudioTriggerStatus_Playing               = BIT(0),
	eAudioTriggerStatus_Prepared              = BIT(1),
	eAudioTriggerStatus_Loading               = BIT(2),
	eAudioTriggerStatus_Unloading             = BIT(3),
	eAudioTriggerStatus_Starting              = BIT(4),
	eAudioTriggerStatus_WaitingForRemoval     = BIT(5),
	eAudioTriggerStatus_CallbackOnAudioThread = BIT(6),
};

struct SAudioTriggerImplState
{
	SAudioTriggerImplState()
		: flags(eAudioTriggerStatus_None)
	{}

	AudioEnumFlagsType flags;
};

struct SUserDataBase
{
	SUserDataBase()
		: pOwnerOverride(nullptr)
		, pUserData(nullptr)
		, pUserDataOwner(nullptr)
	{}

	explicit SUserDataBase(
	  void* const _pOwnerOverride,
	  void* const _pUserData,
	  void* const _pUserDataOwner)
		: pOwnerOverride(_pOwnerOverride)
		, pUserData(_pUserData)
		, pUserDataOwner(_pUserDataOwner)
	{}

	void* pOwnerOverride;
	void* pUserData;
	void* pUserDataOwner;
};

struct SAudioTriggerInstanceState : public SUserDataBase
{
	SAudioTriggerInstanceState()
		: flags(eAudioTriggerStatus_None)
		, audioTriggerId(INVALID_AUDIO_CONTROL_ID)
		, numPlayingEvents(0)
		, numLoadingEvents(0)
		, expirationTimeMS(0.0f)
		, remainingTimeMS(0.0f)
	{}

	AudioEnumFlagsType flags;
	AudioControlId     audioTriggerId;
	size_t             numPlayingEvents;
	size_t             numLoadingEvents;
	float              expirationTimeMS;
	float              remainingTimeMS;
};

struct SAudioStandaloneFileData : public SUserDataBase
{
	SAudioStandaloneFileData()
		: SUserDataBase(nullptr, nullptr, nullptr)
		, fileId(INVALID_AUDIO_STANDALONE_FILE_ID)
	{}

	explicit SAudioStandaloneFileData(
	  void* const _pOwner,
	  void* const _pUserData,
	  void* const _pUserDataOwner,
	  AudioStandaloneFileId const _fileId)
		: SUserDataBase(_pOwner, _pUserData, _pUserDataOwner)
		, fileId(_fileId)
	{}

	AudioStandaloneFileId const fileId;
};

// CATLAudioObject-related typedefs
typedef std::map<AudioStandaloneFileId, SAudioStandaloneFileData, std::less<AudioStandaloneFileId>,
                 STLSoundAllocator<std::pair<AudioStandaloneFileId, SAudioStandaloneFileData>>> ObjectStandaloneFileMap;

typedef std::set<CATLEvent*, std::less<CATLEvent*>, STLSoundAllocator<CATLEvent*>> ObjectEventSet;

typedef std::map<AudioTriggerImplId, SAudioTriggerImplState, std::less<AudioTriggerImplId>,
                 STLSoundAllocator<std::pair<AudioTriggerImplId, SAudioTriggerImplState>>> ObjectTriggerImplStates;

typedef std::map<AudioTriggerInstanceId, SAudioTriggerInstanceState, std::less<AudioTriggerInstanceId>,
                 STLSoundAllocator<std::pair<AudioTriggerInstanceId, SAudioTriggerInstanceState>>> ObjectTriggerStates;

typedef std::map<AudioControlId, AudioSwitchStateId, std::less<AudioControlId>,
                 STLSoundAllocator<std::pair<AudioControlId, AudioSwitchStateId>>> ObjectStateMap;

typedef std::map<AudioControlId, float, std::less<AudioControlId>,
                 STLSoundAllocator<std::pair<AudioControlId, float>>> ObjectRtpcMap;

typedef std::map<AudioEnvironmentId, float, std::less<AudioEnvironmentId>,
                 STLSoundAllocator<std::pair<AudioEnvironmentId, float>>> ObjectEnvironmentMap;

#define AUDIO_MAX_RAY_HITS static_cast<size_t>(5)

class CATLAudioObject final : public TATLObject
{
public:

	class CPropagationProcessor
	{
	public:

		static bool s_bCanIssueRWIs;

		struct SRayInfo
		{
			static float const s_fSmoothingAlpha;

			SRayInfo(size_t const _rayId, AudioObjectId const _audioObjectId)
				: rayId(_rayId)
				, audioObjectId(_audioObjectId)
				, totalSoundOcclusion(0.0f)
				, numHits(0)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
				, startPosition(ZERO)
				, direction(ZERO)
				, randomOffset(ZERO)
				, averageHits(0.0f)
				, distanceToFirstObstacle(FLT_MAX)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
			{}

			~SRayInfo() {}

			void Reset();

			size_t const        rayId;
			AudioObjectId const audioObjectId;
			float               totalSoundOcclusion;
			int                 numHits;
			ray_hit             hits[AUDIO_MAX_RAY_HITS];

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			Vec3  startPosition;
			Vec3  direction;
			Vec3  randomOffset;
			float averageHits;
			float distanceToFirstObstacle;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
		};

		typedef std::vector<SRayInfo, STLSoundAllocator<SRayInfo>> RayInfoVec;

	public:

		CPropagationProcessor(
		  AudioObjectId const _audioObjectId,
		  CAudioObjectTransformation const& _position);

		~CPropagationProcessor();

		// PhysicsSystem callback
		static int    OnObstructionTest(EventPhys const* pEvent);
		static void   ProcessObstructionRay(int const numHits, SRayInfo* const pRayInfo, bool const bReset = false);
		static size_t NumRaysFromCalcType(EAudioOcclusionType const occlusionType);

		void          Update(float const deltaTime);
		void          SetObstructionOcclusionCalcType(EAudioOcclusionType const occlusionType);
		bool          CanRunObstructionOcclusion() const { return s_bCanIssueRWIs && m_occlusionType != eAudioOcclusionType_Ignore; }
		void          GetPropagationData(SATLSoundPropagationData& propagationData) const;
		void          RunObstructionQuery(CAudioObjectTransformation const& listenerPosition, bool const bSyncCall, bool const bReset = false);
		void          ReportRayProcessed(size_t const rayId);
		void          ReleasePendingRays();
		bool          HasPendingRays() const { return m_remainingRays > 0; }

	private:

		void ProcessObstructionOcclusion(bool const bReset = false);
		void CastObstructionRay(
		  Vec3 const& origin,
		  Vec3 const& randomOffset,
		  Vec3 const& direction,
		  size_t const rayIndex,
		  bool const bSyncCall,
		  bool const bReset = false);

		size_t                            m_remainingRays;
		size_t                            m_totalRays;

		CSmoothFloat                      m_obstructionValue;
		CSmoothFloat                      m_occlusionValue;
		CAudioObjectTransformation const& m_position;

		float                             m_currentListenerDistance;
		RayInfoVec                        m_rayInfos;
		EAudioOcclusionType               m_occlusionType;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	public:

		static size_t s_totalSyncPhysRays;
		static size_t s_totalAsyncPhysRays;

		void   DrawObstructionRays(IRenderAuxGeom& auxGeom) const;
		size_t GetNumRays() const { return NumRaysFromCalcType(m_occlusionType); }
		void   ResetRayData();

	private:

		struct SRayDebugInfo
		{
			SRayDebugInfo()
				: begin(ZERO)
				, end(ZERO)
				, stableEnd(ZERO)
				, occlusionValue(0.0f)
				, distanceToNearestObstacle(FLT_MAX)
				, numHits(0)
				, averageHits(0.0f)
			{}

			~SRayDebugInfo() {}

			Vec3  begin;
			Vec3  end;
			Vec3  stableEnd;
			float occlusionValue;
			float distanceToNearestObstacle;
			int   numHits;
			float averageHits;
		};

		typedef std::vector<SRayDebugInfo, STLSoundAllocator<SRayDebugInfo>> RayDebugInfoVec;

	private:

		RayDebugInfoVec m_rayDebugInfos;
		mutable float   m_timeSinceLastUpdateMS;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	};

	explicit CATLAudioObject(AudioObjectId const audioObjectId, CryAudio::Impl::IAudioObject* const pImplData = nullptr, EAudioDataScope const dataScope = eAudioDataScope_None)
		: TATLObject(audioObjectId, dataScope)
		, m_pImplData(pImplData)
		, m_flags(eAudioObjectFlags_None)
		, m_maxRadius(0.0f)
		, m_previousVelocity(0.0f)
		, m_propagationProcessor(audioObjectId, m_attributes.transformation) {}

	~CATLAudioObject() {}

	void Release();

	void ReportStartingTriggerInstance(AudioTriggerInstanceId const audioTriggerInstanceId, AudioControlId const audioTriggerId);
	void ReportStartedTriggerInstance(AudioTriggerInstanceId const audioTriggerInstanceId, void* const pOwnerOverride, void* const pUserData, void* const pUserDataOwner, AudioEnumFlagsType const flags);

	void ReportStartedEvent(CATLEvent* const _pEvent);
	void ReportFinishedEvent(CATLEvent* const _pEvent, bool const _bSuccess);

	void GetStartedStandaloneFileRequestData(CATLStandaloneFile const* const _pStandaloneFile, CAudioRequestInternal& _request);
	void ReportStartedStandaloneFile(
	  CATLStandaloneFile const* const _pStandaloneFile,
	  void* const _pOwner,
	  void* const _pUserData,
	  void* const _pUserDataOwner);

	void                                            ReportFinishedStandaloneFile(CATLStandaloneFile const* const _pStandaloneFile);

	void                                            ReportPrepUnprepTriggerImpl(AudioTriggerImplId const audioTriggerImplId, bool const bPrepared);

	void                                            SetSwitchState(AudioControlId const audioSwitchId, AudioSwitchStateId const audioSwitchStateId);
	void                                            SetRtpc(AudioControlId const audioRtpcId, float const value);
	void                                            SetEnvironmentAmount(AudioEnvironmentId const audioEnvironmentId, float const amount);

	ObjectTriggerImplStates const&                  GetTriggerImpls() const;
	ObjectRtpcMap const&                            GetRtpcs() const        { return m_rtpcs; }
	ObjectEnvironmentMap const&                     GetEnvironments() const;
	ObjectStateMap const&                           GetSwitchStates() const { return m_switchStates; }
	void                                            ClearEnvironments();

	ObjectEventSet const&                           GetActiveEvents() const                                       { return m_activeEvents; }
	ObjectStandaloneFileMap const&                  GetActiveStandaloneFiles() const                              { return m_activeStandaloneFiles; }
	ObjectTriggerStates&                            GetTriggerStates()                                            { return m_triggerStates; }

	void                                            SetImplDataPtr(CryAudio::Impl::IAudioObject* const pImplData) { m_pImplData = pImplData; }
	CryAudio::Impl::IAudioObject*                   GetImplDataPtr() const                                        { return m_pImplData; }

	float                                           GetMaxRadius() const                                          { return m_maxRadius; }

	void                                            Update(float const deltaTime, CryAudio::Impl::SAudioObject3DAttributes const& listenerAttributes);
	void                                            Clear();
	void                                            ReportPhysicsRayProcessed(size_t const rayId);
	CryAudio::Impl::SAudioObject3DAttributes const& Get3DAttributes() const { return m_attributes; }
	void                                            SetTransformation(CAudioObjectTransformation const& transformation);
	void                                            SetObstructionOcclusionCalc(EAudioOcclusionType const calcType);
	void                                            ResetObstructionOcclusion(CAudioObjectTransformation const& listenerTransformation);
	bool                                            CanRunObstructionOcclusion() const { return m_propagationProcessor.CanRunObstructionOcclusion(); }
	void                                            GetPropagationData(SATLSoundPropagationData& propagationData);
	void                                            ReleasePendingRays();
	void                                            SetDopplerTracking(bool const bEnable);
	void                                            SetVelocityTracking(bool const bEnable);
	void                                            UpdateControls(float const deltaTime, CryAudio::Impl::SAudioObject3DAttributes const& listenerAttributes);
	bool                                            CanBeReleased() const;
	AudioEnumFlagsType                              GetFlags() const { return m_flags; }
	void                                            SetFlag(EAudioObjectFlags const flag);
	void                                            RemoveFlag(EAudioObjectFlags const flag);

private:

	void ReportFinishedTriggerInstance(ObjectTriggerStates::iterator& iter);

	ObjectStandaloneFileMap                  m_activeStandaloneFiles;
	ObjectEventSet                           m_activeEvents;
	ObjectTriggerStates                      m_triggerStates;
	ObjectTriggerImplStates                  m_triggerImplStates;
	ObjectRtpcMap                            m_rtpcs;
	ObjectEnvironmentMap                     m_environments;
	ObjectStateMap                           m_switchStates;
	CryAudio::Impl::IAudioObject*            m_pImplData;
	float                                    m_maxRadius;
	AudioEnumFlagsType                       m_flags;
	float                                    m_previousVelocity;
	Vec3                                     m_velocity;
	CryAudio::Impl::SAudioObject3DAttributes m_attributes;
	CryAudio::Impl::SAudioObject3DAttributes m_previousAttributes;
	CTimeValue                               m_previousTime;
	CPropagationProcessor                    m_propagationProcessor;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void CheckBeforeRemoval(CATLDebugNameStore const* const pDebugNameStore) const;
	void DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, CATLDebugNameStore const* const pDebugNameStore) const;
	void ResetObstructionRays() { m_propagationProcessor.ResetRayData(); }

private:

	class CStateDebugDrawData
	{
	public:

		explicit CStateDebugDrawData(AudioSwitchStateId const audioSwitchState = INVALID_AUDIO_SWITCH_STATE_ID);
		~CStateDebugDrawData();

		void Update(AudioSwitchStateId const newState);

		AudioSwitchStateId m_currentState;
		float              m_currentAlpha;

	private:

		static float const s_maxAlpha;
		static float const s_minAlpha;
		static int const   s_maxToMinUpdates;
	};

	typedef std::map<AudioControlId, CStateDebugDrawData, std::less<AudioControlId>,
	                 STLSoundAllocator<std::pair<AudioControlId, CStateDebugDrawData>>> StateDrawInfoMap;

	CryFixedStringT<MAX_AUDIO_CONTROL_NAME_LENGTH> GetTriggerNames(char const* const _szSeparator, CATLDebugNameStore const* const _pDebugNameStore) const;
	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH>  GetEventIDs(char const* const _szSeparator) const;
	CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH>  GetStandaloneFileIDs(char const* const _szSeparator, CATLDebugNameStore const* const _pDebugNameStore) const;

	mutable StateDrawInfoMap m_stateDrawInfoMap;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	DELETE_DEFAULT_CONSTRUCTOR(CATLAudioObject);
	PREVENT_OBJECT_COPY(CATLAudioObject);
};
