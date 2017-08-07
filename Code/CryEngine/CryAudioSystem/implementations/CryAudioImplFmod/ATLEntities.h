// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AudioEvent.h"
#include "AudioObject.h"

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CListener final : public IListener
{
public:

	explicit CListener(int const id)
		: m_id(id)
	{
		ZeroStruct(m_attributes);
	}

	virtual ~CListener() override = default;

	CListener(CListener const&) = delete;
	CListener(CListener&&) = delete;
	CListener&                operator=(CListener const&) = delete;
	CListener&                operator=(CListener&&) = delete;

	ILINE int                 GetId() const     { return m_id; }
	ILINE FMOD_3D_ATTRIBUTES& Get3DAttributes() { return m_attributes; }

	// CryAudio::Impl::IListener
	virtual ERequestStatus Set3DAttributes(SObject3DAttributes const& attributes) override
	{
		FillFmodObjectPosition(attributes, m_attributes);
		FMOD_RESULT const fmodResult = s_pSystem->setListenerAttributes(m_id, &m_attributes);
		ASSERT_FMOD_OK;
		return ERequestStatus::Success;
	}
	// ~CryAudio::Impl::IListener

	static FMOD::Studio::System* s_pSystem;

private:

	int                m_id;
	FMOD_3D_ATTRIBUTES m_attributes;
};

enum class EEventType : EnumFlagsType
{
	None,
	Start,
	Stop,
};

class CTrigger final : public ITrigger
{
public:

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	explicit CTrigger(
	  uint32 const eventPathId,
	  EEventType const eventType,
	  FMOD::Studio::EventDescription* const pEventDescription,
	  FMOD_GUID const guid,
	  char const* const szEventPath)
		: m_eventPathId(eventPathId)
		, m_eventType(eventType)
		, m_pEventDescription(pEventDescription)
		, m_guid(guid)
		, m_eventPath(szEventPath)
	{}
#else
	explicit CTrigger(
	  uint32 const eventPathId,
	  EEventType const eventType,
	  FMOD::Studio::EventDescription* const pEventDescription,
	  FMOD_GUID const guid)
		: m_eventPathId(eventPathId)
		, m_eventType(eventType)
		, m_pEventDescription(pEventDescription)
		, m_guid(guid)
	{}
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE

	virtual ~CTrigger() override = default;

	CTrigger(CTrigger const&) = delete;
	CTrigger(CTrigger&&) = delete;
	CTrigger& operator=(CTrigger const&) = delete;
	CTrigger& operator=(CTrigger&&) = delete;

	// CryAudio::Impl::ITrigger
	virtual ERequestStatus Load()  const override                            { return ERequestStatus::Success; }
	virtual ERequestStatus Unload() const override                           { return ERequestStatus::Success; }
	virtual ERequestStatus LoadAsync(IEvent* const pIEvent) const override   { return ERequestStatus::Success; }
	virtual ERequestStatus UnloadAsync(IEvent* const pIEvent) const override { return ERequestStatus::Success; }
	// ~CryAudio::Impl::ITrigger

	uint32 const                          m_eventPathId;
	EEventType const                      m_eventType;
	FMOD::Studio::EventDescription* const m_pEventDescription;
	FMOD_GUID const                       m_guid;

#if defined(INCLUDE_FMOD_IMPL_PRODUCTION_CODE)
	CryFixedStringT<512> const m_eventPath;
#endif  // INCLUDE_FMOD_IMPL_PRODUCTION_CODE
};

class CParameter final : public IParameter
{
public:

	explicit CParameter(
	  uint32 const eventPathId,
	  float const multiplier,
	  float const shift,
	  char const* const szName)
		: m_eventPathId(eventPathId)
		, m_multiplier(multiplier)
		, m_shift(shift)
		, m_name(szName)
	{}

	virtual ~CParameter() override = default;

	CParameter(CParameter const&) = delete;
	CParameter(CParameter&&) = delete;
	CParameter&                                           operator=(CParameter const&) = delete;
	CParameter&                                           operator=(CParameter&&) = delete;

	uint32                                                GetEventPathId() const     { return m_eventPathId; }
	float                                                 GetValueMultiplier() const { return m_multiplier; }
	float                                                 GetValueShift() const      { return m_shift; }
	CryFixedStringT<CryAudio::MaxObjectNameLength> const& GetName() const            { return m_name; }

private:

	uint32 const m_eventPathId;
	float const  m_multiplier;
	float const  m_shift;
	CryFixedStringT<CryAudio::MaxObjectNameLength> const m_name;
};

