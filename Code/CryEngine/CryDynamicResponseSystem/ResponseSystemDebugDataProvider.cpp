// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ResponseSystemDebugDataProvider.h"
#include "Response.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryString/StringUtils.h>
#include "ConditionImpl.h"
#include "ResponseSystem.h"

#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>

using namespace CryDRS;

enum EDrsLoggingOptions
{
	eDrsLoggingOptions_None       = 0,
	eDrsLoggingOptions_Signals    = BIT(6),  // a
	eDrsLoggingOptions_Lines      = BIT(7),  // b
	eDrsLoggingOptions_Responses  = BIT(8),  // c
	eDrsLoggingOptions_Conditions = BIT(9),  // d
	eDrsLoggingOptions_Actions    = BIT(10), // e
	eDrsLoggingOptions_Variables  = BIT(11)  // f
};

//////////////////////////////////////////////////////////////////////////
const char* GetStringFromEnum(CResponseSystemDebugDataProvider::EStatus valueToConvert)
{
	switch (valueToConvert)
	{
	case CResponseSystemDebugDataProvider::eER_NoValidSegment:
		return "NotStarted - No Valid Responses";
	case CResponseSystemDebugDataProvider::eER_Canceled:
		return "Canceled";
	case CResponseSystemDebugDataProvider::eER_Finished:
		return "Finished";
	case CResponseSystemDebugDataProvider::eER_NotStarted:
		return "NotStarted";
	case CResponseSystemDebugDataProvider::eER_Running:
		return "Running";
	case CResponseSystemDebugDataProvider::eER_NoResponse:
		return "NotStarted - No Response";
	case CResponseSystemDebugDataProvider::eER_Queued:
		return "Queued";
	}

	return CryStringUtils::toString(static_cast<int>(valueToConvert));
}

//////////////////////////////////////////////////////////////////////////
CResponseSystemDebugDataProvider::CResponseSystemDebugDataProvider()
	: m_currentResponse(MAX_NUMBER_OF_TRACKED_SIGNALS + 1)
	, m_loggingOptions(0)
{
	REGISTER_CVAR2("drs_loggingOptions", &m_loggingOptions, 0, VF_CHEAT | VF_CHEAT_NOCHECK | VF_BITFIELD,
	               "Toggles the logging of DRS related messages.\n"
	               "Usage: drs_loggingOptions [0ab...] (flags can be combined)\n"
	               "Default: 0 (none)\n"
	               "a: Received Signals\n"
	               "b: Dialog Lines\n"
	               "c: Started Responses\n"
	               "d: Checked Conditions\n"
	               "e: Started Actions\n"
	               "f: Changed Variables\n");
}

