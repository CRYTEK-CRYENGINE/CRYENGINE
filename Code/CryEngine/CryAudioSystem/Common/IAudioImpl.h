// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SharedAudioData.h"
#include "ATLEntityData.h"
#include <CrySystem/XML/IXml.h>

typedef uint8 TAudioGamepadUniqueID;

/**
 * An enum that lists possible statuses of an AudioRequest.
 * Used as a return type for many function used by the AudioSystem internally,
 * and also for most of the IAudioImpl calls
 */
enum EAudioRequestStatus : AudioEnumFlagsType
{
	eAudioRequestStatus_None                    = 0,
	eAudioRequestStatus_Success                 = 1,
	eAudioRequestStatus_PartialSuccess          = 2,
	eAudioRequestStatus_Failure                 = 3,
	eAudioRequestStatus_Pending                 = 4,
	eAudioRequestStatus_FailureInvalidObjectId  = 5,
	eAudioRequestStatus_FailureInvalidControlId = 6,
	eAudioRequestStatus_FailureInvalidRequest   = 7,
};

enum EAudioEventState : AudioEnumFlagsType
{
	eAudioEventState_None           = 0,
	eAudioEventState_Playing        = 1,
	eAudioEventState_PlayingDelayed = 2,
	eAudioEventState_Loading        = 3,
	eAudioEventState_Unloading      = 4,
	eAudioEventState_Virtual        = 5,
};

/**
 * A utility function to convert a boolean value to an EAudioRequestStatus
 * @param bResult - a boolean value
 * @return eARS_SUCCESS if bResult is true, eARS_FAILURE if bResult is false
 */
inline EAudioRequestStatus BoolToARS(bool bResult)
{
	return bResult ? eAudioRequestStatus_Success : eAudioRequestStatus_Failure;
}

namespace CryAudio
{
namespace Impl
{
/**
 * The CryEngine AudioTranslationLayer uses this interface to communicate with an audio middleware
 */
struct IAudioImpl
{
	//DOC-IGNORE-BEGIN
	virtual ~IAudioImpl() {}
	//DOC-IGNORE-END

	/**
	 * Update all of the internal components that require regular updates, update the audio middleware state.
	 * @param deltaTime - Time since the last call to Update in milliseconds.
	 * @return void
	 */
	virtual void Update(float const deltaTime) = 0;

	/**
	 * Initialize all internal components and the audio middleware.
	 * @return eARS_SUCCESS if the initialization was successful, eARS_FAILURE otherwise.
	 * @see ShutDown
	 */
	virtual EAudioRequestStatus Init() = 0;

	/**
	 * Shuts down all of the internal components and the audio middleware.
	 * Note: After a call to ShutDown(), the system can still be reinitialized by calling Init().
	 * @return eARS_SUCCESS if the shutdown was successful, eARS_FAILURE otherwise.
	 * @see Release, Init
	 */
	virtual EAudioRequestStatus ShutDown() = 0;

	/**
	 * Frees all of the resources used by the class and destroys the instance. This action is not reversible.
	 * @return eARS_SUCCESS if the action was successful, eARS_FAILURE otherwise.
	 * @see ShutDown, Init
	 */
	virtual EAudioRequestStatus Release() = 0;

	/**
	 * Perform a "hot restart" of the audio middleware. Reset all of the internal data.
	 * @see Release, Init
	 */
	virtual void OnAudioSystemRefresh() = 0;

	/**
	 * This method is called every time the main Game (or Editor) window loses focus.
	 * @return eARS_SUCCESS if the action was successful, eARS_FAILURE otherwise.
	 * @see OnGetFocus
	 */
	virtual EAudioRequestStatus OnLoseFocus() = 0;

	/**
	 * This method is called every time the main Game (or Editor) window gets focus.
	 * @return eARS_SUCCESS if the action was successful, eARS_FAILURE otherwise.
	 * @see OnLoseFocus
	 */
	virtual EAudioRequestStatus OnGetFocus() = 0;

	/**
	 * Mute all sounds, after this call there should be no audio coming from the audio middleware.
	 * @return eARS_SUCCESS if the action was successful, eARS_FAILURE otherwise.
	 * @see UnmuteAll, StopAllSounds
	 */
	virtual EAudioRequestStatus MuteAll() = 0;

	/**
	 * Restore the audio output of the audio middleware after a call to MuteAll().
	 * @return eARS_SUCCESS if the action was successful, eARS_FAILURE otherwise.
	 * @see MuteAll
	 */
	virtual EAudioRequestStatus UnmuteAll() = 0;

