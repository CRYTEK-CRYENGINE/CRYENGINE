// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef INCLUDE_SCALEFORM_SDK

	#include "GFxVideoSoundCrySound.h"
	#include <CrySystem/IConsole.h>

static float s_sys_flash_video_soundvolume = 1.0f;
static float s_sys_flash_video_subaudiovolume = 1.0f;

	#if defined(USE_GFX_VIDEO)

		#include "SharedStates.h"
		#include <GHeapNew.h>
		#include <CryThreading/IThreadManager.h>
		#include <CryThreading/IThreadConfigManager.h>

//#define ENABLE_VIDEOSOUND_UPDATE_LOGGING

//////////////////////////////////////////////////////////////////////////
		#if defined(INCLUDE_SCALEFORM_SDK) && !CRY_PLATFORM_ORBIS

namespace CryVideoSoundSystem
{
template<typename Base, typename Instance>
struct DelegateFactory : public Base
{
	static Instance* Create(IAllocatorDelegate* pAllocator, const Instance& src)
	{
		if (!pAllocator)
			return 0;

		Instance* p = (Instance*) pAllocator->Allocate(sizeof(Instance));
		new(p) Instance(src);
		return p;
	}

	static void Destroy(Instance* p)
	{
		if (p)
		{
			IAllocatorDelegate* pAllocator = p->m_pAllocator;
			p->~Instance();
			pAllocator->Free(p);
		}
	}
};

//////////////////////////////////////////////////////////////////////////
struct ChannelDelegate : DelegateFactory<IChannelDelegate, ChannelDelegate>
{
	template<typename Base, typename Instance> friend struct DelegateFactory;

public:
	virtual void Release()
	{
		Destroy(this);
	}

	virtual bool Stop()
	{
		return true /*m_pChannel && FMOD_OK == m_pChannel->stop()*/;
	}

	virtual bool SetPaused(bool pause)
	{
		return true /*m_pChannel && FMOD_OK == m_pChannel->setPaused(pause)*/;
	}

	virtual bool SetVolume(float volume)
	{
		return true /*m_pChannel && FMOD_OK == m_pChannel->setVolume(volume)*/;
	}

	virtual bool Mute(bool mute)
	{
		return true /*m_pChannel && FMOD_OK == m_pChannel->setMute(mute)*/;
	}

	virtual bool SetBytePosition(unsigned int bytePos)
	{
		return true /*m_pChannel && FMOD_OK == m_pChannel->setPosition(bytePos, FMOD_TIMEUNIT_PCMBYTES)*/;
	}

	virtual bool GetBytePosition(unsigned int& bytePos)
	{
		return true /*m_pChannel && FMOD_OK == m_pChannel->getPosition(&bytePos, FMOD_TIMEUNIT_PCMBYTES)*/;
	}

	virtual bool SetSpeakerMix(float fl, float fr, float c, float lfe, float bl, float br, float sl, float sr)
	{
		return true /*m_pChannel && FMOD_OK == m_pChannel->setSpeakerMix(fl, fr, c, lfe, bl, br, sl, sr)*/;
	}

public:
	//static ChannelDelegate* CreateInstance(IAllocatorDelegate* pAllocator, FMOD::Channel* pChannel)
	//{
	//	ChannelDelegate* const pChannelDelegate = Create(pAllocator, ChannelDelegate(pAllocator, pChannel));

	//	if (pChannelDelegate && pChannel)
	//	{
	//		// Boost priority.
	//		pChannel->setPriority(0);

	//		// Disable reverb.
	//		FMOD_REVERB_CHANNELPROPERTIES oReverbProperties = {0};
	//		FMOD_RESULT eResult = pChannel->getReverbProperties(&oReverbProperties);
	//		assert(eResult == FMOD_OK);
	//		oReverbProperties.Room = -10000;
	//		eResult = pChannel->setReverbProperties(&oReverbProperties);
	//		assert(eResult == FMOD_OK);

	//		// If desired we can make this channel part of a category to play through.
	//		/*FMOD::EventSystem* const pEventSystem = static_cast<FMOD::EventSystem* const>(gEnv->pSoundSystem->GetIAudioDevice()->GetEventSystem());

	//		if (pEventSystem)
	//		{
	//			FMOD::EventCategory* pMasterCategory = NULL;
	//			FMOD::EventCategory* pMusicCategory  = NULL;
	//			eResult = pEventSystem->getCategory("master", &pMasterCategory);
	//			assert(eResult == FMOD_OK);

