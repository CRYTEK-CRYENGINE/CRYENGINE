// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IObject.h>
#include <CryMemory/STLPoolAllocator.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <CryMemory/CrySizer.h>

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles audio on the entity.
//////////////////////////////////////////////////////////////////////////
class CEntityComponentAudio final : public IEntityAudioComponent
{
public:
	CRY_ENTITY_COMPONENT_CLASS_GUID(CEntityComponentAudio, IEntityAudioComponent, "CEntityComponentAudio", "51ae5fc2-1b45-4351-ac88-9caf0c757b5f"_cry_guid);

	CEntityComponentAudio();
	virtual ~CEntityComponentAudio() override;

	// IEntityComponent
	virtual void         ProcessEvent(const SEntityEvent& event) override;
	virtual void         Initialize() override;
	virtual uint64       GetEventMask() const override;
	virtual EEntityProxy GetProxyType() const override                    { return ENTITY_PROXY_AUDIO; }
	virtual void         GameSerialize(TSerialize ser) override;
	virtual bool         NeedGameSerialize() override                     { return false; }
	virtual void         GetMemoryUsage(ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }
	virtual void		 OnTransformChanged() override;
	// ~IEntityComponent

	// IEntityAudioComponent
	virtual void                    SetFadeDistance(float const fadeDistance) override                       { m_fadeDistance = fadeDistance; }
	virtual float                   GetFadeDistance() const override                                         { return m_fadeDistance; }
	virtual void                    SetEnvironmentFadeDistance(float const environmentFadeDistance) override { m_environmentFadeDistance = environmentFadeDistance; }
	virtual float                   GetEnvironmentFadeDistance() const override                              { return m_environmentFadeDistance; }
	virtual float                   GetGreatestFadeDistance() const override;
	virtual void                    SetEnvironmentId(CryAudio::EnvironmentId const environmentId) override   { m_environmentId = environmentId; }
	virtual CryAudio::EnvironmentId GetEnvironmentId() const override                                        { return m_environmentId; }
	virtual CryAudio::AuxObjectId   CreateAudioAuxObject() override;
	virtual bool                    RemoveAudioAuxObject(CryAudio::AuxObjectId const audioAuxObjectId) override;
	virtual void                    SetAudioAuxObjectOffset(Matrix34 const& offset, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId) override;
	virtual Matrix34 const&         GetAudioAuxObjectOffset(CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId) override;
	virtual bool                    PlayFile(CryAudio::SPlayFileInfo const& playbackInfo, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId, CryAudio::SRequestUserData const& userData = CryAudio::SRequestUserData::GetEmptyObject()) override;
	virtual void                    StopFile(char const* const szFile, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId) override;
	virtual bool                    ExecuteTrigger(CryAudio::ControlId const audioTriggerId, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId, CryAudio::SRequestUserData const& userData = CryAudio::SRequestUserData::GetEmptyObject()) override;
	virtual void                    StopTrigger(CryAudio::ControlId const audioTriggerId, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId, CryAudio::SRequestUserData const& userData = CryAudio::SRequestUserData::GetEmptyObject()) override;
	virtual void                    SetSwitchState(CryAudio::ControlId const audioSwitchId, CryAudio::SwitchStateId const audioStateId, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId) override;
	virtual void                    SetParameter(CryAudio::ControlId const parameterId, float const value, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId) override;
	virtual void                    SetObstructionCalcType(CryAudio::EOcclusionType const occlusionType, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId) override;
	virtual void                    SetEnvironmentAmount(CryAudio::EnvironmentId const audioEnvironmentId, float const amount, CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId) override;
	virtual void                    SetCurrentEnvironments(CryAudio::AuxObjectId const audioAuxObjectId = CryAudio::DefaultAuxObjectId) override;
	virtual void                    AudioAuxObjectsMoveWithEntity(bool const bCanMoveWithEntity) override;
	virtual void                    AddAsListenerToAudioAuxObject(CryAudio::AuxObjectId const audioAuxObjectId, void (* func)(CryAudio::SRequestInfo const* const), CryAudio::ESystemEvents const eventMask) override;
	virtual void                    RemoveAsListenerFromAudioAuxObject(CryAudio::AuxObjectId const audioAuxObjectId, void (* func)(CryAudio::SRequestInfo const* const)) override;
	virtual CryAudio::AuxObjectId   GetAuxObjectIdFromAudioObject(CryAudio::IObject* pObject) override;
	// ~IEntityAudioComponent

private:

	enum EEntityAudioProxyFlags : CryAudio::EnumFlagsType
	{
		eEntityAudioProxyFlags_None = 0,
		eEntityAudioProxyFlags_CanMoveWithEntity = BIT(0),
	};

	struct SAuxObjectWrapper
	{
		explicit SAuxObjectWrapper(CryAudio::IObject* const _pIObject)
			: pIObject(_pIObject)
			, offset(IDENTITY)
		{}

		~SAuxObjectWrapper() = default;

		CryAudio::IObject* const pIObject;
		Matrix34                 offset;

	private:

		SAuxObjectWrapper()
			: pIObject(nullptr)
			, offset(IDENTITY)
		{}
	};

	using AuxObjectPair = std::pair<CryAudio::AuxObjectId const, SAuxObjectWrapper>;

