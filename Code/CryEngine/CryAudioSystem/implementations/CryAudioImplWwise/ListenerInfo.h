// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CListener;

struct SListenerInfo final
{
	explicit SListenerInfo(CListener* const pListener_, float const distance_)
		: pListener(pListener_)
		, distance(distance_)
	{}

	CListener* pListener;
	float      distance;
};

using ListenerInfos = std::vector<SListenerInfo>;
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
