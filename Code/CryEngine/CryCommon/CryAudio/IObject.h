// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IAudioInterfacesCommonData.h"
#include <CryEntitySystem/IEntityBasicTypes.h>

#include "IAudioInterfacesCommonData.h"

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
	 * @param entityId - ID of the entity that will receive the started/stopped callback depending on what it registered to.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see StopTrigger
	 */
	virtual void ExecuteTrigger(ControlId const triggerId, EntityId const entityId = INVALID_ENTITYID, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

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
	virtual void SetTransformation(CTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

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
	 * Sets the occlusion type to be used by this audio object.
	 * @param occlusionType - occlusion type to apply.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets the occlusion ray offset to be used by this audio object.
	 * @param offset - occlusion ray offset to apply.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetOcclusionRayOffset(float const offset, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Sets this audio object's name.
	 * Is only used during production whenever an entity's name is changed to adjust corresponding audio objects as well.
	 * @param szName - name to set.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Adds a listener with the given id to the audio object.
	 * @param id - id of the listener to add.
	 * @return void
	 */
	virtual void AddListener(ListenerId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Removes a listener with the given id from the audio object.
	 * @param id - id of the listener to remove.
	 * @return void
	 */
	virtual void RemoveListener(ListenerId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Toggles whether this audio object should track and update its absolute velocity.
	 * @param enable - if true enables absolute velocity tracking otherwise disables it.
	 * @return void
	 */
	virtual void ToggleAbsoluteVelocityTracking(bool const enable, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Toggles whether this audio object should track and update its relative velocity (against the listener).
	 * @param enable - if true enables relative velocity tracking otherwise disables it.
	 * @return void
	 */
	virtual void ToggleRelativeVelocityTracking(bool const enable, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;
	// </interfuscator:shuffle>
};
} // namespace CryAudio
