// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIManager.h"

#include "GameExporter.h"
#include "AiBehaviorLibrary.h"

#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIAction.h>

#include <CryScriptSystem/IScriptSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryAISystem/INavigationSystem.h>
#include <CryMath/Random.h>

#include "CoverSurfaceManager.h"

#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/Controls/HyperGraphEditorWnd.h"

#include "SplashScreen.h"
#include "Objects/EntityObject.h"
#include "Controls/QuestionDialog.h"

#include "Notifications/NotificationCenter.h"

#include <Preferences/ViewportPreferences.h>

#define GRAPH_FILE_FILTER         "Graph XML Files (*.xml)|*.xml"

#define SOED_SOTEMPLATES_FILENAME "Libs/SmartObjects/Templates/SOTemplates.xml"

const int kMinQueueSizeToShowProgressNotification = 100;

SERIALIZATION_ENUM_BEGIN(ENavigationUpdateType, "NavigationUpdateType")
SERIALIZATION_ENUM(ENavigationUpdateType::Continuous, "continuous", "Continuous");
SERIALIZATION_ENUM(ENavigationUpdateType::AfterStabilization, "after_stabilization", "After Stabilization");
SERIALIZATION_ENUM(ENavigationUpdateType::Disabled, "disabled", "Disabled");
SERIALIZATION_ENUM_END()