	//			if (eResult == FMOD_OK)
	//			{
	//				eResult = pMasterCategory->getCategory("music", &pMusicCategory);
	//				assert(eResult == FMOD_OK);

	//				if (eResult == FMOD_OK)
	//				{
	//					FMOD::ChannelGroup* pChannelGroup = NULL;
	//					eResult = pMusicCategory->getChannelGroup(&pChannelGroup);
	//					assert(eResult == FMOD_OK);
	//					eResult = pChannel->setChannelGroup(pChannelGroup);
	//					assert(eResult == FMOD_OK);
	//				}
	//			}
	//		}*/
	//	}

	//	return pChannelDelegate;
	//}

protected:

	/*ChannelDelegate(IAllocatorDelegate* pAllocator, FMOD::Channel* pChannel)
	   : m_pAllocator(pAllocator)
	   , m_pChannel(pChannel)
	   {
	   assert(m_pAllocator);
	   assert(m_pChannel);
	   }*/
	virtual ~ChannelDelegate() {}

protected:
	IAllocatorDelegate* m_pAllocator;
	//FMOD::Channel* m_pChannel;
};

//////////////////////////////////////////////////////////////////////////
struct SoundDelegate : public DelegateFactory<ISoundDelegate, SoundDelegate>
{
	template<typename Base, typename Instance> friend struct DelegateFactory;

public:
	virtual void Release()
	{
		Destroy(this);
	}

	virtual IChannelDelegate* Play()
	{
		/*FMOD::Channel* pChannel = 0;
		   if (!m_pFMOD || m_pFMOD->playSound(FMOD_CHANNEL_FREE, m_pSound, false, &pChannel) != FMOD_OK)
		   return 0;*/

		return NULL;  //ChannelDelegate::CreateInstance(m_pAllocator, pChannel);
	}

	virtual bool Lock(unsigned int offset, unsigned int length, LockRange& lr)
	{
		return true;  //m_pSound ? m_pSound->lock(offset, length, &lr.p0, &lr.p1, &lr.length0, &lr.length1) == FMOD_OK : false;
	}

	virtual bool Unlock(const LockRange& lr)
	{
		return true;  //m_pSound ? m_pSound->unlock(lr.p0, lr.p1, lr.length0, lr.length1) == FMOD_OK : false;
	}

public:
	/*static SoundDelegate* CreateInstance(IAllocatorDelegate* pAllocator, FMOD::System* pFMOD, FMOD::Sound* pSound)
	   {
	   return Create(pAllocator, SoundDelegate(pAllocator, pFMOD, pSound));
	   }*/

protected:
	/*SoundDelegate(IAllocatorDelegate* pAllocator, FMOD::System* pFMOD, FMOD::Sound* pSound)
	   : m_pAllocator(pAllocator)
	   , m_pFMOD(pFMOD)
	   , m_pSound(pSound)
	   {
	   assert(m_pAllocator);
	   assert(m_pFMOD);
	   assert(m_pSound);
	   }*/

	//SoundDelegate(const SoundDelegate& rhs)
	//: m_pAllocator(rhs.m_pAllocator)
	//, m_pFMOD(rhs.m_pFMOD)
	//, m_pSound(rhs.m_pSound)
	//{
	//	// transfer sound ownership
	//	SoundDelegate& _rhs = const_cast<SoundDelegate&>(rhs);
	//	_rhs.m_pSound = 0;
	//}

	virtual ~SoundDelegate()
	{
		/*if (m_pSound)
		   {
		   m_pSound->release();
		   m_pSound = 0;
		   }*/
	}

protected:
	IAllocatorDelegate* m_pAllocator;
	//FMOD::System* m_pFMOD;
	//FMOD::Sound* m_pSound;
};

//////////////////////////////////////////////////////////////////////////
struct PlayerDelegate : public DelegateFactory<IPlayerDelegate, PlayerDelegate>
{
	template<typename Base, typename Instance> friend struct DelegateFactory;

public:
	virtual void Release()
	{
		Destroy(this);
	}