	/**
	 * Stop all currently playing sounds. Has no effect on anything triggered after this method is called.
	 * @return eARS_SUCCESS if the action was successful, eARS_FAILURE otherwise.
	 * @see MuteAll
	 */
	virtual EAudioRequestStatus StopAllSounds() = 0;

	/**
	 * Register an audio object with the audio middleware. An object needs to be registered for one to set position, execute triggers on it,
	 * or set Rtpcs and switches. This version of the method is meant be used in debug builds.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject being registered
	 * @param szAudioObjectName - the AudioObject name shown in debug info
	 * @return eARS_SUCCESS if the object has been registered, eARS_FAILURE if the registration failed
	 * @see UnregisterAudioObject, ResetAudioObject
	 */
	virtual EAudioRequestStatus RegisterAudioObject(IAudioObject* const pAudioObject, char const* const szAudioObjectName) = 0;

	/**
	 * Register an audio object with the audio middleware. An object needs to be registered for one to set position, execute triggers on it,
	 * or set Rtpcs and switches. This version of the method is meant to be used in the release builds.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject being registered
	 * @return eARS_SUCCESS if the object has been registered, eARS_FAILURE if the registration failed
	 * @see UnregisterAudioObject, ResetAudioObject
	 */
	virtual EAudioRequestStatus RegisterAudioObject(IAudioObject* const pAudioObject) = 0;

	/**
	 * Unregister an audio object with the audio middleware. After this action executing triggers, setting position, states or rtpcs
	 * should have no effect on the object referenced by pAudioObject.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject being unregistered
	 * @return eARS_SUCCESS if the object has been unregistered, eARS_FAILURE if the un-registration failed
	 * @see RegisterAudioObject, ResetAudioObject
	 */
	virtual EAudioRequestStatus UnregisterAudioObject(IAudioObject* const pAudioObject) = 0;

	/**
	 * Clear out the object data and resets it so that the object can be returned to the pool of available Audio Object
	 * for reuse.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject being reset
	 * @return eARS_SUCCESS if the object has been reset, eARS_FAILURE otherwise
	 * @see RegisterAudioObject, UnregisterAudioObject
	 */
	virtual EAudioRequestStatus ResetAudioObject(IAudioObject* const pAudioObject) = 0;

	/**
	 * Performs actions that need to be executed regularly on an AudioObject.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject being updated.
	 * @return eARS_SUCCESS if the object has been successfully updated, eARS_FAILURE otherwise.
	 */
	virtual EAudioRequestStatus UpdateAudioObject(IAudioObject* const pAudioObject) = 0;

	/**
	 * Allows for playing a stand alone audio file globally or on the specified audio object.
	 * @param _pAudioStandaloneFileInfo - struct that contains audio standalone file info for playback.
	 * @return eARS_SUCCESS if the file started playback, eARS_FAILURE otherwise.
	 * @see StopFile
	 */
	virtual EAudioRequestStatus PlayFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo) = 0;

	/**
	 * Allows for stopping a stand alone audio file globally or on the specified audio object.
	 * @param _pAudioStandaloneFileInfo - struct that contains audio standalone file info for stopping.
	 * @return eARS_SUCCESS if the file stopped, is stopping or is not playing anymore, eARS_FAILURE otherwise.
	 * @see PlayFile
	 */
	virtual EAudioRequestStatus StopFile(SAudioStandaloneFileInfo* const _pAudioStandaloneFileInfo) = 0;

	/**
	 * Load the metadata and media needed by the audio middleware to execute the ATLTriggerImpl described by pAudioTrigger.
	 * Preparing Triggers manually is only necessary if their data have not been loaded via PreloadRequests.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the ATLTriggerImpl is prepared
	 * @param pAudioTrigger     - implementation-specific data for the ATLTriggerImpl being prepared
	 * @return eARS_SUCCESS if the the data was successfully loaded, eARS_FAILURE otherwise
	 * @see UnprepareTriggerSync, PrepareTriggerAsync, UnprepareTriggerAsync
	 */
	virtual EAudioRequestStatus PrepareTriggerSync(
	  IAudioObject* const pAudioObject,
	  IAudioTrigger const* const pAudioTrigger) = 0;

