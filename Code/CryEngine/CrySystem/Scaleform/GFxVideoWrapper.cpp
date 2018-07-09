// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef INCLUDE_SCALEFORM_SDK

	#include "GFxVideoWrapper.h"

	#if defined(USE_GFX_VIDEO)

		#include "SharedStates.h"
		#include "SharedResources.h"
		#include "GTexture_Impl.h"
		#include <GFxLoader.h>
		#if CRY_PLATFORM_WINDOWS
			#include <GFxVideoPC.h>
		#elif CRY_PLATFORM_DURANGO
			#include <GFxVideoDurango.h>
		#elif CRY_PLATFORM_ORBIS
			#include <GfxVideoOrbis.h>
		#endif
		#include "GFxVideoSoundCrySound.h"
		#include "System.h"

		#include <CryThreading/IThreadManager.h>
		#include <CryThreading/IThreadConfigManager.h>

static float s_sys_flash_video_buffertime = 2.0f;
static float s_sys_flash_video_buffertime_loading = 2.0f;
static float s_sys_flash_video_buffertime_startup = 15.0f;

		#if !defined(_RELEASE)
			#define ENABLE_GFX_VIDEO_ERROR_CALLBACK
		#endif

		#if defined(ENABLE_GFX_VIDEO_ERROR_CALLBACK)

			#ifdef __cplusplus
extern "C" {
			#endif

// necessary forward declarations of CriMovie error hooks (to avoid adding libs and headers, criErr_SetCallback is linked in via gfxvideo.lib/a)
typedef void (* CriErrCbFunc)(const char* errid, unsigned long p1, unsigned long p2, unsigned long* pArray);
void criErr_SetCallback(CriErrCbFunc);
const char* criErr_ConvertIdToMessage(const char* errid, unsigned long p1, unsigned long p2);

void        CriUserErrorCallback(const char* errid, unsigned long p1, unsigned long p2, unsigned long* pArray)
{
	const char* pMsg = criErr_ConvertIdToMessage(errid, p1, p2);
	CryGFxLog::GetAccess().LogError("<Video codec> %s", pMsg ? pMsg : "Unspecified error!");
			#if !defined(_RELEASE)
	ICVar* const pVar = gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_error_debugbreak") : nullptr;
	IF (pVar && pVar->GetIVal(), 0)
	{
		__debugbreak();
	}
			#endif
}

			#ifdef __cplusplus
}
			#endif

		#endif // #if defined(ENABLE_GFX_VIDEO_ERROR_CALLBACK)

//////////////////////////////////////////////////////////////////////////
// event listener for the GFxVideoPlayer wrapper below

class CryGFxVideoPlayer_SystemEventListener : public ISystemEventListener
{
public:
	enum EState
	{
		eState_Startup,
		eState_Loading,
		eState_InGame
	};

	// ISystemEventListener interface
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);

public:
	static void Register()
	{
		static bool s_registered = false;
		if (!s_registered)
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(&ms_inst, "CryGFxVideoPlayer_SystemEventListener");
			s_registered = true;
		}
	}

	static CryGFxVideoPlayer_SystemEventListener& GetAccess()
	{
		return ms_inst;
	}

public:
	EState GetLoadingState() const
	{
		return m_eLoadingState;
	}

	float GetBufferTime() const
	{
		switch (m_eLoadingState)
		{
		case eState_Startup:
			return s_sys_flash_video_buffertime_startup;
		case eState_Loading:
			return s_sys_flash_video_buffertime_loading;
		case eState_InGame:
			return s_sys_flash_video_buffertime;
		}

		return s_sys_flash_video_buffertime;
	}

private:
	static CryGFxVideoPlayer_SystemEventListener ms_inst;

private:
	CryGFxVideoPlayer_SystemEventListener()
		: m_eLoadingState(eState_Startup)
	{
	}

	virtual ~CryGFxVideoPlayer_SystemEventListener()
	{
	}

private:
	EState m_eLoadingState;
};

CryGFxVideoPlayer_SystemEventListener CryGFxVideoPlayer_SystemEventListener::ms_inst;

//////////////////////////////////////////////////////////////////////////
// GFxVideoPlayer wrapper

class CryGFxVideoPlayer : public GFxVideoPlayer
{
	virtual void Destroy()
	{
		m_pPlayer->Destroy();
		delete this;
	}

