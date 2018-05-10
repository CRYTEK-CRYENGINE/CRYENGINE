// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>

/**
 * @namespace CryAudio
 * @brief Most parent audio namespace used throughout the entire engine.
 */
namespace CryAudio
{
enum class EEventState : EnumFlagsType
{
	None,
	Playing,
	PlayingDelayed,
	Loading,
	Unloading,
	Virtual,
};

/**
 * @namespace CryAudio::Impl
 * @brief Sub-namespace of the CryAudio namespace used by audio middleware implementations.
 */
namespace Impl
{
/**
 * @struct SObject3DAttributes
 * @brief A struct holding velocity and transformation of audio objects.
 */
struct SObject3DAttributes
{
	SObject3DAttributes()
		: velocity(ZERO)
	{}

	static SObject3DAttributes const& GetEmptyObject() { static SObject3DAttributes const emptyInstance; return emptyInstance; }

	CObjectTransformation transformation;
	Vec3                  velocity;
};
} // namespace Impl
} // namespace CryAudio