//////////////////////////////////////////////////////////////////////////
CResponseSystemDebugDataProvider::~CResponseSystemDebugDataProvider()
{
	if (auto pResponseSystem = CResponseSystem::GetInstance())
	{
		pResponseSystem->GetSpeakerManager()->RemoveListener(this);
	}

	gEnv->pConsole->UnregisterVariable("drs_loggingOptions", true);
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::AddVariableSet(const string& variableName, const string& collectionName, const CVariableValue& oldValue, const CVariableValue& newValue, float time)
{
	if (cry_strncmp(collectionName.c_str(), "Context") == 0 || cry_strncmp(variableName.c_str(), "CurrentTime") == 0)
	{
		return;  //We dont want our list polluted by these...
	}

	if (oldValue.GetType() == eDRVT_Undefined && newValue.GetType() == eDRVT_Undefined)
	{
		return;
	}

	if (newValue == oldValue && oldValue.GetType() != eDRVT_Undefined)
	{
		return;
	}

	VariableChangeInfo newEntry;
	newEntry.timeOfChange = time;
	newEntry.variableName = collectionName + "::" + variableName;

	if ((oldValue.GetType() == eDRVT_Undefined) && (oldValue.GetValue() == CVariableValue::DEFAULT_VALUE))
	{
		newEntry.change = "'Not Existing' to " + newValue.GetValueAsString() + "', Type: " + newValue.GetTypeAsString();
	}
	else
	{
		newEntry.change = "'" + oldValue.GetValueAsString() + "' to '" + newValue.GetValueAsString() + "', Type: " + newValue.GetTypeAsString();
	}

	newEntry.drsUserName = CResponseSystem::GetInstance()->GetCurrentDrsUserName();

	//hard coded limit
	if (m_variableChangeHistory.size() > 256)
	{
		m_variableChangeHistory.erase(m_variableChangeHistory.begin());
	}

	m_variableChangeHistory.push_back(newEntry);

	if (m_loggingOptions & eDrsLoggingOptions_Variables)
	{
		CryLogAlways("<DRS> <%.3f> Variable Change: '%s' '%s'", CResponseSystem::GetInstance()->GetCurrentDrsTime(), newEntry.variableName.c_str(), newEntry.change.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::AddResponseStarted(const string& signalName)
{
	if (m_loggingOptions & eDrsLoggingOptions_Responses)
	{
		CryLogAlways("<DRS> <%.3f> Response started: '%s'", CResponseSystem::GetInstance()->GetCurrentDrsTime(), signalName.c_str());
	}

	const size_t numExecutedResponses = m_executedResponses.size();

	for (int i = 0; i < numExecutedResponses; i++)
	{
		SStartedResponses& current = m_executedResponses[i];

		if (current.currentState == CResponseSystemDebugDataProvider::eER_Queued && current.signalName.compareNoCase(signalName) == 0)
		{
			current.currentState = CResponseSystemDebugDataProvider::eER_Running;
			m_currentResponse = current.id;
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CResponseSystemDebugDataProvider::AddResponseSegmentEvaluated(CResponseSegment* pResponseSegment)
{
	if (m_currentResponse >= m_executedResponses.size())
	{
		return false;
	}

	SStartedResponses& response = m_executedResponses[m_currentResponse];

	for (auto& segment : response.responseSegments)
	{
		segment.bRunning = false;
	}

	SStartedResponsesSegment newResponseSegment;
	newResponseSegment.bStarted = false;
	newResponseSegment.bRunning = false;
	newResponseSegment.segmentName = pResponseSegment->GetName();
	newResponseSegment.pResponseSegment = pResponseSegment;
	newResponseSegment.levelInHierarchy = response.currentlevelInHierarchy;
	response.responseSegments.push_back(newResponseSegment);

	if (m_loggingOptions & eDrsLoggingOptions_Responses)
	{
		CryLogAlways("<DRS> <%.3f> ResponseSegment started: '%s'", CResponseSystem::GetInstance()->GetCurrentDrsTime(), pResponseSegment->GetName());
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CResponseSystemDebugDataProvider::AddConditionChecked(const CConditionsCollection::SConditionInfo* pCondition, bool result)
{
	if (m_currentResponse >= m_executedResponses.size())
	{
		return false;
	}

	SStartedResponses& response = m_executedResponses[m_currentResponse];

	SCheckedCondition newCondition;
	newCondition.bMet = result;

	if (pCondition->m_bNegated)
	{
		newCondition.conditionDesc = "NOT ";
	}

	IVariableUsingBase::s_bDoDisplayCurrentValueInDebugOutput = true;
	newCondition.conditionDesc += pCondition->m_pCondition->GetVerboseInfoWithType();
	IVariableUsingBase::s_bDoDisplayCurrentValueInDebugOutput = false;

	if (!response.responseSegments.empty())
	{
		response.responseSegments.back().checkedConditions.push_back(newCondition);
	}

	if (m_loggingOptions & eDrsLoggingOptions_Conditions)
	{
		CryLogAlways("<DRS> <%.3f> Condition checked: '%s', Met: %i", CResponseSystem::GetInstance()->GetCurrentDrsTime(), newCondition.conditionDesc.c_str(), (int)newCondition.bMet);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CResponseSystemDebugDataProvider::AddResponseSegmentStarted(CResponseSegment* pResponseSegment)
{
	if (m_currentResponse >= m_executedResponses.size())
	{
		return false;
	}

	SStartedResponses& response = m_executedResponses[m_currentResponse];

	for (SStartedResponsesSegment& currentSegment : response.responseSegments)
	{
		if (pResponseSegment == currentSegment.pResponseSegment)
		{
			currentSegment.bRunning = true;
			currentSegment.bStarted = true;
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CResponseSystemDebugDataProvider::AddActionStarted(const string& actionDesc, DRS::IResponseActionInstance* pInstance, DRS::IResponseActor* pActor, CResponseSegment* pResponseSegment)
{
	if (m_currentResponse >= m_executedResponses.size())
	{
		return false;
	}

	SExecutedAction newAction;
	newAction.actorName = (pActor) ? pActor->GetName() : "NoActor";
	newAction.actionDesc = actionDesc;
	newAction.pInstance = pInstance;
	newAction.bEnded = false;
	SStartedResponses& response = m_executedResponses[m_currentResponse];

	for (SStartedResponsesSegment& currentSegment : response.responseSegments)
	{
		if (pResponseSegment == currentSegment.pResponseSegment)
		{
			if (m_loggingOptions & eDrsLoggingOptions_Actions)
			{
				CryLogAlways("<DRS> <%.3f> Action started: '%s', Actor '%s'", CResponseSystem::GetInstance()->GetCurrentDrsTime(), newAction.actionDesc.c_str(), newAction.actorName.c_str());
			}
			currentSegment.bRunning = true;
			currentSegment.bStarted = true;
			currentSegment.executedAction.push_back(newAction);
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CResponseSystemDebugDataProvider::AddActionFinished(DRS::IResponseActionInstance* pInstance)
{
	if (m_currentResponse >= m_executedResponses.size())
	{
		return false;
	}

	SStartedResponses& response = m_executedResponses[m_currentResponse];

	for (SStartedResponsesSegment& currentSegment : response.responseSegments)
	{
		for (SExecutedAction& currentAction : currentSegment.executedAction)
		{
			if (currentAction.pInstance == pInstance && !currentAction.bEnded)
			{
				currentAction.bEnded = true;

				if (m_loggingOptions & eDrsLoggingOptions_Actions)
				{
					CryLogAlways("<DRS> <%.3f> Action finished: '%s', Source: '%s'", CResponseSystem::GetInstance()->GetCurrentDrsTime(), currentAction.actionDesc.c_str(), currentSegment.segmentName.c_str());
				}
				break;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CResponseSystemDebugDataProvider::AddResponseInstanceFinished(EStatus reason)
{
	if (m_currentResponse >= m_executedResponses.size())
	{
		return false;
	}

	SStartedResponses& response = m_executedResponses[m_currentResponse];

	if (m_loggingOptions & eDrsLoggingOptions_Responses)
	{
		CryLogAlways("<DRS> <%.3f> Response finished: '%s', Reason '%s'", CResponseSystem::GetInstance()->GetCurrentDrsTime(), response.signalName.c_str(), GetStringFromEnum(reason));
	}

	if (response.currentState != eER_Canceled)  //we dont want to change from canceled to finished
	{
		response.currentState = reason;
	}

	if (reason != eER_Canceled)  //we are waiting for the actual 'finished' to remove it from our tracking list
	{
		for (SStartedResponsesSegment& current : response.responseSegments)
		{
			current.bRunning = false;
		}

		for (auto it = m_InstanceToID.begin(); it != m_InstanceToID.end(); ++it)
		{
			if (it->second == m_currentResponse)
			{
				m_InstanceToID.erase(it);
				m_currentResponse = MAX_NUMBER_OF_TRACKED_SIGNALS + 1;
				break;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
SERIALIZATION_ENUM_BEGIN_NESTED(CResponseSystemDebugDataProvider, EStatus, "EndReason")
SERIALIZATION_ENUM(CResponseSystemDebugDataProvider::eER_NoValidSegment, "NoValidSegment", "NotStarted - No Valid Responses")
SERIALIZATION_ENUM(CResponseSystemDebugDataProvider::eER_Canceled, "Canceled", "Canceled")
SERIALIZATION_ENUM(CResponseSystemDebugDataProvider::eER_Finished, "Finished", "Finished")
SERIALIZATION_ENUM(CResponseSystemDebugDataProvider::eER_NotStarted, "NotStarted", "NotStarted")
SERIALIZATION_ENUM(CResponseSystemDebugDataProvider::eER_Running, "Running", "Running")
SERIALIZATION_ENUM(CResponseSystemDebugDataProvider::eER_NoResponse, "NoResponse", "NotStarted - No Response")
SERIALIZATION_ENUM(CResponseSystemDebugDataProvider::eER_Queued, "Queued", "Queued")
SERIALIZATION_ENUM_END()

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SStartedResponses::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("signalInfos2", "!+ "))
	{
		int numStartedResponses = 0;

		for (SStartedResponsesSegment& current : responseSegments)
		{
			if (current.bStarted)
			{
				++numStartedResponses;
			}
		}

		ar(currentState, "status", "!^^>200>Status:");
		ar(CryStringUtils::toString(numStartedResponses), "responses", "!^^ StartedResponses:");
		ar.closeBlock();
	}

	ar(timeOfEvent, "time", "!^>80>Time:");
	ar(signalName, "signal", "!^< Signal:");

	if (ar.openBlock("signalInfos2", "!- "))
	{
		ar(senderName, "sender", "!^^>200> Actor:");
		ar(contextVariables, "ContextVariables", "!^^ ContextVariables");
		ar(drsUserName, "source", "!Source:");
		ar.closeBlock();
	}

	int currentBlockDepth = 0;

	for (SStartedResponsesSegment& segment : responseSegments)
	{
		if (currentBlockDepth < segment.levelInHierarchy)
		{
			if (ar.openBlock("FollowUpEx", "!+ ") && ar.openBlock("FollowUp", "!+ FOLLOW UP RESPONSES "))
			{
				currentBlockDepth++;
			}
		}

		if (segment.segmentUiName.empty())
		{
			if (segment.bStarted)
			{
				segment.segmentUiName.Format("!+Response: '%s'", segment.segmentName.c_str());
			}
			else
			{
				segment.segmentUiName.Format("!Response: '%s'", segment.segmentName.c_str());
			}
		}

		ar(segment, "segment", segment.segmentUiName);
	}

	for (int i = 0; i < currentBlockDepth; i++)
	{
		ar.closeBlock();
		ar.closeBlock();
	}

	static string seperator = "______________________________________________________________________________________________________________________________________________________________________________________";
	ar(seperator, "seperator", "!< ");
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SStartedResponsesSegment::Serialize(Serialization::IArchive& ar)
{
	if (checkedConditions.empty())
	{
		ar(checkedConditions, "checkedConditions", "!  No conditions");
	}
	else
	{
		bool bConditionsMet = true;

		for (SCheckedCondition& currentCondition : checkedConditions)
		{
			if (!currentCondition.bMet)
			{
				bConditionsMet = false;
				break;
			}
		}

		if (bConditionsMet)
		{
			ar(bConditionsMet, "conditionsMet", "!>85>^ ConditionsMet");
		}
		ar(checkedConditions, "checkedConditions", "!+  CheckedConditions");
	}

	if (!executedAction.empty())
	{
		ar(executedAction, "executedAction", "!+<  ExecutedAction");
	}
	else
	{
		ar(executedAction, "executedAction", (bStarted) ? "!  No actions" : "!  No actions executed");
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SExecutedAction::Serialize(Serialization::IArchive& ar)
{
	ar(bEnded, "hasEnded", "!^Done");
	ar(actionDesc, "actionDesc", "!<^");
	ar(actorName, "actor", "!^Actor:");
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SCheckedCondition::Serialize(Serialization::IArchive& ar)
{
	ar(bMet, "wasMet", "^!Was Met");
	ar(conditionDesc, "conditionDesc", "!^ Condition:");
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SetCurrentResponseInstance(CResponseInstance* pInstanceForCurrentID)
{
	const auto foundIT = m_InstanceToID.find(pInstanceForCurrentID);

	if (foundIT == m_InstanceToID.end())
	{
		m_currentResponse = MAX_NUMBER_OF_TRACKED_SIGNALS + 1;
	}
	else
	{
		m_currentResponse = foundIT->second;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CResponseSystemDebugDataProvider::AddResponseInstanceCreated(CResponseInstance* pInstanceForCurrentID)
{
	m_InstanceToID[pInstanceForCurrentID] = m_currentResponse;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::Reset()
{
	m_executedResponses.clear();
	m_variableChangeHistory.clear();
	m_InstanceToID.clear();
	m_lineHistory.clear();
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::VariableChangeInfo::Serialize(Serialization::IArchive& ar)
{
	ar(timeOfChange, "time", "!^Time");
	ar(variableName, "variableName", "!^Variable");
	ar(change, "change", "!^< Changed from");

	if (!drsUserName.empty())
	{
		ar(drsUserName, "Changer", "!^< Source");
	}
	else
	{
		ar(drsUserName, "Changer", "!^< Source Unknown");
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::OnLineEvent(const DRS::IResponseActor* pSpeaker, const CHashedString& lineID, eLineEvent lineEvent, const DRS::IDialogLine* pLine)
{
	string speakerName = (pSpeaker) ? pSpeaker->GetName() : "Missing Speaker";
	string lineText = (pLine) ? pLine->GetText() : "Missing: " + lineID.GetText();

	switch (lineEvent)
	{
	case eLineEvent_Started:
		AddDialogLineStarted(lineID, lineText, speakerName, eER_Running, "Started");
		break;
	case eLineEvent_Queued:
		AddDialogLineStarted(lineID, lineText, speakerName, eER_Queued, "Another line needs to finish first");
		break;
	case eLineEvent_SkippedBecauseOfPriority:
		AddDialogLineStarted(lineID, "no line picked", speakerName, eER_NotStarted, "Line ('" + lineText + "') with same/higher priority was already playing");
		break;
	case eLineEvent_SkippedBecauseOfNoValidLineVariations:
		AddDialogLineStarted(lineID, lineText, speakerName, eER_NotStarted, "Skipped because no (more) valid line variations exist.");
		break;
	case eLineEvent_SkippedBecauseOfFaultyData:
		AddDialogLineStarted(lineID, lineText, speakerName, eER_NotStarted, "Data problem, missing entity for speaker or missing DRS actor.");
		break;
	case eLineEvent_Finished:
		AddDialogLineFinished(lineID, speakerName, eER_Finished, "Done");
		break;
	case eLineEvent_Canceled:
		AddDialogLineFinished(lineID, speakerName, eER_Canceled, "Canceled or actor removed");
		break;
	case eLineEvent_SkippedBecauseOfTimeOut:
		AddDialogLineFinished(lineID, speakerName, eER_NotStarted, "Timed out of the queue");
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::Init()
{
	CResponseSystem::GetInstance()->GetSpeakerManager()->AddListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::AddSignalFired(const string& signalName, const string& senderName, const string& contextVariables)
{
	//hard coded limit
	if (m_executedResponses.size() > MAX_NUMBER_OF_TRACKED_SIGNALS)
	{
		m_executedResponses.clear();
		m_currentResponse = MAX_NUMBER_OF_TRACKED_SIGNALS + 1;
		m_InstanceToID.clear();
	}

	SStartedResponses newResponse;
	newResponse.signalName = signalName;
	newResponse.senderName = senderName;
	newResponse.contextVariables = contextVariables;
	newResponse.drsUserName = CResponseSystem::GetInstance()->GetCurrentDrsUserName();
	newResponse.currentState = CResponseSystemDebugDataProvider::eER_Queued;
	newResponse.id = static_cast<ResponseInstanceID>(m_executedResponses.size());
	newResponse.currentlevelInHierarchy = 0;
	newResponse.timeOfEvent = CResponseSystem::GetInstance()->GetCurrentDrsTime();

	m_executedResponses.push_back(newResponse);

	if (m_loggingOptions & eDrsLoggingOptions_Signals)
	{
		CryLogAlways("<DRS> <%.3f> Signal fired: '%s', sender '%s', '%s'", CResponseSystem::GetInstance()->GetCurrentDrsTime(), signalName.c_str(), senderName.c_str(), contextVariables.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
bool CResponseSystemDebugDataProvider::IncrementSegmentHierarchyLevel()
{
	if (m_currentResponse >= m_executedResponses.size())
	{
		return false;
	}

	SStartedResponses& response = m_executedResponses[m_currentResponse];
	response.currentlevelInHierarchy++;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::AddDialogLineFinished(const CHashedString& lineID, const string& speakerName, EStatus status, const string& reason)
{
	for (auto& it : m_lineHistory)
	{
		if ((it.status == eER_Running || it.status == eER_Queued) && (it.lineID == lineID) && (it.speakerName == speakerName))
		{
			it.status = status;
			it.description = reason;
		}
	}

	if (m_loggingOptions & eDrsLoggingOptions_Lines)
	{
		CryLogAlways("<DRS> <%.3f> Line Finished: Speaker: '%s' ID: '%s' State: %s Reason: %s", CResponseSystem::GetInstance()->GetCurrentDrsTime(), speakerName.c_str(), lineID.GetText().c_str(), GetStringFromEnum(status), reason.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::AddDialogLineStarted(const CHashedString& lineID, const string& lineText, const string& speakerName, EStatus status, const string& reason)
{
	SLineHistory temp;
	temp.lineID = lineID;
	temp.lineText = lineText;
	temp.speakerName = speakerName;
	temp.status = status;
	temp.source = CResponseSystem::GetInstance()->GetCurrentDrsUserName();
	temp.timeOfEvent = CResponseSystem::GetInstance()->GetCurrentDrsTime();

	bool bWasHandled = false;

	if (status == eER_Running)  //if a line is started, after it was queued, then we do already have an entry, which we just can update
	{
		for (auto& it : m_lineHistory)
		{
			if ((it.status == eER_Queued) && (it.lineID == lineID) && (it.speakerName == speakerName))
			{
				temp.description = "Started (After Queue)";
				it = temp;
				bWasHandled = true;
			}
		}
	}

	if (!bWasHandled)
	{
		temp.description = reason;
		m_lineHistory.push_back(temp);
	}

	if (m_loggingOptions & eDrsLoggingOptions_Lines)
	{
		CryLogAlways("<DRS> <%.3f> Line Start Requested: Speaker: '%s' ID: '%s' State: '%s'. Reason: '%s' (Text: \"%s\") Source: '%s'", CResponseSystem::GetInstance()->GetCurrentDrsTime(), speakerName.c_str(), lineID.GetText().c_str(), GetStringFromEnum(status), temp.description.c_str(), lineText.c_str(), temp.source.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::GetRecentSignals(DynArray<const char*>& outResults, DRS::IResponseManager::eSignalFilter filter /* = eSF_All */)
{
	for (StartedResponsesList::const_iterator it = m_executedResponses.begin(); it != m_executedResponses.end(); ++it)
	{
		if (filter == DRS::IResponseManager::eSF_All ||
		    (filter == DRS::IResponseManager::eSF_OnlyWithoutResponses && it->currentState == eER_NoResponse) ||
		    (filter == DRS::IResponseManager::eSF_OnlyWithResponses && it->currentState != eER_NoResponse))
		{
			outResults.push_back(it->signalName.c_str());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SerializeRecentResponse(Serialization::IArchive& ar, const DynArray<stack_string>& signalNames, int maxElemets)
{
	if (ar.getFilter() != 0)
	{
		if (signalNames.empty())  // ALL
		{
			m_executedResponses.clear();
		}
		else
		{
			for (const auto& itSignalNames : signalNames)
			{
				string signalName = itSignalNames.c_str();

				for (auto it = m_executedResponses.begin(); it != m_executedResponses.end(); )
				{
					if (it->signalName.compareNoCase(signalName) == 0)
					{
						it = m_executedResponses.erase(it);
					}
					else
					{
						++it;
					}
				}
			}
		}
	}
	else
	{
		static string signalslist;
		static StartedResponsesList tempcopy;

		if (signalNames.empty()) // ALL
		{
			if (maxElemets > m_executedResponses.size())
			{
				maxElemets = m_executedResponses.size();
			}

			tempcopy.assign(m_executedResponses.rbegin(), m_executedResponses.rbegin() + maxElemets);
			signalslist = "ALL";
		}
		else
		{
			signalslist.clear();

			for (const auto& itSignalNames : signalNames)
			{
				string signalName = itSignalNames.c_str();

				if (!signalslist.empty())
				{
					signalslist += ", ";
				}

				signalslist += signalName;
			}

			tempcopy.clear();

			for (StartedResponsesList::const_reverse_iterator it = m_executedResponses.rbegin(); it != m_executedResponses.rend(); ++it)
			{
				for (const auto& signalName : signalNames)
				{
					if (it->signalName.compareNoCase(signalName.c_str()) == 0 && (maxElemets <= 0 || tempcopy.size() < maxElemets))
					{
						tempcopy.push_back(*it);
					}
				}
			}
		}

		static string label;

		if (tempcopy.empty())
		{
			label = "!No history for signal '" + signalslist + "'";
			ar(tempcopy, "ExecutedResponses", label.c_str());
		}
		else
		{
			label = "!+History for signal '" + signalslist + "'";
			ar(tempcopy, "ExecutedResponses", label.c_str());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SerializeVariableChanges(Serialization::IArchive& ar, const string& variableName, int maxElemets)
{
	bool bSerializeAll = variableName == "ALL";

	if (ar.getFilter() != 0)
	{
		if (bSerializeAll)
		{
			m_variableChangeHistory.clear();
		}
		else
		{
			for (auto it = m_variableChangeHistory.begin(); it != m_variableChangeHistory.end(); )
			{
				if (it->variableName.compareNoCase(variableName) == 0)
				{
					it = m_variableChangeHistory.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	}
	else
	{
		static VariableChangeInfoList tempcopy;
		tempcopy.clear();

		for (VariableChangeInfoList::const_iterator it = m_variableChangeHistory.begin(); it != m_variableChangeHistory.end(); ++it)
		{
			if ((bSerializeAll ||
			     it->variableName.find(variableName.c_str()) != string::npos) &&
			    (maxElemets <= 0 ||
			     tempcopy.size() < maxElemets))
			{
				tempcopy.push_back(*it);
			}
		}

		static string label;
		label = "!History of changes for '" + variableName + "'";
		ar(tempcopy, "VariableChanges", label.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SerializeDialogLinesHistory(Serialization::IArchive& ar)
{
	if (ar.getFilter() != 0)
	{
		m_lineHistory.clear();
	}
	else
	{
		ar(m_lineHistory, "linesHistory", "+!Spoken Lines");
	}
}

//////////////////////////////////////////////////////////////////////////
void CResponseSystemDebugDataProvider::SLineHistory::Serialize(Serialization::IArchive& ar)
{
	ar(timeOfEvent, "time", "!^Time:");
	ar(lineID, "lineID", "!^LineID:");
	ar(speakerName, "speaker", "!^Speaker:");
	ar(status, "status", "!^Status:");
	ar(description, "desc", "!Desc:");
	ar(lineText, "lineText", "!LineText:");
	ar(source, "source", "!Source:");
}
