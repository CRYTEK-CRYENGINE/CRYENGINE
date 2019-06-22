// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QtMainFrame.h"

#include "Commands/CommandManager.h"
#include "Commands/KeybindEditor.h"
#include "QT/QMainFrameMenuBar.h"
#include "QT/QtMainFrameWidgets.h"
#include "QT/QToolTabManager.h"
#include "QT/TraySearchBox.h"
#include "AboutDialog.h"
#include "CryEdit.h"
#include "CryEditDoc.h"
#include "LevelIndependentFileMan.h"
#include "LinkTool.h"
#include "LogFile.h"

// MFC
#include <QMfcApp/qmfchost.h>

// EditorCommon
#include <Controls/SaveChangesDialog.h>
#include <EditorFramework/BroadcastManager.h>
#include <EditorFramework/Events.h>
#include <EditorFramework/PersonalizationManager.h>
#include <EditorFramework/ToolBar/ToolBarCustomizeDialog.h>
#include <EditorFramework/WidgetsGlobalActionRegistry.h>
#include <Menu/MenuWidgetBuilders.h>
#include <Preferences/GeneralPreferences.h>
#include <QTrackingTooltip.h>
#include <QtUtil.h>
#include <RenderViewport.h>

#include <CrySystem/IProjectManager.h>

#include <QMenuBar>
#include <QMouseEvent>
#include <QToolBar>
#include <QToolWindowManager/QToolWindowRollupBarArea.h>

#include <algorithm>

REGISTER_HIDDEN_VIEWPANE_FACTORY(QPreferencesDialog, "Preferences", "Editor", true)

struct SPerformancePreferences : public SPreferencePage
{
	SPerformancePreferences()
		: SPreferencePage("Performance", "General/General")
		, userInputPriorityTime(30)
	{
	}

	virtual bool Serialize(yasli::Archive& ar) override
	{
		ar.openBlock("performance", "Performance");
		ar(yasli::Range(userInputPriorityTime, 0, 10000), "userInputPriorityFramerate", "Priority To User Input after (ms)");
		ar.closeBlock();

		return true;
	}

	int userInputPriorityTime;
};

SPerformancePreferences gPerformancePreferences;
REGISTER_PREFERENCES_PAGE_PTR(SPerformancePreferences, &gPerformancePreferences)

//////////////////////////////////////////////////////////////////////////

CEditorMainFrame * CEditorMainFrame::m_pInstance = nullptr;

namespace
{
CTabPaneManager*              s_pToolTabManager = nullptr;
CWidgetsGlobalActionRegistry* s_pWidgetGlobalActionRegistry = nullptr;
}

//////////////////////////////////////////////////////////////////////////
class QMainFrameWidget : public QWinHost
{
public:
	QMainFrameWidget(QWidget* parent) : QWinHost(parent)
	{
		setWindow(AfxGetMainWnd()->GetSafeHwnd());
	}
	virtual QSize sizeHint() const { return QSize(300, 300); }
};

class CToolWindowManagerClassFactory : public QToolWindowManagerClassFactory
{
public:
	virtual IToolWindowWrapper* createWrapper(QToolWindowManager* manager) Q_DECL_OVERRIDE
	{
		return new QSandboxWrapper(manager);
	}

	virtual QSplitter* createSplitter(QToolWindowManager* manager) Q_DECL_OVERRIDE
	{
		return new QNotifierSplitter;
	}

	virtual IToolWindowArea* createArea(QToolWindowManager* manager, QWidget* parent = 0, QTWMWrapperAreaType areaType = watTabs) Q_DECL_OVERRIDE
	{
		if (manager->config().value(QTWM_SUPPORT_SIMPLE_TOOLS, false).toBool() && areaType == watRollups)
			return new QToolWindowRollupBarArea(manager, parent);
		else
			return new QToolsMenuToolWindowArea(manager, parent);
	}
};

class Ui_MainWindow
{
	class CMenu : public QMenu
	{
	public:
		CMenu(QWidget* pParent = nullptr)
			: QMenu(pParent)
		{
		}

		QCommandAction* AddCommand(const char* szCommand)
		{
			QCommandAction* pAction = GetIEditorImpl()->GetCommandManager()->GetCommandAction(szCommand);
			if (pAction)
			{
				addAction(pAction);
			}

			CRY_ASSERT_MESSAGE(pAction, "Trying to add unexisting command to menu: %s", szCommand);

			return pAction;
		}
	};

public:
	QAction*  actionShow_Log_File;
	QAction*  actionRuler;
	QAction*  actionReduce_Working_Set;
	QAction*  actionVery_High;
	QAction*  actionHigh;
	QAction*  actionMedium;
	QAction*  actionLow;
	QAction*  actionXBox_One;
	QAction*  actionPS4;
	QAction*  actionCustom;
	QAction*  actionMute_Audio;
	QAction*  actionStop_All_Sounds;
	QAction*  actionRefresh_Audio;
	QWidget*  centralwidget;
	QMenuBar* menubar;
	CMenu*    menuFile;
	CMenu*    menuFileNew;
	CMenu*    menuFileOpen;
	QMenu*    menuRecent_Files;
	CMenu*    menuEdit;
	CMenu*    menuEditing_Mode;
	CMenu*    menuConstrain;
	CMenu*    menuFast_Rotate;
	CMenu*    menuAlign_Snap;
	CMenu*    menuLevel;
	CMenu*    menuPhysics;
	CMenu*    menuGroup;
	CMenu*    menuLink;
	CMenu*    menuPrefabs;
	CMenu*    menuSelection;
	CMenu*    menuMaterial;
	CMenu*    menuLighting;
	CMenu*    menuDisplay;
	CMenu*    menuLocation;
	CMenu*    menuRemember_Location;
	CMenu*    menuGoto_Location;
	CMenu*    menuSwitch_Camera;
	CMenu*    menuGame;
	CMenu*    menuTools;
	CMenu*    menuLayout;
	CMenu*    menuHelp;
	CMenu*    menuGraphics;
	CMenu*    menuDisplayInfo;
	CMenu*    menuAudio;
	CMenu*    menuAI;
	CMenu*    menuAINavigationUpdate;
	CMenu*    menuDebug;
	CMenu*    menuReload_Scripts;
	CMenu*    menuAdvanced;
	CMenu*    menuCoordinates;
	QMenu*    menuSelection_Mask;
	QMenu*    menuDebug_Agent_Type;
	QMenu*    menuRegenerate_MNM_Agent_Type;

