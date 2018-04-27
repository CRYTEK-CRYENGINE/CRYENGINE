// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

/**
 * @namespace CryAudio
 * @brief Most parent audio namespace used throughout the entire engine.
 */
namespace CryAudio
{
/**
 * @struct CryAudio::IObject
 * @brief A struct exposing public methods to interact with audio objects.
 */
struct IObject
{
	// <interfuscator:shuffle>
	/** @cond */
	virtual ~IObject() = default;
	/** @endcond */

	/**
	 * Executes the passed trigger ID on this audio object.
	 * @param triggerId - ID of the trigger to execute.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see StopTrigger
	 */
	virtual void ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Stops all instances of the passed trigger ID or all instances of all active triggers if CryAudio::InvalidControlId (default) is passed on this audio object.
	 * @param triggerId - ID of the trigger to stop.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see ExecuteTrigger
	 */
	virtual void StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets this audio object's transformation.
	 * @param transformation - const reference to the object holding the transformation to apply.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetTransformation(CObjectTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets a parameter to a given value on this audio object.
	 * @param parameterId - ID of the parameter in question.
	 * @param value - floating point value to which the parameter should be set.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets a switch to a given state on this audio object.
	 * @param audioSwitchId - ID of the switch in question.
	 * @param audioSwitchStateId - ID of the switch's state in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetSwitchState(ControlId const audioSwitchId, SwitchStateId const audioSwitchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets an environment to a given value on this audio object.
	 * @param audioEnvironmentId - ID of the environment in question.
	 * @param amount - floating point value indicating the amount of the environment to apply.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetEnvironment(EnvironmentId const audioEnvironmentId, float const amount, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets all of the environments in which this audio object is located to this audio object.
	 * @param entityToIgnore - ID of the environment providing entity to ignore when determining environments and their values.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetCurrentEnvironments(EntityId const entityToIgnore = INVALID_ENTITYID, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Removes all of the environments currently set to this audio object from this audio object.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ResetEnvironments(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets the occlusion type to be used by this audio object.
	 * @param occlusionType - occlusion type to apply.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Plays the given file on this audio object.
	 * @param playFileInfo - reference to a struct that holds data necessary for playback.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see StopFile
	 */
	virtual void PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Stops the given file on this audio object.
	 * @param szFile - name of the file in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see PlayFile
	 */
	virtual void StopFile(char const* const szFile, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets this audio object's name.
	 * Is only used during production whenever an entity's name is changed to adjust corresponding audio objects as well.
	 * @param szName - name to set.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	* Gets the entityId linked with this object (or INVALID_ENTITYID if not linked to an entity)
	* @return EntityId
	*/
	virtual EntityId GetEntityId() const = 0;
	// </interfuscator:shuffle>
};
} // namespace CryAudio
