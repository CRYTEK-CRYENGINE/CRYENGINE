// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "ResourceSelector.h"

namespace Serialization
{

template<class T> ResourceSelector<T> AudioTrigger(T& s)        { return ResourceSelector<T>(s, "AudioTrigger"); }
template<class T> ResourceSelector<T> AudioSwitch(T& s)         { return ResourceSelector<T>(s, "AudioSwitch"); }
template<class T> ResourceSelector<T> AudioSwitchState(T& s)    { return ResourceSelector<T>(s, "AudioSwitchState"); }
template<class T> ResourceSelector<T> AudioRTPC(T& s)           { return ResourceSelector<T>(s, "AudioRTPC"); }
template<class T> ResourceSelector<T> AudioEnvironment(T& s)    { return ResourceSelector<T>(s, "AudioEnvironment"); }
template<class T> ResourceSelector<T> AudioPreloadRequest(T& s) { return ResourceSelector<T>(s, "AudioPreloadRequest"); }

};