	void setupUi(QMainWindow* MainWindow)
	{
		if (MainWindow->objectName().isEmpty())
			MainWindow->setObjectName(QStringLiteral("MainWindow"));
		MainWindow->resize(1172, 817);
		actionShow_Log_File = new QCommandAction(nullptr, nullptr, MainWindow);
		actionShow_Log_File->setObjectName(QStringLiteral("actionShow_Log_File"));
		actionRuler = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRuler->setObjectName(QStringLiteral("actionRuler"));
		actionRuler->setIcon(CryIcon("icons:Tools/tools-ruler.ico"));
		actionRuler->setCheckable(true);

		string name("Snap Settings");
		string command;
		command.Format("general.open_pane '%s'", name);
		QCommandAction* pSnapSettings = new QCommandAction(QtUtil::ToQString(name) + "...", command.c_str(), menuAlign_Snap);
		pSnapSettings->setIcon(CryIcon("icons:Viewport/viewport-snap-options.ico"));
		actionReduce_Working_Set = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReduce_Working_Set->setObjectName(QStringLiteral("actionReduce_Working_Set"));

		QActionGroup* renderQualityGroup = new QActionGroup(menuGraphics);
		actionVery_High = new QCommandAction(nullptr, nullptr, MainWindow);
		actionVery_High->setObjectName(QStringLiteral("actionVery_High"));
		actionVery_High->setCheckable(true);
		actionVery_High->setActionGroup(renderQualityGroup);
		actionHigh = new QCommandAction(nullptr, nullptr, MainWindow);
		actionHigh->setObjectName(QStringLiteral("actionHigh"));
		actionHigh->setCheckable(true);
		actionHigh->setActionGroup(renderQualityGroup);
		actionMedium = new QCommandAction(nullptr, nullptr, MainWindow);
		actionMedium->setObjectName(QStringLiteral("actionMedium"));
		actionMedium->setCheckable(true);
		actionMedium->setActionGroup(renderQualityGroup);
		actionLow = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLow->setObjectName(QStringLiteral("actionLow"));
		actionLow->setCheckable(true);
		actionLow->setActionGroup(renderQualityGroup);
		actionXBox_One = new QCommandAction(nullptr, nullptr, MainWindow);
		actionXBox_One->setObjectName(QStringLiteral("actionXBox_One"));
		actionXBox_One->setCheckable(true);
		actionXBox_One->setActionGroup(renderQualityGroup);
		actionPS4 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionPS4->setObjectName(QStringLiteral("actionPS4"));
		actionPS4->setCheckable(true);
		actionPS4->setActionGroup(renderQualityGroup);
		actionCustom = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCustom->setObjectName(QStringLiteral("actionCustom"));
		actionCustom->setCheckable(true);
		actionCustom->setActionGroup(renderQualityGroup);

		actionMute_Audio = new QCommandAction(nullptr, nullptr, MainWindow);
		actionMute_Audio->setObjectName(QStringLiteral("actionMute_Audio"));
		actionMute_Audio->setIcon(CryIcon("icons:Audio/Mute_Audio.ico"));
		actionMute_Audio->setCheckable(true);
		actionStop_All_Sounds = new QCommandAction(nullptr, nullptr, MainWindow);
		actionStop_All_Sounds->setObjectName(QStringLiteral("actionStop_All_Sounds"));
		actionStop_All_Sounds->setIcon(CryIcon("icons:Audio/Stop_All_Sounds.ico"));
		actionRefresh_Audio = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRefresh_Audio->setObjectName(QStringLiteral("actionRefresh_Audio"));
		actionRefresh_Audio->setIcon(CryIcon("icons:Audio/Refresh_Audio.ico"));
		centralwidget = new QWidget(MainWindow);
		centralwidget->setObjectName(QStringLiteral("centralwidget"));
		MainWindow->setCentralWidget(centralwidget);
		menubar = new QMenuBar(MainWindow);
		menubar->setObjectName(QStringLiteral("menubar"));
		menubar->setGeometry(QRect(0, 0, 1172, 21));
		menuFile = new CMenu(menubar);
		menuFile->setObjectName(QStringLiteral("menuFile"));
		menuRecent_Files = new QMenu(menuFile);
		menuRecent_Files->setObjectName(QStringLiteral("menuRecent_Files"));
		menuEdit = new CMenu(menubar);
		menuEdit->setObjectName(QStringLiteral("menuEdit"));
		menuEditing_Mode = new CMenu(menuEdit);
		menuEditing_Mode->setObjectName(QStringLiteral("menuEditing_Mode"));
		menuConstrain = new CMenu(menuEdit);
		menuConstrain->setObjectName(QStringLiteral("menuConstrain"));
		menuFast_Rotate = new CMenu(menuEdit);
		menuFast_Rotate->setObjectName(QStringLiteral("menuFast_Rotate"));
		menuAlign_Snap = new CMenu(menuEdit);
		menuAlign_Snap->setObjectName(QStringLiteral("menuAlign_Snap"));

		menuLevel = new CMenu(menubar);
		menuLevel->setObjectName(QStringLiteral("menuLevel"));
		menuPhysics = new CMenu(menuLevel);
		menuPhysics->setObjectName(QStringLiteral("menuPhysics"));
		menuGroup = new CMenu(menuLevel);
		menuGroup->setObjectName(QStringLiteral("menuGroup"));
		menuLink = new CMenu(menuLevel);
		menuLink->setObjectName(QStringLiteral("menuLink"));
		menuPrefabs = new CMenu(menuLevel);
		menuPrefabs->setObjectName(QStringLiteral("menuPrefabs"));
		menuSelection = new CMenu(menuLevel);
		menuSelection->setObjectName(QStringLiteral("menuSelection"));
		menuSelection_Mask = new QSelectionMaskMenu();
		menuSelection_Mask->setObjectName(QStringLiteral("menuSelection_Mask"));
		menuLighting = new CMenu(menuLevel);
		menuLighting->setObjectName(QStringLiteral("menuLighting"));
		menuMaterial = new CMenu(menuLevel);
		menuMaterial->setObjectName(QStringLiteral("menuMaterial"));

		menuDisplay = new CMenu(menubar);
		menuDisplay->setObjectName(QStringLiteral("menuDisplay"));
		menuLocation = new CMenu(menuDisplay);
		menuLocation->setObjectName(QStringLiteral("menuLocation"));
		menuRemember_Location = new CMenu(menuLocation);
		menuRemember_Location->setObjectName(QStringLiteral("menuRemember_Location"));
		menuGoto_Location = new CMenu(menuLocation);
		menuGoto_Location->setObjectName(QStringLiteral("menuGoto_Location"));
		menuSwitch_Camera = new CMenu(menuDisplay);
		menuSwitch_Camera->setObjectName(QStringLiteral("menuSwitch_Camera"));
		menuGame = new CMenu(menubar);
		menuGame->setObjectName(QStringLiteral("menuGame"));
		menuTools = new CMenu(menubar);
		menuTools->setObjectName(QStringLiteral("menuTools"));
		menuLayout = new CMenu(menubar);
		menuLayout->setObjectName(QStringLiteral("menuLayout"));
		menuHelp = new CMenu(menubar);
		menuHelp->setObjectName(QStringLiteral("menuHelp"));
		menuGraphics = new CMenu(menuDisplay);
		menuGraphics->setObjectName(QStringLiteral("menuGraphics"));
		menuDisplayInfo = new CMenu(menuDisplay);
		menuDisplayInfo->setObjectName(QStringLiteral("menuDisplayInfo"));
		menuDisplayInfo->setIcon(CryIcon("icons:Viewport/viewport-displayinfo.ico"));
		menuAudio = new CMenu(menuGame);
		menuAudio->setObjectName(QStringLiteral("menuAudio"));
		menuAI = new CMenu(menuGame);
		menuAI->setObjectName(QStringLiteral("menuAI"));
		menuAINavigationUpdate = new CMenu(menuAI);
		menuAINavigationUpdate->setObjectName(QStringLiteral("menuAINavigationUpdate"));
		menuDebug = new CMenu(menubar);
		menuDebug->setObjectName(QStringLiteral("menuDebug"));
		menuDebug_Agent_Type = new QNavigationAgentTypeMenu();
		menuDebug_Agent_Type->setObjectName(QStringLiteral("menuDebug_Agent_Type"));
		menuRegenerate_MNM_Agent_Type = new QMNMRegenerationAgentTypeMenu();
		menuRegenerate_MNM_Agent_Type->setObjectName(QStringLiteral("menuRegenerate_MNM_Agent_Type"));
		menuReload_Scripts = new CMenu(menuDebug);
		menuReload_Scripts->setObjectName(QStringLiteral("menuReload_Scripts"));
		menuAdvanced = new CMenu(menuDebug);
		menuAdvanced->setObjectName(QStringLiteral("menuAdvanced"));
		menuCoordinates = new CMenu(menuEdit);
		menuCoordinates->setObjectName(QStringLiteral("menuCoordinates"));
		MainWindow->setMenuBar(menubar);

		menubar->addAction(menuFile->menuAction());
		menubar->addAction(menuEdit->menuAction());
		menubar->addAction(menuLevel->menuAction());
		menubar->addAction(menuDisplay->menuAction());
		menubar->addAction(menuGame->menuAction());
		menubar->addAction(menuDebug->menuAction());
		menubar->addAction(menuTools->menuAction());
		menubar->addAction(menuLayout->menuAction());
		menubar->addAction(menuHelp->menuAction());

		menuFileNew = new CMenu(menuFile);
		menuFileNew->AddCommand("project.new");
		QCommandAction* pCommandAciton = GetIEditor()->GetICommandManager()->CreateNewAction("general.new");
		pCommandAciton->setText("Level...");
		menuFileNew->addAction(pCommandAciton);
		menuFile->addAction(menuFileNew->menuAction());

		menuFileOpen = new CMenu(menuFile);
		menuFileOpen->AddCommand("project.open");
		pCommandAciton = GetIEditor()->GetICommandManager()->CreateNewAction("general.open");
		pCommandAciton->setText("Level...");
		menuFileOpen->addAction(pCommandAciton);
		menuFile->addAction(menuFileOpen->menuAction());

		menuFile->AddCommand("general.save");
		menuFile->AddCommand("general.save_as");
		menuFile->addSeparator();
		menuFile->AddCommand("asset.toggle_browser");
		menuFile->addSeparator();
		menuFile->AddCommand("exporter.export_to_engine");
		menuFile->AddCommand("exporter.export_occlusion_mesh");
		menuFile->AddCommand("exporter.export_svogi_data");
		menuFile->addSeparator();
		menuFile->addAction(menuRecent_Files->menuAction());
		menuFile->addSeparator();
		menuFile->AddCommand("general.exit");
		menuEdit->AddCommand("general.undo");
		menuEdit->AddCommand("general.redo");
		menuEdit->addSeparator();
		menuEdit->AddCommand("general.delete");
		menuEdit->AddCommand("general.rename");
		menuEdit->AddCommand("general.duplicate");
		menuEdit->AddCommand("general.copy");
		menuEdit->AddCommand("general.cut");
		menuEdit->AddCommand("general.paste");
		menuEdit->addSeparator();
		menuEdit->AddCommand("general.find");
		menuEdit->addAction(menuAlign_Snap->menuAction());
		menuEdit->addAction(menuEditing_Mode->menuAction());
		menuEdit->addAction(menuConstrain->menuAction());
		menuEdit->addAction(menuFast_Rotate->menuAction());
		menuEdit->addSeparator();
		menuEdit->addMenu(menuCoordinates);
		menuEdit->addSeparator();
		// TODO: State tracking for edit mode should be improved and this action group should not be necessary
		QActionGroup* pActionGroup = new QActionGroup(MainWindow);
		pActionGroup->addAction(menuEditing_Mode->AddCommand("tools.select"));
		pActionGroup->addAction(menuEditing_Mode->AddCommand("tools.move"));
		pActionGroup->addAction(menuEditing_Mode->AddCommand("tools.rotate"));
		pActionGroup->addAction(menuEditing_Mode->AddCommand("tools.scale"));
		menuConstrain->AddCommand("tools.enable_x_axis_constraint");
		menuConstrain->AddCommand("tools.enable_y_axis_constraint");
		menuConstrain->AddCommand("tools.enable_z_axis_constraint");
		menuConstrain->AddCommand("tools.enable_xy_axis_constraint");
		menuFast_Rotate->AddCommand("tools.fast_rotate_x");
		menuFast_Rotate->AddCommand("tools.fast_rotate_y");
		menuFast_Rotate->AddCommand("tools.fast_rotate_z");
		menuFast_Rotate->AddCommand("tools.set_fast_rotate_angle");
		menuAlign_Snap->AddCommand("level.align_to_grid");
		menuAlign_Snap->AddCommand("level.toggle_snap_to_pivot");
		menuAlign_Snap->addSeparator();
		menuAlign_Snap->AddCommand("level.toggle_snap_to_grid");
		menuAlign_Snap->AddCommand("level.toggle_snap_to_angle");
		menuAlign_Snap->AddCommand("level.toggle_snap_to_scale");
		menuAlign_Snap->AddCommand("level.toggle_snap_to_vertex");
		menuAlign_Snap->AddCommand("level.toggle_snap_to_terrain");
		menuAlign_Snap->AddCommand("level.toggle_snap_to_geometry");
		menuAlign_Snap->AddCommand("level.toggle_snap_to_surface_normal");
		menuAlign_Snap->addAction(pSnapSettings);
		menuLevel->AddCommand("object.save_to_grp");
		menuLevel->AddCommand("object.load_from_grp");
		menuLevel->addSeparator();
		menuLevel->addAction(menuLink->menuAction());
		menuLevel->addAction(menuGroup->menuAction());
		menuLevel->addAction(menuPrefabs->menuAction());
		menuLevel->addSeparator();
		menuLevel->addAction(menuSelection->menuAction());

		{
			//Intentionally using new menu syntax instead of mimicking generated code
			CAbstractMenu builder;

			builder.CreateCommandAction("layer.hide_all");
			builder.CreateCommandAction("layer.unhide_all");

			const int sec = builder.GetNextEmptySection();

			builder.CreateCommandAction("layer.lock_all", sec);
			builder.CreateCommandAction("layer.unlock_all", sec);
			builder.CreateCommandAction("layer.lock_read_only_layers", sec);

			QMenu* menuLayers = menuLevel->addMenu(QObject::tr("Layers"));
			builder.Build(MenuWidgetBuilders::CMenuBuilder(menuLayers));
		}

		menuLevel->addAction(menuMaterial->menuAction());
		menuLevel->addAction(menuLighting->menuAction());
		menuLevel->addAction(menuPhysics->menuAction());
		menuLevel->addSeparator();
		menuLevel->AddCommand("level.go_to_position");
		menuLevel->AddCommand("selection.go_to");
		menuLevel->addSeparator();
		menuLevel->addAction(actionRuler);
		menuLevel->addSeparator();
		menuLevel->AddCommand("exporter.save_level_resources");
		menuLevel->AddCommand("exporter.export_selected_objects");

		menuPhysics->AddCommand("physics.get_state_selection");
		menuPhysics->AddCommand("physics.reset_state_selection");
		menuPhysics->AddCommand("physics.simulate_objects");
		menuPhysics->AddCommand("physics.generate_joints");
		menuGroup->AddCommand("group.create_from_selection");
		menuGroup->AddCommand("group.ungroup");
		menuGroup->addSeparator();
		menuGroup->AddCommand("group.open");
		menuGroup->AddCommand("group.close");
		menuGroup->addSeparator();
		menuGroup->AddCommand("group.attach_to");
		menuGroup->AddCommand("group.detach");
		menuGroup->AddCommand("group.detach_from_hierarchy");
		menuLink->AddCommand("tools.link");
		menuLink->AddCommand("tools.unlink");
		menuPrefabs->AddCommand("prefab.create_from_selection");
		menuPrefabs->AddCommand("prefab.add_to_prefab");
		menuPrefabs->addSeparator();
		menuPrefabs->AddCommand("prefab.open");
		menuPrefabs->AddCommand("prefab.close");
		menuPrefabs->addSeparator();
		menuPrefabs->AddCommand("prefab.open_all");
		menuPrefabs->AddCommand("prefab.close_all");
		menuPrefabs->AddCommand("prefab.reload_all");
		menuPrefabs->addSeparator();
		menuPrefabs->AddCommand("prefab.extract_all");
		menuPrefabs->AddCommand("prefab.clone_all");
		menuPrefabs->addSeparator();
		menuSelection->AddCommand("general.select_all");
		menuSelection->AddCommand("selection.clear");
		menuSelection->AddCommand("selection.invert");
		menuSelection->AddCommand("selection.lock");
		menuSelection->addMenu(menuSelection_Mask);
		menuSelection->addSeparator();
		menuSelection->AddCommand("selection.hide_objects");
		menuSelection->AddCommand("object.show_all");
		menuSelection->addSeparator();
		menuSelection->AddCommand("selection.lock_objects");
		menuSelection->AddCommand("object.unlock_all");
		menuMaterial->AddCommand("material.assign_current_to_selection");
		menuMaterial->AddCommand("material.reset_selection");
		menuMaterial->AddCommand("material.set_current_from_object");
		menuMaterial->AddCommand("tools.pick_material");
		menuLighting->AddCommand("object.generate_all_cubemaps");
		menuDisplay->AddCommand("viewport.toggle_wireframe_mode");
		menuDisplay->addAction(menuGraphics->menuAction());
		menuDisplay->addSeparator();
		menuDisplay->addAction(menuDisplayInfo->menuAction());
		menuDisplayInfo->AddCommand("level.toggle_display_info");
		menuDisplayInfo->addSeparator();
		menuDisplayInfo->AddCommand("level.display_info_low");
		menuDisplayInfo->AddCommand("level.display_info_medium");
		menuDisplayInfo->AddCommand("level.display_info_high");
		menuDisplay->addAction(menuLocation->menuAction());
		menuDisplay->addAction(menuSwitch_Camera->menuAction());
		menuDisplay->AddCommand("camera.toggle_speed_height_relative");
		menuDisplay->AddCommand("camera.toggle_terrain_collisions");
		menuDisplay->AddCommand("camera.toggle_object_collisions");
		menuDisplay->AddCommand("general.fullscreen");

		menuLocation->addAction(menuRemember_Location->menuAction());
		menuLocation->addAction(menuGoto_Location->menuAction());
		for (auto i = 1; i <= 12; ++i)
		{
			// Create remember location actions based on command
			QCommandAction* pRememberLocation = new QCommandAction(QString("Remember Location %1").arg(i),
			                                                       QString("level.tag_location '%1'").arg(i).toStdString().c_str(), nullptr);
			QKeySequence rememberLocationShortcut(QString("Ctrl+Alt+F%1").arg(i));
			pRememberLocation->setShortcuts({ rememberLocationShortcut });
			pRememberLocation->SetDefaultShortcuts({ rememberLocationShortcut });
			menuRemember_Location->addAction(pRememberLocation);

			// Create go to location actions based on command
			QCommandAction* pGoToLocation = new QCommandAction(QString("Go to Location %1").arg(i),
			                                                   QString("level.go_to_tag_location '%1'").arg(i).toStdString().c_str(), nullptr);
			QKeySequence goToLocationShortcut(QString("Ctrl+F%1").arg(i));
			pGoToLocation->setShortcuts({ goToLocationShortcut });
			pGoToLocation->SetDefaultShortcuts({ goToLocationShortcut });
			menuGoto_Location->addAction(pGoToLocation);
		}

		menuSwitch_Camera->AddCommand("viewport.make_default_camera_current");
		menuSwitch_Camera->AddCommand("viewport.make_selected_camera_current");
		menuSwitch_Camera->AddCommand("viewport.cycle_current_camera");
		menuGame->AddCommand("game.toggle_game_mode");
		menuGame->AddCommand("game.toggle_suspend_input");
		menuGame->AddCommand("game.toggle_simulate_physics_ai");
		menuGame->addSeparator();
		menuGame->addAction(menuAudio->menuAction());
		menuGame->addAction(menuAI->menuAction());
		menuGame->addSeparator();
		menuGame->AddCommand("game.sync_player_with_camera");
		menuGraphics->addAction(actionVery_High);
		menuGraphics->addAction(actionHigh);
		menuGraphics->addAction(actionMedium);
		menuGraphics->addAction(actionLow);
		menuGraphics->addAction(actionXBox_One);
		menuGraphics->addAction(actionPS4);
		menuGraphics->addAction(actionCustom);
		menuAudio->addAction(actionMute_Audio);
		menuAudio->addAction(actionStop_All_Sounds);
		menuAudio->addAction(actionRefresh_Audio);
		menuAI->addMenu(menuAINavigationUpdate);
		menuAI->addMenu(menuRegenerate_MNM_Agent_Type);
		menuAI->addSeparator();
		menuAI->addMenu(menuDebug_Agent_Type);
		menuAI->AddCommand("ai.visualize_navigation_accessibility");
		menuAI->AddCommand("ai.show_navigation_areas");
		menuAI->addSeparator();
		menuAI->AddCommand("ai.generate_cover_surfaces");
		menuDebug->addAction(menuReload_Scripts->menuAction());
		menuDebug->AddCommand("level.reload_textures_shaders");
		menuDebug->AddCommand("entity.reload_all_archetypes");
		menuDebug->AddCommand("level.reload_geometry");
		menuDebug->AddCommand("terrain.reload_terrain");
		menuDebug->AddCommand("level.validate");
		menuDebug->AddCommand("object.validate_positions");
		menuDebug->AddCommand("object.resolve_missing_objects_materials");
		menuDebug->AddCommand("level.save_stats");
		menuDebug->addSeparator();
		menuDebug->addAction(menuAdvanced->menuAction());
		menuDebug->addSeparator();
		menuDebug->addAction(actionShow_Log_File);
		menuReload_Scripts->AddCommand("level.reload_all_scripts");
		menuReload_Scripts->AddCommand("entity.reload_all_scripts");
		menuReload_Scripts->AddCommand("game.reload_item_scripts");
		menuReload_Scripts->AddCommand("ai.reload_all_scripts");
		menuReload_Scripts->AddCommand("ui.reload_all_scripts");
		menuAdvanced->AddCommand("entity.compile_scripts");
		menuAdvanced->addAction(actionReduce_Working_Set);
		menuAdvanced->AddCommand("level.update_procedural_vegetation");
		menuCoordinates->AddCommand("level.set_view_coordinate_system");
		menuCoordinates->AddCommand("level.set_local_coordinate_system");
		menuCoordinates->AddCommand("level.set_parent_coordinate_system");
		menuCoordinates->AddCommand("level.set_world_coordinate_system");

		retranslateUi(MainWindow);

		QMetaObject::connectSlotsByName(MainWindow);
	} // setupUi

