// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   A basic speaker manager, which at the moment displays the text ingame
   and executes a audio trigger, if existing. Can also check if the line has finished

   /************************************************************************/

#pragma once

#include <CryString/HashedString.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>

struct ICVar;

namespace CryDRS
{
class CVariable;
class CResponseActor;
class CDialogLine;

class CDefaultLipsyncProvider final : public DRS::ISpeakerManager::ILipsyncProvider
{
	//for now, our default lipsync provider just starts a animation when a line starts, and stops it when the line finishes
public:
	CDefaultLipsyncProvider();
	~CDefaultLipsyncProvider();

	//////////////////////////////////////////////////////////
	// ISpeakerManager::ILipsyncProvider implementation
	virtual DRS::LipSyncID OnLineStarted(DRS::IResponseActor* pSpeaker, const DRS::IDialogLine* pLine) override;
	virtual void           OnLineEnded(DRS::LipSyncID lipsyncId, DRS::IResponseActor* pSpeaker, const DRS::IDialogLine* pLine) override;
	virtual bool           Update(DRS::LipSyncID lipsyncId, DRS::IResponseActor* pSpeaker, const DRS::IDialogLine* pLine) override;
	//////////////////////////////////////////////////////////

protected:
	int    m_lipsyncAnimationLayer;
	float  m_lipsyncTransitionTime;
	string m_defaultLipsyncAnimationName;
	ICVar* pDefaultLipsyncAnimationNameVariable;
};

class CSpeakerManager final : public DRS::ISpeakerManager
{
public:
	CSpeakerManager();
	~CSpeakerManager();

	void Init();
	void Reset();
	void Update();

	void OnActorRemoved(const CResponseActor* pActor);

	//////////////////////////////////////////////////////////
	// ISpeakerManager implementation
	virtual bool IsSpeaking(const DRS::IResponseActor* pActor, const CHashedString& lineID = CHashedString::GetEmpty()) const override;
	virtual bool StartSpeaking(DRS::IResponseActor* pActor, const CHashedString& lineID) override;
	virtual bool CancelSpeaking(const DRS::IResponseActor* pActor, int maxPrioToCancel = -1) override;
	virtual bool AddListener(DRS::ISpeakerManager::IListener* pListener) override;
	virtual bool RemoveListener(DRS::ISpeakerManager::IListener* pListener) override;
	virtual void SetCustomLipsyncProvider(DRS::ISpeakerManager::ILipsyncProvider* pProvider) override;
	//////////////////////////////////////////////////////////////////////////

private:
	enum EEndingConditions
	{
		eEC_Done                   = 0,
		eEC_WaitingForStartTrigger = BIT(0),
		eEC_WaitingForStopTrigger  = BIT(1),
		eEC_WaitingForTimer        = BIT(2),
		eEC_WaitingForLipsync      = BIT(3),
	};

	struct SSpeakInfo
	{
		CResponseActor*    pActor;
		string             text;
		CHashedString      lineID;
		const CDialogLine* pPickedLine;
		float              finishTime;
		int                priority;

		int                voiceAttachmentIndex; //cached index of the voice attachment index
		AudioProxyId       speechAuxProxy;
		AudioControlId     startTriggerID;
		AudioControlId     stopTriggerID;
		string             standaloneFile;

		uint32             endingConditions; //EEndingConditions
		DRS::LipSyncID     lipsyncId;
	};

	struct SWaitingInfo
	{
		SSpeakInfo*     pSpeechThatWeWaitingFor;
		CResponseActor* pActor;
		CHashedString   lineID;
	};

	void        UpdateAudioProxyPosition(IEntity* pEntity, const SSpeakInfo& newSpeakerInfo);
	void        ReleaseSpeakerAudioProxy(SSpeakInfo& speakerInfo, bool stopTrigger);
	static void OnAudioCallback(SAudioRequestInfo const* const pAudioRequestInfo);

	void        InformListener(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID, DRS::ISpeakerManager::IListener::eLineEvent event, const CDialogLine* pLine);
	void        SetNumActiveSpeaker(int newAmountOfSpeaker);

	typedef std::vector<SSpeakInfo> SpeakerList;
	SpeakerList m_activeSpeakers;

	typedef std::vector<SWaitingInfo> QueuedSpeakerList;
	QueuedSpeakerList m_queuedSpeakers;

	typedef std::vector<DRS::ISpeakerManager::IListener*> ListenerList;
	ListenerList                            m_listeners;

	DRS::ISpeakerManager::ILipsyncProvider* m_pLipsyncProvider;
	CDefaultLipsyncProvider*                m_pDefaultLipsyncProvider;

	int            m_numActiveSpeaker;
	CVariable*     m_pActiveSpeakerVariable;
	AudioControlId m_audioRtpcIdLocal;
	AudioControlId m_audioRtpcIdGlobal;

	// CVars
	int    m_displaySubtitlesCVar;
	int    m_playAudioCVar;
	int    m_samePrioCancelsLinesCVar;
	ICVar* m_pDrsDialogDialogRunningEntityRtpcName;
	ICVar* m_pDrsDialogDialogRunningGlobalRtpcName;
};
}  //namespace CryDRS
