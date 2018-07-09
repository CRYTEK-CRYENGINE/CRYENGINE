// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __aimanager_h__
#define __aimanager_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <IAIManager.h>

#include "Util/Variable.h"

#include <CryAISystem/IAgent.h>
#include <CryAISystem/INavigationSystem.h>
#include <CryAISystem/NavigationSystem/INavigationUpdatesManager.h>
#include <EditorFramework/Preferences.h>
#include <CrySerialization/yasli/decorators/Range.h>
#include "IEditor.h"

enum class ENavigationUpdateType
{
	Continuous,
	AfterStabilization,
	Disabled,
};

// forward declarations.
class CAIGoalLibrary;
class CAIBehaviorLibrary;

class CProgressNotification;

class CSOParamBase
{
public:
	CSOParamBase* pNext;
	IVariable*    pVariable;
	bool          bIsGroup;

	CSOParamBase(bool isGroup) : pNext(0), pVariable(0), bIsGroup(isGroup) {}
	virtual ~CSOParamBase() { if (pNext) delete pNext; }

	void PushBack(CSOParamBase* pParam)
	{
		if (pNext)
			pNext->PushBack(pParam);
		else
			pNext = pParam;
	}
};

class CSOParam : public CSOParamBase
{
public:
	CSOParam() : CSOParamBase(false), bVisible(true), bEditable(true) {}

	string sName;     // the internal name
	string sCaption;  // property name as it is shown in the editor
	bool   bVisible;  // is the property visible
	bool   bEditable; // should the property be enabled
	string sValue;    // default value
	string sHelp;     // description of the property
};

class CSOParamGroup : public CSOParamBase
{
public:
	CSOParamGroup() : CSOParamBase(true), pChildren(0), bExpand(true) {}
	virtual ~CSOParamGroup() { if (pChildren) delete pChildren; if (pVariable) pVariable->Release(); }

	string        sName;   // property group name
	bool          bExpand; // is the group expanded by default
	string        sHelp;   // description
	CSOParamBase* pChildren;
};

class CSOTemplate
{
public:
	CSOTemplate() : params(0) {}
	~CSOTemplate() { if (params) delete params; }

	int           id;          // id used in the rules to reference this template
	string        name;        // name of the template
	string        description; // description
	CSOParamBase* params;      // pointer to the first param
};

// map of all known smart object templates mapped by template id
typedef std::map<int, CSOTemplate*> MapTemplates;

class CCoverSurfaceManager;

// Preferences
struct SAINavigationPreferences : public SPreferencePage
{
	SAINavigationPreferences()
		: SPreferencePage("Navigation", "AI/Navigation")
		, m_navigationDebugAgentType(0)
		, m_navigationShowAreas(true)
		, m_navigationUpdateType(ENavigationUpdateType::AfterStabilization)
		, m_navigationDebugDisplay(false)
		, m_visualizeNavigationAccessibility(false)
		, m_navigationRegenDisabledOnLevelLoad(true)
		, m_initialNavigationAreaHeightOffset(-0.05f)
	{
	}

	virtual bool Serialize(yasli::Archive& ar) override
	{
		INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

		IF_UNLIKELY(pNavigationSystem == nullptr)
		{
			return false;
		}

		const size_t agentTypeCount = pNavigationSystem->GetAgentTypeCount();
		Serialization::StringList agentTypeList;

		for (int agentIndex = 0; agentIndex < agentTypeCount; ++agentIndex)
		{
			const NavigationAgentTypeID currentAgentID = pNavigationSystem->GetAgentTypeID(agentIndex);
			const char *szCurrentAgentName = pNavigationSystem->GetAgentTypeName(currentAgentID);
			agentTypeList.push_back(szCurrentAgentName);
		}

		ar.openBlock("general", "General");
		ar(m_navigationShowAreas, "navigationShowAreas", "");
		ar(m_visualizeNavigationAccessibility, "visualizeNavigationAccessibility", "");
		ar(m_navigationUpdateType, "navigationUpdateType", "");
		ar(m_navigationDebugDisplay, "navigationDebugDisplay", "");
		ar(m_navigationRegenDisabledOnLevelLoad, "navigationRegenDisabledOnLevelLoad", "Disable Navigation Regeneration on Level Load");
		ar(m_initialNavigationAreaHeightOffset, "initNavAreaHeightOffset", "Initial Navigation Area Height Offset");
		ar.doc("Initial navigation area height offset from the terrain when the area is created.");
		
		if (agentTypeCount > 0)
		{
			Serialization::StringListValue agentTypeValue(agentTypeList, m_navigationDebugAgentType);
			ar(agentTypeValue, "navigationDebugAgentType", "");

			if (ar.isInput())
			{
				m_navigationDebugAgentType = agentTypeValue.index();
				signalSettingsChanged();
			}
		}
		else
		{
			ar.warning(*this, "No navigation agent types registered. Check your configuration.");
		}
		ar.closeBlock();

		return true;
	}

