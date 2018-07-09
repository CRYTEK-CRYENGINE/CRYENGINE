// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include <CrySystem/IProjectManager.h>

//////////////////////////////////////////////////////////////////////////
// QT headers
#include <QTimer>
#include <QMenu>
#include <QMenuBar>
#include <QLayout>
#include <QDockWidget>
#include <QLineEdit>
#include <QFile>
#include <QDir>
#include <QToolBar>
#include <QLabel>
#include <QToolButton>
#include <QToolBox>
#include <QPushButton>
#include <QImageReader>
#include <QMouseEvent>
#include <QAbstractEventDispatcher>
#include <QCursor>

#include <algorithm>
//////////////////////////////////////////////////////////////////////////

#include <CryIcon.h>
#include <QtUtil.h>
#include "QtUtil.h"

#include "QtMainFrame.h"
#include "QtMainFrameWidgets.h"
#include "QMainFrameMenuBar.h"
#include "Commands/QCommandAction.h"
#include "QtViewPane.h"
#include "RenderViewport.h"

//////////////////////////////////////////////////////////////////////////
#include "CryEdit.h"
#include "CryEditDoc.h"
#include "LevelEditor/LevelEditor.h"
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
#include "Util/BoostPythonHelpers.h"
#include "ViewManager.h"
#include "IBackgroundTaskManager.h"
#include "ProcessInfo.h"
#include "QToolTabManager.h"
#include "LevelIndependentFileMan.h"
#include "Commands/KeybindEditor.h"
#include "KeyboardShortcut.h"
#include "EditorFramework/Events.h"
#include <Preferences/GeneralPreferences.h>
#include "Controls/SandboxWindowing.h"
#include "QToolWindowManager/QToolWindowManager.h"
#include "QToolWindowManager/QCustomWindowFrame.h"
#include "DockingSystem/DockableContainer.h"
#include "QTrackingTooltip.h"
#include "QT/MainToolBars/QMainToolBarManager.h"
#include "QT/MainToolBars/QToolBarCreator.h"
#include "QT/Widgets/QWaitProgress.h"
#include "QT/TraySearchBox.h"
#include "Controls/EditorDialog.h"
#include "Controls/SaveChangesDialog.h"
#include "QControls.h"
#include "AboutDialog.h"
#include "QMfcApp/qmfchost.h"
#include "KeyboardShortcut.h"
#include "Menu/MenuWidgetBuilders.h"
#include "LinkTool.h"

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

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

CEditorMainFrame * CEditorMainFrame::m_pInstance = 0;

namespace
{
CTabPaneManager* s_pToolTabManager = 0;
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
};

class Ui_MainWindow
{
public:
	QAction*  actionNew;
	QAction*  actionOpen;
	QAction*  actionSave_As;
	QAction*  actionToggle_Content_Browser;
	QAction*  actionExport_to_Engine;
	QAction*  actionSave_Level_Resources;
	QAction*  actionExport_Selected_Objects;
	QAction*  actionExport_Occlusion_Mesh;
	QAction*  actionShow_Log_File;
	QAction*  actionExit;
	QAction*  actionUndo;
	QAction*  actionRedo;
	QAction*  actionDelete;
	QAction*  actionDuplicate;
	QAction*  actionCopy;
	QAction*  actionCut;
	QAction*  actionPaste;
	QAction*  actionHide_Selection;
	QAction*  actionUnhide_All;
	QAction*  actionFreeze_Selection;
	QAction*  actionUnfreeze_All;
	QAction*  actionSelect_None;
	QAction*  actionSelect_Invert;
	QAction*  actionFind;
	QAction*  actionFindPrevious;
	QAction*  actionFindNext;
	QAction*  actionLock_Selection;
	QAction*  actionSelect_Mode;
	QAction*  actionMove;
	QAction*  actionRotate;
	QAction*  actionScale;
	QAction*  actionLink;
	QAction*  actionUnlink;
	QAction*  actionAlign_To_Grid;
	QAction*  actionAlign_To_Object;
	QAction*  actionRotate_X_Axis;
	QAction*  actionRotate_Y_Axis;
	QAction*  actionRotate_Z_Axis;
	QAction*  actionRotate_Angle;
	QAction*  actionGoto_Selection;
	QAction*  actionMaterial_Assign_Current;
	QAction*  actionMaterial_Reset_to_Default;
	QAction*  actionMaterial_Get_from_Selected;
	QAction*  actionLighting_Regenerate_All_Cubemap;
	QAction*  actionPick_Material;
	QAction*  actionGet_Physics_State;
	QAction*  actionReset_Physics_State;
	QAction*  actionPhysics_Simulate_Objects;
	QAction*  actionPhysics_Generate_Joints;
	QAction*  actionWireframe;
	QAction*  actionRuler;
	QAction*  actionToggleGridSnapping;
	QAction*  actionToggleAngleSnapping;
	QAction*  actionToggleScaleSnapping;
	QAction*  actionToggleVertexSnapping;
	QAction*  actionToggleTerrainSnapping;
	QAction*  actionToggleGeometrySnapping;
	QAction*  actionToggleNormalSnapping;
	QAction*  actionGoto_Position;
	QAction*  actionRemember_Location_1;
	QAction*  actionRemember_Location_2;
	QAction*  actionRemember_Location_3;
	QAction*  actionRemember_Location_4;
	QAction*  actionRemember_Location_5;
	QAction*  actionRemember_Location_6;
	QAction*  actionRemember_Location_7;
	QAction*  actionRemember_Location_8;
	QAction*  actionRemember_Location_9;
	QAction*  actionRemember_Location_10;
	QAction*  actionRemember_Location_11;
	QAction*  actionRemember_Location_12;
	QAction*  actionGoto_Location_1;
	QAction*  actionGoto_Location_2;
	QAction*  actionGoto_Location_3;
	QAction*  actionGoto_Location_4;
	QAction*  actionGoto_Location_5;
	QAction*  actionGoto_Location_6;
	QAction*  actionGoto_Location_7;
	QAction*  actionGoto_Location_8;
	QAction*  actionGoto_Location_9;
	QAction*  actionGoto_Location_10;
	QAction*  actionGoto_Location_11;
	QAction*  actionGoto_Location_12;
	QAction*  actionCamera_Default;
	QAction*  actionCamera_Sequence;
	QAction*  actionCamera_Selected_Object;
	QAction*  actionCamera_Cycle;
	QAction*  actionShow_Hide_Helpers;
	QAction*  actionCycle_Display_Info;
	QAction*  actionToggle_Fullscreen_Viewport;
	QAction*  actionGroup;
	QAction*  actionUngroup;
	QAction*  actionGroup_Open;
	QAction*  actionGroup_Close;
	QAction*  actionGroup_Attach;
	QAction*  actionGroup_Detach;
	QAction*  actionGroup_DetachToRoot;
	QAction*  actionReload_Terrain;
	QAction*  actionSwitch_to_Game;
	QAction*  actionSuspend_Game_Input;
	QAction*  actionEnable_Physics_AI;
	QAction*  actionSynchronize_Player_with_Camera;
	QAction*  actionEdit_Equipment_Packs;
	QAction*  actionReload_All_Scripts;
	QAction*  actionReload_Entity_Scripts;
	QAction*  actionReload_Item_Scripts;
	QAction*  actionReload_AI_Scripts;
	QAction*  actionReload_UI_Scripts;
	QAction*  actionReload_Archetypes;
	QAction*  actionReload_Texture_Shaders;
	QAction*  actionReload_Geometry;
	QAction*  actionCheck_Level_for_Errors;
	QAction*  actionCheck_Object_Positions;
	QAction*  actionSave_Level_Statistics;
	QAction*  actionCompile_Script;
	QAction*  actionReduce_Working_Set;
	QAction*  actionUpdate_Procedural_Vegetation;
	QAction*  actionSave;
	QAction*  actionSave_Selected_Objects;
	QAction*  actionLoad_Selected_Objects;
	QAction*  actionLock_X_Axis;
	QAction*  actionLock_Y_Axis;
	QAction*  actionLock_Z_Axis;
	QAction*  actionLock_XY_Axis;
	QAction*  actionResolve_Missing_Objects;
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
	QAction*  actionCoordinates_View;
	QAction*  actionCoordinates_Local;
	QAction*  actionCoordinates_Parent;
	QAction*  actionCoordinates_World;
	QWidget*  centralwidget;
	QMenuBar* menubar;
	QMenu*    menuFile;
	QMenu*    menuRecent_Files;
	QMenu*    menuEdit;
	QMenu*    menuEditing_Mode;
	QMenu*    menuConstrain;
	QMenu*    menuFast_Rotate;
	QMenu*    menuAlign_Snap;
	QMenu*    menuLevel;
	QMenu*    menuPhysics;
	QMenu*    menuGroup;
	QMenu*    menuLink;
	QMenu*    menuPrefabs;
	QMenu*    menuSelection;
	QMenu*    menuMaterial;
	QMenu*    menuLighting;
	QMenu*    menuDisplay;
	QMenu*    menuLocation;
	QMenu*    menuRemember_Location;
	QMenu*    menuGoto_Location;
	QMenu*    menuSwitch_Camera;
	QMenu*    menuGame;
	QMenu*    menuTools;
	QMenu*    menuLayout;
	QMenu*    menuHelp;
	QMenu*    menuGraphics;
	QMenu*    menuAudio;
	QMenu*    menuAI;
	QMenu*    menuAINavigationUpdate;
	QMenu*    menuDebug;
	QMenu*    menuReload_Scripts;
	QMenu*    menuAdvanced;
	QMenu*    menuCoordinates;
	QMenu*    menuSelection_Mask;
	QMenu*    menuDebug_Agent_Type;
	QMenu*    menuRegenerate_MNM_Agent_Type;

