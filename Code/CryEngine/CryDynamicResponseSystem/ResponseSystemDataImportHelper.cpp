// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ResponseSystemDataImportHelper.h"
#include "ActionCancelSignal.h"
#include "ActionSendSignal.h"
#include "ActionSetActor.h"
#include "ActionSetVariable.h"
#include "ActionWait.h"
#include "ActionSpeakLine.h"
#include "ConditionImpl.h"
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include "SpecialConditionsImpl.h"
#include "ResponseManager.h"
#include "VariableCollection.h"
#include "ResponseSystem.h"

using namespace CryDRS;

#define CHECK_CORRECT_FORMAT                                                                                                                                                                                       \
  if (strcmp(szFormatName, "CryDrsInternal") != 0)                                                                                                                                                                 \
  {                                                                                                                                                                                                                \
    DrsLogWarning((string("Could not create Action from string with the given Format '") + string(szFormatName) + "', only the Format 'CryDrsInternal' is supported. CreationData was: " + creationData).c_str()); \
    return nullptr;                                                                                                                                                                                                \
  }

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionSharedPtr CreatorCancelSignalAction(const string& creationData, const char* szFormatName)
{
	CHECK_CORRECT_FORMAT
	return DRS::IResponseActionSharedPtr(new CActionCancelSignal(creationData));
}
//--------------------------------------------------------------------------------------------------
DRS::IResponseActionSharedPtr CreatorSendSignalAction(const string& creationData, const char* szFormatName)
{
	CHECK_CORRECT_FORMAT
	return DRS::IResponseActionSharedPtr(new CActionSendSignal(creationData, true));
}
//--------------------------------------------------------------------------------------------------
DRS::IResponseActionSharedPtr CreatorSetActorAction(const string& creationData, const char* szFormatName)
{
	CHECK_CORRECT_FORMAT
	return DRS::IResponseActionSharedPtr(new CActionSetActor(creationData, nullptr));
}
//--------------------------------------------------------------------------------------------------
DRS::IResponseActionSharedPtr CreatorWaitAction(const string& creationData, const char* szFormatName)
{
	CHECK_CORRECT_FORMAT
	return DRS::IResponseActionSharedPtr(new CActionWait((float)atof(creationData.c_str())));
}
//--------------------------------------------------------------------------------------------------
DRS::IResponseActionSharedPtr CreateSpeakLineAction(const string& creationData, const char* szFormatName)
{
	CHECK_CORRECT_FORMAT
	return DRS::IResponseActionSharedPtr(new CActionSpeakLine(creationData));
}
//--------------------------------------------------------------------------------------------------
DRS::IResponseActionSharedPtr CreatorSetVariableAction(const string& creationData, const char* szFormatName)
{
	CHECK_CORRECT_FORMAT

	CActionSetVariable::EChangeOperation operation;
	CVariableValue targetValue;

	std::vector<string> parameters;

	if (creationData.find('=') != string::npos)
	{
		operation = CActionSetVariable::eChangeOperation_Set;
		CDataImportHelper::splitStringList(creationData, '=', '\0', &parameters);
	}
	else if (creationData.find('+') != string::npos)
	{
		operation = CActionSetVariable::eChangeOperation_Increment;
		CDataImportHelper::splitStringList(creationData, '+', '\0', &parameters);

	}
	else if (creationData.find('-') != string::npos)
	{
		operation = CActionSetVariable::eChangeOperation_Decrement;
		CDataImportHelper::splitStringList(creationData, '-', '\0', &parameters);
	}
	else
		return nullptr;

	if (parameters.size() == 2)
	{
		std::vector<string> collectionAndVariable;
		if (CDataImportHelper::splitStringList(parameters[0], ':', '\0', &collectionAndVariable))
		{
			std::vector<string> variableValueAndCooldown;
			float cooldown = -1.0f;
			if (CDataImportHelper::splitStringList(parameters[1], ' ', '\0', &variableValueAndCooldown))
			{
				cooldown = (float)atof(variableValueAndCooldown[1]);
				CConditionParserHelper::GetResponseVariableValueFromString(variableValueAndCooldown[0].c_str(), &targetValue);
			}
			else
			{
				CConditionParserHelper::GetResponseVariableValueFromString(parameters[1].c_str(), &targetValue);
			}

			return DRS::IResponseActionSharedPtr(new CActionSetVariable(collectionAndVariable[0], collectionAndVariable[1], targetValue, operation, cooldown));  //cooldown
		}
	}

	DrsLogWarning((string("Wrong data for SetVariable action: '") + creationData + "'").c_str());
	return nullptr;
}
//--------------------------------------------------------------------------------------------------
DRS::IConditionSharedPtr CreatorVariableCondition(const string& creationData, const char* szFormatName)
{
	CHECK_CORRECT_FORMAT
	string collectionAndVariable;
	CVariableValue value1 = CVariableValue::NEG_INFINITE;
	CVariableValue value2 = CVariableValue::POS_INFINITE;

	std::vector<string> parameters;
	CDataImportHelper::splitStringList(creationData, '>', '<', &parameters);
	if (parameters.size() >= 2)
	{
		if (parameters.size() >= 3)  //Format: value1>[vc-name]:[v-name]<value2
		{
			collectionAndVariable = parameters[1];
			CConditionParserHelper::GetResponseVariableValueFromString(parameters[0].c_str(), &value1);
			CConditionParserHelper::GetResponseVariableValueFromString(parameters[2].c_str(), &value2);
		}
		else
		{
			if (creationData.find('>') != string::npos)  //Format: [vc-name]:[v-name]>value1
			{
				collectionAndVariable = parameters[0];
				CConditionParserHelper::GetResponseVariableValueFromString(parameters[1].c_str(), &value1);
			}
			else  //Format: [vc-name]:[v-name]<value2
			{
				collectionAndVariable = parameters[0];
				CConditionParserHelper::GetResponseVariableValueFromString(parameters[1].c_str(), &value2);
			}
		}
	}
	else  //Format: [vc-name]:[v-name]=value1
	{
		parameters.clear();
		CDataImportHelper::splitStringList(creationData, '=', '\0', &parameters);
		if (parameters.size() == 2)
		{
			collectionAndVariable = parameters[0];
			CConditionParserHelper::GetResponseVariableValueFromString(parameters[1].c_str(), &value1);
			value2 = value1;
		}
		else
		{
			DrsLogWarning((string("Wrong parameters for variable condition: '") + creationData + "'").c_str());
			return nullptr;
		}
	}

	parameters.clear();
	CDataImportHelper::splitStringList(collectionAndVariable, ':', '\0', &parameters);
	bool bIsTimeSinceCondition = (parameters.size() == 3) && (parameters[2] == "TimeSince" || parameters[2] == "timeSince");
	if (parameters.size() != 2 && !bIsTimeSinceCondition)
	{
		DrsLogWarning((string("Wrong parameters for variable condition: '") + creationData + "'").c_str());
		return nullptr;
	}

	const CHashedString collectionName = parameters[0];
	const CHashedString variableName = parameters[1];
	static const CHashedString gameToken = "gametoken";

	if (CVariableCollection::s_localCollectionName == collectionName)
	{
		//if (!bIsTimeSinceCondition)
		//	return DRS::IConditionSharedPtr(new CConditionLocalVariable(variableName, value1, value2));
		//else
		//	return DRS::IConditionSharedPtr(new CTimeSinceConditionLocal(variableName, value1, value2));
		return nullptr;
	}
	else if (CVariableCollection::s_contextCollectionName == collectionName)
	{
		//if (!bIsTimeSinceCondition)
		//	return DRS::IConditionSharedPtr(new CConditionContextVariable(variableName, value1, value2));
		//else
		//	return DRS::IConditionSharedPtr(new CTimeSinceConditionContext(variableName, value1, value2));
		return nullptr;
	}
	else if (collectionName == gameToken)
	{
		return DRS::IConditionSharedPtr(new CGameTokenCondition(parameters[1], value1, value2));
	}
	else
	{
		CVariableCollection* pCollection = CResponseSystem::GetInstance()->GetCollection(collectionName);
		if (!pCollection)
		{
			DrsLogWarning((string("Could not find the variable collection: '") + collectionName.GetText() + "'").c_str());
			return nullptr;
		}
		CVariable* pVariable = pCollection->GetVariable(variableName);
		if (!pVariable)
		{
			DrsLogWarning((string("Could not find the variable: '") + variableName.GetText() + "'").c_str());
			return nullptr;
		}

		//if (!bIsTimeSinceCondition)
		//	return DRS::IConditionSharedPtr(new CConditionGlobalVariable(pVariable, value1, value2));
		//else
		//	return DRS::IConditionSharedPtr(new CTimeSinceConditionGlobal(pVariable, value1, value2));
		return nullptr;
	}
}
//--------------------------------------------------------------------------------------------------
DRS::IConditionSharedPtr CreatorRandomCondition(const string& creationData, const char* szFormatName)
{
	CHECK_CORRECT_FORMAT
	return DRS::IConditionSharedPtr(new CRandomCondition(atoi(creationData.c_str())));
}