	void retranslateUi(QMainWindow* MainWindow)
	{
		actionShow_Log_File->setText(QApplication::translate("MainWindow", "Show Log File", 0));
		actionRuler->setText(QApplication::translate("MainWindow", "Ruler", 0));

		actionReduce_Working_Set->setText(QApplication::translate("MainWindow", "Reduce Working Set", 0));

		ESystemConfigSpec currentConfigSpec = GetIEditorImpl()->GetEditorConfigSpec();

		actionVery_High->setText(QApplication::translate("MainWindow", "Very High", 0));
		actionVery_High->setProperty("command", QString("general.set_config_spec %1").arg(CONFIG_VERYHIGH_SPEC));
		actionVery_High->setChecked(currentConfigSpec == CONFIG_VERYHIGH_SPEC);
		actionHigh->setText(QApplication::translate("MainWindow", "High", 0));
		actionHigh->setProperty("command", QString("general.set_config_spec %1").arg(CONFIG_HIGH_SPEC));
		actionHigh->setChecked(currentConfigSpec == CONFIG_HIGH_SPEC);
		actionMedium->setText(QApplication::translate("MainWindow", "Medium", 0));
		actionMedium->setProperty("command", QString("general.set_config_spec %1").arg(CONFIG_MEDIUM_SPEC));
		actionMedium->setChecked(currentConfigSpec == CONFIG_MEDIUM_SPEC);
		actionLow->setText(QApplication::translate("MainWindow", "Low", 0));
		actionLow->setProperty("command", QString("general.set_config_spec %1").arg(CONFIG_LOW_SPEC));
		actionLow->setChecked(currentConfigSpec == CONFIG_LOW_SPEC);
		actionXBox_One->setText(QApplication::translate("MainWindow", "XBox One", 0));
		actionXBox_One->setProperty("command", QString("general.set_config_spec %1").arg(CONFIG_DURANGO));
		actionXBox_One->setChecked(currentConfigSpec == CONFIG_DURANGO);
		actionPS4->setText(QApplication::translate("MainWindow", "PS4", 0));
		actionPS4->setProperty("command", QString("general.set_config_spec %1").arg(CONFIG_ORBIS));
		actionPS4->setChecked(currentConfigSpec == CONFIG_ORBIS);
		actionCustom->setText(QApplication::translate("MainWindow", "Custom", 0));
		actionCustom->setProperty("command", QString("general.set_config_spec %1").arg(CONFIG_CUSTOM));
		actionCustom->setChecked(currentConfigSpec == CONFIG_CUSTOM);

		actionMute_Audio->setText(QApplication::translate("MainWindow", "Mute Audio", 0));
		actionStop_All_Sounds->setText(QApplication::translate("MainWindow", "Stop All Sounds", 0));
		actionRefresh_Audio->setText(QApplication::translate("MainWindow", "Refresh Audio", 0));
		menuFile->setTitle(QApplication::translate("MainWindow", "&File", 0));
		menuFileNew->setTitle(QApplication::translate("MainWindow", "New", 0));
		menuFileOpen->setTitle(QApplication::translate("MainWindow", "Open", 0));
		menuRecent_Files->setTitle(QApplication::translate("MainWindow", "Recent Files", 0));
		menuEdit->setTitle(QApplication::translate("MainWindow", "Edit", 0));
		menuEditing_Mode->setTitle(QApplication::translate("MainWindow", "Editing Mode", 0));
		menuConstrain->setTitle(QApplication::translate("MainWindow", "Constrain", 0));
		menuFast_Rotate->setTitle(QApplication::translate("MainWindow", "Fast Rotate", 0));
		menuAlign_Snap->setTitle(QApplication::translate("MainWindow", "Align/Snap", 0));
		menuLevel->setTitle(QApplication::translate("MainWindow", "Level", 0));
		menuPhysics->setTitle(QApplication::translate("MainWindow", "Physics", 0));
		menuGroup->setTitle(QApplication::translate("MainWindow", "Group", 0));
		menuLink->setTitle(QApplication::translate("MainWindow", "Link", 0));
		menuPrefabs->setTitle(QApplication::translate("MainWindow", "Prefabs", 0));
		menuSelection->setTitle(QApplication::translate("MainWindow", "Selection", 0));
		menuSelection_Mask->setTitle(QApplication::translate("MainWindow", "Selection Mask", 0));
		menuMaterial->setTitle(QApplication::translate("MainWindow", "Material", 0));
		menuLighting->setTitle(QApplication::translate("MainWindow", "Lighting", 0));
		menuDisplay->setTitle(QApplication::translate("MainWindow", "Display", 0));
		menuLocation->setTitle(QApplication::translate("MainWindow", "Location", 0));
		menuRemember_Location->setTitle(QApplication::translate("MainWindow", "Remember Location", 0));
		menuGoto_Location->setTitle(QApplication::translate("MainWindow", "Go to Location", 0));
		menuSwitch_Camera->setTitle(QApplication::translate("MainWindow", "Switch Camera", 0));
		menuGame->setTitle(QApplication::translate("MainWindow", "Game", 0));
		menuTools->setTitle(QApplication::translate("MainWindow", "Tools", 0));
		menuLayout->setTitle(QApplication::translate("MainWindow", "Layout", 0));
		menuHelp->setTitle(QApplication::translate("MainWindow", "Help", 0));
		menuGraphics->setTitle(QApplication::translate("MainWindow", "Graphics", 0));
		menuDisplayInfo->setTitle(QApplication::translate("MainWindow", "Display Info", 0));
		menuAudio->setTitle(QApplication::translate("MainWindow", "Audio", 0));
		menuAI->setTitle(QApplication::translate("MainWindow", "AI", 0));
		menuAINavigationUpdate->setTitle(QApplication::translate("MainWindow", "Navigation Update Mode", 0));
		menuDebug->setTitle(QApplication::translate("MainWindow", "Debug", 0));
		menuDebug_Agent_Type->setTitle(QApplication::translate("MainWindow", "Debug Navigation Agent Type", 0));
		menuRegenerate_MNM_Agent_Type->setTitle(QApplication::translate("MainWindow", "Regenerate Navigation", 0));
		menuReload_Scripts->setTitle(QApplication::translate("MainWindow", "Reload Scripts", 0));
		menuAdvanced->setTitle(QApplication::translate("MainWindow", "Advanced", 0));
		menuCoordinates->setTitle(QApplication::translate("MainWindow", "Coordinates", 0));
		Q_UNUSED(MainWindow);
	} // retranslateUi

};

