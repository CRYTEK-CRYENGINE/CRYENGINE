// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CommunicationManager_h__
#define __CommunicationManager_h__

#pragma once

#include "Communication.h"
#include "CommunicationChannelManager.h"
#include "CommunicationPlayer.h"
#include "CommunicationTestManager.h"

#include <CryAISystem/ICommunicationManager.h>
#include <CryCore/Containers/CryListenerSet.h>

#include <CryCore/StlUtils.h>

/**

   The CommunicationManager acts as the heart of the communication system.
   Communication can be used to give the sense of AI reacting and interacting with each other.
   Currently the system support sound and animation based communication.

   System overview:

   The CommunicationManager is a global singleton managing:

 * The map of playing communications (CommPlayID : PlayingCommunication).
 * An ordered queue of pending communications (CommPlayID & SCommunicationRequest).
 * An unordered queue of pending communications (CommPlayID & SCommunicationRequest).
 * A set of communication configurations:
 * Used to define the communication events for a particular AI.
 * Defined in Scripts/AI/Communication/BasicCommunications.xml
 * A set of communication channels controlled by CommunicationChannelManager:
 * Used to control and limit when AI can use play certain communications.
 * Helps manage excess noise and repetition.
 * Defined in Scripts/AI/Communication/ChannelConfig.xml.
 * An OnCommunicationFinished() callback to detect finished communications.
 * A CommunicationPlayer instance...

   The CommunicationPlayer is owned by the CommunicationManager singleton and manages:

 * Tracking a map of playing communication state (CommPlayID : PlayState).
 * Starting the playing of communications via CommunicationHandler(s) (owned by AIProxy(s)).
 * Callbacks from the CommunicationHandler(s) via PlayState::OnCommunicationHandlerEvent().
 * Detects finished playback (based on PlayState finish flags) during update.
 * Informs CommunicationManager via listener of finished communications.

   The CommunicationHandler is created with each AIProxy and is responsible for:

 * Dispatching the play sound/animation requests to the relivant systems.
 * Listening for sound events by implementing ISoundEventListener.
 * Listening for animation events by implementing IAnimationgGraphStateListener.
 * Translating sound/animation stopped/failed/cancelled events in to ECommunicationHandlerEvents.
 * Informing the CommunicationPlayer of finished events.

 */

