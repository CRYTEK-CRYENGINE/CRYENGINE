// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <IAudioImpl.h>
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
using SampleId = uint;
using ListenerId = uint;
using ChannelList = std::vector<int>;

class CTrigger final : public ITrigger
{
public:

	CTrigger() = default;
	CTrigger(CTrigger const&) = delete;
	CTrigger(CTrigger&&) = delete;
	CTrigger& operator=(CTrigger const&) = delete;
	CTrigger& operator=(CTrigger&&) = delete;

	// CryAudio::Impl::ITrigger
	virtual ERequestStatus Load() const override                             { return ERequestStatus::Success; }
	virtual ERequestStatus Unload() const override                           { return ERequestStatus::Success; }
	virtual ERequestStatus LoadAsync(IEvent* const pIEvent) const override   { return ERequestStatus::Success; }
	virtual ERequestStatus UnloadAsync(IEvent* const pIEvent) const override { return ERequestStatus::Success; }
	// ~CryAudio::Impl::ITrigger

	SampleId m_sampleId = 0;
	float    m_attenuationMinDistance = 0.0f;
	float    m_attenuationMaxDistance = 100.0f;
	int      m_volume = 128;
	int      m_numLoops = 1;
	bool     m_bPanningEnabled = true;
	bool     m_bStartEvent = true;
};

struct SParameter final : public IParameter
{
	// Empty implementation so that the engine has something
	// to refer to since parameters are not currently supported by
	// the SDL Mixer implementation
	SParameter() = default;
	SParameter(SParameter const&) = delete;
	SParameter(SParameter&&) = delete;
	SParameter& operator=(SParameter const&) = delete;
	SParameter& operator=(SParameter&&) = delete;
};

struct SSwitchState final : public ISwitchState
{
	// Empty implementation so that the engine has something
	// to refer to since switches are not currently supported by
	// the SDL Mixer implementation
	SSwitchState() = default;
	SSwitchState(SSwitchState const&) = delete;
	SSwitchState(SSwitchState&&) = delete;
	SSwitchState& operator=(SSwitchState const&) = delete;
	SSwitchState& operator=(SSwitchState&&) = delete;
};

struct SEnvironment final : public IEnvironment
{
	// Empty implementation so that the engine has something
	// to refer to since environments are not currently supported by
	// the SDL Mixer implementation
	SEnvironment() = default;
	SEnvironment(SEnvironment const&) = delete;
	SEnvironment(SEnvironment&&) = delete;
	SEnvironment& operator=(SEnvironment const&) = delete;
	SEnvironment& operator=(SEnvironment&&) = delete;
};

class CEvent final : public IEvent, public CPoolObject<CEvent, stl::PSyncNone>
{
public:

	explicit CEvent(CATLEvent& event)
		: m_event(event)
	{}

	CEvent(CEvent const&) = delete;
	CEvent(CEvent&&) = delete;
	CEvent& operator=(CEvent const&) = delete;
	CEvent& operator=(CEvent&&) = delete;

	// CryAudio::Impl::IEvent
	virtual ERequestStatus Stop() override;
	// ~CryAudio::Impl::IEvent

	CATLEvent&      m_event;
	ChannelList     m_channels;
	CTrigger const* m_pTrigger = nullptr;
};

class CStandaloneFile final : public IStandaloneFile
{
public:

	CStandaloneFile(char const* const szName, CATLStandaloneFile& atlStandaloneFile)
		: m_atlFile(atlStandaloneFile)
		, m_name(szName)
	{}

	CStandaloneFile(CStandaloneFile const&) = delete;
	CStandaloneFile(CStandaloneFile&&) = delete;
	CStandaloneFile& operator=(CStandaloneFile const&) = delete;
	CStandaloneFile& operator=(CStandaloneFile&&) = delete;

	CATLStandaloneFile&                m_atlFile;
	SampleId                           m_sampleId = 0; // ID unique to the file, only needed for the 'finished' request
	CryFixedStringT<MaxFilePathLength> m_name;
	ChannelList                        m_channels;
};

using EventInstanceList = std::vector<CEvent*>;
using StandAloneFileInstanceList = std::vector<CStandaloneFile*>;

class CObject final : public IObject, public CPoolObject<CObject, stl::PSyncNone>
{
public:

	explicit CObject(uint32 const id)
		: m_id(id)
		, m_bPositionChanged(false)
	{}

	CObject(CObject const&) = delete;
	CObject(CObject&&) = delete;
	CObject& operator=(CObject const&) = delete;
	CObject& operator=(CObject&&) = delete;

	// CryAudio::Impl::IObject
	virtual ERequestStatus Update() override;
	virtual ERequestStatus Set3DAttributes(SObject3DAttributes const& attributes) override;
	virtual ERequestStatus SetEnvironment(IEnvironment const* const pIEnvironment, float const amount) override;
	virtual ERequestStatus SetParameter(IParameter const* const pIParameter, float const value) override;
	virtual ERequestStatus SetSwitchState(ISwitchState const* const pISwitchState) override;
	virtual ERequestStatus SetObstructionOcclusion(float const obstruction, float const occlusion) override;
	virtual ERequestStatus ExecuteTrigger(ITrigger const* const pITrigger, IEvent* const pIEvent) override;
	virtual ERequestStatus StopAllTriggers() override;
	virtual ERequestStatus PlayFile(IStandaloneFile* const pIStandaloneFile) override;
	virtual ERequestStatus StopFile(IStandaloneFile* const pIStandaloneFile) override;
	virtual ERequestStatus SetName(char const* const szName) override;
	// ~CryAudio::Impl::IObject

	const uint32               m_id;
	CObjectTransformation      m_transformation;
	EventInstanceList          m_events;
	StandAloneFileInstanceList m_standaloneFiles;
	bool                       m_bPositionChanged;
};

class CListener final : public IListener
{
public:

	explicit CListener(const ListenerId id)
		: m_id(id)
	{}

	CListener(CListener const&) = delete;
	CListener(CListener&&) = delete;
	CListener& operator=(CListener const&) = delete;
	CListener& operator=(CListener&&) = delete;

	// CryAudio::Impl::IListener
	virtual ERequestStatus Set3DAttributes(SObject3DAttributes const& attributes) override;
	// ~CryAudio::Impl::IListener

	const ListenerId m_id;
};

struct SFile final : public IFile
{
	SFile() = default;
	SFile(SFile const&) = delete;
	SFile(SFile&&) = delete;
	SFile& operator=(SFile const&) = delete;
	SFile& operator=(SFile&&) = delete;

	SampleId sampleId;
};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