//--------------------------------------------------------------------------------------------------
CDataImportHelper::CDataImportHelper()
{
	RegisterActionCreator("CancelSignal", CreatorCancelSignalAction);
	RegisterActionCreator("SendSignal", CreatorSendSignalAction);
	RegisterActionCreator("SetActor", CreatorSetActorAction);
	RegisterActionCreator("SetVariable", CreatorSetVariableAction);
	RegisterActionCreator("Wait", CreatorWaitAction);
	RegisterActionCreator("SpeakLine", CreateSpeakLineAction);

	RegisterConditionCreator("VariableCondition", CreatorVariableCondition);
	RegisterConditionCreator("RandomCondition", CreatorRandomCondition);
}

//--------------------------------------------------------------------------------------------------
bool CDataImportHelper::AddResponseCondition(DRS::IDataImportHelper::ResponseID responseID, DRS::IConditionSharedPtr pCondition, bool bNegated)
{
	CHashedString signalName((uint32)responseID);
	CResponseManager::MappedSignals::iterator itFound = m_mappedSignals.find(signalName);
	if (itFound == m_mappedSignals.end())
		return false;

	ResponsePtr& response = itFound->second;
	//if (!response->m_pBaseSegment)
	//{
	//	response->m_pBaseSegment = new CResponseSegment();
	//}
	//response->m_pBaseSegment->AddCondition(pCondition, bNegated);

	return true;
}