//////////////////////////////////////////////////////////////////////////
CEditorMainFrame::CEditorMainFrame(QWidget* parent)
	: QMainWindow(parent)
	, m_levelEditor(new CLevelEditor)
	, m_pAutoBackupTimer(nullptr)
	, m_bClosing(false)
	, m_bUserEventPriorityMode(false)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	m_levelEditor->Initialize();

	// Make the level editor the fallback handler for all unhandled events
	m_loopHandler.SetDefaultHandler(m_levelEditor.get());
	m_pInstance = this;
	m_pAboutDlg = nullptr;
	Ui_MainWindow().setupUi(this);
	s_pToolTabManager = new CTabPaneManager(this);
	s_pWidgetGlobalActionRegistry = new CWidgetsGlobalActionRegistry();
	connect(m_levelEditor.get(), &CLevelEditor::LevelLoaded, this, &CEditorMainFrame::UpdateWindowTitle);

	setAttribute(Qt::WA_DeleteOnClose, true);

	// Enable idle loop
	QTimer::singleShot(0, this, &CEditorMainFrame::OnIdleCallback);
	QTimer::singleShot(500, this, &CEditorMainFrame::OnBackgroundUpdateTimer);

	m_pAutoBackupTimer = new QTimer();
	m_pAutoBackupTimer->setInterval(gEditorFilePreferences.autoSaveTime() * 60 * 1000);
	m_pAutoBackupTimer->start();

	connect(m_pAutoBackupTimer, &QTimer::timeout, this, &CEditorMainFrame::OnAutoSaveTimer);
	gEditorFilePreferences.autoSaveTimeChanged.Connect(this, &CEditorMainFrame::OnAutoBackupTimeChanged);

	// Enable idle loop
	m_loopHandler.AddNativeHandler(reinterpret_cast<uintptr_t>(this), WrapMemberFunction(this, &CEditorMainFrame::OnNativeEvent));

	// Define docking priority
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);

	setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks);

	QVariantMap toolConfig;
	toolConfig[QTWM_AREA_DOCUMENT_MODE] = false;
	toolConfig[QTWM_AREA_IMAGE_HANDLE] = true;
	toolConfig[QTWM_AREA_SHOW_DRAG_HANDLE] = false;
	toolConfig[QTWM_AREA_TABS_CLOSABLE] = false;
	toolConfig[QTWM_AREA_EMPTY_SPACE_DRAG] = true;
	toolConfig[QTWM_THUMBNAIL_TIMER_INTERVAL] = 1000;
	toolConfig[QTWM_TOOLTIP_OFFSET] = QPoint(1, 20);
	toolConfig[QTWM_AREA_TAB_ICONS] = false;
	toolConfig[QTWM_RELEASE_POLICY] = rcpWidget;
	toolConfig[QTWM_WRAPPERS_ARE_CHILDREN] = !gEditorGeneralPreferences.showWindowsInTaskbar;
	toolConfig[QTWM_RAISE_DELAY] = 1000;
	toolConfig[QTWM_RETITLE_WRAPPER] = true;
	toolConfig[QTWM_SINGLE_TAB_FRAME] = true;
	toolConfig[QTWM_BRING_ALL_TO_FRONT] = true;
	toolConfig[SANDBOX_WRAPPER_MINIMIZE_ICON] = CryIcon("icons:Window/Window_Minimize.ico");
	toolConfig[SANDBOX_WRAPPER_MAXIMIZE_ICON] = CryIcon("icons:Window/Window_Maximize.ico");
	toolConfig[SANDBOX_WRAPPER_RESTORE_ICON] = CryIcon("icons:Window/Window_Restore.ico");
	toolConfig[SANDBOX_WRAPPER_CLOSE_ICON] = CryIcon("icons:Window/Window_Close.ico");
	toolConfig[QTWM_TAB_CLOSE_ICON] = CryIcon("icons:Window/Window_Close.ico");
	toolConfig[QTWM_SINGLE_TAB_FRAME_CLOSE_ICON] = CryIcon("icons:Window/Window_Close.ico");
	QToolWindowManagerClassFactory* classFactory = new CToolWindowManagerClassFactory();
	m_toolManager = new QToolWindowManager(this, toolConfig, classFactory);

	CEditorDialog::s_config = toolConfig;
	CDockableContainer::s_dockingFactory = classFactory;

	registerMainWindow(this);
	connect(m_toolManager, &QToolWindowManager::updateTrackingTooltip, [](const QString& str, const QPoint& p) { QTrackingTooltip::ShowTextTooltip(str, p); });
	connect(m_toolManager, &QToolWindowManager::toolWindowVisibilityChanged, [](QWidget* w, bool visible) { s_pToolTabManager->OnTabPaneMoved(w, visible); });
	// Keep track of this connection, we want to disconnect as soon as the editor saves the final layout right before closing all panels
	m_layoutChangedConnection = connect(m_toolManager, &QToolWindowManager::layoutChanged, [=]() { s_pToolTabManager->SaveLayout(); });
	QToolWindowManager* mainDockArea = m_toolManager;
	setCentralWidget(mainDockArea);

	UpdateWindowTitle();

	setWindowIcon(QIcon("icons:editor_icon.ico"));
	qApp->setWindowIcon(windowIcon());

	QWidget* w = QSandboxWindow::wrapWidget(this, m_toolManager);
	w->setObjectName("mainWindow");
	w->show();

	GetIEditorImpl()->GetLevelEditorSharedState()->signalEditToolChanged.Connect(this, &CEditorMainFrame::OnEditToolChanged);

	//Important so the focus is set to this and messages reach the CLevelEditor when clicking on the menu.
	setFocusPolicy(Qt::StrongFocus);
}