	/**
	 * Release the metadata and media needed by the audio middleware to execute the ATLTriggerImpl described by pAudioTrigger
	 * Un-preparing Triggers manually is only necessary if their data are not managed via PreloadRequests.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 * @param pAudioTrigger     - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to handle the ATLTriggerImpl being unprepared
	 * @return eARS_SUCCESS if the the data was successfully unloaded, eARS_FAILURE otherwise
	 * @see PrepareTriggerSync, PrepareTriggerAsync, UnprepareTriggerAsync
	 */
	virtual EAudioRequestStatus UnprepareTriggerSync(
	  IAudioObject* const pAudioObject,
	  IAudioTrigger const* const pAudioTrigger) = 0;

	/**
	 * Load the metadata and media needed by the audio middleware to execute the IAudioTrigger described by pAudioTrigger
	 * asynchronously. An active event that references pAudioEvent is created on the corresponding AudioObject.
	 * The callback called once the loading is done must report that this event is finished.
	 * Preparing Triggers manually is only necessary if their data have not been loaded via PreloadRequests.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the ATLTriggerImpl is prepared
	 * @param pAudioTrigger     - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to handle the ATLTriggerImpl being prepared
	 * @param pAudioEvent       - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the ATLEvent corresponding to the
	 *                 loading process started
	 * @return eARS_SUCCESS if the the request was successfully sent to the audio middleware, eARS_FAILURE otherwise
	 * @see UnprepareTriggerAsync, PrepareTriggerSync, UnprepareTriggerSync
	 */
	virtual EAudioRequestStatus PrepareTriggerAsync(
	  IAudioObject* const pAudioObject,
	  IAudioTrigger const* const pAudioTrigger,
	  IAudioEvent* const pAudioEvent) = 0;

	/**
	 * Unload the metadata and media needed by the audio middleware to execute the ATLTriggerImpl described by pAudioTrigger
	 * asynchronously. An active event that references pAudioEvent is created on the corresponding AudioObject.
	 * The callback called once the unloading is done must report that this event is finished.
	 * Un-preparing Triggers manually is only necessary if their data are not managed via PreloadRequests.
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the ATLTriggerImpl is un-prepared
	 * @param pAudioTrigger     - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to handle the ATLTriggerImpl
	 * @param pAudioEvent       - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the ATLEvent corresponding to the
	 *                 unloading process started
	 * @return eARS_SUCCESS if the the request was successfully sent to the audio middleware, eARS_FAILURE otherwise
	 * @see PrepareTriggerAsync, PrepareTriggerSync, UnprepareTriggerSync
	 */
	virtual EAudioRequestStatus UnprepareTriggerAsync(
	  IAudioObject* const pAudioObject,
	  IAudioTrigger const* const pAudioTrigger,
	  IAudioEvent* const pAudioEvent) = 0;

	/**
	 * Activate an audio-middleware-specific ATLTriggerImpl
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the ATLTriggerImpl should be executed
	 * @param pAudioTrigger     - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to handle the ATLTriggerImpl
	 * @param pAudioEvent       - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the ATLEvent corresponding to the
	 *                 process started by the activation of ATLTriggerImpl
	 * @return eARS_SUCCESS if the ATLTriggerImpl has been successfully activated by the audio middleware, eARS_FAILURE otherwise
	 * @see SetRtpc, SetSwitchState, StopEvent
	 */
	virtual EAudioRequestStatus ActivateTrigger(
	  IAudioObject* const pAudioObject,
	  IAudioTrigger const* const pAudioTrigger,
	  IAudioEvent* const pAudioEvent) = 0;

	/**
	 * Stop and ATLEvent active on an AudioObject
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the
	 *                 specified ATLEvent is active
	 * @param pAudioEvent       - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to identify the ATLEvent that needs to be stopped
	 * @return eARS_SUCCESS if the ATLEvent has been successfully stopped, eARS_FAILURE otherwise
	 * @see StopAllEvents, PrepareTriggerAsync, UnprepareTriggerAsync, ActivateTrigger
	 */
	virtual EAudioRequestStatus StopEvent(
	  IAudioObject* const pAudioObject,
	  IAudioEvent const* const pAudioEvent) = 0;

	/**
	 * Stop all ATLEvents currently active on an AudioObject
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the
	 *                 ATLEvent should be stopped
	 * @return eARS_SUCCESS if all of the ATLEvents have been successfully stopped, eARS_FAILURE otherwise
	 * @see StopEvent, PrepareTriggerAsync, UnprepareTriggerAsync, ActivateTrigger
	 */
	virtual EAudioRequestStatus StopAllEvents(
	  IAudioObject* const pAudioObject) = 0;

