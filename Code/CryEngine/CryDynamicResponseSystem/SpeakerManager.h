// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/HashedString.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include "../CryCommon/CryCore/Containers/CryListenerSet.h"

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
	virtual ~CDefaultLipsyncProvider() override;

	//////////////////////////////////////////////////////////////////////////
	// ISpeakerManager::ILipsyncProvider implementation
	virtual DRS::LipSyncID OnLineStarted(DRS::IResponseActor* pSpeaker, const DRS::IDialogLine* pLine) override;
	virtual void           OnLineEnded(DRS::LipSyncID lipsyncId, DRS::IResponseActor* pSpeaker, const DRS::IDialogLine* pLine) override;
	virtual bool           Update(DRS::LipSyncID lipsyncId, DRS::IResponseActor* pSpeaker, const DRS::IDialogLine* pLine) override;
	//////////////////////////////////////////////////////////////////////////

protected:
	int    m_lipsyncAnimationLayer;
	float  m_lipsyncTransitionTime;
	string m_defaultLipsyncAnimationName;
};

class CSpeakerManager final : public DRS::ISpeakerManager
{
public:
	CSpeakerManager();
	virtual ~CSpeakerManager() override;

	void Init();
	void Shutdown();
	void Reset();
	void Update();

	void OnActorRemoved(const CResponseActor* pActor);

	//////////////////////////////////////////////////////////////////////////
	// ISpeakerManager implementation
	virtual bool                  IsSpeaking(const DRS::IResponseActor* pActor, const CHashedString& lineID = CHashedString::GetEmpty(), bool bCheckQueuedLinesAsWell = false) const override;
	virtual IListener::eLineEvent StartSpeaking(DRS::IResponseActor* pActor, const CHashedString& lineID) override;
	virtual bool                  CancelSpeaking(const DRS::IResponseActor* pActor, int maxPrioToCancel = -1, const CHashedString& lineID = CHashedString::GetEmpty(), bool bCancelQueuedLines = true) override;
	virtual bool                  AddListener(DRS::ISpeakerManager::IListener* pListener) override;
	virtual bool                  RemoveListener(DRS::ISpeakerManager::IListener* pListener) override;
	virtual void                  SetCustomLipsyncProvider(DRS::ISpeakerManager::ILipsyncProvider* pProvider) override;
	//////////////////////////////////////////////////////////////////////////

private:
	enum EEndingConditions
	{
		eEC_Done                   = 0,
		eEC_WaitingForStartTrigger = BIT(0),
		eEC_WaitingForStopTrigger  = BIT(1),
		eEC_WaitingForTimer        = BIT(2),
		eEC_WaitingForLipsync      = BIT(3)
	};

	struct SSpeakInfo
	{
		SSpeakInfo() = default;
		SSpeakInfo(CryAudio::AuxObjectId auxAudioObjectId) : speechAuxObjectId(auxAudioObjectId), voiceAttachmentIndex(-1) {}

		CResponseActor*       pActor;
		IEntity*              pEntity;
		string                text;
		CHashedString         lineID;
		const CDialogLine*    pPickedLine;
		float                 finishTime;
		int                   priority;

		int                   voiceAttachmentIndex;      //cached index of the voice attachment index // -1 means invalid ID;
		CryAudio::AuxObjectId speechAuxObjectId;
		CryAudio::ControlId   startTriggerID;
		CryAudio::ControlId   stopTriggerID;
		string                standaloneFile;

		uint32                endingConditions;      //EEndingConditions
		DRS::LipSyncID        lipsyncId;
		bool                  bWasCanceled;
	};

	struct SWaitingInfo
	{
		CResponseActor* pActor;
		CHashedString   lineID;
		int             linePriority;
		float           waitEndTime;

		bool operator==(const SWaitingInfo& other) const { return pActor == other.pActor && lineID == other.lineID; }  //no need to check prio or endTime, since we only allow one instance of the same lineId per speaker
	};

	void        UpdateAudioProxyPosition(IEntity* pEntity, const SSpeakInfo& newSpeakerInfo);
	void        ReleaseSpeakerAudioProxy(SSpeakInfo& speakerInfo, bool stopTrigger);
	static void OnAudioCallback(CryAudio::SRequestInfo const* const pAudioRequestInfo);

	void        InformListener(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID, DRS::ISpeakerManager::IListener::eLineEvent event, const CDialogLine* pLine);
	bool        OnLineAboutToStart(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID);
	void        SetNumActiveSpeaker(int newAmountOfSpeaker);

	//the pure execution, without further checking if the line can be started (that should have happen before)
	void ExecuteStartSpeaking(SSpeakInfo* pSpeakerInfoToUse);

	void QueueLine(CResponseActor* pActor, const CHashedString& lineID, float maxQueueDuration, const int priority);

	typedef std::vector<SSpeakInfo> SpeakerList;
	SpeakerList m_activeSpeakers;

	typedef std::vector<SWaitingInfo> QueuedSpeakerList;
	QueuedSpeakerList m_queuedSpeakers;

	typedef CListenerSet<DRS::ISpeakerManager::IListener*> ListenerList;
	ListenerList                            m_listeners;

	DRS::ISpeakerManager::ILipsyncProvider* m_pLipsyncProvider;
	CDefaultLipsyncProvider*                m_pDefaultLipsyncProvider;

	int                                            m_numActiveSpeaker;
	CryAudio::ControlId                            m_audioParameterIdLocal;
	CryAudio::ControlId                            m_audioParameterIdGlobal;

	std::vector<std::pair<CResponseActor*, float>> m_recentlyFinishedSpeakers;

	// CVars
	int          m_displaySubtitlesCVar;
	int          m_playAudioCVar;
	int          m_samePrioCancelsLinesCVar;
	float        m_defaultMaxQueueTime;
	static float s_defaultPauseAfterLines;
	ICVar*       m_pDrsDialogDialogRunningEntityParameterName;
	ICVar*       m_pDrsDialogDialogRunningGlobalParameterName;
};
}  //namespace CryDRS