	ADD_PREFERENCE_PAGE_PROPERTY(int, navigationDebugAgentType, setNavigationDebugAgentType)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, navigationShowAreas, setNavigationShowAreas)
	ADD_PREFERENCE_PAGE_PROPERTY(ENavigationUpdateType, navigationUpdateType, setNavigationUpdateType)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, navigationRegenDisabledOnLevelLoad, setNavigationRegenDisabledOnLevelLoad)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, navigationDebugDisplay, setNavigationDebugDisplay)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, visualizeNavigationAccessibility, setVisualizeNavigationAccessibility)
	ADD_PREFERENCE_PAGE_PROPERTY(float, initialNavigationAreaHeightOffset, setInitialNavigationAreaHeightOffset)
};

//////////////////////////////////////////////////////////////////////////
class CAIManager : public IAIManager, public IAutoEditorNotifyListener
{
protected:

	//! Navigation WorldMonitor reacts to physics events and schedules NavMesh regeneration.
	//! It has two states: Started and Stopped. But on editor side we also want to track, when
	//! we need to temporarily pause it and when to resume - hence the additional states.
	enum class ENavigationWorldMonitorState
	{
		Stopped,                       //!< World monitor is stopped and not listening for physics updates.
		Started,                       //!< World monitor is started and listening for physics updates.
		StoppedAndPaused,              //!< World monitor is stopped and pause was also requested.
		StartedAndPaused,              //!< World monitor was started, but then temporarily paused - in effect it's stopped right now and not listening for physics updates.
		StartedPausedAndResumePending, //!< World monitor is paused right now, but will be started on the next update.
		                               //!< This way, we can skip physics events overflowing world monitor right after level load.

		StatesCount
	};

public:
	CAIManager();
	~CAIManager() override;
	void Init(ISystem* system);

	void Update(uint32 updateFlags);

	// IAutoEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	// !IAutoEditorNotifyListener

	// IAIManager
	virtual void                OnAreaModified(const AABB& aabb, const CBaseObject* modifiedByObject = nullptr) override;
	virtual bool                IsReadyToGameExport(unsigned int& adjustedExportFlags) const override;

	virtual bool                NewAction(string& filename) override;
	virtual const MapTemplates& GetMapTemplates() const override { return m_mapTemplates; }
	virtual void                GetSmartObjectActions(std::vector<string>& values) const override;
	// !IAIManager

	IAISystem*            GetAISystem();
	CAIBehaviorLibrary*   GetBehaviorLibrary()     { return m_pBehaviorLibrary; };
	CCoverSurfaceManager* GetCoverSurfaceManager() { return m_coverSurfaceManager.get(); };

	//////////////////////////////////////////////////////////////////////////
	//! Smart Objects States and Actions enumeration
	void GetSmartObjectStates(std::vector<string>& values) const;
	void AddSmartObjectState(const char* sState);

	//////////////////////////////////////////////////////////////////////////
	//! AI Anchor Actions enumeration.
	void GetAnchorActions(std::vector<string>& actions) const;
	int  AnchorActionToId(const char* sAction) const;

	//////////////////////////////////////////////////////////////////////////
	void        ReloadScripts();
	void        ReloadActionGraphs();
	void        SaveActionGraphs();
	void        SaveAndReloadActionGraphs();

	const char* GetSmartObjectTemplateName(int index) const {}

