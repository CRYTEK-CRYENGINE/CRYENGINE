// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ITriggerConnection.h>
#include <PoolObject.h>
#include <portaudio.h>

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
enum class EEventFlags : EnumFlagsType
{
	None           = 0,
	IsLocalized    = BIT(0),
	ToBeDestructed = BIT(1),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EEventFlags);

class CEvent final : public ITriggerConnection, public CPoolObject<CEvent, stl::PSyncNone>
{
public:

	enum class EActionType : EnumFlagsType
	{
		None,
		Start,
		Stop,
	};

	CEvent() = delete;
	CEvent(CEvent const&) = delete;
	CEvent(CEvent&&) = delete;
	CEvent& operator=(CEvent const&) = delete;
	CEvent& operator=(CEvent&&) = delete;

	explicit CEvent(
		EEventFlags const flags,
		EActionType const actionType,
		uint32 const pathId,
		int const numLoops,
		double const sampleRate,
		PaStreamParameters const& streamParameters,
		char const* const szFilePath,
		char const* const szFolder,
		char const* const szName)
		: m_flags(flags)
		, m_actionType(actionType)
		, m_pathId(pathId)
		, m_numLoops(numLoops)
		, m_sampleRate(sampleRate)
		, m_numInstances(0)
		, m_streamParameters(streamParameters)
		, m_filePath(szFilePath)
		, m_folder(szFolder)
		, m_name(szName)
	{}

	virtual ~CEvent() override = default;

	// CryAudio::Impl::ITriggerConnection
	virtual ETriggerResult Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId) override;
	virtual void           Stop(IObject* const pIObject) override;
	// ~CryAudio::Impl::ITriggerConnection

	EEventFlags GetFlags() const                                      { return m_flags; }
	void        SetFlag(EEventFlags const flag)                       { m_flags |= flag; }

	void        SetSampleRate(double const sampleRate)                { m_sampleRate = sampleRate; }
	void        SetStreamParameters(PaStreamParameters const& params) { m_streamParameters = params; }
	void        SetFilePath(char const* const szFilePath)             { m_filePath = szFilePath; }

	uint32      GetPathId() const                                     { return m_pathId; }
	char const* GetFolder() const                                     { return m_folder.c_str(); }
	char const* GetName() const                                       { return m_name.c_str(); }

	bool        CanBeDestructed() const                               { return ((m_flags& EEventFlags::ToBeDestructed) != 0) && (m_numInstances == 0); }

	void        IncrementNumInstances()                               { ++m_numInstances; }
	void        DecrementNumInstances();

private:

	EEventFlags                                 m_flags;
	EActionType const                           m_actionType;
	uint32 const                                m_pathId;
	int const                                   m_numLoops;
	double                                      m_sampleRate;
	uint16                                      m_numInstances;
	PaStreamParameters                          m_streamParameters;
	CryFixedStringT<MaxFilePathLength>          m_filePath;
	CryFixedStringT<MaxFilePathLength> const    m_folder;
	CryFixedStringT<MaxControlNameLength> const m_name;
};
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
