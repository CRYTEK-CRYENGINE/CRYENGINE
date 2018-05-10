// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   This class is used to provide debug data, that can be displayed in the editor

   /************************************************************************/

#pragma once

#include "VariableValue.h"
#include "ConditionsCollection.h"

namespace DRS
{
struct IResponseActor;
}

namespace CryDRS
{
struct IResponseActionInstance;
class CResponseInstance;
class CResponseSegment;

class CResponseSystemDebugDataProvider final : public DRS::ISpeakerManager::IListener
{
public:
	typedef int ResponseInstanceID;

	enum EStatus
	{
		eER_NotStarted,
		eER_Running,
		eER_NoValidSegment,
		eER_Canceled,
		eER_Finished,
		eER_NoResponse,
		eER_Queued,
	};

	CResponseSystemDebugDataProvider();
	~CResponseSystemDebugDataProvider();

	//////////////////////////////////////////////////////////
	// ISpeakerListener implementation
	virtual void OnLineEvent(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID, eLineEvent lineEvent, const DRS::IDialogLine* pLine) override;
	//////////////////////////////////////////////////////////

	void Init();
	void Reset();

	//Variable Data
	void AddVariableSet(const string& variableName, const string& collectionName, const CVariableValue& oldValue, const CVariableValue& newValue, float timeOfChange);

	//Response Data
	void SetCurrentResponseInstance(CResponseInstance* pInstanceForCurrentID);    // tells the debugDataProvider which Response is the active one for all the add-x- methods
	void AddSignalFired(const string& signalName, const string& senderName, const string& contextVariables);
	void AddResponseStarted(const string& signalName);
	bool IncrementSegmentHierarchyLevel();
	bool AddResponseSegmentEvaluated(CResponseSegment* pResponseSegment);
	bool AddResponseSegmentStarted(CResponseSegment* pResponseSegment);
	bool AddResponseInstanceCreated(CResponseInstance* pInstanceForCurrentID);
	bool AddConditionChecked(const CConditionsCollection::SConditionInfo* pCondition, bool result);
	bool AddActionStarted(const string& actionDesc, DRS::IResponseActionInstance* pInstance, DRS::IResponseActor* pActor, CResponseSegment* pResponseSegment);
	bool AddActionFinished(DRS::IResponseActionInstance* pInstance);
	bool AddResponseInstanceFinished(EStatus reason);
	void GetRecentSignals(DynArray<const char*>& outResults, DRS::IResponseManager::eSignalFilter filter = DRS::IResponseManager::eSF_All);

	void SerializeRecentResponse(Serialization::IArchive& ar, const DynArray<stack_string>& signalNames, int maxElemets);
	void SerializeVariableChanges(Serialization::IArchive& ar, const string& variableName, int maxElemets);
	void SerializeDialogLinesHistory(Serialization::IArchive& ar);

private:

	struct SExecutedAction
	{
		string                        actorName;
		string                        actionDesc;
		bool                          bEnded;
		DRS::IResponseActionInstance* pInstance;

		void                          Serialize(Serialization::IArchive& ar);
	};

	struct SCheckedCondition
	{
		SCheckedCondition() : bMet(false) {}

		string conditionDesc;
		bool   bMet;

		void   Serialize(Serialization::IArchive& ar);
	};

	struct SStartedResponsesSegment
	{
		bool                           bStarted;
		bool                           bRunning;
		string                         segmentName;
		string                         segmentUiName;
		int                            levelInHierarchy;
		CResponseSegment*              pResponseSegment;
		std::vector<SCheckedCondition> checkedConditions;
		std::vector<SExecutedAction>   executedAction;

		void                           Serialize(Serialization::IArchive& ar);
	};

	struct SStartedResponses
	{
		ResponseInstanceID                    id;
		string                                signalName;
		string                                senderName;
		string                                contextVariables;
		string                                drsUserName;
		EStatus                               currentState;
		int                                   currentlevelInHierarchy;
		float                                 timeOfEvent;
		std::vector<SStartedResponsesSegment> responseSegments;
		SStartedResponsesSegment*             currentSegment;

		void                                  Serialize(Serialization::IArchive& ar);
	};

	struct VariableChangeInfo
	{
		string variableName;
		string change;
		float  timeOfChange;
		string drsUserName;
		void   Serialize(Serialization::IArchive& ar);
	};

	struct SLineHistory
	{
		CHashedString lineID;
		string        lineText;
		string        speakerName;
		EStatus       status;
		string        description;
		string        source;
		float         timeOfEvent;

		virtual void  Serialize(Serialization::IArchive& ar);
	};

	void AddDialogLineStarted(const CHashedString& lineID, const string& lineText, const string& speakerName, EStatus status, const string& reason);
	void AddDialogLineFinished(const CHashedString& lineID, const string& speakerName, EStatus status, const string& reason);

	typedef std::vector<SStartedResponses> StartedResponsesList;
	StartedResponsesList m_executedResponses;

	typedef std::vector<VariableChangeInfo> VariableChangeInfoList;
	VariableChangeInfoList m_variableChangeHistory;

	typedef std::vector<SLineHistory> LineHistoryList;
	LineHistoryList    m_lineHistory;

	ResponseInstanceID m_currentResponse;

	int                m_loggingOptions;

	static const int   MAX_NUMBER_OF_TRACKED_SIGNALS = 512;

	typedef std::map<CResponseInstance*, ResponseInstanceID> ResponseInstanceToResponseInstanceIDMapping;
	ResponseInstanceToResponseInstanceIDMapping m_InstanceToID;
};
}