//////////////////////////////////////////////////////////////////////////
// Function created on designers request to automatically randomize
// model index variations per instance of the AI character.
//////////////////////////////////////////////////////////////////////////
void ed_randomize_variations(IConsoleCmdArgs* pArgs)
{
	CBaseObjectsArray objects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objects);

	for (int i = 0; i < (int)objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntityObject = (CEntityObject*)pObject;
			if (pEntityObject->GetProperties2())
			{
				CVarBlock* pEntityProperties = pEntityObject->GetProperties();
				if (pEntityObject->GetPrototype())
					pEntityProperties = pEntityObject->GetPrototype()->GetProperties();
				if (!pEntityProperties)
					continue;
				// This is some kind of AI entity.
				// Check if it have AI variations
				IVariable* p_nModelVariations = pEntityProperties->FindVariable("nModelVariations", true);
				IVariable* p_nVariation = pEntityObject->GetProperties2()->FindVariable("nVariation", true);
				if (p_nModelVariations && p_nVariation)
				{
					int nModelVariations = 0;
					p_nModelVariations->Get(nModelVariations);
					if (nModelVariations > 0)
					{
						int nVariation = 1 + (int)(cry_random(0.0f, 1.0f) * nModelVariations);
						IVariable* p_nVariation = pEntityObject->GetProperties2()->FindVariable("nVariation", true);
						if (p_nVariation)
						{
							p_nVariation->Set(nVariation);
							pEntityObject->Reload();
							continue;
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CAI Manager.
//////////////////////////////////////////////////////////////////////////

// Register AI Settings
SAINavigationPreferences gAINavigationPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SAINavigationPreferences, &gAINavigationPreferences)

CAIManager::CAIManager()
	: m_coverSurfaceManager(new CCoverSurfaceManager)
	, m_navigationWorldMonitorState(ENavigationWorldMonitorState::Stopped)
	, m_currentUpdateType(ENavigationUpdateType::Continuous)
	, m_pendingNavigationCalculateAccessibility(true)
	, m_refreshMnmOnGameExit(false)
	, m_MNMRegenerationPausedCount(0)
	, m_navigationUpdatePaused(false)
	, m_resumeMNMRegenWhenPumpedPhysicsEventsAreFinished(false)
{
	m_pAISystem = nullptr;
	m_pBehaviorLibrary = new CAIBehaviorLibrary;

	gViewportDebugPreferences.objectHideMaskChanged.Connect(this, &CAIManager::OnHideMaskChanged);
}

CAIManager::~CAIManager()
{
	gViewportDebugPreferences.objectHideMaskChanged.DisconnectObject(this);

	FreeActionGraphs();

	SAFE_DELETE(m_pBehaviorLibrary);
}

void CAIManager::Init(ISystem* system)
{
	m_pAISystem = system->GetAISystem();
	if (!m_pAISystem)
		return;

	// (MATT) Reset the AI to allow it to update its configuration, with respect to ai_CompatibilityMode cvar {2009/06/25}
	m_pAISystem->Reset(IAISystem::RESET_INTERNAL);

	SplashScreen::SetText("Loading AI Behaviors...");
	m_pBehaviorLibrary->LoadBehaviors("Scripts\\AI\\Behaviors\\");

	SplashScreen::SetText("Loading Smart Objects...");
	//enumerate Anchor actions.
	EnumAnchorActions();

	//load smart object templates
	ReloadTemplates();

	SplashScreen::SetText("Loading Action Flowgraphs...");
	LoadActionGraphs();

	REGISTER_COMMAND("ed_randomize_variations", ed_randomize_variations, VF_NULL, "");

	LoadNavigationEditorSettings();
}

void CAIManager::Update(uint32 updateFlags)
{
	if (!m_pAISystem)
		return;

	if (updateFlags & ESYSUPDATE_EDITOR)
	{
		if ((updateFlags & ESYSUPDATE_EDITOR_AI_PHYSICS) == 0)
		{
			NavigationContinuousUpdate();
		}
	}

	m_navigationUpdateProgress.Update(m_pAISystem->GetNavigationSystem(), m_navigationUpdatePaused);
}

void CAIManager::LoadNavigationEditorSettings()
{
	gAINavigationPreferences.visualizeNavigationAccessibilityChanged.Connect(this, &CAIManager::OnVisualizeNavigationAccessibilityChanged);
	gAINavigationPreferences.navigationUpdateTypeChanged.Connect(this, &CAIManager::OnNavigationUpdateChanged);
	gAINavigationPreferences.navigationDebugAgentTypeChanged.Connect(this, &CAIManager::OnNavigationDebugDisplayAgentTypeChanged);
	gAINavigationPreferences.signalSettingsChanged.Connect(this, &CAIManager::OnAINavigationPreferencesChanged);

	if (gAINavigationPreferences.visualizeNavigationAccessibility())
		if (ICVar* pVar = gEnv->pConsole->GetCVar("ai_MNMDebugAccessibility"))
			pVar->Set(1);

	OnAINavigationPreferencesChanged();
}

void CAIManager::OnAINavigationPreferencesChanged()
{
	OnVisualizeNavigationAccessibilityChanged();
	OnNavigationUpdateChanged();
	OnNavigationDebugDisplayAgentTypeChanged();
}

void CAIManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginGameMode:
		{
			bool allowDynamicRegen = false;
			ICVar* pVar = gEnv->pConsole->GetCVar("ai_MNMAllowDynamicRegenInEditor");
			CRY_ASSERT_MESSAGE(pVar, "The cvar ai_MNMAllowDynamicRegenInEditor is not defined.");
			if (pVar)
			{
				allowDynamicRegen = pVar->GetIVal() != 0;
			}

			if (!allowDynamicRegen)
			{
				PauseNavigationUpdating();
				PauseMNMRegeneration();
			}
		}
		break;
	case eNotify_OnEndGameMode:
		RestartNavigationUpdating();
		m_resumeMNMRegenWhenPumpedPhysicsEventsAreFinished = true;
		break;

	case eNotify_OnClearLevelContents:
		// - get rid of some leftovers in the free-ID-list of the ICoverSystem
		// - this is important for when a new level is about to get created since those leftover IDs might otherwise cause an underflow when counting the number of
		//   cover surfaces by CCoverSurfaceManager::WriteToFile()
		// - the result would be that the new level thinks he has like 2^32 surfaces and when loading it, memory allocation for this amount of non-existent surfaces would fail, of course
		m_coverSurfaceManager->ClearGameSurfaces();
		break;

	case eNotify_OnBeginLoad:
		PauseNavigationWorldMonitor();
		if (gAINavigationPreferences.navigationRegenDisabledOnLevelLoad())
		{
			PauseMNMRegeneration();
		}
		break;

	case eNotify_OnEndLoad:
		RequestResumeNavigationWorldMonitorOnNextUpdate();
		ResumeMNMRegenerationWithoutUpdatingPengingNavMeshChanges();
		break;
	case eNotify_OnBeginExportToGame:
		PauseNavigationUpdating();
		break;
	case eNotify_OnExportToGame:
	case eNotify_OnExportToGameFailed:
		RestartNavigationUpdating();
		break;
	}
}

void CAIManager::OnHideMaskChanged()
{
	IAISystem* pAISystem = gEnv->pAISystem;
	if (pAISystem)
	{
		if (gViewportDebugPreferences.GetObjectHideMask() == 0)
		{
			RestartNavigationUpdating();
		}
		else
		{
			PauseNavigationUpdating();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::ReloadActionGraphs()
{
	if (!m_pAISystem)
		return;
	//	FreeActionGraphs();
	GetAISystem()->GetAIActionManager()->ReloadActions();
	LoadActionGraphs();
}

void CAIManager::LoadActionGraphs()
{
	if (!m_pAISystem)
		return;

	int i = 0;
	IAIAction* pAction;
	while (pAction = m_pAISystem->GetAIActionManager()->GetAIAction(i++))
	{
		CHyperFlowGraph* m_pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraphForAction(pAction);
		if (m_pFlowGraph)
		{
			// [3/24/2011 evgeny] Reconnect Sandbox CHyperFlowGraph to CryAction CFlowGraph
			m_pFlowGraph->SetIFlowGraph(pAction->GetFlowGraph());
		}
		else
		{
			m_pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->CreateGraphForAction(pAction);
			m_pFlowGraph->AddRef();
			string filename(AI_ACTIONS_PATH);
			filename += '/';
			filename += pAction->GetName();
			filename += ".xml";
			m_pFlowGraph->SetName("");
			m_pFlowGraph->Load(filename);
		}
	}
}

void CAIManager::SaveAndReloadActionGraphs()
{
	if (!m_pAISystem)
		return;

	string actionName;
	CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		CHyperGraphDialog* pHGDlg = (CHyperGraphDialog*) pWnd;
		CHyperGraph* pGraph = pHGDlg->GetGraphView()->GetGraph();
		if (pGraph)
		{
			IAIAction* pAction = pGraph->GetAIAction();
			if (pAction)
			{
				actionName = pAction->GetName();
				pHGDlg->GetGraphView()->SetGraph(nullptr);
			}
		}
	}

	SaveActionGraphs();
	ReloadActionGraphs();

	if (!actionName.IsEmpty())
	{
		IAIAction* pAction = GetAISystem()->GetAIActionManager()->GetAIAction(actionName);
		if (pAction)
		{
			CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
			CHyperFlowGraph* pFlowGraph = pManager->FindGraphForAction(pAction);
			assert(pFlowGraph);
			if (pFlowGraph)
				pManager->OpenView(pFlowGraph);
		}
	}
}

void CAIManager::SaveActionGraphs()
{
	if (!m_pAISystem)
		return;

	CWaitCursor waitCursor;

	int i = 0;
	IAIAction* pAction;
	while (pAction = m_pAISystem->GetAIActionManager()->GetAIAction(i++))
	{
		CHyperFlowGraph* m_pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraphForAction(pAction);
		if (m_pFlowGraph->IsModified())
		{
			m_pFlowGraph->Save(m_pFlowGraph->GetName() + string(".xml"));
			pAction->Invalidate();
		}
	}
}

void CAIManager::FreeActionGraphs()
{
	CFlowGraphManager* pFGMgr = GetIEditorImpl()->GetFlowGraphManager();
	if (pFGMgr)
	{
		pFGMgr->FreeGraphsForActions();
	}
}

IAISystem* CAIManager::GetAISystem()
{
	return GetIEditorImpl()->GetSystem()->GetAISystem();
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::ReloadScripts()
{
	GetBehaviorLibrary()->ReloadScripts();
	EnumAnchorActions();
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::GetAnchorActions(std::vector<string>& actions) const
{
	actions.clear();
	for (AnchorActions::const_iterator it = m_anchorActions.begin(); it != m_anchorActions.end(); it++)
	{
		actions.push_back(it->first);
	}
}

//////////////////////////////////////////////////////////////////////////
int CAIManager::AnchorActionToId(const char* sAction) const
{
	AnchorActions::const_iterator it = m_anchorActions.find(sAction);
	if (it != m_anchorActions.end())
		return it->second;
	return -1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct CAIAnchorDump : public IScriptTableDumpSink
{
	CAIManager::AnchorActions actions;

	CAIAnchorDump(IScriptTable* obj)
	{
		m_pScriptObject = obj;
	}

	virtual void OnElementFound(int nIdx, ScriptVarType type) {}
	virtual void OnElementFound(const char* sName, ScriptVarType type)
	{
		if (type == svtNumber)
		{
			// New behavior.
			int val;
			if (m_pScriptObject->GetValue(sName, val))
			{
				actions[sName] = val;
			}
		}
	}
private:
	IScriptTable* m_pScriptObject;
};

//////////////////////////////////////////////////////////////////////////
void CAIManager::EnumAnchorActions()
{
	IScriptSystem* pScriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	SmartScriptTable pAIAnchorTable(pScriptSystem, true);
	if (pScriptSystem->GetGlobalValue("AIAnchorTable", pAIAnchorTable))
	{
		CAIAnchorDump anchorDump(pAIAnchorTable);
		pAIAnchorTable->Dump(&anchorDump);
		m_anchorActions = anchorDump.actions;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::GetSmartObjectStates(std::vector<string>& values) const
{
	if (!m_pAISystem)
		return;

	const char* sStateName;
	for (int i = 0; sStateName = m_pAISystem->GetSmartObjectManager()->GetSmartObjectStateName(i); ++i)
		values.push_back(sStateName);
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::GetSmartObjectActions(std::vector<string>& values) const
{
	if (!m_pAISystem)
		return;

	IAIActionManager* pAIActionManager = m_pAISystem->GetAIActionManager();
	assert(pAIActionManager);
	if (!pAIActionManager)
		return;

	values.clear();

	for (int i = 0; IAIAction* pAIAction = pAIActionManager->GetAIAction(i); ++i)
	{
		const char* szActionName = pAIAction->GetName();
		if (szActionName)
		{
			values.push_back(szActionName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::AddSmartObjectState(const char* sState)
{
	if (!m_pAISystem)
		return;
	m_pAISystem->GetSmartObjectManager()->RegisterSmartObjectState(sState);
}

//////////////////////////////////////////////////////////////////////////
bool CAIManager::NewAction(string& filename)
{
	CFileUtil::CreateDirectory(PathUtil::Make(PathUtil::GetGameFolder(), AI_ACTIONS_PATH));

	if (!CFileUtil::SelectSaveFile(GRAPH_FILE_FILTER, "xml", PathUtil::Make(PathUtil::GetGameFolder(), AI_ACTIONS_PATH).c_str(), filename))
		return false;

	filename.MakeLower();

	// check if file exists.
	FILE* file = fopen(filename, "rb");
	if (file)
	{
		fclose(file);
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Can't create AI Action because another AI Action with this name already exists!\n\nCreation canceled..."));
		return false;
	}

	// Make a new graph.
	CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
	CHyperGraph* pGraph = pManager->CreateGraph();

	CHyperNode* pStartNode = (CHyperNode*) pGraph->CreateNode("AI:ActionStart");
	pStartNode->SetPos(Gdiplus::PointF(80, 10));
	CHyperNode* pEndNode = (CHyperNode*) pGraph->CreateNode("AI:ActionEnd");
	pEndNode->SetPos(Gdiplus::PointF(400, 10));
	CHyperNode* pPosNode = (CHyperNode*) pGraph->CreateNode("Entity:EntityPos");
	pPosNode->SetPos(Gdiplus::PointF(20, 70));

	pGraph->UnselectAll();
	pGraph->ConnectPorts(pStartNode, &pStartNode->GetOutputs()->at(1), pPosNode, &pPosNode->GetInputs()->at(0));

	bool r = pGraph->Save(filename);

	ReloadActionGraphs();

	return r;
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::AssignPFPropertiesToPathType(const string& sPathType, const AgentPathfindingProperties& properties)
{
	m_mapPFProperties.insert(PFPropertiesMap::value_type(sPathType, properties));
}

//////////////////////////////////////////////////////////////////////////
const AgentPathfindingProperties* CAIManager::GetPFPropertiesOfPathType(const string& sPathType)
{
	PFPropertiesMap::iterator iterPFProperties = m_mapPFProperties.find(sPathType);
	return (iterPFProperties != m_mapPFProperties.end()) ? &iterPFProperties->second : 0;
}

//////////////////////////////////////////////////////////////////////////
string CAIManager::GetPathTypeNames()
{
	string pathNames;
	for (PFPropertiesMap::iterator iter = m_mapPFProperties.begin(); iter != m_mapPFProperties.end(); ++iter)
		pathNames += iter->first + ' ';
	return pathNames;
}

//////////////////////////////////////////////////////////////////////////
string CAIManager::GetPathTypeName(int index)
{
	CRY_ASSERT(index >= 0 && index < m_mapPFProperties.size());
	PFPropertiesMap::iterator iter = m_mapPFProperties.begin();
	if (index >= 0 && index < m_mapPFProperties.size())
	{
		std::advance(iter, index);
		return iter->first;
	}

	//  int i=0;
	//  for (PFPropertiesMap::iterator iter = mapPFProperties.begin(); iter != mapPFProperties.end(); ++iter, ++i)
	//  {
	//      if(i == index)
	//      return iter->first;
	//  }
	return "";
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::FreeTemplates()
{
	MapTemplates::iterator it, itEnd = m_mapTemplates.end();
	for (it = m_mapTemplates.begin(); it != itEnd; ++it)
		delete it->second;
	m_mapTemplates.clear();
}

bool CAIManager::ReloadTemplates()
{
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(SOED_SOTEMPLATES_FILENAME);
	if (!root || !root->isTag("SOTemplates"))
		return false;

	FreeTemplates();

	int count = root->getChildCount();
	for (int i = 0; i < count; ++i)
	{
		XmlNodeRef node = root->getChild(i);
		if (node->isTag("Template"))
		{
			CSOTemplate* pTemplate = new CSOTemplate;

			node->getAttr("id", pTemplate->id);
			pTemplate->name = node->getAttr("name");
			pTemplate->description = node->getAttr("description");

			// check is the id unique
			MapTemplates::iterator find = m_mapTemplates.find(pTemplate->id);
			if (find != m_mapTemplates.end())
			{
				CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("WARNING:\nSmart object template named " + pTemplate->name + " will be ignored! The id is not unique."));
				delete pTemplate;
				continue;
			}

			// load params
			pTemplate->params = LoadTemplateParams(node);
			if (!pTemplate->params)
			{
				CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("WARNING:\nSmart object template named " + pTemplate->name + " will be ignored! Can't load params."));
				delete pTemplate;
				continue;
			}

			// insert in map
			m_mapTemplates[pTemplate->id] = pTemplate;
		}
	}
	return true;
}

CSOParamBase* CAIManager::LoadTemplateParams(XmlNodeRef root) const
{
	CSOParamBase* pFirst = nullptr;

	int count = root->getChildCount();
	for (int i = 0; i < count; ++i)
	{
		XmlNodeRef node = root->getChild(i);
		if (node->isTag("Param"))
		{
			CSOParam* pParam = new CSOParam;

			pParam->sName = node->getAttr("name");
			pParam->sCaption = node->getAttr("caption");
			node->getAttr("visible", pParam->bVisible);
			node->getAttr("editable", pParam->bEditable);
			pParam->sValue = node->getAttr("value");
			pParam->sHelp = node->getAttr("help");

			IVariable* pVar = nullptr;
			/*
			      if ( pParam->sName == "bNavigationRule" )
			        pVar = &gSmartObjectsUI.bNavigationRule;
			      else if ( pParam->sName == "sEvent" )
			        pVar = &gSmartObjectsUI.sEvent;
			      else if ( pParam->sName == "userClass" )
			        pVar = &gSmartObjectsUI.userClass;
			      else if ( pParam->sName == "userState" )
			        pVar = &gSmartObjectsUI.userState;
			      else if ( pParam->sName == "userHelper" )
			        pVar = &gSmartObjectsUI.userHelper;
			      else if ( pParam->sName == "iMaxAlertness" )
			        pVar = &gSmartObjectsUI.iMaxAlertness;
			      else if ( pParam->sName == "objectClass" )
			        pVar = &gSmartObjectsUI.objectClass;
			      else if ( pParam->sName == "objectState" )
			        pVar = &gSmartObjectsUI.objectState;
			      else if ( pParam->sName == "objectHelper" )
			        pVar = &gSmartObjectsUI.objectHelper;
			      else if ( pParam->sName == "entranceHelper" )
			        pVar = &gSmartObjectsUI.entranceHelper;
			      else if ( pParam->sName == "exitHelper" )
			        pVar = &gSmartObjectsUI.exitHelper;
			      else if ( pParam->sName == "limitsDistanceFrom" )
			        pVar = &gSmartObjectsUI.limitsDistanceFrom;
			      else if ( pParam->sName == "limitsDistanceTo" )
			        pVar = &gSmartObjectsUI.limitsDistanceTo;
			      else if ( pParam->sName == "limitsOrientation" )
			        pVar = &gSmartObjectsUI.limitsOrientation;
			      else if ( pParam->sName == "delayMinimum" )
			        pVar = &gSmartObjectsUI.delayMinimum;
			      else if ( pParam->sName == "delayMaximum" )
			        pVar = &gSmartObjectsUI.delayMaximum;
			      else if ( pParam->sName == "delayMemory" )
			        pVar = &gSmartObjectsUI.delayMemory;
			      else if ( pParam->sName == "multipliersProximity" )
			        pVar = &gSmartObjectsUI.multipliersProximity;
			      else if ( pParam->sName == "multipliersOrientation" )
			        pVar = &gSmartObjectsUI.multipliersOrientation;
			      else if ( pParam->sName == "multipliersVisibility" )
			        pVar = &gSmartObjectsUI.multipliersVisibility;
			      else if ( pParam->sName == "multipliersRandomness" )
			        pVar = &gSmartObjectsUI.multipliersRandomness;
			      else if ( pParam->sName == "fLookAtOnPerc" )
			        pVar = &gSmartObjectsUI.fLookAtOnPerc;
			      else if ( pParam->sName == "userPreActionState" )
			        pVar = &gSmartObjectsUI.userPreActionState;
			      else if ( pParam->sName == "objectPreActionState" )
			        pVar = &gSmartObjectsUI.objectPreActionState;
			      else if ( pParam->sName == "highPriority" )
			        pVar = &gSmartObjectsUI.highPriority;
			      else if ( pParam->sName == "actionName" )
			        pVar = &gSmartObjectsUI.actionName;
			      else if ( pParam->sName == "userPostActionState" )
			        pVar = &gSmartObjectsUI.userPostActionState;
			      else if ( pParam->sName == "objectPostActionState" )
			        pVar = &gSmartObjectsUI.objectPostActionState;

			      if ( !pVar )
			      {
			          CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("WARNING:\nSmart object template has a Param tag named ") + pParam->sName + " which is not recognized as valid name!\nThe Param will be ignored..."));
			        delete pParam;
			        continue;
			      }
			 */
			pParam->pVariable = pVar;

			if (pFirst)
				pFirst->PushBack(pParam);
			else
				pFirst = pParam;
		}
		else if (node->isTag("ParamGroup"))
		{
			CSOParamGroup* pGroup = new CSOParamGroup;

			pGroup->sName = node->getAttr("name");
			node->getAttr("expand", pGroup->bExpand);
			pGroup->sHelp = node->getAttr("help");

			// load children
			pGroup->pChildren = LoadTemplateParams(node);
			if (!pGroup->pChildren)
			{
				CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("WARNING:\nSmart object template has a ParamGroup tag named " + pGroup->sName + " without any children nodes!\nThe ParamGroup will be ignored..."));
				delete pGroup;
				continue;
			}

			CVariableArray* pVar = new CVariableArray();
			pVar->AddRef();
			pGroup->pVariable = pVar;

			if (pFirst)
				pFirst->PushBack(pGroup);
			else
				pFirst = pGroup;
		}
	}

	return pFirst;
}

void CAIManager::OnEnterGameMode(bool inGame)
{
	bool allowDynamicRegen = false;
	ICVar* pVar = gEnv->pConsole->GetCVar("ai_MNMAllowDynamicRegenInEditor");
	CRY_ASSERT_MESSAGE(pVar, "The cvar ai_MNMAllowDynamicRegenInEditor is not defined.");
	if (pVar)
	{
		allowDynamicRegen = pVar->GetIVal() != 0;
	}

	INavigationSystem* pNavigationSystem = m_pAISystem->GetNavigationSystem();

	if (inGame)
	{
		m_refreshMnmOnGameExit = false;

		if (allowDynamicRegen)
		{
			for (size_t i = 0, n = pNavigationSystem->GetAgentTypeCount(); i < n; ++i)
			{
				pNavigationSystem->AddMeshChangeCallback(pNavigationSystem->GetAgentTypeID(i), functor(*this, &CAIManager::OnNavigationMeshChanged));
			}
		}
		else
		{
			StopNavigationWorldMonitor();
		}
	}
	else
	{
		if (allowDynamicRegen)
		{
			for (size_t i = 0, n = pNavigationSystem->GetAgentTypeCount(); i < n; ++i)
			{
				pNavigationSystem->RemoveMeshChangeCallback(pNavigationSystem->GetAgentTypeID(i), functor(*this, &CAIManager::OnNavigationMeshChanged));
			}
		}
		else
		{
			StartNavigationWorldMonitor();
		}

		if (m_refreshMnmOnGameExit)
		{
			RebuildAINavigation();
			m_refreshMnmOnGameExit = false;
		}
	}
}

void CAIManager::RegenerateNavigationByTypeName(const char* szType)
{
	if (!m_pAISystem)
		return;

	INavigationSystem* pNavigationSystem = m_pAISystem->GetNavigationSystem();
	if (!pNavigationSystem)
		return;
	
	const size_t agentTypeCount = m_pAISystem->GetNavigationSystem()->GetAgentTypeCount();

	string actionName(szType);

	if (0 == actionName.compareNoCase("all"))
	{
		const auto msgBoxResult = CQuestionDialog::SQuestion("MNM Navigation Rebuilding request",
			"Are you sure you want to fully rebuild the Multilayer Navigation Mesh data?\nDepending on the size of your level, this can take several minutes.");
		if (msgBoxResult == QDialogButtonBox::Yes)
		{
			RebuildAINavigation();
		}
	}
	else
	{
		for (size_t i = 0; i < agentTypeCount; ++i)
		{
			const NavigationAgentTypeID navigationAgentTypeID = pNavigationSystem->GetAgentTypeID(i);
			if (navigationAgentTypeID != 0 && actionName.compareNoCase(pNavigationSystem->GetAgentTypeName(navigationAgentTypeID)) == 0)
			{
				pNavigationSystem->GetUpdateManager()->RequestGlobalUpdateForAgentType(navigationAgentTypeID);
				break;
			}
		}
	}
}

void CAIManager::OnNavigationUpdateChanged()
{
	SetNavigationUpdateType((ENavigationUpdateType)gAINavigationPreferences.navigationUpdateType());
}

void CAIManager::SetNavigationUpdateType(ENavigationUpdateType updateType)
{
	if (m_currentUpdateType == updateType)
		return;

	INavigationSystem* pNavigationSystem = m_pAISystem ? m_pAISystem->GetNavigationSystem() : nullptr;
	if (!pNavigationSystem)
		return;

	switch (m_currentUpdateType)
	{
	case ENavigationUpdateType::AfterStabilization:
		pNavigationSystem->GetUpdateManager()->DisableUpdatesAfterStabilization();
		break;
	case ENavigationUpdateType::Disabled:
		ResumeMNMRegeneration();
		break;
	}

	m_currentUpdateType = updateType;
	gAINavigationPreferences.setNavigationUpdateType(updateType);

	switch (updateType)
	{
	case ENavigationUpdateType::AfterStabilization:
		pNavigationSystem->GetUpdateManager()->EnableUpdatesAfterStabilization();
		break;
	case ENavigationUpdateType::Disabled:
		PauseMNMRegeneration();
		break;
	}
}

void CAIManager::NavigationContinuousUpdate()
{
	if (m_pAISystem) //0-pointer crash when starting without Game-Dll
	{
		INavigationSystem* pNavigationSystem = m_pAISystem->GetNavigationSystem();

		//if (gAINavigationPreferences.navigationUpdateType() != ENavigationUpdateType::Disabled)
		{
			// NavigationContinuousUpdate() is called every frame after physics pumped its events.
			// Here is a good place to start WorldMonitor if it was stopped, or to resume it if it was paused.
			// By resuming paused WorldMonitor here after physics event we skip overflowing the WorldMonitor right after level load.
			StartNavigationWorldMonitorIfNeeded();
		}

		const INavigationSystem::WorkingState state = pNavigationSystem->Update();
		switch (state)
		{
		case INavigationSystem::Idle:
			if (m_pendingNavigationCalculateAccessibility)
			{
				CalculateNavigationAccessibility();
				m_pendingNavigationCalculateAccessibility = false;
			}
			break;

		case INavigationSystem::Working:
			m_pendingNavigationCalculateAccessibility = true;
			break;
		}
	}
}

void CAIManager::OnAreaModified(const AABB& aabb, const CBaseObject* object /*= nullptr*/)
{
	if (!m_pAISystem)
		return;

	if (INavigationSystem* pNavigationSystem = m_pAISystem->GetNavigationSystem())
	{
		if (object)
		{
			int id = -1;
			if (IPhysicalEntity* pent = object->GetCollisionEntity())
			{
				id = gEnv->pPhysicalWorld->GetPhysicalEntityId(pent);
			}
			pNavigationSystem->GetUpdateManager()->EntityChanged(id, aabb);
		}
		else
		{
			pNavigationSystem->GetUpdateManager()->WorldChanged(aabb);
		}
	}
}

void CAIManager::RebuildAINavigation()
{
	ResumeMNMRegeneration();

	if (m_pAISystem) //0-pointer crash when starting without Game-Dll
	{
		if (INavigationSystem* pNavigationSystem = m_pAISystem->GetNavigationSystem())
		{
			pNavigationSystem->GetUpdateManager()->RequestGlobalUpdate();
		}
	}
	
	// bring it back to the original state
	PauseMNMRegeneration();
}

void CAIManager::PauseNavigationUpdating()
{
	m_navigationUpdatePaused = true;
	
	if (m_pAISystem) //0-pointer crash when starting without Game-Dll
		m_pAISystem->GetNavigationSystem()->PauseNavigationUpdate();
}

void CAIManager::RestartNavigationUpdating()
{
	m_navigationUpdatePaused = false;
	
	if (m_pAISystem) //0-pointer crash when starting without Game-Dll
		m_pAISystem->GetNavigationSystem()->RestartNavigationUpdate();
}

bool CAIManager::IsReadyToGameExport(unsigned int& adjustedExportFlags) const
{
	if (adjustedExportFlags & eExp_AI_MNM)
	{
		if (m_pAISystem)
		{
			if (m_pAISystem->GetNavigationSystem()->GetUpdateManager()->HasBufferedRegenerationRequests())
			{
				QDialogButtonBox::StandardButtons answer = CQuestionDialog::SQuestion(
					"Navigation System Warning",
					"There are pending unapplied changes to navigation data.\n"
					"Would you like to cancel the export and to update navigation mesh with this changes now?\n\n"
					"Warning: AI may not be able to navigate correctly through the world in pure game without updating.",
					QDialogButtonBox::Yes | QDialogButtonBox::No,
					QDialogButtonBox::Yes
				);
				if (answer == QDialogButtonBox::Yes)
				{
					m_pAISystem->GetNavigationSystem()->GetUpdateManager()->ApplyBufferedRegenerationRequests();
					return false;
				}
			}
		}
		
		if (!IsNavigationFullyGenerated())
		{
			QDialogButtonBox::StandardButtons answer = CQuestionDialog::SQuestion(
				"Navigation System Warning",
				"The processing of the navigation data is still in progress. New navigation data cannot be exported until fully processed.\n"
				"Would you like to export the level with the old navigation data?\n\n"
				"Warning: AI may not be able to navigate correctly through the world in pure game.",
				QDialogButtonBox::Yes | QDialogButtonBox::Cancel,
				QDialogButtonBox::Cancel
			);
			if (answer == QDialogButtonBox::Cancel)
				return false;

			//don't export navigation mesh when user clicks "Yes"
			adjustedExportFlags &= ~eExp_AI_MNM;
		}
	}
	return true;
}

bool CAIManager::IsNavigationFullyGenerated() const
{
	if (m_pAISystem)
		return (m_pAISystem->GetNavigationSystem()->GetState() == INavigationSystem::Idle);

	return true;
}

size_t CAIManager::GetNavigationAgentTypeCount() const
{
	size_t agentCount = 0;
	if (m_pAISystem) //0-pointer crash when starting without Game-Dll
		agentCount = m_pAISystem->GetNavigationSystem()->GetAgentTypeCount();

	return agentCount;
}

const char* CAIManager::GetNavigationAgentTypeName(size_t i) const
{
	const NavigationAgentTypeID id = m_pAISystem->GetNavigationSystem()->GetAgentTypeID(i);
	if (id)
		return m_pAISystem->GetNavigationSystem()->GetAgentTypeName(id);

	return 0;
}

void CAIManager::OnNavigationDebugDisplayAgentTypeChanged()
{
	if (NavigationAgentTypeID debugAgentTypeID = m_pAISystem->GetNavigationSystem()->GetAgentTypeID(gAINavigationPreferences.navigationDebugAgentType()))
		m_pAISystem->GetNavigationSystem()->SetDebugDisplayAgentType(debugAgentTypeID);
}

bool CAIManager::GetNavigationDebugDisplayAgent(size_t* index) const
{
	INavigationSystem* pNavigationSystem(nullptr);
	if (!m_pAISystem)
	{
		return false;
	}

	pNavigationSystem = m_pAISystem->GetNavigationSystem();
	if (!pNavigationSystem)
	{
		return false;
	}

	const NavigationAgentTypeID id = pNavigationSystem->GetDebugDisplayAgentType();
	if (id)
	{
		const size_t agentTypeCount = m_pAISystem->GetNavigationSystem()->GetAgentTypeCount();
		for (size_t i = 0; i < agentTypeCount; ++i)
		{
			if (id == m_pAISystem->GetNavigationSystem()->GetAgentTypeID(i))
			{
				if (index)
					*index = i;
				return true;
			}
		}
	}

	return false;
}

void CAIManager::CalculateNavigationAccessibility()
{
	LOADING_TIME_PROFILE_SECTION
	if (m_pAISystem)
	{
		INavigationSystem* pINavigationSystem = m_pAISystem->GetNavigationSystem();
		if (pINavigationSystem)
		{
			pINavigationSystem->CalculateAccessibility();
		}
	}
}

void CAIManager::OnVisualizeNavigationAccessibilityChanged()
{
	ICVar* pVar = gEnv->pConsole->GetCVar("ai_MNMDebugAccessibility");
	CRY_ASSERT_MESSAGE(pVar, "The cvar ai_MNMDebugAccessibility is not defined.");
	if (pVar)
	{
		pVar->Set(gAINavigationPreferences.visualizeNavigationAccessibility() == false ? 0 : 1);
	}
}

void CAIManager::NavigationDebugDisplay()
{
	if (gAINavigationPreferences.navigationDebugDisplay())
	{
		if (m_pAISystem)
		{
			INavigationSystem* pINavigationSystem = m_pAISystem->GetNavigationSystem();
			if (pINavigationSystem)
			{
				const NavigationAgentTypeID id = pINavigationSystem->GetDebugDisplayAgentType();
				if (id)
					pINavigationSystem->DebugDraw();
			}
		}
	}
}

void CAIManager::EarlyUpdate()
{
	if (m_pAISystem)
	{
		if (INavigationSystem* pINavigationSystem = m_pAISystem->GetNavigationSystem())
		{
			pINavigationSystem->GetUpdateManager()->ClearRegenerationRequestedThisCycleFlag();
		}
	}
}

void CAIManager::LateUpdate()
{
	if (m_pAISystem)
	{
		if (m_resumeMNMRegenWhenPumpedPhysicsEventsAreFinished)
		{
			if (INavigationSystem* pINavigationSystem = m_pAISystem->GetNavigationSystem())
			{
				if (!pINavigationSystem->GetUpdateManager()->WasRegenerationRequestedThisCycle())
				{
					m_resumeMNMRegenWhenPumpedPhysicsEventsAreFinished = false;
					ResumeMNMRegeneration();
				}
			}
		}
	}
}

void CAIManager::OnNavigationMeshChanged(NavigationAgentTypeID, NavigationMeshID, uint32)
{
	m_refreshMnmOnGameExit = true;
}

static bool StartNavigationWorldMonitorImpl(IAISystem* pAISystem)
{
	if (pAISystem)
	{
		if (INavigationSystem* pNavigationSystem = pAISystem->GetNavigationSystem())
		{
			pNavigationSystem->StartWorldMonitoring();
			return true;
		}
	}
	return false;
}

static bool StopNavigationWorldMonitorImpl(IAISystem* pAISystem)
{
	if (pAISystem)
	{
		if (INavigationSystem* pNavigationSystem = pAISystem->GetNavigationSystem())
		{
			pNavigationSystem->StopWorldMonitoring();
			return true;
		}
	}
	return false;
}

void CAIManager::StartNavigationWorldMonitor()
{
	switch (m_navigationWorldMonitorState)
	{
	case ENavigationWorldMonitorState::Stopped:
		{
			if (StartNavigationWorldMonitorImpl(m_pAISystem))
			{
				SetNavigationWorldMonitorState(ENavigationWorldMonitorState::Started);
			}
		}
		break;

	case ENavigationWorldMonitorState::StoppedAndPaused:
		{
			SetNavigationWorldMonitorState(ENavigationWorldMonitorState::StartedAndPaused);
		}
		break;

	case ENavigationWorldMonitorState::Started:
	case ENavigationWorldMonitorState::StartedAndPaused:
	case ENavigationWorldMonitorState::StartedPausedAndResumePending:
		break;

	default:
		CRY_ASSERT_MESSAGE(false, "Unknown state, should never happen");
	}
	static_assert(static_cast<int>(ENavigationWorldMonitorState::StatesCount) == 5, "Invalid state count!");
}

void CAIManager::StopNavigationWorldMonitor()
{
	switch (m_navigationWorldMonitorState)
	{
	case ENavigationWorldMonitorState::Started:
		{
			StopNavigationWorldMonitorImpl(m_pAISystem);
			SetNavigationWorldMonitorState(ENavigationWorldMonitorState::Stopped);
		}
		break;

	case ENavigationWorldMonitorState::StartedAndPaused:
	case ENavigationWorldMonitorState::StartedPausedAndResumePending:
		{
			SetNavigationWorldMonitorState(ENavigationWorldMonitorState::StoppedAndPaused);
		}
		break;

	case ENavigationWorldMonitorState::Stopped:
	case ENavigationWorldMonitorState::StoppedAndPaused:
		break;

	default:
		CRY_ASSERT_MESSAGE(false, "Unknown state, should never happen");
	}
	static_assert(static_cast<int>(ENavigationWorldMonitorState::StatesCount) == 5, "Invalid state count!");
}

void CAIManager::PauseNavigationWorldMonitor()
{
	switch (m_navigationWorldMonitorState)
	{
	case ENavigationWorldMonitorState::Stopped:
	case ENavigationWorldMonitorState::StartedPausedAndResumePending:
		SetNavigationWorldMonitorState(ENavigationWorldMonitorState::StoppedAndPaused);
		break;

	case ENavigationWorldMonitorState::Started:
		StopNavigationWorldMonitorImpl(m_pAISystem);
		SetNavigationWorldMonitorState(ENavigationWorldMonitorState::StartedAndPaused);
		break;

	case ENavigationWorldMonitorState::StartedAndPaused:
	case ENavigationWorldMonitorState::StoppedAndPaused:
		break;

	default:
		CRY_ASSERT_MESSAGE(false, "Unknown state, should never happen");
	}
	static_assert(static_cast<int>(ENavigationWorldMonitorState::StatesCount) == 5, "Invalid state count!");
}

void CAIManager::RequestResumeNavigationWorldMonitorOnNextUpdate()
{
	switch (m_navigationWorldMonitorState)
	{
	case ENavigationWorldMonitorState::StoppedAndPaused:
		{
			SetNavigationWorldMonitorState(ENavigationWorldMonitorState::Stopped);
		}
		break;

	case ENavigationWorldMonitorState::StartedAndPaused:
		{
			SetNavigationWorldMonitorState(ENavigationWorldMonitorState::StartedPausedAndResumePending);
		}
		break;

	case ENavigationWorldMonitorState::Started:
	case ENavigationWorldMonitorState::Stopped:
	case ENavigationWorldMonitorState::StartedPausedAndResumePending:
		break;

	default:
		CRY_ASSERT_MESSAGE(false, "Unknown state, should never happen");
	}
	static_assert(static_cast<int>(ENavigationWorldMonitorState::StatesCount) == 5, "Invalid state count!");
}

void CAIManager::StartNavigationWorldMonitorIfNeeded()
{
	switch (m_navigationWorldMonitorState)
	{
	case ENavigationWorldMonitorState::StartedPausedAndResumePending:
		{
			SetNavigationWorldMonitorState(ENavigationWorldMonitorState::Stopped);
			StartNavigationWorldMonitor();
		}
		break;

	case ENavigationWorldMonitorState::Stopped:
		{
			StartNavigationWorldMonitor();
		}
		break;

	case ENavigationWorldMonitorState::StartedAndPaused:
	case ENavigationWorldMonitorState::StoppedAndPaused:
	case ENavigationWorldMonitorState::Started:
		break;

	default:
		CRY_ASSERT_MESSAGE(false, "Unknown state, should never happen");
	}
	static_assert(static_cast<int>(ENavigationWorldMonitorState::StatesCount) == 5, "Invalid state count!");
}

/*static */ const char* CAIManager::GetNavigationWorldMonitorStateName(const ENavigationWorldMonitorState state)
{
	switch (state)
	{
	case ENavigationWorldMonitorState::Stopped:
		return "Stopped";
	case ENavigationWorldMonitorState::Started:
		return "Started";
	case ENavigationWorldMonitorState::StoppedAndPaused:
		return "StoppedAndPaused";
	case ENavigationWorldMonitorState::StartedAndPaused:
		return "StartedAndPaused";
	case ENavigationWorldMonitorState::StartedPausedAndResumePending:
		return "StartedPausedAndResumePending";
	default:
		CRY_ASSERT_MESSAGE(false, "Unknown state, should never happen");
		return "<Unknown state>";
	}
	static_assert(static_cast<int>(ENavigationWorldMonitorState::StatesCount) == 5, "Invalid state count!");
}

void CAIManager::SetNavigationWorldMonitorState(const ENavigationWorldMonitorState newState)
{
	CryLog("CAIManager: navigation world monitor state: %s -> %s",
	       GetNavigationWorldMonitorStateName(m_navigationWorldMonitorState),
	       GetNavigationWorldMonitorStateName(newState));

	m_navigationWorldMonitorState = newState;
}

void CAIManager::PauseMNMRegeneration()
{
	if (m_MNMRegenerationPausedCount++ == 0)
	{
		if (m_pAISystem)
		{
			if (INavigationSystem* pNavigationSystem = m_pAISystem->GetNavigationSystem())
			{
				pNavigationSystem->GetUpdateManager()->DisableRegenerationRequestsAndBuffer();
			}
		}
	}
}

void CAIManager::ResumeMNMRegeneration(bool updateChangedVolumes)
{
	if (--m_MNMRegenerationPausedCount == 0)
	{
		if (m_pAISystem)
		{
			if (INavigationSystem* pNavigationSystem = m_pAISystem->GetNavigationSystem())
			{
				pNavigationSystem->GetUpdateManager()->EnableRegenerationRequestsExecution(true);
			}
		}
	}
}

void CAIManager::ResumeMNMRegenerationWithoutUpdatingPengingNavMeshChanges()
{
	if (--m_MNMRegenerationPausedCount == 0)
	{
		if (m_pAISystem)
		{
			if (INavigationSystem* pNavigationSystem = m_pAISystem->GetNavigationSystem())
			{
				pNavigationSystem->GetUpdateManager()->EnableRegenerationRequestsExecution(false);
			}
		}
	}
}

CAIManager::SNavigationUpdateProgress::~SNavigationUpdateProgress()
{
	delete m_pUpdateProgressNotification;
}

void CAIManager::SNavigationUpdateProgress::Update(const INavigationSystem* pNavigationSystem, bool paused)
{
	uint32 queueSize = pNavigationSystem->GetWorkingQueueSize();
	if (queueSize > 0)
	{
		m_queueInitSize = max(m_queueInitSize, queueSize);

		if (m_queueInitSize > kMinQueueSizeToShowProgressNotification)
		{
			if (!m_pUpdateProgressNotification && !paused)
			{
				m_pUpdateProgressNotification = new CProgressNotification("Navigation System", "Updating MNM", true);
			}
			if (m_pUpdateProgressNotification)
			{
				float progressValue = 1.0f - (float)queueSize / (float)m_queueInitSize;
				QString progressString = "Updating MNM: " + QString::number(int(progressValue * 100)) + "%";
				if (paused)
				{
					progressString += " (Paused)";
				}
				m_pUpdateProgressNotification->SetProgress(progressValue);
				m_pUpdateProgressNotification->SetMessage(progressString);
			}
		}
	}
	else
	{
		m_queueInitSize = 0;

		if (m_pUpdateProgressNotification)
		{
			m_pUpdateProgressNotification->SetMessage("Updating MNM: Completed");
			m_pUpdateProgressNotification->SetProgress(1.0f);
			SAFE_DELETE(m_pUpdateProgressNotification);
		}
	}
}