//--------------------------------------------------------------------------------------------------
bool CDataImportHelper::AddResponseAction(DRS::IDataImportHelper::ResponseID responseID, DRS::IResponseActionSharedPtr pAction)
{
	//todo delay
	CHashedString signalName((uint32)responseID);
	CResponseManager::MappedSignals::iterator itFound = m_mappedSignals.find(signalName);
	if (itFound == m_mappedSignals.end())
		return false;

	ResponsePtr& response = itFound->second;
	//if (!response->m_pBaseSegment)
	//{
	//	response->m_pBaseSegment = new CResponseSegment();
	//}
	//response->m_pBaseSegment->AddAction(pAction);

	return true;
}

//--------------------------------------------------------------------------------------------------
DRS::IDataImportHelper::ResponseSegmentID CDataImportHelper::AddResponseSegment(ResponseID parentResponse, const string& szName)
{
	CHashedString signalName((uint32)parentResponse);
	CResponseManager::MappedSignals::iterator itFound = m_mappedSignals.find(signalName);
	if (itFound == m_mappedSignals.end())
		return INVALID_ID;
	ResponsePtr& response = itFound->second;
	CResponseSegment temp;
#if !defined(_RELEASE)
	temp.m_name = szName;
#endif
	//response->SetProperties(response->GetProperties() | CResponse::eRP_HasAdditionalSegments);
	//response->m_responseSegments.push_back(temp);
	//return response->m_responseSegments.size() - 1;
	return 666;
}

//--------------------------------------------------------------------------------------------------
bool CDataImportHelper::AddResponseSegmentAction(ResponseID parentResponse, ResponseSegmentID segmentID, DRS::IResponseActionSharedPtr pAction)
{
	CHashedString signalName((uint32)parentResponse);
	CResponseManager::MappedSignals::iterator itFound = m_mappedSignals.find(signalName);
	if (itFound == m_mappedSignals.end())
		return false;
	ResponsePtr& response = itFound->second;
	//if (response->m_responseSegments.size() <= segmentID)
	//	return false;
	//CResponseSegment& responseSegment = response->m_responseSegments[segmentID];
	//responseSegment.AddAction(pAction);
	return true;
}

