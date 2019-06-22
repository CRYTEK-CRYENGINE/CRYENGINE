// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QMainWindow>
#include "Pointers.h"
#include <QtViewPane.h>

#include <vector>

class BroadcastEvent;
class QBoxLayout;
class QDockWidget;
class QMainWindow;
class QPropertyTreeLegacy;
class QResizeEvent;
class QSplitter;
class QToolBar;
class QToolButton;
class QViewport;
struct SRenderContext;
struct SViewportState;

class DockWidgetManager;
namespace Explorer
{
struct ExplorerEntry;
struct ExplorerAction;
class ExplorerPanel;
class ExplorerActionHandler;
}

#include <CrySerialization/Forward.h>

namespace CharacterTool {

using std::vector;
using std::unique_ptr;

using namespace Explorer;

struct ViewportPlaybackHotkeyConsumer;
struct DisplayAnimationOptions;
class AnimEventPresetPanel;
class BlendSpacePreview;
class DisplayParametersPanel;
class PlaybackPanel;
class PropertiesPanel;
class SceneParametersPanel;
class TransformPanel;
class QSplitViewport;
struct IViewportMode;
struct DisplayOptions;
struct ViewportOptions;
struct System;

class CharacterToolForm : public CDockableWindow
{
	Q_OBJECT
public:
	CharacterToolForm(QWidget* parent = 0);
	~CharacterToolForm();

	void Serialize(Serialization::IArchive& ar);

	//////////////////////////////////////////////////////////
	// CDockableWidget implementation
	virtual const char*                       GetPaneTitle() const override        { return "Character Tool"; }
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	//////////////////////////////////////////////////////////

	virtual QMenu* GetPaneMenu() const;

public slots:
	void             OnExportAnimationLayers();
	void             OnImportAnimationLayers();
	void             OnFileSaveAll();
	void             OnFileRecent();
	void             OnFileRecentAboutToShow(QMenu* menu);
	void             OnFileNewCharacter();
	void             OnFileOpenCharacter();
	void             OnFileCleanAnimations();
	void             OnFileResaveAnimSettings();
	void             OnLayoutReset();
	void             OnLayoutSave();
	void             OnLayoutSet();
	void             OnLayoutRemove();

	void             OnIdleUpdate();
	void             OnPreRenderCompressed(const SRenderContext& context);
	void             OnRenderCompressed(const SRenderContext& context);
	void             OnPreRenderOriginal(const SRenderContext& context);
	void             OnRenderOriginal(const SRenderContext& context);
	void             OnViewportUpdate();
	void             OnViewportOptionsChanged();
	void             OnDisplayOptionsChanged(const DisplayOptions& displayOptions);
	void             OnDisplayParametersButton();
	void             OnExplorerSelectionChanged();
	void             OnDockWidgetsChanged();
	void             OnCharacterLoaded();
	void             OnPanelDestroyed(QObject* obj);
	void             OnFocusChanged(QWidget* old, QWidget* now);
	void             OnClearProxiesButton();

	void             OnAnimEventPresetPanelPutEvent();

	bool             TryToClose()         { return true; }

	IViewportMode*   ViewportMode() const { return m_mode; }
	PlaybackPanel*   GetPlaybackPanel()   { return m_playbackPanel; }
	int              ProxyMakingMode();
	PropertiesPanel* GetPropertiesPanel() { return m_propertiesPanel; }
protected:
	bool             event(QEvent* ev) override;
	void             closeEvent(QCloseEvent* ev);
	bool             eventFilter(QObject* sender, QEvent* ev) override;
	void             customEvent(QEvent* event) override;
private:

	virtual QRect  GetPaneRect() override;

	ExplorerPanel* CreateExplorerPanel();
	void           SerializeLayout(Serialization::IArchive& ar);
	void           SaveLayoutToFile(const char* filename);
	void           LoadLayoutFromFile(const char* filename);
	void           ResetLayout();
	virtual void   Initialize() override;
	void           UpdateLayoutMenu();
	void           UpdatePanesMenu();
	void           UpdateViewportMode(ExplorerEntry* newEntry);
	void           InstallMode(IViewportMode* mode, ExplorerEntry* modeEntry);
	void           ResetDockWidgetsToDefault();
	void           ReadViewportOptions(const ViewportOptions& options, const DisplayAnimationOptions& animationOptions);
	void           LeaveMode();
	void           OnMainFrameAboutToQuit(BroadcastEvent& event);
	void           LoadGlobalData();

	struct SPrivate;
	System*                                    m_system;
	QScopedPointer<SPrivate>                   m_private;
	QSplitViewport*                            m_splitViewport;
	int                                        m_displayParametersSplitterWidths[2];
	QSplitter*                                 m_displayParametersSplitter;
	IViewportMode*                             m_mode;
	QScopedPointer<IViewportMode>              m_modeCharacter;
	ExplorerEntry*                             m_modeEntry;

	std::vector<QDockWidget*>                  m_dockWidgets;
	PlaybackPanel*                             m_playbackPanel;
	PropertiesPanel*                           m_propertiesPanel;
	BlendSpacePreview*                         m_blendSpacePreview;
	SceneParametersPanel*                      m_sceneParametersPanel;
	DisplayParametersPanel*                    m_displayParametersPanel;
	AnimEventPresetPanel*                      m_animEventPresetPanel;
	QToolBar*                                  m_modeToolBar;
	QToolButton*                               m_displayParametersButton;
	QToolButton*                               m_createProxyModeButton;
	QToolButton*                               m_createRagdollModeButton;
	QToolButton*                               m_clearProxiesButton;
	QToolButton*                               m_testRagdollButton;
	TransformPanel*                            m_transformPanel;

	QMenu*                                     m_menuView;
	vector<string>                             m_recentCharacters;
	unique_ptr<ViewportPlaybackHotkeyConsumer> m_viewportPlaybackHotkeyConsumer;
	unique_ptr<DockWidgetManager>              m_dockWidgetManager;
	unique_ptr<QPropertyTreeLegacy>                  m_contentLayerPropertyTree;

	QAction*                                   m_actionViewBindPose;
	QAction*                                   m_actionViewShowOriginalAnimation;
	QAction*                                   m_actionViewShowCompressionFlickerDiff;

	QMenu*                                     m_menuLayout;
	QMenu*                                     m_pPaneMenu;
	QAction*                                   m_actionLayoutReset;
	QAction*                                   m_actionLayoutLoadState;
	QAction*                                   m_actionLayoutSaveState;

	std::vector<char>                          m_defaultLayoutSnapshot;

	bool                                       m_closed;
	bool                                       m_bHasFocus;
};

}
