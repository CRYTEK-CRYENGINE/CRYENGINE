// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VOICELISTENER_H__
#define __VOICELISTENER_H__

#pragma once

#ifndef OLD_VOICE_SYSTEM_DEPRECATED

	#include "IGameObject.h"

struct ISound;

class CGameContext;

class CVoiceListener : public CGameObjectExtensionHelper<CVoiceListener, IGameObjectExtension>, public INetworkSoundListener, public ISoundEventListener
{
public:
	CVoiceListener();
	~CVoiceListener();
	// IGameObjectExtension
	virtual bool                 Init(IGameObject* pGameObject);
	virtual void                 PostInit(IGameObject* pGameObject);
	virtual void                 InitClient(int channelId);
	virtual void                 PostInitClient(int channelId);
	virtual void                 Release();
	virtual void                 FullSerialize(TSerialize ser)                                                 {}
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) { return true; }
	virtual void                 PostSerialize()                                                               {}
	virtual void                 SerializeSpawnInfo(TSerialize ser)                                            {}
	virtual ISerializableInfoPtr GetSpawnInfo()                                                                { return 0; }
	virtual void                 Update(SEntityUpdateContext& ctx, int updateSlot);
	virtual void                 HandleEvent(const SGameObjectEvent&);
	virtual void                 ProcessEvent(const SEntityEvent&);
	virtual void                 SetChannelId(uint16 id) {};
	virtual void                 PostUpdate(float frameTime);
	virtual void                 PostRemoteSpawn() {};
	virtual void                 GetMemoryUsage(ICrySizer* s)
	{
		s->Add(*this);
		s->AddObject(m_testData);
	}
	// ~IGameObjectExtension

	// INetworkSoundListener
	virtual bool FillDataBuffer(unsigned int nBitsPerSample, unsigned int nSamplesPerSecond, unsigned int nNumSamples, void* pData);
	virtual void OnActivate(bool active);
	virtual void SetSoundPlaybackDistances(float max3dDistance, float min2dDistance);
	virtual void UpdateSound3dPan();
	// ~INetworkSoundListener

	// ISoundEventListener
	virtual void OnSoundEvent(ESoundCallbackEvent event, ISound* pSound);
	// ~ISoundEventListener

private:
	_smart_ptr<IVoiceContext> m_pVoiceContext;
	_smart_ptr<ISound>        m_pSound;
	std::vector<int16>        m_testData;
	float                     m_max3dDistance;
	float                     m_min2dDistance;
	float                     m_3dPan;

	void StartPlaying(bool checkStarted);
};

#endif

#endif
