// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CommunicationHandler_h__
#define __CommunicationHandler_h__

#pragma once

#include <CryAISystem/IAgent.h>

class CommunicationHandler
	: public IAICommunicationHandler
	  , public IAnimationGraphStateListener
{
	friend class CommunicationVoiceTestManager;
public:
	CommunicationHandler(CAIProxy& proxy, IEntity* entity);
	virtual ~CommunicationHandler();

	// IAICommunicationHandler
	virtual SCommunicationSound PlaySound(CommPlayID playID, const char* name, IEventListener* listener = 0) override;
	virtual void                StopSound(const SCommunicationSound& soundToStop) override;

	virtual SCommunicationSound PlayVoice(CommPlayID playID, const char* variationName, IEventListener* listener = 0) override;
	virtual void                StopVoice(const SCommunicationSound& voiceToStop) override;

	virtual void                SendDialogueRequest(CommPlayID playID, const char* name, IEventListener* listener = 0) override {}

	virtual void                PlayAnimation(CommPlayID playID, const char* name, EAnimationMethod method, IEventListener* listener = 0) override;
	virtual void                StopAnimation(CommPlayID playID, const char* name, EAnimationMethod method) override;

	virtual bool                IsInAGState(const char* name) override;
	virtual void                ResetAnimationState() override;

	virtual bool                IsPlayingAnimation() const override;
	virtual bool                IsPlayingSound() const override;

	virtual void                OnSoundTriggerFinishedToPlay(CryAudio::ControlId const triggerId) override;
	//~IAICommunicationHandler

	// IAnimationgGraphStateListener
	virtual void SetOutput(const char* output, const char* value) override {};
	virtual void QueryComplete(TAnimationGraphQueryID queryID, bool succeeded) override;
	virtual void DestroyedState(IAnimationGraphState*) override;
	//~IAnimationgGraphStateListener

	void        Reset();
	void        OnReused(IEntity* entity);

	static void TriggerFinishedCallback(CryAudio::SRequestInfo const* const pAudioRequestInfo);

private:
	enum ESoundType
	{
		Sound = 0,
		Voice,
	};

	SCommunicationSound   PlaySound(CommPlayID playID, const char* name, ESoundType type, IEventListener* listener = 0);
	IAnimationGraphState* GetAGState();

	CAIProxy&             m_proxy;
	EntityId              m_entityId;
	IAnimationGraphState* m_agState;

	struct PlayingSound
	{
		PlayingSound()
			: listener(0)
			, type(Sound)
			, playID(0)
			, correspondingStopControlId(CryAudio::InvalidControlId)
		{
		}

		ESoundType type;
		IAICommunicationHandler::IEventListener* listener;
		//Index used to reference this event in listener. Set when sound event started
		CommPlayID                               playID;
		CryAudio::ControlId                      correspondingStopControlId;
	};

	struct PlayingAnimation
	{
		PlayingAnimation()
			: listener(0)
			, playing(false)
			, playID(0)
		{
		}

		IAICommunicationHandler::IEventListener* listener;
		string           name;

		EAnimationMethod method;
		bool             playing;
		//Index used to reference this event in listener. Set when animation event started
		CommPlayID       playID;
	};

	typedef std::map<TAnimationGraphQueryID, PlayingAnimation> PlayingAnimations;
	PlayingAnimations      m_playingAnimations;
	TAnimationGraphQueryID m_currentQueryID;  // because animation graph can send query result during SetInput,
	bool                   m_currentPlaying;  // before we had chance to insert in the map

	typedef std::map<CryAudio::ControlId, PlayingSound> PlayingSounds;
	PlayingSounds         m_playingSounds;

	AnimationGraphInputID m_signalInputID;
	AnimationGraphInputID m_actionInputID;
};

#endif