	virtual ISoundDelegate* CreateSound(unsigned int numChannels, unsigned int sampleRate, unsigned int lengthInBytes)
	{
		return NULL;
		/*if (!m_pFMOD)
		   return 0;

		   FMOD_CREATESOUNDEXINFO exinfo;
		   memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
		   exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
		   exinfo.length = lengthInBytes;
		   exinfo.numchannels = numChannels;
		   exinfo.defaultfrequency = sampleRate;
		   exinfo.format = FMOD_SOUND_FORMAT_PCM16;

		   FMOD_MODE const flags = FMOD_2D | FMOD_SOFTWARE | FMOD_OPENUSER | FMOD_LOOP_NORMAL | FMOD_LOWMEM;

		   FMOD::Sound* pSound = 0;
		   if (m_pFMOD->createSound(0, flags, &exinfo, &pSound) != FMOD_OK)
		   {
		   if (pSound)
		    pSound->release();
		   return 0;
		   }

		   unsigned int len = 0;
		   if (pSound->getLength(&len, FMOD_TIMEUNIT_PCMBYTES) != FMOD_OK || len != lengthInBytes)
		   {
		   pSound->release();
		   return 0;
		   }

		   return SoundDelegate::CreateInstance(m_pAllocator, m_pFMOD, pSound);*/
	}

	virtual bool MuteMainTrack() const
	{
		return false;
	}

public:
	/*static PlayerDelegate* CreateInstance(IAllocatorDelegate* pAllocator, FMOD::System* pFMOD)
	   {
	   return NULL;
	   return Create(pAllocator, PlayerDelegate(pAllocator, pFMOD));
	   }*/

protected:
	/*PlayerDelegate(IAllocatorDelegate* pAllocator, FMOD::System* pFMOD)
	   : m_pAllocator(pAllocator)
	   , m_pFMOD(pFMOD)
	   {
	   assert(m_pAllocator);
	   assert(m_pFMOD);
	   }*/
	virtual ~PlayerDelegate() {}

protected:
	IAllocatorDelegate* m_pAllocator;
	//FMOD::System* m_pFMOD;
};

} // namespace CryVideoSoundSystem

//CryVideoSoundSystem::IPlayerDelegate* CSoundSystem::CreateVideoSoundSystem(CryVideoSoundSystem::IAllocatorDelegate* pAllocator) const
//{
//	FMOD::System* pFMOD = (FMOD::System*) (m_pAudioDevice ? m_pAudioDevice->GetSoundLibrary() : 0);
//	return pFMOD && pAllocator ? CryVideoSoundSystem::PlayerDelegate::CreateInstance(pAllocator, pFMOD) : 0;
//}

		#endif // #if defined(INCLUDE_SCALEFORM_SDK) && !CRY_PLATFORM_ORBIS

using namespace CryVideoSoundSystem;

class GFxVideoCrySoundSystemImpl;
class GFxVideoCrySound;

//////////////////////////////////////////////////////////////////////////
// Video sound timer to make up for Xbox 360's QPC shortcomings,
// i.e. time lags measurably after several seconds already
// eventually causing video/sound hitches and forced resyncs

struct VideoSoundTimer
{
public:

	static uint64 GetTick()
	{
		uint64 ticks;
		QueryPerformanceCounter((LARGE_INTEGER*) &ticks);
		return ticks;
	}
	static float TicksToSeconds(const uint64 ticks)
	{
		return (float) ((double) ticks / (double) ms_frequency);
	}
	static uint64 TicksPerSecond()
	{
		return ms_frequency;
	}

private:
	static uint64 ms_frequency;

public:
	static void InitFrequency()
	{
		uint64 freq = 0;
		QueryPerformanceFrequency((LARGE_INTEGER*) &freq);
		ms_frequency = freq;
	}

private:
	VideoSoundTimer() {}
	~VideoSoundTimer() {}
};

uint64 VideoSoundTimer::ms_frequency = 0;

//////////////////////////////////////////////////////////////////////////
// impl class declarations

class GFxVideoCrySoundSystemImpl : public GNewOverrideBase<GStat_Video_Mem>
{
public:
	GFxVideoCrySoundSystemImpl(GMemoryHeap* pHeap);
	~GFxVideoCrySoundSystemImpl();

	void   AttachSound(GFxVideoCrySound* pSound);
	void   DetachSound(GFxVideoCrySound* pSound);

	uint64 GetSafeStartTick() const;

	void   PulseEvent() { m_event.PulseEvent(); }
	//IPlayerDelegate* GetPlayerDelegate() { return m_pPlayerDelegate; }

private:
	static const unsigned int UpdateThreadStackSize = 16 * 1024;

	struct AllocatorDelegate : public IAllocatorDelegate
	{
	public:
		void* Allocate(size_t size)
		{
			++m_numAllocs;
			return GHEAP_ALLOC(m_pHeap, size, GStat_Video_Mem);
		}

		void Free(void* p)
		{
			GHEAP_FREE(m_pHeap, p);
			--m_numAllocs;
		}

	public:
		AllocatorDelegate(GMemoryHeap* pHeap)
			: m_pHeap(pHeap)
			, m_numAllocs(0)
		{
			assert(m_pHeap);
		}