	/**
	 * Set the 3D attributes of an AudioObject inside the audio middleware
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                       AudioImplementation code to manage the AudioObject whose position is being set
	 * @param attributes   - a struct containing the audio object's transformation and velocity
	 * @return eARS_SUCCESS if the AudioObject's position has been successfully set, eARS_FAILURE otherwise
	 * @see StopEvent, PrepareTriggerAsync, UnprepareTriggerAsync, ActivateTrigger
	 */
	virtual EAudioRequestStatus Set3DAttributes(
	  IAudioObject* const pAudioObject,
	  SAudioObject3DAttributes const& attributes) = 0;

	/**
	 * Set the ATLRtpcImpl to the specified value on the provided AudioObject
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the ATLRtpcImpl is being set
	 * @param pAudioRtpc        - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to handle the ATLRtpcImpl which is being set
	 * @param value            - the value to be set
	 * @return eARS_SUCCESS if the provided value has been successfully set on the passed ATLRtpcImpl, eARS_FAILURE otherwise
	 * @see ActivateTrigger, SetSwitchState, SetEnvironment, SetListener3DAttributes
	 */
	virtual EAudioRequestStatus SetRtpc(
	  IAudioObject* const pAudioObject,
	  IAudioRtpc const* const pAudioRtpc,
	  float const value) = 0;

	/**
	 * Set the ATLSwitchStateImpl on the given AudioObject
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the ATLSwitchStateImpl is being set
	 * @param pAudioSwitchState - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to handle the ATLSwitchStateImpl which is being set
	 * @return eARS_SUCCESS if the provided ATLSwitchStateImpl has been successfully set, eARS_FAILURE otherwise
	 * @see ActivateTrigger, SetRtpc, SetEnvironment, Set3DAttributes
	 */
	virtual EAudioRequestStatus SetSwitchState(
	  IAudioObject* const pAudioObject,
	  IAudioSwitchState const* const pAudioSwitchState) = 0;

	/**
	 * Set the provided Obstruction and Occlusion values on the given AudioObject
	 * @param pAudioObject - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the Obstruction and Occlusion is being set
	 * @param obstruction      - the obstruction value to be set; it describes how much the direct sound path from the AudioObject to the Listener is obstructed
	 * @param occlusion        - the occlusion value to be set; it describes how much all sound paths (direct and indirect) are obstructed
	 * @return eARS_SUCCESS if the provided the values been successfully set, eARS_FAILURE otherwise
	 * @see SetEnvironment
	 */
	virtual EAudioRequestStatus SetObstructionOcclusion(
	  IAudioObject* const pAudioObject,
	  float const obstruction,
	  float const occlusion) = 0;

	/**
	 * Set the provided value for the specified ATLEnvironmentImpl on the given AudioObject
	 * @param pAudioObject     - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to manage the AudioObject on which the ATLEnvironmentImpl value is being set
	 * @param pAudioEnvironment - implementation-specific data needed by the audio middleware and the
	 *                 AudioImplementation code to identify and use the ATLEnvironmentImpl being set
	 * @param amount               - the fade value for the provided ATLEnvironmentImpl, 0.0f means no effect at all, 1.0f corresponds to the full effect
	 * @return eARS_SUCCESS if the provided the value has been successfully set, eARS_FAILURE otherwise
	 * @see SetObstructionOcclusion
	 */
	virtual EAudioRequestStatus SetEnvironment(
	  IAudioObject* const pAudioObject,
	  IAudioEnvironment const* const pAudioEnvironment,
	  float const amount) = 0;

	/**
	 * Set the world position of an AudioListener inside the audio middleware
	 * @param pAudioListener - implementation-specific data needed by the audio middleware and the
	 *                             AudioImplementation code to manage the AudioListener whose position is being set
	 * @param attributes     - a struct containing the audio listener's transformation and velocity
	 * @return eARS_SUCCESS if the AudioListener's position has been successfully set, eARS_FAILURE otherwise
	 * @see Set3DAttributes
	 */
	virtual EAudioRequestStatus SetListener3DAttributes(
	  IAudioListener* const pAudioListener,
	  SAudioObject3DAttributes const& attributes) = 0;

