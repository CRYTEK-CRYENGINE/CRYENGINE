// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/IAIAction.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>

#include "QtViewPane.h"
#include "Objects/EntityObject.h"
#include "Objects/SelectionGroup.h"
#include "SmartObject.h"
#include "GameEngine.h"
#include "Util/MFCUtil.h"

#include "AI\AIManager.h"
#include "ItemDescriptionDlg.h"
#include "SmartObjectClassDialog.h"
#include "SmartObjectHelperDialog.h"
#include "SmartObjectTemplateDialog.h"

#include "HyperGraph\FlowGraphManager.h"
#include "HyperGraph\FlowGraph.h"
#include "SmartObjectHelperObject.h"
#include "SmartObjectsEditorDialog.h"
#include "Controls/QuestionDialog.h"
#include "Controls/SharedFonts.h"
#include "ClassFactory.h"
#include "Objects/ObjectManager.h"
#include "Util/FileUtil.h"

#define SOED_DIALOGFRAME_CLASSNAME "SmartObjectsEditorDialog"
#define CLASS_TEMPLATES_FOLDER     "Libs/SmartObjects/ClassTemplates/"

#define IDC_REPORTCONTROL          1234

enum
{
	IDW_SOED_TASK_PANE = AFX_IDW_CONTROLBAR_FIRST + 13,
	IDW_SOED_PROPS_PANE,
	IDW_SOED_TREE_PANE,
	IDW_SOED_DESC_PANE,
};

#define COLUMN_TYPE         -1
#define COLUMN_NAME         0
#define COLUMN_DESCRIPTION  1
#define COLUMN_USER_CLASS   2
#define COLUMN_USER_STATE   3
#define COLUMN_OBJECT_CLASS 4
#define COLUMN_OBJECT_STATE 5
#define COLUMN_ACTION       6
#define COLUMN_ORDER        7

// static members
SmartObjectConditions CSOLibrary::m_Conditions;
CSOLibrary::VectorHelperData CSOLibrary::m_vHelpers;
CSOLibrary::VectorEventData CSOLibrary::m_vEvents;
CSOLibrary::VectorStateData CSOLibrary::m_vStates;
CSOLibrary::VectorClassTemplateData CSOLibrary::m_vClassTemplates;
CSOLibrary::VectorClassData CSOLibrary::m_vClasses;
bool CSOLibrary::m_bSaveNeeded      = false;
bool CSOLibrary::m_bLoadNeeded      = true;
int CSOLibrary::m_iNumEditors       = 0;
CSOLibrary* CSOLibrary::m_pInstance = NULL;

// functor used for comparing and ordering data structures by name
template<class T>
struct less_name : public std::binary_function<const T&, const T&, bool>
{
	bool operator()(const T& _Left, const T& _Right) const
	{
		return _Left.name < _Right.name;
	}
};

// functor used for comparing and ordering data structures by name (case insensitive)
template<class T>
struct less_name_no_case : public std::binary_function<const T&, const T&, bool>
{
	bool operator()(const T& _Left, const T& _Right) const
	{
		return stricmp(_Left.name, _Right.name) < 0;
	}
};

void ReloadSmartObjects(IConsoleCmdArgs* /* pArgs */)
{
	CSOLibrary::Reload();
}

//////////////////////////////////////////////////////////////////////////
// Called by the editor to notify the listener about the specified event.
void CSOLibrary::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginGameMode:           // Sent when editor goes to game mode.
	case eNotify_OnBeginSimulationMode:     // Sent when simulation mode is started.
	case eNotify_OnBeginSceneSave:          // Sent when document is about to be saved.
	case eNotify_OnQuit:                    // Sent before editor quits.
	case eNotify_OnBeginNewScene:           // Sent when the document is begin to be cleared.
	case eNotify_OnBeginSceneOpen:          // Sent when document is about to be opened.
	case eNotify_OnClearLevelContents:      // Send when the document is about to close.
		if (m_bSaveNeeded && m_iNumEditors == 0 && Save())
		{
			m_bSaveNeeded = false;
			m_bLoadNeeded = false;
		}
		if (event == eNotify_OnQuit)
		{
			GetIEditor()->UnregisterNotifyListener(this);
			m_pInstance = NULL;
			delete this;
		}
		break;

	case eNotify_OnInit:                    // Sent after editor fully initialized.
		REGISTER_COMMAND("so_reload", ReloadSmartObjects, VF_NULL, "");
		break;

	case eNotify_OnEndSceneOpen:            // Sent after document have been opened.
	case eNotify_OnEndSceneSave:            // Sent after document have been saved.
	case eNotify_OnEndNewScene:             // Sent after the document have been cleared.
	case eNotify_OnMissionChange:           // Send when the current mission changes.
	case eNotify_OnEditModeChange:          // Sent when editing mode change (move,rotate,scale,....)
	case eNotify_OnEditToolBeginChange:     // Sent when edit tool is about to be changed (ObjectMode,TerrainModify,....)
	case eNotify_OnEditToolEndChange:       // Sent when edit tool has been changed (ObjectMode,TerrainModify,....)
	case eNotify_OnEndGameMode:             // Send when editor goes out of game mode.
	case eNotify_OnUpdateViewports:         // Sent when editor needs to update data in the viewports.
	case eNotify_OnInvalidateControls:      // Sent when editor needs to update some of the data that can be cached by controls like combo boxes.
	case eNotify_OnSelectionChange:         // Sent when object selection change.
	case eNotify_OnPlaySequence:            // Sent when editor start playing animation sequence.
	case eNotify_OnStopSequence:            // Sent when editor stop playing animation sequence.
		break;
	}
}

void CSOLibrary::Reload()
{
	m_bLoadNeeded = true;
	CSmartObjectsEditorDialog* pView = (CSmartObjectsEditorDialog*) GetIEditor()->FindView("Smart Objects Editor");
	if (pView)
		pView->ReloadEntries(true);
	else
		Load();
	InvalidateSOEntities();
	if (gEnv->pAISystem && !GetIEditor()->IsInGameMode() && !GetIEditor()->GetGameEngine()->GetSimulationMode())
		gEnv->pAISystem->GetSmartObjectManager()->ReloadSmartObjectRules();
}

void CSOLibrary::InvalidateSOEntities()
{
	CObjectClassDesc* pClass = GetIEditor()->GetObjectManager()->FindClass("SmartObject");
	if (!pClass)
		return;

	CBaseObjectsArray objects;
	GetIEditor()->GetObjectManager()->GetObjects(objects);
	CBaseObjectsArray::iterator it, itEnd = objects.end();
	for (it = objects.begin(); it != itEnd; ++it)
	{
		CBaseObject* pBaseObject = *it;
		if (pBaseObject->GetClassDesc() == pClass)
		{
			CSmartObject* pSOEntity = (CSmartObject*) pBaseObject;
			pSOEntity->OnPropertyChange(0);
		}
	}
}

bool CSOLibrary::Load()
{
	if (!m_bLoadNeeded)
		return true;
	m_bLoadNeeded = false;

	if (!m_pInstance)
	{
		m_pInstance = new CSOLibrary;
		GetIEditor()->RegisterNotifyListener(m_pInstance);
	}

	LoadClassTemplates();

	// load the rules from SmartObjects.xml
	if (!LoadFromFile(PathUtil::GetGameFolder() + "/" + SMART_OBJECTS_XML))
	{
		m_bLoadNeeded = true;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CSOLibrary::Load() failed to load from %s", SMART_OBJECTS_XML);
		return false;
	}

	m_bSaveNeeded = false;
	return true;
}

void CSOLibrary::LoadClassTemplates()
{
	m_vClassTemplates.clear();

	string sLibPath = PathUtil::Make(CLASS_TEMPLATES_FOLDER, "*", "xml");
	ICryPak* pack = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t handle = pack->FindFirst(sLibPath, &fd);
	int nCount = 0;
	if (handle < 0)
		return;

	do
	{
		XmlNodeRef root = GetISystem()->LoadXmlFromFile(string(CLASS_TEMPLATES_FOLDER) + fd.name);
		if (!root)
			continue;

		CClassTemplateData* data = AddClassTemplate(PathUtil::GetFileName(fd.name));
		if (!data)
			continue;

		int count = root->getChildCount();
		for (int i = 0; i < count; ++i)
		{
			XmlNodeRef node = root->getChild(i);
			if (node->isTag("object"))
			{
				data->model = node->getAttr("model");
			}
			else if (node->isTag("helper"))
			{
				CClassTemplateData::CTemplateHelper helper;
				helper.name = node->getAttr("name");
				node->getAttr("pos", helper.qt.t);
				node->getAttr("rot", helper.qt.q);
				node->getAttr("radius", helper.radius);
				node->getAttr("projectOnGround", helper.project);
				data->helpers.push_back(helper);
			}
		}
	}
	while (pack->FindNext(handle, &fd) >= 0);

	pack->FindClose(handle);
}

CSOLibrary::CClassTemplateData* CSOLibrary::AddClassTemplate(const char* name)
{
	CClassTemplateData classTemplateData;
	classTemplateData.name = name;
	VectorClassTemplateData::iterator it = std::lower_bound(m_vClassTemplates.begin(), m_vClassTemplates.end(), classTemplateData, less_name_no_case<CClassTemplateData>());

	if (it != m_vClassTemplates.end() && stricmp(it->name, name) == 0)
	{
		return NULL;
	}

	return &*m_vClassTemplates.insert(it, classTemplateData);
}

//////////////////////////////////////////////////////////////////////////
// Smart Objects UI structures.
//////////////////////////////////////////////////////////////////////////

/** User Interface definition of Smart Objects
 */
class CSmartObjectsUIDefinition
{
public:
	CVariable<CString> sName;
	CVariable<CString> sFolder;
	CVariable<CString> sDescription;
	CVariable<bool>    bEnabled;
	CVariable<CString> sTemplate;

	CVariable<bool>    bNavigationRule;
	CVariable<CString> sEvent;
	CVariable<CString> sChainedUserEvent;
	CVariable<CString> sChainedObjectEvent;

	CVariableArray     objectTable;
	CVariableArray     userTable;
	CVariableArray     navigationTable;
	CVariableArray     limitsTable;
	CVariableArray     delayTable;
	CVariableArray     multipliersTable;
	CVariableArray     actionTable;

	//////////////////////////////////////////////////////////////////////////
	// Object variables
	CVariable<CString> objectClass;
	CVariable<CString> objectState;
	CVariable<CString> objectHelper;

	//////////////////////////////////////////////////////////////////////////
	// User variables
	CVariable<CString> userClass;
	CVariable<CString> userState;
	CVariable<CString> userHelper;
	CVariable<int>     iMaxAlertness;

	//////////////////////////////////////////////////////////////////////////
	// Navigation variables
	CVariable<CString> entranceHelper;
	CVariable<CString> exitHelper;

	//////////////////////////////////////////////////////////////////////////
	// Limits variables
	CVariable<float> limitsDistanceFrom;
	CVariable<float> limitsDistanceTo;
	CVariable<float> limitsOrientation;
	CVariable<bool>  horizLimitOnly;
	CVariable<float> limitsOrientationToTarget;

	//////////////////////////////////////////////////////////////////////////
	// Delay variables
	CVariable<float> delayMinimum;
	CVariable<float> delayMaximum;
	CVariable<float> delayMemory;

	//////////////////////////////////////////////////////////////////////////
	// Multipliers variables
	CVariable<float> multipliersProximity;
	CVariable<float> multipliersOrientation;
	CVariable<float> multipliersVisibility;
	CVariable<float> multipliersRandomness;

	//////////////////////////////////////////////////////////////////////////
	// Action variables
	CVariable<float>   fLookAtOnPerc;
	CVariable<CString> userPreActionState;
	CVariable<CString> objectPreActionState;
	CVariableEnum<int> actionType;
	CVariable<CString> actionName;
	CVariable<CString> userPostActionState;
	CVariable<CString> objectPostActionState;

	//////////////////////////////////////////////////////////////////////////
	// Exact positioning
	CVariableEnum<int> approachSpeed;
	CVariableEnum<int> approachStance;
	CVariable<CString> animationHelper;
	CVariable<CString> approachHelper;
	CVariable<float>   fStartWidth;
	CVariable<float>   fStartDirectionTolerance;
	CVariable<float>   fStartArcAngle;

	//	CVarEnumList<int> enumType;
	//	CVarEnumList<int> enumBlendType;

	CVarBlockPtr             m_vars;
	IVariable::OnSetCallback m_onSetCallback;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		m_vars = new CVarBlock;

		/*
		    //////////////////////////////////////////////////////////////////////////
		    // Init enums.
		    //////////////////////////////////////////////////////////////////////////
		    enumBlendType.AddRef(); // We not using pointer.
		    enumBlendType.AddItem( "AlphaBlend",ParticleBlendType_AlphaBased );
		    enumBlendType.AddItem( "ColorBased",ParticleBlendType_ColorBased );
		    enumBlendType.AddItem( "Additive",ParticleBlendType_Additive );
		    enumBlendType.AddItem( "None",ParticleBlendType_None );

		    enumType.AddRef(); // We not using pointer.
		    enumType.AddItem( "Billboard",PART_FLAG_BILLBOARD );
		    enumType.AddItem( "Horizontal",PART_FLAG_HORIZONTAL );
		    enumType.AddItem( "Underwater",PART_FLAG_UNDERWATER );
		    enumType.AddItem( "Align To Direction",PART_FLAG_ALIGN_TO_DIR );
		    enumType.AddItem( "Line",PART_FLAG_LINEPARTICLE );
		 */

		AddVariable(m_vars, sName, "Name", IVariable::DT_SIMPLE, "The name for this smart object rule");
		AddVariable(m_vars, sDescription, "Description", IVariable::DT_SIMPLE, "A description for this smart object rule");
		AddVariable(m_vars, sFolder, "Location", IVariable::DT_SIMPLE, "The path to folder where this smart object rule is located");
		AddVariable(m_vars, bEnabled, "Enabled", IVariable::DT_SIMPLE, "Is this smart object rule active?");
		AddVariable(m_vars, sTemplate, "Template", IVariable::DT_SOTEMPLATE, "Template used on this smart object rule");

		AddVariable(m_vars, bNavigationRule, "Navigation Rule", IVariable::DT_SIMPLE, "Is this a navigation smart object rule?");
		AddVariable(m_vars, sEvent, "Event", IVariable::DT_SOEVENT, "Event on which this rule is triggered");
		AddVariable(m_vars, sChainedUserEvent, "Chained User Event", IVariable::DT_SOEVENT, "Event chained with the user");
		AddVariable(m_vars, sChainedObjectEvent, "Chained Object Event", IVariable::DT_SOEVENT, "Event chained with the object");
		//		AddVariable( m_vars, bRelativeToTarget, "Relative to Target" );

		// setup the limits
		iMaxAlertness.SetLimits(0.0f, 2.0f);

		//passRadius.SetLimits( 0.0f, 1000.0f );

		limitsDistanceFrom.SetLimits(0.0f, 1000.0f);
		limitsDistanceTo.SetLimits(0.0f, 1000.0f);
		limitsOrientation.SetLimits(-360.0f, 360.0f);
		limitsOrientationToTarget.SetLimits(-360.0f, 360.0f);

		delayMinimum.SetLimits(0.0f, 1000.0f);
		delayMaximum.SetLimits(0.0f, 1000.0f);
		delayMemory.SetLimits(0.0f, 1000.0f);

		multipliersRandomness.SetLimits(0.0f, 100.0f);
		fLookAtOnPerc.SetLimits(0.0f, 100.0f);

		fStartWidth.SetLimits(0.0f, 10.0f);
		fStartDirectionTolerance.SetLimits(0.0f, 180.0f);
		fStartArcAngle.SetLimits(-360.0f, 360.0f);

		// setup the enumeration lists
		actionType.AddEnumItem("None", 0);                 // eAT_None
		actionType.AddEnumItem("AI Action", 1);            // eAT_Action
		actionType.AddEnumItem("High Priority Action", 2); // eAT_PriorityAction
		//		actionType.AddEnumItem( "Go To Object", 3 );			// eAT_Approach
		//		actionType.AddEnumItem( "High Priority Go To", 4 );		// eAT_PriorityApproach
		//		actionType.AddEnumItem( "Go To + Action", 5 );			// eAT_ApproachAction
		//		actionType.AddEnumItem( "H.P. Go To + Action", 6 );		// eAT_PriorityApproachAction
		actionType.AddEnumItem("AI Signal", 7);              // eAT_AISignal
		actionType.AddEnumItem("One Shot Animation", 8);     // eAT_AnimationSignal
		actionType.AddEnumItem("Looping Animation", 9);      // eAT_AnimationAction
		actionType.AddEnumItem("HP One Shot Animation", 10); // eAT_PriorityAnimationSignal
		actionType.AddEnumItem("HP Looping Animation", 11);  // eAT_PriorityAnimationAction

		approachSpeed.AddEnumItem("<ignore>", -1);
		approachSpeed.AddEnumItem("Normal/Walk", 0);
		approachSpeed.AddEnumItem("Fast/Run", 1);
		approachSpeed.AddEnumItem("Very Fast/Sprint", 2);

		approachStance.AddEnumItem("<ignore>", -1);
		approachStance.AddEnumItem("Combat", STANCE_STAND);
		approachStance.AddEnumItem("Crouch", STANCE_CROUCH);
		approachStance.AddEnumItem("Prone", STANCE_PRONE);
		approachStance.AddEnumItem("Relaxed", STANCE_RELAXED);
		approachStance.AddEnumItem("Stealth", STANCE_STEALTH);
		approachStance.AddEnumItem("Alerted", STANCE_ALERTED);
		approachStance.AddEnumItem("LowCover", STANCE_LOW_COVER);
		approachStance.AddEnumItem("HighCover", STANCE_HIGH_COVER);

		//////////////////////////////////////////////////////////////////////////
		// Init tables.
		//////////////////////////////////////////////////////////////////////////

		objectTable.DeleteAllVariables();
		userTable.DeleteAllVariables();
		navigationTable.DeleteAllVariables();
		limitsTable.DeleteAllVariables();
		delayTable.DeleteAllVariables();
		multipliersTable.DeleteAllVariables();
		actionTable.DeleteAllVariables();

		AddVariable(m_vars, userTable, "User", IVariable::DT_SIMPLE, "Define the smart object user");
		AddVariable(m_vars, objectTable, "Object", IVariable::DT_SIMPLE, "Define the smart object to be used\nIf empty the user will use himself");
		AddVariable(m_vars, navigationTable, "Navigation", IVariable::DT_SIMPLE, "An AI navigation link will be created between these two helpers");
		AddVariable(m_vars, limitsTable, "Limits", IVariable::DT_SIMPLE, "Only objects within this range will be considered");
		AddVariable(m_vars, delayTable, "Delay", IVariable::DT_SIMPLE, "");
		AddVariable(m_vars, multipliersTable, "Multipliers", IVariable::DT_SIMPLE, "");
		AddVariable(m_vars, actionTable, "Action", IVariable::DT_SIMPLE, "");

		userTable.SetFlags(IVariable::UI_BOLD);
		objectTable.SetFlags(IVariable::UI_BOLD);
		navigationTable.SetFlags(IVariable::UI_BOLD);
		limitsTable.SetFlags(IVariable::UI_BOLD);
		delayTable.SetFlags(IVariable::UI_BOLD);
		multipliersTable.SetFlags(IVariable::UI_BOLD);
		actionTable.SetFlags(IVariable::UI_BOLD);

		AddVariable(userTable, userClass, "Class", IVariable::DT_SOCLASS, "Only users of this class will be considered");
		AddVariable(userTable, userState, "State Pattern", IVariable::DT_SOSTATEPATTERN, "Only users which state matches this pattern will be considered");
		AddVariable(userTable, userHelper, "Helper", IVariable::DT_SOHELPER, "User's helper point used in calculations");
		AddVariable(userTable, iMaxAlertness, "Max. Alertness", IVariable::DT_SIMPLE, "Consider only users which alertness state is not higher");

