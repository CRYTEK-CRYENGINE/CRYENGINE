// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

/**
 * @namespace CryAudio
 * @brief Most parent audio namespace used throughout the entire engine.
 */
namespace CryAudio
{
/**
 * @struct CryAudio::IProfileData
 * @brief A struct exposing public methods to retrieve profile data from the audio system.
 */
struct IProfileData
{
	// <interfuscator:shuffle>
	/** @cond */
	virtual ~IProfileData() = default;
	/** @endcond */

	/**
	 * Retrieves an implementation's name as provided by the implementation itself.
	 * @return char*
	 */
	virtual char const* GetImplName() const = 0;
	// </interfuscator:shuffle>
};
} // namespace CryAudio