		~AllocatorDelegate()
		{
			assert(!m_numAllocs);
		}

	private:
		GMemoryHeap* m_pHeap;
		size_t       m_numAllocs;
	};

private:
	static int UpdateFunc(GThread*, void* h);
	float      UpdateAuxStreams();

private:
	GMemoryHeap*                m_pHeap;
	GArrayDH<GFxVideoCrySound*> m_sounds;
	GLock                       m_soundsLock;
	IPlayerDelegate*            m_pPlayerDelegate;
	GPtr<GThread>               m_pUpdateThread;
	GEvent                      m_event;
	volatile bool               m_stopThread;
	AllocatorDelegate           m_allocDelegate;
};

class GFxVideoCrySound : public GFxVideoSound
{
public:
	virtual bool      CreateOutput(UInt32 numChannels, UInt32 sampleRate);
	virtual void      DestroyOutput();

	virtual PCMFormat GetPCMFormat() const { return PCM_SInt16; }
	virtual void      Start(PCMStream* pStream);
	virtual void      Stop();
	virtual Status    GetStatus() const { return m_status; }

	virtual void      Pause(bool pause);
	virtual void      GetTime(UInt64* pCount, UInt64* pUnit) const;

public:
	GFxVideoCrySound(GMemoryHeap* pHeap, GFxVideoCrySoundSystemImpl* pSoundSystem, GFxVideoPlayer::SoundTrack type);
	virtual ~GFxVideoCrySound();

	float Update(bool muteMainTrack);

private:
	typedef signed short SoundSampleType;

	static const unsigned int AuxSoundReadBufLenMs = 300;
	static const unsigned int AuxSoundLenMs = AuxSoundReadBufLenMs * 15 / 10; // = AuxSoundReadBufLenMs * 5

private:
	uint64       GetTotalBytesReadInTicks() const          { return BytesToTicks(m_totalBytesRead); }
	uint64       BytesToTicks(unsigned int bytes) const    { return uint64(VideoSoundTimer::TicksPerSecond()) * bytes / (m_sampleRate * m_numChannels * sizeof(SoundSampleType)); }
	unsigned int DistToFillBuffPos(unsigned int pos) const { return (pos > m_fillPosition) ? (m_fillPosition + m_soundLength) - pos : m_fillPosition - pos; }

	unsigned int ReadAndFillSound(bool fillup);
	void         ClearSoundBuffer();
	unsigned int GetPCMData();
	void         ReleaseBuffers();

	void         UpdateVolume(bool muteMainTrack);

private:
	GMemoryHeap*                m_pHeap;
	GFxVideoCrySoundSystemImpl* m_pSoundSystem;

	IPlayerDelegate*            m_pPlayerDelegate;
	ISoundDelegate*             m_pSound;
	IChannelDelegate*           m_pChannel;

	PCMStream*                  m_pPCMStream;
	unsigned int                m_numChannels;
	Status                      m_status;
	unsigned int                m_sampleRate;
	float                       m_volume;
	bool                        m_muted;

	GFxVideoPlayer::SoundTrack  m_type;

	SoundSampleType*            m_pChannelBuffer[MaxChannels];
	unsigned char*              m_pInterleavedBuffer;
	unsigned int                m_bufferSize;
	unsigned int                m_bufferSamples;
	unsigned int                m_soundLength;

	unsigned int                m_fillPosition;
	unsigned int                m_totalBytesRead;

	uint64                      m_startTick;
	uint64                      m_totalTicks;
	uint64                      m_starvedAtTick;

	bool                        m_starved;

	mutable GLock               m_lock;
};

//////////////////////////////////////////////////////////////////////////
// GFxVideoCrySoundSystemImpl

GFxVideoCrySoundSystemImpl::GFxVideoCrySoundSystemImpl(GMemoryHeap* pHeap)
	: m_pHeap(pHeap)
	, m_sounds(pHeap)
	, m_soundsLock()
	, m_pPlayerDelegate(0)
	, m_pUpdateThread(0)
	, m_event()
	, m_stopThread()
	, m_allocDelegate(pHeap)
{
	assert(pHeap);
	//ISoundSystem* pSoundSystem = gEnv->pAudioSystem;
	//m_pPlayerDelegate = pSoundSystem ? pSoundSystem->CreateVideoSoundSystem(&m_allocDelegate) : 0;
}

GFxVideoCrySoundSystemImpl::~GFxVideoCrySoundSystemImpl()
{
	if (m_pPlayerDelegate)
	{
		m_pPlayerDelegate->Release();
		m_pPlayerDelegate = 0;
	}
}