		AddVariable(objectTable, objectClass, "Class", IVariable::DT_SOCLASS, "Only objects of this class will be considered");
		AddVariable(objectTable, objectState, "State Pattern", IVariable::DT_SOSTATEPATTERN, "Only objects which state matches this pattern will be considered");
		AddVariable(objectTable, objectHelper, "Helper", IVariable::DT_SOHELPER, "Object's helper point used in calculations");

		//		AddVariable( navigationTable, passRadius, "Pass Radius", IVariable::DT_SIMPLE, "Defines a navigational smart object if higher than zero" );
		AddVariable(navigationTable, entranceHelper, "Entrance", IVariable::DT_SONAVHELPER, "Entrance point of navigational smart object");
		AddVariable(navigationTable, exitHelper, "Exit", IVariable::DT_SONAVHELPER, "Exit point of navigational smart object");

		AddVariable(limitsTable, limitsDistanceFrom, "Distance from", IVariable::DT_SIMPLE, "Range at which objects could be used");
		AddVariable(limitsTable, limitsDistanceTo, "Distance to", IVariable::DT_SIMPLE, "Range at which objects could be used");
		AddVariable(limitsTable, limitsOrientation, "Orientation", IVariable::DT_SIMPLE, "Orientation limit at which objects could be used");
		AddVariable(limitsTable, horizLimitOnly, "Horizontal Limit Only", IVariable::DT_SIMPLE, "When checked, the limit only applies to horizontal orientation (vertical orientation is excluded)");
		AddVariable(limitsTable, limitsOrientationToTarget, "Orient. to Target", IVariable::DT_SIMPLE, "Object's orientation limit towards the user's attention target");

		AddVariable(delayTable, delayMinimum, "Minimum", IVariable::DT_SIMPLE, "Shortest time needed for interaction to happen");
		AddVariable(delayTable, delayMaximum, "Maximum", IVariable::DT_SIMPLE, "Longest time needed for interaction to happen");
		AddVariable(delayTable, delayMemory, "Memory", IVariable::DT_SIMPLE, "Time needed to forget");

		AddVariable(multipliersTable, multipliersProximity, "Proximity", IVariable::DT_SIMPLE, "How important is the proximity");
		AddVariable(multipliersTable, multipliersOrientation, "Orientation", IVariable::DT_SIMPLE, "How important is the orientation of the user towards the object");
		AddVariable(multipliersTable, multipliersVisibility, "Visibility", IVariable::DT_SIMPLE, "How important is the visibility between user and object");
		AddVariable(multipliersTable, multipliersRandomness, "Randomness", IVariable::DT_PERCENT, "How much randomness will be added");

		AddVariable(actionTable, fLookAtOnPerc, "LookAt On %", IVariable::DT_PERCENT, "Look at the object before using it (only when the user is an AI agent)");
		AddVariable(actionTable, userPreActionState, "User: Pre-Action State", IVariable::DT_SOSTATES, "How user's states will change before executing the action");
		AddVariable(actionTable, objectPreActionState, "Object: Pre-Action State", IVariable::DT_SOSTATES, "How object's states will change before executing the action");
		AddVariable(actionTable, actionType, "Action Type", IVariable::DT_SIMPLE, "What type of action to execute");
		AddVariable(actionTable, actionName, "Action Name", IVariable::DT_SOACTION, "The action to be executed on interaction");
		AddVariable(actionTable, userPostActionState, "User: Post-Action State", IVariable::DT_SOSTATES, "How user's states will change after executing the action");
		AddVariable(actionTable, objectPostActionState, "Object: Post-Action State", IVariable::DT_SOSTATES, "How object's states will change after executing the action");

		AddVariable(actionTable, approachSpeed, "Speed", IVariable::DT_SIMPLE, "Movement speed while approaching the helper point");
		AddVariable(actionTable, approachStance, "Stance", IVariable::DT_SIMPLE, "Body stance while approaching the helper point");
		AddVariable(objectTable, animationHelper, "Animation Helper", IVariable::DT_SOANIMHELPER, "Object's helper point at which the animation will be played");
		AddVariable(objectTable, approachHelper, "Approach Helper", IVariable::DT_SOHELPER, "Object's helper point to approach before the animation play (if empty Animation Helper is used)");
		AddVariable(actionTable, fStartWidth, "Start Line Width", IVariable::DT_SIMPLE, "Width of the starting line for playing animation");
		AddVariable(actionTable, fStartArcAngle, "Arc Angle", IVariable::DT_SIMPLE, "Arc angle if the start line need to be bended");
		AddVariable(actionTable, fStartDirectionTolerance, "Direction Tolerance", IVariable::DT_SIMPLE, "Start direction tolerance for playing animation");

		return m_vars;
	}

	//////////////////////////////////////////////////////////////////////////
	// returns true if folder was edited
	bool GetConditionFromUI(SmartObjectCondition& condition) const
	{
		CString temp;
		bool result = false;

		condition.sName = (const CString&) sName;
		condition.sDescription = (const CString&) sDescription;

		CString newLocation;
		CString location = sFolder;
		int i = 0;
		CString folder = location.Tokenize(_T("/\\"), i);
		while (!folder.IsEmpty())
		{
			newLocation += _T('/');
			newLocation += folder;
			folder = location.Tokenize(_T("/\\"), i);
		}
		if (newLocation != condition.sFolder.c_str())
		{
			result = true;
			condition.sFolder = newLocation;
		}

		condition.bEnabled = bEnabled;
		condition.iRuleType = bNavigationRule ? 1 : 0;
		condition.sEvent = (const CString&) sEvent;
		condition.sChainedUserEvent = (const CString&) sChainedUserEvent;
		condition.sChainedObjectEvent = (const CString&) sChainedObjectEvent;

		userClass.Get(temp);
		condition.sUserClass = temp.Trim(" :");
		userState.Get(temp);
		condition.sUserState = temp;
		userHelper.Get(temp);
		temp.Delete(0, temp.ReverseFind(':') + 1);
		condition.sUserHelper = temp;
		condition.iMaxAlertness = iMaxAlertness;

		objectClass.Get(temp);
		condition.sObjectClass = temp.Trim(" :");
		objectState.Get(temp);
		condition.sObjectState = temp;
		objectHelper.Get(temp);
		temp.Delete(0, temp.ReverseFind(':') + 1);
		condition.sObjectHelper = temp;

		entranceHelper.Get(temp);
		temp.Delete(0, temp.ReverseFind(':') + 1);
		condition.sEntranceHelper = temp;
		exitHelper.Get(temp);
		temp.Delete(0, temp.ReverseFind(':') + 1);
		condition.sExitHelper = temp;

		condition.fDistanceFrom = limitsDistanceFrom;
		condition.fDistanceTo = limitsDistanceTo;
		condition.fOrientationLimit = limitsOrientation;
		condition.bHorizLimitOnly = horizLimitOnly;
		condition.fOrientationToTargetLimit = limitsOrientationToTarget;

		condition.fMinDelay = delayMinimum;
		condition.fMaxDelay = delayMaximum;
		condition.fMemory = delayMemory;

		condition.fProximityFactor = multipliersProximity;
		condition.fOrientationFactor = multipliersOrientation;
		condition.fVisibilityFactor = multipliersVisibility;
		condition.fRandomnessFactor = multipliersRandomness;

		condition.fLookAtOnPerc = fLookAtOnPerc;
		objectPreActionState.Get(temp);
		condition.sObjectPreActionState = temp;
		userPreActionState.Get(temp);
		condition.sUserPreActionState = temp;
		condition.eActionType = (EActionType) (int) actionType;
		actionName.Get(temp);
		condition.sAction = temp;
		objectPostActionState.Get(temp);
		condition.sObjectPostActionState = temp;
		userPostActionState.Get(temp);
		condition.sUserPostActionState = temp;

		condition.fApproachSpeed = approachSpeed;
		condition.iApproachStance = approachStance;
		animationHelper.Get(temp);
		temp.Delete(0, temp.ReverseFind(':') + 1);
		condition.sAnimationHelper = temp;
		approachHelper.Get(temp);
		temp.Delete(0, temp.ReverseFind(':') + 1);
		condition.sApproachHelper = temp;
		condition.fStartWidth = fStartWidth;
		condition.fDirectionTolerance = fStartDirectionTolerance;
		condition.fStartArcAngle = fStartArcAngle;

		return result;
	}

	//////////////////////////////////////////////////////////////////////////
	void SetConditionToUI(const SmartObjectCondition& condition, CPropertyCtrl* pPropertyCtrl)
	{
		const MapTemplates& mapTemplates = GetIEditor()->GetAIManager()->GetMapTemplates();
		MapTemplates::const_iterator find = mapTemplates.find(condition.iTemplateId);
		if (find == mapTemplates.end())
			find = mapTemplates.find(0);

		// template id 0 must always exist!
		// if doesn't then probably templates aren't loaded...
		assert(find != mapTemplates.end());

		CSOTemplate* pTemplate = find->second;
		sTemplate = pTemplate->name.GetString();

		sName = condition.sName.c_str();
		sFolder = condition.sFolder.c_str();
		sDescription = condition.sDescription.c_str();

		bEnabled = condition.bEnabled;
		bNavigationRule = condition.iRuleType == 1;
		sEvent = condition.sEvent.c_str();
		sChainedUserEvent = condition.sChainedUserEvent.c_str();
		sChainedObjectEvent = condition.sChainedObjectEvent.c_str();

		CString temp = condition.sUserClass;
		temp.Trim(" :");
		userClass = temp;
		userState = condition.sUserState.c_str();
		userHelper = temp.IsEmpty() ? "" : temp + ':' + condition.sUserHelper.c_str();
		iMaxAlertness = condition.iMaxAlertness;

		temp = condition.sObjectClass;
		temp.Trim(" :");
		objectClass = temp;
		objectState = condition.sObjectState.c_str();
		objectHelper = temp.IsEmpty() ? "" : temp + ':' + condition.sObjectHelper.c_str();

		entranceHelper = temp + ':' + condition.sEntranceHelper.c_str();
		exitHelper = temp + ':' + condition.sExitHelper.c_str();

		limitsDistanceFrom = condition.fDistanceFrom;
		limitsDistanceTo = condition.fDistanceTo;
		limitsOrientation = condition.fOrientationLimit;
		horizLimitOnly = condition.bHorizLimitOnly;
		limitsOrientationToTarget = condition.fOrientationToTargetLimit;

		delayMinimum = condition.fMinDelay;
		delayMaximum = condition.fMaxDelay;
		delayMemory = condition.fMemory;

		multipliersProximity = condition.fProximityFactor;
		multipliersOrientation = condition.fOrientationFactor;
		multipliersVisibility = condition.fVisibilityFactor;
		multipliersRandomness = condition.fRandomnessFactor;

		fLookAtOnPerc = condition.fLookAtOnPerc;
		userPreActionState = condition.sUserPreActionState.c_str();
		objectPreActionState = condition.sObjectPreActionState.c_str();
		actionType = condition.eActionType;
		actionName = condition.sAction.c_str();
		userPostActionState = condition.sUserPostActionState.c_str();
		objectPostActionState = condition.sObjectPostActionState.c_str();

		approachSpeed = condition.fApproachSpeed;
		approachStance = condition.iApproachStance;
		animationHelper = temp.IsEmpty() ? "" : temp + ':' + condition.sAnimationHelper.c_str();
		approachHelper = temp.IsEmpty() ? "" : temp + ':' + condition.sApproachHelper.c_str();
		fStartWidth = condition.fStartWidth;
		fStartDirectionTolerance = condition.fDirectionTolerance;
		fStartArcAngle = condition.fStartArcAngle;

		// remove all variables from UI
		m_vars->DeleteAllVariables();

		// add standard variables
		m_vars->AddVariable(&sName);
		m_vars->AddVariable(&sDescription);
		m_vars->AddVariable(&sFolder);
		m_vars->AddVariable(&bEnabled);
		m_vars->AddVariable(&sTemplate);

		// add template variables
		AddVariablesFromTemplate(m_vars, pTemplate->params);

		pPropertyCtrl->DeleteAllItems();
		pPropertyCtrl->AddVarBlock(m_vars);

		pPropertyCtrl->ExpandAll();
		for (int i = 0; i < m_collapsedItems.size(); ++i)
			pPropertyCtrl->Expand(pPropertyCtrl->FindItemByVar(m_collapsedItems[i]), false);

		m_collapsedItems.clear();
	}

	IVariable* ResolveVariable(const CSOParam* pParam)
	{
		IVariable* pVar = NULL;
		if (pParam->sName == "bNavigationRule")
			return &bNavigationRule;
		else if (pParam->sName == "sEvent")
			return &sEvent;
		else if (pParam->sName == "sChainedUserEvent")
			return &sChainedUserEvent;
		else if (pParam->sName == "sChainedObjectEvent")
			return &sChainedObjectEvent;
		else if (pParam->sName == "userClass")
			return &userClass;
		else if (pParam->sName == "userState")
			return &userState;
		else if (pParam->sName == "userHelper")
			return &userHelper;
		else if (pParam->sName == "iMaxAlertness")
			return &iMaxAlertness;
		else if (pParam->sName == "objectClass")
			return &objectClass;
		else if (pParam->sName == "objectState")
			return &objectState;
		else if (pParam->sName == "objectHelper")
			return &objectHelper;
		else if (pParam->sName == "entranceHelper")
			return &entranceHelper;
		else if (pParam->sName == "exitHelper")
			return &exitHelper;
		else if (pParam->sName == "limitsDistanceFrom")
			return &limitsDistanceFrom;
		else if (pParam->sName == "limitsDistanceTo")
			return &limitsDistanceTo;
		else if (pParam->sName == "limitsOrientation")
			return &limitsOrientation;
		else if (pParam->sName == "horizLimitOnly")
			return &horizLimitOnly;
		else if (pParam->sName == "limitsOrientationToTarget")
			return &limitsOrientationToTarget;
		else if (pParam->sName == "delayMinimum")
			return &delayMinimum;
		else if (pParam->sName == "delayMaximum")
			return &delayMaximum;
		else if (pParam->sName == "delayMemory")
			return &delayMemory;
		else if (pParam->sName == "multipliersProximity")
			return &multipliersProximity;
		else if (pParam->sName == "multipliersOrientation")
			return &multipliersOrientation;
		else if (pParam->sName == "multipliersVisibility")
			return &multipliersVisibility;
		else if (pParam->sName == "multipliersRandomness")
			return &multipliersRandomness;
		else if (pParam->sName == "fLookAtOnPerc")
			return &fLookAtOnPerc;
		else if (pParam->sName == "userPreActionState")
			return &userPreActionState;
		else if (pParam->sName == "objectPreActionState")
			return &objectPreActionState;
		else if (pParam->sName == "actionType")
			return &actionType;
		else if (pParam->sName == "actionName")
			return &actionName;
		else if (pParam->sName == "userPostActionState")
			return &userPostActionState;
		else if (pParam->sName == "objectPostActionState")
			return &objectPostActionState;
		else if (pParam->sName == "approachSpeed")
			return &approachSpeed;
		else if (pParam->sName == "approachStance")
			return &approachStance;
		else if (pParam->sName == "animationHelper")
			return &animationHelper;
		else if (pParam->sName == "approachHelper")
			return &approachHelper;
		else if (pParam->sName == "fStartRadiusTolerance")   // backward compatibility
			return &fStartWidth;
		else if (pParam->sName == "fStartWidth")
			return &fStartWidth;
		else if (pParam->sName == "fStartDirectionTolerance")
			return &fStartDirectionTolerance;
		else if (pParam->sName == "fTargetRadiusTolerance")   // backward compatibility
			return &fStartArcAngle;
		else if (pParam->sName == "fStartArcAngle")
			return &fStartArcAngle;

		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr(string("WARNING:\nSmart object template has a Param tag named ") + pParam->sName + " which is not recognized as valid name!\nThe Param will be ignored..."));
		return NULL;
	}

private:
	std::vector<IVariable*> m_collapsedItems;

	void AddVariablesFromTemplate(CVarBlock* var, CSOParamBase* param)
	{
		while (param)
		{
			if (!param->bIsGroup)
			{
				CSOParam* pParam = static_cast<CSOParam*>(param);
				if (pParam->bVisible)
				{
					if (!pParam->pVariable)
						pParam->pVariable = ResolveVariable(pParam);

					if (pParam->pVariable)
					{
						pParam->pVariable->SetHumanName(pParam->sCaption);
						pParam->pVariable->SetDescription(pParam->sHelp);
						pParam->pVariable->SetFlags(pParam->bEditable ? 0 : IVariable::UI_DISABLED);
						var->AddVariable(param->pVariable);
					}
				}
			}
			else
			{
				CSOParamGroup* pParam = static_cast<CSOParamGroup*>(param);
				pParam->pVariable->DeleteAllVariables();

				pParam->pVariable->SetName(pParam->sName);
				pParam->pVariable->SetDescription(pParam->sHelp);
				var->AddVariable(pParam->pVariable);
				AddVariablesFromTemplate(pParam->pVariable, pParam->pChildren);

				if (!pParam->bExpand)
					m_collapsedItems.push_back(pParam->pVariable);
			}
			param = param->pNext;
		}
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariablesFromTemplate(IVariable* var, CSOParamBase* param)
	{
		while (param)
		{
			if (!param->bIsGroup)
			{
				CSOParam* pParam = static_cast<CSOParam*>(param);
				if (pParam->bVisible)
				{
					if (!pParam->pVariable)
						pParam->pVariable = ResolveVariable(pParam);

					if (pParam->pVariable)
					{
						pParam->pVariable->SetHumanName(pParam->sCaption);
						pParam->pVariable->SetDescription(pParam->sHelp);
						pParam->pVariable->SetFlags(pParam->bEditable ? 0 : IVariable::UI_DISABLED);
						var->AddVariable(param->pVariable);
					}
				}
			}
			else
			{
				CSOParamGroup* pParam = static_cast<CSOParamGroup*>(param);
				pParam->pVariable->DeleteAllVariables();

				pParam->pVariable->SetName(pParam->sName);
				pParam->pVariable->SetDescription(pParam->sHelp);
				var->AddVariable(pParam->pVariable);
				AddVariablesFromTemplate(pParam->pVariable, pParam->pChildren);

				if (!pParam->bExpand)
					m_collapsedItems.push_back(pParam->pVariable);
			}
			param = param->pNext;
		}
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariable(CVariableArray& varArray, CVariableBase& var, const char* varName, unsigned char dataType = IVariable::DT_SIMPLE, const char* description = NULL)
	{
		var.AddRef(); // Variables are local and must not be released by CVarBlock.
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		if (description)
			var.SetDescription(description);
		if (m_onSetCallback)
			var.AddOnSetCallback(m_onSetCallback);
		varArray.AddVariable(&var);
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariable(CVarBlock* vars, CVariableBase& var, const char* varName, unsigned char dataType = IVariable::DT_SIMPLE, const char* description = NULL)
	{
		var.AddRef(); // Variables are local and must not be released by CVarBlock.
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		if (description)
			var.SetDescription(description);
		if (m_onSetCallback)
			var.AddOnSetCallback(m_onSetCallback);
		vars->AddVariable(&var);
	}
};

static CSmartObjectsUIDefinition gSmartObjectsUI;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CSmartObjectEntry : public CXTPReportRecord
{
	DECLARE_DYNAMIC(CSmartObjectEntry)

public:
	SmartObjectCondition* m_pCondition;
	//	CXTPReportRecordItem*	m_pIconItem;

	CSmartObjectEntry(SmartObjectCondition* pCondition)
	{
		m_pCondition = pCondition;

		//		m_pIconItem = AddItem(new CXTPReportRecordItem());
		CXTPReportRecordItem* m_pNameItem = AddItem(new CXTPReportRecordItemText(pCondition->sName));
		m_pNameItem->HasCheckbox(TRUE);
		m_pNameItem->SetChecked(pCondition->bEnabled);
		AddItem(new CXTPReportRecordItemText(pCondition->sDescription));
		AddItem(new CXTPReportRecordItemText(pCondition->sUserClass));
		AddItem(new CXTPReportRecordItemText(pCondition->sUserState));
		AddItem(new CXTPReportRecordItemText(pCondition->sObjectClass));
		AddItem(new CXTPReportRecordItemText(pCondition->sObjectState));
		AddItem(new CXTPReportRecordItemText(pCondition->sAction))->AddHyperlink(
		  new CXTPReportHyperlink(0, pCondition->eActionType == eAT_Action || pCondition->eActionType == eAT_PriorityAction
		                          ? pCondition->sAction.length() : 0));
		AddItem(new CXTPReportRecordItemNumber(pCondition->iOrder));

		COLORREF color = m_pCondition->bEnabled ? GetSysColor(COLOR_WINDOWTEXT) : GetSysColor(COLOR_GRAYTEXT);
		for (int i = 0; i < 6; ++i)
			GetItem(i)->SetTextColor(color);

		//SetPreviewItem(new CXTPReportRecordItemPreview(strPreview));
		//		int nIcon = 0;
		//		if (err.severity == CErrorRecord::ESEVERITY_ERROR)
		//			nIcon = BITMAP_ERROR;
		//		else if (err.severity == CErrorRecord::ESEVERITY_WARNING)
		//			nIcon = BITMAP_WARNING;
		//		else if (err.severity == CErrorRecord::ESEVERITY_COMMENT)
		//			nIcon = BITMAP_COMMENT;
		//		m_pIconItem->SetIconIndex(nIcon);
		//		m_pIconItem->SetGroupPriority(nIcon);
		//		m_pIconItem->SetSortPriority(nIcon);
	}

	void Update()
	{
		// There's a bug in SetCaption - that's why I'm setting captions to " " instead of ""
		GetItem(0)->SetCaption(m_pCondition->sName.empty() ? " " : m_pCondition->sName.c_str());
		GetItem(0)->SetChecked(m_pCondition->bEnabled);
		GetItem(1)->SetCaption(m_pCondition->sDescription.empty() ? " " : m_pCondition->sDescription.c_str());
		GetItem(2)->SetCaption(m_pCondition->sUserClass.empty() ? " " : m_pCondition->sUserClass.c_str());
		GetItem(3)->SetCaption(m_pCondition->sUserState.empty() ? " " : m_pCondition->sUserState.c_str());
		GetItem(4)->SetCaption(m_pCondition->sObjectClass.empty() ? " " : m_pCondition->sObjectClass.c_str());
		GetItem(5)->SetCaption(m_pCondition->sObjectState.empty() ? " " : m_pCondition->sObjectState.c_str());
		GetItem(6)->SetCaption(m_pCondition->sAction.empty() ? " " : m_pCondition->sAction.c_str());
		GetItem(6)->GetHyperlinkAt(0)->m_nHyperTextLen = m_pCondition->eActionType == eAT_Action || m_pCondition->eActionType == eAT_PriorityAction ? m_pCondition->sAction.length() : 0;
		//AddHyperlink(new CXTPReportHyperlink( 0, m_pCondition->sAction.length() ));
		CString order;
		order.Format(_T("%04d"), m_pCondition->iRuleType);
		GetItem(7)->SetCaption(order);

		COLORREF color = m_pCondition->bEnabled ? GetSysColor(COLOR_WINDOWTEXT) : GetSysColor(COLOR_GRAYTEXT);
		for (int i = 0; i < 6; ++i)
			GetItem(i)->SetTextColor(color);
	}

	/*
	   virtual void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	   {
	    if (m_pIconItem == pDrawArgs->pItem && pDrawArgs->pItem->GetIconIndex() == 0)
	    {
	      // Red error text.
	      pItemMetrics->clrForeground = RGB(0xFF, 0, 0);
	    }
	   }
	 */
};

IMPLEMENT_DYNAMIC(CSmartObjectEntry, CXTPReportRecord)

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CDragReportCtrl, CXTPReportControl)

BEGIN_MESSAGE_MAP(CDragReportCtrl, CXTPReportControl)
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_CAPTURECHANGED()
ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

CDragReportCtrl::CDragReportCtrl()
{
	m_bDragging = false;
	m_ptDrag.x = 0;
	m_ptDrag.y = 0;

	m_hCursorNoDrop = (HCURSOR) LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_POINTER_SO_SELECT), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE);
	m_hCursorNormal = (HCURSOR) LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_POINTER), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE);

	m_pSourceRow = NULL;
	m_pTargetRow = NULL;
}

