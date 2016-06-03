// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioTrigger.h"

using namespace CryAudio::Impl::PortAudio;

//////////////////////////////////////////////////////////////////////////
CAudioTrigger::CAudioTrigger(
  uint32 const _pathId,
  int const _numLoops,
  double const _sampleRate,
  EPortAudioEventType const _eventType,
  CryFixedStringT<512> const& _filePath,
  PaStreamParameters const& _streamParameters)
	: pathId(_pathId)
	, numLoops(_numLoops)
	, sampleRate(_sampleRate)
	, eventType(_eventType)
	, filePath(_filePath)
	, streamParameters(_streamParameters)
{
}

//////////////////////////////////////////////////////////////////////////
CAudioTrigger::~CAudioTrigger()
{
}