void CEditorMainFrame::UpdateWindowTitle(const QString& levelPath /*= "" */)
{
	const Version& v = GetIEditorImpl()->GetFileVersion();

	// Show active game project as "ProjectDir/GameDll".
	const QString game = QtUtil::ToQString(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName());
	QString title = QString("CRYENGINE Sandbox - Build %2 - Project '%3'").arg(QString::number(v[0])).arg(game);

	if (!levelPath.isEmpty())
		title.prepend(levelPath.mid(levelPath.lastIndexOf(QRegExp("[/\\\\]")) + 1) + " - ");

	setWindowTitle(title);
}

CEditorMainFrame::~CEditorMainFrame()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->signalEditToolChanged.DisconnectObject(this);

	m_loopHandler.RemoveNativeHandler(reinterpret_cast<uintptr_t>(this));

	if (m_pAutoBackupTimer)
		m_pAutoBackupTimer->deleteLater();

	if (m_pInstance)
	{
		SAFE_DELETE(s_pWidgetGlobalActionRegistry);
		SAFE_DELETE(s_pToolTabManager);
		m_pInstance = 0;
	}
}

void CEditorMainFrame::SetDefaultLayout()
{
	CTabPaneManager::GetInstance()->LayoutLoaded();

	CTabPaneManager::GetInstance()->CreatePane("ViewportClass_Perspective", "Perspective", IViewPaneClass::DOCK_DEFAULT);

	CTabPaneManager::GetInstance()->CreatePane("Rollup");

	CTabPaneManager::GetInstance()->CreatePane("Inspector", 0);
	CTabPaneManager::GetInstance()->CreatePane("Console", 0);

	QWidget* w = this;
	while (w->parentWidget())
	{
		w = w->parentWidget();
	}
	w->showMaximized();
}

void CEditorMainFrame::PostLoad()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	InitActions();
	InitMenus();

	// First attempt to migrate any toolbars in the root path to the mainframe folder. This will place any toolbars in the root
	// in this editor's (MainFrame) specific folder. It will also upgrade toolbars to the latest version
	GetIEditor()->GetToolBarService()->MigrateToolBars("", "MainFrame");

	// Hardcoded editor name because mainframe toolbars are just a hack caused by not having a proper standalone level editor
	std::vector<QToolBar*> toolbars = GetIEditor()->GetToolBarService()->LoadToolBars("MainFrame");
	for (QToolBar* pToolBar : toolbars)
	{
		addToolBar(pToolBar);
	}

	GetIEditorImpl()->GetTrayArea()->RegisterTrayWidget<CTraySearchBox>(0);
	InitMenuBar();

	if (!GetIEditorImpl()->IsInMatEditMode())
	{
		InitLayout();
	}

	GetIEditorImpl()->GetTrayArea()->MainFrameInitialized();
	GetIEditorImpl()->Notify(eNotify_OnMainFrameInitialized);
}

CEditorMainFrame* CEditorMainFrame::GetInstance()
{
	return m_pInstance;
}

void CEditorMainFrame::CreateToolsMenu()
{
	std::vector<IViewPaneClass*> viewPaneClasses;

	QMenu* pToolsMenu = menuBar()->findChild<QMenu*>("menuTools");
	if (!pToolsMenu)
	{
		Warning("Expected to find a menu named 'menuTools' in the main menu bar");
		return;
	}

	const QObjectList& children = pToolsMenu->children();
	for (auto child : children)
	{
		QString temp = child->objectName();
		QMenu* pMenu = qobject_cast<QMenu*>(child);
		if (pMenu)
			temp = pMenu->title();
		else
		{
			QAction* pAction = qobject_cast<QAction*>(child);
			temp = pAction->text();
		}
		temp = "";
	}

	std::map<string, int> numClassesInCategory;

	std::vector<IClassDesc*> classes;
	GetIEditorImpl()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_VIEWPANE, classes);
	for (int i = 0; i < classes.size(); i++)
	{
		numClassesInCategory[classes[i]->Category()]++;
	}
	for (int i = 0; i < classes.size(); i++)
	{
		IClassDesc* pClass = classes[i];
		IViewPaneClass* pViewClass = (IViewPaneClass*)pClass;

		if (!pViewClass)
		{
			continue;
		}
		if (stl::find(viewPaneClasses, pViewClass))
		{
			continue;
		}

		string name = pViewClass->ClassName();
		string category = pViewClass->Category();

		string cmd;
		cmd.Format("general.open_pane '%s'", (const char*)pViewClass->ClassName());
		viewPaneClasses.push_back(pViewClass);

		QMenu* pMenu = nullptr;
		QMenu* pParentMenu = pToolsMenu;

		if (!pViewClass->NeedsMenuItem())
			continue;

		QString menuName;
		QString menuPath(pViewClass->GetMenuPath());

		QStringList paths;
		if (!menuPath.isEmpty())
			paths = menuPath.split("/");

		// Find the menu in which to insert the tool. If it doesn't exist, create it
		for (const QString& entry : paths)
		{
			menuName = "menu" + entry;
			pMenu = pParentMenu->findChild<QMenu*>(menuName);

			if (!pMenu)
			{
				pMenu = pParentMenu->addMenu(entry);
				pMenu->setObjectName(menuName);
			}

			pParentMenu = pMenu;
		}

		QAction* pAction = new QCommandAction(pViewClass->GetPaneTitle(), cmd.c_str(), pParentMenu);
		QString iconName(pViewClass->GetPaneTitle());
		iconName = iconName.replace(" ", "-").toLower();
		iconName = "icons:Tools/tools_" + iconName + ".ico";
		if (QFile::exists(iconName))
		{
			pAction->setIcon(CryIcon(iconName));
		}
		pParentMenu->addAction(pAction);
	}

	if (GetIEditorImpl()->IsDevModeEnabled())
	{
		string cmd("general.open_pane 'Icon Design Helper'");
		pToolsMenu->addAction(new QCommandAction("Icon Design Helper", cmd.c_str(), pToolsMenu));
	}

	QList<QAction*> actions = pToolsMenu->actions();
	QList<QAction*> looseActions;
	QList<QAction*> subMenuActions;

	for (QAction* pAction : actions)
	{
		if (!pAction->menu())
		{
			looseActions.append(pAction);
			continue;
		}

		subMenuActions.append(pAction);
	}

	QAction* pLevelEditorAction = nullptr;
	for (int i = 0; i < subMenuActions.size(); ++i)
	{
		if (subMenuActions[i]->text() == "Level Editor")
		{
			pLevelEditorAction = subMenuActions[i];
			subMenuActions.removeAt(i);
			break;
		}
	}

	QAction* pViewportAction = nullptr;
	for (int i = 0; i < subMenuActions.size(); ++i)
	{
		if (subMenuActions[i]->text() == "Viewport")
		{
			pViewportAction = subMenuActions[i];
			subMenuActions.removeAt(i);
			break;
		}
	}

	std::sort(looseActions.begin(), looseActions.end(), [](const QAction* pFirst, const QAction* pSecond)
	{
		return pFirst->text() < pSecond->text();
	});
	std::sort(subMenuActions.begin(), subMenuActions.end(), [](const QAction* pFirst, const QAction* pSecond)
	{
		return pFirst->text() < pSecond->text();
	});

	pToolsMenu->addAction(pLevelEditorAction);
	pToolsMenu->addAction(pViewportAction);
	pToolsMenu->addSeparator();
	pToolsMenu->addActions(looseActions);
	pToolsMenu->addActions(subMenuActions);
	pToolsMenu->insertSeparator(subMenuActions[0]);
}