CDragReportCtrl::~CDragReportCtrl()
{
	if (m_hCursorNoDrop)
		DestroyCursor(m_hCursorNoDrop);
	if (m_hCursorNormal)
		DestroyCursor(m_hCursorNormal);
}

void CDragReportCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_bDragging)
	{
		__super::OnLButtonDown(nFlags, point);

		m_pTargetRow = NULL;
		m_pSourceRow = HitTest(point);
		if (m_pSourceRow)
		{
			m_ptDrag = point;
			m_bDragEx = false;

			CSmartObjectsEditorDialog* pDlg = (CSmartObjectsEditorDialog*) GetParent();
			if (!pDlg->m_pColOrder->IsSorted())
				m_pSourceRow = NULL;
		}
	}
}

void CDragReportCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_bDragging)
		__super::OnLButtonUp(nFlags, point);
	else if (!m_bDragEx)
	{
		m_bDragging = false;
		CSmartObjectsEditorDialog* pDlg = (CSmartObjectsEditorDialog*) GetParent();
		if (m_hItemTarget)
		{
			if (CSOLibrary::StartEditing())
			{
				CXTPReportSelectedRows* pDragRows = GetSelectedRows();

				CString folder = pDlg->GetFolderPath(m_hItemTarget);

				POSITION pos = pDragRows->GetFirstSelectedRowPosition();
				while (pos)
				{
					CXTPReportRow* pRow = pDragRows->GetNextSelectedRow(pos);
					CSmartObjectEntry* pEntry = (CSmartObjectEntry*) pRow->GetRecord();
					pEntry->m_pCondition->sFolder = folder;
				}

				pDlg->m_Tree.SelectItem(m_hItemTarget);
			}
		}
		else if (m_pTargetRow && m_pSourceRow != m_pTargetRow)
		{
			if (pDlg->CheckOutLibrary())
			{
				CSmartObjectEntry* pSource = (CSmartObjectEntry*) m_pSourceRow->GetRecord();
				CSmartObjectEntry* pTarget = (CSmartObjectEntry*) m_pTargetRow->GetRecord();
				m_pSourceRow = m_pTargetRow = NULL;

				pDlg->ModifyRuleOrder(pSource->m_pCondition->iOrder, pTarget->m_pCondition->iOrder);
				pDlg->ReloadEntries(false);
			}
		}
		ReleaseCapture();
	}
}

void CDragReportCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	CSmartObjectsEditorDialog* pDlg = (CSmartObjectsEditorDialog*) GetParent();
	if (!m_bDragging)
	{
		__super::OnMouseMove(nFlags, point);

		if ((m_ptDrag.x || m_ptDrag.y) && (abs(point.x - m_ptDrag.x) > 5 || abs(point.y - m_ptDrag.y) > 5))
		{
			CXTPReportRow* pRow = HitTest(m_ptDrag);
			GetSelectedRows()->Add(pRow);

			SetCapture();
			m_hItemSource = pDlg->m_Tree.GetSelectedItem();
			m_hItemTarget = NULL;
			m_bDragging = true;
			SetCursor(m_hCursorNoDrop);
		}
	}
	else
	{
		CPoint pt = point;
		ClientToScreen(&point);
		pDlg->m_Tree.ScreenToClient(&point);
		m_hItemTarget = pDlg->m_Tree.HitTest(point);
		if (m_hItemSource == m_hItemTarget)
			m_hItemTarget = NULL;
		pDlg->m_Tree.SelectDropTarget(m_hItemTarget);
		if (m_hItemTarget)
			SetCursor(m_hCursorNormal);
		else
			SetCursor(m_hCursorNoDrop);

		if (m_pSourceRow)
		{
			CXTPReportRow* pOverRow = HitTest(pt);

			// erase previous drop marker...
			DrawDropTarget();
			m_pTargetRow = pOverRow;

			CRect rc = GetReportRectangle();
			if (rc.left <= pt.x && pt.x < rc.right)
			{
				if (pt.y < rc.top)
				{
					OnVScroll(SB_LINEUP, 0, NULL);
					UpdateWindow();
				}
				else if (pt.y > rc.bottom)
				{
					OnVScroll(SB_LINEDOWN, 0, NULL);
					UpdateWindow();
				}
			}

			// ...and draw current drop marker
			DrawDropTarget();
		}
	}
}

void CDragReportCtrl::DrawDropTarget()
{
	if (m_pSourceRow && m_pTargetRow && m_pSourceRow != m_pTargetRow)
	{
		int sourceIdx = m_pSourceRow->GetIndex();
		int targetIdx = m_pTargetRow->GetIndex();

		CClientDC dc(this);
		CRect rc = m_pTargetRow->GetRect();
		if (sourceIdx < targetIdx)
			rc.top = rc.bottom - 2;
		else
			rc.top = rc.top - 2;
		rc.bottom = rc.top + 4;

		dc.InvertRect(rc);
	}
}

void CDragReportCtrl::OnCaptureChanged(CWnd* pWnd)
{
	// erase marker
	DrawDropTarget();

	SetCursor(m_hCursorNormal);

	m_bDragging = false;
	m_ptDrag.x = 0;
	m_ptDrag.y = 0;
	m_hItemTarget = NULL;
	__super::OnCaptureChanged(pWnd);

	CSmartObjectsEditorDialog* pDlg = (CSmartObjectsEditorDialog*) GetParent();
	pDlg->m_Tree.SelectDropTarget(NULL);
}

void CDragReportCtrl::OnContextMenu(CWnd* pWnd, CPoint pos)
{
	int selCount = GetSelectedRows()->GetCount();

	if (pos.x == -1 && pos.y == -1)
	{
		pos.x = 7;
		pos.y = GetReportRectangle().top + 1;
		ClientToScreen(&pos);
	}

	CPoint ptClient(pos);
	ScreenToClient(&ptClient);
	CXTPReportRow* pRow = HitTest(ptClient);
	if (pRow || ptClient.y > GetReportRectangle().top)
	{
		CMenu menu;
		VERIFY(menu.CreatePopupMenu());

		menu.AppendMenu(MF_STRING, ID_SOED_NEW_CONDITION, _T("New..."));
		menu.AppendMenu(MF_STRING | (selCount != 1 ? MF_DISABLED : 0), ID_SOED_CLONE_CONDITION, _T("Duplicate..."));
		menu.AppendMenu(MF_STRING | (!selCount ? MF_DISABLED : 0), ID_SOED_DELETE_CONDITION, _T("Delete"));

		//menu.AppendMenu( MF_SEPARATOR );

		/*
		   // create columns items
		   CXTPReportColumns* pColumns = m_View.GetColumns();
		   int nColumnCount = pColumns->GetCount();
		   for ( int i = 0; i < nColumnCount; ++i )
		   {
		   CXTPReportColumn* pCol = pColumns->GetAt(i);
		   CString sCaption = pCol->GetCaption();
		   if ( !sCaption.IsEmpty() )
		   {
		    menu.AppendMenu( MF_STRING, ID_COLUMN_SHOW + i, sCaption );
		    menu.CheckMenuItem( ID_COLUMN_SHOW + i, MF_BYCOMMAND | (pCol->IsVisible() ? MF_CHECKED : MF_UNCHECKED) );
		   }
		   }
		 */

		CSmartObjectsEditorDialog* pDlg = (CSmartObjectsEditorDialog*) GetParent();

		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pos.x, pos.y, this, NULL);

		switch (nMenuResult)
		{
		case ID_SOED_NEW_CONDITION:
			pDlg->OnAddEntry();
			break;
		case ID_SOED_CLONE_CONDITION:
			pDlg->OnDuplicateEntry();
			break;
		case ID_SOED_DELETE_CONDITION:
			pDlg->OnRemoveEntry();
			break;
		}
	}
	else
		__super::OnContextMenu(pWnd, pos);
}

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CSmartObjectsEditorDialog, CXTPFrameWnd)

BEGIN_MESSAGE_MAP(CSmartObjectsEditorDialog, CXTPFrameWnd)
ON_WM_SETFOCUS()
ON_WM_DESTROY()
ON_WM_CLOSE()
//ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()

// XT Commands.
ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)
ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)

// Group by pane commands
//	ON_COMMAND_RANGE( ID_SOED_GROUP_BY_NONE, ID_SOED_GROUP_BY_ACTION, OnGroupBy )

// Modify pane commands
ON_COMMAND(ID_SOED_NEW_CONDITION, OnAddEntry)
ON_COMMAND(ID_SOED_EDIT_CONDITION, OnEditEntry)
ON_COMMAND(ID_SOED_CLONE_CONDITION, OnDuplicateEntry)
ON_COMMAND(ID_SOED_DELETE_CONDITION, OnRemoveEntry)
ON_UPDATE_COMMAND_UI(ID_SOED_CLONE_CONDITION, EnableIfOneSelected)
ON_UPDATE_COMMAND_UI(ID_SOED_DELETE_CONDITION, EnableIfSelected)

ON_COMMAND(ID_SOED_HELPERS_EDIT, OnHelpersEdit)
ON_COMMAND(ID_SOED_HELPERS_NEW, OnHelpersNew)
ON_COMMAND(ID_SOED_HELPERS_DELETE, OnHelpersDelete)
ON_COMMAND(ID_SOED_HELPERS_DONE, OnHelpersDone)

//ON_NOTIFY(NM_CLICK, ID_MOVEMENT_CAR, OnNotifyMovement )
//ON_BN_CLICKED( ID_MOVEMENT_CAR, OnEditMovement )

//	ON_NOTIFY( NM_CLICK, IDC_REPORTCONTROL, OnReportItemClick )
//	ON_NOTIFY( NM_RCLICK, IDC_REPORTCONTROL, OnReportItemRClick )
//	ON_NOTIFY( NM_DBLCLK, IDC_REPORTCONTROL, OnReportItemDblClick )
ON_NOTIFY(XTP_NM_REPORT_HEADER_RCLICK, IDC_REPORTCONTROL, OnReportColumnRClick)
ON_NOTIFY(XTP_NM_REPORT_SELCHANGED, IDC_REPORTCONTROL, OnReportSelChanged)
ON_NOTIFY(XTP_NM_REPORT_HYPERLINK, IDC_REPORTCONTROL, OnReportHyperlink)
ON_NOTIFY(XTP_NM_REPORT_CHECKED, IDC_REPORTCONTROL, OnReportChecked)
ON_NOTIFY(XTP_NM_REPORT_VALUECHANGED, IDC_REPORTCONTROL, OnReportEdit)

// tree view
ON_NOTIFY(TVN_BEGINLABELEDIT, 2, OnTreeBeginEdit)
ON_NOTIFY(TVN_ENDLABELEDIT, 2, OnTreeEndEdit)
ON_NOTIFY(TVN_SELCHANGED, 2, OnTreeSelChanged)
ON_NOTIFY(NM_SETFOCUS, 2, OnTreeSetFocus)

// description edit box
ON_NOTIFY(EN_CHANGE, 3, OnDescriptionEdit)

END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CSmartObjectsEditorViewClass : public IViewPaneClass
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID()	 override { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       override { return "Smart Objects Editor"; };
	virtual const char*    Category()        override { return "Game"; };
	virtual const char*    GetMenuPath()     override { return "Deprecated"; }
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CSmartObjectsEditorDialog); };
	virtual const char*    GetPaneTitle()    override { return _T("Smart Objects Editor"); };
	virtual bool           SinglePane()      override { return true; };
};

REGISTER_CLASS_DESC(CSmartObjectsEditorViewClass)

//////////////////////////////////////////////////////////////////////////
CSmartObjectsEditorDialog::CSmartObjectsEditorDialog()
	: m_bIgnoreNotifications(false)
	, m_pEditedEntry(NULL)
{
	GetIEditor()->RegisterNotifyListener(this);
	++CSOLibrary::m_iNumEditors;
	GetIEditor()->GetObjectManager()->signalObjectChanged.Connect(this, &CSmartObjectsEditorDialog::OnObjectEvent);

	GetIEditor()->GetObjectManager()->signalSelectionChanged.Connect(this, &CSmartObjectsEditorDialog::OnSelectionChanged);
	m_bSinkNeeded = true;

	//! This callback will be called on response to object event.
	typedef Functor2<CBaseObject*, int> EventCallback;

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, SOED_DIALOGFRAME_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		wndcls.style = 0 /*CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW*/;
		wndcls.lpfnWndProc = ::DefWindowProc;
		wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
		wndcls.hInstance = hInst;
		wndcls.hIcon = NULL;
		wndcls.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground = NULL;    // (HBRUSH) (COLOR_3DFACE + 1);
		wndcls.lpszMenuName = NULL;
		wndcls.lpszClassName = SOED_DIALOGFRAME_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
	CRect rc(0, 0, 0, 0);
	BOOL bRes = Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, AfxGetMainWnd());
	if (!bRes)
		return;

	OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
CSmartObjectsEditorDialog::~CSmartObjectsEditorDialog()
{
	if (CSOLibrary::m_bSaveNeeded && SaveSOLibrary())
	{
		CSOLibrary::m_bSaveNeeded = false;
		CSOLibrary::m_bLoadNeeded = false;
	}

	--CSOLibrary::m_iNumEditors;
	GetIEditor()->GetObjectManager()->signalObjectChanged.DisconnectObject(this);
	GetIEditor()->GetObjectManager()->signalSelectionChanged.DisconnectObject(this);
	GetIEditor()->UnregisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
BOOL CSmartObjectsEditorDialog::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd)
{
	return __super::Create(SOED_DIALOGFRAME_CLASSNAME, "", dwStyle, rect, pParentWnd);
}

//////////////////////////////////////////////////////////////////////////
SmartObjectConditions::iterator CSmartObjectsEditorDialog::FindRuleByPtr(const SmartObjectCondition* pRule)
{
	SmartObjectConditions::iterator it, itEnd = CSOLibrary::m_Conditions.end();
	for (it = CSOLibrary::m_Conditions.begin(); it != itEnd; ++it)
		if (&*it == pRule)
			return it;

	// if this ever happen it means that the rule is already deleted!
	assert(!"Rule not found in CSmartObjectsEditorDialog::FindRuleByPtr!");
	return itEnd;
}

//////////////////////////////////////////////////////////////////////////
BOOL CSmartObjectsEditorDialog::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	BOOL res = FALSE;

	res = m_View.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	if (res)
		return res;

	return __super::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

