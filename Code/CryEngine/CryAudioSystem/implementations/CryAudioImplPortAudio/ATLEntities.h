// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ATLEntityData.h>

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
class CAudioListener final : public IAudioListener
{
public:

	CAudioListener() {}
	virtual ~CAudioListener() {}

	DELETE_DEFAULT_CONSTRUCTOR(CAudioListener);
	PREVENT_OBJECT_COPY(CAudioListener);
};

class CAudioParameter final : public IAudioRtpc
{
public:

	CAudioParameter() {}
	virtual ~CAudioParameter() {}

	DELETE_DEFAULT_CONSTRUCTOR(CAudioParameter);
	PREVENT_OBJECT_COPY(CAudioParameter);
};

class CAudioSwitchState final : public IAudioSwitchState
{
public:

	CAudioSwitchState() {}
	virtual ~CAudioSwitchState() {}

	DELETE_DEFAULT_CONSTRUCTOR(CAudioSwitchState);
	PREVENT_OBJECT_COPY(CAudioSwitchState);
};

class CAudioEnvironment final : public IAudioEnvironment
{
public:

	CAudioEnvironment() {}
	virtual ~CAudioEnvironment() {}

	DELETE_DEFAULT_CONSTRUCTOR(CAudioEnvironment);
	PREVENT_OBJECT_COPY(CAudioEnvironment);
};

class CAudioFileEntry final : public IAudioFileEntry
{
public:

	CAudioFileEntry() {}
	virtual ~CAudioFileEntry() {}

	PREVENT_OBJECT_COPY(CAudioFileEntry);
};

class CAudioStandaloneFile final : public IAudioStandaloneFile
{
};
}
}
}