	bool DetectAudioVideoMismatch(const char* url) const
	{
		CryGFxLog& log = CryGFxLog::GetAccess();

		bool mismatch = false;

		VideoInfo vInfo;
		if (LoadVideoInfo(url, &vInfo, &CryGFxFileOpener::GetAccess()))
		{
			const float videoTime = vInfo.TotalFrames / ((float) vInfo.FrameRate / 1000.0f);

			for (size_t i = 0; i < vInfo.AudioTracks.GetSize(); ++i)
			{
				assert(vInfo.AudioTracks[i].SampleRate > 0);
				const float audioTime = vInfo.AudioTracks[i].TotalSamples / (float) vInfo.AudioTracks[i].SampleRate;
				if (fabsf(audioTime - videoTime) > 1.0f)
				{
					log.LogWarning("Length of channel %2d in audio stream : %.2f seconds", vInfo.AudioTracks[i].Index, audioTime);
					mismatch = true;
				}
			}

			if (mismatch)
			{
				log.LogWarning("Length of video stream               : %.2f seconds", videoTime);
				log.LogError("Length of audio and video streams mismatch. Video \"%s\" will not be played!", url);
			}
		}

		return mismatch;
	}

	virtual void Open(const char* url)
	{
		if (gEnv->pCryPak->FGetSize(url))
		{
			m_isValid = !DetectAudioVideoMismatch(url);
			m_isStereoVideo = IsStereoVideoFile(url);
			m_pPlayer->Open(url);
		#if defined(ENABLE_FLASH_INFO)
			if (!m_videoFilePath.IsEmpty() && m_videoFilePath.CompareNoCase(url) == 0)
				CryGFxLog::GetAccess().LogError("Re-opening video file \"%s\". Trying to loop? Please use AS \"loop\" flag instead which is more efficient!", url);
			m_videoFilePath = url;
			m_pPlayer->RenameVideoHeap(m_videoFilePath.ToCStr());
		#endif
		}
		else
			CryGFxLog::GetAccess().LogError("Video file \"%s\" not found!", url);
	}

	virtual Status GetStatus()
	{
		return m_isValid ? m_pPlayer->GetStatus() : Finished;
	}
	virtual void GetVideoInfo(VideoInfo* info)
	{
		m_pPlayer->GetVideoInfo(info);
	}
	virtual void Play()
	{
		m_pPlayer->Play();
	}
	virtual void Seek(UInt32 pos)
	{
		m_pPlayer->Seek(pos);
	}
	virtual void SetSubtitleChannel(SInt channel)
	{
		m_pPlayer->SetSubtitleChannel(channel);
	}
	virtual SInt GetSubtitleChannel()
	{
		return m_pPlayer->GetSubtitleChannel();
	}
	virtual UInt32 GetPosition()
	{
		return m_pPlayer->GetPosition();
	}

	virtual GTexture* CreateTexture(GRenderer* pRenderer, SInt* texture_width, SInt* texture_height)
	{
		GTexture* pTex = m_pPlayer->CreateTexture(pRenderer, texture_width, texture_height);
		if (pTex)
		{
			GTextureXRenderBase* pTexBase = static_cast<GTextureXRenderBase*>(pTex);
			assert(pTexBase->IsYUV());
			if (pTexBase->IsYUV())
			{
				GTextureXRenderYUV* pTexYUV = static_cast<GTextureXRenderYUV*>(pTex);
				pTexYUV->SetStereoContent(m_isStereoVideo);
			}
		}
		return pTex;
	}

	virtual void UpdateTexture(GTexture* pRenderer, char* subtitle_msg, SInt subtitle_mgs_len)
	{
		m_pPlayer->UpdateTexture(pRenderer, subtitle_msg, subtitle_mgs_len);
	}
	virtual bool IsTextureReady()
	{
		return m_pPlayer->IsTextureReady();
	}
	virtual void Decode()
	{
		#if defined ENABLE_FLASH_INFO
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Video Decode %s", m_videoFilePath.ToCStr());
		#endif
		m_pPlayer->Decode();
	}
	virtual void Stop()
	{
		m_pPlayer->Stop();
	}
	virtual void Pause(bool on_off)
	{
		m_pPlayer->Pause(on_off);
	}
	virtual void SetSyncObject(SyncObject* pSyncObj)
	{
		m_pPlayer->SetSyncObject(pSyncObj);
	}
	virtual void GetCurrentCuePoints(GArray<CuePoint>* cue_points)
	{
		m_pPlayer->GetCurrentCuePoints(cue_points);
	}
	virtual void SetAudioTrack(SInt track_index)
	{
		m_pPlayer->SetAudioTrack(track_index);
	}
	virtual void SetSubAudioTrack(SInt track_index)
	{
		m_pPlayer->SetSubAudioTrack(track_index);
	}
	virtual void ReplaceCenterVoice(SInt track_index)
	{
		m_pPlayer->ReplaceCenterVoice(track_index);
	}
	virtual void SetLoopFlag(bool flag)
	{
		m_pPlayer->SetLoopFlag(flag);
	}

