// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GFXVIDEOSOUNDCRYSOUND_H_
#define _GFXVIDEOSOUNDCRYSOUND_H_

#pragma once

#ifdef INCLUDE_SCALEFORM_SDK

namespace CryVideoSoundSystem
{
	struct IChannelDelegate
	{
		// <interfuscator:shuffle>
		virtual void Release() = 0;

		virtual bool Stop()                = 0;
		virtual bool SetPaused(bool pause) = 0;

		virtual bool SetVolume(float volume) = 0;
		virtual bool Mute(bool mute)         = 0;

		virtual bool SetBytePosition(unsigned int bytePos)  = 0;
		virtual bool GetBytePosition(unsigned int& bytePos) = 0;

		virtual bool SetSpeakerMix(float fl, float fr, float c, float lfe, float bl, float br, float sl, float sr) = 0;
		// </interfuscator:shuffle>

	protected:
		virtual ~IChannelDelegate() {}
	};

	struct ISoundDelegate
	{
		struct LockRange
		{
			void* p0;
			void* p1;
			unsigned int length0;
			unsigned int length1;
		};

		// <interfuscator:shuffle>
		virtual void Release() = 0;

		virtual IChannelDelegate* Play() = 0;

		virtual bool Lock(unsigned int offset, unsigned int length, LockRange& lr) = 0;
		virtual bool Unlock(const LockRange& lr) = 0;
		// </interfuscator:shuffle>

	protected:
		virtual ~ISoundDelegate() {}
	};

	struct IPlayerDelegate
	{
		// <interfuscator:shuffle>
		virtual void Release() = 0;

		virtual ISoundDelegate* CreateSound(unsigned int numChannels, unsigned int sampleRate, unsigned int lengthInBytes) = 0;

		virtual bool MuteMainTrack() const = 0;
		// </interfuscator:shuffle>

	protected:
		virtual ~IPlayerDelegate() {}
	};

	struct IAllocatorDelegate
	{
		// <interfuscator:shuffle>
		virtual void* Allocate(size_t size) = 0;
		virtual void  Free(void* p)         = 0;
		// </interfuscator:shuffle>

	protected:
		virtual ~IAllocatorDelegate() {}
	};
}                                        // namespace CryVideoSoundSystem

#endif                                   //#ifdef INCLUDE_SCALEFORM_SDK

#ifdef INCLUDE_SCALEFORM_SDK

#include <CrySystem/Scaleform/ConfigScaleform.h>

#if defined(USE_GFX_VIDEO)

#pragma warning(push)
#pragma warning(disable : 6326)          // Potential comparison of a constant with another constant
#pragma warning(disable : 6011)          // Dereferencing NULL pointer
#include <CryCore/Platform/CryWindows.h>
#include <GFxSystemSoundInterface.h>     // includes <windows.h>
#pragma warning(pop)

class GFxVideoCrySoundSystemImpl;

class GFxVideoCrySoundSystem:public GFxVideoSoundSystem
{
public:
	virtual GFxVideoSound* Create(GFxVideoPlayer::SoundTrack type);

public:
	GFxVideoCrySoundSystem(GMemoryHeap* pHeap);
	virtual ~GFxVideoCrySoundSystem();

public:
	static void InitCVars();

private:
	GFxVideoCrySoundSystemImpl* m_pImpl;
};

#else

class GFxVideoCrySoundSystem
{
public:
	static void InitCVars();

};

#endif   // #if defined(USE_GFX_VIDEO)

#endif   // #ifdef INCLUDE_SCALEFORM_SDK

#endif   // #ifndef _GFXVIDEOSOUNDCRYSOUND_H_