uint64 GFxVideoCrySoundSystemImpl::GetSafeStartTick() const
{
	return VideoSoundTimer::GetTick();
}

void GFxVideoCrySoundSystemImpl::AttachSound(GFxVideoCrySound* pSound)
{
	const char* threadName = "CryGFxVideo_SoundUpdate";
	assert(pSound);

	GLock::Locker lock(&m_soundsLock);

	m_sounds.PushBack(pSound);

	if (!m_pUpdateThread)
	{
		m_stopThread = false;
		IThreadConfigManager* pThreadConfigMngr = gEnv->pThreadManager->GetThreadConfigManager();
		const SThreadConfig* pThreadConfig = pThreadConfigMngr->GetThreadConfig(threadName);

		// Decoder not present in config
		if (pThreadConfig == pThreadConfigMngr->GetDefaultThreadConfig())
			CryGFxLog::GetAccess().LogWarning("Unable to find thread config for '%s' . Using global default config.", threadName);

		GThread::CreateParams params(UpdateFunc, this, pThreadConfig->stackSizeBytes);
		params.threadName = threadName;
		params.processor = pThreadConfig->affinityFlag;
		params.priority = ConvertToGFxThreadPriority(pThreadConfig->priority);

		m_pUpdateThread = *GHEAP_NEW(m_pHeap) GThread(params);
		m_pUpdateThread->Start();
	}

	m_event.PulseEvent();
}

void GFxVideoCrySoundSystemImpl::DetachSound(GFxVideoCrySound* pSound)
{
	assert(pSound);
	GLock::Locker lock(&m_soundsLock);

	for (size_t i = 0; i < m_sounds.GetSize(); ++i)
	{
		if (m_sounds[i] == pSound)
		{
			m_sounds.RemoveAt(i);
			break;
		}
	}

	if (m_sounds.GetSize() == 0 && m_pUpdateThread)
	{
		m_stopThread = true;
		m_pUpdateThread = 0;
		m_event.PulseEvent();
	}
}

int GFxVideoCrySoundSystemImpl::UpdateFunc(GThread*, void* h)
{
	GFxVideoCrySoundSystemImpl* pThis = (GFxVideoCrySoundSystemImpl*) h;
	unsigned int waitTime = 2000; // initial wait will be interrupted by pulsed event sent by first AttachSound() call, i.e. won't cause issue with timer wrap around detection on Xbox 360
	while (true)
	{
		pThis->m_event.Wait(waitTime);
		if (pThis->m_stopThread)
			break;
		waitTime = (unsigned int) (pThis->UpdateAuxStreams() * 1000);
	}
	return 0;
}

float GFxVideoCrySoundSystemImpl::UpdateAuxStreams()
{
	GLock::Locker lock(&m_soundsLock);

	const bool muteMainTrack = m_pPlayerDelegate ? m_pPlayerDelegate->MuteMainTrack() : false;

	float nextCall = 0.5f; // should be small enough for timer wrap around detection on Xbox 360 (TickCycle / TicksPerSecond() seconds)
	for (size_t i = 0; i < m_sounds.GetSize(); ++i)
	{
		float t = m_sounds[i]->Update(muteMainTrack);
		if (t < nextCall)
			nextCall = t;
	}
	return nextCall;
}

//////////////////////////////////////////////////////////////////////////
// GFxVideoCrySound

GFxVideoCrySound::GFxVideoCrySound(GMemoryHeap* pHeap, GFxVideoCrySoundSystemImpl* pSoundSystem, GFxVideoPlayer::SoundTrack type)
	: GFxVideoSound()
	, m_pHeap(pHeap)
	, m_pSoundSystem(pSoundSystem)
	, m_pPlayerDelegate(/*pSoundSystem ? pSoundSystem->GetPlayerDelegate() :*/ 0)
	, m_pSound(0)
	, m_pChannel(0)
	, m_pPCMStream(0)
	, m_numChannels(0)
	, m_status(Sound_Stopped)
	, m_sampleRate(0)
	, m_volume(1.0f)
	, m_muted(false)
	, m_type(type)
	//, m_pChannelBuffer()
	, m_pInterleavedBuffer(0)
	, m_bufferSize(0)
	, m_bufferSamples(0)
	, m_soundLength(0)
	, m_fillPosition(0)
	, m_totalBytesRead(0)
	, m_startTick(0)
	, m_totalTicks(0)
	, m_starvedAtTick(0)
	, m_starved(false)
	, m_lock()
{
	assert(m_pSoundSystem);
	assert(m_pPlayerDelegate);

	for (size_t i = 0; i < MaxChannels; ++i)
		m_pChannelBuffer[i] = 0;
};