//////////////////////////////////////////////////////////////////////////
BOOL CSmartObjectsEditorDialog::OnInitDialog()
{
	m_bFilterCanceled = false;

	try
	{
		// Initialize the command bars
		if (!InitCommandBars())
			return -1;
	}
	catch (CResourceException* e)
	{
		e->Delete();
		return -1;
	}

	ModifyStyleEx(WS_EX_CLIENTEDGE, 0);

	// Get a pointer to the command bars object.
	//	CXTPCommandBars* pCommandBars = GetCommandBars();
	//	if(pCommandBars == NULL)
	//	{
	//		TRACE0("Failed to create command bars object.\n");
	//		return -1;
	//	}

	// Add the menu bar
	//	CXTPCommandBar* pMenuBar = pCommandBars->SetMenu( _T("Menu Bar"),IDR_VEHICLE_EDITOR );
	//	pMenuBar->SetFlags(xtpFlagStretched);
	//	pMenuBar->EnableCustomization(FALSE);

	// Create standard toolbar.
	//CXTPToolBar *pStdToolBar = pCommandBars->Add( _T("VehicleEditor ToolBar"),xtpBarTop );

	GetDockingPaneManager()->InstallDockingPanes(this);
	GetDockingPaneManager()->SetTheme(xtpPaneThemeOffice2003);
	GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);

	// root variable block
	//	block = new CVarBlock;
	//	block->AddRef();

	// init panes
	CreatePanes();

	// init client pane
	CRect rc;
	GetClientRect(rc);
	m_View.Create(WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, rc, this, IDC_REPORTCONTROL);

	m_pColName = m_View.AddColumn(new CXTPReportColumn(COLUMN_NAME, "Name", 60));
	m_pColDescription = m_View.AddColumn(new CXTPReportColumn(COLUMN_DESCRIPTION, "Description", 160));
	m_pColUserClass = m_View.AddColumn(new CXTPReportColumn(COLUMN_USER_CLASS, "User's Class", 60));
	m_pColUserState = m_View.AddColumn(new CXTPReportColumn(COLUMN_USER_STATE, "User's State Pattern", 60));
	m_pColObjectClass = m_View.AddColumn(new CXTPReportColumn(COLUMN_OBJECT_CLASS, "Object's Class", 60));
	m_pColObjectState = m_View.AddColumn(new CXTPReportColumn(COLUMN_OBJECT_STATE, "Object's State Pattern", 60));
	m_pColAction = m_View.AddColumn(new CXTPReportColumn(COLUMN_ACTION, "Action", 60));
	m_pColOrder = m_View.AddColumn(new CXTPReportColumn(COLUMN_ORDER, "", 0, FALSE, XTP_REPORT_NOICON, TRUE, FALSE));

	m_View.ModifyStyleEx(0, WS_EX_STATICEDGE);

	m_View.AllowEdit(TRUE);

	m_pColName->SetEditable(TRUE);
	m_pColDescription->SetEditable(TRUE);
	m_pColUserClass->SetEditable(FALSE);
	m_pColUserState->SetEditable(FALSE);
	m_pColObjectClass->SetEditable(FALSE);
	m_pColObjectState->SetEditable(FALSE);
	m_pColAction->SetEditable(FALSE);
	m_pColOrder->SetEditable(FALSE);

	m_pColDescription->SetVisible(FALSE);

	//	m_View.GetReportHeader()->ShowItemsInGroups( TRUE );
	//	m_View.GetColumns()->GetGroupsOrder()->Add( m_pColObjectClass );
	//	m_View.GetColumns()->GetGroupsOrder()->Add( m_pColObjectState );

	m_View.GetColumns()->SetSortColumn(m_pColOrder, TRUE);

	m_View.EnablePreviewMode(TRUE);

	class CMyPaintManager : public CXTPReportPaintManager
	{
	public:
		//virtual void DrawGroupRow( CDC* pDC, CXTPReportGroupRow* pRow, CRect rcRow)
		virtual void DrawGroupRow(CDC* pDC, CXTPReportGroupRow* pRow, CRect rcRow, XTP_REPORTRECORDITEM_METRICS* pMetrics)
		{
			HGDIOBJ oldFont = m_fontText.Detach();
			m_fontText.Attach(m_fontBoldText.Detach());
			__super::DrawGroupRow(pDC, pRow, rcRow, pMetrics);
			m_fontBoldText.Attach(m_fontText.Detach());
			m_fontText.Attach(oldFont);

			if (!pRow->GetTreeDepth())
			{
				rcRow.bottom = rcRow.top + 1;
				pDC->FillSolidRect(rcRow, 0);
			}
		}
		virtual void DrawFocusedRow(CDC* pDC, CRect rcRow)
		{
		}
		virtual int GetRowHeight(CDC* pDC, CXTPReportRow* pRow)
		{
			return __super::GetRowHeight(pDC, pRow) + (pRow->IsGroupRow() ? 2 : 0);
		}
	};
	m_View.SetPaintManager(new CMyPaintManager);

	CXTPReportPaintManager* manager = m_View.GetPaintManager();
	//	manager->m_clrGroupRowText = manager->m_clrWindowText;
	manager->m_clrGroupShadeBack.SetStandardValue(0x00ffffc0);
	manager->m_clrHighlight = manager->m_clrBtnFace;
	manager->m_clrHighlightText = manager->m_clrWindowText;
	manager->m_clrIndentControl = manager->m_clrGridLine;
	manager->m_strNoItems = "There are no smart object rules in this folder.";
	manager->m_nTreeIndent = 4;

	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	if (layout.Load(_T("SmartObjectsLayout")) && layout.GetPaneList().GetCount() >= 4)
		GetDockingPaneManager()->SetLayout(&layout);

	//	// Load templates
	//	ReloadTemplates();

	// Load rules and populate ReportControl with actual values
	ReloadEntries(true);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::CreatePanes()
{
	CRect rc;
	GetClientRect(&rc);

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for description
	CXTPDockingPane* pDockPane = GetDockingPaneManager()->CreatePane(IDW_SOED_DESC_PANE, CRect(350, 0, 400, 60), xtpPaneDockBottom);
	pDockPane->SetTitle(_T("Description"));
	pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	/////////////////////////////////////////////////////////////////////////
	// Create Edit Control
	m_Description.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY, rc, this, 3);
	m_Description.ModifyStyleEx(0, WS_EX_STATICEDGE);
	m_Description.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_Description.SetMargins(2, 2);
	m_Description.SetBkColor(GetSysColor(COLOR_WINDOW));
	m_Description.SetTextColor(GetSysColor(COLOR_WINDOWTEXT));

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for Properties
	pDockPane = GetDockingPaneManager()->CreatePane(IDW_SOED_PROPS_PANE, CRect(0, 0, 300, 240), xtpPaneDockRight);
	pDockPane->SetTitle(_T("Properties"));
	pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	//////////////////////////////////////////////////////////////////////////
	// Create properties control.
	//////////////////////////////////////////////////////////////////////////
	m_Properties.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, this, 1);
	m_Properties.ModifyStyleEx(0, WS_EX_STATICEDGE);

	//m_Properties.SetFlags( CPropertyCtrl::F_VS_DOT_NET_STYLE );
	m_Properties.SetItemHeight(16);
	m_Properties.ExpandAll();

	m_vars = gSmartObjectsUI.CreateVars();
	m_Properties.AddVarBlock(m_vars);
	m_Properties.ExpandAll();
	m_Properties.EnableWindow(FALSE);

	m_Properties.SetUpdateCallback(functor(*this, &CSmartObjectsEditorDialog::OnUpdateProperties));
	m_Properties.EnableUpdateCallback(true);

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for TaskPanel
	CXTPDockingPane* pTaskPane = GetDockingPaneManager()->CreatePane(IDW_SOED_TASK_PANE, CRect(0, 0, 176, 240), xtpPaneDockLeft);
	pTaskPane->SetTitle(_T("Tasks"));
	pTaskPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	/////////////////////////////////////////////////////////////////////////
	// Create empty Task Panel
	m_taskPanel.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, this, 0);
	m_taskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	m_taskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	//	m_taskPanel.SetSelectItemOnFocus( TRUE );
	m_taskPanel.AllowDrag(FALSE);
	//m_taskPanel.SetAnimation( xtpTaskPanelAnimationNo );
	m_taskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
	m_taskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(4, 4, 4, 4);
	m_taskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(2, 2, 2, 2);
	m_taskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(1, 1, 1, 1);
	m_taskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(0, 2, 0, 2);
	m_taskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2, 2, 2, 2);
	m_taskPanel.GetPaintManager()->m_nGroupSpacing = 0;

	CXTPTaskPanelGroupItem* pItem = NULL;
	CXTPTaskPanelGroup* pGroup = NULL;
	int groupId = 0;

	pGroup = m_taskPanel.AddGroup(++groupId);
	pGroup->SetCaption(_T("Rules"));
	pItem = pGroup->AddLinkItem(ID_SOED_NEW_CONDITION);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption(_T("New..."));
	pItem->SetTooltip(_T("Adds a new entry"));
	//	pItem = pGroup->AddLinkItem( ID_SOED_EDIT_CONDITION ); pItem->SetType( xtpTaskItemTypeLink );
	//	pItem->SetCaption( _T("Edit...") ); pItem->SetTooltip( _T("Edits selected entry") );
	pItem = pGroup->AddLinkItem(ID_SOED_CLONE_CONDITION);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption(_T("Duplicate..."));
	pItem->SetTooltip(_T("Duplicates selected entry"));
	pItem = pGroup->AddLinkItem(ID_SOED_DELETE_CONDITION);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption(_T("Delete"));
	pItem->SetTooltip(_T("Removes selected entries"));

	pGroup = m_taskPanel.AddGroup(++groupId);
	pGroup->SetCaption(_T("Helpers"));
	m_pItemHelpersEdit = pGroup->AddLinkItem(ID_SOED_HELPERS_EDIT);
	m_pItemHelpersEdit->SetType(xtpTaskItemTypeLink);
	m_pItemHelpersEdit->SetCaption(_T("Edit..."));
	m_pItemHelpersEdit->SetTooltip(_T("Displays Smart Object helpers for editing"));
	m_pItemHelpersNew = pGroup->AddLinkItem(ID_SOED_HELPERS_NEW);
	m_pItemHelpersNew->SetType(xtpTaskItemTypeLink);
	m_pItemHelpersNew->SetCaption(_T("New..."));
	m_pItemHelpersNew->SetTooltip(_T("Adds a new Smart Object helper"));
	m_pItemHelpersNew->SetEnabled(FALSE);
	m_pItemHelpersDelete = pGroup->AddLinkItem(ID_SOED_HELPERS_DELETE);
	m_pItemHelpersDelete->SetType(xtpTaskItemTypeLink);
	m_pItemHelpersDelete->SetCaption(_T("Delete..."));
	m_pItemHelpersDelete->SetTooltip(_T("Removes selected Smart Object helper(s)"));
	m_pItemHelpersDelete->SetEnabled(FALSE);
	m_pItemHelpersDone = pGroup->AddLinkItem(ID_SOED_HELPERS_DONE);
	m_pItemHelpersDone->SetType(xtpTaskItemTypeLink);
	m_pItemHelpersDone->SetCaption(_T("Done"));
	m_pItemHelpersDone->SetTooltip(_T("Ends helpers edit mode"));
	m_pItemHelpersDone->SetEnabled(FALSE);

	/*
	   pGroup = m_taskPanel.AddGroup( ++groupId );
	   pGroup->SetCaption( _T("Group by") );
	   pItem = pGroup->AddLinkItem( ID_SOED_GROUP_BY_NONE ); pItem->SetType( xtpTaskItemTypeLink );
	   pItem->SetCaption( _T("None") ); pItem->SetTooltip( _T("Shows entries without grouping") );
	   pItem->SetBold( TRUE );
	   pItem = pGroup->AddLinkItem( ID_SOED_GROUP_BY_ACTION ); pItem->SetType( xtpTaskItemTypeLink );
	   pItem->SetCaption( _T("Action") ); pItem->SetTooltip( _T("Groups the entries by Action") );
	   pItem = pGroup->AddLinkItem( ID_SOED_GROUP_BY_USER_CLASS ); pItem->SetType(xtpTaskItemTypeLink);
	   pItem->SetCaption( _T("User's Class") ); pItem->SetTooltip( _T("Groups the entries by Smart Object's user class") );
	   pItem = pGroup->AddLinkItem( ID_SOED_GROUP_BY_USER_STATE ); pItem->SetType(xtpTaskItemTypeLink);
	   pItem->SetCaption( _T("User's State Pattern") ); pItem->SetTooltip( _T("Groups the entries by Smart Object's user state") );
	   pItem = pGroup->AddLinkItem( ID_SOED_GROUP_BY_OBJECT_CLASS ); pItem->SetType( xtpTaskItemTypeLink );
	   pItem->SetCaption( _T("Object's Class") ); pItem->SetTooltip( _T("Groups the entries by Smart Object class") );
	   pItem = pGroup->AddLinkItem( ID_SOED_GROUP_BY_OBJECT_STATE ); pItem->SetType(xtpTaskItemTypeLink);
	   pItem->SetCaption( _T("Object's State Pattern") ); pItem->SetTooltip( _T("Groups the entries by Smart Object state") );
	 */

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for Tree Control
	pDockPane = GetDockingPaneManager()->CreatePane(IDW_SOED_TREE_PANE, CRect(176, 0, 300, 240), xtpPaneDockTop, pTaskPane);
	pDockPane->SetTitle(_T("Rules"));
	pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	/////////////////////////////////////////////////////////////////////////
	// Create Tree Control
	m_Tree.CreateEx(WS_EX_STATICEDGE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
	                | TVS_EDITLABELS | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_INFOTIP | TVS_SHOWSELALWAYS, rc, this, 2);
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_HYPERGRAPH_TREE, 16, RGB(255, 0, 255));
	m_Tree.SetImageList(&m_imageList, TVSIL_NORMAL);
}

//////////////////////////////////////////////////////////////////////////
CString CSmartObjectsEditorDialog::GetFolderPath(HTREEITEM item) const
{
	CString folder;
	while (item)
	{
		HTREEITEM parent = m_Tree.GetParentItem(item);
		if (parent)
			folder = _T('/') + m_Tree.GetItemText(item) + folder;
		item = parent;
	}
	return folder;
}