	virtual void SetBufferTime(Float time)
	{
		time = CryGFxVideoPlayer_SystemEventListener::GetAccess().GetBufferTime();
		if (time < 0.5f)
			time = 0.5f;
		m_pPlayer->SetBufferTime(time);
	}

	virtual void SetNumberOfFramePools(UInt pools)
	{
		pools = 1;
		m_pPlayer->SetNumberOfFramePools(pools);
	}

	virtual void SetReloadThresholdTime(Float time)
	{
		time = CryGFxVideoPlayer_SystemEventListener::GetAccess().GetBufferTime();
		time /= 2.5f;
		if (time < 0.25f)
			time = 0.25f;
		m_pPlayer->SetReloadThresholdTime(time);
	}
	virtual void GetReadBufferInfo(ReadBufferInfo* info)
	{
		m_pPlayer->GetReadBufferInfo(info);
	}
	virtual void SetDecodeHeaderTimeout(Float timeout)
	{
		m_pPlayer->SetDecodeHeaderTimeout(timeout);
	}
	virtual void RenameVideoHeap(const char* pName)
	{
		m_pPlayer->RenameVideoHeap(pName);
	}

public:
	CryGFxVideoPlayer(GFxVideoPlayer* pPlayer)
		: m_pPlayer(pPlayer)
		, m_isStereoVideo(false)
		, m_isValid(false)
		#if defined ENABLE_FLASH_INFO
		, m_videoFilePath()
		#endif
	{
		assert(m_pPlayer);
		CryInterlockedIncrement(&ms_numInstances);
	}
	virtual ~CryGFxVideoPlayer()
	{
		CryInterlockedDecrement(&ms_numInstances);
	}

	static int GetNumInstances()
	{
		return ms_numInstances;
	}

protected:
	static volatile int ms_numInstances;

protected:
	GFxVideoPlayer* m_pPlayer;
	bool            m_isStereoVideo;
	bool            m_isValid;
		#if defined ENABLE_FLASH_INFO
	GString         m_videoFilePath;
		#endif
};

volatile int CryGFxVideoPlayer::ms_numInstances = 0;

//////////////////////////////////////////////////////////////////////////
// platform specific GFxVideo wrappers

		#if CRY_PLATFORM_WINDOWS
class CryGFxVideoPC : public GFxVideoPC
{
public:
	CryGFxVideoPC(UInt numDecodingThreads, GThread::ThreadPriority decodeThreadsPriority)
		: GFxVideoPC(numDecodingThreads, decodeThreadsPriority)
	{
	}

	virtual GFxVideoPlayer* CreateVideoPlayer(GMemoryHeap* pHeap, GFxTaskManager* pTaskManager, GFxFileOpenerBase* pFileOpener, GFxLog* pLog)
	{
		CryGFxVideoPlayer* pPlayerWrapped = 0;
		GFxVideoPlayer* pPlayer = GFxVideoPC::CreateVideoPlayer(pHeap, pTaskManager, pFileOpener, pLog);
		if (pPlayer)
			pPlayerWrapped = new CryGFxVideoPlayer(pPlayer);
		return pPlayerWrapped;
	}

	void PauseVideoDecoding(bool pause)
	{
		if (pDecoder)
			pDecoder->PauseDecoding(pause);
	}
};
		#elif CRY_PLATFORM_DURANGO
class CryGFxVideoDurango : public GFxVideoDurango
{
public:
	CryGFxVideoDurango(UInt decodingProcs, GThread::ThreadPriority decodingThreadsPriority, GFxDurangoProc readerProc)
		: GFxVideoDurango(decodingProcs, decodingThreadsPriority, readerProc)
	{
	}

	virtual GFxVideoPlayer* CreateVideoPlayer(GMemoryHeap* pHeap, GFxTaskManager* pTaskManager, GFxFileOpenerBase* pFileOpener, GFxLog* pLog)
	{
		CryGFxVideoPlayer* pPlayerWrapped = 0;
		GFxVideoPlayer* pPlayer = GFxVideo::CreateVideoPlayer(pHeap, pTaskManager, pFileOpener, pLog);
		if (pPlayer)
			pPlayerWrapped = new CryGFxVideoPlayer(pPlayer);
		return pPlayerWrapped;
	}

