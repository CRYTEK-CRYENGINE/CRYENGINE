// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include <CrySerialization/IArchiveHost.h>
#include <CryDynamicResponseSystem/IDynamicResponseCondition.h>
#include <CryGame/IGameFramework.h>

#include "ResponseSystemDataImportHelper.h"
#include "DialogLineDatabase.h"
#include "ResponseSystem.h"
#include "ResponseManager.h"
#include "ResponseInstance.h"
#include "VariableCollection.h"
#include "ResponseInstance.h"
#include "SchematycEntityDrsComponent.h"
#include "ActionCancelSignal.h"
#include "ActionSpeakLine.h"
#include "ActionSetVariable.h"
#include "ActionCopyVariable.h"
#include "ActionExecuteResponse.h"
#include "ActionResetTimer.h"
#include "ActionSendSignal.h"
#include "ActionSetActor.h"
#include "ActionSetGameToken.h"
#include "ActionWait.h"
#include "SpecialConditionsImpl.h"
#include "ConditionImpl.h"

using namespace CryDRS;

CResponseSystem* CResponseSystem::s_pInstance = nullptr;

void ReloadResponseDefinition(IConsoleCmdArgs* pArgs)
{
	CResponseSystem::GetInstance()->ReInit();
}

//--------------------------------------------------------------------------------------------------
void SendDrsSignal(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() >= 3)
	{
		CResponseSystem* pDrs = CResponseSystem::GetInstance();
		if (pDrs)
		{
			DRS::IResponseActor* pSender = pDrs->GetResponseActor(pArgs->GetArg(1));
			if (pSender)
			{
				pSender->QueueSignal(pArgs->GetArg(2));
				return;
			}
			else
			{
				DrsLogWarning((string("Could not find an actor with that name: ") + pArgs->GetArg(1)).c_str());
			}
		}
	}

	DrsLogWarning("Format: SendDrsSignal <senderEntityName> <signalname>");
}

//--------------------------------------------------------------------------------------------------
void ChangeFileFormat(ICVar* pICVar)
{
	if (!pICVar)
		return;

	const char* pVal = pICVar->GetString();

	CResponseManager::EUsedFileFormat fileFormat = CResponseManager::eUFF_JSON;
	if (strcmp(pVal, "BIN") == 0)
		fileFormat = CResponseManager::eUFF_BIN;
	else if (strcmp(pVal, "XML") == 0)
		fileFormat = CResponseManager::eUFF_XML;

	CResponseSystem::GetInstance()->GetResponseManager()->SetFileFormat(fileFormat);
}

//--------------------------------------------------------------------------------------------------
CResponseSystem::CResponseSystem()
{
	m_bIsCurrentlyUpdating = false;
	m_pendingResetRequest = DRS::IDynamicResponseSystem::eResetHint_Nothing;
	m_pDataImporterHelper = nullptr;

	m_pSpeakerManager = new CSpeakerManager();
	m_pDialogLineDatabase = new CDialogLineDatabase();
	m_pVariableCollectionManager = new CVariableCollectionManager();
	m_pResponseManager = new CResponseManager();
	CRY_ASSERT(s_pInstance == nullptr && "ResponseSystem was created more than once!");
	s_pInstance = this;

	REGISTER_COMMAND("drs_sendSignal", SendDrsSignal, VF_NULL, "Sends a DRS Signal by hand. Useful for testing. Format SendDrsSignal <senderEntityName> <signalname>");

	m_pUsedFileFormat = REGISTER_STRING_CB("drs_fileFormat", "JSON", VF_NULL, "Specifies the file format to use (JSON, XML, BIN)", ::ChangeFileFormat);
    m_pDataPath = REGISTER_STRING("drs_dataPath", "Libs" CRY_NATIVE_PATH_SEPSTR "DynamicResponseSystem", VF_NULL, "Specifies the path where to find the response and dialogline files");

	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CResponseSystem");

#if defined(DRS_COLLECT_DEBUG_DATA)
	m_responseSystemDebugDataProvider.Init();
#endif
}

//--------------------------------------------------------------------------------------------------
CResponseSystem::~CResponseSystem()
{
	if (gEnv->pSchematyc != nullptr)
	{
		gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(GetSchematycPackageGUID());
	}

	for (CResponseActor* pActor : m_createdActors)
	{
		delete pActor;
	}
	m_createdActors.clear();

	gEnv->pConsole->UnregisterVariable("drs_fileFormat", true);
	gEnv->pConsole->UnregisterVariable("drs_dataPath", true);
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	delete m_pSpeakerManager;
	delete m_pDialogLineDatabase;

	delete m_pDataImporterHelper;
	delete m_pResponseManager;
	delete m_pVariableCollectionManager;
	s_pInstance = nullptr;
}