//////////////////////////////////////////////////////////////////////////
CString CSmartObjectsEditorDialog::GetCurrentFolderPath() const
{
	return GetFolderPath(m_Tree.GetSelectedItem());
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CSmartObjectsEditorDialog::ForceFolder(const CString& folder)
{
	HTREEITEM root = m_Tree.GetRootItem();
	HTREEITEM item = root;
	int token = 0;
	CString name = folder.Tokenize(_T("/\\"), token);
	while (!name.IsEmpty())
	{
		HTREEITEM child = m_Tree.GetChildItem(item);
		while (child && m_Tree.GetItemText(child).CompareNoCase(name) != 0)
			child = m_Tree.GetNextSiblingItem(child);
		if (child)
		{
			name = m_Tree.GetItemText(child);
			item = child;
		}
		else
			item = m_Tree.InsertItem(name, 2, 2, item, TVI_SORT);
		name = folder.Tokenize(_T("/\\"), token);
	}
	return item;
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CSmartObjectsEditorDialog::SetCurrentFolder(const CString& folder)
{
	HTREEITEM item = ForceFolder(folder);
	HTREEITEM i = item;
	while (i = m_Tree.GetParentItem(i))
		m_Tree.Expand(i, TVE_EXPAND);
	m_Tree.SelectItem(item);
	return item;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::ReloadEntries(bool bFromFile)
{
	// remember the selected rule
	SmartObjectCondition editedCondition;
	if (m_pEditedEntry)
		editedCondition = *m_pEditedEntry->m_pCondition;

	// remember the selected folder
	CString folder = GetCurrentFolderPath();

	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();
	HTREEITEM root = m_Tree.InsertItem(_T("/"), 2, 2);

	m_View.BeginUpdate();
	m_View.GetRecords()->RemoveAll();

	if (GetISystem()->GetIFlowSystem())
	{
		if (bFromFile)
		{
			// load the rules from SmartObjects.xml
			if (!CSOLibrary::Load())
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CSmartObjectsEditorDialog::ReloadEntries() failed to load from %s", SMART_OBJECTS_XML);
				return;
			}
		}

		SmartObjectConditions::iterator it, itEnd = CSOLibrary::m_Conditions.end();
		for (it = CSOLibrary::m_Conditions.begin(); it != itEnd; ++it)
		{
			// make sure the folder exists in the tree view
			ForceFolder(it->sFolder.c_str());

			CXTPReportRecord* pRecord = m_View.AddRecord(new CSmartObjectEntry(&*it));
			if (folder.CompareNoCase(it->sFolder.c_str()) != 0)
				pRecord->SetVisible(FALSE);
		}
	}
	m_Tree.Expand(root, TVE_EXPAND);
	SetCurrentFolder(folder);

	m_View.EndUpdate();
	m_Tree.SetRedraw(TRUE);
	m_Tree.RedrawWindow(0, 0, RDW_INVALIDATE);

	m_View.Populate();

	if (m_pEditedEntry)
	{
		m_pEditedEntry = NULL;
		CXTPReportRows* pRows = m_View.GetRows();
		int i = pRows->GetCount();
		while (i--)
		{
			CXTPReportRow* pRow = pRows->GetAt(i);
			CSmartObjectEntry* pEntry = (CSmartObjectEntry*) pRow->GetRecord();
			if (*pEntry->m_pCondition == editedCondition)
			{
				m_View.EnsureVisible(pRow);
				//				m_View.GetSelectedRows()->Add( pRow );
				m_View.SetFocusedRow(pRow);
				m_pEditedEntry = pEntry;
				break;
			}
		}
	}

	m_bSinkNeeded = true;
}

//////////////////////////////////////////////////////////////////////////
bool CSmartObjectsEditorDialog::ChangeTemplate(SmartObjectCondition* pRule, const CSOParamBase* pParam) const
{
	CString msg;
	SetTemplateDefaults(*pRule, pParam, &msg);
	if (msg.IsEmpty())
	{
		SetTemplateDefaults(*pRule, pParam);
		return true;
	}

	msg.Insert(0, "WARNING!\n\nThe change of the template will implicitly alter the following field(s):\n");
	msg += "\nDo you really want to change the template?";

	if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(msg)))
	{
		return false;
	}

	SetTemplateDefaults(*pRule, pParam);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnUpdateProperties(IVariable* pVar)
{
	if (m_pEditedEntry)
	{
		if (!CSOLibrary::StartEditing())
		{
			// Restore variables
			static NMHDR nmhdr;
			nmhdr.idFrom = IDC_REPORTCONTROL;
			nmhdr.hwndFrom = m_View.m_hWnd;
			nmhdr.code = XTP_NM_REPORT_SELCHANGED;
			PostMessage(WM_NOTIFY, IDC_REPORTCONTROL, (LPARAM) &nmhdr);
			return;
		}

		if (pVar->GetDataType() == IVariable::DT_SOTEMPLATE)
		{
			// Template changed -> special handling here...
			string name;
			pVar->Get(name);
			const MapTemplates& mapTemplates = GetIEditor()->GetAIManager()->GetMapTemplates();
			MapTemplates::const_iterator it, itEnd = mapTemplates.end();
			for (it = mapTemplates.begin(); it != itEnd; ++it)
				if (it->second->name == name)
					break;

			// is it a valid template name?
			if (it == itEnd)
			{
				CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Specified template is not defined!"));

				it = mapTemplates.find(m_pEditedEntry->m_pCondition->iTemplateId);
				if (it != itEnd)
				{
					m_Properties.EnableUpdateCallback(false);
					pVar->Set(it->second->name);
					m_Properties.EnableUpdateCallback(true);
				}
				return;
			}

			// does it differs from current?
			if (m_pEditedEntry->m_pCondition->iTemplateId != it->first)
			{
				// try to set default template values
				if (ChangeTemplate(m_pEditedEntry->m_pCondition, it->second->params))
				{
					m_pEditedEntry->m_pCondition->iTemplateId = it->first;

					// Update variables.
					//m_Properties.EnableUpdateCallback( false );
					//gSmartObjectsUI.SetConditionToUI( *m_pEditedEntry->m_pCondition, &m_Properties );
					//m_Properties.EnableUpdateCallback( true );
				}

				static NMHDR nmhdr;
				nmhdr.hwndFrom = m_View;
				nmhdr.idFrom = IDC_REPORTCONTROL;
				nmhdr.code = XTP_NM_REPORT_SELCHANGED;
				PostMessage(WM_NOTIFY, (WPARAM) (int) IDC_REPORTCONTROL, (LPARAM) &nmhdr);
			}
			return;
		}

		// Update variables.
		//m_Properties.EnableUpdateCallback( false );
		if (gSmartObjectsUI.GetConditionFromUI(*m_pEditedEntry->m_pCondition))
		{
			// folder property was edited
			m_bSinkNeeded = true;

			// make sure the folder exists in the tree view
			HTREEITEM item = SetCurrentFolder(m_pEditedEntry->m_pCondition->sFolder.c_str());
			m_pEditedEntry->m_pCondition->sFolder = GetFolderPath(item);
			/*
			   HTREEITEM root = m_Tree.GetRootItem();
			   CString folder = m_pEditedEntry->m_pCondition->sFolder.c_str();
			   CString realFolder;
			   HTREEITEM item = root;
			   int token = 0;
			   CString name = folder.Tokenize( _T("/\\"), token );
			   while ( !name.IsEmpty() )
			   {
			   m_Tree.Expand( item, TVE_EXPAND );

			   HTREEITEM child = m_Tree.GetChildItem( item );
			   while ( child && m_Tree.GetItemText( child ).CompareNoCase( name ) != 0 )
			    child = m_Tree.GetNextSiblingItem( child );
			   if ( child )
			   {
			    name = m_Tree.GetItemText( child );
			    item = child;
			   }
			   else
			    item = m_Tree.InsertItem( name, 2, 2, item, TVI_SORT );
			   realFolder += _T('/');
			   realFolder += name;
			   name = folder.Tokenize( _T("/\\"), token );
			   }
			   m_pEditedEntry->m_pCondition->sFolder = realFolder;
			   m_Tree.SelectItem( item );
			 */
		}
		//m_Properties.EnableUpdateCallback( true );

		if (pVar->GetDataType() == IVariable::DT_SOCLASS ||
		    //			pVar->GetDataType() == IVariable::DT_SOSTATE ||
		    pVar->GetDataType() == IVariable::DT_SOSTATES ||
		    pVar->GetDataType() == IVariable::DT_SOSTATEPATTERN ||
		    pVar->GetDataType() == IVariable::DT_SOACTION ||
		    pVar == &gSmartObjectsUI.sName ||
		    pVar == &gSmartObjectsUI.sDescription ||
		    pVar == &gSmartObjectsUI.bEnabled)
		//			pVar->GetDataType() == IVariable::DT_OBJECT )
		{
			m_pEditedEntry->Update();
			m_Description.SetWindowText((const CString&) gSmartObjectsUI.sDescription);
			m_View.Populate();
		}

		struct Local
		{
			static void CopyClassToHelper(const CString& sClass, CVariable<CString>& var)
			{
				CString sHelper;
				if (!sClass.IsEmpty())
				{
					var.Get(sHelper);
					int f = sHelper.ReverseFind(':');
					if (f >= 0)
						sHelper.Delete(0, f + 1);
					sHelper = sClass + ':' + sHelper;
				}
				var.Set(sHelper);
			}
		};

		// if class was changed update helper value
		//m_Properties.EnableUpdateCallback( false );
		if (pVar->GetDataType() == IVariable::DT_SOCLASS)
		{
			if (pVar == &gSmartObjectsUI.userClass)
			{
				CString sHelper, sClass;
				pVar->Get(sClass);
				Local::CopyClassToHelper(sClass, gSmartObjectsUI.userHelper);
			}
			else if (pVar == &gSmartObjectsUI.objectClass)
			{
				CString sHelper, sClass;
				pVar->Get(sClass);
				Local::CopyClassToHelper(sClass, gSmartObjectsUI.objectHelper);
				Local::CopyClassToHelper(sClass, gSmartObjectsUI.entranceHelper);
				Local::CopyClassToHelper(sClass, gSmartObjectsUI.exitHelper);
				Local::CopyClassToHelper(sClass, gSmartObjectsUI.animationHelper);
				Local::CopyClassToHelper(sClass, gSmartObjectsUI.approachHelper);
			}
		}
		//m_Properties.EnableUpdateCallback( true );

		if (pVar == &gSmartObjectsUI.bNavigationRule)
		{
			UpdatePropertyTables();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Called by the editor to notify the listener about the specified event.
void CSmartObjectsEditorDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnIdleUpdate)      // Sent every frame while editor is idle.
	{
		if (m_bSinkNeeded)
			SinkSelection();
		return;
	}

	switch (event)
	{
	case eNotify_OnBeginGameMode:           // Sent when editor goes to game mode.
	case eNotify_OnBeginSimulationMode:     // Sent when simulation mode is started.
	case eNotify_OnBeginSceneSave:          // Sent when document is about to be saved.
	case eNotify_OnQuit:                    // Sent before editor quits.
	case eNotify_OnBeginNewScene:           // Sent when the document is begin to be cleared.
	case eNotify_OnBeginSceneOpen:          // Sent when document is about to be opened.
	case eNotify_OnClearLevelContents:      // Send when the document is about to close.
		OnHelpersDone();
		if (CSOLibrary::m_bSaveNeeded && SaveSOLibrary())
		{
			CSOLibrary::m_bSaveNeeded = false;
			CSOLibrary::m_bLoadNeeded = false;
		}
		break;

	case eNotify_OnInit:                    // Sent after editor fully initialized.
	case eNotify_OnEndSceneOpen:            // Sent after document have been opened.
		//		ReloadEntries();
		break;

	case eNotify_OnEndSceneSave:            // Sent after document have been saved.
	case eNotify_OnEndNewScene:             // Sent after the document have been cleared.
	case eNotify_OnMissionChange:           // Send when the current mission changes.
		break;

	//////////////////////////////////////////////////////////////////////////
	// Editing events.
	//////////////////////////////////////////////////////////////////////////
	case eNotify_OnEditModeChange:          // Sent when editing mode change (move,rotate,scale,....)
	case eNotify_OnEditToolBeginChange:     // Sent when edit tool is about to be changed (ObjectMode,TerrainModify,....)
	case eNotify_OnEditToolEndChange:       // Sent when edit tool has been changed (ObjectMode,TerrainModify,....)
		break;

	// Game related events.
	case eNotify_OnEndGameMode:             // Send when editor goes out of game mode.
		break;

	// UI events.
	case eNotify_OnUpdateViewports:         // Sent when editor needs to update data in the viewports.
	case eNotify_OnInvalidateControls:      // Sent when editor needs to update some of the data that can be cached by controls like combo boxes.
		break;

	// Object events.
	case eNotify_OnSelectionChange:         // Sent when object selection change.
		// Unfortunately I have never received this notification!!!
		// SinkSelection();
		break;
	case eNotify_OnPlaySequence:            // Sent when editor start playing animation sequence.
	case eNotify_OnStopSequence:            // Sent when editor stop playing animation sequence.
		break;
	}
}

void CSOLibrary::String2Classes(const string& sClass, SetStrings& classes)
{
	Load();

	int start = 0, end, length = sClass.length();
	string single;

	while (start < length)
	{
		start = sClass.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz", start);
		if (start < 0)
			break;

		end = sClass.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz", start);
		if (end == -1)
			end = length;

		single = sClass.substr(start, end - start);
		classes.insert(single);

		start = end + 1;
	}
	;
}

void CSmartObjectsEditorDialog::ParseClassesFromProperties(CBaseObject* pObject, SetStrings& classes)
{
	if ((pObject->GetType() & OBJTYPE_ENTITY) ||
	    (pObject->GetType() & OBJTYPE_AIPOINT) && (!StrCmp(pObject->GetTypeName(), "AIAnchor") || !StrCmp(pObject->GetTypeName(), "SmartObject")))
	{
		CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
		CEntityPrototype* pPrototype = pEntity->GetPrototype();
		CVarBlock* pVars = pPrototype ? pPrototype->GetProperties() : pEntity->GetProperties();
		if (pVars)
		{
			IVariablePtr pVariable = pVars->FindVariable("soclasses_SmartObjectClass", false);
			if (pVariable)
			{
				CString value;
				pVariable->Get(value);
				if (!value.IsEmpty())
					CSOLibrary::String2Classes((const char*) value, classes);
			}
		}

		pVars = pEntity->GetProperties2();
		if (pVars)
		{
			IVariablePtr pVariable = pVars->FindVariable("soclasses_SmartObjectClass", false);
			if (pVariable)
			{
				CString value;
				pVariable->Get(value);
				if (!value.IsEmpty())
					CSOLibrary::String2Classes((const char*) value, classes);
			}
		}
	}
}

void CSmartObjectsEditorDialog::SinkSelection()
{
	m_bSinkNeeded = false;

	// get selected folder
	CString folder = GetCurrentFolderPath();
	/*
	   HTREEITEM item = m_Tree.GetSelectedItem();
	   while ( item )
	   {
	   if ( m_Tree.GetParentItem( item ) )
	    folder = _T('/') + m_Tree.GetItemText( item ) + folder;
	   item = m_Tree.GetParentItem( item );
	   }
	 */

	m_sNewObjectClass.Empty();

	m_sFilterClasses.Empty();
	m_sFirstFilterClass.Empty();
	SetStrings filterClasses;

	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	int selCount = pSelection->GetCount();

	// Block UI updates
	m_View.BeginUpdate();

	m_pItemHelpersDelete->SetEnabled(!m_sEditedClass.IsEmpty() && selCount == 1 && pSelection->GetObject(0)->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)));

	// Update helpers
	m_bIgnoreNotifications = true;
	if (m_sEditedClass.IsEmpty())
	{
		MapHelperObjects::iterator it, itEnd = m_mapHelperObjects.end();
		for (it = m_mapHelperObjects.begin(); it != itEnd; ++it)
		{
			CSmartObjectHelperObject* pHelperObject = it->second;
			pHelperObject->RemoveEventListener(functor(*this, &CSmartObjectsEditorDialog::OnObjectEventLegacy));
			pHelperObject->DetachThis();
			pHelperObject->Release();
			GetIEditor()->GetObjectManager()->DeleteObject(pHelperObject);
		}
		m_mapHelperObjects.clear();
	}
	else
	{
		typedef std::set<CEntityObject*> SetEntities;
		SetEntities setEntities;

		int i = selCount;

		// build a list of selected entities which smart object class matches 'm_sEditedClass'
		while (i--)
		{
			CBaseObject* pSelected = pSelection->GetObject(i);
			if (pSelected->GetType() & OBJTYPE_OTHER)
			{
				if (pSelected->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)))
					pSelected = pSelected->GetParent();
			}

			SetStrings classes;
			ParseClassesFromProperties(pSelected, classes);
			if (classes.find((const char*) m_sEditedClass) != classes.end())
				setEntities.insert(static_cast<CEntityObject*>(pSelected));
		}

		// remove not needed helpers
		MapHelperObjects::iterator it, next, itEnd = m_mapHelperObjects.end();
		for (it = m_mapHelperObjects.begin(); it != itEnd; ++it)
		{
			CEntityObject* const pEntity = it->first;
			CSmartObjectHelperObject* pHelperObject = it->second;

			if (setEntities.find(pEntity) == setEntities.end())
			{
				// remove all helpers for this entity
				it->second = NULL;
				pHelperObject->RemoveEventListener(functor(*this, &CSmartObjectsEditorDialog::OnObjectEventLegacy));
				pHelperObject->DetachThis();
				pHelperObject->Release();
				GetIEditor()->GetObjectManager()->DeleteObject(pHelperObject);
			}
			else
			{
				// remove this helper if it isn't in the list of helpers for this class
				if (CSOLibrary::FindHelper(m_sEditedClass, pHelperObject->GetName().GetString()) == CSOLibrary::m_vHelpers.end())
				{
					it->second = NULL;
					pHelperObject->RemoveEventListener(functor(*this, &CSmartObjectsEditorDialog::OnObjectEventLegacy));
					pHelperObject->DetachThis();
					pHelperObject->Release();
					GetIEditor()->GetObjectManager()->DeleteObject(pHelperObject);
				}
			}
		}

		// remove NULL helpers
		it = m_mapHelperObjects.begin();
		while (it != itEnd)
		{
			next = it;
			next++;
			if (!it->second)
				m_mapHelperObjects.erase(it);
			it = next;
		}

		// for each helper of edited class...
		CSOLibrary::VectorHelperData::iterator itHelpers, itHelpersEnd = CSOLibrary::HelpersUpperBound(m_sEditedClass);
		for (itHelpers = CSOLibrary::HelpersLowerBound(m_sEditedClass); itHelpers != itHelpersEnd; ++itHelpers)
		{
			const CSOLibrary::CHelperData& helper = *itHelpers;

			// ... and for each entity
			SetEntities::iterator it, itEnd = setEntities.end();
			for (it = setEntities.begin(); it != itEnd; ++it)
			{
				CEntityObject* pEntity = *it;

				// create/update child helper object
				CSmartObjectHelperObject* pHelperObject = NULL;
				int i = pEntity->GetChildCount();
				while (i--)
				{
					CBaseObject* pChild = pEntity->GetChild(i);
					if ((pChild->GetType() & OBJTYPE_OTHER) != 0 && pChild->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)))
					{
						pHelperObject = static_cast<CSmartObjectHelperObject*>(pChild);
						if (helper.name == pHelperObject->GetName().GetString())
							break;
						else
							pHelperObject = NULL;
					}
				}

				if (!pHelperObject)
				{
					pHelperObject = static_cast<CSmartObjectHelperObject*>(GetIEditor()->GetObjectManager()->NewObject("SmartObjectHelper"));
					if (pHelperObject)
					{
						pHelperObject->AddRef();
						pEntity->AttachChild(pHelperObject);
						m_mapHelperObjects.insert(std::make_pair(pEntity, pHelperObject));
						pHelperObject->AddEventListener(functor(*this, &CSmartObjectsEditorDialog::OnObjectEventLegacy));
					}
				}

				if (pHelperObject)
				{
					pHelperObject->SetSmartObjectEntity(pEntity);
					GetIEditor()->GetObjectManager()->ChangeObjectName(pHelperObject, helper.name.GetString());
					//Vec3 pos = pHelper->qt.t
					//Vec3 pos = pEntity->GetWorldTM().GetInvertedFast().TransformPoint( GetWorldPos() );
					//pVar->Set( pos );
					pHelperObject->SetPos(helper.qt.t, eObjectUpdateFlags_PositionChanged);
					pHelperObject->SetRotation(helper.qt.q, eObjectUpdateFlags_RotationChanged);
				}
			}
		}
	}
	m_bIgnoreNotifications = false;

	// Get the selection once again since it might be changed
	pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	selCount = pSelection->GetCount();

	m_pItemHelpersDelete->SetEnabled(!m_sEditedClass.IsEmpty() && selCount == 1 && pSelection->GetObject(0)->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)));

	// make a list of all selected SmartObjectClass properties
	int j = selCount;
	while (j--)
	{
		CBaseObject* pObject = pSelection->GetObject(j);

		SetStrings classes;
		ParseClassesFromProperties(pObject, classes);
		if (classes.empty())
		{
			filterClasses.clear();
			break;
		}
		if (!m_bFilterCanceled)
			filterClasses.insert(classes.begin(), classes.end());
	}

	// Update report table
	CXTPReportRecords* pRecords = m_View.GetRecords();
	int i = pRecords->GetCount();
	if (filterClasses.size())
	{
		// there are selected object(s) - filter records

		while (i--)
		{
			CSmartObjectEntry* pRecord = static_cast<CSmartObjectEntry*>(pRecords->GetAt(i));

			if (folder.CompareNoCase(pRecord->m_pCondition->sFolder.c_str()) != 0)
			{
				pRecord->SetVisible(FALSE);
				continue;
			}

			string& sObjectClass = pRecord->m_pCondition->sObjectClass;
			string& sUserClass = pRecord->m_pCondition->sUserClass;

			// check if it matches any of filtered classes
			bool bShow = filterClasses.find(sUserClass) != filterClasses.end() ||
			             !sObjectClass.empty() && filterClasses.find(sObjectClass) != filterClasses.end();

			pRecord->SetVisible(bShow);
		}
	}
	else
	{
		// no selected objects - simply show all records
		while (i--)
		{
			CSmartObjectEntry* pRecord = static_cast<CSmartObjectEntry*>(pRecords->GetAt(i));
			pRecord->SetVisible(folder.CompareNoCase(pRecord->m_pCondition->sFolder.c_str()) == 0);
		}
	}
	m_View.Populate();

	SetStrings::iterator it, itEnd = filterClasses.end();
	for (it = filterClasses.begin(); it != itEnd; ++it)
	{
		if (m_sFilterClasses.IsEmpty())
		{
			m_sFilterClasses += *it;
			m_sFirstFilterClass = *it;
		}
		else
			m_sFilterClasses += ',' + *it;
	}

	CRect rcClient = GetDockingPaneManager()->GetClientPane()->GetPaneWindowRect();
	ScreenToClient(rcClient);
	if (filterClasses.size())
	{
		rcClient.top = min(rcClient.top + 23, rcClient.bottom);
		Invalidate();
	}
	m_View.MoveWindow(rcClient);

	// Enable UI updates
	m_View.EndUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::DeleteHelper(const CString& className, const CString& helperName)
{
	m_bSinkNeeded = true;
	CSOLibrary::m_bSaveNeeded = true;

	CSOLibrary::VectorHelperData::iterator it = CSOLibrary::FindHelper(className, helperName);
	if (it != CSOLibrary::m_vHelpers.end())
		CSOLibrary::m_vHelpers.erase(it);
}

//////////////////////////////////////////////////////////////////////////
CSOLibrary::VectorHelperData::iterator CSOLibrary::FindHelper(const CString& className, const CString& helperName)
{
	Load();

	VectorHelperData::iterator it, itEnd = m_vHelpers.end();
	for (it = m_vHelpers.begin(); it != itEnd; ++it)
		if (it->className == className && it->name == helperName)
			return it;
	return itEnd;
}

//////////////////////////////////////////////////////////////////////////
CSOLibrary::VectorHelperData::iterator CSOLibrary::HelpersLowerBound(const CString& className)
{
	Load();

	VectorHelperData::iterator it, itEnd = m_vHelpers.end();
	for (it = m_vHelpers.begin(); it != itEnd; ++it)
		if (it->className >= className)
			return it;
	return itEnd;
}

//////////////////////////////////////////////////////////////////////////
CSOLibrary::VectorHelperData::iterator CSOLibrary::HelpersUpperBound(const CString& className)
{
	Load();

	VectorHelperData::iterator it, itEnd = m_vHelpers.end();
	for (it = m_vHelpers.begin(); it != itEnd; ++it)
		if (it->className > className)
			return it;
	return itEnd;
}

//////////////////////////////////////////////////////////////////////////
CSOLibrary::VectorEventData::iterator CSOLibrary::FindEvent(const char* name)
{
	Load();

	VectorEventData::iterator it, itEnd = m_vEvents.end();
	for (it = m_vEvents.begin(); it != itEnd; ++it)
		if (it->name == name)
			return it;
	return itEnd;
}

//////////////////////////////////////////////////////////////////////////
void CSOLibrary::AddEvent(const char* name, const char* description)
{
	Load();

	m_bSaveNeeded = true;
	VectorEventData::iterator it, itEnd = m_vEvents.end();
	for (it = m_vEvents.begin(); it != itEnd; ++it)
		if (it->name > name)
			break;

	CEventData event;
	event.name = name;
	event.description = description;
	m_vEvents.insert(it, event);
}

//////////////////////////////////////////////////////////////////////////
CSOLibrary::VectorStateData::iterator CSOLibrary::FindState(const char* name)
{
	Load();

	CStateData stateData;
	stateData.name = name;

	VectorStateData::iterator it = std::lower_bound(m_vStates.begin(), m_vStates.end(), stateData, less_name<CStateData>());
	if (it != m_vStates.end() && it->name == name)
		return it;
	else
		return m_vStates.end();
}

//////////////////////////////////////////////////////////////////////////
void CSOLibrary::AddState(const char* name, const char* description, const char* location)
{
	Load();

	m_bSaveNeeded = true;
	CStateData stateData = { name, description, location };
	VectorStateData::iterator it = std::lower_bound(m_vStates.begin(), m_vStates.end(), stateData, less_name<CStateData>());
	if (it != m_vStates.end() && it->name == name)
	{
		it->description = description;
		it->location = location;
	}
	else
		m_vStates.insert(it, stateData);
}

//////////////////////////////////////////////////////////////////////////
CSOLibrary::VectorClassData::iterator CSOLibrary::FindClass(const char* name)
{
	Load();

	CClassData classData;
	classData.name = name;

	VectorClassData::iterator it = std::lower_bound(m_vClasses.begin(), m_vClasses.end(), classData, less_name<CClassData>());
	if (it != m_vClasses.end() && it->name == name)
		return it;
	else
		return m_vClasses.end();
}

//////////////////////////////////////////////////////////////////////////
void CSOLibrary::AddClass(const char* name, const char* description, const char* location, const char* templateName)
{
	Load();

	m_bSaveNeeded = true;
	CClassTemplateData const* pTemplate = FindClassTemplate(templateName);
	CClassData classData = { name, description, location, templateName, pTemplate };
	VectorClassData::iterator it = std::lower_bound(m_vClasses.begin(), m_vClasses.end(), classData, less_name<CClassData>());
	if (it != m_vClasses.end() && it->name == name)
	{
		it->description = description;
		it->location = location;
		it->templateName = templateName;
		it->pClassTemplateData = pTemplate;
	}
	else
		m_vClasses.insert(it, classData);
}