GFxVideoCrySound::~GFxVideoCrySound()
{
}

void GFxVideoCrySound::ReleaseBuffers()
{
	for (size_t ch = 0; ch < MaxChannels; ++ch)
	{
		if (m_pChannelBuffer[ch])
		{
			GHEAP_FREE(m_pHeap, m_pChannelBuffer[ch]);
			m_pChannelBuffer[ch] = 0;
		}
	}

	if (m_pInterleavedBuffer)
	{
		GHEAP_FREE(m_pHeap, m_pInterleavedBuffer);
		m_pInterleavedBuffer = 0;
	}

	m_bufferSize = 0;
	m_bufferSamples = 0;
}

void GFxVideoCrySound::UpdateVolume(bool muteMainTrack)
{
	const float newVolume = clamp_tpl(m_type == GFxVideoPlayer::MainTrack ? s_sys_flash_video_soundvolume : s_sys_flash_video_subaudiovolume, 0.0f, 1.0f);
	if (m_volume != newVolume)
	{
		m_volume = newVolume;
		m_pChannel->SetVolume(newVolume);
	}

	if (m_type == GFxVideoPlayer::MainTrack && m_muted != muteMainTrack)
	{
		m_muted = muteMainTrack;
		m_pChannel->Mute(muteMainTrack);
	}
}

float GFxVideoCrySound::Update(bool muteMainTrack)
{
	GLock::Locker lock(&m_lock);

	assert(m_pChannel);

	if (m_status != Sound_Playing)
		return 1.0f;

	UpdateVolume(muteMainTrack);

	const uint64 curTime = m_totalTicks + (VideoSoundTimer::GetTick() - m_startTick);
	const uint64 refillTime = GetTotalBytesReadInTicks();

	if (!m_starved && curTime > refillTime)
	{
		m_starvedAtTick = refillTime;
		m_starved = true;
	}

	unsigned int distToRefill = 0;
	if (!m_starved)
	{
		unsigned int curChannelPos = 0;
		bool ret = m_pChannel->GetBytePosition(curChannelPos);
		assert(ret);
		GUNUSED(ret);

		assert(curChannelPos <= m_soundLength);
		distToRefill = DistToFillBuffPos(curChannelPos);
	}

	if (distToRefill < m_bufferSize / 3)
	{
		if (m_starved)
			m_fillPosition = 0;

		const unsigned int recvBytes = ReadAndFillSound(true); // m_starved; // always refill to inject zeros to reduce clicks should we starve in the near future
		const uint64 newRefillTime = GetTotalBytesReadInTicks();
		const uint64 newCurTime = m_starved ? refillTime : curTime;
		#if defined(ENABLE_VIDEOSOUND_UPDATE_LOGGING)
		const bool prevStarved = m_starved;
		#endif
		if (m_starved)
		{
			if (recvBytes)
			{
				m_pChannel->SetBytePosition(0);
				m_totalTicks = m_starvedAtTick;
				m_startTick = VideoSoundTimer::GetTick();
				m_starved = false;
			}
			else
				ClearSoundBuffer();
		}

		if (recvBytes)
		{
			assert(newRefillTime >= newCurTime);
			const float t = max(VideoSoundTimer::TicksToSeconds(newRefillTime - newCurTime) * 2.0f / 3.0f, 0.015f);
		#if defined(ENABLE_VIDEOSOUND_UPDATE_LOGGING)
			gEnv->pLog->Log("Received %d bytes. Previously%s starved. %d bytes to refill pos. Wait %.2f ms...", recvBytes, prevStarved ? "" : " not", distToRefill, t * 1000.0f);
		#endif
			return t;
		}
		else
		{
			const float t = 0.015f;
		#if defined(ENABLE_VIDEOSOUND_UPDATE_LOGGING)
			gEnv->pLog->Log("%s. %d bytes to refill pos. Wait %.2f ms...", prevStarved ? "Still starving" : "Nothing received", distToRefill, t * 1000.0f);
		#endif
			return 0.015f;
		}
	}
	else
	{
		const float t = max(VideoSoundTimer::TicksToSeconds(refillTime - curTime) / 2.0f, 0.015f);
		#if defined(ENABLE_VIDEOSOUND_UPDATE_LOGGING)
		gEnv->pLog->Log("No request made. %d bytes to refill pos. Wait %.2f ms...", distToRefill, t * 1000.0f);
		#endif
		return t;
	}
}