	void PauseVideoDecoding(bool pause)
	{
		if (pDecoder)
			pDecoder->PauseDecoding(pause);
	}
};
		#elif CRY_PLATFORM_ORBIS
class CryGFxVideoOrbis : public GFxVideoOrbis
{
public:
	CryGFxVideoOrbis(UInt decodingProcs, GThread::ThreadPriority decodingThreadsPriority, GFxOrbisProc readerProc)
		: GFxVideoOrbis(decodingProcs, decodingThreadsPriority, readerProc)
	{
	}

	virtual GFxVideoPlayer* CreateVideoPlayer(GMemoryHeap* pHeap, GFxTaskManager* pTaskManager, GFxFileOpenerBase* pFileOpener, GFxLog* pLog)
	{
		CryGFxVideoPlayer* pPlayerWrapped = 0;
		GFxVideoPlayer* pPlayer = GFxVideo::CreateVideoPlayer(pHeap, pTaskManager, pFileOpener, pLog);
		if (pPlayer)
			pPlayerWrapped = new CryGFxVideoPlayer(pPlayer);
		return pPlayerWrapped;
	}

	void PauseVideoDecoding(bool pause)
	{
		if (pDecoder)
			pDecoder->PauseDecoding(pause);
	}
};
		#endif

void GFxVideoWrapper::SetVideo(GFxLoader* pLoader)
{
	assert(pLoader);

	GPtr<GFxVideo> pvc = 0;
	GPtr<GFxVideoSoundSystem> pvs = 0;

	IThreadConfigManager* pThreadConfigMngr = gEnv->pThreadManager->GetThreadConfigManager();

		#if CRY_PLATFORM_WINDOWS
	const char* sDecoderThreadName = "GFxVideo_Decoder";
	GThread::ThreadPriority threadPrio = GThread::NormalPriority;

	// Collect number of threads to create
	int nNumDecoders = 0;
	while (true)
	{
		const SThreadConfig* pThreadConfigDecoder = pThreadConfigMngr->GetThreadConfig("%s_%d", sDecoderThreadName, nNumDecoders);

		// Decoder not present in config
		if (pThreadConfigDecoder == pThreadConfigMngr->GetDefaultThreadConfig())
			break;

		// First thread sets prio
		if (nNumDecoders == 0)
			threadPrio = ConvertToGFxThreadPriority(pThreadConfigDecoder->priority);

		++nNumDecoders;
	}

	if (nNumDecoders == 0)
	{
		CryGFxLog::GetAccess().LogWarning("Unable to find thread config for 'GFxVideo_Decoder_0'");
	}

	// Create threads
	pvc = *new CryGFxVideoPC(nNumDecoders, threadPrio);

		#elif defined(DURANGO) || defined(ORBIS)

			#if defined(DURANGO)
	UInt nDecoderThreadAffinity = static_cast<UInt>(GFxDurango_Proc4);
	UInt nReaderThreadAffinity = static_cast<UInt>(GFxDurango_Proc5);
			#elif defined(ORBIS)
	UInt nDecoderThreadAffinity = static_cast<UInt>(GFxOrbis_Proc4);
	UInt nReaderThreadAffinity = static_cast<UInt>(GFxOrbis_Proc5);
			#endif

	const char* sDecoderThreadName = "GFxVideo_Decoder";
	const char* sReaderThreadName = "GFxVideo_Reader";

	const SThreadConfig* pDefaultConfig = pThreadConfigMngr->GetDefaultThreadConfig();
	const SThreadConfig* pDecoderThreadConfig = pThreadConfigMngr->GetThreadConfig(sDecoderThreadName);
	const SThreadConfig* pReaderThreadConfig = pThreadConfigMngr->GetThreadConfig(sReaderThreadName);

	// Get decoder thread affinity
	if (pDecoderThreadConfig != pDefaultConfig)
		nDecoderThreadAffinity = static_cast<UInt>(pDecoderThreadConfig->affinityFlag);
	else
		CryGFxLog::GetAccess().LogWarning("Unable to find GFxVideo decoder thread config: '%s'.", sDecoderThreadName);

	// Get reader thread affinity
	if (pReaderThreadConfig != pDefaultConfig)
		nReaderThreadAffinity = static_cast<UInt>(pReaderThreadConfig->affinityFlag);
	else
		CryGFxLog::GetAccess().LogWarning("Unable to find GFxVideo reader thread config: '%s'.", sReaderThreadName);

	// Get decoder thread priority
	GThread::ThreadPriority nDecoderThreadPrio = ConvertToGFxThreadPriority(pDecoderThreadConfig->priority);

			#if defined(DURANGO)
	// For Durango GFx only supports a reader thread targeting a single CPU
	if (!(nReaderThreadAffinity && !(nReaderThreadAffinity & (nReaderThreadAffinity - 1)))) // Is not pow2
	{
		CryGFxLog::GetAccess().LogWarning("On Durango reader thread ('%s') affinity flag is only allowed to target a single CPU. [%u]", sReaderThreadName, nReaderThreadAffinity);
		nReaderThreadAffinity = static_cast<UInt>(GFxDurango_Proc5); //default
	}

	pvc = *new CryGFxVideoDurango(nDecoderThreadAffinity, nDecoderThreadPrio, static_cast<GFxDurangoProc>(nReaderThreadAffinity));
			#elif defined(ORBIS)
	// For Orbis GFx only supports a reader thread targeting a single CPU
	if (!(nReaderThreadAffinity && !(nReaderThreadAffinity & (nReaderThreadAffinity - 1)))) // Is not pow2
	{
		CryGFxLog::GetAccess().LogWarning("On Orbis reader thread ('%s') affinity flag is only allowed to target a single CPU. [%u]", sReaderThreadName, nReaderThreadAffinity);
		nReaderThreadAffinity = static_cast<UInt>(GFxOrbis_Proc5); //default
	}

	pvc = *new CryGFxVideoOrbis(nDecoderThreadAffinity, nDecoderThreadPrio, static_cast<GFxOrbisProc>(nReaderThreadAffinity));
			#endif
		#else
			#error "Unsupported platform"
		#endif

	if (pvc)
	{
		pvs = *new GFxVideoCrySoundSystem(pvc->GetHeap());
		pvc->SetSoundSystem(pvs);
	}
	pLoader->SetVideo(pvc);

	CryGFxVideoPlayer_SystemEventListener::Register();

		#if defined(ENABLE_GFX_VIDEO_ERROR_CALLBACK)
	criErr_SetCallback(CriUserErrorCallback);
		#endif
}

