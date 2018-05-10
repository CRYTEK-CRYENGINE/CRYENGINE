// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CrySystem/File/ICryPak.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>

#include "ResponseManager.h"
#include "ResponseInstance.h"
#include "ResponseSystemDebugDataProvider.h"
#include "ResponseSystem.h"
#include "VariableCollection.h"
#include "Response.h"
#include "ResponseSystemDataImportHelper.h"

namespace CryDRS
{
string CResponseManager::s_currentSignal;

typedef ::std::pair<CHashedString, ResponsePtr> ResponsePair;
typedef ::std::pair<string, ResponsePtr> ResponsePairSorted;

struct ResponsePairSerializer
{
	ResponsePairSerializer(CryDRS::ResponsePair& pair) : pair_(pair) {}
	void Serialize(Serialization::IArchive& ar)
	{
		ar(pair_.first, "key", "^");
		ar(pair_.second, "value", "^");
	}
	CryDRS::ResponsePair& pair_;
};
bool Serialize(Serialization::IArchive& ar, CryDRS::ResponsePair& pair, const char* szName, const char* szLabel)
{
	ResponsePairSerializer keyValue(pair);
	return ar(keyValue, szName, szLabel);
}

//--------------------------------------------------------------------------------------------------
struct ResponsePairSortedSerializer
{
	ResponsePairSortedSerializer(CryDRS::ResponsePairSorted& pair) : pair_(pair) {}
	void Serialize(Serialization::IArchive& ar)
	{
		ar(pair_.first, "key", "^");
		if (pair_.first.empty())
		{
			if (ar.isEdit())
			{
				ar.warning(pair_.first, "Response without a signal name detected.");
			}
			return;
		}
		ar(pair_.second, "value", "^");
	}
	CryDRS::ResponsePairSorted& pair_;
};
bool Serialize(Serialization::IArchive& ar, CryDRS::ResponsePairSorted& pair, const char* szName, const char* szLabel)
{
	ResponsePairSortedSerializer keyValue(pair);
	return ar(keyValue, szName, szLabel);
}

bool Serialize(Serialization::IArchive& ar, CryDRS::ResponsePtr& ptr, const char* szName, const char* szLabel)
{
	if (!ptr.get())
	{
		ptr.reset(new CryDRS::CResponse());
	}
	ar(*ptr.get(), szName, szLabel);
	return true;
}

//--------------------------------------------------------------------------------------------------
CResponseManager::CResponseManager()
{
	m_usedFileFormat = CResponseManager::eUFF_JSON;
}

//--------------------------------------------------------------------------------------------------
CResponseManager::~CResponseManager()
{
	for (ResponseInstanceList::iterator it = m_runningResponses.begin(); it != m_runningResponses.end(); ++it)
	{
		ReleaseInstance(*it, false);
	}
}

static string s_responseDataPath;
static string s_currentFilePath;

//--------------------------------------------------------------------------------------------------
bool CResponseManager::_LoadFromFiles(const string& dataPath)
{
	string searchPath = dataPath + string("*.*");
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(searchPath.c_str(), &fd);
	if (handle != -1)
	{
		do
		{
			const string sName = fd.name;
			if (sName != "." && sName != ".." && !sName.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					_LoadFromFiles(dataPath + sName + '/');
				}
				else
				{
					s_currentFilePath = dataPath + sName;
					if (m_usedFileFormat == eUFF_JSON)
					{
						Serialization::LoadJsonFile(*this, s_currentFilePath);
					}
					else if (m_usedFileFormat == eUFF_XML)
					{
						Serialization::LoadXmlFile(*this, s_currentFilePath);
					}
					else if (m_usedFileFormat == eUFF_BIN)
					{
						Serialization::LoadBinaryFile(*this, s_currentFilePath);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);
		pCryPak->FindClose(handle);
	}

	return true;
}

//--------------------------------------------------------------------------------------------------
bool CResponseManager::LoadFromFiles(const char* szDataPath)
{
	s_responseDataPath = PathUtil::AddSlash(szDataPath);

	m_mappedSignals.clear();

	return _LoadFromFiles(s_responseDataPath);
}

//--------------------------------------------------------------------------------------------------
bool CResponseManager::SaveToFiles(const char* szDataPath)
{
	const string outFolder = szDataPath;
	for (MappedSignals::iterator it = m_mappedSignals.begin(); it != m_mappedSignals.end(); ++it)
	{
		if (m_usedFileFormat == eUFF_JSON)
		{
			string outfile = outFolder + it->first.GetText() + ".response";
			Serialization::SaveJsonFile(outfile, Serialization::SStruct(*it->second));
		}
		else if (m_usedFileFormat == eUFF_XML)
		{
			string outfile = outFolder + it->first.GetText() + ".xml";
			Serialization::SaveXmlFile(outfile, Serialization::SStruct(*it->second), "responses");
		}
		else if (m_usedFileFormat == eUFF_BIN)
		{
			string outfile = outFolder + it->first.GetText() + ".bin";
			Serialization::SaveBinaryFile(outfile, Serialization::SStruct(*it->second));
		}
	}
	return true;
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::QueueSignal(const SSignal& signal)
{
	DRS_DEBUG_DATA_ACTION(AddSignalFired(signal.m_signalName.GetText(), (signal.m_pSender) ? signal.m_pSender->GetName() : "no sender", (signal.m_pSignalContext) ? signal.m_pSignalContext->GetVariablesAsString() : "(no context variables)"));

	m_currentlyQueuedSignals.push_back(signal);
}

//--------------------------------------------------------------------------------------------------
bool CResponseManager::CancelSignalProcessing(const SSignal& signalToCancel)
{
	bool bSomethingCanceled = false;
	for (CResponseInstance* pRunningResponse : m_runningResponses)
	{
		if ((pRunningResponse->GetOriginalSender() == signalToCancel.m_pSender || signalToCancel.m_pSender == nullptr)
		    && (pRunningResponse->GetSignalName() == signalToCancel.m_signalName || !signalToCancel.m_signalName.IsValid())
		    && (pRunningResponse->GetSignalInstanceId() != signalToCancel.m_id))
		{
			pRunningResponse->Cancel();
			bSomethingCanceled = true;
		}
	}

	for (SignalList::iterator itQueued = m_currentlyQueuedSignals.begin(); itQueued != m_currentlyQueuedSignals.end(); )
	{
		SSignal& currentSignal = *itQueued;
		if ((currentSignal.m_pSender == signalToCancel.m_pSender || signalToCancel.m_pSender == nullptr)
		    && currentSignal.m_signalName == signalToCancel.m_signalName)
		{
			InformListenerAboutSignalProcessingFinished(currentSignal.m_signalName, currentSignal.m_pSender, currentSignal.m_pSignalContext, currentSignal.m_id, nullptr, DRS::IResponseManager::IListener::ProcessingResult_Canceled);
			itQueued = m_currentlyQueuedSignals.erase(itQueued);
			bSomethingCanceled = true;
		}
		else
		{
			++itQueued;
		}
	}
	return bSomethingCanceled;
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::Update()
{
	float currentTime = CResponseSystem::GetInstance()->GetCurrentDrsTime();

	for (ResponseInstanceList::iterator it = m_runningResponses.begin(); it != m_runningResponses.end(); )
	{
		if ((*it)->Update())
		{
			++it;
		}
		else
		{
			InformListenerAboutSignalProcessingFinished((*it)->GetSignalName(), (*it)->GetOriginalSender(), (*it)->GetContextVariablesImpl(), (*it)->GetSignalInstanceId(), *it, IResponseManager::IListener::ProcessingResult_Done);
			ReleaseInstance(*it, false);
			it = m_runningResponses.erase(it);
		}
	}

	if (!m_currentlyQueuedSignals.empty())
	{
		static SignalList signalsToBeProcessed;
		signalsToBeProcessed.swap(m_currentlyQueuedSignals); // we don't work directly on m_currentlyQueuedSignals, because starting the responses can already queue new signals
		for (SSignal& currentSignal : signalsToBeProcessed)
		{
			//find the mapped Response for the Signal
			MappedSignals::const_iterator mappedResponse = m_mappedSignals.find(currentSignal.m_signalName);

			if (mappedResponse != m_mappedSignals.end())
			{
				CResponseInstance* pResponseInstance = mappedResponse->second->StartExecution(currentSignal);
				InformListenerAboutSignalProcessingStarted(currentSignal, pResponseInstance);
				if (!pResponseInstance) //no instance = conditions not met, we are done
				{
					InformListenerAboutSignalProcessingFinished(currentSignal.m_signalName, currentSignal.m_pSender, currentSignal.m_pSignalContext, currentSignal.m_id, pResponseInstance, DRS::IResponseManager::IListener::ProcessingResult_ConditionsNotMet);
				}
			}
			else
			{
				InformListenerAboutSignalProcessingStarted(currentSignal, nullptr);
				DRS_DEBUG_DATA_ACTION(AddResponseStarted(currentSignal.m_signalName.GetText()));
				DRS_DEBUG_DATA_ACTION(AddResponseInstanceFinished(CResponseSystemDebugDataProvider::eER_NoResponse));
				InformListenerAboutSignalProcessingFinished(currentSignal.m_signalName, currentSignal.m_pSender, currentSignal.m_pSignalContext, currentSignal.m_id, nullptr, DRS::IResponseManager::IListener::ProcessingResult_NoResponseDefined);
			}
		}
		signalsToBeProcessed.clear();
	}
}

//--------------------------------------------------------------------------------------------------
CResponseInstance* CResponseManager::CreateInstance(SSignal& signal, CResponse* pResponse)
{
	CResponseInstance* newInstance = new CResponseInstance(signal, pResponse);
	m_runningResponses.push_back(newInstance);
	DRS_DEBUG_DATA_ACTION(AddResponseInstanceCreated(newInstance));

	return newInstance;
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::ReleaseInstance(CResponseInstance* pInstance, bool removeFromRunningInstances /* = true */)
{
	if (removeFromRunningInstances)
	{
		for (ResponseInstanceList::iterator it = m_runningResponses.begin(); it != m_runningResponses.end(); ++it)
		{
			if (pInstance == *it)
			{
				m_runningResponses.erase(it);
				delete pInstance;
				return;
			}
		}
		DrsLogError("Someone tried to delete an ResponseInstance through the DynamicResponseManager that was not managed by the DynamicResponseManager!");
	}
	else
	{
		delete pInstance;
	}

	DRS_DEBUG_DATA_ACTION(AddResponseInstanceFinished(CResponseSystemDebugDataProvider::eER_Finished));
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::Reset(bool bResetExecutionCounter, bool bClearAllResponseMappings /* = false */)
{
	m_currentlyQueuedSignals.clear();

	for (CResponseInstance* pRunningResponse : m_runningResponses)
	{
		pRunningResponse->Cancel();
		pRunningResponse->Update();
		ReleaseInstance(pRunningResponse, false);
	}

	if (bClearAllResponseMappings)
	{
		m_mappedSignals.clear();
	}

	if (bResetExecutionCounter)
	{
		for (auto& mappedSignal : m_mappedSignals)
		{
			mappedSignal.second->Reset();
		}
	}

	m_runningResponses.clear();
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::Serialize(Serialization::IArchive& ar)
{
	if (!m_runningResponses.empty() && ar.isInput())  //easiest way to make sure, we are not modifying responses currently running -> stop all running
	{
		for (CResponseInstance* pRunningResponse : m_runningResponses)
		{
			SET_DRS_USER_SCOPED("Reset because of ResponseManager::Serialize");
			pRunningResponse->Cancel();
			pRunningResponse->Update();
			ReleaseInstance(pRunningResponse, false);
		}
		m_runningResponses.clear();
	}

	if ((ar.getFilter() & eSH_ReplaceAllExisting) > 0 || ar.caps(Serialization::IArchive::BINARY))
	{
		if (ar.isEdit())
		{
			typedef std::map<string, ResponsePtr> SortedMappedSignal;
			SortedMappedSignal sortedSignals;
			for (auto& mappedSignal : m_mappedSignals)
			{
				sortedSignals[mappedSignal.first.GetText()] = std::move(mappedSignal.second);
			}
			for (auto& sortedSignal : sortedSignals)
			{
				ar(*sortedSignal.second, sortedSignal.first.c_str(), sortedSignal.first.c_str());
			}
			m_mappedSignals.clear();
			for (auto& sortedSignal : sortedSignals)
			{
				m_mappedSignals[sortedSignal.first] = std::move(sortedSignal.second);
			}
		}
		else
		{
			ar(m_mappedSignals, "signals_responses", "+Signal Responses:");
		}
	}
	else
	{
		s_currentSignal = PathUtil::GetFileName(s_currentFilePath);
		m_mappedSignals[s_currentSignal] = std::make_shared<CryDRS::CResponse>();
		m_mappedSignals[s_currentSignal]->Serialize(ar);
		s_currentSignal.clear();
	}
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::SetFileFormat(EUsedFileFormat format)
{
	m_usedFileFormat = format;
}

//--------------------------------------------------------------------------------------------------
DynArray<const char*> CResponseManager::GetRecentSignals(DRS::IResponseManager::eSignalFilter filter /* = DRS::IResponseManager::eSF_All */)
{
	DynArray<const char*> temp;
	DRS_DEBUG_DATA_ACTION(GetRecentSignals(temp, filter));
	return temp;
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::SerializeResponse(Serialization::IArchive& ar, const DynArray<stack_string>& signalNames, DRS::IResponseActor* pActorForEvaluation)
{
#if !defined(_RELEASE)
	CConditionsCollection::s_pActorForEvaluation = pActorForEvaluation;
#endif

	if (signalNames.empty())
	{
		if (ar.isEdit() || ar.caps(Serialization::IArchive::BINARY))
		{
			ar.setFilter(ar.getFilter() | eSH_ReplaceAllExisting);
			return Serialize(ar);
		}
		else
		{
			SaveToFiles(s_responseDataPath);
			return;
		}
	}

	for (const string signalName : signalNames)
	{
		const CHashedString hashedSignalName = signalName;
		bool bFound = false;
		for (MappedSignals::iterator it = m_mappedSignals.begin(); it != m_mappedSignals.end(); ++it)
		{
			if (it->first == hashedSignalName)
			{
				if (ar.isInput() && !m_runningResponses.empty()) //stop the response first, if currently running
				{
					for (ResponseInstanceList::iterator itRunning = m_runningResponses.begin(); itRunning != m_runningResponses.end(); )
					{
						if ((*itRunning)->GetSignalName() == hashedSignalName)
						{
							(*itRunning)->Cancel();
							(*itRunning)->Update();
							ReleaseInstance(*itRunning, false);
							itRunning = m_runningResponses.erase(itRunning);
						}
						else
						{
							++itRunning;
						}
					}
				}

				bFound = true;
				s_currentSignal = signalName;
				bool bBlockOpened = false;
				if (ar.isEdit())
				{
					bBlockOpened = ar.openBlock("Signal", " ");
					ar(it->first.GetText(), "response-name", "!^^< RESPONSE TREE for SIGNAL ");
				}
				it->second->Serialize(ar);
				s_currentSignal.clear();

				if (bBlockOpened)
				{
					ar.closeBlock();
				}
			}
		}
		if (!bFound && ar.isEdit())
		{
			ar(signalName, "NoResponse", "!No Response defined for:");
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::SerializeRecentResponse(Serialization::IArchive& ar, const DynArray<stack_string>& signalNames, int maxElemets /* = -1 */)
{
	DRS_DEBUG_DATA_ACTION(SerializeRecentResponse(ar, signalNames, maxElemets));
}

//--------------------------------------------------------------------------------------------------
bool CResponseManager::AddResponse(const stack_string& signalName)
{
	string copyString = signalName;
	if (m_mappedSignals.find(copyString) != m_mappedSignals.end())
		return false;
	m_mappedSignals[copyString] = std::make_shared<CryDRS::CResponse>();
	return true;
}

//--------------------------------------------------------------------------------------------------
bool CResponseManager::RemoveResponse(const stack_string& signalName)
{
	const CHashedString hashedSignalName = signalName.c_str();

	if (!m_runningResponses.empty()) //stop the response first, if currently running
	{
		for (ResponseInstanceList::iterator it = m_runningResponses.begin(); it != m_runningResponses.end(); )
		{
			if ((*it)->GetSignalName() == hashedSignalName)
			{
				(*it)->Cancel();
				(*it)->Update();
				ReleaseInstance(*it, false);
				it = m_runningResponses.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	auto foundIT = m_mappedSignals.find(hashedSignalName);
	if (foundIT != m_mappedSignals.end())
	{
		m_mappedSignals.erase(foundIT);
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::SerializeVariableChanges(Serialization::IArchive& ar, const stack_string& variableName /*= "ALL"*/, int maxElemets /*= -1*/)
{
	DRS_DEBUG_DATA_ACTION(SerializeVariableChanges(ar, variableName.c_str(), maxElemets));
}

//--------------------------------------------------------------------------------------------------
ResponsePtr CResponseManager::GetResponse(const CHashedString& signalName)
{
	MappedSignals::iterator foundIT = m_mappedSignals.find(signalName);
	if (foundIT != m_mappedSignals.end())
	{
		return foundIT->second;
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
bool CResponseManager::AddListener(DRS::IResponseManager::IListener* pNewListener, DRS::SignalInstanceId signalID /* = DRS::s_InvalidSignalID */)
{
	m_listeners.push_back(std::make_pair(pNewListener, signalID));
	return true;
}

//--------------------------------------------------------------------------------------------------
bool CResponseManager::RemoveListener(DRS::IResponseManager::IListener* pListenerToRemove)
{
	for (ListenerList::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		if (it->first == pListenerToRemove)
		{
			m_listeners.erase(it);
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::InformListenerAboutSignalProcessingStarted(const SSignal& signal, DRS::IResponseInstance* pInstance)
{
	DRS::IResponseManager::IListener::SSignalInfos signalInfo(signal.m_signalName, signal.m_pSender, signal.m_pSignalContext, signal.m_id);

	for (std::pair<DRS::IResponseManager::IListener*, DRS::SignalInstanceId>& current : m_listeners)
	{
		if (current.second == signal.m_id || current.second == DRS::s_InvalidSignalId)
		{
			current.first->OnSignalProcessingStarted(signalInfo, pInstance);
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::InformListenerAboutSignalProcessingFinished(
  const CHashedString& signalName,
  CResponseActor* pSender,
  const VariableCollectionSharedPtr& pSignalContext,
  const DRS::SignalInstanceId signalID,
  DRS::IResponseInstance* pInstance,
  DRS::IResponseManager::IListener::eProcessingResult outcome)
{
	DRS::IResponseManager::IListener::SSignalInfos signalInfo(signalName, pSender, pSignalContext, signalID);

	for (ListenerList::iterator it = m_listeners.begin(); it != m_listeners.end(); )
	{
		if (it->second == signalID)
		{
			it->first->OnSignalProcessingFinished(signalInfo, pInstance, outcome);
			it = m_listeners.erase(it);  //the processing for this signalId has finished, no need to keep the listener around.
		}
		else
		{
			if (it->second == DRS::s_InvalidSignalId)
			{
				it->first->OnSignalProcessingFinished(signalInfo, pInstance, outcome);
			}
			++it;
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::SerializeResponseStates(Serialization::IArchive& ar)
{
	struct SResponseStateInfo
	{
		CHashedString responsename;
		uint32        executionCount;
		float         lastStartTime;
		float         lastEndTime;
		void          Serialize(Serialization::IArchive& ar)
		{
			if (lastEndTime < lastStartTime)  //check if currently running
			{
				ar(responsename, "name", "^Name");
			}
			else
			{
				ar(responsename, "name", "^!Name");
			}
			ar(executionCount, "executionCount", "^>70>Executions");
			ar(lastStartTime, "LastStart", "^>70>LastStart");
			ar(lastEndTime, "LastEnd", "^>70>LastEnd");
		}
	};
	std::vector<SResponseStateInfo> responseStates;

	if (ar.isInput())
	{
		ar(responseStates, "ResponsesData", "-Responses Data");
		for (SResponseStateInfo& current : responseStates)
		{
			ResponsePtr pResponse = GetResponse(current.responsename);
			if (pResponse)
			{
				pResponse->SetExecutionCounter(current.executionCount);
				pResponse->SetLastStartTime(current.lastStartTime);
				pResponse->SetLastEndTime(current.lastEndTime);
			}
		}
	}
	else
	{
		int i = 0;
		responseStates.resize(m_mappedSignals.size());
		for (MappedSignals::iterator it = m_mappedSignals.begin(); it != m_mappedSignals.end(); ++it)
		{
			responseStates[i].responsename = it->first;
			responseStates[i].executionCount = it->second->GetExecutionCounter();
			responseStates[i].lastStartTime = it->second->GetLastStartTime();
			responseStates[i].lastEndTime = it->second->GetLastEndTime();
			++i;
		}
		ar(responseStates, "ResponsesData", "Responses Data");
	}
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::GetAllResponseData(DRS::ValuesList* pOutCollectionsList, bool bSkipDefaultValues)
{
	std::pair<string, string> temp;
	for (MappedSignals::iterator it = m_mappedSignals.begin(); it != m_mappedSignals.end(); ++it)
	{
		if (!bSkipDefaultValues || it->second->GetExecutionCounter() > 0)
		{
			temp.first.Format("_Internal.%s", it->first.GetText().c_str());
			temp.second.Format("%u,%f,%f", it->second->GetExecutionCounter(), it->second->GetLastStartTime(), it->second->GetLastEndTime());
			pOutCollectionsList->push_back(temp);
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CResponseManager::SetAllResponseData(DRS::ValuesListIterator start, DRS::ValuesListIterator end)
{
	for (MappedSignals::iterator it = m_mappedSignals.begin(); it != m_mappedSignals.end(); ++it)
	{
		it->second->Reset();
	}

	uint32 executionCounter;
	float startTime;
	float endTime;

	DRS::ValuesString collectionAndVariable;
	DRS::ValuesString collectionName;
	CHashedString responseName;

	for (DRS::ValuesListIterator it = start; it != end; ++it)
	{
		collectionAndVariable = it->first;
		const int pos = collectionAndVariable.find('.');
		collectionName = collectionAndVariable.substr(0, pos);

		if (collectionName == "_Internal")
		{
			responseName = collectionAndVariable.substr(pos + 1).c_str();
			ResponsePtr pResponse = GetResponse(responseName);
			if (pResponse)
			{
				if (sscanf(it->second.c_str(), "%u,%f,%f", &executionCounter, &startTime, &endTime) == 3)
				{
					pResponse->SetExecutionCounter(executionCounter);
					pResponse->SetLastStartTime(startTime);
					pResponse->SetLastEndTime(endTime);
				}
				else
				{
					DrsLogWarning((string("Could not parse response execution information from given data '") + it->second.c_str() + "'.").c_str());
				}
			}
		}
	}
}

bool CResponseManager::IsSignalProcessed(const SSignal& signal)
{
	for (const SSignal& queuedSignal : m_currentlyQueuedSignals)
	{
		if ((queuedSignal.m_signalName == signal.m_signalName || !signal.m_signalName.IsValid())
		    && (queuedSignal.m_pSender == signal.m_pSender || signal.m_pSender == nullptr)
		    && queuedSignal.m_id != signal.m_id)
		{
			return true;
		}
	}

	for (const CResponseInstance* pRunningInstance : m_runningResponses)
	{
		if ((pRunningInstance->GetSignalName() == signal.m_signalName || !signal.m_signalName.IsValid())
		    && (pRunningInstance->GetOriginalSender() == signal.m_pSender || signal.m_pSender == nullptr)
		    && pRunningInstance->GetSignalInstanceId() != signal.m_id)
		{
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
bool CResponseManager::HasMappingForSignal(const CHashedString& signalName)
{
	return m_mappedSignals.find(signalName) != m_mappedSignals.end();

}

void CryDRS::CResponseManager::OnActorRemoved(const CResponseActor* pActor)
{
	//We stop any response that the just removed actor might be running
	for (ResponseInstanceList::iterator it = m_runningResponses.begin(); it != m_runningResponses.end(); )
	{
		CResponseInstance* pCurrent = *it;
		if (pCurrent->GetCurrentActor() == pActor)
		{
			InformListenerAboutSignalProcessingFinished(pCurrent->GetSignalName()
				, pCurrent->GetOriginalSender()
				, pCurrent->GetContextVariablesImpl()
				, pCurrent->GetSignalInstanceId()
				, pCurrent
				, IResponseManager::IListener::ProcessingResult_Canceled);
			pCurrent->Cancel();
			pCurrent->Update();
			ReleaseInstance(pCurrent, false);
			it = m_runningResponses.erase(it);
		}
		else
		{
			++it;
		}
	}

	auto newEndIter = std::remove_if(m_currentlyQueuedSignals.begin(), m_currentlyQueuedSignals.end(),
		[pActor](const SSignal& signal) { return signal.m_pSender == pActor; });

	if (newEndIter != m_currentlyQueuedSignals.end())
	{
		// move signals away to temporary list and report their removal, as it may trigger callbacks and change m_currentlyQueuedSignals
		SignalList tempSignalsToRemove;
		tempSignalsToRemove.reserve(std::distance(newEndIter, m_currentlyQueuedSignals.end()));
		std::move(newEndIter, m_currentlyQueuedSignals.end(), std::back_inserter(tempSignalsToRemove));
		m_currentlyQueuedSignals.erase(newEndIter, m_currentlyQueuedSignals.end());

		for (const auto& signal : tempSignalsToRemove)
		{
			InformListenerAboutSignalProcessingFinished(signal.m_signalName, signal.m_pSender, signal.m_pSignalContext, signal.m_id, nullptr, DRS::IResponseManager::IListener::ProcessingResult_Canceled);
		}
	}
}

//--------------------------------------------------------------------------------------------------
DRS::SignalInstanceId g_currentSignalId = 0;

SSignal::SSignal(
  const CHashedString& signalName,
  CResponseActor* pSender,
  VariableCollectionSharedPtr pSignalContext)
	: m_signalName(signalName)
	, m_pSender(pSender)
	, m_pSignalContext(pSignalContext)
	, m_id(++g_currentSignalId)
{
}
}