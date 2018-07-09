// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ATLEntityData.h>
#include <portaudio.h>
#include <atomic>
#include <PoolObject.h>

// Forward declare C struct
struct SNDFILE_tag;
using SNDFILE = struct SNDFILE_tag;

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
class CObject;

class CEvent final : public IEvent, public CPoolObject<CEvent, stl::PSyncNone>
{
public:

	explicit CEvent(CATLEvent& event_);
	virtual ~CEvent() override;

	CEvent(CEvent const&) = delete;
	CEvent(CEvent&&) = delete;
	CEvent& operator=(CEvent const&) = delete;
	CEvent& operator=(CEvent&&) = delete;

	bool    Execute(
	  int const numLoops,
	  double const sampleRate,
	  CryFixedStringT<MaxFilePathLength> const& filePath,
	  PaStreamParameters const& streamParameters);
	void Update();

	// CryAudio::Impl::IEvent
	virtual ERequestStatus Stop() override;
	// ~CryAudio::Impl::IEvent

	SNDFILE*          pSndFile;
	PaStream*         pStream;
	void*             pData;
	CObject*          pObject;
	int               numChannels;
	int               remainingLoops;
	CATLEvent&        event;
	uint32            pathId;
	PaSampleFormat    sampleFormat;
	std::atomic<bool> bDone;
};
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