	void setupUi(QMainWindow* MainWindow)
	{
		CEditorCommandManager* pCommandManager = GetIEditorImpl()->GetCommandManager();

		if (MainWindow->objectName().isEmpty())
			MainWindow->setObjectName(QStringLiteral("MainWindow"));
		MainWindow->resize(1172, 817);
		actionNew = new QCommandAction(nullptr, nullptr, MainWindow);
		actionNew->setIcon(CryIcon("icons:General/File_New.ico"));
		actionNew->setObjectName(QStringLiteral("actionNew"));
		actionOpen = new QCommandAction(nullptr, nullptr, MainWindow);
		actionOpen->setObjectName(QStringLiteral("actionOpen"));
		CryIcon icon;
		icon.addFile(QStringLiteral("icons:General/Folder_Open.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionOpen->setIcon(icon);
		actionSave_As = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSave_As->setObjectName(QStringLiteral("actionSave_As"));
		actionSave_As->setIcon(CryIcon("icons:General/Save_as.ico"));
		actionToggle_Content_Browser = new QCommandAction("Quick Asset Browser", "asset.toggle_browser", MainWindow);
		actionToggle_Content_Browser->setIcon(CryIcon("icons:Tools/tools_asset-browser.ico"));
		actionExport_to_Engine = new QCommandAction(nullptr, nullptr, MainWindow);
		actionExport_to_Engine->setObjectName(QStringLiteral("actionExport_to_Engine"));
		actionExport_to_Engine->setIcon(CryIcon("icons:General/Export.ico"));
		actionExport_Occlusion_Mesh = new QCommandAction(nullptr, nullptr, MainWindow);
		actionExport_Occlusion_Mesh->setObjectName(QStringLiteral("actionExport_Occlusion_Mesh"));
		actionSave_Level_Resources = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSave_Level_Resources->setObjectName(QStringLiteral("actionSave_Level_Resources"));
		actionExport_Selected_Objects = new QCommandAction(nullptr, nullptr, MainWindow);
		actionExport_Selected_Objects->setObjectName(QStringLiteral("actionExport_Selected_Objects"));
		actionExport_Selected_Objects->setIcon(CryIcon("icons:Tools/Export_Selected_Objects.ico"));
		actionShow_Log_File = new QCommandAction(nullptr, nullptr, MainWindow);
		actionShow_Log_File->setObjectName(QStringLiteral("actionShow_Log_File"));
		actionExit = new QCommandAction(nullptr, nullptr, MainWindow);
		actionExit->setObjectName(QStringLiteral("actionExit"));
		actionExit->setIcon(CryIcon("icons:General/Exit.ico"));
		actionUndo = new QCommandAction(nullptr, nullptr, MainWindow);
		actionUndo->setObjectName(QStringLiteral("actionUndo"));
		CryIcon icon1;
		icon1.addFile(QStringLiteral("icons:General/History_Undo.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionUndo->setIcon(icon1);
		actionRedo = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRedo->setObjectName(QStringLiteral("actionRedo"));
		CryIcon icon2;
		icon2.addFile(QStringLiteral("icons:General/History_Redo.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionRedo->setIcon(icon2);
		actionDelete = new QCommandAction(nullptr, nullptr, MainWindow);
		actionDelete->setObjectName(QStringLiteral("actionDelete"));
		actionDelete->setIcon(CryIcon("icons:General/Close.ico"));
		actionDelete->setIcon(CryIcon("icons:General/Delete_Asset.ico"));
		actionDuplicate = new QCommandAction(nullptr, nullptr, MainWindow);
		actionDuplicate->setObjectName(QStringLiteral("actionDuplicate"));
		actionDuplicate->setIcon(CryIcon("icons:General/Duplicate_asset.ico"));
		actionCopy = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCopy->setIcon(CryIcon("icons:General/Copy_Asset.ico"));
		actionCopy->setObjectName(QStringLiteral("actionCopy"));
		actionCut = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCut->setIcon(CryIcon("icons:General/Cut_Asset.ico"));
		actionCut->setObjectName(QStringLiteral("actionCut"));
		actionPaste = new QCommandAction(nullptr, nullptr, MainWindow);
		actionPaste->setIcon(CryIcon("icons:General/Paste_Asset.ico"));
		actionPaste->setObjectName(QStringLiteral("actionPaste"));
		actionHide_Selection = new QCommandAction(nullptr, nullptr, MainWindow);
		actionHide_Selection->setObjectName(QStringLiteral("actionHide_Selection"));
		actionUnhide_All = new QCommandAction(nullptr, nullptr, MainWindow);
		actionUnhide_All->setObjectName(QStringLiteral("actionUnhide_All"));
		actionFreeze_Selection = new QCommandAction(nullptr, nullptr, MainWindow);
		actionFreeze_Selection->setObjectName(QStringLiteral("actionFreeze_Selection"));
		CryIcon icon3;
		icon3.addFile(QStringLiteral("icons:General/editable_false.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionFreeze_Selection->setIcon(icon3);
		actionUnfreeze_All = new QCommandAction(nullptr, nullptr, MainWindow);
		actionUnfreeze_All->setObjectName(QStringLiteral("actionUnfreeze_All"));
		CryIcon icon4;
		icon4.addFile(QStringLiteral("icons:General/editable_true.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionUnfreeze_All->setIcon(icon4);
		actionSelect_None = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSelect_None->setObjectName(QStringLiteral("actionSelect_None"));
		actionSelect_Invert = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSelect_Invert->setObjectName(QStringLiteral("actionSelect_Invert"));
		actionFind = new QCommandAction(nullptr, nullptr, MainWindow);
		actionFind->setObjectName(QStringLiteral("actionFind"));
		CryIcon icon7;
		icon7.addFile(QStringLiteral("icons:General/Search.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionFind->setIcon(icon7);
		actionFindPrevious = new QCommandAction(nullptr, nullptr, MainWindow);
		actionFindPrevious->setObjectName(QStringLiteral("actionFindPrevious"));
		CryIcon iconFindPrevious;
		iconFindPrevious.addFile(QStringLiteral("icons:General/Arrow_Left.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionFindPrevious->setIcon(iconFindPrevious);
		CryIcon iconFindNext;
		iconFindNext.addFile(QStringLiteral("icons:General/Arrow_Right.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionFindNext = new QCommandAction(nullptr, nullptr, MainWindow);
		actionFindNext->setObjectName(QStringLiteral("actionFindNext"));
		actionFindNext->setIcon(iconFindNext);
		actionLock_Selection = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLock_Selection->setObjectName(QStringLiteral("actionLock_Selection"));
		actionLock_Selection->setCheckable(true);
		actionSelect_Mode = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSelect_Mode->setObjectName(QStringLiteral("actionSelect_Mode"));
		actionSelect_Mode->setCheckable(true);
		int editMode = GetIEditorImpl()->GetEditMode();
		actionSelect_Mode->setChecked(editMode == eEditModeSelect);
		CryIcon icon8;
		icon8.addFile(QStringLiteral("icons:Navigation/Basics_Select.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionSelect_Mode->setIcon(icon8);
		actionMove = new QCommandAction(nullptr, nullptr, MainWindow);
		actionMove->setObjectName(QStringLiteral("actionMove"));
		actionMove->setCheckable(true);
		CryIcon icon9;
		icon9.addFile(QStringLiteral("icons:Navigation/Basics_Move.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionMove->setIcon(icon9);
		actionRotate = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRotate->setObjectName(QStringLiteral("actionRotate"));
		actionRotate->setCheckable(true);
		CryIcon icon10;
		icon10.addFile(QStringLiteral("icons:Navigation/Basics_Rotate.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionRotate->setIcon(icon10);
		actionScale = new QCommandAction(nullptr, nullptr, MainWindow);
		actionScale->setObjectName(QStringLiteral("actionScale"));
		actionScale->setCheckable(true);
		CryIcon icon11;
		icon11.addFile(QStringLiteral("icons:Navigation/Basics_Scale.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionScale->setIcon(icon11);
		actionLink = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLink->setObjectName(QStringLiteral("actionLink"));
		CryIcon icon13;
		icon13.addFile(QStringLiteral("icons:Navigation/Tools_Link.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionLink->setIcon(icon13);
		actionLink->setCheckable(true);
		actionUnlink = new QCommandAction(nullptr, nullptr, MainWindow);
		actionUnlink->setObjectName(QStringLiteral("actionUnlink"));
		CryIcon icon14;
		icon14.addFile(QStringLiteral("icons:Navigation/Tools_Link_Unlink.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionUnlink->setIcon(icon14);
		actionAlign_To_Grid = new QCommandAction(nullptr, nullptr, MainWindow);
		actionAlign_To_Grid->setObjectName(QStringLiteral("actionAlign_To_Grid"));
		actionAlign_To_Grid->setCheckable(false);
		CryIcon icon15;
		icon15.addFile(QStringLiteral("icons:Navigation/Align_To_Grid.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionAlign_To_Grid->setIcon(icon15);
		actionAlign_To_Object = new QCommandAction(nullptr, nullptr, MainWindow);
		actionAlign_To_Object->setObjectName(QStringLiteral("actionAlign_To_Object"));
		CryIcon icon16;
		icon16.addFile(QStringLiteral("icons:Viewport/viewport-snap-pivot.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionAlign_To_Object->setIcon(icon16);
		actionRotate_X_Axis = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRotate_X_Axis->setObjectName(QStringLiteral("actionRotate_X_Axis"));
		actionRotate_Y_Axis = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRotate_Y_Axis->setObjectName(QStringLiteral("actionRotate_Y_Axis"));
		actionRotate_Z_Axis = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRotate_Z_Axis->setObjectName(QStringLiteral("actionRotate_Z_Axis"));
		actionRotate_Angle = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRotate_Angle->setObjectName(QStringLiteral("actionRotate_Angle"));
		actionGoto_Selection = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Selection->setObjectName(QStringLiteral("actionGoto_Selection"));
		CryIcon icon17;
		icon17.addFile(QStringLiteral("icons:General/Go_To_Selection.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionGoto_Selection->setIcon(icon17);
		actionMaterial_Assign_Current = new QCommandAction(nullptr, nullptr, MainWindow);
		actionMaterial_Assign_Current->setObjectName(QStringLiteral("actionMaterial_Assign_Current"));
		actionMaterial_Assign_Current->setIcon(CryIcon("icons:MaterialEditor/Material_Editor_Assign_To_Object.ico"));
		actionMaterial_Reset_to_Default = new QCommandAction(nullptr, nullptr, MainWindow);
		actionMaterial_Reset_to_Default->setObjectName(QStringLiteral("actionMaterial_Reset_to_Default"));
		actionMaterial_Reset_to_Default->setIcon(CryIcon("icons:MaterialEditor/Material_Editor_Reset_Material.ico"));
		actionMaterial_Get_from_Selected = new QCommandAction(nullptr, nullptr, MainWindow);
		actionMaterial_Get_from_Selected->setObjectName(QStringLiteral("actionMaterial_Get_from_Selected"));
		actionMaterial_Get_from_Selected->setIcon(CryIcon("icons:MaterialEditor/Material_Editor_Pick_From_Object.ico"));
		actionLighting_Regenerate_All_Cubemap = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLighting_Regenerate_All_Cubemap->setObjectName(QStringLiteral("actionLighting_Regenerate_All_Cubemap"));
		actionPick_Material = new QCommandAction(nullptr, nullptr, MainWindow);
		actionPick_Material->setObjectName(QStringLiteral("actionPick_Material"));
		actionPick_Material->setIcon(CryIcon("icons:General/Picker.ico"));
		actionGet_Physics_State = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGet_Physics_State->setObjectName(QStringLiteral("actionGet_Physics_State"));
		CryIcon icon18;
		icon18.addFile(QStringLiteral("icons:General/Get_Physics.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionGet_Physics_State->setIcon(icon18);
		actionReset_Physics_State = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReset_Physics_State->setObjectName(QStringLiteral("actionReset_Physics_State"));
		CryIcon icon19;
		icon19.addFile(QStringLiteral("icons:General/Reset_Physics.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionReset_Physics_State->setIcon(icon19);
		actionPhysics_Simulate_Objects = new QCommandAction(nullptr, nullptr, MainWindow);
		actionPhysics_Simulate_Objects->setObjectName(QStringLiteral("actionPhysics_Simulate_Objects"));
		CryIcon icon20;
		icon20.addFile(QStringLiteral("icons:General/Simulate_Physics.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionPhysics_Simulate_Objects->setIcon(icon20);
		actionPhysics_Generate_Joints = new QCommandAction(nullptr, nullptr, MainWindow);
		actionPhysics_Generate_Joints->setObjectName(QStringLiteral("actionPhysics_Generate_Joints"));
		actionPhysics_Generate_Joints->setIcon(CryIcon("icons:General/Physics_Generate_Joints.ico"));
		actionWireframe = new QCommandAction(nullptr, nullptr, MainWindow);
		actionWireframe->setObjectName(QStringLiteral("actionWireframe"));
		actionWireframe->setIcon(CryIcon("icons:Viewport/Modes/display_wireframe.ico"));
		actionRuler = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRuler->setObjectName(QStringLiteral("actionRuler"));
		actionRuler->setIcon(CryIcon("icons:Tools/tools-ruler.ico"));
		actionRuler->setCheckable(true);
		actionToggleGridSnapping = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_grid");
		actionToggleGridSnapping->setCheckable(true);
		actionToggleAngleSnapping = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_angle");
		actionToggleAngleSnapping->setCheckable(true);
		actionToggleScaleSnapping = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_scale");
		actionToggleScaleSnapping->setCheckable(true);
		actionToggleVertexSnapping = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_vertex");
		actionToggleVertexSnapping->setCheckable(true);
		actionToggleTerrainSnapping = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_terrain");
		actionToggleTerrainSnapping->setCheckable(true);
		actionToggleGeometrySnapping = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_geometry");
		actionToggleGeometrySnapping->setCheckable(true);
		actionToggleNormalSnapping = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_surface_normal");
		actionToggleNormalSnapping->setCheckable(true);
		actionAlign_To_Object = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_pivot");
		actionAlign_To_Object->setCheckable(true);
		QString name("Snap Settings");
		QString cmd = QString("general.open_pane '%1'").arg(name);
		QCommandAction* pSnapSettings = new QCommandAction(name.append("..."), (const char*)cmd.toLocal8Bit(), menuAlign_Snap);
		pSnapSettings->setIcon(CryIcon("icons:Viewport/viewport-snap-options.ico"));
		actionGoto_Position = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Position->setObjectName(QStringLiteral("actionGoto_Position"));
		actionGoto_Position->setIcon(CryIcon("icons:Tools/Go_To_Position.ico"));
		actionRemember_Location_1 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_1->setObjectName(QStringLiteral("actionRemember_Location_1"));
		actionRemember_Location_2 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_2->setObjectName(QStringLiteral("actionRemember_Location_2"));
		actionRemember_Location_3 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_3->setObjectName(QStringLiteral("actionRemember_Location_3"));
		actionRemember_Location_4 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_4->setObjectName(QStringLiteral("actionRemember_Location_4"));
		actionRemember_Location_5 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_5->setObjectName(QStringLiteral("actionRemember_Location_5"));
		actionRemember_Location_6 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_6->setObjectName(QStringLiteral("actionRemember_Location_6"));
		actionRemember_Location_7 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_7->setObjectName(QStringLiteral("actionRemember_Location_7"));
		actionRemember_Location_8 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_8->setObjectName(QStringLiteral("actionRemember_Location_8"));
		actionRemember_Location_9 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_9->setObjectName(QStringLiteral("actionRemember_Location_9"));
		actionRemember_Location_10 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_10->setObjectName(QStringLiteral("actionRemember_Location_10"));
		actionRemember_Location_11 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_11->setObjectName(QStringLiteral("actionRemember_Location_11"));
		actionRemember_Location_12 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionRemember_Location_12->setObjectName(QStringLiteral("actionRemember_Location_12"));
		actionGoto_Location_1 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_1->setObjectName(QStringLiteral("actionGoto_Location_1"));
		actionGoto_Location_2 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_2->setObjectName(QStringLiteral("actionGoto_Location_2"));
		actionGoto_Location_3 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_3->setObjectName(QStringLiteral("actionGoto_Location_3"));
		actionGoto_Location_4 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_4->setObjectName(QStringLiteral("actionGoto_Location_4"));
		actionGoto_Location_5 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_5->setObjectName(QStringLiteral("actionGoto_Location_5"));
		actionGoto_Location_6 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_6->setObjectName(QStringLiteral("actionGoto_Location_6"));
		actionGoto_Location_7 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_7->setObjectName(QStringLiteral("actionGoto_Location_7"));
		actionGoto_Location_8 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_8->setObjectName(QStringLiteral("actionGoto_Location_8"));
		actionGoto_Location_9 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_9->setObjectName(QStringLiteral("actionGoto_Location_9"));
		actionGoto_Location_10 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_10->setObjectName(QStringLiteral("actionGoto_Location_10"));
		actionGoto_Location_11 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_11->setObjectName(QStringLiteral("actionGoto_Location_11"));
		actionGoto_Location_12 = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGoto_Location_12->setObjectName(QStringLiteral("actionGoto_Location_12"));
		actionCamera_Default = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCamera_Default->setObjectName(QStringLiteral("actionCamera_Default"));
		actionCamera_Sequence = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCamera_Sequence->setObjectName(QStringLiteral("actionCamera_Sequence"));
		actionCamera_Selected_Object = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCamera_Selected_Object->setObjectName(QStringLiteral("actionCamera_Selected_Object"));
		actionCamera_Cycle = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCamera_Cycle->setObjectName(QStringLiteral("actionCamera_Cycle"));
		actionShow_Hide_Helpers = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_display_helpers");
		actionShow_Hide_Helpers->setCheckable(true);
		actionCycle_Display_Info = GetIEditorImpl()->GetICommandManager()->GetAction("general.cycle_displayinfo");
		actionToggle_Fullscreen_Viewport = new QCommandAction(nullptr, nullptr, MainWindow);
		actionToggle_Fullscreen_Viewport->setObjectName(QStringLiteral("actionToggle_Fullscreen_Viewport"));
		actionGroup = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGroup->setObjectName(QStringLiteral("actionGroup"));
		actionGroup->setIcon(CryIcon("icons:General/Group.ico"));
		actionUngroup = new QCommandAction(nullptr, nullptr, MainWindow);
		actionUngroup->setObjectName(QStringLiteral("actionUngroup"));
		actionUngroup->setIcon(CryIcon("icons:General/UnGroup.ico"));
		actionGroup_Open = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGroup_Open->setObjectName(QStringLiteral("actionGroup_Open"));
		actionGroup_Close = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGroup_Close->setObjectName(QStringLiteral("actionGroup_Close"));
		actionGroup_Attach = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGroup_Attach->setObjectName(QStringLiteral("actionGroup_Attach"));
		actionGroup_Detach = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGroup_Detach->setObjectName(QStringLiteral("actionGroup_Detach"));
		actionGroup_DetachToRoot = new QCommandAction(nullptr, nullptr, MainWindow);
		actionGroup_DetachToRoot->setObjectName(QStringLiteral("actionGroup_DetachToRoot"));
		actionReload_Terrain = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_Terrain->setObjectName(QStringLiteral("actionReload_Terrain"));
		actionSwitch_to_Game = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSwitch_to_Game->setObjectName(QStringLiteral("actionSwitch_to_Game"));
		actionSwitch_to_Game->setIcon(CryIcon("icons:Game/Game_Play.ico"));
		actionSuspend_Game_Input = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSuspend_Game_Input->setObjectName(QStringLiteral("actionSuspend_Game_Input"));
		actionEnable_Physics_AI = new QCommandAction(nullptr, nullptr, MainWindow);
		actionEnable_Physics_AI->setObjectName(QStringLiteral("actionEnable_Physics_AI"));
		actionEnable_Physics_AI->setCheckable(true);
		actionEnable_Physics_AI->setIcon(CryIcon("icons:common/general_physics_play.ico"));
		actionSynchronize_Player_with_Camera = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSynchronize_Player_with_Camera->setObjectName(QStringLiteral("actionSynchronize_Player_with_Camera"));
		actionSynchronize_Player_with_Camera->setCheckable(true);

		QCommandAction* actionAIShowNavigationAreas = pCommandManager->GetCommandAction("ai.show_navigation_areas");
		if (actionAIShowNavigationAreas)
		{
			actionAIShowNavigationAreas->setCheckable(true);
			actionAIShowNavigationAreas->setChecked(gAINavigationPreferences.navigationShowAreas());
		}
		QCommandAction* actionAIVisualizeNavigationAccessibility = pCommandManager->GetCommandAction("ai.visualize_navigation_accessibility");
		if (actionAIVisualizeNavigationAccessibility)
		{
			actionAIVisualizeNavigationAccessibility->setCheckable(true);
			actionAIVisualizeNavigationAccessibility->setChecked(gAINavigationPreferences.visualizeNavigationAccessibility());
		}

		actionReload_All_Scripts = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_All_Scripts->setObjectName(QStringLiteral("actionReload_All_Scripts"));
		actionReload_Entity_Scripts = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_Entity_Scripts->setObjectName(QStringLiteral("actionReload_Entity_Scripts"));
		actionReload_Item_Scripts = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_Item_Scripts->setObjectName(QStringLiteral("actionReload_Item_Scripts"));
		actionReload_AI_Scripts = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_AI_Scripts->setObjectName(QStringLiteral("actionReload_AI_Scripts"));
		actionReload_UI_Scripts = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_UI_Scripts->setObjectName(QStringLiteral("actionReload_UI_Scripts"));
		actionReload_Archetypes = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_Archetypes->setObjectName(QStringLiteral("actionReload_Archetypes"));
		actionReload_Texture_Shaders = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_Texture_Shaders->setObjectName(QStringLiteral("actionReload_Texture_Shaders"));
		actionReload_Geometry = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReload_Geometry->setObjectName(QStringLiteral("actionReload_Geometry"));
		actionCheck_Level_for_Errors = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCheck_Level_for_Errors->setObjectName(QStringLiteral("actionCheck_Level_for_Errors"));
		actionCheck_Object_Positions = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCheck_Object_Positions->setObjectName(QStringLiteral("actionCheck_Object_Positions"));
		actionSave_Level_Statistics = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSave_Level_Statistics->setObjectName(QStringLiteral("actionSave_Level_Statistics"));
		actionCompile_Script = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCompile_Script->setObjectName(QStringLiteral("actionCompile_Script"));
		actionReduce_Working_Set = new QCommandAction(nullptr, nullptr, MainWindow);
		actionReduce_Working_Set->setObjectName(QStringLiteral("actionReduce_Working_Set"));
		actionUpdate_Procedural_Vegetation = new QCommandAction(nullptr, nullptr, MainWindow);
		actionUpdate_Procedural_Vegetation->setObjectName(QStringLiteral("actionUpdate_Procedural_Vegetation"));
		actionSave = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSave->setObjectName(QStringLiteral("actionSave"));
		CryIcon icon22;
		icon22.addFile(QStringLiteral("icons:General/File_Save.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionSave->setIcon(icon22);
		actionSave_Selected_Objects = new QCommandAction(nullptr, nullptr, MainWindow);
		actionSave_Selected_Objects->setObjectName(QStringLiteral("actionSave_Selected_Objects"));
		CryIcon icon23;
		icon23.addFile(QStringLiteral("icons:MaterialEditor/Material_Save.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionSave_Selected_Objects->setIcon(icon23);
		actionLoad_Selected_Objects = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLoad_Selected_Objects->setObjectName(QStringLiteral("actionLoad_Selected_Objects"));
		CryIcon icon24;
		icon24.addFile(QStringLiteral("icons:MaterialEditor/Material_Load.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionLoad_Selected_Objects->setIcon(icon24);
		actionLock_X_Axis = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLock_X_Axis->setObjectName(QStringLiteral("actionLock_X_Axis"));
		actionLock_X_Axis->setCheckable(true);
		CryIcon icon25;
		icon25.addFile(QStringLiteral("icons:Navigation/Axis_X.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionLock_X_Axis->setIcon(icon25);
		actionLock_Y_Axis = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLock_Y_Axis->setObjectName(QStringLiteral("actionLock_Y_Axis"));
		actionLock_Y_Axis->setCheckable(true);
		CryIcon icon26;
		icon26.addFile(QStringLiteral("icons:Navigation/Axis_Y.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionLock_Y_Axis->setIcon(icon26);
		actionLock_Z_Axis = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLock_Z_Axis->setObjectName(QStringLiteral("actionLock_Z_Axis"));
		actionLock_Z_Axis->setCheckable(true);
		CryIcon icon27;
		icon27.addFile(QStringLiteral("icons:Navigation/Axis_Z.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionLock_Z_Axis->setIcon(icon27);
		actionLock_XY_Axis = new QCommandAction(nullptr, nullptr, MainWindow);
		actionLock_XY_Axis->setObjectName(QStringLiteral("actionLock_XY_Axis"));
		actionLock_XY_Axis->setCheckable(true);
		CryIcon icon28;
		icon28.addFile(QStringLiteral("icons:Navigation/Axis_XY.ico"), QSize(), CryIcon::Normal, CryIcon::Off);
		actionLock_XY_Axis->setIcon(icon28);
		actionCoordinates_View = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCoordinates_View->setObjectName(QStringLiteral("actionCoordinates_View"));
		actionCoordinates_View->setCheckable(true);
		actionCoordinates_View->setIcon(CryIcon("icons:Navigation/Coordinates_View.ico"));
		actionCoordinates_Local = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCoordinates_Local->setObjectName(QStringLiteral("actionCoordinates_Local"));
		actionCoordinates_Local->setCheckable(true);
		actionCoordinates_Local->setIcon(CryIcon("icons:Navigation/Coordinates_Local.ico"));
		actionCoordinates_Parent = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCoordinates_Parent->setObjectName(QStringLiteral("actionCoordinates_Parent"));
		actionCoordinates_Parent->setCheckable(true);
		actionCoordinates_Parent->setIcon(CryIcon("icons:Navigation/Coordinates_Parent.ico"));
		actionCoordinates_World = new QCommandAction(nullptr, nullptr, MainWindow);
		actionCoordinates_World->setObjectName(QStringLiteral("actionCoordinates_World"));
		actionCoordinates_World->setCheckable(true);
		actionCoordinates_World->setIcon(CryIcon("icons:Navigation/Coordinates_World.ico"));
		RefCoordSys value = GetIEditorImpl()->GetReferenceCoordSys();
		switch (value)
		{
		case COORDS_WORLD:
			actionCoordinates_World->setChecked(true);
			break;
		case COORDS_PARENT:
			actionCoordinates_Parent->setChecked(true);
			break;
		case COORDS_VIEW:
			actionCoordinates_View->setChecked(true);
			break;
		case COORDS_LOCAL:
			actionCoordinates_Local->setChecked(true);
			break;
		}
		actionResolve_Missing_Objects = new QCommandAction(nullptr, nullptr, MainWindow);
		actionResolve_Missing_Objects->setObjectName(QStringLiteral("actionResolve_Missing_Objects"));

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
		menuFile = new QMenu(menubar);
		menuFile->setObjectName(QStringLiteral("menuFile"));
		menuRecent_Files = new QMenu(menuFile);
		menuRecent_Files->setObjectName(QStringLiteral("menuRecent_Files"));
		menuEdit = new QMenu(menubar);
		menuEdit->setObjectName(QStringLiteral("menuEdit"));
		menuEditing_Mode = new QMenu(menuEdit);
		menuEditing_Mode->setObjectName(QStringLiteral("menuEditing_Mode"));
		menuConstrain = new QMenu(menuEdit);
		menuConstrain->setObjectName(QStringLiteral("menuConstrain"));
		menuFast_Rotate = new QMenu(menuEdit);
		menuFast_Rotate->setObjectName(QStringLiteral("menuFast_Rotate"));
		menuAlign_Snap = new QMenu(menuEdit);
		menuAlign_Snap->setObjectName(QStringLiteral("menuAlign_Snap"));

		menuLevel = new QMenu(menubar);
		menuLevel->setObjectName(QStringLiteral("menuLevel"));
		menuPhysics = new QMenu(menuLevel);
		menuPhysics->setObjectName(QStringLiteral("menuPhysics"));
		menuGroup = new QMenu(menuLevel);
		menuGroup->setObjectName(QStringLiteral("menuGroup"));
		menuLink = new QMenu(menuLevel);
		menuLink->setObjectName(QStringLiteral("menuLink"));
		menuPrefabs = new QMenu(menuLevel);
		menuPrefabs->setObjectName(QStringLiteral("menuPrefabs"));
		menuSelection = new QMenu(menuLevel);
		menuSelection->setObjectName(QStringLiteral("menuSelection"));
		menuSelection_Mask = new QSelectionMaskMenu();
		menuSelection_Mask->setObjectName(QStringLiteral("menuSelection_Mask"));
		menuLighting = new QMenu(menuLevel);
		menuLighting->setObjectName(QStringLiteral("menuLighting"));
		menuMaterial = new QMenu(menuLevel);
		menuMaterial->setObjectName(QStringLiteral("menuMaterial"));

		menuDisplay = new QMenu(menubar);
		menuDisplay->setObjectName(QStringLiteral("menuDisplay"));
		menuLocation = new QMenu(menuDisplay);
		menuLocation->setObjectName(QStringLiteral("menuLocation"));
		menuRemember_Location = new QMenu(menuLocation);
		menuRemember_Location->setObjectName(QStringLiteral("menuRemember_Location"));
		menuGoto_Location = new QMenu(menuLocation);
		menuGoto_Location->setObjectName(QStringLiteral("menuGoto_Location"));
		menuSwitch_Camera = new QMenu(menuDisplay);
		menuSwitch_Camera->setObjectName(QStringLiteral("menuSwitch_Camera"));
		menuGame = new QMenu(menubar);
		menuGame->setObjectName(QStringLiteral("menuGame"));
		menuTools = new QMenu(menubar);
		menuTools->setObjectName(QStringLiteral("menuTools"));
		menuLayout = new QMenu(menubar);
		menuLayout->setObjectName(QStringLiteral("menuLayout"));
		menuHelp = new QMenu(menubar);
		menuHelp->setObjectName(QStringLiteral("menuHelp"));
		menuGraphics = new QMenu(menuDisplay);
		menuGraphics->setObjectName(QStringLiteral("menuGraphics"));
		menuAudio = new QMenu(menuGame);
		menuAudio->setObjectName(QStringLiteral("menuAudio"));
		menuAI = new QMenu(menuGame);
		menuAI->setObjectName(QStringLiteral("menuAI"));
		menuAINavigationUpdate = new QMenu(menuAI);
		menuAINavigationUpdate->setObjectName(QStringLiteral("menuAINavigationUpdate"));
		menuDebug = new QMenu(menubar);
		menuDebug->setObjectName(QStringLiteral("menuDebug"));
		menuDebug_Agent_Type = new QNavigationAgentTypeMenu();
		menuDebug_Agent_Type->setObjectName(QStringLiteral("menuDebug_Agent_Type"));
		menuRegenerate_MNM_Agent_Type = new QMNMRegenerationAgentTypeMenu();
		menuRegenerate_MNM_Agent_Type->setObjectName(QStringLiteral("menuRegenerate_MNM_Agent_Type"));
		menuReload_Scripts = new QMenu(menuDebug);
		menuReload_Scripts->setObjectName(QStringLiteral("menuReload_Scripts"));
		menuAdvanced = new QMenu(menuDebug);
		menuAdvanced->setObjectName(QStringLiteral("menuAdvanced"));
		menuCoordinates = new QMenu(menuEdit);
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
		menuFile->addAction(actionNew);
		menuFile->addAction(actionOpen);
		menuFile->addAction(actionSave);
		menuFile->addAction(actionSave_As);
		menuFile->addSeparator();
		menuFile->addAction(actionToggle_Content_Browser);
		menuFile->addSeparator();
		menuFile->addAction(actionExport_to_Engine);
		menuFile->addAction(actionExport_Occlusion_Mesh);

		QAction* actionExportSvogiData = pCommandManager->GetCommandAction("general.export_svogi_data");
		if (actionExportSvogiData)
		{
			menuFile->addAction(actionExportSvogiData);
		}

		menuFile->addSeparator();
		menuFile->addAction(menuRecent_Files->menuAction());
		menuFile->addSeparator();
		menuFile->addAction(actionExit);
		menuEdit->addAction(actionUndo);
		menuEdit->addAction(actionRedo);
		menuEdit->addSeparator();
		menuEdit->addAction(actionDelete);
		menuEdit->addAction(actionDuplicate);
		menuEdit->addAction(actionCopy);
		menuEdit->addAction(actionCut);
		menuEdit->addAction(actionPaste);
		menuEdit->addSeparator();
		menuEdit->addAction(actionFind);
		menuEdit->addAction(menuAlign_Snap->menuAction());
		menuEdit->addAction(menuEditing_Mode->menuAction());
		menuEdit->addAction(menuConstrain->menuAction());
		menuEdit->addAction(menuFast_Rotate->menuAction());
		menuEdit->addSeparator();
		menuEdit->addMenu(menuCoordinates);
		menuEdit->addSeparator();
		menuEditing_Mode->addAction(actionSelect_Mode);
		menuEditing_Mode->addAction(actionMove);
		menuEditing_Mode->addAction(actionRotate);
		menuEditing_Mode->addAction(actionScale);
		menuConstrain->addAction(actionLock_X_Axis);
		menuConstrain->addAction(actionLock_Y_Axis);
		menuConstrain->addAction(actionLock_Z_Axis);
		menuConstrain->addAction(actionLock_XY_Axis);
		menuFast_Rotate->addAction(actionRotate_X_Axis);
		menuFast_Rotate->addAction(actionRotate_Y_Axis);
		menuFast_Rotate->addAction(actionRotate_Z_Axis);
		menuFast_Rotate->addAction(actionRotate_Angle);
		menuAlign_Snap->addAction(actionAlign_To_Grid);
		menuAlign_Snap->addAction(actionAlign_To_Object);
		menuAlign_Snap->addSeparator();
		menuAlign_Snap->addAction(actionToggleGridSnapping);
		menuAlign_Snap->addAction(actionToggleAngleSnapping);
		menuAlign_Snap->addAction(actionToggleScaleSnapping);
		menuAlign_Snap->addAction(actionToggleVertexSnapping);
		menuAlign_Snap->addAction(actionToggleTerrainSnapping);
		menuAlign_Snap->addAction(actionToggleGeometrySnapping);
		menuAlign_Snap->addAction(actionToggleNormalSnapping);
		menuAlign_Snap->addAction(pSnapSettings);

		menuLevel->addAction(actionSave_Selected_Objects);
		menuLevel->addAction(actionLoad_Selected_Objects);
		menuLevel->addSeparator();
		menuLevel->addAction(menuLink->menuAction());
		menuLevel->addAction(menuGroup->menuAction());
		menuLevel->addAction(menuPrefabs->menuAction());
		menuLevel->addSeparator();
		menuLevel->addAction(menuSelection->menuAction());

		{
			//Intentionally using new menu syntax instead of mimicking generated code
			CAbstractMenu builder;

			builder.AddCommandAction("level.hide_all_layers");
			builder.AddCommandAction("level.show_all_layers");

			const int sec = builder.GetNextEmptySection();

			builder.AddCommandAction("level.freeze_all_layers", sec);
			builder.AddCommandAction("level.unfreeze_all_layers", sec);
			builder.AddCommandAction("level.freeze_read_only_layers", sec);

			QMenu* menuLayers = menuLevel->addMenu(QObject::tr("Layers"));
			builder.Build(MenuWidgetBuilders::CMenuBuilder(menuLayers));
		}

		menuLevel->addAction(menuMaterial->menuAction());
		menuLevel->addAction(menuLighting->menuAction());
		menuLevel->addAction(menuPhysics->menuAction());
		menuLevel->addSeparator();
		menuLevel->addAction(actionGoto_Position);
		menuLevel->addAction(actionGoto_Selection);
		menuLevel->addSeparator();
		menuLevel->addAction(actionRuler);
		menuLevel->addSeparator();
		menuLevel->addAction(actionSave_Level_Resources);
		menuLevel->addAction(actionExport_Selected_Objects);

		menuPhysics->addAction(actionGet_Physics_State);
		menuPhysics->addAction(actionReset_Physics_State);
		menuPhysics->addAction(actionPhysics_Simulate_Objects);
		menuPhysics->addAction(actionPhysics_Generate_Joints);
		menuGroup->addAction(actionGroup);
		menuGroup->addAction(actionUngroup);
		menuGroup->addSeparator();
		menuGroup->addAction(actionGroup_Open);
		menuGroup->addAction(actionGroup_Close);
		menuGroup->addSeparator();
		menuGroup->addAction(actionGroup_Attach);
		menuGroup->addAction(actionGroup_Detach);
		menuGroup->addAction(actionGroup_DetachToRoot);
		menuLink->addAction(actionLink);
		menuLink->addAction(actionUnlink);
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.create_from_selection"));
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.add_to_prefab"));
		menuPrefabs->addSeparator();
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.open"));
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.close"));
		menuPrefabs->addSeparator();
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.open_all"));
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.close_all"));
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.reload_all"));
		menuPrefabs->addSeparator();
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.extract_all"));
		menuPrefabs->addAction(pCommandManager->GetCommandAction("prefab.clone_all"));
		menuPrefabs->addSeparator();
		menuSelection->addAction(pCommandManager->GetCommandAction("general.select_all"));
		menuSelection->addAction(actionSelect_None);
		menuSelection->addAction(actionSelect_Invert);
		menuSelection->addAction(actionLock_Selection);
		menuSelection->addMenu(menuSelection_Mask);
		menuSelection->addSeparator();
		menuSelection->addAction(actionHide_Selection);
		menuSelection->addAction(actionUnhide_All);
		menuSelection->addSeparator();
		menuSelection->addAction(actionFreeze_Selection);
		menuSelection->addAction(actionUnfreeze_All);
		menuMaterial->addAction(actionMaterial_Assign_Current);
		menuMaterial->addAction(actionMaterial_Reset_to_Default);
		menuMaterial->addAction(actionMaterial_Get_from_Selected);
		menuMaterial->addAction(actionPick_Material);
		menuLighting->addAction(actionLighting_Regenerate_All_Cubemap);
		menuDisplay->addAction(actionWireframe);
		menuDisplay->addAction(menuGraphics->menuAction());
		menuDisplay->addSeparator();
		menuDisplay->addAction(actionShow_Hide_Helpers);
		menuDisplay->addAction(actionCycle_Display_Info);
		menuDisplay->addAction(menuLocation->menuAction());
		menuDisplay->addAction(menuSwitch_Camera->menuAction());
		QAction* cameraSpeedHeightRelative = GetIEditorImpl()->GetICommandManager()->GetAction("camera.toggle_speed_height_relative");
		cameraSpeedHeightRelative->setCheckable(true);
		menuDisplay->addAction(cameraSpeedHeightRelative);
		QAction* cameraToggleTerrainCollisions = GetIEditorImpl()->GetICommandManager()->GetAction("camera.toggle_terrain_collisions");
		cameraToggleTerrainCollisions->setCheckable(true);
		menuDisplay->addAction(cameraToggleTerrainCollisions);
		QAction* toggleObjectCollisions = GetIEditorImpl()->GetICommandManager()->GetAction("camera.toggle_object_collisions");
		toggleObjectCollisions->setCheckable(true);
		menuDisplay->addAction(toggleObjectCollisions);
		menuDisplay->addAction(actionToggle_Fullscreen_Viewport);
		menuLocation->addAction(menuRemember_Location->menuAction());
		menuLocation->addAction(menuGoto_Location->menuAction());
		menuRemember_Location->addAction(actionRemember_Location_1);
		menuRemember_Location->addAction(actionRemember_Location_2);
		menuRemember_Location->addAction(actionRemember_Location_3);
		menuRemember_Location->addAction(actionRemember_Location_4);
		menuRemember_Location->addAction(actionRemember_Location_5);
		menuRemember_Location->addAction(actionRemember_Location_6);
		menuRemember_Location->addAction(actionRemember_Location_7);
		menuRemember_Location->addAction(actionRemember_Location_8);
		menuRemember_Location->addAction(actionRemember_Location_9);
		menuRemember_Location->addAction(actionRemember_Location_10);
		menuRemember_Location->addAction(actionRemember_Location_11);
		menuRemember_Location->addAction(actionRemember_Location_12);
		menuGoto_Location->addAction(actionGoto_Location_1);
		menuGoto_Location->addAction(actionGoto_Location_2);
		menuGoto_Location->addAction(actionGoto_Location_3);
		menuGoto_Location->addAction(actionGoto_Location_4);
		menuGoto_Location->addAction(actionGoto_Location_5);
		menuGoto_Location->addAction(actionGoto_Location_6);
		menuGoto_Location->addAction(actionGoto_Location_7);
		menuGoto_Location->addAction(actionGoto_Location_8);
		menuGoto_Location->addAction(actionGoto_Location_9);
		menuGoto_Location->addAction(actionGoto_Location_10);
		menuGoto_Location->addAction(actionGoto_Location_11);
		menuGoto_Location->addAction(actionGoto_Location_12);
		menuSwitch_Camera->addAction(actionCamera_Default);
		menuSwitch_Camera->addAction(actionCamera_Sequence);
		menuSwitch_Camera->addAction(actionCamera_Selected_Object);
		menuSwitch_Camera->addAction(actionCamera_Cycle);
		menuGame->addAction(actionSwitch_to_Game);
		menuGame->addAction(actionSuspend_Game_Input);
		menuGame->addAction(actionEnable_Physics_AI);
		menuGame->addSeparator();
		menuGame->addAction(menuAudio->menuAction());
		menuGame->addAction(menuAI->menuAction());
		menuGame->addSeparator();
		menuGame->addAction(actionSynchronize_Player_with_Camera);
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
		menuAI->addAction(actionAIVisualizeNavigationAccessibility);
		menuAI->addAction(actionAIShowNavigationAreas);
		menuAI->addSeparator();
		menuAI->addAction(pCommandManager->GetCommandAction("ai.generate_cover_surfaces"));
		menuDebug->addAction(menuReload_Scripts->menuAction());
		menuDebug->addAction(actionReload_Texture_Shaders);
		menuDebug->addAction(actionReload_Archetypes);
		menuDebug->addAction(actionReload_Geometry);
		menuDebug->addAction(actionReload_Terrain);
		menuDebug->addAction(actionCheck_Level_for_Errors);
		menuDebug->addAction(actionCheck_Object_Positions);
		menuDebug->addAction(actionResolve_Missing_Objects);
		menuDebug->addAction(actionSave_Level_Statistics);
		menuDebug->addSeparator();
		menuDebug->addAction(menuAdvanced->menuAction());
		menuDebug->addSeparator();
		menuDebug->addAction(actionShow_Log_File);
		menuReload_Scripts->addAction(actionReload_All_Scripts);
		menuReload_Scripts->addAction(actionReload_Entity_Scripts);
		menuReload_Scripts->addAction(actionReload_Item_Scripts);
		menuReload_Scripts->addAction(actionReload_AI_Scripts);
		menuReload_Scripts->addAction(actionReload_UI_Scripts);
		menuAdvanced->addAction(actionCompile_Script);
		menuAdvanced->addAction(actionReduce_Working_Set);
		menuAdvanced->addAction(actionUpdate_Procedural_Vegetation);
		menuCoordinates->addAction(actionCoordinates_View);
		menuCoordinates->addAction(actionCoordinates_Local);
		menuCoordinates->addAction(actionCoordinates_Parent);
		menuCoordinates->addAction(actionCoordinates_World);

		retranslateUi(MainWindow);

		QMetaObject::connectSlotsByName(MainWindow);
	} // setupUi

	void retranslateUi(QMainWindow* MainWindow)
	{
		actionNew->setText(QApplication::translate("MainWindow", "&New...", 0));
		actionNew->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "New", 0)));
		actionNew->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.new", 0)));
		actionOpen->setText(QApplication::translate("MainWindow", "&Open...", 0));
		actionOpen->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "Open", 0)));
		actionOpen->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.open", 0)));
		actionSave_As->setText(QApplication::translate("MainWindow", "Save As...", 0));
		actionSave_As->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "SaveAs", 0)));
		actionSave_As->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.save_as", 0)));
		actionToggle_Content_Browser->setProperty("command", QVariant(QApplication::translate("MainWindow", "asset.toggle_browser", 0)));
		actionExport_to_Engine->setText(QApplication::translate("MainWindow", "Export to Engine", 0));
		actionExport_to_Engine->setShortcuts(CKeyboardShortcut("F7; Ctrl+E").ToKeySequence());
		actionExport_Occlusion_Mesh->setText(QApplication::translate("MainWindow", "Export Occlusion Mesh", 0));
		actionSave_Level_Resources->setText(QApplication::translate("MainWindow", "Save Level Resources", 0));
		actionExport_Selected_Objects->setText(QApplication::translate("MainWindow", "Export Selected Objects", 0));
		actionShow_Log_File->setText(QApplication::translate("MainWindow", "Show Log File", 0));
		actionExit->setText(QApplication::translate("MainWindow", "Exit", 0));
		actionExit->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.exit", 0)));
		actionExit->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "Quit", 0)));
		actionExit->setProperty("menurole", QVariant(QApplication::translate("MainWindow", "QuitRole", 0)));
		actionUndo->setText(QApplication::translate("MainWindow", "Undo", 0));
		actionUndo->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "Undo", 0)));
		actionUndo->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.undo", 0)));
		actionRedo->setText(QApplication::translate("MainWindow", "Redo", 0));
		actionRedo->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "Redo", 0)));
		actionRedo->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.redo", 0)));
		actionDelete->setText(QApplication::translate("MainWindow", "Delete", 0));
		actionDelete->setShortcut(QApplication::translate("MainWindow", "Del", 0));
		actionDelete->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "Delete", 0)));
		actionDelete->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.delete", 0)));
		actionDuplicate->setText(QApplication::translate("MainWindow", "Duplicate", 0));
		actionDuplicate->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.duplicate", 0)));
		actionCopy->setText(QApplication::translate("MainWindow", "Copy", 0));
		actionCopy->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.copy", 0)));
		actionCut->setText(QApplication::translate("MainWindow", "Cut", 0));
		actionCut->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.cut", 0)));
		actionPaste->setText(QApplication::translate("MainWindow", "Paste", 0));
		actionPaste->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.paste", 0)));
		actionHide_Selection->setText(QApplication::translate("MainWindow", "Hide Selection", 0));
		actionHide_Selection->setShortcut(QApplication::translate("MainWindow", "H", 0));
		actionUnhide_All->setText(QApplication::translate("MainWindow", "Unhide All", 0));
		actionUnhide_All->setShortcut(QApplication::translate("MainWindow", "Shift+H", 0));
		actionFreeze_Selection->setText(QApplication::translate("MainWindow", "Freeze Selection", 0));
		actionFreeze_Selection->setShortcut(QApplication::translate("MainWindow", "F", 0));
		actionUnfreeze_All->setText(QApplication::translate("MainWindow", "Unfreeze All Objects", 0));
		actionUnfreeze_All->setShortcut(QApplication::translate("MainWindow", "Shift+F", 0));
		actionSelect_None->setText(QApplication::translate("MainWindow", "Select None", 0));
		actionSelect_None->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "SelectNone", 0)));
		actionSelect_Invert->setText(QApplication::translate("MainWindow", "Select Invert", 0));
		actionFind->setText(QApplication::translate("MainWindow", "Find...", 0));
		actionFind->setShortcut(QApplication::translate("MainWindow", "Ctrl+F", 0));
		actionFind->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.find", 0)));
		actionFindPrevious->setText(QApplication::translate("MainWindow", "Find Previous", 0));
		actionFindPrevious->setShortcut(QApplication::translate("MainWindow", "Shift+F3", 0));
		actionFindPrevious->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.find_previous", 0)));
		actionFindNext->setText(QApplication::translate("MainWindow", "Find Next", 0));
		actionFindNext->setShortcut(QApplication::translate("MainWindow", "F3", 0));
		actionFindNext->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.find_next", 0)));
		actionLock_Selection->setText(QApplication::translate("MainWindow", "Lock Selection", 0));
		actionLock_Selection->setShortcut(QApplication::translate("MainWindow", "Ctrl+Shift+Space", 0));
		actionSelect_Mode->setText(QApplication::translate("MainWindow", "Select", 0));
		actionSelect_Mode->setShortcut(QApplication::translate("MainWindow", "4", 0));
		actionSelect_Mode->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "EditModeGroup", 0)));
		actionMove->setText(QApplication::translate("MainWindow", "Move", 0));
		actionMove->setShortcut(QApplication::translate("MainWindow", "1", 0));
		actionMove->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "EditModeGroup", 0)));
		actionRotate->setText(QApplication::translate("MainWindow", "Rotate", 0));
		actionRotate->setShortcut(QApplication::translate("MainWindow", "2", 0));
		actionRotate->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "EditModeGroup", 0)));
		actionScale->setText(QApplication::translate("MainWindow", "Scale", 0));
		actionScale->setShortcut(QApplication::translate("MainWindow", "3", 0));
		actionScale->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "EditModeGroup", 0)));
		actionLink->setText(QApplication::translate("MainWindow", "Link", 0));
		actionUnlink->setText(QApplication::translate("MainWindow", "Unlink", 0));
		actionAlign_To_Grid->setText(QApplication::translate("MainWindow", "Align to Grid", 0));
		actionAlign_To_Object->setText(QApplication::translate("MainWindow", "Snap to Pivot", 0));
		actionRotate_X_Axis->setText(QApplication::translate("MainWindow", "Rotate X Axis", 0));
		actionRotate_Y_Axis->setText(QApplication::translate("MainWindow", "Rotate Y Axis", 0));
		actionRotate_Z_Axis->setText(QApplication::translate("MainWindow", "Rotate Z Axis", 0));
		actionRotate_Angle->setText(QApplication::translate("MainWindow", "Rotate Angle...", 0));
		actionGoto_Selection->setText(QApplication::translate("MainWindow", "Go to Selection", 0));
		actionGoto_Selection->setProperty("command", QVariant(QApplication::translate("MainWindow", "level.go_to_selection", 0)));
		actionMaterial_Assign_Current->setText(QApplication::translate("MainWindow", "Assign Current", 0));
		actionMaterial_Reset_to_Default->setText(QApplication::translate("MainWindow", "Reset to Default", 0));
		actionMaterial_Get_from_Selected->setText(QApplication::translate("MainWindow", "Get from Selected", 0));
		actionLighting_Regenerate_All_Cubemap->setText(QApplication::translate("MainWindow", "Regenerate All Cubemaps", 0));
		actionLighting_Regenerate_All_Cubemap->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.generate_all_cubemaps", 0)));
		actionPick_Material->setText(QApplication::translate("MainWindow", "Pick Material", 0));
		actionGet_Physics_State->setText(QApplication::translate("MainWindow", "Get Physics State", 0));
		actionReset_Physics_State->setText(QApplication::translate("MainWindow", "Reset Physics State", 0));
		actionPhysics_Simulate_Objects->setText(QApplication::translate("MainWindow", "Simulate Objects", 0));
		actionPhysics_Generate_Joints->setText(QApplication::translate("MainWindow", "Generate Breakable Joints", 0));
		actionWireframe->setText(QApplication::translate("MainWindow", "Wireframe/Solid Mode", 0));
		actionWireframe->setShortcut(QApplication::translate("MainWindow", "Alt+W", 0));
		actionRuler->setText(QApplication::translate("MainWindow", "Ruler", 0));
		actionGoto_Position->setText(QApplication::translate("MainWindow", "Go to Position", 0));
		actionRemember_Location_1->setText(QApplication::translate("MainWindow", "Remember Location 1", 0));
		actionRemember_Location_1->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F1", 0));
		actionRemember_Location_2->setText(QApplication::translate("MainWindow", "Remember Location 2", 0));
		actionRemember_Location_2->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F2", 0));
		actionRemember_Location_3->setText(QApplication::translate("MainWindow", "Remember Location 3", 0));
		actionRemember_Location_3->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F3", 0));
		actionRemember_Location_4->setText(QApplication::translate("MainWindow", "Remember Location 4", 0));
		actionRemember_Location_4->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F4", 0));
		actionRemember_Location_5->setText(QApplication::translate("MainWindow", "Remember Location 5", 0));
		actionRemember_Location_5->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F5", 0));
		actionRemember_Location_6->setText(QApplication::translate("MainWindow", "Remember Location 6", 0));
		actionRemember_Location_6->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F6", 0));
		actionRemember_Location_7->setText(QApplication::translate("MainWindow", "Remember Location 7", 0));
		actionRemember_Location_7->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F7", 0));
		actionRemember_Location_8->setText(QApplication::translate("MainWindow", "Remember Location 8", 0));
		actionRemember_Location_8->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F8", 0));
		actionRemember_Location_9->setText(QApplication::translate("MainWindow", "Remember Location 9", 0));
		actionRemember_Location_9->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F9", 0));
		actionRemember_Location_10->setText(QApplication::translate("MainWindow", "Remember Location 10", 0));
		actionRemember_Location_10->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F10", 0));
		actionRemember_Location_11->setText(QApplication::translate("MainWindow", "Remember Location 11", 0));
		actionRemember_Location_11->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F11", 0));
		actionRemember_Location_12->setText(QApplication::translate("MainWindow", "Remember Location 12", 0));
		actionRemember_Location_12->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+F12", 0));
		actionGoto_Location_1->setText(QApplication::translate("MainWindow", "Go to Location 1", 0));
		actionGoto_Location_1->setShortcut(QApplication::translate("MainWindow", "Ctrl+F1", 0));
		actionGoto_Location_2->setText(QApplication::translate("MainWindow", "Go to Location 2", 0));
		actionGoto_Location_2->setShortcut(QApplication::translate("MainWindow", "Ctrl+F2", 0));
		actionGoto_Location_3->setText(QApplication::translate("MainWindow", "Go to Location 3", 0));
		actionGoto_Location_3->setShortcut(QApplication::translate("MainWindow", "Ctrl+F3", 0));
		actionGoto_Location_4->setText(QApplication::translate("MainWindow", "Go to Location 4", 0));
		actionGoto_Location_4->setShortcut(QApplication::translate("MainWindow", "Ctrl+F4", 0));
		actionGoto_Location_5->setText(QApplication::translate("MainWindow", "Go to Location 5", 0));
		actionGoto_Location_5->setShortcut(QApplication::translate("MainWindow", "Ctrl+F5", 0));
		actionGoto_Location_6->setText(QApplication::translate("MainWindow", "Go to Location 6", 0));
		actionGoto_Location_6->setShortcut(QApplication::translate("MainWindow", "Ctrl+F6", 0));
		actionGoto_Location_7->setText(QApplication::translate("MainWindow", "Go to Location 7", 0));
		actionGoto_Location_7->setShortcut(QApplication::translate("MainWindow", "Ctrl+F7", 0));
		actionGoto_Location_8->setText(QApplication::translate("MainWindow", "Go to Location 8", 0));
		actionGoto_Location_8->setShortcut(QApplication::translate("MainWindow", "Ctrl+F8", 0));
		actionGoto_Location_9->setText(QApplication::translate("MainWindow", "Go to Location 9", 0));
		actionGoto_Location_9->setShortcut(QApplication::translate("MainWindow", "Ctrl+F9", 0));
		actionGoto_Location_10->setText(QApplication::translate("MainWindow", "Go to Location 10", 0));
		actionGoto_Location_10->setShortcut(QApplication::translate("MainWindow", "Ctrl+F10", 0));
		actionGoto_Location_11->setText(QApplication::translate("MainWindow", "Go to Location 11", 0));
		actionGoto_Location_11->setShortcut(QApplication::translate("MainWindow", "Ctrl+F11", 0));
		actionGoto_Location_12->setText(QApplication::translate("MainWindow", "Go to Location 12", 0));
		actionGoto_Location_12->setShortcut(QApplication::translate("MainWindow", "Ctrl+F12", 0));
		actionCamera_Default->setText(QApplication::translate("MainWindow", "Default Camera", 0));
		actionCamera_Sequence->setText(QApplication::translate("MainWindow", "Sequence Camera", 0));
		actionCamera_Selected_Object->setText(QApplication::translate("MainWindow", "Selected Camera Object", 0));
		actionCamera_Cycle->setText(QApplication::translate("MainWindow", "Cycle Camera", 0));
		actionCamera_Cycle->setShortcut(QApplication::translate("MainWindow", "Ctrl+'", 0));
		actionShow_Hide_Helpers->setText(QApplication::translate("MainWindow", "Show/Hide Helpers", 0));
		actionToggle_Fullscreen_Viewport->setText(QApplication::translate("MainWindow", "Toggle Fullscreen", 0));
		actionToggle_Fullscreen_Viewport->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "FullScreen", 0)));
		actionToggle_Fullscreen_Viewport->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.fullscreen", 0)));
		actionGroup->setText(QApplication::translate("MainWindow", "Group", 0));
		actionUngroup->setText(QApplication::translate("MainWindow", "Ungroup", 0));
		actionGroup_Open->setText(QApplication::translate("MainWindow", "Open", 0));
		actionGroup_Close->setText(QApplication::translate("MainWindow", "Close", 0));
		actionGroup_Attach->setText(QApplication::translate("MainWindow", "Attach to...", 0));
		actionGroup_Detach->setText(QApplication::translate("MainWindow", "Detach", 0));
		actionGroup_DetachToRoot->setText(QApplication::translate("MainWindow", "Detach from Hierarchy", 0));
		actionReload_Terrain->setText(QApplication::translate("MainWindow", "Reload Terrain", 0));
		actionReload_Terrain->setProperty("command", QVariant(QApplication::translate("MainWindow", "terrain.reload_terrain", 0)));
		actionSwitch_to_Game->setText(QApplication::translate("MainWindow", "Switch to Game", 0));
		actionSwitch_to_Game->setShortcuts(CKeyboardShortcut("F5; Ctrl+G").ToKeySequence());
		actionSuspend_Game_Input->setText(QApplication::translate("MainWindow", "Suspend Game Input", 0));
		actionSuspend_Game_Input->setShortcut(QApplication::translate("MainWindow", "Pause", 0));
		actionSuspend_Game_Input->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.suspend_game_input", 0)));
		actionEnable_Physics_AI->setText(QApplication::translate("MainWindow", "Enable Physics/AI", 0));
		actionEnable_Physics_AI->setShortcut(QApplication::translate("MainWindow", "Ctrl+P", 0));
		actionSynchronize_Player_with_Camera->setText(QApplication::translate("MainWindow", "Synchronize Player", 0));
#ifndef QT_NO_TOOLTIP
		actionSynchronize_Player_with_Camera->setToolTip(QApplication::translate("MainWindow", "Synchronize Player with Camera", 0));
#endif // QT_NO_TOOLTIP

		actionReload_All_Scripts->setText(QApplication::translate("MainWindow", "Reload All Scripts", 0));
		actionReload_Entity_Scripts->setText(QApplication::translate("MainWindow", "Reload Entity Scripts", 0));
		actionReload_Item_Scripts->setText(QApplication::translate("MainWindow", "Reload Item Scripts", 0));
		actionReload_AI_Scripts->setText(QApplication::translate("MainWindow", "Reload AI Scripts", 0));
		actionReload_UI_Scripts->setText(QApplication::translate("MainWindow", "Reload UI Scripts", 0));
		actionReload_Archetypes->setText(QApplication::translate("MainWindow", "Reload Archetypes", 0));
		actionReload_Texture_Shaders->setText(QApplication::translate("MainWindow", "Reload Texture/Shaders", 0));
		actionReload_Geometry->setText(QApplication::translate("MainWindow", "Reload Geometry", 0));
		actionCheck_Level_for_Errors->setText(QApplication::translate("MainWindow", "Check Level for Errors", 0));
		actionCheck_Object_Positions->setText(QApplication::translate("MainWindow", "Check Object Positions", 0));
		actionSave_Level_Statistics->setText(QApplication::translate("MainWindow", "Save Level Statistics", 0));
		actionCompile_Script->setText(QApplication::translate("MainWindow", "Compile Script", 0));
		actionReduce_Working_Set->setText(QApplication::translate("MainWindow", "Reduce Working Set", 0));
		actionUpdate_Procedural_Vegetation->setText(QApplication::translate("MainWindow", "Update Procedural Vegetation", 0));
		actionSave->setText(QApplication::translate("MainWindow", "&Save", 0));
		actionSave->setProperty("standardkey", QVariant(QApplication::translate("MainWindow", "Save", 0)));
		actionSave->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.save", 0)));
		actionSave_Selected_Objects->setText(QApplication::translate("MainWindow", "Save Selected Objects", 0));
		actionSave_Selected_Objects->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+S", 0));
		actionLoad_Selected_Objects->setText(QApplication::translate("MainWindow", "Load Selected Objects", 0));
		actionLoad_Selected_Objects->setShortcut(QApplication::translate("MainWindow", "Ctrl+Alt+L", 0));
		actionLock_X_Axis->setText(QApplication::translate("MainWindow", "Lock on X Axis", 0));
		actionLock_X_Axis->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "LockAxisGroup", 0)));
		actionLock_Y_Axis->setText(QApplication::translate("MainWindow", "Lock on Y Axis", 0));
		actionLock_Y_Axis->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "LockAxisGroup", 0)));
		actionLock_Z_Axis->setText(QApplication::translate("MainWindow", "Lock on Z Axis", 0));
		actionLock_Z_Axis->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "LockAxisGroup", 0)));
		actionLock_XY_Axis->setText(QApplication::translate("MainWindow", "Lock on XY Plane", 0));
		actionLock_XY_Axis->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "LockAxisGroup", 0)));
		actionCoordinates_View->setText(QApplication::translate("MainWindow", "View", 0));
		actionCoordinates_View->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "CoordinatesGroup", 0)));
		actionCoordinates_View->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_view_coordinates", 0)));
		actionCoordinates_Local->setText(QApplication::translate("MainWindow", "Local", 0));
		actionCoordinates_Local->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "CoordinatesGroup", 0)));
		actionCoordinates_Local->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_local_coordinates", 0)));
		actionCoordinates_Parent->setText(QApplication::translate("MainWindow", "Parent", 0));
		actionCoordinates_Parent->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "CoordinatesGroup", 0)));
		actionCoordinates_Parent->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_parent_coordinates", 0)));
		actionCoordinates_World->setText(QApplication::translate("MainWindow", "World", 0));
		actionCoordinates_World->setProperty("actionGroup", QVariant(QApplication::translate("MainWindow", "CoordinatesGroup", 0)));
		actionCoordinates_World->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_world_coordinates", 0)));
		actionResolve_Missing_Objects->setText(QApplication::translate("MainWindow", "Resolve Missing Objects/Materials", 0));

		ESystemConfigSpec currentConfigSpec = GetIEditorImpl()->GetEditorConfigSpec();

		actionVery_High->setText(QApplication::translate("MainWindow", "Very High", 0));
		actionVery_High->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_config_spec 4", 0)));
		actionVery_High->setChecked(currentConfigSpec == CONFIG_VERYHIGH_SPEC);
		actionHigh->setText(QApplication::translate("MainWindow", "High", 0));
		actionHigh->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_config_spec 3", 0)));
		actionHigh->setChecked(currentConfigSpec == CONFIG_HIGH_SPEC);
		actionMedium->setText(QApplication::translate("MainWindow", "Medium", 0));
		actionMedium->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_config_spec 2", 0)));
		actionMedium->setChecked(currentConfigSpec == CONFIG_MEDIUM_SPEC);
		actionLow->setText(QApplication::translate("MainWindow", "Low", 0));
		actionLow->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_config_spec 1", 0)));
		actionLow->setChecked(currentConfigSpec == CONFIG_LOW_SPEC);
		actionXBox_One->setText(QApplication::translate("MainWindow", "XBox One", 0));
		actionXBox_One->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_config_spec 5", 0)));
		actionXBox_One->setChecked(currentConfigSpec == CONFIG_DURANGO);
		actionPS4->setText(QApplication::translate("MainWindow", "PS4", 0));
		actionPS4->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_config_spec 6", 0)));
		actionPS4->setChecked(currentConfigSpec == CONFIG_ORBIS);
		actionCustom->setText(QApplication::translate("MainWindow", "Custom", 0));
		actionCustom->setProperty("command", QVariant(QApplication::translate("MainWindow", "general.set_config_spec 0", 0)));
		actionCustom->setChecked(currentConfigSpec == CONFIG_CUSTOM);

		actionMute_Audio->setText(QApplication::translate("MainWindow", "Mute Audio", 0));
		actionStop_All_Sounds->setText(QApplication::translate("MainWindow", "Stop All Sounds", 0));
		actionRefresh_Audio->setText(QApplication::translate("MainWindow", "Refresh Audio", 0));
		menuFile->setTitle(QApplication::translate("MainWindow", "&File", 0));
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
	, m_bClosing(false)
	, m_bUserEventPriorityMode(false)
{
	LOADING_TIME_PROFILE_SECTION;

	// Make the level editor the fallback handler for all unhandled events
	m_loopHandler.SetDefaultHandler(m_levelEditor.get());
	m_pInstance = this;
	m_pAboutDlg = nullptr;
	Ui_MainWindow().setupUi(this);
	s_pToolTabManager = new CTabPaneManager(this);
	m_pMainToolBarManager = new QMainToolBarManager(this);
	connect(m_levelEditor.get(), &CLevelEditor::LevelLoaded, this, &CEditorMainFrame::UpdateWindowTitle);

	setAttribute(Qt::WA_DeleteOnClose, true);

	// Enable idle loop
	QTimer::singleShot(0, this, &CEditorMainFrame::OnIdleCallback);
	QTimer::singleShot(500, this, &CEditorMainFrame::OnBackgroundUpdateTimer);
	QTimer::singleShot(gEditorFilePreferences.autoSaveTime * 60 * 1000, this, &CEditorMainFrame::OnAutoSaveTimer);

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

	CViewManager* pViewManager = GetIEditorImpl()->GetViewManager();
	pViewManager->signalAxisConstrainChanged.Connect(this, &CEditorMainFrame::OnAxisConstrainChanged);

	UpdateWindowTitle();

	setWindowIcon(QIcon("icons:editor_icon.ico"));
	qApp->setWindowIcon(windowIcon());

	//QWidget* w = QCustomWindowFrame::wrapWidget(this);
	QWidget* w = QSandboxWindow::wrapWidget(this, m_toolManager);
	w->setObjectName("mainWindow");
	w->show();

	//Important so the focus is set to this and messages reach the CLevelEditor when clicking on the menu.
	setFocusPolicy(Qt::StrongFocus);

	GetIEditorImpl()->Notify(eNotify_OnMainFrameCreated);
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

//////////////////////////////////////////////////////////////////////////
CEditorMainFrame::~CEditorMainFrame()
{
	m_loopHandler.RemoveNativeHandler(reinterpret_cast<uintptr_t>(this));

	if (m_pInstance)
	{
		SAFE_DELETE(s_pToolTabManager);
		m_pInstance = 0;
	}
}

//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////
void CEditorMainFrame::PostLoad()
{
	LOADING_TIME_PROFILE_SECTION;
	InitActions();
	InitMenus();
	m_pMainToolBarManager->CreateMainFrameToolBars();
	GetIEditorImpl()->GetTrayArea()->RegisterTrayWidget<CTraySearchBox>(0);
	InitMenuBar();

	if (!GetIEditorImpl()->IsInMatEditMode())
	{
		InitLayout();
	}

	GetIEditorImpl()->GetTrayArea()->MainFrameInitialized();
}

//////////////////////////////////////////////////////////////////////////

CEditorMainFrame* CEditorMainFrame::GetInstance()
{
	return m_pInstance;
}

//////////////////////////////////////////////////////////////////////////
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

	int i;

	std::map<string, int> numClassesInCategory;

	std::vector<IClassDesc*> classes;
	GetIEditorImpl()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_VIEWPANE, classes);
	for (i = 0; i < classes.size(); i++)
	{
		numClassesInCategory[classes[i]->Category()]++;
	}
	for (i = 0; i < classes.size(); i++)
	{
		IClassDesc* pClass = classes[i];
		IViewPaneClass* pViewClass = (IViewPaneClass*)pClass;

		if (!pViewClass)
		{
			continue;
		}
		if (stl::find(viewPaneClasses, pViewClass))
			continue;

		int numClasses = numClassesInCategory[pViewClass->Category()];
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
		QMenu* menu = pAction->menu();

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
		pContinousUpdate->setCheckable(true);
		pContinousUpdate->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateType::Continuous);
		gAINavigationPreferences.signalSettingsChanged.Connect([pContinousUpdate]()
		{
			pContinousUpdate->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateType::Continuous);
		});
	}

	QAction* pAfterChangeUpdate = pCommandManager->GetCommandAction("ai.set_navigation_update_afterchange");
	if (pAfterChangeUpdate)
	{
		pUpdateModeActionGroup->addAction(pAfterChangeUpdate);
		pUpdateModeMenu->addAction(pAfterChangeUpdate);
		pAfterChangeUpdate->setCheckable(true);
		pAfterChangeUpdate->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateType::AfterStabilization);
		gAINavigationPreferences.signalSettingsChanged.Connect([pAfterChangeUpdate]()
		{
			pAfterChangeUpdate->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateType::AfterStabilization);
		});
	}

	QAction* pUpdateDisabled = pCommandManager->GetCommandAction("ai.set_navigation_update_disabled");
	if (pUpdateDisabled)
	{
		pUpdateModeActionGroup->addAction(pUpdateDisabled);
		pUpdateModeMenu->addAction(pUpdateDisabled);
		pUpdateDisabled->setCheckable(true);
		pUpdateDisabled->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateType::Disabled);
		gAINavigationPreferences.signalSettingsChanged.Connect([pUpdateDisabled]()
		{
			pUpdateDisabled->setChecked(gAINavigationPreferences.navigationUpdateType() == ENavigationUpdateType::Disabled);
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
			pCommandManager->GetAction("camera.toggle_speed_height_relative")->setChecked(CRenderViewport::s_cameraPreferences.speedHeightRelativeEnabled);
			pCommandManager->GetAction("camera.toggle_terrain_collisions")->setChecked(CRenderViewport::s_cameraPreferences.terrainCollisionEnabled);
			pCommandManager->GetAction("camera.toggle_object_collisions")->setChecked(CRenderViewport::s_cameraPreferences.objectCollisionEnabled);
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
		QString name("Keyboard Shortcuts");
		QString cmd = QString("general.open_pane '%1'").arg(name);
		QCommandAction* pKeyboardShortcuts = new QCommandAction(name.append("..."), (const char*)cmd.toLocal8Bit(), editMenu);
		pKeyboardShortcuts->setIcon(CryIcon("icons:Tools/Keyboard_Shortcuts.ico"));
		editMenu->addAction(pKeyboardShortcuts);

		name = "Preferences";
		cmd = QString("general.open_pane '%1'").arg(name);
		editMenu->addAction(new QCommandAction(name.append("..."), (const char*)cmd.toLocal8Bit(), editMenu));

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
		cmd = QString("general.open_pane '%1'").arg(name);
		QAction* pUndoHistory = new QCommandAction(name, (const char*)cmd.toLocal8Bit(), editMenu);
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

		QString name = "Console Commands";
		QString cmd = QString("general.open_pane '%1'").arg(name);
		QAction* pAction = new QCommandAction(name, static_cast<const char*>(cmd.toLocal8Bit()), pHelpMenu);
		pAction->setIcon(CryIcon("icons:ObjectTypes/Console_Commands.ico"));
		pHelpMenu->addAction(pAction);

		name = "Console Variables";
		cmd = QString("general.open_pane '%1'").arg(name);
		pAction = new QCommandAction(name, static_cast<const char*>(cmd.toLocal8Bit()), pHelpMenu);
		pAction->setIcon(CryIcon("icons:ObjectTypes/Console_Variables.ico"));
		pHelpMenu->addAction(pAction);

		pHelpMenu->addSeparator();

		name = "About Sandbox...";
		pAction = new QAction(name, pHelpMenu);
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

//////////////////////////////////////////////////////////////////////////

void CEditorMainFrame::InitActions()
{
	CEditorCommandManager* commandMgr = GetIEditorImpl()->GetCommandManager();

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
				QObject* object = action->parent();
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
				commandMgr->RegisterAction(action, command);
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

	//Register actions from the command manager to the main frame if they have been created before
	std::vector<CCommand*> commands;
	commandMgr->GetCommandList(commands);
	for (CCommand* cmd : commands)
	{
		if (cmd->CanBeUICommand())
		{
			QCommandAction* action = commandMgr->GetCommandAction(cmd->GetCommandString());

			// Make sure shortcuts are callable from other windows as well (we might need to specialize this a bit more in the future)
			action->setShortcutContext(Qt::WindowShortcut);

			if (action->parent() == nullptr)
			{
				addAction(action);
			}
		}
	}

	//Must be called after actions declared in the .ui file are registered to the command manager
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

bool CEditorMainFrame::focusNextPrevChild(bool next)
{
	if (GetIEditorImpl()->GetEditTool() && GetIEditorImpl()->GetEditTool()->IsAllowTabKey())
		return false;
	return __super::focusNextPrevChild(next);
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
	connect(pCustomize, &QAction::triggered, [this](bool bChecked)
	{
		QToolBarCreator* pToolBarCreator = new QToolBarCreator();
		pToolBarCreator->show();
		m_pMainToolBarManager->CreateMainFrameToolBars();
	});

	menu.exec(pEvent->globalPos());
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
	case eNotify_OnEditToolEndChange:
		{
			if (!GetIEditorImpl()->GetEditTool()->IsKindOf(RUNTIME_CLASS(CLinkTool)))
			{
				findChild<QAction*>("actionLink")->setChecked(false);
			}
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
	LOADING_TIME_PROFILE_SECTION;
	SaveConfig();
	// disconnect now or we'll end up closing all panels and saving an empty layout
	disconnect(m_layoutChangedConnection);

	AboutToQuitEvent aboutToQuitEvent;
	GetIEditorImpl()->GetGlobalBroadcastManager()->Broadcast(aboutToQuitEvent);

	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CEditorMainFrame::BeforeClose() Save Changes?");

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
	GetIEditorImpl()->ClearSelection();
	GetIEditorImpl()->SetEditTool(0);

	CTabPaneManager::GetInstance()->CloseAllPanes();

	GetIEditorImpl()->CloseDocument();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEditorMainFrame::SaveConfig()
{
	LOADING_TIME_PROFILE_SECTION;
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

//////////////////////////////////////////////////////////////////////////
void CEditorMainFrame::OnAxisConstrainChanged(int axis)
{
	QAction* pAction = nullptr;
	switch (axis)
	{
	case AXIS_X:
		pAction = findChild<QAction*>("actionLock_X_Axis");
		break;

	case AXIS_Y:
		pAction = findChild<QAction*>("actionLock_Y_Axis");
		break;

	case AXIS_Z:
		pAction = findChild<QAction*>("actionLock_Z_Axis");
		break;

	case AXIS_XY:
		pAction = findChild<QAction*>("actionLock_XY_Axis");
		break;

	default:
		pAction = findChild<QAction*>("actionLock_X_Axis");
		pAction->setChecked(false);
		pAction = findChild<QAction*>("actionLock_Y_Axis");
		pAction->setChecked(false);
		pAction = findChild<QAction*>("actionLock_Z_Axis");
		pAction->setChecked(false);
		pAction = findChild<QAction*>("actionLock_XY_Axis");
		pAction->setChecked(false);
		return;
	}

	if (pAction)
		pAction->setChecked(true);
}

//////////////////////////////////////////////////////////////////////////
void CEditorMainFrame::ResetAutoSaveTimers()
{

}

//////////////////////////////////////////////////////////////////////////
void CEditorMainFrame::OnAutoSaveTimer()
{
	if (gEditorFilePreferences.autoSaveEnabled)
	{
		// Call autosave function of CryEditApp.
		GetIEditorImpl()->GetDocument()->SaveAutoBackup();
	}
	QTimer::singleShot(gEditorFilePreferences.autoSaveTime * 60 * 1000, this, &CEditorMainFrame::OnAutoSaveTimer);
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

//////////////////////////////////////////////////////////////////////////
void CEditorMainFrame::OnIdleCallback()
{
	if (gEnv->stoppedOnAssert)
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
QToolWindowManager* CEditorMainFrame::GetToolManager()
{
	return m_toolManager;
}

//////////////////////////////////////////////////////////////////////////
QMainToolBarManager* CEditorMainFrame::GetToolBarManager()
{
	return m_pMainToolBarManager;
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