class CSwitchState final : public ISwitchState
{
public:

	explicit CSwitchState(
	  uint32 const eventPathId_,
	  float const value_,
	  char const* const szName_)
		: eventPathId(eventPathId_)
		, value(value_)
		, name(szName_)
	{}

	virtual ~CSwitchState() override = default;

	CSwitchState(CSwitchState const&) = delete;
	CSwitchState(CSwitchState&&) = delete;
	CSwitchState& operator=(CSwitchState const&) = delete;
	CSwitchState& operator=(CSwitchState&&) = delete;

	uint32 const eventPathId;
	float const  value;
	CryFixedStringT<CryAudio::MaxObjectNameLength> const name;
};

class CEnvironment final : public IEnvironment
{
public:

	explicit CEnvironment(
	  FMOD::Studio::EventDescription* const pEventDescription_,
	  FMOD::Studio::Bus* const pBus_)
		: pEventDescription(pEventDescription_)
		, pBus(pBus_)
	{}

	virtual ~CEnvironment() override = default;

	CEnvironment(CEnvironment const&) = delete;
	CEnvironment(CEnvironment&&) = delete;
	CEnvironment& operator=(CEnvironment const&) = delete;
	CEnvironment& operator=(CEnvironment&&) = delete;

	FMOD::Studio::EventDescription* const pEventDescription;
	FMOD::Studio::Bus* const              pBus;
};

class CFile final : public IFile
{
public:

	CFile() = default;
	virtual ~CFile() override = default;

	CFile(CFile const&) = delete;
	CFile(CFile&&) = delete;
	CFile& operator=(CFile const&) = delete;
	CFile& operator=(CFile&&) = delete;

	FMOD::Studio::Bank* pBank = nullptr;
};

class CStandaloneFileBase : public IStandaloneFile
{
public:

	explicit CStandaloneFileBase(char const* const szFile, CATLStandaloneFile& atlStandaloneFile);
	virtual ~CStandaloneFileBase() override;

	virtual void StartLoading() = 0;
	virtual bool IsReady() = 0;
	virtual void Play(FMOD_3D_ATTRIBUTES const& attributes) = 0;
	virtual void Set3DAttributes(FMOD_3D_ATTRIBUTES const& attributes) = 0;
	virtual void Stop() = 0;

	void         ReportFileStarted();
	void         ReportFileFinished();

	CATLStandaloneFile&                          m_atlStandaloneFile;
	CryFixedStringT<CryAudio::MaxFilePathLength> m_fileName;
	CObjectBase* m_pObject = nullptr;
	static FMOD::System*                         s_pLowLevelSystem;

};

class CStandaloneFile final : public CStandaloneFileBase
{
public:

	explicit CStandaloneFile(char const* const szFile, CATLStandaloneFile& atlStandaloneFile)
		: CStandaloneFileBase(szFile, atlStandaloneFile)
	{}

	// CStandaloneFileBase
	virtual void StartLoading() override;
	virtual bool IsReady() override;
	virtual void Play(FMOD_3D_ATTRIBUTES const& attributes) override;
	virtual void Set3DAttributes(FMOD_3D_ATTRIBUTES const& attributes) override;
	virtual void Stop() override;
	// ~CStandaloneFileBase

	FMOD::Sound*   m_pLowLevelSound = nullptr;
	FMOD::Channel* m_pChannel = nullptr;

};

class CProgrammerSoundFile final : public CStandaloneFileBase
{
public:

	explicit CProgrammerSoundFile(char const* const szFile, FMOD_GUID const eventGuid, CATLStandaloneFile& atlStandaloneFile)
		: CStandaloneFileBase(szFile, atlStandaloneFile)
		, m_eventGuid(eventGuid)
	{}

	// CStandaloneFileBase
	virtual void StartLoading() override;
	virtual bool IsReady() override;
	virtual void Play(FMOD_3D_ATTRIBUTES const& attributes) override;
	virtual void Set3DAttributes(FMOD_3D_ATTRIBUTES const& attributes) override;
	virtual void Stop() override;
	// ~CStandaloneFileBase

private:

	FMOD_GUID const              m_eventGuid;
	FMOD::Studio::EventInstance* m_pEventInstance = nullptr;

};
class CObjectBase;

using Objects = std::vector<CObjectBase*>;
using Events = std::vector<CEvent*>;
using StandaloneFiles = std::vector<CStandaloneFile*>;
using ParameterToIndexMap = std::map<CParameter const* const, int>;
using SwitchToIndexMap = std::map<CSwitchState const* const, int>;
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