void CEditorMainFrame::BindSnapMenu()
{
	QAction* pSnapToGrid = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_grid");
	pSnapToGrid->setChecked(m_levelEditor->IsGridSnappingEnabled());
	connect(m_levelEditor.get(), &CLevelEditor::GridSnappingEnabled, [this, pSnapToGrid]()
	{
		pSnapToGrid->setChecked(m_levelEditor->IsGridSnappingEnabled());
	});

	QAction* pSnapToAngle = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_angle");
	pSnapToAngle->setChecked(m_levelEditor->IsAngleSnappingEnabled());
	connect(m_levelEditor.get(), &CLevelEditor::AngleSnappingEnabled, [this, pSnapToAngle]()
	{
		pSnapToAngle->setChecked(m_levelEditor->IsAngleSnappingEnabled());
	});

	QAction* pSnapToScale = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_scale");
	pSnapToScale->setChecked(m_levelEditor->IsScaleSnappingEnabled());
	connect(m_levelEditor.get(), &CLevelEditor::ScaleSnappingEnabled, [this, pSnapToScale]()
	{
		pSnapToScale->setChecked(m_levelEditor->IsScaleSnappingEnabled());
	});

	QAction* pSnapToVertex = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_vertex");
	pSnapToVertex->setChecked(m_levelEditor->IsVertexSnappingEnabled());
	connect(m_levelEditor.get(), &CLevelEditor::VertexSnappingEnabled, [this, pSnapToVertex]()
	{
		pSnapToVertex->setChecked(m_levelEditor->IsVertexSnappingEnabled());
	});

	QAction* pSnapToTerrain = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_terrain");
	QAction* pSnapToGeometry = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_geometry");
	pSnapToTerrain->setChecked(m_levelEditor->IsTerrainSnappingEnabled());
	connect(m_levelEditor.get(), &CLevelEditor::TerrainSnappingEnabled, [this, pSnapToTerrain, pSnapToGeometry]()
	{
		pSnapToTerrain->setChecked(m_levelEditor->IsTerrainSnappingEnabled());
		pSnapToGeometry->setChecked(m_levelEditor->IsGeometrySnappingEnabled());
	});

	pSnapToGeometry->setChecked(m_levelEditor->IsGeometrySnappingEnabled());
	connect(m_levelEditor.get(), &CLevelEditor::GeometrySnappingEnabled, [this, pSnapToTerrain, pSnapToGeometry]()
	{
		pSnapToTerrain->setChecked(m_levelEditor->IsTerrainSnappingEnabled());
		pSnapToGeometry->setChecked(m_levelEditor->IsGeometrySnappingEnabled());
	});

	QAction* pSnapToNormal = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_surface_normal");
	pSnapToNormal->setChecked(m_levelEditor->IsSurfaceNormalSnappingEnabled());
	connect(m_levelEditor.get(), &CLevelEditor::SurfaceNormalSnappingEnabled, [this, pSnapToNormal]()
	{
		pSnapToNormal->setChecked(m_levelEditor->IsSurfaceNormalSnappingEnabled());
	});
}

void CEditorMainFrame::BindAIMenu()
{
	QMenu* pUpdateModeMenu = menuBar()->findChild<QMenu*>("menuAINavigationUpdate");
	if (!pUpdateModeMenu)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Ai Navigation Update Menu not found");
		return;
	}

	CEditorCommandManager* pCommandManager = GetIEditorImpl()->GetCommandManager();

	QActionGroup* pUpdateModeActionGroup = new QActionGroup(pUpdateModeMenu);

	QAction* pContinousUpdate = pCommandManager->GetCommandAction("ai.set_navigation_update_continuous");
	if (pContinousUpdate)
	{
		pUpdateModeActionGroup->addAction(pContinousUpdate);
		pUpdateModeMenu->addAction(pContinousUpdate);
		pContinousUpdate->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateMode::Continuous);
		gAINavigationPreferences.signalSettingsChanged.Connect([pContinousUpdate]()
		{
			pContinousUpdate->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateMode::Continuous);
		});
	}

	QAction* pAfterChangeUpdate = pCommandManager->GetCommandAction("ai.set_navigation_update_afterchange");
	if (pAfterChangeUpdate)
	{
		pUpdateModeActionGroup->addAction(pAfterChangeUpdate);
		pUpdateModeMenu->addAction(pAfterChangeUpdate);
		pAfterChangeUpdate->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateMode::AfterStabilization);
		gAINavigationPreferences.signalSettingsChanged.Connect([pAfterChangeUpdate]()
		{
			pAfterChangeUpdate->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateMode::AfterStabilization);
		});
	}

	QAction* pUpdateDisabled = pCommandManager->GetCommandAction("ai.set_navigation_update_disabled");
	if (pUpdateDisabled)
	{
		pUpdateModeActionGroup->addAction(pUpdateDisabled);
		pUpdateModeMenu->addAction(pUpdateDisabled);
		pUpdateDisabled->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateMode::Disabled);
		gAINavigationPreferences.signalSettingsChanged.Connect([pUpdateDisabled]()
		{
			pUpdateDisabled->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateMode::Disabled);
		});
	}
}

void CEditorMainFrame::AboutToShowEditMenu(QMenu* editMenu)
{
	QList<QAction*> actions = editMenu->actions();
	QAction* pLockAction = nullptr;

	for (int i = 0; i < actions.size(); ++i)
	{
		QAction* pAction = actions[i];
		if (pAction->objectName() == "actionLock_Selection")
		{
			pLockAction = pAction;
			break;
		}
	}

	if (pLockAction)
	{
		pLockAction->setChecked(GetIEditorImpl()->IsSelectionLocked());
	}
}

void CEditorMainFrame::CreateLayoutMenu(QMenu* layoutMenu)
{
	layoutMenu->clear();

	bool bAddSeparator = false;

	QFileInfoList files = CTabPaneManager::GetInstance()->GetUserLayouts();
	for (auto it = files.begin(); it != files.end(); ++it)
	{
		QString commandCall = QString("layout.load '%1'").arg(it->absoluteFilePath());
		layoutMenu->addAction(new QCommandAction(it->baseName(), commandCall.toStdString().c_str(), layoutMenu));
		bAddSeparator = true;
	}

	if (bAddSeparator)
	{
		layoutMenu->addSeparator();
	}

	bAddSeparator = false;

	files = CTabPaneManager::GetInstance()->GetProjectLayouts();
	for (auto it = files.begin(); it != files.end(); ++it)
	{
		QString commandCall = QString("layout.load '%1'").arg(it->absoluteFilePath());
		layoutMenu->addAction(new QCommandAction(it->baseName(), commandCall.toStdString().c_str(), layoutMenu));
		bAddSeparator = true;
	}

	if (bAddSeparator)
	{
		layoutMenu->addSeparator();
	}

	CEditorCommandManager* comMan = GetIEditorImpl()->GetCommandManager();

	layoutMenu->addAction(comMan->GetCommandAction("layout.save_as"));
	layoutMenu->addAction(comMan->GetCommandAction("layout.load_dlg"));
	layoutMenu->addAction(comMan->GetCommandAction("layout.reset"));
}

void CEditorMainFrame::InitMenus()
{
	CreateToolsMenu();
	BindSnapMenu();
	BindAIMenu();

	//Layout menu
	QMenu* layoutMenu = menuBar()->findChild<QMenu*>("menuLayout");
	if (!layoutMenu)
	{
		Warning("Expected to find a menu named 'menuLayout' in the main menu bar");
	}
	else
	{
		connect(layoutMenu, &QMenu::aboutToShow, [this, layoutMenu]()
		{
			CreateLayoutMenu(layoutMenu);
		});
	}

	//Display menu
	QMenu* displayMenu = menuBar()->findChild<QMenu*>("menuDisplay");
	if (!displayMenu)
	{
		Warning("Expected to find a menu named 'menuDisplay' in the main menu bar");
	}
	else
	{
		connect(displayMenu, &QMenu::aboutToShow, [this]()
		{
			ICommandManager* pCommandManager = GetIEditorImpl()->GetICommandManager();
			pCommandManager->GetAction("camera.toggle_speed_height_relative")->setChecked(CRenderViewport::s_cameraPreferences.IsSpeedHeightRelativeEnabled());
			pCommandManager->GetAction("camera.toggle_terrain_collisions")->setChecked(CRenderViewport::s_cameraPreferences.IsTerrainCollisionEnabled());
			pCommandManager->GetAction("camera.toggle_object_collisions")->setChecked(CRenderViewport::s_cameraPreferences.IsObjectCollisionEnabled());
		});
	}

	//Edit menu
	QMenu* editMenu = menuBar()->findChild<QMenu*>("menuEdit");
	if (!editMenu)
	{
		Warning("Expected to find a menu named 'menuEdit' in the main menu bar");
	}
	else
	{
		// Add entries
		string name("Keyboard Shortcuts");
		string command;
		command.Format("general.open_pane '%s'", name);
		QCommandAction* pKeyboardShortcuts = new QCommandAction(QtUtil::ToQString(name) + "...", command.c_str(), editMenu);
		pKeyboardShortcuts->setIcon(CryIcon("icons:Tools/Keyboard_Shortcuts.ico"));
		editMenu->addAction(pKeyboardShortcuts);

		name = "Preferences";
		command.Format("general.open_pane '%s'", name);
		editMenu->addAction(new QCommandAction(QtUtil::ToQString(name) + "...", command.c_str(), editMenu));

		// Search for the action (separator) right after redo
		QList<QAction*> editActions = editMenu->actions();
		QAction* pAfterRedo = nullptr;
		for (auto i = 0; i < editActions.size(); ++i)
		{
			if (editActions[i]->text() == "Redo")
			{
				if (++i < editActions.size())
					pAfterRedo = editActions[i];

				break;
			}

		}

		name = "Undo History";
		command.Format("general.open_pane '%s'", name);
		QAction* pUndoHistory = new QCommandAction(QtUtil::ToQString(name) + "...", command.c_str(), editMenu);
		pUndoHistory->setIcon(CryIcon("icons:General/History_Undo_List.ico"));
		editMenu->insertAction(pAfterRedo, pUndoHistory);

		connect(editMenu, &QMenu::aboutToShow, [this, editMenu]()
		{
			AboutToShowEditMenu(editMenu);
		});
	}

	//Recent files menu
	QMenu* recentFilesMenu = menuBar()->findChild<QMenu*>("menuRecent_Files");
	if (!recentFilesMenu)
	{
		Warning("Expected to find a menu named 'menuRecent_Files' in the main menu bar");
	}
	else
	{
		connect(recentFilesMenu, &QMenu::aboutToShow, [this, recentFilesMenu]()
		{
			m_levelEditor->CreateRecentFilesMenu(recentFilesMenu);
		});
	}

	QMenu* const pHelpMenu = menuBar()->findChild<QMenu*>("menuHelp");
	if (!pHelpMenu)
	{
		Warning("Expected to find a menu named 'helpMenu' in the main menu bar");
	}
	else
	{
		pHelpMenu->addAction(GetIEditorImpl()->GetICommandManager()->GetAction("general.help"));

		pHelpMenu->addSeparator();

		const char* szName = "Console Commands";
		string command;
		command.Format("general.open_pane '%s'", szName);
		QAction* pAction = new QCommandAction(QtUtil::ToQString(szName) + "...", command.c_str(), pHelpMenu);
		pAction->setIcon(CryIcon("icons:ObjectTypes/Console_Commands.ico"));
		pHelpMenu->addAction(pAction);

		szName = "Console Variables";
		command.Format("general.open_pane '%s'", szName);
		pAction = new QCommandAction(QtUtil::ToQString(szName) + "...", command.c_str(), pHelpMenu);
		pAction->setIcon(CryIcon("icons:ObjectTypes/Console_Variables.ico"));
		pHelpMenu->addAction(pAction);

		pHelpMenu->addSeparator();

		szName = "About Sandbox...";
		pAction = new QAction(szName, pHelpMenu);
		pAction->setProperty("menurole", QVariant(QApplication::translate("MainWindow", "AboutRole", 0)));
		connect(pAction, &QAction::triggered, [this]()
		{
			if (!m_pAboutDlg)
				m_pAboutDlg = new CAboutDialog();

			m_pAboutDlg->exec();
		});

		pHelpMenu->addAction(pAction);
	}
}