//--------------------------------------------------------------------------------------------------
bool CDataImportHelper::AddResponseSegmentCondition(ResponseID parentResponse, ResponseSegmentID segmentID, DRS::IConditionSharedPtr pConditions, bool bNegated)
{
	CHashedString signalName((uint32)parentResponse);
	CResponseManager::MappedSignals::iterator itFound = m_mappedSignals.find(signalName);
	if (itFound == m_mappedSignals.end())
		return false;
	ResponsePtr& response = itFound->second;
	//if (response->m_responseSegments.size() <= segmentID)
	//	return false;
	//CResponseSegment& responseSegment = response->m_responseSegments[segmentID];

	//if (!responseSegment.m_pConditions)
	//{
	//	responseSegment.m_pConditions = new CConditionsCollection();
	//}
	//responseSegment.m_pConditions->AddCondition(pConditions, bNegated);
	return true;
}

//--------------------------------------------------------------------------------------------------
DRS::IDataImportHelper::ResponseID CDataImportHelper::AddSignalResponse(const string& szName)
{
	CHashedString signalName = szName;
	CResponseManager::MappedSignals::iterator itFound = m_mappedSignals.find(signalName);
	if (itFound == m_mappedSignals.end())
	{
		m_mappedSignals[signalName] = ResponsePtr(new CResponse);
	}

	return signalName.GetHash();
}

//--------------------------------------------------------------------------------------------------
void CDataImportHelper::RegisterConditionCreator(const CHashedString& conditionType, CondtionCreatorFct pFunc)
{
	m_conditionCreators[conditionType] = pFunc;
}

//--------------------------------------------------------------------------------------------------
void CDataImportHelper::RegisterActionCreator(const CHashedString& actionType, ActionCreatorFct pFunc)
{
	m_actionsCreators[actionType] = pFunc;
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionSharedPtr CDataImportHelper::CreateActionFromString(const CHashedString& type, const string& data, const char* szFormatName)
{
	std::unordered_map<CHashedString, ActionCreatorFct>::iterator itFound = m_actionsCreators.find(type);
	if (m_actionsCreators.end() != itFound)
	{
		return itFound->second(data, szFormatName);
	}
	DrsLogWarning((string("Could not find a creator method for actions of type '") + type.GetText() + "' data was: " + data).c_str());
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
DRS::IConditionSharedPtr CDataImportHelper::CreateConditionFromString(const CHashedString& type, const string& data, const char* szFormatName)
{
	std::unordered_map<CHashedString, CondtionCreatorFct>::iterator itFound = m_conditionCreators.find(type);
	if (m_conditionCreators.end() != itFound)
	{
		return itFound->second(data, szFormatName);
	}
	DrsLogWarning((string("Could not find a creator method for condition of type '") + type.GetText() + "' data was: " + data).c_str());
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
bool CDataImportHelper::HasActionCreatorForType(const CHashedString& type)
{
	std::unordered_map<CHashedString, ActionCreatorFct>::iterator itFound = m_actionsCreators.find(type);
	return m_actionsCreators.end() != itFound;
}

//--------------------------------------------------------------------------------------------------
bool CDataImportHelper::HasConditionCreatorForType(const CHashedString& type)
{
	std::unordered_map<CHashedString, CondtionCreatorFct>::iterator itFound = m_conditionCreators.find(type);
	return m_conditionCreators.end() != itFound;
}

//--------------------------------------------------------------------------------------------------
void CDataImportHelper::Serialize(Serialization::IArchive& ar)
{
	ar(m_mappedSignals, "signals_responses", "+Signal Responses:");
}

//--------------------------------------------------------------------------------------------------
void CDataImportHelper::Reset()
{
	m_mappedSignals.clear();
}

//--------------------------------------------------------------------------------------------------
bool CDataImportHelper::splitStringList(const string& stringToSplitUp, const char delimeter1, const char delimeter2, std::vector<string>* outResult, bool bTrim)
{
	bool bWasFound = false;
	const int stringLength = stringToSplitUp.size();
	if (stringLength > 1)
	{
		const char* szCurrentPos = stringToSplitUp.c_str();
		const char* szCurrentPosStart = stringToSplitUp.c_str();
		for (int i = 0; i < stringLength; i++)
		{
			if (*szCurrentPos == delimeter1 || *szCurrentPos == delimeter2)
			{
				bWasFound = true;
				outResult->push_back(string(szCurrentPosStart, szCurrentPos));
				szCurrentPosStart = szCurrentPos + 1;
			}
			++szCurrentPos;
		}
		outResult->push_back(string(szCurrentPosStart, szCurrentPos));
	}

	if (bTrim)
	{
		for (std::vector<string>::iterator it = outResult->begin(); it != outResult->end(); ++it)
		{
			it->Trim();
		}
	}

	return bWasFound;
}