bool GFxVideoCrySound::CreateOutput(UInt32 numChannels, UInt32 sampleRate)
{
	if (!numChannels || numChannels > MaxChannels)
		return false;

	GLock::Locker lock(&m_lock);

	m_numChannels = numChannels;
	m_sampleRate = sampleRate;
	m_soundLength = (sampleRate * AuxSoundLenMs / 1000) * numChannels * sizeof(SoundSampleType);

	m_bufferSamples = m_sampleRate * AuxSoundReadBufLenMs / 1000;
	m_bufferSize = m_bufferSamples * m_numChannels * sizeof(SoundSampleType);

	assert(!m_pInterleavedBuffer);
	m_pInterleavedBuffer = (unsigned char*) GHEAP_ALLOC(m_pHeap, m_bufferSize, GStat_Video_Mem);
	if (!m_pInterleavedBuffer)
	{
		ReleaseBuffers();
		return false;
	}

	for (size_t ch = 0; ch < m_numChannels; ++ch)
	{
		assert(!m_pChannelBuffer[ch]);
		m_pChannelBuffer[ch] = (SoundSampleType*) GHEAP_ALLOC(m_pHeap, m_bufferSamples * sizeof(SoundSampleType), GStat_Video_Mem);
		if (!m_pChannelBuffer[ch])
		{
			ReleaseBuffers();
			return false;
		}
	}

	m_pSound = m_pPlayerDelegate ? m_pPlayerDelegate->CreateSound(m_numChannels, m_sampleRate, m_soundLength) : 0;
	if (!m_pSound)
	{
		CryGFxLog::GetAccess().LogError("Failed to create video sound output. Video will not play! Is the dummy sound system active?");
		ReleaseBuffers();
		return false;
	}

	return true;
}

void GFxVideoCrySound::DestroyOutput()
{
	GLock::Locker lock(&m_lock);

	assert(!m_pChannel);
	SAFE_RELEASE(m_pSound);

	ReleaseBuffers();
}

void GFxVideoCrySound::Start(PCMStream* pStream)
{
	{
		GLock::Locker lock(&m_lock);

		assert(m_pSound);
		m_pChannel = m_pSound->Play();
		if (!m_pChannel)
		{
			SAFE_RELEASE(m_pSound);
			return;
		}

		if (m_type == GFxVideoPlayer::SubAudio)
			m_pChannel->SetSpeakerMix(0, 0, 1, 0, 0, 0, 0, 0);

		m_pPCMStream = pStream;
		m_status = Sound_Playing;
		m_totalBytesRead = 0;
		m_totalTicks = 0;
		m_fillPosition = 0;
		m_starved = false;
		m_starvedAtTick = 0;

		ReadAndFillSound(true);

		m_startTick = m_pSoundSystem->GetSafeStartTick();
	}
	m_pSoundSystem->AttachSound(this);
}

void GFxVideoCrySound::Stop()
{
	m_pSoundSystem->DetachSound(this);
	{
		GLock::Locker lock(&m_lock);

		if (m_pChannel)
			m_pChannel->Stop();

		m_status = Sound_Stopped;
		m_pPCMStream = 0;

		SAFE_RELEASE(m_pChannel);
		m_volume = 1.0f;
		m_muted = false;
	}
}

void GFxVideoCrySound::Pause(bool pause)
{
	GLock::Locker lock(&m_lock);

	bool pulseEvent = false;
	if (pause)
	{
		if (m_status == Sound_Playing)
		{
			m_status = Sound_Paused;
			m_totalTicks += VideoSoundTimer::GetTick() - m_startTick;
		}
	}
	else
	{
		if (m_status == Sound_Paused)
		{
			m_status = Sound_Playing;
			m_startTick = VideoSoundTimer::GetTick();
			pulseEvent = true;
		}
	}

	if (m_pChannel)
	{
		m_pChannel->SetPaused(pause);
		if (pulseEvent)
			m_pSoundSystem->PulseEvent();
	}
}

void GFxVideoCrySound::GetTime(UInt64* pCount, UInt64* pUnit) const
{
	GLock::Locker lock(&m_lock);

	assert(pCount);
	if (!m_starved)
	{
		uint64 curTimeInTicks = m_totalTicks;
		if (m_status == Sound_Playing)
			curTimeInTicks += VideoSoundTimer::GetTick() - m_startTick;
		*pCount = curTimeInTicks;
	}
	else
		*pCount = m_starvedAtTick;

	assert(pUnit);
	*pUnit = VideoSoundTimer::TicksPerSecond();
}