void CEditorMainFrame::InitMenuBar()
{
	setMenuWidget(new QMainFrameMenuBar(menuBar(), this));
}

void CEditorMainFrame::InitActions()
{
	CEditorCommandManager* pCommandManager = GetIEditorImpl()->GetCommandManager();

	//For all actions defined in the ui file...
	QList<QAction*> actions = findChildren<QAction*>();
	for (int i = 0; i < actions.count(); i++)
	{
		QCommandAction* action = qobject_cast<QCommandAction*>(actions.at(i));

		if (action)
		{
			//Note : To specialize default shortcuts for other platforms than windows, store the shortcut in a property and handle it here.

			QVariant menuRoleProp = action->property("menurole");
			if (menuRoleProp.isValid())
			{
				QtUtil::AssignMenuRole(action, menuRoleProp.toString());
			}

			QVariant standardKeyProp = action->property("standardkey");
			if (standardKeyProp.isValid())
			{
				CKeyboardShortcut::StandardKey key(CKeyboardShortcut::StringToStandardKey(standardKeyProp.toString().toStdString().c_str()));
				QList<QKeySequence> shortcuts = CKeyboardShortcut(key).ToKeySequence();
				action->SetDefaultShortcuts(shortcuts);
				action->setShortcuts(shortcuts);//This will override the shortcut set in ui file if present
			}
			else if (!action->shortcut().isEmpty())
			{
				QList<QKeySequence> defaults;
				defaults.push_back(action->shortcut());
				action->SetDefaultShortcuts(defaults);
			}

			//Connect and register actions
			QVariant pythonProp = action->property("python");
			if (pythonProp.isValid())
			{
				//Note : could also replace the action in the menu by a QPythonAction
				string command("python.execute '");
				command += pythonProp.toString().toStdString().c_str() + '\'';
				action->SetCommand(command);
			}
			else
			{
				string command;

				QVariant commandProp = action->property("command");
				if (commandProp.isValid())
				{
					QString qstr = commandProp.toString();
					command = qstr.toStdString().c_str();
				}
				else
				{
					string name = action->objectName().toStdString().c_str();
					command = string("ui_action.") + name;
				}

				action->SetCommand(command);
				pCommandManager->RegisterAction(action, command);
			}

			// Create ActionGroups from Action properties
			QVariant groupProp = action->property("actionGroup");
			if (groupProp.isValid())
			{
				QString actionGroupName = groupProp.toString();
				QActionGroup* actionGroup = findChild<QActionGroup*>(actionGroupName);
				if (!actionGroup)
				{
					actionGroup = new QActionGroup(this);
					actionGroup->setObjectName(actionGroupName);
				}
				actionGroup->addAction(action);
			}
		}
	}

	// Workaround for having CEditorMainframe handle shortcuts for all registered commands
	pCommandManager->signalCommandAdded.Connect(this, &CEditorMainFrame::AddCommand);

	// Register any existing commands from the command manager as actions of the main frame
	std::vector<CCommand*> commands;
	pCommandManager->GetCommandList(commands);
	for (CCommand* cmd : commands)
	{
		if (cmd->CanBeUICommand())
		{
			QCommandAction* pCommandAction = pCommandManager->GetCommandAction(cmd->GetCommandString());
			if (pCommandAction->parent() == nullptr)
			{
				addAction(pCommandAction);
			}
		}
	}

	// Must be called after actions declared in the .ui file are registered to the command manager
	CKeybindEditor::LoadUserKeybinds();
}

void CEditorMainFrame::InitLayout()
{
	bool bLayoutLoaded = CTabPaneManager::GetInstance()->LoadUserLayout();
	if (!bLayoutLoaded)
	{
		bLayoutLoaded = CTabPaneManager::GetInstance()->LoadDefaultLayout();
	}

	if (!bLayoutLoaded)
		SetDefaultLayout();
}

void CEditorMainFrame::contextMenuEvent(QContextMenuEvent* pEvent)
{
	QWidget* pChild = childAt(pEvent->pos());

	if (!pChild)
	{
		QMainWindow::contextMenuEvent(pEvent);
		return;
	}

	QList<QToolBar*> toolbars = findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
	QMenu menu;

	bool isToolBarOrChild = false;
	for (QToolBar* pToolbar : toolbars)
	{
		QAction* pAction = menu.addAction(pToolbar->windowTitle());
		pAction->setCheckable(true);
		pAction->setChecked(pToolbar->isVisible());
		connect(pAction, &QAction::triggered, [pToolbar](bool bChecked)
		{
			pToolbar->setVisible(bChecked);
		});

		if (!isToolBarOrChild && (pToolbar == pChild || pToolbar->isAncestorOf(pChild)))
		{
			isToolBarOrChild = true;
		}
	}

	if (!menuWidget()->isAncestorOf(pChild) && !isToolBarOrChild) // handle only if the target is the menubar or in the main toolbar
	{
		QMainWindow::contextMenuEvent(pEvent);
		return;
	}

	menu.addSeparator();
	QAction* pCustomize = menu.addAction("Customize...");
	connect(pCustomize, &QAction::triggered, this, &CEditorMainFrame::OnCustomizeToolBar);

	menu.exec(pEvent->globalPos());
}

void CEditorMainFrame::OnCustomizeToolBar()
{
	CToolBarCustomizeDialog* pToolBarCustomizeDialog = new CToolBarCustomizeDialog(this, "MainFrame");

	pToolBarCustomizeDialog->signalToolBarAdded.Connect(this, &CEditorMainFrame::OnToolBarAdded);
	pToolBarCustomizeDialog->signalToolBarModified.Connect(this, &CEditorMainFrame::OnToolBarModified);
	pToolBarCustomizeDialog->signalToolBarRenamed.Connect(this, &CEditorMainFrame::OnToolBarRenamed);
	pToolBarCustomizeDialog->signalToolBarRemoved.Connect(this, &CEditorMainFrame::OnToolBarRemoved);

	connect(pToolBarCustomizeDialog, &QWidget::destroyed, [this, pToolBarCustomizeDialog]()
	{
		pToolBarCustomizeDialog->signalToolBarRemoved.DisconnectObject(this);
		pToolBarCustomizeDialog->signalToolBarModified.DisconnectObject(this);
		pToolBarCustomizeDialog->signalToolBarAdded.DisconnectObject(this);
	});

	pToolBarCustomizeDialog->show();
}

void CEditorMainFrame::OnToolBarAdded(QToolBar* pToolBar)
{
	// Make sure new toolbars start up as invisible
	pToolBar->setVisible(false);

	addToolBar(pToolBar);
}

void CEditorMainFrame::OnToolBarModified(QToolBar* pToolBar)
{
	QList<QToolBar*> oldToolBars = findChildren<QToolBar*>(pToolBar->objectName(), Qt::FindDirectChildrenOnly);

	if (!oldToolBars.isEmpty())
	{
		QToolBar* pFirstToolBar = oldToolBars.first();
		insertToolBar(pFirstToolBar, pToolBar);
		pToolBar->setVisible(pFirstToolBar->isVisible());
	}

	for (QToolBar* pOldToolBar : oldToolBars)
	{
		// This is necessary because the modified toolbars placement will be incorrect if we remove/delete it's old version
		// on the same tick of the event loop
		QTimer::singleShot(0, [this, pOldToolBar]
		{
			removeToolBar(pOldToolBar);
			pOldToolBar->deleteLater();
		});
	}
}

void CEditorMainFrame::OnToolBarRenamed(const char* szOldToolBarName, QToolBar* pToolBar)
{
	OnToolBarRemoved(szOldToolBarName);
	OnToolBarAdded(pToolBar);
}