class CCommunicationManager
	: public CommunicationPlayer::ICommunicationFinishedListener
	  , public ICommunicationManager
{
public:

	enum
	{
		MaxVariationCount = 32,
	};

public:
	CCommunicationManager(const char* configurationFileName = NULL);

	// ICommunicationManager
	virtual uint32        GetConfigCount() const override;
	virtual const char*   GetConfigName(uint32 index) const override;
	virtual CommConfigID  GetConfigIDByIndex(uint32 index) const override;

	virtual CommChannelID GetChannelID(const char* name) const override;
	virtual CommConfigID  GetConfigID(const char* name) const override;
	virtual CommID        GetCommunicationID(const char* name) const override;
	virtual const char*   GetCommunicationName(const CommID& communicationID) const override;

	virtual bool          CanCommunicationPlay(const SCommunicationRequest& request, float* estimatedWaitTime = 0) override;
	virtual CommPlayID    PlayCommunication(SCommunicationRequest& request) override;
	virtual void          StopCommunication(const CommPlayID& playID) override;
	virtual bool          IsPlaying(const CommPlayID& playID, float* timeRemaining = 0) const override;
	virtual bool          IsQueued(const CommPlayID& playID, float* estimatedWaitTime = 0) const override;

	virtual void          RegisterListener(ICommGlobalListener* eventListener, const char* name = NULL) override { m_globalListeners.Add(eventListener, name); }
	virtual void          UnregisterListener(ICommGlobalListener* eventListener) override                        { m_globalListeners.Remove(eventListener); }
	virtual void          RemoveInstanceListener(const CommPlayID& playID) override;

	virtual uint32        GetConfigCommunicationCount(const CommConfigID& configID) const override;
	virtual const char*   GetConfigCommunicationNameByIndex(const CommConfigID& configID, uint32 index) const override;

	//Sets silence durations for actors, exluding them from communication sounds/animations for the length of the duration.
	virtual void                      SetRestrictedDuration(EntityId actorId, float voiceDuration, float animDuration) override;
	//Adds restriction on actor, excluding them from communiction sounds/animations until explicityly removed.
	virtual void                      AddActorRestriction(EntityId actorId, bool restrictVoice, bool restrictAnimation) override;
	//Removes restriction on actor, if no more restrictions are present actor will be available for communication sounds/animations.
	virtual void                      RemoveActorRestriction(EntityId actorId, bool unrestrictVoice, bool unrestrictAnimation) override;

	virtual const WWiseConfiguration& GetWiseConfiguration() const override { return m_wiseConfiguration; }

	// Set the specified new value to the specified variable
	virtual void SetVariableValue(const char* variableName, const bool newVariableValue) override;
	virtual void GetVariablesNames(const char** variableNames, const size_t maxSize, size_t& actualSize) const override;
	//~ICommunicationManager

	size_t GetPriority(const CommPlayID& playID) const;

	void   Reload();
	void   LoadConfigurationAndScanRootFolder(const char* rootFolder);

	void   Reset();
	void   ResetHistory();
	void   Update(float updateTime);

	void   DebugDraw();

	/// Returns boolean indicating if the queried request can be forcibly played, even if channel is occupied.
	bool CanForceAnimation(const SCommunication& comm, const SCommunicationRequest& request);

	// ICommunicationFinishedListener
	/// playID is the id of the communication to stop.
	/// StateFlags contains whether the components of the communication are finished or not (animation, sound, etc).
	virtual void OnCommunicationFinished(const CommPlayID& playID, uint8 stateFlags) override;
	//~ICommunicationFinishedListener

	void WriteStatistics();
	void ResetStatistics() { m_debugStatisticsMap.clear(); }

	typedef std::map<CommID, SCommunication> Communications;
	struct CommunicationsConfig
	{
		string         name;
		Communications comms;
	};

	const CommunicationsConfig& GetConfig(const CommConfigID& configID) const;

	CommunicationTestManager&   GetTestManager();

private:
	void ScanFolder(const char* folderName);

	bool LoadCommunicationSettingsXML(const char* fileName);
	bool LoadCommunication(const XmlNodeRef& commNode, SCommunication& comm);
	bool LoadVariation(const XmlNodeRef& varNode, SCommunicationVariation& variation);
	bool LoadVariable(const XmlNodeRef& varNode, Variables::Collection& variable);
	void LoadVariables(const XmlNodeRef& rootNode, const char* fileName);
	void LoadGlobalConfiguration(const XmlNodeRef& rootNode);

	bool ChooseVariation(const SCommunication& comm, const SCommunication::EVariationChoiceMethod method, const uint32 index, uint32& outputIndex) const;
	bool CanVariationBePlayed(const SCommunication& comm, const uint32 variationIndex) const;
	void UpdateHistory(SCommunication& comm, uint32 variationPlayed);

	bool Play(const CommPlayID& playID, SCommunicationRequest& request,
	          const CommunicationChannel::Ptr& channel, SCommunication& comm);

	struct QueuedCommunication
	{
		CommPlayID            playID;
		SCommunicationRequest request;
		float                 age;
	};
	typedef std::list<QueuedCommunication> QueuedCommunications;

	void FlushQueue(QueuedCommunications& queue, const CommunicationChannel::Ptr& channel, EntityId targetId);
	void Queue(QueuedCommunications& queue, const CommPlayID& playID, const SCommunicationRequest& request);
	bool RemoveFromQueue(QueuedCommunications& queue, const CommPlayID& playID);
	void UpdateQueue(QueuedCommunications& queue, float updateTime);
	void ProcessQueues();
	bool PlayFromQueue(QueuedCommunication& queued);

	//Clear out lower priority playing communications, and clean up lower priority unfinalized communications
	void FlushCommunications(const SCommunicationRequest& request, CommunicationChannel::Ptr channel);

	bool CheckAnimationGraphState(EntityId actorId, const string& name);
	void SendAnimationGraphReset(EntityId entityId);

	void UpdateStateOfInternalVariables(const SCommunicationRequest& request);
	void ResetStateOfInternalVariables();
	void UpdateStateOfTargetAboveBelowVariables(const Vec3& entityPos, const Vec3& targetPos);

	string m_rootFolder;
	string m_configurationFileName;

	typedef std::map<CommConfigID, CommunicationsConfig> Configs;
	Configs                 m_configs;

	Variables::Declarations m_variablesDeclaration;
	Variables::Collection   m_variables;

	struct PlayingCommunication
	{
		PlayingCommunication() : actorID(0), eventListener(NULL), skipSound(false), minSilence(-1.0f), startTime(-1.0f){};
		PlayingCommunication(const PlayingCommunication& comm) : actorID(comm.actorID), channel(comm.channel), eventListener(NULL), animName(comm.animName), skipSound(comm.skipSound), minSilence(comm.minSilence), startTime(comm.startTime){}

		EntityId                                      actorID;
		CommunicationChannel::Ptr                     channel;
		ICommunicationManager::ICommInstanceListener* eventListener;
		string animName;
		bool   skipSound;
		float  minSilence;
		float  startTime;
	};

	typedef std::map<CommPlayID, PlayingCommunication> PlayingCommunications;
	PlayingCommunications m_playing;

	typedef std::vector<PlayingCommunication> CommunicationVec;
	/// Vector of entries that have stopped playing in the manager, but have outstanding animations.
	CommunicationVec            m_unfinalized;

	CommunicationChannelManager m_channels;
	CommunicationPlayer         m_player;

	//Map of actorIds to silence times. If an entry exists for an actor, he will not be able to play new communications until the silence timer runs out.
	typedef std::map<EntityId, SRestrictedActorParams> TRestrictedActorMap;
	TRestrictedActorMap m_restrictedActors;

	//Object to store entity properties related to readability
	struct SActorReadabilitySettings
	{
		SActorReadabilitySettings() : m_bIgnoreAnimations(false), m_bIgnoreVoice(false) {}

		bool m_bIgnoreAnimations;
		bool m_bIgnoreVoice;
	};

	//Map of actorIds to entity properties related to readability
	typedef std::map<EntityId, SActorReadabilitySettings> TActorReadabilityProperties;

	TActorReadabilityProperties m_actorPropertiesMap;

	//Helper function to retrieve readability settings for an actor
	void GetActorData(EntityId actorId, SActorReadabilitySettings& actorReadabilitySettings);
	//Helper to retrieve table data for readability settings
	void ReadDataFromTable(const SmartScriptTable& ssTable, SActorReadabilitySettings& actorReadabilitySettings);

	//Updates duration based actor restrictions, and deletes restrictions that are no longer valid
	void UpdateActorRestrictions(float updateTime);
	bool IsVoiceRestricted(EntityId actorId);
	bool IsAnimationRestricted(EntityId actorId);

	QueuedCommunications       m_orderedQueue;
	QueuedCommunications       m_unorderedQueue;

	CommPlayID                 m_playGenID;
	mutable std::vector<uint8> m_randomPool;

	CommunicationTestManager   m_testManager;

	struct CommDebugEntry
	{
		CommDebugEntry() : m_id(CommPlayID()), m_displayed(false), m_timeTracked(0.0f) {}

		CommDebugEntry(const string& val, CommPlayID pid, const string& type)
			: m_commName(val), m_id(pid), m_type(type), m_displayed(false), m_timeTracked(0.0f) {}

		CommDebugEntry(const string& val, CommPlayID pid, const string& type, CommChannelID cid)
			: m_commName(val), m_id(pid), m_type(type), m_displayed(false), m_timeTracked(0.0f) { m_channels.insert(cid); }

		void UpdateChannelInfo(const CommDebugEntry& target) { if (!target.m_channels.empty()) m_channels.insert(*(target.m_channels.begin())); }

		string                  m_commName;
		CommPlayID              m_id;

		string                  m_type;

		std::set<CommChannelID> m_channels;

		bool                    m_displayed;
		float                   m_timeTracked;
	};

	typedef std::map<EntityId, std::vector<CommDebugEntry>> TDebugMapHistory;

	struct CommKeys
	{
		CommKeys(const CommConfigID& c1, const CommID& c2) : configId(c1), commId(c2) {}
		bool operator<(const CommKeys& right) const
		{
			return ((configId < right.configId) || (configId == right.configId && commId < right.commId));
		}
		CommConfigID configId;
		CommID       commId;
	};

	struct CommDebugCount
	{
		CommDebugCount() : requestCount(0), playCount(0) {}

		unsigned int requestCount;
		unsigned int playCount;
	};

	typedef std::map<CommKeys, CommDebugCount> TDebugMapStatistics;

	TDebugMapStatistics m_debugStatisticsMap;

	///Looks for an existing record in the debug history, and either inserts a new record if not found, or updates an existing one
	void UpdateDebugHistoryEntry(IEntity* pEntity, const CommDebugEntry& entryToUpdate);

	///Updates the time displayed of all tracked debug records
	void UpdateDebugHistory(float updateTime);

	///Checks for a matching entry based on separate possible criteria
	struct FindMatchingRecord
	{
		FindMatchingRecord(const CommDebugEntry& entry)
			: m_searchEntry(entry) {}

		bool operator()(const CommDebugEntry& entry) const
		{
			if (entry.m_commName == m_searchEntry.m_commName && entry.m_id == m_searchEntry.m_id) return true;
			if (m_searchEntry.m_timeTracked > 0.0f && entry.m_timeTracked > m_searchEntry.m_timeTracked) return true;
			return false;
		}

		CommDebugEntry m_searchEntry;
	};

	TDebugMapHistory m_debugHistoryMap;

	typedef CListenerSet<ICommGlobalListener*> TCommunicationListeners;
	TCommunicationListeners m_globalListeners;

	void UpdateGlobalListeners(ECommunicationEvent event, EntityId actorId, const CommID& commId)
	{
		for (TCommunicationListeners::Notifier notifier(m_globalListeners); notifier.IsValid(); notifier.Next())
			notifier->OnCommunicationEvent(event, actorId, commId);
	}

	void CullPlayingCommunications();

	static const char* m_kTargetIsAbove_VariableName;
	static const char* m_kTargetIsBelow_VariableName;

	WWiseConfiguration m_wiseConfiguration;
};

#endif // __CommunicationManager_h__