int GFxVideoWrapper::GetNumVideoInstances()
{
	return CryGFxVideoPlayer::GetNumInstances();
}

void GFxVideoWrapper::InitCVars()
{
		#if defined(_DEBUG)
	{
		static bool s_init = false;
		assert(!s_init);
		s_init = true;
	}
		#endif

	REGISTER_CVAR2("sys_flash_video_buffertime", &s_sys_flash_video_buffertime, s_sys_flash_video_buffertime, VF_NULL, "Sets buffer time for videos (in seconds)");
	REGISTER_CVAR2("sys_flash_video_buffertime_loading", &s_sys_flash_video_buffertime_loading, s_sys_flash_video_buffertime_loading, VF_NULL, "Sets buffer time for videos during loading (in seconds)");
	REGISTER_CVAR2("sys_flash_video_buffertime_startup", &s_sys_flash_video_buffertime_startup, s_sys_flash_video_buffertime_startup, VF_NULL, "Sets buffer time for videos during startup (in seconds)");
}

void CryGFxVideoPlayer_SystemEventListener::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_GAME_POST_INIT_DONE:
		m_eLoadingState = eState_InGame;
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_PREPARE:
		m_eLoadingState = eState_Loading;
		break;
	case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
		m_eLoadingState = eState_InGame;
		break;
	case ESYSTEM_EVENT_CHANGE_FOCUS:
		{
			if (gEnv->pSystem->IsQuitting())
				return;

			GFxVideoBase* videoBase = CSharedFlashPlayerResources::GetAccess().GetLoader(true)->GetVideo();

		#if CRY_PLATFORM_WINDOWS
			CryGFxVideoPC* video = (CryGFxVideoPC*)videoBase;
		#elif CRY_PLATFORM_DURANGO
			CryGFxVideoDurango* video = (CryGFxVideoDurango*)videoBase;
		#elif CRY_PLATFORM_ORBIS
			CryGFxVideoOrbis* video = (CryGFxVideoOrbis*)videoBase;
		#endif

		#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
			if (video)
				video->PauseVideoDecoding(wparam ? false : true);
		#endif
		}
		break;
	default:
		break;
	}
}

	#endif // #if defined(USE_GFX_VIDEO)

#endif   //#ifdef INCLUDE_SCALEFORM_SDK
