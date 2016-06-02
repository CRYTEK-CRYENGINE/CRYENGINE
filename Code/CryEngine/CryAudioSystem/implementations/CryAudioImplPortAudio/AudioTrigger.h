// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ATLEntityData.h>
#include <portaudio.h>

enum EPortAudioEventType
{
	ePortAudioEventType_None = 0,
	ePortAudioEventType_Start,
	ePortAudioEventType_Stop,
};

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
class CAudioTrigger final : public IAudioTrigger
{
public:

	CAudioTrigger(
	  uint32 const _pathId,
	  int const _numLoops,
	  double const _sampleRate,
	  EPortAudioEventType const _eventType,
	  CryFixedStringT<512> const& _filePath,
	  PaStreamParameters const& _streamParameters
	  );
	virtual ~CAudioTrigger();

	uint32 const               pathId;
	int const                  numLoops;
	double const               sampleRate;
	EPortAudioEventType const  eventType;
	CryFixedStringT<512> const filePath;
	PaStreamParameters const   streamParameters;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioTrigger);
	PREVENT_OBJECT_COPY(CAudioTrigger);
};
}
}
}