//////////////////////////////////////////////////////////////////////////
CSOLibrary::CClassTemplateData const* CSOLibrary::FindClassTemplate(const char* name)
{
	if (!name || !*name)
		return NULL;

	Load();

	CClassTemplateData classTemplateData;
	classTemplateData.name = name;

	VectorClassTemplateData::iterator it = std::lower_bound(m_vClassTemplates.begin(), m_vClassTemplates.end(), classTemplateData, less_name_no_case<CClassTemplateData>());
	if (it != m_vClassTemplates.end() && stricmp(it->name, name) == 0)
	{
		it->name = name;
		return &*it;
	}
	else
		return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnObjectEvent(CObjectEvent& event)
{
	if (!m_bIgnoreNotifications)
	{
		switch (event.m_type)
		{
			case OBJECT_ON_DELETE:     // Sent after object was deleted from object manager.
				if (!m_sEditedClass.IsEmpty() && event.m_pObj->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)))
				{
					CSmartObjectHelperObject* pHelperObject = (CSmartObjectHelperObject*)event.m_pObj;
					DeleteHelper(m_sEditedClass, event.m_pObj->GetName().GetString());
				}
				m_bSinkNeeded = true;
				break;
			case OBJECT_ON_SELECT:     // Sent when objects becomes selected.
			case OBJECT_ON_UNSELECT:   // Sent when objects unselected.
				m_bSinkNeeded = true;
				m_bFilterCanceled = false;
				break;
			case OBJECT_ON_TRANSFORM:  // Sent when object transformed.
				if (event.m_pObj->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)))
				{
					CSmartObjectHelperObject* pHelperObject = (CSmartObjectHelperObject*)event.m_pObj;
					CSOLibrary::VectorHelperData::iterator it = CSOLibrary::FindHelper(m_sEditedClass, pHelperObject->GetName().GetString());
					if (it != CSOLibrary::m_vHelpers.end())
					{
						it->qt.t = event.m_pObj->GetPos();
						it->qt.q = event.m_pObj->GetRotation();
						m_bSinkNeeded = true;
						CSOLibrary::m_bSaveNeeded = true;
					}
				}
				break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnSelectionChanged()
{
	if (!m_bIgnoreNotifications)
	{
		m_bSinkNeeded = true;
		m_bFilterCanceled = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::ModifyRuleOrder(int from, int to)
{
	SmartObjectConditions::iterator it = CSOLibrary::m_Conditions.begin(), itEnd = CSOLibrary::m_Conditions.end();
	if (from < to)
	{
		for (; it != itEnd; ++it)
		{
			int order = it->iOrder;
			if (it->iOrder == from)
				it->iOrder = to;
			else if (from < it->iOrder && it->iOrder <= to)
				it->iOrder--;
		}
	}
	else
	{
		for (; it != itEnd; ++it)
		{
			if (it->iOrder == from)
				it->iOrder = to;
			else if (to <= it->iOrder && it->iOrder < from)
				it->iOrder++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnAddEntry()
{
	if (!GetISystem()->GetIFlowSystem())
		return;

	if (!CheckOutLibrary())
		return;

	m_View.SetFocus();
	m_View.GetSelectedRows()->Clear();

	CSmartObjectTemplateDialog sotd(this);
	sotd.SetSOTemplate("");
	if (sotd.DoModal() != IDOK)
		return;
	int templateId = sotd.GetSOTemplateId();

	CItemDescriptionDlg dlg(this, true, true);
	dlg.m_sCaption = _T("New Smart Object Rule");
	dlg.m_sItemCaption = _T("Rule &name:");
	if (dlg.DoModal() != IDOK)
		return;

	SmartObjectCondition condition =
	{
		"Actor",    //	string	sUserClass;
		"",         //	string	sUserState;
		"",         //	string	sUserHelper;

		m_sFirstFilterClass,//	string	sObjectClass;
		"",         //	string	sObjectState;
		"",         //	string	sObjectHelper;

		0.0f,       //	float	fDistanceFrom;
		10.0f,      //	float	fDistanceTo;
		360.0f,     //	float	fOrientationLimit;
		false,      //  bool    bHorizLimitOnly
		360.0f,     //	float	fOrientationToTargetLimit;

		0.5f,       //	float	fMinDelay;
		5.0f,       //	float	fMaxDelay;
		2.0f,       //	float	fMemory;

		1.0f,       //	float	fProximityFactor;
		0.0f,       //	float	fOrientationFactor;
		0.0f,       //	float	fVisibilityFactor;
		0.0f,       //	float	fRandomnessFactor;

		0,          //	float	fLookAtOnPerc;
		"",         //	string	sUserPreActionState;
		"",         //	string	sObjectPreActionState;
		eAT_Action, //	EActionType eActionType;
		"",         //	string	sAction;
		"",         //	string	sUserPostActionState;
		"",         //	string	sObjectPostActionState;

		2,          //	int		iMaxAlertness;
		true,       //	bool	bEnabled;
		"",         //	string	sName;
		"",         //	string	sFolder;
		"",         //	string	sDescription;
		0,          //	int		iOrder;

		0,          //	int		iRuleType;
		"",         //	string	sEvent;
		"",         //	string	sChainedUserEvent;
		"",         //	string	sChainedObjectEvent;
		"",         //	string	sEntranceHelper;
		"",         //	string	sExitHelper;

		templateId,     //	int		iTemplateId;

		0.0f,       //	float	fApproachSpeed;
		3,          //	int		iApproachStance;
		"",         //	string	sAnimationHelper;
		"",         //	string	sApproachHelper;
		0.0f,       //	float	fStartWidth;
		0.0f,       //	float	fDirectionTolerance;
		0.0f,       //	float	fStartArcAngle;
	};
	condition.sName = dlg.m_sItemEdit;
	condition.sDescription = dlg.m_sDescription;
	condition.sFolder = GetCurrentFolderPath();
	condition.iOrder = INT_MAX;

	const MapTemplates& mapTemplates = GetIEditor()->GetAIManager()->GetMapTemplates();
	MapTemplates::const_iterator itTemplate = mapTemplates.find(templateId);
	assert(itTemplate != mapTemplates.end());
	const CSOTemplate* pTemplate = itTemplate->second;

	SetTemplateDefaults(condition, pTemplate->params);

	CSOLibrary::m_Conditions.push_back(condition);
	ModifyRuleOrder(INT_MAX, 0);
	condition.iOrder = 0; // to select the right row
	m_pEditedEntry = NULL;
	ReloadEntries(false);
	m_bSinkNeeded = true;

	// make sure the folder exists in the tree view
	SetCurrentFolder(condition.sFolder.c_str());

	// now select newly created row
	int i = m_View.GetRows()->GetCount();
	while (i--)
	{
		CXTPReportRow* pRow = m_View.GetRows()->GetAt(i);
		CSmartObjectEntry* pRecord = (CSmartObjectEntry*) pRow->GetRecord();
		if (pRecord && *pRecord->m_pCondition == condition)
		{
			m_pEditedEntry = pRecord;
			m_View.EnsureVisible(pRow);
			m_View.SetFocusedRow(pRow);
			break;
		}
	}

	CSOLibrary::m_bSaveNeeded = true;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnEditEntry()
{
	//	ReloadEntries();
	CSOLibrary::m_bSaveNeeded = true;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnRemoveEntry()
{
	if (!GetISystem()->GetIFlowSystem())
		return;

	if (!CheckOutLibrary())
		return;

	CXTPReportSelectedRows* rows = m_View.GetSelectedRows();
	POSITION pos = rows->GetFirstSelectedRowPosition();
	for (int i = 0; i < rows->GetCount(); ++i)
	{
		CXTPReportRow* row = rows->GetNextSelectedRow(pos);
		CSmartObjectEntry* record = (CSmartObjectEntry*) row->GetRecord();
		if (record)
		{
			ModifyRuleOrder(record->m_pCondition->iOrder, INT_MAX);
			CSOLibrary::m_Conditions.erase(FindRuleByPtr(record->m_pCondition));
		}
	}
	m_pEditedEntry = NULL;
	ReloadEntries(false);
	CSOLibrary::m_bSaveNeeded = true;

	OnReportSelChanged(NULL, NULL);
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnDuplicateEntry()
{
	if (!GetISystem()->GetIFlowSystem())
		return;

	if (!CheckOutLibrary())
		return;

	CXTPReportSelectedRows* rows = m_View.GetSelectedRows();
	if (rows->GetCount() != 1)
		return;

	POSITION pos = rows->GetFirstSelectedRowPosition();
	CXTPReportRow* row = rows->GetNextSelectedRow(pos);
	CSmartObjectEntry* record = (CSmartObjectEntry*) row->GetRecord();
	if (record)
	{
		SmartObjectCondition condition = *record->m_pCondition;
		int order = condition.iOrder + 1;
		condition.iOrder = INT_MAX;

		CItemDescriptionDlg dlg(this, true, true);
		dlg.m_sCaption = _T("Duplicate Smart Object Rule");
		dlg.m_sItemCaption = _T("Rule &name:");
		dlg.m_sItemEdit = CString(_T("Copy of ")) + condition.sName.c_str();
		dlg.m_sDescription = condition.sDescription;
		if (dlg.DoModal() != IDOK)
			return;

		condition.sName = dlg.m_sItemEdit;
		condition.sDescription = dlg.m_sDescription;

		CSOLibrary::m_Conditions.push_back(condition);
		ModifyRuleOrder(INT_MAX, order);
		condition.iOrder = order; // to select the right row
		m_pEditedEntry = NULL;
		ReloadEntries(false);

		// now select duplicated row
		int i = m_View.GetRows()->GetCount();
		while (i--)
		{
			CXTPReportRow* pRow = m_View.GetRows()->GetAt(i);
			CSmartObjectEntry* pRecord = (CSmartObjectEntry*) pRow->GetRecord();
			if (pRecord && *pRecord->m_pCondition == condition)
			{
				m_pEditedEntry = pRecord;
				m_View.EnsureVisible(pRow);
				m_View.SetFocusedRow(pRow);
				break;
			}
		}

		CSOLibrary::m_bSaveNeeded = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnHelpersEdit()
{
	if (!CheckOutLibrary())
		return;

	if (m_sEditedClass.IsEmpty())
	{
		CString sClass = m_sFirstFilterClass;
		if (m_pEditedEntry && sClass.IsEmpty())
		{
			if (!m_pEditedEntry->m_pCondition->sObjectClass.empty())
				sClass = m_pEditedEntry->m_pCondition->sObjectClass;
			else
				sClass = m_pEditedEntry->m_pCondition->sUserClass;
		}

		CSmartObjectClassDialog dlg(this);
		dlg.SetSOClass(sClass);
		dlg.EnableOK();
		if (dlg.DoModal() == IDOK)
		{
			sClass = dlg.GetSOClass();
			if (!sClass.IsEmpty())
			{
				m_sEditedClass = sClass;

				m_pItemHelpersNew->SetEnabled(TRUE);
				m_pItemHelpersDone->SetEnabled(TRUE);

				m_pItemHelpersEdit->SetCaption("Editing " + sClass);
				m_pItemHelpersEdit->SetBold(TRUE);

				m_bSinkNeeded = true;
			}
		}
	}
	else if (!m_mapHelperObjects.empty())
	{
		CString sSelectedHelper;
		CBaseObject* pSelectedObject = GetIEditor()->GetSelectedObject();
		if (!pSelectedObject)
			return;

		if (pSelectedObject->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)))
			sSelectedHelper = pSelectedObject->GetName();

		CSmartObjectHelperDialog dlg(this, false, false, false);
		dlg.SetSOHelper(m_sEditedClass, sSelectedHelper);
		if (dlg.DoModal() == IDOK)
		{
			sSelectedHelper = dlg.GetSOHelper();
			MapHelperObjects::iterator it, itEnd = m_mapHelperObjects.end();
			for (it = m_mapHelperObjects.begin(); it != itEnd; ++it)
			{
				if (it->second->GetName() == sSelectedHelper.GetString())
				{
					GetIEditor()->SelectObject(it->second);
					GetIEditor()->GetObjectManager()->UnselectObject(pSelectedObject);
					break;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnHelpersNew()
{
	if (!m_sEditedClass.IsEmpty())
	{
		CItemDescriptionDlg dlg(this);
		dlg.m_sCaption = "New Helper";
		dlg.m_sItemCaption = "Helper &name:";
		if (dlg.DoModal() == IDOK)
		{
			CSOLibrary::VectorHelperData::iterator it = CSOLibrary::FindHelper(m_sEditedClass, dlg.m_sItemEdit);
			if (it != CSOLibrary::m_vHelpers.end())
			{
				CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("The specified smart object helper name is already used.\n\nCreation canceled..."));
				return;
			}
			CSOLibrary::CHelperData helper;
			helper.className = m_sEditedClass;
			helper.qt.t.zero();
			helper.qt.q.SetIdentity();
			helper.name = dlg.m_sItemEdit;
			helper.description = dlg.m_sDescription;
			it = CSOLibrary::HelpersUpperBound(m_sEditedClass);
			CSOLibrary::m_vHelpers.insert(it, helper);
			CSOLibrary::m_bSaveNeeded = true;

			// this will eventually show the new helper
			m_bSinkNeeded = true;
			SinkSelection();

			// try to select the newly created helper
			CBaseObject* pSelected = GetIEditor()->GetSelectedObject();
			if (pSelected)
			{
				if (pSelected->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)))
					pSelected = pSelected->GetParent();

				int i = pSelected->GetChildCount();
				while (i--)
				{
					CBaseObject* pChild = pSelected->GetChild(i);
					if (pChild->GetName() == helper.name.GetString())
					{
						GetIEditor()->ClearSelection();
						GetIEditor()->SelectObject(pChild);
						break;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnHelpersDelete()
{
	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	int selCount = pSelection->GetCount();

	if (!m_sEditedClass.IsEmpty() && selCount == 1 && pSelection->GetObject(0)->IsKindOf(RUNTIME_CLASS(CSmartObjectHelperObject)))
	{
		CString msg;
		msg.Format("Delete helper '%s'?", pSelection->GetObject(0)->GetName());

		if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(msg)))
		{
			DeleteHelper(m_sEditedClass, pSelection->GetObject(0)->GetName().GetString());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnHelpersDone()
{
	if (!m_sEditedClass.IsEmpty())
	{
		m_bIgnoreNotifications = true;
		MapHelperObjects::iterator it, itEnd = m_mapHelperObjects.end();
		for (it = m_mapHelperObjects.begin(); it != itEnd; ++it)
		{
			CSmartObjectHelperObject* pHelperObject = it->second;
			pHelperObject->RemoveEventListener(functor(*this, &CSmartObjectsEditorDialog::OnObjectEventLegacy));
			pHelperObject->DetachThis();
			pHelperObject->Release();
			GetIEditor()->GetObjectManager()->DeleteObject(pHelperObject);
		}
		m_mapHelperObjects.clear();
		m_bIgnoreNotifications = false;

		m_sEditedClass.Empty();

		m_pItemHelpersEdit->SetCaption("Edit...");
		m_pItemHelpersEdit->SetBold(FALSE);

		m_pItemHelpersNew->SetEnabled(FALSE);
		m_pItemHelpersDelete->SetEnabled(FALSE);
		m_pItemHelpersDone->SetEnabled(FALSE);

		m_bSinkNeeded = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::EnableIfOneSelected(CCmdUI* target)
{
	CXTPReportSelectedRows* pRows = m_View.GetSelectedRows();
	if (pRows->GetCount() == 1)
	{
		POSITION pos = pRows->GetFirstSelectedRowPosition();
		target->Enable(pRows->GetNextSelectedRow(pos)->GetRecord() != NULL);
	}
	else
		target->Enable(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::EnableIfSelected(CCmdUI* target)
{
	target->Enable(m_View.GetSelectedRows()->GetCount());
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSmartObjectsEditorDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*) lParam;
		if (!pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_SOED_TASK_PANE:
				pwndDockWindow->Attach(&m_taskPanel);
				m_taskPanel.SetOwner(this);
				break;
			case IDW_SOED_PROPS_PANE:
				pwndDockWindow->Attach(&m_Properties);
				m_Properties.SetOwner(this);
				break;
			case IDW_SOED_TREE_PANE:
				pwndDockWindow->Attach(&m_Tree);
				m_Tree.SetOwner(this);
				break;
			case IDW_SOED_DESC_PANE:
				pwndDockWindow->Attach(&m_Description);
				m_Description.SetOwner(this);
				break;
			default:
				return FALSE;
			}
		}
		return TRUE;
	}
	else if (wParam == XTP_DPN_CLOSEPANE)
	{
		// get a pointer to the docking pane being closed.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_SOED_TASK_PANE:
				break;
			case IDW_SOED_PROPS_PANE:
				break;
			case IDW_SOED_TREE_PANE:
				break;
			case IDW_SOED_DESC_PANE:
				break;
			}
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CSmartObjectsEditorDialog::PreTranslateMessage(MSG* pMsg)
{
	// allow tooltip messages to be filtered
	if (__super::PreTranslateMessage(pMsg))
		return TRUE;

	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		// All keypresses are translated by this frame window

		::TranslateMessage(pMsg);
		::DispatchMessage(pMsg);

		return TRUE;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSmartObjectsEditorDialog::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			SendMessage(WM_COMMAND, MAKEWPARAM(nCmdID, 0), 0);
		}
		break;
	case XTP_TPN_RCLICK:
		break;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnClose()
{
	// This will call CSmartObjectsEditorDialog::OnUpdateProperties if needed.
	// It must be done here to ensure applying changes on the edited rule!
	m_Properties.SelectItem(0);

	__super::OnClose();
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::RecalcLayout(BOOL bNotify)
{
	__super::RecalcLayout(bNotify);

	if (IsWindow(m_View))
	{
		CRect rc = GetDockingPaneManager()->GetClientPane()->GetPaneWindowRect();
		ScreenToClient(rc);
		if (!m_sFilterClasses.IsEmpty())
			rc.top = min(rc.top + 23, rc.bottom);
		m_View.MoveWindow(rc);
		Invalidate();
	}
}

void CSmartObjectsEditorDialog::OnMouseMove(UINT flags, CPoint pt)
{
	CWnd* pCapture = GetCapture();
	if (!pCapture || pCapture == this)
	{
		bool ptInRect = m_rcCloseBtn.PtInRect(pt);
		if (!pCapture)
		{
			if (!ptInRect || (flags & MK_LBUTTON))
				return;

			SetCapture();
			CClientDC dc(this);
			COLORREF clr = GetSysColor(COLOR_3DSHADOW);
			dc.Draw3dRect(m_rcCloseBtn, clr, clr);
		}
		else
		{
			CClientDC dc(this);
			COLORREF clr = ptInRect ? GetSysColor(COLOR_3DSHADOW) : GetSysColor(COLOR_INFOBK);
			dc.Draw3dRect(m_rcCloseBtn, clr, clr);

			if (!ptInRect && !(flags & MK_LBUTTON))
				ReleaseCapture();
		}
	}
}

void CSmartObjectsEditorDialog::OnLButtonUp(UINT flags, CPoint pt)
{
	CWnd* pCapture = GetCapture();
	if (pCapture == this && !flags && m_rcCloseBtn.PtInRect(pt))
	{
		m_bFilterCanceled = true;
		m_bSinkNeeded = true;
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CSmartObjectsEditorDialog::OnEraseBkgnd(CDC* pDC)
{
	__super::OnEraseBkgnd(pDC);
	if (!m_sFilterClasses.IsEmpty())
	{
		CRect rc = GetDockingPaneManager()->GetClientPane()->GetPaneWindowRect();
		ScreenToClient(rc);
		rc.bottom = min(rc.top + 21, rc.bottom);

		pDC->FillSolidRect(rc, GetSysColor(COLOR_INFOBK));
		CGdiObject* pOldFont = pDC->SelectStockObject(DEFAULT_GUI_FONT);
		pDC->SetTextColor(GetSysColor(COLOR_INFOTEXT));

		CRect rcText(rc);
		rcText.right = max(rcText.left, rcText.right - 19);
		pDC->MoveTo(rcText.right + 4, rcText.top + 7);
		pDC->LineTo(rcText.right + 11, rcText.top + 14);
		pDC->MoveTo(rcText.right + 5, rcText.top + 7);
		pDC->LineTo(rcText.right + 12, rcText.top + 14);
		pDC->MoveTo(rcText.right + 4, rcText.top + 13);
		pDC->LineTo(rcText.right + 11, rcText.top + 6);
		pDC->MoveTo(rcText.right + 5, rcText.top + 13);
		pDC->LineTo(rcText.right + 12, rcText.top + 6);

		m_rcCloseBtn.SetRect(rcText.right + 1, rcText.top + 4, rcText.right + 15, rcText.bottom - 4);

		if (m_sFilterClasses.Find(',') >= 0)
			pDC->DrawText(" Filtered by classes: " + m_sFilterClasses, rcText, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
		else
			pDC->DrawText(" Filtered by class: " + m_sFilterClasses, rcText, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
		pDC->Draw3dRect(rc, GetSysColor(COLOR_3DSHADOW), GetSysColor(COLOR_3DSHADOW));
		pDC->SelectObject(pOldFont);

		rc.top = rc.bottom;
		rc.bottom += 2;
		pDC->FillSolidRect(rc, GetSysColor(COLOR_BTNFACE));
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectsEditorDialog::OnDestroy()
{
	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	GetDockingPaneManager()->GetLayout(&layout);
	layout.Save(_T("SmartObjectsLayout"));

	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
#define ID_REMOVE_ITEM             1
#define ID_SORT_ASC                2
#define ID_SORT_DESC               3
#define ID_GROUP_BYTHIS            4
#define ID_SHOW_GROUPBOX           5
#define ID_SHOW_FIELDCHOOSER       6
#define ID_COLUMN_BESTFIT          7
#define ID_COLUMN_ARRANGEBY        100
#define ID_COLUMN_ALIGMENT         200
#define ID_COLUMN_ALIGMENT_LEFT    ID_COLUMN_ALIGMENT + 1
#define ID_COLUMN_ALIGMENT_RIGHT   ID_COLUMN_ALIGMENT + 2
#define ID_COLUMN_ALIGMENT_CENTER  ID_COLUMN_ALIGMENT + 3
#define ID_COLUMN_SHOW_DESCRIPTION 400
#define ID_COLUMN_SHOW_USER_OBJECT 401
#define ID_COLUMN_SHOW             500

void CSmartObjectsEditorDialog::OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify->pColumn);
	CPoint ptClick = pItemNotify->pt;

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	menu.AppendMenu(MF_STRING, ID_COLUMN_SHOW_DESCRIPTION, _T("Name/Description"));
	menu.AppendMenu(MF_STRING, ID_COLUMN_SHOW_USER_OBJECT, _T("Name/User/Object"));

	menu.AppendMenu(MF_SEPARATOR);

	// create columns items
	CXTPReportColumns* pColumns = m_View.GetColumns();
	int nColumnCount = pColumns->GetCount();
	for (int i = 0; i < nColumnCount; ++i)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(i);
		CString sCaption = pCol->GetCaption();
		if (!sCaption.IsEmpty())
		{
			menu.AppendMenu(MF_STRING, ID_COLUMN_SHOW + i, sCaption);
			menu.CheckMenuItem(ID_COLUMN_SHOW + i, MF_BYCOMMAND | (pCol->IsVisible() ? MF_CHECKED : MF_UNCHECKED));
		}
	}

	// track menu
	int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);

	// process column selection item
	if (nMenuResult >= ID_COLUMN_SHOW)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nMenuResult - ID_COLUMN_SHOW);
		if (pCol)
			pCol->SetVisible(!pCol->IsVisible());
	}

	// other general items
	switch (nMenuResult)
	{
	case ID_COLUMN_SHOW_DESCRIPTION:
		m_pColName->SetVisible(TRUE);
		m_pColDescription->SetVisible(TRUE);
		m_pColUserClass->SetVisible(FALSE);
		m_pColUserState->SetVisible(FALSE);
		m_pColObjectClass->SetVisible(FALSE);
		m_pColObjectState->SetVisible(FALSE);
		m_pColAction->SetVisible(FALSE);
		break;
	case ID_COLUMN_SHOW_USER_OBJECT:
		m_pColName->SetVisible(TRUE);
		m_pColDescription->SetVisible(FALSE);
		m_pColUserClass->SetVisible(TRUE);
		m_pColUserState->SetVisible(TRUE);
		m_pColObjectClass->SetVisible(TRUE);
		m_pColObjectState->SetVisible(TRUE);
		m_pColAction->SetVisible(TRUE);
		break;
	}
}

void CSmartObjectsEditorDialog::OnReportSelChanged(NMHDR* /*pNotifyStruct*/, LRESULT* /*result*/)
{
	// This will call CSmartObjectsEditorDialog::OnUpdateProperties if needed.
	// It must be done here to ensure applying changes on the edited rule!
	m_Properties.SelectItem(0);

	CXTPReportRow* focused = m_View.GetFocusedRow();
	if (!focused)
	{
		m_Properties.EnableWindow(FALSE);
		m_Description.SetWindowText(_T(""));
		return;
	}

	m_pEditedEntry = (CSmartObjectEntry*) focused->GetRecord();
	if (!m_pEditedEntry)
	{
		m_Properties.EnableWindow(FALSE);
		m_Description.SetWindowText(_T(""));
		return;
	}

	m_Properties.EnableWindow(TRUE);
	m_Description.SetWindowText(m_pEditedEntry->m_pCondition->sDescription.c_str());

	// Update variables.
	m_Properties.EnableUpdateCallback(false);
	gSmartObjectsUI.SetConditionToUI(*m_pEditedEntry->m_pCondition, &m_Properties);
	m_Properties.EnableUpdateCallback(true);

	UpdatePropertyTables();
	m_View.SetFocus();
}

void CSmartObjectsEditorDialog::UpdatePropertyTables()
{
	//bool nav = gSmartObjectsUI.bNavigationRule;

	//gSmartObjectsUI.entranceHelper.SetFlags( nav ? 0 : IVariable::UI_DISABLED );
	//gSmartObjectsUI.exitHelper.SetFlags( nav ? 0 : IVariable::UI_DISABLED );
	//m_Properties.Expand( m_Properties.FindItemByVar( &gSmartObjectsUI.navigationTable ), nav );

	//gSmartObjectsUI.limitsDistanceFrom.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//gSmartObjectsUI.limitsDistanceTo.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//gSmartObjectsUI.limitsOrientation.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//m_Properties.Expand( m_Properties.FindItemByVar( &gSmartObjectsUI.limitsTable ), !nav );

	//gSmartObjectsUI.delayMinimum.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//gSmartObjectsUI.delayMaximum.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//gSmartObjectsUI.delayMemory.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//m_Properties.Expand( m_Properties.FindItemByVar( &gSmartObjectsUI.delayTable ), !nav );

	//gSmartObjectsUI.multipliersProximity.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//gSmartObjectsUI.multipliersOrientation.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//gSmartObjectsUI.multipliersVisibility.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//gSmartObjectsUI.multipliersRandomness.SetFlags( nav ? IVariable::UI_DISABLED : 0 );
	//m_Properties.Expand( m_Properties.FindItemByVar( &gSmartObjectsUI.multipliersTable ), !nav );

	m_Description.SetWindowText((const CString&) gSmartObjectsUI.sDescription);
}

void CSmartObjectsEditorDialog::OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->nHyperlink >= 0)
	{
		CSmartObjectEntry* pRecord = DYNAMIC_DOWNCAST(CSmartObjectEntry, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			IAIAction* pAction = gEnv->pAISystem ? gEnv->pAISystem->GetAIActionManager()->GetAIAction(pRecord->m_pCondition->sAction) : NULL;
			if (pAction)
			{
				CFlowGraphManager* pManager = GetIEditor()->GetFlowGraphManager();
				CHyperFlowGraph* pFlowGraph = pManager->FindGraphForAction(pAction);
				assert(pFlowGraph);
				if (pFlowGraph)
					pManager->OpenView(pFlowGraph);
			}
		}
	}
}

void CSmartObjectsEditorDialog::OnReportChecked(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	CSmartObjectEntry* pRecord = DYNAMIC_DOWNCAST(CSmartObjectEntry, pItemNotify->pRow->GetRecord());
	if (pRecord)
	{
		if (!CSOLibrary::StartEditing())
		{
			pRecord->Update();
			return;
		}

		pRecord->m_pCondition->bEnabled = !pRecord->m_pCondition->bEnabled;
		if (m_View.GetFocusedRow() == pItemNotify->pRow)
		{
			m_Properties.EnableUpdateCallback(false);
			gSmartObjectsUI.bEnabled = pRecord->m_pCondition->bEnabled;
			m_Properties.EnableUpdateCallback(true);

			COLORREF color = pRecord->m_pCondition->bEnabled ? GetSysColor(COLOR_WINDOWTEXT) : GetSysColor(COLOR_GRAYTEXT);
			for (int i = 0; i < 6; ++i)
				pRecord->GetItem(i)->SetTextColor(color);
		}
	}
}

void CSmartObjectsEditorDialog::OnReportEdit(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	CSmartObjectEntry* pRecord = DYNAMIC_DOWNCAST(CSmartObjectEntry, pItemNotify->pRow->GetRecord());
	if (pRecord)
	{
		if (!CSOLibrary::StartEditing())
		{
			pRecord->Update();
			return;
		}

		if (pItemNotify->pColumn == m_pColName)
		{
			pRecord->m_pCondition->sName = static_cast<CXTPReportRecordItemText*>(pItemNotify->pItem)->GetValue();
			pItemNotify->pItem->SetCaption(pRecord->m_pCondition->sName.c_str());
		}
		else if (pItemNotify->pColumn == m_pColDescription)
		{
			pRecord->m_pCondition->sDescription = static_cast<CXTPReportRecordItemText*>(pItemNotify->pItem)->GetValue();
			pItemNotify->pItem->SetCaption(pRecord->m_pCondition->sDescription.c_str());
		}

		if (m_View.GetFocusedRow() == pItemNotify->pRow)
		{
			m_Properties.EnableUpdateCallback(false);
			gSmartObjectsUI.sName = pRecord->m_pCondition->sName.c_str();
			gSmartObjectsUI.sDescription = pRecord->m_pCondition->sDescription.c_str();
			m_Description.SetWindowText(pRecord->m_pCondition->sDescription.c_str());
			m_Properties.EnableUpdateCallback(true);
		}
	}
}

void CSmartObjectsEditorDialog::OnTreeBeginEdit(NMHDR* pNotifyStruct, LRESULT* result)
{
	if (!CSOLibrary::StartEditing())
	{
		*result = TRUE;
		return;
	}

	LPNMTVDISPINFO pItemNotify = (LPNMTVDISPINFO) pNotifyStruct;
	*result = m_Tree.GetParentItem(pItemNotify->item.hItem) == NULL;
}

void CSmartObjectsEditorDialog::OnTreeEndEdit(NMHDR* pNotifyStruct, LRESULT* result)
{
	LPNMTVDISPINFO pItemNotify = (LPNMTVDISPINFO) pNotifyStruct;
	bool res = pItemNotify->item.pszText && *pItemNotify->item.pszText;
	if (res)
	{
		CSOLibrary::m_bSaveNeeded = true;

		bool bRemove = false;
		CString txt(pItemNotify->item.pszText);
		res = txt.FindOneOf(_T("/\\")) < 0;

		if (res)
		{
			// first check is there already a folder with that name
			HTREEITEM item = m_Tree.GetChildItem(m_Tree.GetParentItem(pItemNotify->item.hItem));
			while (item)
			{
				if (item != pItemNotify->item.hItem &&
				    m_Tree.GetItemText(item).CompareNoCase(txt) == 0)
				{
					res = false;

					CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("There is already a sibling folder with that name!"));
					break;
				}
				item = m_Tree.GetNextSiblingItem(item);
			}
		}

		if (res)
		{
			// renaming is allowed => update paths
			CString from, to;
			HTREEITEM item = m_Tree.GetParentItem(pItemNotify->item.hItem);
			while (item)
			{
				HTREEITEM parent = m_Tree.GetParentItem(item);
				if (parent)
					from = _T('/') + m_Tree.GetItemText(item) + from;
				else
					from = from + _T('/');
				item = parent;
			}
			to = from + txt;
			from += m_Tree.GetItemText(pItemNotify->item.hItem);
			MoveAllRules(from, to);
		}
	}
	*result = res;
}

void CSmartObjectsEditorDialog::OnTreeSelChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	m_View.GetColumns()->SetSortColumn(m_pColOrder, TRUE);
	m_bSinkNeeded = true;
}

void CSmartObjectsEditorDialog::OnTreeSetFocus(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	m_View.SetFocusedRow(NULL, TRUE);
}

void CSmartObjectsEditorDialog::MoveAllRules(const CString& from, const CString& to)
{
	CString temp;
	CString from1 = from + _T('/');
	int len = from1.GetLength();
	SmartObjectConditions::iterator it, itEnd = CSOLibrary::m_Conditions.end();
	for (it = CSOLibrary::m_Conditions.begin(); it != itEnd; ++it)
	{
		SmartObjectCondition& condition = *it;
		if (from.CompareNoCase(condition.sFolder.c_str()) == 0)
			condition.sFolder = to;
		else
		{
			temp = condition.sFolder.substr(0, len).c_str();
			if (from1.CompareNoCase(temp) == 0)
				condition.sFolder = to + _T('/') + condition.sFolder.substr(len).c_str();
		}
	}
}

void CSmartObjectsEditorDialog::OnDescriptionEdit(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	if (m_pEditedEntry)
	{
		CString txt = m_pEditedEntry->m_pCondition->sDescription;
		m_Description.GetWindowText(txt);
		m_pEditedEntry->m_pCondition->sDescription = txt;
		gSmartObjectsUI.sDescription = txt;
	}
}

/*
   #define ID_POPUP_COLLAPSEALLGROUPS 1
   #define ID_POPUP_EXPANDALLGROUPS 2

   //////////////////////////////////////////////////////////////////////////
   void CSmartObjectsEditorDialog::OnReportItemRClick( NMHDR* pNotifyStruct, LRESULT* result )
   {
   XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

   if ( !pItemNotify->pRow )
    return;

   if ( pItemNotify->pRow->IsGroupRow() )
   {
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, ID_POPUP_COLLAPSEALLGROUPS,_T("Collapse &All Groups") );
    menu.AppendMenu(MF_STRING, ID_POPUP_EXPANDALLGROUPS,_T("E&xpand All Groups") );

    // track menu
    int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN |TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);

    // general items processing
    switch (nMenuResult)
    {
    case ID_POPUP_COLLAPSEALLGROUPS:
      pItemNotify->pRow->GetControl()->CollapseAll();
      break;
    case ID_POPUP_EXPANDALLGROUPS:
      pItemNotify->pRow->GetControl()->ExpandAll();
      break;
    }
   } else if (pItemNotify->pItem)
   {
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 0,_T("Select Object(s)") );
    menu.AppendMenu(MF_STRING, 1,_T("Copy Warning(s) To Clipboard") );

    // track menu
    int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN|TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);
   }
   }
 */

bool CSmartObjectsEditorDialog::CheckOutLibrary()
{
	// This will call CSmartObjectsEditorDialog::OnUpdateProperties if needed.
	// It must be done here to ensure applying changes on the edited rule!
	m_Properties.SelectItem(0);

	return CSOLibrary::StartEditing();
}

bool CSmartObjectsEditorDialog::SaveSOLibrary()
{
	// This will call CSmartObjectsEditorDialog::OnUpdateProperties if needed.
	// It must be done here to ensure applying changes on the edited rule!
	m_Properties.SelectItem(0);

	return CSOLibrary::Save();
}

CSOLibrary::~CSOLibrary()
{
	m_Conditions.clear();
	m_vHelpers.clear();
	m_vEvents.clear();
	m_vStates.clear();
	m_vClassTemplates.clear();
	m_vClasses.clear();
}

bool CSOLibrary::StartEditing()
{
	Load();

	CFileUtil::CreateDirectory(PathUtil::GetPathWithoutFilename(PathUtil::GetGameFolder() + "/" + SMART_OBJECTS_XML));
	if (!CFileUtil::OverwriteFile(PathUtil::Make(PathUtil::GetGameFolder(), SMART_OBJECTS_XML).c_str()))
		return false;
	m_bSaveNeeded = true;
	return true;
}

bool CSOLibrary::Save()
{
	CFileUtil::CreateDirectory(PathUtil::GetPathWithoutFilename(PathUtil::GetGameFolder() + "/" + SMART_OBJECTS_XML));
	if (!CFileUtil::OverwriteFile(PathUtil::Make(PathUtil::GetGameFolder(), SMART_OBJECTS_XML).c_str()))
		return false;

	// save to SmartObjects.xml
	CString sSavePath = PathUtil::GetGameFolder() + "/" + SMART_OBJECTS_XML;
	if (!SaveToFile(sSavePath))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CSOLibrary::Save() failed to save in %s", SMART_OBJECTS_XML);
		return false;
	}
	CryLog("CSOLibrary::Save() saved in %s", SMART_OBJECTS_XML);
	return true;
}

struct less_ptr : public std::binary_function<const SmartObjectCondition*, const SmartObjectCondition*, bool>
{
	bool operator()(const SmartObjectCondition* _Left, const SmartObjectCondition* _Right) const
	{
		return *_Left < *_Right;
	}
};

bool CSOLibrary::SaveToFile(const char* sFileName)
{
	typedef std::vector<const SmartObjectCondition*> RulesList;
	RulesList sorted;

	SmartObjectConditions::iterator itUnsorted, itUnsortedEnd = m_Conditions.end();
	for (itUnsorted = m_Conditions.begin(); itUnsorted != itUnsortedEnd; ++itUnsorted)
	{
		SmartObjectCondition* pCondition = &*itUnsorted;
		sorted.push_back(pCondition);
	}
	std::sort(sorted.begin(), sorted.end(), less_ptr());

	XmlNodeRef root(GetISystem()->CreateXmlNode("SmartObjectsLibrary"));
	RulesList::iterator itRules = sorted.begin();
	for (int i = 0; i < sorted.size(); ++i)
	{
		XmlNodeRef node(root->createNode("SmartObject"));
		const SmartObjectCondition& rule = **itRules;
		++itRules;

		node->setAttr("iTemplateId", rule.iTemplateId);

		node->setAttr("sName", rule.sName);
		node->setAttr("sFolder", rule.sFolder);
		node->setAttr("sDescription", rule.sDescription);
		//node->setAttr( "iOrder",				rule.iOrder );

		node->setAttr("sUserClass", rule.sUserClass);
		node->setAttr("sUserState", rule.sUserState);
		node->setAttr("sUserHelper", rule.sUserHelper);

		node->setAttr("sObjectClass", rule.sObjectClass);
		node->setAttr("sObjectState", rule.sObjectState);
		node->setAttr("sObjectHelper", rule.sObjectHelper);

		node->setAttr("iMaxAlertness", rule.iMaxAlertness);
		node->setAttr("bEnabled", rule.bEnabled);
		node->setAttr("sEvent", rule.sEvent);

		node->setAttr("iRuleType", rule.iRuleType);
		node->setAttr("sEntranceHelper", rule.sEntranceHelper);
		node->setAttr("sExitHelper", rule.sExitHelper);

		node->setAttr("fMinDelay", rule.fMinDelay);
		node->setAttr("fMaxDelay", rule.fMaxDelay);
		node->setAttr("fMemory", rule.fMemory);

		node->setAttr("fDistanceFrom", rule.fDistanceFrom);
		node->setAttr("fDistanceLimit", rule.fDistanceTo);
		node->setAttr("fOrientationLimit", rule.fOrientationLimit);
		node->setAttr("bHorizLimitOnly", rule.bHorizLimitOnly);
		node->setAttr("fOrientToTargetLimit", rule.fOrientationToTargetLimit);

		node->setAttr("fProximityFactor", rule.fProximityFactor);
		node->setAttr("fOrientationFactor", rule.fOrientationFactor);
		node->setAttr("fRandomnessFactor", rule.fRandomnessFactor);
		node->setAttr("fVisibilityFactor", rule.fVisibilityFactor);

		node->setAttr("fLookAtOnPerc", rule.fLookAtOnPerc);
		node->setAttr("sObjectPreActionState", rule.sObjectPreActionState);
		node->setAttr("sUserPreActionState", rule.sUserPreActionState);
		node->setAttr("sAction", rule.sAction);
		node->setAttr("eActionType", rule.eActionType);
		node->setAttr("sObjectPostActionState", rule.sObjectPostActionState);
		node->setAttr("sUserPostActionState", rule.sUserPostActionState);

		node->setAttr("fApproachSpeed", rule.fApproachSpeed);
		node->setAttr("iApproachStance", rule.iApproachStance);
		node->setAttr("sAnimationHelper", rule.sAnimationHelper);
		node->setAttr("sApproachHelper", rule.sApproachHelper);
		node->setAttr("fStartWidth", rule.fStartWidth);
		node->setAttr("fStartDirectionTolerance", rule.fDirectionTolerance);
		node->setAttr("fStartArcAngle", rule.fStartArcAngle);

		root->addChild(node);
	}

	VectorClassData::iterator itClasses, itClassesEnd = m_vClasses.end();
	for (itClasses = m_vClasses.begin(); itClasses != itClassesEnd; ++itClasses)
	{
		XmlNodeRef node(root->createNode("Class"));
		const CClassData& classData = *itClasses;

		node->setAttr("name", classData.name);
		node->setAttr("description", classData.description);
		node->setAttr("location", classData.location);
		node->setAttr("template", classData.templateName);

		root->addChild(node);
	}

	VectorStateData::iterator itStates, itStatesEnd = m_vStates.end();
	for (itStates = m_vStates.begin(); itStates != itStatesEnd; ++itStates)
	{
		XmlNodeRef node(root->createNode("State"));
		const CStateData& stateData = *itStates;

		node->setAttr("name", stateData.name);
		node->setAttr("description", stateData.description);
		node->setAttr("location", stateData.location);

		root->addChild(node);
	}

	VectorHelperData::const_iterator itHelpers, itHelpersEnd = m_vHelpers.end();
	for (itHelpers = m_vHelpers.begin(); itHelpers != itHelpersEnd; ++itHelpers)
	{
		XmlNodeRef node(root->createNode("Helper"));
		const CHelperData& helper = *itHelpers;

		node->setAttr("className", helper.className);
		node->setAttr("name", helper.name);
		node->setAttr("pos", helper.qt.t);
		node->setAttr("rot", helper.qt.q);
		node->setAttr("description", helper.description);

		root->addChild(node);
	}

	VectorEventData::iterator it, itEnd = m_vEvents.end();
	for (it = m_vEvents.begin(); it != itEnd; ++it)
	{
		XmlNodeRef node(root->createNode("Event"));
		const CEventData& event = *it;

		node->setAttr("name", event.name);
		node->setAttr("description", event.description);

		root->addChild(node);
	}

	bool bSucceed = root->saveToFile(sFileName);
	if (bSucceed && gEnv->pAISystem)
		gEnv->pAISystem->GetSmartObjectManager()->ReloadSmartObjectRules();
	return bSucceed;
}

void CSOLibrary::AddAllStates(const string& sStates)
{
	if (!sStates.empty())
	{
		SetStrings setStates;
		String2Classes(sStates, setStates);
		SetStrings::const_iterator it, itEnd = setStates.end();
		for (it = setStates.begin(); it != itEnd; ++it)
			AddState(*it, "", "");
	}
}

bool CSOLibrary::LoadFromFile(const char* sFileName)
{
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(sFileName);
	if (!root)
		return false;

	m_Conditions.clear();
	m_vHelpers.clear();
	m_vEvents.clear();
	m_vStates.clear();
	m_vClasses.clear();

	CHelperData helper;
	SmartObjectCondition rule;
	int count = root->getChildCount();
	for (int i = 0; i < count; ++i)
	{
		XmlNodeRef node = root->getChild(i);
		if (node->isTag("SmartObject"))
		{
			rule.iTemplateId = 0;
			node->getAttr("iTemplateId", rule.iTemplateId);

			rule.sName = node->getAttr("sName");
			rule.sDescription = node->getAttr("sDescription");
			rule.sFolder = node->getAttr("sFolder");
			rule.sEvent = node->getAttr("sEvent");

			node->getAttr("iMaxAlertness", rule.iMaxAlertness);
			node->getAttr("bEnabled", rule.bEnabled);

			rule.iRuleType = 0;
			node->getAttr("iRuleType", rule.iRuleType);

			// this was here to convert older data formats - not needed anymore.
			//float fPassRadius = 0.0f; // default value if not found in XML
			//node->getAttr( "fPassRadius", fPassRadius );
			//if ( fPassRadius > 0 )
			//	rule.iRuleType = 1;

			rule.sEntranceHelper = node->getAttr("sEntranceHelper");
			rule.sExitHelper = node->getAttr("sExitHelper");

			rule.sUserClass = node->getAttr("sUserClass");
			if (!rule.sUserClass.empty())
				AddClass(rule.sUserClass, "", "", "");
			rule.sUserState = node->getAttr("sUserState");
			AddAllStates(rule.sUserState);
			rule.sUserHelper = node->getAttr("sUserHelper");

			rule.sObjectClass = node->getAttr("sObjectClass");
			if (!rule.sObjectClass.empty())
				AddClass(rule.sObjectClass, "", "", "");
			rule.sObjectState = node->getAttr("sObjectState");
			AddAllStates(rule.sObjectState);
			rule.sObjectHelper = node->getAttr("sObjectHelper");

			node->getAttr("fMinDelay", rule.fMinDelay);
			node->getAttr("fMaxDelay", rule.fMaxDelay);
			node->getAttr("fMemory", rule.fMemory);

			rule.fDistanceFrom = 0.0f; // default value if not found in XML
			node->getAttr("fDistanceFrom", rule.fDistanceFrom);
			node->getAttr("fDistanceLimit", rule.fDistanceTo);
			node->getAttr("fOrientationLimit", rule.fOrientationLimit);
			rule.bHorizLimitOnly = false;
			node->getAttr("bHorizLimitOnly", rule.bHorizLimitOnly);
			rule.fOrientationToTargetLimit = 360.0f; // default value if not found in XML
			node->getAttr("fOrientToTargetLimit", rule.fOrientationToTargetLimit);

			node->getAttr("fProximityFactor", rule.fProximityFactor);
			node->getAttr("fOrientationFactor", rule.fOrientationFactor);
			node->getAttr("fVisibilityFactor", rule.fVisibilityFactor);
			node->getAttr("fRandomnessFactor", rule.fRandomnessFactor);

			node->getAttr("fLookAtOnPerc", rule.fLookAtOnPerc);
			rule.sUserPreActionState = node->getAttr("sUserPreActionState");
			AddAllStates(rule.sUserPreActionState);
			rule.sObjectPreActionState = node->getAttr("sObjectPreActionState");
			AddAllStates(rule.sObjectPreActionState);

			rule.sAction = node->getAttr("sAction");
			int iActionType;
			if (node->getAttr("eActionType", iActionType))
			{
				rule.eActionType = (EActionType) iActionType;
			}
			else
			{
				rule.eActionType = eAT_None;

				bool bSignal = !rule.sAction.empty() && rule.sAction[0] == '%';
				bool bAnimationSignal = !rule.sAction.empty() && rule.sAction[0] == '$';
				bool bAnimationAction = !rule.sAction.empty() && rule.sAction[0] == '@';
				bool bAction = !rule.sAction.empty() && !bSignal && !bAnimationSignal && !bAnimationAction;

				bool bHighPriority = rule.iMaxAlertness >= 100;
				node->getAttr("bHighPriority", bHighPriority);

				if (bHighPriority && bAction)
					rule.eActionType = eAT_PriorityAction;
				else if (bAction)
					rule.eActionType = eAT_Action;
				else if (bSignal)
					rule.eActionType = eAT_AISignal;
				else if (bAnimationSignal)
					rule.eActionType = eAT_AnimationSignal;
				else if (bAnimationAction)
					rule.eActionType = eAT_AnimationAction;

				if (bSignal || bAnimationSignal || bAnimationAction)
					rule.sAction.erase(0, 1);
			}
			rule.iMaxAlertness %= 100;

			rule.sUserPostActionState = node->getAttr("sUserPostActionState");
			AddAllStates(rule.sUserPostActionState);
			rule.sObjectPostActionState = node->getAttr("sObjectPostActionState");
			AddAllStates(rule.sObjectPostActionState);

			rule.fApproachSpeed = 1.0f; // default value if not found in XML
			node->getAttr("fApproachSpeed", rule.fApproachSpeed);
			rule.iApproachStance = 4; // default value if not found in XML
			node->getAttr("iApproachStance", rule.iApproachStance);
			rule.sAnimationHelper = ""; // default value if not found in XML
			rule.sAnimationHelper = node->getAttr("sAnimationHelper");
			rule.sApproachHelper = ""; // default value if not found in XML
			rule.sApproachHelper = node->getAttr("sApproachHelper");
			rule.fStartWidth = 0.0f; // default value if not found in XML
			node->getAttr("fStartWidth", rule.fStartWidth);
			rule.fDirectionTolerance = 0.0f; // default value if not found in XML
			node->getAttr("fStartDirectionTolerance", rule.fDirectionTolerance);
			rule.fStartArcAngle = 0.0f; // default value if not found in XML
			node->getAttr("fStartArcAngle", rule.fStartArcAngle);

			if (!node->getAttr("iOrder", rule.iOrder))
				rule.iOrder = i;

			m_Conditions.push_back(rule);
		}
		else if (node->isTag("Helper"))
		{
			helper.className = node->getAttr("className");
			helper.name = node->getAttr("name");
			Vec3 pos;
			node->getAttr("pos", pos);
			Quat rot;
			node->getAttr("rot", rot);
			helper.qt = QuatT(rot, pos);
			helper.description = node->getAttr("description");

			VectorHelperData::iterator it = HelpersUpperBound(helper.className);
			m_vHelpers.insert(it, helper);
		}
		else if (node->isTag("Event"))
		{
			CEventData event;
			event.name = node->getAttr("name");
			event.description = node->getAttr("description");

			VectorEventData::iterator it = std::upper_bound(m_vEvents.begin(), m_vEvents.end(), event, less_name<CEventData>());
			m_vEvents.insert(it, event);
		}
		else if (node->isTag("State"))
		{
			CString name = node->getAttr("name");
			CString description = node->getAttr("description");
			CString location = node->getAttr("location");
			AddState(name, description, location);
		}
		else if (node->isTag("Class"))
		{
			CString name = node->getAttr("name");
			CString description = node->getAttr("description");
			CString location = node->getAttr("location");
			CString templateName = node->getAttr("template");
			AddClass(name, description, location, templateName);
		}
	}

	return true;
}

void CSmartObjectsEditorDialog::SetTemplateDefaults(SmartObjectCondition& condition, const CSOParamBase* param, CString* message /*= NULL*/) const
{
	if (!param)
		return;

	if (param->bIsGroup)
	{
		const CSOParamGroup* pGroup = static_cast<const CSOParamGroup*>(param);
		SetTemplateDefaults(condition, pGroup->pChildren, message);
	}
	else
	{
		const CSOParam* pParam = static_cast<const CSOParam*>(param);

		if (!message || !pParam->bEditable)
		{
			if (message)
			{
				IVariable* pVar = pParam->pVariable ? pParam->pVariable : gSmartObjectsUI.ResolveVariable(pParam);

				// Report altering on visible properties only
				if (gSmartObjectsUI.m_vars->IsContainsVariable(pVar))
				{
					CString temp;
					if (pParam->sName == "bNavigationRule")
						temp = condition.iRuleType == 1 ? "1" : "0";
					else if (pParam->sName == "sEvent")
						temp = condition.sEvent;
					else if (pParam->sName == "sChainedUserEvent")
						temp = condition.sChainedUserEvent;
					else if (pParam->sName == "sChainedObjectEvent")
						temp = condition.sChainedObjectEvent;
					else if (pParam->sName == "userClass")
						temp = condition.sUserClass;
					else if (pParam->sName == "userState")
						temp = condition.sUserState;
					else if (pParam->sName == "userHelper")
						temp = condition.sUserHelper;
					else if (pParam->sName == "iMaxAlertness")
						temp.Format("%d", condition.iMaxAlertness);
					else if (pParam->sName == "objectClass")
						temp = condition.sObjectClass;
					else if (pParam->sName == "objectState")
						temp = condition.sObjectState;
					else if (pParam->sName == "objectHelper")
						temp = condition.sObjectHelper;
					else if (pParam->sName == "entranceHelper")
						temp = condition.sEntranceHelper;
					else if (pParam->sName == "exitHelper")
						temp = condition.sExitHelper;
					else if (pParam->sName == "limitsDistanceFrom")
						temp.Format("%g", condition.fDistanceFrom);
					else if (pParam->sName == "limitsDistanceTo")
						temp.Format("%g", condition.fDistanceTo);
					else if (pParam->sName == "limitsOrientation")
						temp.Format("%g", condition.fOrientationLimit);
					else if (pParam->sName == "horizLimitOnly")
						temp = condition.bHorizLimitOnly ? "1" : "0";
					else if (pParam->sName == "limitsOrientationToTarget")
						temp.Format("%g", condition.fOrientationToTargetLimit);
					else if (pParam->sName == "delayMinimum")
						temp.Format("%g", condition.fMinDelay);
					else if (pParam->sName == "delayMaximum")
						temp.Format("%g", condition.fMaxDelay);
					else if (pParam->sName == "delayMemory")
						temp.Format("%g", condition.fMemory);
					else if (pParam->sName == "multipliersProximity")
						temp.Format("%g", condition.fProximityFactor);
					else if (pParam->sName == "multipliersOrientation")
						temp.Format("%g", condition.fOrientationFactor);
					else if (pParam->sName == "multipliersVisibility")
						temp.Format("%g", condition.fVisibilityFactor);
					else if (pParam->sName == "multipliersRandomness")
						temp.Format("%g", condition.fRandomnessFactor);
					else if (pParam->sName == "fLookAtOnPerc")
						temp.Format("%g", condition.fLookAtOnPerc);
					else if (pParam->sName == "userPreActionState")
						temp = condition.sUserPreActionState;
					else if (pParam->sName == "objectPreActionState")
						temp = condition.sObjectPreActionState;
					else if (pParam->sName == "actionType")
						temp.Format("%d", condition.eActionType);
					else if (pParam->sName == "actionName")
						temp = condition.sAction;
					else if (pParam->sName == "userPostActionState")
						temp = condition.sUserPostActionState;
					else if (pParam->sName == "objectPostActionState")
						temp = condition.sObjectPostActionState;
					else if (pParam->sName == "approachSpeed")
						temp.Format("%g", condition.fApproachSpeed);
					else if (pParam->sName == "approachStance")
						temp.Format("%d", condition.iApproachStance);
					else if (pParam->sName == "animationHelper")
						temp = condition.sAnimationHelper;
					else if (pParam->sName == "approachHelper")
						temp = condition.sApproachHelper;
					else if (pParam->sName == "fStartRadiusTolerance")   // backward compatibility
						temp.Format("%g", condition.fStartWidth);
					else if (pParam->sName == "fStartWidth")
						temp.Format("%g", condition.fStartWidth);
					else if (pParam->sName == "fStartDirectionTolerance")
						temp.Format("%g", condition.fDirectionTolerance);
					else if (pParam->sName == "fTargetRadiusTolerance")   // backward compatibility
						temp.Format("%g", condition.fStartArcAngle);
					else if (pParam->sName == "fStartArcAngle")
						temp.Format("%g", condition.fStartArcAngle);

					if (temp != pParam->sValue.GetString())
					{
						*message += "  - ";
						*message += pVar->GetHumanName();
						*message += ";\n";
					}
				}
			}
			else
			{
				if (pParam->sName == "bNavigationRule")
					condition.iRuleType = pParam->sValue == "1";
				else if (pParam->sName == "sEvent")
					condition.sEvent = pParam->sValue;
				else if (pParam->sName == "sChainedUserEvent")
					condition.sChainedUserEvent = pParam->sValue;
				else if (pParam->sName == "sChainedObjectEvent")
					condition.sChainedObjectEvent = pParam->sValue;
				else if (pParam->sName == "userClass")
					condition.sUserClass = pParam->sValue;
				else if (pParam->sName == "userState")
					condition.sUserState = pParam->sValue;
				else if (pParam->sName == "userHelper")
					condition.sUserHelper = pParam->sValue;
				else if (pParam->sName == "iMaxAlertness")
					condition.iMaxAlertness = atoi(pParam->sValue);
				else if (pParam->sName == "objectClass")
					condition.sObjectClass = pParam->sValue;
				else if (pParam->sName == "objectState")
					condition.sObjectState = pParam->sValue;
				else if (pParam->sName == "objectHelper")
					condition.sObjectHelper = pParam->sValue;
				else if (pParam->sName == "entranceHelper")
					condition.sEntranceHelper = pParam->sValue;
				else if (pParam->sName == "exitHelper")
					condition.sExitHelper = pParam->sValue;
				else if (pParam->sName == "limitsDistanceFrom")
					condition.fDistanceFrom = atof(pParam->sValue);
				else if (pParam->sName == "limitsDistanceTo")
					condition.fDistanceTo = atof(pParam->sValue);
				else if (pParam->sName == "limitsOrientation")
					condition.fOrientationLimit = atof(pParam->sValue);
				else if (pParam->sName == "horizLimitOnly")
					condition.bHorizLimitOnly = pParam->sValue == "1";
				else if (pParam->sName == "limitsOrientationToTarget")
					condition.fOrientationToTargetLimit = atof(pParam->sValue);
				else if (pParam->sName == "delayMinimum")
					condition.fMinDelay = atof(pParam->sValue);
				else if (pParam->sName == "delayMaximum")
					condition.fMaxDelay = atof(pParam->sValue);
				else if (pParam->sName == "delayMemory")
					condition.fMemory = atof(pParam->sValue);
				else if (pParam->sName == "multipliersProximity")
					condition.fProximityFactor = atof(pParam->sValue);
				else if (pParam->sName == "multipliersOrientation")
					condition.fOrientationFactor = atof(pParam->sValue);
				else if (pParam->sName == "multipliersVisibility")
					condition.fVisibilityFactor = atof(pParam->sValue);
				else if (pParam->sName == "multipliersRandomness")
					condition.fRandomnessFactor = atof(pParam->sValue);
				else if (pParam->sName == "fLookAtOnPerc")
					condition.fLookAtOnPerc = atof(pParam->sValue);
				else if (pParam->sName == "userPreActionState")
					condition.sUserPreActionState = pParam->sValue;
				else if (pParam->sName == "objectPreActionState")
					condition.sObjectPreActionState = pParam->sValue;
				else if (pParam->sName == "actionType")
					condition.eActionType = (EActionType) atoi(pParam->sValue);
				else if (pParam->sName == "actionName")
					condition.sAction = pParam->sValue;
				else if (pParam->sName == "userPostActionState")
					condition.sUserPostActionState = pParam->sValue;
				else if (pParam->sName == "objectPostActionState")
					condition.sObjectPostActionState = pParam->sValue;
				else if (pParam->sName == "approachSpeed")
					condition.fApproachSpeed = atof(pParam->sValue);
				else if (pParam->sName == "approachStance")
					condition.iApproachStance = atoi(pParam->sValue);
				else if (pParam->sName == "animationHelper")
					condition.sAnimationHelper = pParam->sValue;
				else if (pParam->sName == "approachHelper")
					condition.sApproachHelper = pParam->sValue;
				else if (pParam->sName == "fStartRadiusTolerance")   // backward compatibility
					condition.fStartArcAngle = atof(pParam->sValue);
				else if (pParam->sName == "fStartArcAngle")
					condition.fStartArcAngle = atof(pParam->sValue);
				else if (pParam->sName == "fStartDirectionTolerance")
					condition.fDirectionTolerance = atof(pParam->sValue);
				else if (pParam->sName == "fTargetRadiusTolerance")   // backward compatibility
					condition.fStartArcAngle = atof(pParam->sValue);
				else if (pParam->sName == "fStartArcAngle")
					condition.fStartArcAngle = atof(pParam->sValue);
			}
		}
	}

	SetTemplateDefaults(condition, param->pNext, message);
}

