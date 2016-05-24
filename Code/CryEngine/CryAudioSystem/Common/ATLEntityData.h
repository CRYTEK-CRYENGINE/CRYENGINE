// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>

//////////////////////////////////////////////////////////////////////////
// Description:
//      This file defines the data-types used in the IAudioSystemImplementation.h
//////////////////////////////////////////////////////////////////////////

namespace CryAudio
{
namespace Impl
{
/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding ATLAudioObject (e.g. a middleware-specific unique ID)
 */
struct IAudioObject
{
	virtual ~IAudioObject() {}
};

/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding ATLListenerObject (e.g. a middleware-specific unique ID)
 */
struct IAudioListener
{
	virtual ~IAudioListener() {}
};

/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding IAudioTrigger
 * (e.g. a middleware-specific event ID or name, a sound-file name to be passed to an API function)
 */
struct IAudioTrigger
{
	virtual ~IAudioTrigger() {}
};

struct SAudioTriggerInfo
{
	float maxRadius;
};

/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding IAudioRtpc
 * (e.g. a middleware-specific control ID or a parameter name to be passed to an API function)
 */
struct IAudioRtpc
{
	virtual ~IAudioRtpc() {}
};

/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding IAudioSwitchState
 * (e.g. a middleware-specific control ID or a switch and state names to be passed to an API function)
 */
struct IAudioSwitchState
{
	virtual ~IAudioSwitchState() {}
};

/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding IAudioEnvironment
 * (e.g. a middleware-specific bus ID or name to be passed to an API function)
 */
struct IAudioEnvironment
{
	virtual ~IAudioEnvironment() {}
};

/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding IAudioEvent
 * (e.g. a middleware-specific playingID of an active event/sound for a play event)
 */
struct IAudioEvent
{
	virtual ~IAudioEvent() {}
};

/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding IAudioFileEntry.
 * (e.g. a middleware-specific bank ID if the AudioFileEntry represents a soundbank)
 */
struct IAudioFileEntry
{
	virtual ~IAudioFileEntry() {}
};

/**
 * This is a POD structure used to pass the information about a file preloaded into memory between
 * the CryAudioSystem and an AudioImpl
 * Note: This struct cannot define a constructor, it needs to be a POD!
 */
struct SAudioFileEntryInfo
{
	void*            pFileData;                // pointer to the memory location of the file's contents
	size_t           memoryBlockAlignment;     // memory alignment to be used for storing this file's contents in memory
	size_t           size;                     // file size
	char const*      szFileName;               // file name
	bool             bLocalized;               // is the file localized
	IAudioFileEntry* pImplData;                // pointer to the implementation-specific data needed for this AudioFileEntry
};

/**
 * An AudioImpl may use this interface to define a class for storing implementation-specific
 * data needed for identifying and using the corresponding IAudioStandaloneFile.
 * (e.g. middleware-specific custom data that is associated with the standalone file)
 */
struct IAudioStandaloneFile
{
	virtual ~IAudioStandaloneFile() {}
};

/**
 * This is a POD structure used to pass the information about a standalone file
 * between the CryAudioSystem and an AudioImpl
 * Note: This struct cannot define a constructor, it needs to be a POD!
 * Members:
 * pAudioObject - implementation-specific data needed by the audio middleware and the
 * AudioImpl code to manage the AudioObject to which the stand alone audio file is associated.
 * pImplData        - pointer to the implementation-specific data needed for a stand alone audio file.
 * szFileName       - name of or path to the stand alone audio file.
 * fileId           - Id unique to the file.
 * fileInstanceId   - Id unique to the file instance.
 */
struct SAudioStandaloneFileInfo
{
	SAudioStandaloneFileInfo()
		: pAudioObject(nullptr)
		, pImplData(nullptr)
		, szFileName(nullptr)
		, pUsedAudioTrigger(nullptr)
		, fileId(INVALID_AUDIO_STANDALONE_FILE_ID)
		, fileInstanceId(INVALID_AUDIO_STANDALONE_FILE_ID)
		, bLocalized(true)
	{}

	IAudioObject*         pAudioObject;      // pointer to the memory location of the associated audio object
	IAudioStandaloneFile* pImplData;         // pointer to the implementation-specific data needed for this AudioStandaloneFile
	char const*           szFileName;        // file name
	const IAudioTrigger*  pUsedAudioTrigger; // the audioTrigger that should be used for the playback
	AudioStandaloneFileId fileId;            // ID unique to the file
	AudioStandaloneFileId fileInstanceId;    // ID unique to the file instance
	bool                  bLocalized;        // is the asset localized
};

/**
 * This is a POD structure used to pass the information about an AudioImpl's memory usage
 * Note: This struct cannot define a constructor, it needs to be a POD!
 */
struct SAudioImplMemoryInfo
{
	size_t primaryPoolSize;              // total size in bytes of the Primary Memory Pool used by an AudioImpl
	size_t primaryPoolUsedSize;          // bytes allocated inside the Primary Memory Pool used by an AudioImpl
	size_t primaryPoolAllocations;       // number of allocations performed in the Primary Memory Pool used by an AudioImpl
	size_t secondaryPoolSize;            // total size in bytes of the Secondary Memory Pool used by an AudioImpl
	size_t secondaryPoolUsedSize;        // bytes allocated inside the Secondary Memory Pool used by an AudioImpl
	size_t secondaryPoolAllocations;     // number of allocations performed in the Secondary Memory Pool used by an AudioImpl
	size_t bucketUsedSize;               // bytes allocated by the Bucket allocator (used for small object allocations)
	size_t bucketAllocations;            // total number of allocations by the Bucket allocator (used for small object allocations)
};
}
}