	// Pathfinding properties
	void                              AssignPFPropertiesToPathType(const string& sPathType, const AgentPathfindingProperties& properties);
	const AgentPathfindingProperties* GetPFPropertiesOfPathType(const string& sPathType);
	string                            GetPathTypeNames();
	string                            GetPathTypeName(int index);
	size_t                            GetNumPathTypes() const { return m_mapPFProperties.size(); }

	void                              OnEnterGameMode(bool inGame);

	void                              NavigationContinuousUpdate();
	void                              RebuildAINavigation();
	void                              PauseNavigationUpdating();
	void                              RestartNavigationUpdating();

	bool                              IsNavigationFullyGenerated() const;

	size_t                            GetNavigationAgentTypeCount() const;
	const char*                       GetNavigationAgentTypeName(size_t i) const;
	void                              OnNavigationDebugDisplayAgentTypeChanged();
	bool                              GetNavigationDebugDisplayAgent(size_t* index) const;

	void                              CalculateNavigationAccessibility();
	void                              OnVisualizeNavigationAccessibilityChanged();
	void                              OnAINavigationPreferencesChanged();

	void                              NavigationDebugDisplay();

	void                              LoadNavigationEditorSettings();

	void                              LateUpdate();
	void                              EarlyUpdate();

	void                              SetNavigationUpdateType(ENavigationUpdateType updateType);
	void                              OnNavigationUpdateChanged();

	void                              RegenerateNavigationByTypeName(const char* szType);

private:
	void               OnHideMaskChanged();

	void               EnumAnchorActions();

	void               LoadActionGraphs();
	void               FreeActionGraphs();

	void               FreeTemplates();
	bool               ReloadTemplates();
	CSOParamBase*      LoadTemplateParams(XmlNodeRef root) const;

	void               OnNavigationMeshChanged(NavigationAgentTypeID agentTypeId, NavigationMeshID navigationMeshId, uint32 i);

	void               StartNavigationWorldMonitor();
	void               StopNavigationWorldMonitor();
	void               PauseNavigationWorldMonitor();
	void               RequestResumeNavigationWorldMonitorOnNextUpdate();
	void               StartNavigationWorldMonitorIfNeeded();
	void               SetNavigationWorldMonitorState(const ENavigationWorldMonitorState newState);
	static const char* GetNavigationWorldMonitorStateName(const ENavigationWorldMonitorState state);

	void               PauseMNMRegeneration();
	void               ResumeMNMRegeneration(bool updateChangedVolumes = true);

	// Resume navigation regeneration updating but without updating the changes that were made to NavMesh areas, when the navigation generation was disabled.
	void               ResumeMNMRegenerationWithoutUpdatingPengingNavMeshChanges();

	MapTemplates                        m_mapTemplates;

	CAIBehaviorLibrary*                 m_pBehaviorLibrary;
	IAISystem*                          m_pAISystem;

	std::auto_ptr<CCoverSurfaceManager> m_coverSurfaceManager;

	//! AI Anchor Actions.
	friend struct CAIAnchorDump;
	typedef std::map<string, int> AnchorActions;
	AnchorActions   m_anchorActions;

	typedef std::map<string, AgentPathfindingProperties> PFPropertiesMap;
	PFPropertiesMap              m_mapPFProperties;

	ENavigationUpdateType        m_currentUpdateType;
	ENavigationWorldMonitorState m_navigationWorldMonitorState;
	int                          m_MNMRegenerationPausedCount;
	bool                         m_pendingNavigationCalculateAccessibility;
	bool                         m_refreshMnmOnGameExit;
	bool                         m_resumeMNMRegenWhenPumpedPhysicsEventsAreFinished;

	bool                         m_navigationUpdatePaused;

	struct SNavigationUpdateProgress
	{
		SNavigationUpdateProgress() :
			m_pUpdateProgressNotification(nullptr),
			m_queueInitSize(0)
		{}

		~SNavigationUpdateProgress();

		void Update(const INavigationSystem* pNavigationSystem, bool paused);

		CProgressNotification* m_pUpdateProgressNotification;
		uint32                 m_queueInitSize;
	}
	m_navigationUpdateProgress;
};

extern SAINavigationPreferences gAINavigationPreferences;

#endif // __aimanager_h__