	typedef std::map<CryAudio::AuxObjectId const, SAuxObjectWrapper, std::less<CryAudio::AuxObjectId>, stl::STLPoolAllocator<AuxObjectPair>> AuxObjects;

	static AuxObjectPair s_nullAuxObjectPair;

	void           OnListenerEnter(CEntity const* const pEntity);
	void           OnListenerMoveNear(Vec3 const& closestPointToArea);
	void           OnListenerMoveInside(Vec3 const& listenerPos);
	void           OnListenerExclusiveMoveInside(CEntity const* const __restrict pEntity, CEntity const* const __restrict pAreaHigh, CEntity const* const __restrict pAreaLow, float const fade);
	void           OnMove();
	AuxObjectPair& GetAudioAuxObjectPair(CryAudio::AuxObjectId const audioAuxObjectId);
	void           SetEnvironmentAmountInternal(CEntity const* const pIEntity, float const amount) const;

	// Function objects
	struct SReleaseAudioProxy
	{
		inline void operator()(AuxObjectPair const& pair)
		{
			gEnv->pAudioSystem->ReleaseObject(pair.second.pIObject);
		}
	};

	struct SRepositionAudioProxy
	{
		SRepositionAudioProxy(Matrix34 const& _transformation, CryAudio::SRequestUserData const& _userData)
			: transformation(_transformation)
			, userData(_userData)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->SetTransformation(transformation * pair.second.offset, userData);
		}

	private:

		Matrix34 const&                   transformation;
		CryAudio::SRequestUserData const& userData;
	};

	struct SPlayFile
	{
		SPlayFile(CryAudio::SPlayFileInfo const& _playbackInfo, CryAudio::SRequestUserData const& _userData)
			: playbackInfo(_playbackInfo)
			, userData(_userData)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->PlayFile(playbackInfo, userData);
		}

	private:

		CryAudio::SPlayFileInfo const&    playbackInfo;
		CryAudio::SRequestUserData const& userData;
	};

	struct SStopFile
	{
		explicit SStopFile(char const* const _szFile)
			: szFile(_szFile)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->StopFile(szFile);
		}

	private:

		char const* const szFile;
	};

	struct SStopTrigger
	{
		SStopTrigger(CryAudio::ControlId const _audioTriggerId, CryAudio::SRequestUserData const& _userData)
			: audioTriggerId(_audioTriggerId)
			, userData(_userData)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->StopTrigger(audioTriggerId, userData);
		}

	private:

		CryAudio::ControlId const         audioTriggerId;
		CryAudio::SRequestUserData const& userData;
	};

	struct SSetSwitchState
	{
		SSetSwitchState(CryAudio::ControlId const _audioSwitchId, CryAudio::SwitchStateId const _audioStateId)
			: audioSwitchId(_audioSwitchId)
			, audioStateId(_audioStateId)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->SetSwitchState(audioSwitchId, audioStateId);
		}

	private:

		CryAudio::ControlId const     audioSwitchId;
		CryAudio::SwitchStateId const audioStateId;
	};

	struct SSetParameter
	{
		SSetParameter(CryAudio::ControlId const _parameterId, float const _value)
			: parameterId(_parameterId)
			, value(_value)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->SetParameter(parameterId, value);
		}

	private:

		CryAudio::ControlId const parameterId;
		float const               value;
	};

	struct SSetOcclusionType
	{
		explicit SSetOcclusionType(CryAudio::EOcclusionType const _occlusionType)
			: occlusionType(_occlusionType)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->SetOcclusionType(occlusionType);
		}

	private:

		CryAudio::EOcclusionType const occlusionType;
	};

	struct SSetEnvironmentAmount
	{
		SSetEnvironmentAmount(CryAudio::EnvironmentId const _audioEnvironmentId, float const _amount)
			: audioEnvironmentId(_audioEnvironmentId)
			, amount(_amount)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->SetEnvironment(audioEnvironmentId, amount);
		}

	private:

		CryAudio::EnvironmentId const audioEnvironmentId;
		float const                   amount;
	};

	struct SSetCurrentEnvironments
	{
		explicit SSetCurrentEnvironments(EntityId const entityId)
			: m_entityId(entityId)
		{}

		inline void operator()(AuxObjectPair const& pair)
		{
			pair.second.pIObject->SetCurrentEnvironments(m_entityId);
		}

	private:

		EntityId const m_entityId;
	};

	struct SSetAuxAudioProxyOffset
	{
		SSetAuxAudioProxyOffset(Matrix34 const& _offset, Matrix34 const& _entityPosition)
			: offset(_offset)
			, entityPosition(_entityPosition)
		{}

		inline void operator()(AuxObjectPair& pair)
		{
			pair.second.offset = offset;
			(SRepositionAudioProxy(entityPosition, CryAudio::SRequestUserData::GetEmptyObject()))(pair);
		}

	private:

		Matrix34 const& offset;
		Matrix34 const& entityPosition;
	};
	// ~Function objects

	AuxObjects              m_mapAuxObjects;
	CryAudio::AuxObjectId   m_auxObjectIdCounter;

	CryAudio::EnvironmentId m_environmentId;
	CryAudio::EnumFlagsType m_flags;

	float                   m_fadeDistance;
	float                   m_environmentFadeDistance;
};