	/**
	 * Inform the audio middleware about the memory location of a preloaded audio-data file
	 * @param pAudioFileEntry - ATL-specific information describing the resources used by the preloaded file being reported
	 * @return eARS_SUCCESS if the audio middleware is able to use the preloaded file, eARS_FAILURE otherwise
	 * @see UnregisterInMemoryFile
	 */
	virtual EAudioRequestStatus RegisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry) = 0;

	/**
	 * Inform the audio middleware that the memory containing the preloaded audio-data file should no longer be used
	 * @param pAudioFileEntry - ATL-specific information describing the resources used by the preloaded file being invalidated
	 * @return eARS_SUCCESS if the audio middleware was able to unregister the preloaded file supplied, eARS_FAILURE otherwise
	 * @see RegisterInMemoryFile
	 */
	virtual EAudioRequestStatus UnregisterInMemoryFile(SAudioFileEntryInfo* const pAudioFileEntry) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an AudioFileEntry, fill the fields of the struct
	 * referenced by pFileEntryInfo with the data necessary to correctly access and store the file's contents in memory.
	 * Create an object implementing IAudioFileEntry to hold implementation-specific data about the file and store a pointer to it in a member of pFileEntryInfo
	 * @param pAudioFileEntryNode - an XML node containing the necessary information about the file
	 * @param pFileEntryInfo      - a pointer to the struct containing the data used by the ATL to load the file into memory
	 * @return eARS_SUCCESS if the XML node was parsed successfully, eARS_FAILURE otherwise
	 */
	virtual EAudioRequestStatus ParseAudioFileEntry(
	  XmlNodeRef const pAudioFileEntryNode,
	  SAudioFileEntryInfo* const pFileEntryInfo) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioFileEntry instance.
	 * Normally, an IAudioFileEntry instance is created by ParseAudioFileEntry() and a pointer is stored in a member of SAudioFileEntryInfo.
	 * @param pOldAudioFileEntry - pointer to the object implementing IAudioFileEntry to be discarded
	 * @see ParseAudioFileEntry
	 */
	virtual void DeleteAudioFileEntry(IAudioFileEntry* const pOldAudioFileEntry) = 0;

	/**
	 * Get the full path to the folder containing the file described by the pFileEntryInfo
	 * @param pFileEntryInfo - ATL-specific information describing the file whose location is being queried
	 * @return A C-string containing the path to the folder where the file corresponding to the pFileEntryInfo is stored
	 */
	virtual char const* const GetAudioFileLocation(SAudioFileEntryInfo* const pFileEntryInfo) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an ATLTriggerImpl, return a pointer to the data needed for identifying
	 * and using this ATLTriggerImpl instance inside the AudioImplementation
	 * @param pAudioTriggerNode - an XML node corresponding to the new ATLTriggerImpl to be created
	 * @param info - reference to a struct to be filled be the implementation with information about the trigger (eg. max attenuation radius)
	 * @return IAudioTrigger const* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding ATLTriggerImpl; NULL if the new AudioTriggerImplData instance was not created
	 * @see DeleteAudioTrigger
	 */
	virtual IAudioTrigger const* NewAudioTrigger(XmlNodeRef const pAudioTriggerNode, SAudioTriggerInfo& info) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioTrigger instance
	 * @param pOldAudioTrigger - pointer to the object implementing IAudioTrigger to be discarded
	 * @see NewAudioTrigger
	 */
	virtual void DeleteAudioTrigger(IAudioTrigger const* const pOldAudioTrigger) = 0;

	/**
	 * Create an object implementing NewAudioStandaloneFile that stores all of the data needed by the AudioImplementation
	 * to identify and use an audio standalone file. Return a pointer to that object.
	 * @return IAudioStandaloneFile pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding audio standalone file.
	 * @return NULL if the new IAudioStandaloneFile instance was not created.
	 * @see DeleteAudioStandaloneFile, ResetAudioStandaloneFile
	 */
	virtual IAudioStandaloneFile* NewAudioStandaloneFile() = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioStandaloneFile instance
	 * @param _pOldAudioStandaloneFile - pointer to the object implementing IAudioStandaloneFile to be discarded
	 * @see ResetAudioStandaloneFile
	 */
	virtual void DeleteAudioStandaloneFile(IAudioStandaloneFile const* const _pOldAudioStandaloneFile) = 0;