void CEditorMainFrame::OnToolBarRemoved(const char* szToolBarName)
{
	QToolBar* pOldToolBar = findChild<QToolBar*>(szToolBarName);
	removeToolBar(pOldToolBar);
	pOldToolBar->deleteLater();
}

void CEditorMainFrame::OnEditToolChanged()
{
	if (!GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool()->IsKindOf(RUNTIME_CLASS(CLinkTool)))
	{
		if (QAction* pAction = GetIEditorImpl()->GetICommandManager()->GetAction("tools.link"))
		{
			pAction->setChecked(false);
		}
	}
}

void CEditorMainFrame::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_EditorConfigSpecChanged:
		{
			ESystemConfigSpec currentConfigSpec = GetIEditorImpl()->GetEditorConfigSpec();

			QAction* actionVery_High = findChild<QAction*>("actionVery_High");
			QAction* actionHigh = findChild<QAction*>("actionHigh");
			QAction* actionMedium = findChild<QAction*>("actionMedium");
			QAction* actionLow = findChild<QAction*>("actionLow");
			QAction* actionXBox_One = findChild<QAction*>("actionXBox_One");
			QAction* actionPS4 = findChild<QAction*>("actionPS4");
			QAction* actionCustom = findChild<QAction*>("actionCustom");

			actionVery_High->setChecked(currentConfigSpec == CONFIG_VERYHIGH_SPEC);
			actionHigh->setChecked(currentConfigSpec == CONFIG_HIGH_SPEC);
			actionMedium->setChecked(currentConfigSpec == CONFIG_MEDIUM_SPEC);
			actionLow->setChecked(currentConfigSpec == CONFIG_LOW_SPEC);
			actionXBox_One->setChecked(currentConfigSpec == CONFIG_DURANGO);
			actionPS4->setChecked(currentConfigSpec == CONFIG_ORBIS);
			actionCustom->setChecked(currentConfigSpec == CONFIG_CUSTOM);
		}
		break;
	}
}

void CEditorMainFrame::closeEvent(QCloseEvent* event)
{
	LOADING_TIME_PROFILE_AUTO_SESSION("sandbox_close");
	if (BeforeClose())
	{
		qApp->exit();
	}
	else
	{
		event->ignore();
	}
}

bool CEditorMainFrame::BeforeClose()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	SaveConfig();
	// disconnect now or we'll end up closing all panels and saving an empty layout
	disconnect(m_layoutChangedConnection);

	AboutToQuitEvent aboutToQuitEvent;
	GetIEditorImpl()->GetGlobalBroadcastManager()->Broadcast(aboutToQuitEvent);

	{
		CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "CEditorMainFrame::BeforeClose() Save Changes?");

		if (aboutToQuitEvent.GetChangeListCount() > 0)
		{
			CSaveChangesDialog saveChangesDialog;

			for (size_t i = 0; i < aboutToQuitEvent.GetChangeListCount(); ++i)
			{
				saveChangesDialog.AddChangeList(aboutToQuitEvent.GetChangeListKey(i).c_str(), aboutToQuitEvent.GetChanges(i));
			}

			if (saveChangesDialog.Execute() == QDialog::Rejected || saveChangesDialog.GetButton() == QDialogButtonBox::Cancel)
			{
				return false;
			}
		}

		if (!GetIEditorImpl()->GetLevelIndependentFileMan()->PromptChangedFiles())
		{
			return false;
		}
	}

	CCryEditDoc* pDoc = GetIEditorImpl()->GetDocument();

	if (!pDoc->CanClose())
	{
		return false;
	}

	m_bClosing = true;

	pDoc->SetModifiedFlag(FALSE);

	// Close all edit panels.
	GetIEditorImpl()->GetObjectManager()->ClearSelection();
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool(nullptr);

	CTabPaneManager::GetInstance()->CloseAllPanes();

	GetIEditorImpl()->CloseDocument();
	return true;
}

void CEditorMainFrame::SaveConfig()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	if (!GetIEditorImpl()->IsInMatEditMode())
	{
		CTabPaneManager::GetInstance()->SaveLayout();
	}
	CKeybindEditor::SaveUserKeybinds();
	GetIEditor()->GetPersonalizationManager()->SavePersonalization();
}

bool CEditorMainFrame::event(QEvent* event)
{
	static qreal scale = devicePixelRatioF();
	if ((event->type() == QEvent::ScreenChangeInternal || event->type() == QEvent::PlatformSurface) && scale != devicePixelRatioF())
	{
		// Make sure toolbars relocate themselves properly when changing DPI
		QMainWindow::restoreState(QMainWindow::saveState());
		scale = devicePixelRatioF();
	}
	else if ((const int)event->type() == SandboxEvent::MissedShortcut)
	{
		// from SandboxWrapper windows we broadcast MissedShorcut event to search trough registered actions in main window to simulate application wide shortuts
		QList<QAction*> actions = findChildren<QAction*>();
		MissedShortcutEvent* missed_event = static_cast<MissedShortcutEvent*>(event);
		QList<QAction*> matching_actions;
		for (QAction* action : actions)
		{
			if (action->isEnabled())
			{
				for (QKeySequence& keyseq : action->shortcuts())
				{
					if (keyseq == missed_event->GetSequence())
					{
						matching_actions.append(action);
					}
				}
			}
		}

		if (matching_actions.count() > 1)
		{
			CryLog("Ambiguous shortcut overload : %s", missed_event->GetSequence().toString(QKeySequence::NativeText).toLatin1().constData());
		}
		else if (matching_actions.count() == 1)
		{
			matching_actions[0]->trigger();
			return true;
		}
	}
	return QMainWindow::event(event);
}

void CEditorMainFrame::OnAutoBackupTimeChanged()
{
	m_pAutoBackupTimer->setInterval(gEditorFilePreferences.autoSaveTime() * 60 * 1000);
}

void CEditorMainFrame::OnAutoSaveTimer()
{
	if (!gEditorFilePreferences.autoSaveEnabled)
		return;

	GetIEditorImpl()->GetDocument()->SaveAutoBackup();
}

bool CEditorMainFrame::OnNativeEvent(void* message, long* result)
{
#ifdef WIN32
	MSG* winMsg = (MSG*)message;
	switch (winMsg->message)
	{
	// whitelist some events as user events
	case WM_KEYDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_KEYUP:
	case WM_MOUSEWHEEL:
		m_lastUserInputTime = gEnv->pTimer->GetAsyncTime();
		break;
	}
#endif
	return false;
}

void CEditorMainFrame::AddCommand(CCommand* pCommand)
{
	QAction* pAction = GetIEditor()->GetICommandManager()->GetAction(pCommand->GetCommandString().c_str());

	// This is a valid case because there might be for example, a user created command that has an error in it.
	// We don't generate an action in this case
	if (!pAction)
		return;

	addAction(pAction);
}

void CEditorMainFrame::OnIdleCallback()
{
	if (Cry::Assert::IsInAssert())
	{
		// If inside assert dialog, we keep idling.
		QTimer::singleShot(30, this, &CEditorMainFrame::OnIdleCallback);
		return;
	}

	bool bOverViewport = false;
	bool res = true;
	if (m_bUserEventPriorityMode)
	{
		QWidget* wnd = qApp->widgetAt(QCursor::pos());

		if (GetIEditorImpl()->GetViewManager()->IsViewport(wnd))
		{
			bOverViewport = true;
		}
	}

	CTimeValue frameStartTime = gEnv->pTimer->GetAsyncTime();
	if (!m_bUserEventPriorityMode ||
	    (frameStartTime - m_lastUserInputTime).GetMilliSeconds() >= m_lastFrameDuration.GetMilliSeconds() ||
	    bOverViewport)
	{
		res = CCryEditApp::GetInstance()->IdleProcessing(false);
		m_lastFrameDuration = gEnv->pTimer->GetAsyncTime() - frameStartTime;
	}

	CTabPaneManager::GetInstance()->OnIdle();

	// Calculate the average frame for the given frame count
	static float movingAverage = 0.0;
	static const float decayRate = 1.0f / 30.0f; // 30 frames

	float currFrameTime = m_lastFrameDuration.GetMilliSeconds();
	// Exponential moving avg: newAverage = decayRate * currVal + (1 - decayRate) * prevMovingAverage
	movingAverage = decayRate * currFrameTime + (1 - decayRate) * movingAverage;

	// if idle frame processing took more than timeout value ms, activate emergency mode where we basically disregard all updates during user interaction
	m_bUserEventPriorityMode = (movingAverage > gPerformancePreferences.userInputPriorityTime);

	QTimer::singleShot(res ? 0 : 16, this, &CEditorMainFrame::OnIdleCallback);
}

void CEditorMainFrame::OnBackgroundUpdateTimer()
{
	int period = 0;
	static ICVar* pBackgroundUpdatePeriod = gEnv->pConsole->GetCVar("ed_backgroundUpdatePeriod");
	if (pBackgroundUpdatePeriod)
	{
		period = pBackgroundUpdatePeriod->GetIVal();
	}
	if (period > 0)
	{
		if (windowState() != Qt::WindowMinimized)
		{
			CCryEditApp::GetInstance()->IdleProcessing(true);
		}
		QTimer::singleShot(period, this, &CEditorMainFrame::OnBackgroundUpdateTimer);
	}
}

QToolWindowManager* CEditorMainFrame::GetToolManager()
{
	return m_toolManager;
}

void CEditorMainFrame::AddWaitProgress(CWaitProgress* task)
{
	m_waitTasks.push_back(task);
}

void CEditorMainFrame::RemoveWaitProgress(CWaitProgress* task)
{
	auto it = std::find(m_waitTasks.begin(), m_waitTasks.end(), task);
	if (it != m_waitTasks.end())
	{
		m_waitTasks.erase(it);
	}
}

bool CEditorMainFrame::IsClosing() const
{
	return m_bClosing;
}