//--------------------------------------------------------------------------------------------------
bool CResponseSystem::Init()
{	
	m_filesFolder = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + m_pDataPath->GetString();
	m_pSpeakerManager->Init();
	m_pDialogLineDatabase->InitFromFiles(m_filesFolder + CRY_NATIVE_PATH_SEPSTR "DialogLines");
	
	m_currentTime.SetSeconds(0.0f);

	m_pVariableCollectionManager->GetCollection(CVariableCollection::s_globalCollectionName)->CreateVariable("CurrentTime", 0.0f);

	return m_pResponseManager->LoadFromFiles(m_filesFolder + CRY_NATIVE_PATH_SEPSTR "Responses");
}

//--------------------------------------------------------------------------------------------------
bool CResponseSystem::ReInit()
{
	Reset();  //stop running responses and reset variables
	m_pDialogLineDatabase->InitFromFiles(m_filesFolder + CRY_NATIVE_PATH_SEPSTR "DialogLines");
	return m_pResponseManager->LoadFromFiles(m_filesFolder + CRY_NATIVE_PATH_SEPSTR "Responses");
}

//--------------------------------------------------------------------------------------------------
void CResponseSystem::Update()
{
	if (m_pendingResetRequest != DRS::IDynamicResponseSystem::eResetHint_Nothing)
	{
		InternalReset(m_pendingResetRequest);
		m_pendingResetRequest = DRS::IDynamicResponseSystem::eResetHint_Nothing;
	}

	m_currentTime += gEnv->pTimer->GetFrameTime();
	
	m_pVariableCollectionManager->GetCollection(CVariableCollection::s_globalCollectionName)->SetVariableValue("CurrentTime", m_currentTime.GetSeconds());

	m_bIsCurrentlyUpdating = true;
	m_pVariableCollectionManager->Update();
	m_pResponseManager->Update();
	m_pSpeakerManager->Update();
	m_bIsCurrentlyUpdating = false;
}

//--------------------------------------------------------------------------------------------------
CVariableCollection* CResponseSystem::CreateVariableCollection(const CHashedString& name)
{
	return m_pVariableCollectionManager->CreateVariableCollection(name);
}
//--------------------------------------------------------------------------------------------------
void CResponseSystem::ReleaseVariableCollection(DRS::IVariableCollection* pToBeReleased)
{
	m_pVariableCollectionManager->ReleaseVariableCollection(static_cast<CVariableCollection*>(pToBeReleased));
}

//--------------------------------------------------------------------------------------------------
CVariableCollection* CResponseSystem::GetCollection(const CHashedString& name)
{
	return m_pVariableCollectionManager->GetCollection(name);
}

