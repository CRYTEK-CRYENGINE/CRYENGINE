// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements the Editor->Game communication interface.
  
 -------------------------------------------------------------------------
  History:
  - 30:8:2004   11:17 : Created by Márcio Martins

*************************************************************************/
#ifndef __EDITORGAME_H__
#define __EDITORGAME_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <CryGame/IGameRef.h>
#include <CrySandbox/IEditorGame.h>

struct IGameStartup;
class CEquipmentSystemInterface;

class CEditorGame :
	public IEditorGame
{
public:
	CEditorGame();
    CEditorGame(const char* binariesDir);
	virtual ~CEditorGame();

	virtual bool Init(ISystem *pSystem, IGameToEditorInterface *pGameToEditorInterface) override;
    virtual int Update(bool haveFocus, unsigned int updateFlags) override;
	virtual void Shutdown() override;
	virtual bool SetGameMode(bool bGameMode) override;
	virtual IEntity * GetPlayer() override;
	virtual void SetPlayerPosAng(Vec3 pos,Vec3 viewDir) override;
	virtual void HidePlayer(bool bHide) override;
	virtual void OnBeforeLevelLoad() override;
	virtual void OnAfterLevelInit(const char *levelName, const char *levelFolder) override;
	virtual void OnAfterLevelLoad(const char *levelName, const char *levelFolder) override;
	virtual void OnCloseLevel() override;
	virtual void OnSaveLevel() override;
	virtual bool BuildEntitySerializationList(XmlNodeRef output) override;
	virtual bool GetAdditionalMinimapData(XmlNodeRef output) override;

	virtual IFlowSystem * GetIFlowSystem() override;
	virtual IGameTokenSystem* GetIGameTokenSystem() override;
	virtual IEquipmentSystemInterface* GetIEquipmentSystemInterface() override;

	virtual bool IsMultiplayerGameRules() override { return m_bUsingMultiplayerGameRules; }
	virtual bool SupportsMultiplayerGameRules() override { return true; }
	virtual void ToggleMultiplayerGameRules() override;

	virtual void RegisterTelemetryTimelineRenderers(Telemetry::ITelemetryRepository* pRepository) override;

	virtual void OnDisplayRenderUpdated( bool displayHelpers ) override;
	virtual void OnEntitySelectionChanged(EntityId entityId, bool isSelected) override {}
	virtual void OnReloadScripts(EReloadScriptsType scriptsType) override {}

private:
    void InitMembers(const char* binariesDir);
    void FillSystemInitParams(SSystemInitParams &startupParams, ISystem* system);
	void InitUIEnums(IGameToEditorInterface* pGTE);
	void InitGlobalFileEnums(IGameToEditorInterface* pGTE);
	void InitActionEnums(IGameToEditorInterface* pGTE);
	void InitHUDEventEnums(IGameToEditorInterface* pGTE);
	void InitEntityClassesEnums(IGameToEditorInterface* pGTE);
	void InitLevelTypesEnums(IGameToEditorInterface* pGTE);
	void InitEntityArchetypeEnums(IGameToEditorInterface* pGTE, const char* levelFolder = NULL, const char* levelName = NULL);
	void InitForceFeedbackEnums(IGameToEditorInterface* pGTE);
	void InitActionInputEnums(IGameToEditorInterface* pGTE);
	void InitReadabilityEnums(IGameToEditorInterface* pGTE);
	void InitActionMapsEnums(IGameToEditorInterface* pGTE);
	void InitLedgeTypeEnums(IGameToEditorInterface* pGTE);
	void InitSmartMineTypeEnums(IGameToEditorInterface* pGTE);
	void InitDamageTypeEnums(IGameToEditorInterface *pGTE);
	void InitDialogBuffersEnum(IGameToEditorInterface* pGTE);
	void InitTurretEnum(IGameToEditorInterface* pGTE);
	void InitDoorPanelEnum(IGameToEditorInterface* pGTE);
	void InitModularBehaviorTreeEnum(IGameToEditorInterface* pGTE);

	bool ConfigureNetContext( bool on );
	static void OnChangeEditorMode( ICVar * );
	void EnablePlayer(bool bPlayer);
	static void ResetClient(IConsoleCmdArgs*);
	static const char *GetGameRulesName();
	void ScanBehaviorTrees(const string& folderName, std::vector<string>& behaviorTrees);

	IGameRef m_pGame;
	IGameStartup* m_pGameStartup;
	CEquipmentSystemInterface* m_pEquipmentSystemInterface;

	bool m_bEnabled;
	bool m_bGameMode;
	bool m_bPlayer;
	bool m_bUsingMultiplayerGameRules;

	const char* m_binariesDir;
	IGameToEditorInterface* m_pGTE;

	static ICVar* s_pEditorGameMode;
	static CEditorGame* s_pEditorGame;
	
};


#endif //__EDITORGAME_H__
