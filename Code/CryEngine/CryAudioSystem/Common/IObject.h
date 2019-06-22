// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>

struct IRenderAuxGeom;

namespace CryAudio
{
enum class EObjectFunctionality : EnumFlagsType
{
	TrackAbsoluteVelocity,
	TrackRelativeVelocity,
};

namespace Impl
{
struct IEnvironmentConnection;
struct IEvent;
struct IParameterConnection;
struct ISwitchStateConnection;
struct ITriggerConnection;
struct IListener;

/**
 * An implementation may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding object (e.g. a middleware-specific unique ID)
 */
struct IObject
{
	/** @cond */
	virtual ~IObject() = default;
	/** @endcond */

	/**
	 * Performs actions that need to be executed regularly on an AudioObject. Called with every tick of the audio thread.
	 * @param deltaTime - accumulated game frame time in seconds.
	 * @return void
	 */
	virtual void Update(float const deltaTime) = 0;

	/**
	 * Set the the object's transformation.
	 * @param transformation - a class containing the object's position and rotation
	 * @return void
	 */
	virtual void SetTransformation(CTransformation const& transformation) = 0;

	/**
	 * Gets the object's transformation.
	 * @return CTransformation of the object.
	 */
	virtual CTransformation const& GetTransformation() const = 0;

	/**
	 * Set the provided occlusion value.
	 * @param pIListener - listener which listens to the occluded sound.
	 * @param occlusion - the occlusion value to be set, it describes how much all sound paths (direct and indirect) are obstructed
	 * @param numRemainingListeners - the number of remaining listeners which will get an occlusion update. Is 1 for the last listener.
	 * @return void
	 */
	virtual void SetOcclusion(IListener* const pIListener, float const occlusion, uint8 const numRemainingListeners) = 0;

	/**
	 * Set the provided occlusion type.
	 * @param occlusionType - the occlusion type to be set
	 * @return void
	 */
	virtual void SetOcclusionType(EOcclusionType const occlusionType) = 0;

	/**
	 * Stop all triggers currently active on the object.
	 * @return void
	 */
	virtual void StopAllTriggers() = 0;

	/**
	 * Sets this object's name.
	 * Is only used during production whenever an entity's name is changed to adjust corresponding objects as well.
	 * @param szName - name to set
	 * @return ERequestStatus - indicates the outcome of underlying process
	 */
	virtual ERequestStatus SetName(char const* const szName) = 0;

	/**
	 * Adds a listener to the audio object.
	 * @param pIListener - listener to add.
	 * @return void
	 */
	virtual void AddListener(IListener* const pIListener) = 0;

	/**
	 * Removes a listener from the audio object.
	 * @param pIListener - listener to remove.
	 * @return void
	 */
	virtual void RemoveListener(IListener* const pIListener) = 0;

	/**
	 * Enables and disables a certain functionality on the object.
	 * @param type - defines the type of functionality.
	 * @param enable - defines if the functionality should be enabled or disabled.
	 * @return void
	 */
	virtual void ToggleFunctionality(EObjectFunctionality const type, bool const enable) = 0;

	//////////////////////////////////////////////////////////////////////////
	// NOTE: The methods below are ONLY USED when INCLUDE_AUDIO_PRODUCTION_CODE is defined!
	//////////////////////////////////////////////////////////////////////////

	/**
	 * Informs the object that it can draw its debug information.
	 * @param[out] auxGeom - a reference to the IRenderAuxGeom that draws the debug info.
	 * @param[in] posX - x-axis position of the auxGeom.
	 * @param[in] posY - y-axis position of the auxGeom.
	 * @param[in] szTextFilter - current set text filter. Is nullptr if filtering is disabled.
	 * @return void
	 */
	virtual void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter) = 0;
};
} // namespace Impl
} // namespace CryAudio