	/**
	 * Reset all the members of an IAudioStandaloneFile instance without releasing the memory, so that the
	 * instance can be returned to the pool to be reused.
	 * @param _pAudioStandaloneFile - pointer to the object implementing IAudioStandaloneFile to be reset
	 * @see DeleteAudioStandaloneFile
	 */
	virtual void ResetAudioStandaloneFile(IAudioStandaloneFile* const _pAudioStandaloneFile) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an ATLRtpcImpl, return a pointer to the data needed for identifying
	 * and using this ATLRtpcImpl instance inside the AudioImplementation
	 * @param pAudioRtpcNode - an XML node corresponding to the new ATLRtpcImpl to be created
	 * @return IAudioRtpc const* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding ATLRtpcImpl; NULL if the new AudioTrigger instance was not created
	 * @see DeleteAudioRtpc
	 */
	virtual IAudioRtpc const* NewAudioRtpc(XmlNodeRef const pAudioRtpcNode) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioRtpc instance
	 * @param pOldAudioRtpc - pointer to the object implementing IAudioRtpc to be discarded
	 * @see NewAudioRtpc
	 */
	virtual void DeleteAudioRtpc(IAudioRtpc const* const pOldAudioRtpc) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an ATLSwitchStateImpl, return a pointer to the data needed for identifying
	 * and using this ATLSwitchStateImpl instance inside the AudioImplementation
	 * @param pAudioSwitchStateNode - an XML node corresponding to the new ATLSwitchStateImpl to be created
	 * @return IAudioRtpc const* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding ATLSwitchStateImpl; NULL if the new AudioTriggerImplData instance was not created
	 * @see DeleteAudioSwitchState
	 */
	virtual IAudioSwitchState const* NewAudioSwitchState(XmlNodeRef const pAudioSwitchStateNode) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioSwitchState instance
	 * @param pOldAudioSwitchState - pointer to the object implementing IAudioSwitchState to be discarded
	 * @see NewAudioSwitchState
	 */
	virtual void DeleteAudioSwitchState(IAudioSwitchState const* const pOldAudioSwitchState) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an ATLEnvironmentImpl, return a pointer to the data needed for identifying
	 * and using this ATLEnvironmentImpl instance inside the AudioImplementation
	 * @param pAudioEnvironmentNode - an XML node corresponding to the new ATLEnvironmentImpl to be created
	 * @return IAudioEnvironment const* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding ATLEnvironmentImpl; NULL if the new IAudioEnvironment instance was not created
	 * @see DeleteAudioEnvironment
	 */
	virtual IAudioEnvironment const* NewAudioEnvironment(XmlNodeRef const pAudioEnvironmentNode) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioEnvironment instance
	 * @param pOldAudioEnvironment - pointer to the object implementing IAudioEnvironment to be discarded
	 * @see NewAudioEnvironment
	 */
	virtual void DeleteAudioEnvironment(IAudioEnvironment const* const pOldAudioEnvironment) = 0;

	/**
	 * Create an object implementing IAudioObject that stores all of the data needed by the AudioImplementation
	 * to identify and use the GlobalAudioObject with the provided AudioObjectID
	 * @param audioObjectID - the AudioObjectID to be used for the new GlobalAudioObject
	 * @return IAudioObject* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding GlobalAudioObject; NULL if the new IAudioObject instance was not created
	 * @see DeleteAudioObject
	 */
	virtual IAudioObject* NewGlobalAudioObject(AudioObjectId const audioObjectID) = 0;

	/**
	 * Create an object implementing IAudioObject that stores all of the data needed by the AudioImplementation
	 * to identify and use the AudioObject with the provided AudioObjectID. Return a pointer to that object.
	 * @param audioObjectID - the AudioObjectID to be used for the new AudioObject
	 * @return IAudioObject* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding GlobalAudioObject; NULL if the new IAudioObject instance was not created
	 * @see DeleteAudioObject
	 */
	virtual IAudioObject* NewAudioObject(AudioObjectId const audioObjectID) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioObject instance
	 * Remark: ResetAudioObject is always called before this method is called.
	 * @param pOldAudioObject - pointer to the object implementing IAudioObject to be discarded
	 * @see NewAudioObject, NewGlobalAudioObject
	 */
	virtual void DeleteAudioObject(IAudioObject const* const pOldAudioObject) = 0;