//--------------------------------------------------------------------------------------------------
CVariableCollection* CResponseSystem::GetCollection(const CHashedString& name, DRS::IResponseInstance* pResponseInstance)
{
	// taken from IVariableUsingBase::GetCurrentCollection
	if (name == CVariableCollection::s_localCollectionName)  //local variable collection
	{
		CResponseActor* pCurrentActor = static_cast<CResponseInstance*>(pResponseInstance)->GetCurrentActor();
		return pCurrentActor->GetLocalVariables();
	}
	else if (name == CVariableCollection::s_contextCollectionName)  //context variable collection
	{
		return static_cast<CResponseInstance*>(pResponseInstance)->GetContextVariablesImpl().get();
	}
	else
	{
		static CVariableCollectionManager* const pVariableCollectionMgr = CResponseSystem::GetInstance()->GetVariableCollectionManager();
		return pVariableCollectionMgr->GetCollection(name);
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
CDataImportHelper* CResponseSystem::GetCustomDataformatHelper()
{
	if (!m_pDataImporterHelper)
	{
		m_pDataImporterHelper = new CDataImportHelper();
	}
	return m_pDataImporterHelper;
}

//--------------------------------------------------------------------------------------------------
CResponseActor* CResponseSystem::CreateResponseActor(const char* szActorName, EntityId entityID, const char* szGlobalVariableCollectionToUse)
{
	CResponseActor* newActor = nullptr;
	CResponseActor* pExistingActor = GetResponseActor(CHashedString(szActorName));
	if(pExistingActor)
	{
		string actorname(szActorName);
		uint32 uniqueActorNameCounter = 1;
		while (pExistingActor)
		{
			actorname.Format("%s_%i", szActorName, uniqueActorNameCounter++);
			pExistingActor = GetResponseActor(CHashedString(actorname));
		}
		DrsLogInfo(string().Format("Actor with name: '%s' was already existing. Renamed it to '%s'", szActorName, actorname).c_str());
		newActor = new CResponseActor(actorname, entityID, szGlobalVariableCollectionToUse);
	}
	else
	{
		newActor = new CResponseActor(szActorName, entityID, szGlobalVariableCollectionToUse);
	}
	
	m_createdActors.push_back(newActor);
	return newActor;
}

//--------------------------------------------------------------------------------------------------
bool CResponseSystem::ReleaseResponseActor(DRS::IResponseActor* pActorToFree)
{
	for (ResponseActorList::iterator it = m_createdActors.begin(), itEnd = m_createdActors.end(); it != itEnd; ++it)
	{
		if (*it == pActorToFree)
		{
			delete *it;
			m_createdActors.erase(it);
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
CResponseActor* CResponseSystem::GetResponseActor(const CHashedString& actorName)
{
	for (CResponseActor* pActor : m_createdActors)
	{
		if (pActor->GetNameHashed() == actorName)
		{
			return pActor;
		}
	}
	return nullptr;
}

//--------------------------------------------------------------------------------------------------
void CResponseSystem::Reset(uint32 resetHint)
{
	if (m_bIsCurrentlyUpdating)
	{
		//we delay the actual execution until the beginning of the next update, because we do not want the reset to happen, while we are iterating over the currently running responses
		m_pendingResetRequest = resetHint;
	}
	else
	{
		InternalReset(resetHint);
	}
}

//--------------------------------------------------------------------------------------------------
bool CResponseSystem::CancelSignalProcessing(const CHashedString& signalName, DRS::IResponseActor* pSender /* = nullptr */, DRS::SignalInstanceId instanceToSkip /* = s_InvalidSignalId */)
{
	SSignal temp(signalName, static_cast<CResponseActor*>(pSender), nullptr);
	temp.m_id = instanceToSkip;
	return m_pResponseManager->CancelSignalProcessing(temp);
}

//--------------------------------------------------------------------------------------------------
DRS::ActionSerializationClassFactory& CResponseSystem::GetActionSerializationFactory()
{
	return Serialization::ClassFactory<DRS::IResponseAction>::the();
}

//--------------------------------------------------------------------------------------------------
DRS::ConditionSerializationClassFactory& CResponseSystem::GetConditionSerializationFactory()
{
	return Serialization::ClassFactory<DRS::IResponseCondition>::the();
}

//--------------------------------------------------------------------------------------------------
void CResponseSystem::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("GlobalVariableCollections", "Global Variable Collections"))
	{
		m_pVariableCollectionManager->Serialize(ar);
		ar.closeBlock();
	}

#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.isEdit())
	{
		if (ar.openBlock("LocalVariableCollections", "Local Variable Collections"))
		{
			for (CResponseActor* pActor : m_createdActors)
			{
				if (pActor->GetNonGlobalVariableCollection())
				{
					VariableCollectionSharedPtr pCollection = pActor->GetNonGlobalVariableCollection();
					ar(*pCollection, pCollection->GetName().m_textCopy, pCollection->GetName().m_textCopy);
				}
				else
				{
					string temp = "uses global collection: " + pActor->GetCollectionName().GetText();
					ar(temp, pActor->GetName(), pActor->GetName());
				}
			}
			ar.closeBlock();
		}
	}
#endif

	m_pResponseManager->SerializeResponseStates(ar);

	m_currentTime.SetSeconds(m_pVariableCollectionManager->GetCollection(CVariableCollection::s_globalCollectionName)->GetVariableValue("CurrentTime").GetValueAsFloat());  //we serialize our DRS time manually via the time variable
}

//--------------------------------------------------------------------------------------------------
DRS::IVariableCollectionSharedPtr CResponseSystem::CreateContextCollection()
{
	static int uniqueNameCounter = 0;
	return std::make_shared<CVariableCollection>(string().Format("Context_%i", ++uniqueNameCounter));  //drs-todo: pool me
}

//--------------------------------------------------------------------------------------------------
void CResponseSystem::OnSystemEvent(ESystemEvent event, UINT_PTR pWparam, UINT_PTR pLparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		//stop active responses + active speakers on level change
		Reset(DRS::IDynamicResponseSystem::eResetHint_StopRunningResponses | DRS::IDynamicResponseSystem::eResetHint_Speaker);
		break;
	case ESYSTEM_EVENT_INITIALIZE_DRS:  //= ESYSTEM_EVENT_REGISTER_SCHEMATYC_ENV
		{
			Init();

			if (gEnv->pSchematyc)
			{
				const char* szName = "DynamicResponseSystem";
				const char* szDescription = "Dynamic response system";
				Schematyc::EnvPackageCallback callback = SCHEMATYC_MEMBER_DELEGATE(&CResponseSystem::RegisterSchematycEnvPackage, *this);
				gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(SCHEMATYC_MAKE_ENV_PACKAGE(GetSchematycPackageGUID(), szName, Schematyc::g_szCrytek, szDescription, callback));
			}
			break;
		}
	case ESYSTEM_EVENT_GAME_FRAMEWORK_ABOUT_TO_SHUTDOWN:
		{
			m_pSpeakerManager->Shutdown();
			m_pResponseManager->Reset(false, true);  //We have to release all mapped responses here, because they might contain game specific actions/conditions.
		}
		break;
	}
}

//--------------------------------------------------------------------------------------------------
void CResponseSystem::RegisterSchematycEnvPackage(Schematyc::IEnvRegistrar& registrar)
{
	CSchematycEntityDrsComponent::Register(registrar);
}

//--------------------------------------------------------------------------------------------------
VariableCollectionSharedPtr CResponseSystem::CreateLocalCollection(const string& name)
{
	return std::make_shared<CVariableCollection>(name);  //drs-todo: pool me
}

//--------------------------------------------------------------------------------------------------
#if !defined(_RELEASE)
void CResponseSystem::SetCurrentDrsUserName(const char* szNewDrsUserName)
{
	m_currentDrsUserName = szNewDrsUserName;
}
#endif

//--------------------------------------------------------------------------------------------------
void CResponseSystem::InternalReset(uint32 resetFlags)
{
	if (resetFlags & (DRS::IDynamicResponseSystem::eResetHint_StopRunningResponses | DRS::IDynamicResponseSystem::eResetHint_ResetAllResponses))
	{
		m_pResponseManager->Reset((resetFlags& DRS::IDynamicResponseSystem::eResetHint_ResetAllResponses) > 0);
	}
	if (resetFlags & DRS::IDynamicResponseSystem::eResetHint_Variables)
	{
		m_pVariableCollectionManager->Reset();
		m_currentTime.SetSeconds(0.0f);
		m_pVariableCollectionManager->GetCollection(CVariableCollection::s_globalCollectionName)->SetVariableValue("CurrentTime", m_currentTime.GetSeconds());

		//also reset the local variable collections (which are not managed by the variable collection manager)
		for (CResponseActor* pActor : m_createdActors)
		{
			if (pActor->GetNonGlobalVariableCollection())
			{
				pActor->GetNonGlobalVariableCollection()->Reset();
			}
		}
	}
	if (resetFlags & DRS::IDynamicResponseSystem::eResetHint_Speaker)
	{
		m_pSpeakerManager->Reset();
		m_pDialogLineDatabase->Reset();
	}
	if (resetFlags & DRS::IDynamicResponseSystem::eResetHint_DebugInfos)
	{
		DRS_DEBUG_DATA_ACTION(Reset());
	}
}
constexpr const char* szDrsDataTag = "<DRS_DATA>";
constexpr const char* szDrsVariablesStartTag = "VariablesStart";
constexpr const char* szDrsVariablesEndTag = "VariablesEnd";
constexpr const char* szDrsResponsesStartTag = "ResponsesStart";
constexpr const char* szDrsResponsesEndTag = "ResponsesEnd";
constexpr const char* szDrsLinesStartTag = "LinesStart";
constexpr const char* szDrsLinesEndTag = "LinesEnd";

//--------------------------------------------------------------------------------------------------
DRS::ValuesListPtr CResponseSystem::GetCurrentState(uint32 saveHints) const
{
	const bool bSkipDefaultValues = (saveHints & SaveHints_SkipDefaultValues) > 0;

	DRS::ValuesListPtr variableList(new DRS::ValuesList(), [](DRS::ValuesList* pList)
	{
		pList->clear();
		delete pList;
	});

	std::pair<DRS::ValuesString, DRS::ValuesString> temp;
	temp.first = szDrsDataTag;
	if (saveHints & SaveHints_Variables)
	{
		temp.second = szDrsVariablesStartTag;
		variableList->push_back(temp);
		m_pVariableCollectionManager->GetAllVariableCollections(variableList.get(), bSkipDefaultValues);
		temp.second = szDrsVariablesEndTag;
		variableList->push_back(temp);
	}

	if (saveHints & SaveHints_ResponseData)
	{
		temp.second = szDrsResponsesStartTag;
		variableList->push_back(temp);
		m_pResponseManager->GetAllResponseData(variableList.get(), bSkipDefaultValues);
		temp.second = szDrsResponsesEndTag;
		variableList->push_back(temp);
	}

	if (saveHints & SaveHints_LineData)
	{
		temp.second = szDrsLinesStartTag;
		variableList->push_back(temp);
		m_pDialogLineDatabase->GetAllLineData(variableList.get(), bSkipDefaultValues);
		temp.second = szDrsLinesEndTag;
		variableList->push_back(temp);
	}

	return variableList;
}

//--------------------------------------------------------------------------------------------------
void CResponseSystem::SetCurrentState(const DRS::ValuesList& collectionsList)
{
	DRS::ValuesListIterator itStartOfVariables = collectionsList.begin();
	DRS::ValuesListIterator itStartOfResponses = collectionsList.begin();
	DRS::ValuesListIterator itStartOfLines = collectionsList.begin();
	DRS::ValuesListIterator itEndOfVariables = collectionsList.begin();
	DRS::ValuesListIterator itEndOfResponses = collectionsList.begin();
	DRS::ValuesListIterator itEndOfLines = collectionsList.begin();

	for (DRS::ValuesListIterator it = collectionsList.begin(); it != collectionsList.end(); ++it)
	{
		if (it->first == szDrsDataTag)
		{
			if (it->second == szDrsVariablesStartTag)
				itStartOfVariables = it + 1;
			if (it->second == szDrsVariablesEndTag)
				itEndOfVariables = it;
		}
	}

	for (DRS::ValuesListIterator it = collectionsList.begin(); it != collectionsList.end(); ++it)
	{
		if (it->first == szDrsDataTag)
		{
			if (it->second == szDrsResponsesStartTag)
				itStartOfResponses = it + 1;
			if (it->second == szDrsResponsesEndTag)
				itEndOfResponses = it;
		}
	}

	for (DRS::ValuesListIterator it = collectionsList.begin(); it != collectionsList.end(); ++it)
	{
		if (it->first == szDrsDataTag)
		{
			if (it->second == szDrsLinesStartTag)
				itStartOfLines = it + 1;
			if (it->second == szDrsLinesEndTag)
				itEndOfLines = it;
		}
	}

	if (itStartOfVariables != itEndOfVariables)
	{
		m_pVariableCollectionManager->SetAllVariableCollections(itStartOfVariables, itEndOfVariables);
		//we serialize our DRS time manually via the "CurrentTime" variable
		m_currentTime.SetSeconds(m_pVariableCollectionManager->GetCollection(CVariableCollection::s_globalCollectionName)->GetVariableValue("CurrentTime").GetValueAsFloat());
	}

	if (itStartOfResponses != itEndOfResponses)
	{
		m_pResponseManager->SetAllResponseData(itStartOfResponses, itEndOfResponses);
	}

	if (itStartOfLines != itEndOfLines)
	{
		m_pDialogLineDatabase->SetAllLineData(itStartOfLines, itEndOfLines);
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

CResponseActor::CResponseActor(const string& name, EntityId usedEntityID, const char* szGlobalVariableCollectionToUse)
	: m_linkedEntityID(usedEntityID)
	, m_name(name)
	, m_auxAudioObjectIdToUse(CryAudio::InvalidAuxObjectId)
	, m_nameHashed(name)
	, m_variableCollectionName(CHashedString(szGlobalVariableCollectionToUse))
{
	if (szGlobalVariableCollectionToUse)
	{
		m_pNonGlobalVariableCollection = nullptr; // we don't store a pointer to the global variable collection (for now), because we would not be informed if it`s removed from the outside.
	}
	else
	{
		m_pNonGlobalVariableCollection = CResponseSystem::GetInstance()->CreateLocalCollection(name);
	}
}

//--------------------------------------------------------------------------------------------------
CResponseActor::~CResponseActor()
{
	CResponseSystem::GetInstance()->GetSpeakerManager()->OnActorRemoved(this);
	CResponseSystem::GetInstance()->GetResponseManager()->OnActorRemoved(this);
	m_pNonGlobalVariableCollection = nullptr;  //Remark: if we are using a global variable collection we are not releasing it. We assume it`s handled outside	
}

//--------------------------------------------------------------------------------------------------
IEntity* CResponseActor::GetLinkedEntity() const
{
	if (m_linkedEntityID == INVALID_ENTITYID)  //if no actor exists, we assume it's a virtual speaker (radio, walkie talkie...) and therefore use the client actor
	{
		if (gEnv->pGameFramework)
		{
			return gEnv->pGameFramework->GetClientEntity();
		}
		return nullptr;
	}
	// We are using the User Data of the IResponseActor to store the ID of the Entity associated with that Actor.
	return gEnv->pEntitySystem->GetEntity(m_linkedEntityID);
}

//--------------------------------------------------------------------------------------------------
DRS::SignalInstanceId CResponseActor::QueueSignal(const CHashedString& signalName, DRS::IVariableCollectionSharedPtr pSignalContext /*= nullptr*/, DRS::IResponseManager::IListener* pSignalListener /*= nullptr*/)
{
	CResponseManager* pResponseMgr = CResponseSystem::GetInstance()->GetResponseManager();
	SSignal signal(signalName, this, std::static_pointer_cast<CVariableCollection>(pSignalContext));
	if (pSignalListener)
	{
		pResponseMgr->AddListener(pSignalListener, signal.m_id);
	}
	pResponseMgr->QueueSignal(signal);
	return signal.m_id;
}

//--------------------------------------------------------------------------------------------------
CVariableCollection* CResponseActor::GetLocalVariables()
{
	if (m_pNonGlobalVariableCollection)
	{
		return m_pNonGlobalVariableCollection.get();
	}
	else
	{
		CVariableCollection* pLocalVariables = CResponseSystem::GetInstance()->GetCollection(m_variableCollectionName);
		if (!pLocalVariables)
		{
			pLocalVariables = CResponseSystem::GetInstance()->CreateVariableCollection(m_variableCollectionName);
		}
		return pLocalVariables;
	}
}

//--------------------------------------------------------------------------------------------------
const CVariableCollection* CResponseActor::GetLocalVariables() const
{
	if (m_pNonGlobalVariableCollection)
	{
		return m_pNonGlobalVariableCollection.get();
	}
	else
	{
		const CVariableCollection* pLocalVariables = CResponseSystem::GetInstance()->GetCollection(m_variableCollectionName);
		if (!pLocalVariables)
		{
			pLocalVariables = CResponseSystem::GetInstance()->CreateVariableCollection(m_variableCollectionName);
		}
		return pLocalVariables;
	}
}

namespace DRS
{
SERIALIZATION_CLASS_NULL(IResponseAction, "");
SERIALIZATION_CLASS_NULL(IResponseCondition, "");
}

REGISTER_DRS_ACTION(CActionCancelSignal, "CancelSignal", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionCancelSpeaking, "CancelSpeaking", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionSetVariable, "ChangeVariable", "11DD11");
REGISTER_DRS_ACTION(CActionCopyVariable, "CopyVariable", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionExecuteResponse, "ExecuteResponse", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionResetTimerVariable, "ResetTimerVariable", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionSendSignal, "SendSignal", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionSetActor, "SetActor", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionSetActorByVariable, "SetActorFromVariable", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionSetGameToken, "SetGameToken", DEFAULT_DRS_ACTION_COLOR);
REGISTER_DRS_ACTION(CActionSpeakLine, "SpeakLine", "00FF00");
REGISTER_DRS_ACTION(CActionWait, "Wait", DEFAULT_DRS_ACTION_COLOR);

REGISTER_DRS_CONDITION(CExecutionLimitCondition, "Execution Limit", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CGameTokenCondition, "GameToken", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CInheritConditionsCondition, "Inherit Conditions", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CPlaceholderCondition, "Placeholder", "FF66CC");
REGISTER_DRS_CONDITION(CRandomCondition, "Random", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CTimeSinceCondition, "TimeSince", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CTimeSinceResponseCondition, "TimeSinceResponse", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CVariableEqualCondition, "Variable equal to ", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CVariableLargerCondition, "Variable greater than ", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CVariableRangeCondition, "Variable in range ", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CVariableSmallerCondition, "Variable less than ", DEFAULT_DRS_CONDITION_COLOR);
REGISTER_DRS_CONDITION(CVariableAgainstVariablesCondition, "Variable to Variable", DEFAULT_DRS_CONDITION_COLOR);