void GFxVideoCrySound::ClearSoundBuffer()
{
	assert(m_pSound);

	ISoundDelegate::LockRange lr = { 0 };
	if (m_pSound->Lock(0, m_soundLength, lr))
	{
		assert(lr.p0);
		assert(lr.length0 == m_soundLength);
		GMemUtil::Set(lr.p0, 0, lr.length0);
		bool ret = m_pSound->Unlock(lr);
		assert(ret);
		GUNUSED(ret);
	}
}

unsigned int GFxVideoCrySound::ReadAndFillSound(bool fillup)
{
	assert(m_pSound);
	assert(m_pInterleavedBuffer);

	const unsigned int recvBytes = GetPCMData();
	if (fillup && recvBytes < m_bufferSize)
		GMemUtil::Set(&m_pInterleavedBuffer[recvBytes], 0, m_bufferSize - recvBytes);

	const unsigned int copySize = !fillup ? recvBytes : m_bufferSize;
	if (copySize > 0)
	{
		ISoundDelegate::LockRange lr = { 0 };
		if (m_pSound->Lock(m_fillPosition, copySize, lr))
		{
			assert(lr.p0);
			assert(lr.length0 + lr.length1 == copySize);
			GMemUtil::Copy(lr.p0, m_pInterleavedBuffer, lr.length0);
			if (copySize > lr.length0)
			{
				assert(lr.p1);
				GMemUtil::Copy(lr.p1, m_pInterleavedBuffer + lr.length0, lr.length1);
			}

			bool ret = m_pSound->Unlock(lr);
			assert(ret);
			GUNUSED(ret);
		}
	}

	m_totalBytesRead += recvBytes;
	m_fillPosition += recvBytes;
	if (m_fillPosition >= m_soundLength)
		m_fillPosition -= m_soundLength;

	return recvBytes;
}

unsigned int GFxVideoCrySound::GetPCMData()
{
	const unsigned int numChannels = m_numChannels;
	const unsigned int recvNumSamples = m_pPCMStream->GetDataSInt16(numChannels, m_pChannelBuffer, m_bufferSamples);

	static const unsigned int chMap[MaxChannels] = { 0, 1, 4, 5, 2, 3, 6, 7 };

	for (unsigned int ch = 0; ch < numChannels; ++ch)
	{
		SoundSampleType* pBuffer = (SoundSampleType*) m_pInterleavedBuffer;

		const SoundSampleType* pSrc = m_pChannelBuffer[chMap[ch]];
		SoundSampleType* pDst = &pBuffer[ch];

		for (unsigned int i = 0; i < recvNumSamples; ++i, ++pSrc, pDst += numChannels)
			*pDst = *pSrc;
	}

	return recvNumSamples * numChannels * sizeof(SoundSampleType);
}

//////////////////////////////////////////////////////////////////////////
// GFxVideoCrySoundSystem

GFxVideoCrySoundSystem::GFxVideoCrySoundSystem(GMemoryHeap* pHeap)
	: GFxVideoSoundSystem(pHeap)
	, m_pImpl(GHEAP_NEW(GetHeap()) GFxVideoCrySoundSystemImpl(GetHeap()))
{
}

GFxVideoCrySoundSystem::~GFxVideoCrySoundSystem()
{
	delete m_pImpl;
}

GFxVideoSound* GFxVideoCrySoundSystem::Create(GFxVideoPlayer::SoundTrack type)
{
	VideoSoundTimer::InitFrequency();
	return m_pImpl ? (GHEAP_NEW(GetHeap()) GFxVideoCrySound(GetHeap(), m_pImpl, type)) : 0;
}

	#endif // #if defined(USE_GFX_VIDEO)

void GFxVideoCrySoundSystem::InitCVars()
{
	#if defined(_DEBUG)
	{
		static bool s_init = false;
		assert(!s_init);
		s_init = true;
	}
	#endif

	REGISTER_CVAR2("sys_flash_video_soundvolume", &s_sys_flash_video_soundvolume, s_sys_flash_video_soundvolume, VF_NULL, "Sets volume of video's main sound track(0..1). Has no effect if video support is not compiled in.");
	REGISTER_CVAR2("sys_flash_video_subaudiovolume", &s_sys_flash_video_subaudiovolume, s_sys_flash_video_subaudiovolume, VF_NULL, "Sets volume of video's sub audio sound track (0..1). Has no effect if video support is not compiled in.");
}

#endif   //#ifdef INCLUDE_SCALEFORM_SDK