	/**
	 * Create an object implementing IAudioListener that stores all of the data needed by the AudioImplementation
	 * to identify and use the DefaultAudioListener. Return a pointer to that object.
	 * @param audioObjectId - id of the created AudioListener object
	 * @return IAudioListener* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the DefaultAudioListener; NULL if the new IAudioListener instance was not created
	 * @see DeleteAudioListener
	 */
	virtual IAudioListener* NewDefaultAudioListener(AudioObjectId const audioObjectId) = 0;

	/**
	 * Create an object implementing IAudioListener that stores all of the data needed by the AudioImplementation
	 * to identify and use an AudioListener. Return a pointer to that object.
	 * @param audioObjectId - id of the created AudioListener object
	 * @return IAudioListener* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding AudioListener; NULL if the new IAudioListener instance was not created
	 * @see DeleteAudioListener
	 */
	virtual IAudioListener* NewAudioListener(AudioObjectId const audioObjectId) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioListener instance
	 * @param pOldAudioListener - pointer to the object implementing IAudioListener to be discarded
	 * @see NewDefaultAudioListener, NewAudioListenerObjectData
	 */
	virtual void DeleteAudioListener(IAudioListener* const pOldAudioListener) = 0;

	/**
	 * Create an object implementing IAudioEvent that stores all of the data needed by the AudioImplementation
	 * to identify and use an AudioEvent. Return a pointer to that object.
	 * @param audioEventID - AudioEventID to be used for the newly created AudioEvent
	 * @return IAudioEvent* pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding AudioEvent; NULL if the new IAudioEvent instance was not created
	 * @see DeleteAudioEvent, ResetAudioEvent
	 */
	virtual IAudioEvent* NewAudioEvent(AudioEventId const audioEventID) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IAudioEvent instance
	 * Remark: ResetAudioEvent is always called before this method is called.
	 * @param pOldEventData - pointer to the object implementing IAudioEvent to be discarded
	 * @see NewAudioEvent, ResetAudioEvent
	 */
	virtual void DeleteAudioEvent(IAudioEvent const* const pOldAudioEvent) = 0;

	/**
	 * Reset all the members of an IAudioEvent instance without releasing the memory, so that the
	 * instance can be returned to the pool to be reused.
	 * @param pAudioEvent - pointer to the object implementing IAudioEvent to be reset
	 * @see NewAudioEvent, DeleteAudioEvent
	 */
	virtual void ResetAudioEvent(IAudioEvent* const pAudioEvent) = 0;

	/**
	 * Called whenever a Gamepad gets connected.
	 * This is used by audio middleware that supports controller effects such as rumble.
	 * @param deviceUniqueID - unique device identifier
	 */
	virtual void GamepadConnected(TAudioGamepadUniqueID const deviceUniqueID) = 0;

	/**
	 * Called whenever a Gamepad gets disconnected.
	 * This is used by audio middleware that supports controller effects such as rumble.
	 * @param deviceUniqueID - unique device identifier
	 */
	virtual void GamepadDisconnected(TAudioGamepadUniqueID const deviceUniqueID) = 0;

	/**
	 * Informs the audio middleware that the localized sound banks and streamed files need to use
	 * a different language. NOTE: this function DOES NOT unload or reload the currently loaded audio files
	 * @param szLanguage - a C-string representing the CryEngine language
	 * @see GetAudioFileLocation
	 */
	virtual void SetLanguage(char const* const szLanguage) = 0;

	//////////////////////////////////////////////////////////////////////////
	// NOTE: The methods below are ONLY USED when INCLUDE_AUDIO_PRODUCTION_CODE is defined!
	//////////////////////////////////////////////////////////////////////////

	/**
	 * Return a string describing the audio middleware used. This string is printed on
	 * the first line of the AudioDebug header shown on the screen whenever s_DrawAudioDebug is not 0
	 * @return A zero-terminated C-string with the description of the audio middleware used by this AudioImplementation.
	 */
	virtual char const* const GetImplementationNameString() const = 0;

	/**
	 * Fill in the memoryInfo describing the current memory usage of this AudioImplementation.
	 * This data gets displayed in the AudioDebug header shown on the screen whenever s_DrawAudioDebug is not 0
	 * @param memoryInfo - a reference to an instance of SAudioImplMemoryInfo
	 */
	virtual void GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const = 0;

	/**
	 * Asks the audio implementation to fill the audioFileData structure with data (eg. duration of track) relating to the
	 * standalone file referenced in szFilename.
	 * @param szFilename - filepath to the standalone file
	 * @param audioFileData - a reference to an instance of SAudioFileData
	 */
	virtual void GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) const = 0;
};
}
}
