// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "CharacterToolForm.h"
#include <QAction>
#include <QBoxLayout>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QPushButton>
#include <QRegExp>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QAdvancedTreeView.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>
#include "QViewport.h"
#include "DockTitleBarWidget.h"
#include "UnsavedChangesDialog.h"
#include "FileDialogs/EngineFileDialog.h"
#include "FileDialogs/SystemFileDialog.h"
#include "FilePathUtil.h"
#include "SplitViewport.h"
#include "CharacterDocument.h"
#include "AnimationList.h"
#include "Explorer/ExplorerPanel.h"
#include "PlaybackPanel.h"
#include "SkeletonList.h"
#include "CharacterList.h"
#include "EditorCompressionPresetTable.h"
#include <IEditor.h>

#include "Expected.h"
#include "Serialization.h"
#include <CrySerialization/yasli/JSONOArchive.h>
#include <CrySerialization/yasli/JSONIArchive.h>

#include "CleanCompiledAnimationsTool.h"
#include "ResaveAnimSettingsTool.h"
#include "AnimationTagList.h"
#include "ModeCharacter.h"
#include "Serialization/Decorators/INavigationProvider.h"
#include "GizmoSink.h"
#include "BlendSpacePreview.h"
#include "SceneParametersPanel.h"
#include "PropertiesPanel.h"
#include "AnimEventPresetPanel.h"

#include "DisplayParametersPanel.h"

#include "TransformPanel.h"
#include "CharacterToolSystem.h"
#include "SceneContent.h"
#include "DockWidgetManager.h"
#include "CharacterGizmoManager.h"
#include "FileDialogs/EngineFileDialog.h"
#include "Controls/SaveChangesDialog.h"
#include "EditorFramework/BroadcastManager.h"
#include <QtViewPane.h>
#include <CryIcon.h>
#include "Controls/QuestionDialog.h"
#include <ICommandManager.h>
#include "Preferences/ViewportPreferences.h"

extern CharacterTool::System* g_pCharacterToolSystem;

namespace CharacterTool {

REGISTER_VIEWPANE_FACTORY_AND_MENU(CharacterToolForm, "Character Tool", "Tools", false, "Animation")

struct ViewportPlaybackHotkeyConsumer : public QViewportConsumer
{
	PlaybackPanel* playbackPanel;

	ViewportPlaybackHotkeyConsumer(PlaybackPanel* playbackPanel)
		: playbackPanel(playbackPanel)
	{
	}

	void OnViewportKey(const SKeyEvent& ev) override
	{
		if (ev.type == ev.TYPE_PRESS && ev.key != Qt::Key_Delete && ev.key != Qt::Key_D && ev.key != Qt::Key_Z)
			playbackPanel->HandleKeyEvent(ev.key);
	}
};

static string GetStateFilename()
{
	string result = GetIEditor()->GetUserFolder();
	result += "\\CharacterTool\\State.json";
	return result;
}

// ---------------------------------------------------------------------------

struct CharacterToolForm::SPrivate : IEditorNotifyListener
{
	SPrivate(CharacterToolForm* form, CharacterDocument* document)
		: form(form)
	{
		GetIEditor()->RegisterNotifyListener(this);
	}

	~SPrivate()
	{
		GetIEditor()->UnregisterNotifyListener(this);
	}

	void OnEditorNotifyEvent(EEditorNotifyEvent event) override
	{
		if (event == eNotify_OnIdleUpdate)
		{
			form->OnIdleUpdate();
		}
		else if (event == eNotify_OnQuit)
		{
			form->SaveLayoutToFile(GetStateFilename());
		}
	}

	CharacterToolForm* form;
};

CharacterToolForm::CharacterToolForm(QWidget* parent)
	: CDockableWindow(parent)
	, m_system(g_pCharacterToolSystem)
	, m_splitViewport(0)
	, m_mode(0)
	, m_modeEntry(0)
	, m_propertiesPanel(nullptr)
	, m_dockWidgetManager(new DockWidgetManager(this))
	, m_menuView()
	, m_closed(false)
	, m_bHasFocus(true)
{
	m_displayParametersSplitterWidths[0] = 400;
	m_displayParametersSplitterWidths[1] = 200;

	Initialize();

	setFocusPolicy(Qt::ClickFocus);

	CBroadcastManager* const pGlobalBroadcastManager = GetIEditor()->GetGlobalBroadcastManager();
	pGlobalBroadcastManager->Connect(BroadcastEvent::AboutToQuit, this, &CharacterToolForm::OnMainFrameAboutToQuit);
}

struct ActionCreator
{
	QMenu*   menu;
	QObject* handlerObject;
	bool     checkable;

	ActionCreator(QMenu* menu, QObject* handlerObject, bool checkable)
		: menu(menu)
		, handlerObject(handlerObject)
		, checkable(checkable)
	{
	}

	QAction* operator()(const char* name, const char* slot)
	{
		QAction* action = menu->addAction(name);
		action->setCheckable(checkable);
		if (!EXPECTED(QObject::connect(action, SIGNAL(triggered()), handlerObject, slot)))
			return 0;
		return action;
	}
	QAction* operator()(const char* name, const char* slot, const QVariant& actionData)
	{
		QAction* action = menu->addAction(name);
		action->setCheckable(checkable);
		action->setData(actionData);
		if (!EXPECTED(QObject::connect(action, SIGNAL(triggered()), handlerObject, slot)))
			return 0;
		return action;
	}
};

void CharacterToolForm::UpdatePanesMenu()
{
	if (m_menuView)
	{
		menuBar()->removeAction(m_menuView->menuAction());
		m_menuView->deleteLater();
	}

	m_menuView = createPopupMenu();
	m_menuView->setParent(this);
	m_menuView->setTitle("&View");
	menuBar()->insertMenu(menuBar()->actions()[1], m_menuView);
}

void CharacterToolForm::Initialize()
{
	LOADING_TIME_PROFILE_SECTION;
	m_private.reset(new SPrivate(this, m_system->document.get()));

	m_modeCharacter.reset(new ModeCharacter());

	setDockNestingEnabled(true);

	EXPECTED(connect(m_system->document.get(), SIGNAL(SignalExplorerSelectionChanged()), this, SLOT(OnExplorerSelectionChanged())));
	EXPECTED(connect(m_system->document.get(), SIGNAL(SignalCharacterLoaded()), this, SLOT(OnCharacterLoaded())));
	EXPECTED(connect(m_system->document.get(), SIGNAL(SignalDisplayOptionsChanged(const DisplayOptions &)), this, SLOT(OnDisplayOptionsChanged(const DisplayOptions &))));
	EXPECTED(connect(QApplication::instance(), SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(OnFocusChanged(QWidget*, QWidget*))));

	QWidget* centralWidget = new QWidget();
	setCentralWidget(centralWidget);
	{
		centralWidget->setContentsMargins(0, 0, 0, 0);

		QBoxLayout* centralLayout = new QBoxLayout(QBoxLayout::TopToBottom, centralWidget);
		centralLayout->setMargin(0);
		centralLayout->setSpacing(0);

		{
			QBoxLayout* topLayout = new QBoxLayout(QBoxLayout::LeftToRight, 0);
			topLayout->setMargin(0);

			m_modeToolBar = new QToolBar();
			m_modeToolBar->setIconSize(QSize(32, 32));
			m_modeToolBar->setStyleSheet("QToolBar { border: 0px }");
			m_modeToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			topLayout->addWidget(m_modeToolBar, 0);
			topLayout->addStretch(1);

			m_transformPanel = new TransformPanel();
			topLayout->addWidget(m_transformPanel);

			m_displayParametersButton = new QToolButton();
			m_displayParametersButton->setText("Display Options");
			m_displayParametersButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			m_displayParametersButton->setCheckable(true);
			m_displayParametersButton->setIcon(CryIcon("icons:General/Options.ico"));
			EXPECTED(connect(m_displayParametersButton, SIGNAL(toggled(bool)), this, SLOT(OnDisplayParametersButton())));
			topLayout->addWidget(m_displayParametersButton);

			m_createProxyModeButton = new QToolButton();
			m_createProxyModeButton->setText("Edit Proxies");
			m_createProxyModeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			m_createProxyModeButton->setCheckable(true);
			m_createProxyModeButton->setIcon(CryIcon("icons:common/animation_skeleton.ico"));
			topLayout->addWidget(m_createProxyModeButton);

			m_clearProxiesButton = new QToolButton();
			m_clearProxiesButton->setText("Clear Proxies");
			EXPECTED(connect(m_clearProxiesButton, SIGNAL(clicked()), this, SLOT(OnClearProxiesButton())));
			topLayout->addWidget(m_clearProxiesButton);

			m_testRagdollButton = new QToolButton();
			m_testRagdollButton->setText("Test Ragdoll");
			EXPECTED(connect(m_testRagdollButton, &QToolButton::pressed, [this](){ ((ModeCharacter*)m_modeCharacter.data())->CommenceRagdollTest(); }));
			topLayout->addWidget(m_testRagdollButton);

			centralLayout->addLayout(topLayout, 0);
		}

		m_displayParametersSplitter = new QSplitter(Qt::Horizontal);
		centralLayout->addWidget(m_displayParametersSplitter, 1);

		m_splitViewport = new QSplitViewport(0);
		EXPECTED(connect(m_splitViewport->OriginalViewport(), SIGNAL(SignalPreRender(const SRenderContext &)), this, SLOT(OnPreRenderOriginal(const SRenderContext &))));
		EXPECTED(connect(m_splitViewport->OriginalViewport(), SIGNAL(SignalRender(const SRenderContext &)), this, SLOT(OnRenderOriginal(const SRenderContext &))));
		EXPECTED(connect(m_splitViewport->CompressedViewport(), SIGNAL(SignalPreRender(const SRenderContext &)), this, SLOT(OnPreRenderCompressed(const SRenderContext &))));
		EXPECTED(connect(m_splitViewport->CompressedViewport(), SIGNAL(SignalRender(const SRenderContext &)), this, SLOT(OnRenderCompressed(const SRenderContext &))));
		EXPECTED(connect(m_splitViewport->CompressedViewport(), SIGNAL(SignalUpdate()), this, SLOT(OnViewportUpdate())));
		EXPECTED(connect(m_splitViewport, &QSplitViewport::dropFile, [this](const QString& file)
			{
				const QString cmdline = QString("meshimporter.generate_character '%1'").arg(file);
				const string result = GetIEditor()->GetICommandManager()->Execute(cmdline.toLocal8Bit().constData());
		  }));

		m_displayParametersSplitter->addWidget(m_splitViewport);

		m_displayParametersPanel = new DisplayParametersPanel(0, m_system->document.get(), m_system->contextList->Tail());
		m_displayParametersPanel->setVisible(false);
		m_displayParametersSplitter->addWidget(m_displayParametersPanel);
		m_displayParametersSplitter->setStretchFactor(0, 1);
		m_displayParametersSplitter->setStretchFactor(1, 0);
		QList<int> sizes;
		sizes.push_back(400);
		sizes.push_back(200);
		m_displayParametersSplitter->setSizes(sizes);
	}

	m_splitViewport->CompressedViewport()->installEventFilter(this);

	QMenuBar* menu = new QMenuBar(this);
	QMenu* fileMenu = menu->addMenu("&File");
	EXPECTED(connect(fileMenu->addAction("&New Character..."), SIGNAL(triggered()), this, SLOT(OnFileNewCharacter())));
	EXPECTED(connect(fileMenu->addAction("&Open Character..."), SIGNAL(triggered()), this, SLOT(OnFileOpenCharacter())));
	QMenu* menuFileRecent = fileMenu->addMenu("&Recent Characters");
	QObject::connect(menuFileRecent, &QMenu::aboutToShow, this, [=]() { OnFileRecentAboutToShow(menuFileRecent); });
	EXPECTED(connect(fileMenu->addAction("&Save All"), SIGNAL(triggered()), this, SLOT(OnFileSaveAll())));
	fileMenu->addSeparator();
	EXPECTED(connect(fileMenu->addAction("Import Animation Layers"), &QAction::triggered, this, &CharacterToolForm::OnImportAnimationLayers));
	EXPECTED(connect(fileMenu->addAction("Export Animation Layers"), &QAction::triggered, this, &CharacterToolForm::OnExportAnimationLayers));
	fileMenu->addSeparator();
	EXPECTED(connect(fileMenu->addAction("&Clean Compiled Animations..."), SIGNAL(triggered()), this, SLOT(OnFileCleanAnimations())));
	EXPECTED(connect(fileMenu->addAction("Resave &AnimSettings..."), SIGNAL(triggered()), this, SLOT(OnFileResaveAnimSettings())));

	QMenu* const importMenu = fileMenu->addMenu(tr("Import FBX"));
	EXPECTED(connect(importMenu->addAction("Skin"), &QAction::triggered, []() { GetIEditor()->OpenView("Mesh"); }));
	EXPECTED(connect(importMenu->addAction("Animation"), &QAction::triggered, []() { GetIEditor()->OpenView("Animation"); }));
	EXPECTED(connect(importMenu->addAction("Skeleton"), &QAction::triggered, []() { GetIEditor()->OpenView("Skeleton"); }));

	m_menuLayout = menu->addMenu("&Layout");
	UpdateLayoutMenu();

	setMenuBar(menu);

	m_animEventPresetPanel = new AnimEventPresetPanel(this, m_system);

	m_playbackPanel = new PlaybackPanel(this, m_system, m_animEventPresetPanel);
	m_propertiesPanel = new PropertiesPanel(this, m_system);
	m_sceneParametersPanel = new SceneParametersPanel(this, m_system);
	m_blendSpacePreview = new BlendSpacePreview(this, m_system->document.get());

	m_viewportPlaybackHotkeyConsumer.reset(new ViewportPlaybackHotkeyConsumer(m_playbackPanel));
	m_splitViewport->OriginalViewport()->AddConsumer(m_viewportPlaybackHotkeyConsumer.get());
	m_splitViewport->CompressedViewport()->AddConsumer(m_viewportPlaybackHotkeyConsumer.get());

	InstallMode(m_modeCharacter.data(), m_system->document->GetActiveCharacterEntry());

	EXPECTED(connect(m_dockWidgetManager.get(), SIGNAL(SignalChanged()), SLOT(OnDockWidgetsChanged())));
	m_dockWidgetManager->AddDockWidgetType<ExplorerPanel>(this, &CharacterToolForm::CreateExplorerPanel, "Explorer", Qt::LeftDockWidgetArea);

	ResetDockWidgetsToDefault();

	ReadViewportOptions(m_system->document->GetViewportOptions(), m_system->document->GetDisplayOptions()->animation);

	//This is now done on focus event, as this is taking a huge amount of time.
	// > 1 minute very easily
	//Multithreading this would need a big refactor as it notifies to the model and this triggers crashes
	//m_system->LoadGlobalData();
	m_animEventPresetPanel->LoadPresets();

	m_system->document->EnableAudio(true);

	{
		MemoryOArchive oa;
		SerializeLayout(oa);
		m_defaultLayoutSnapshot.assign(oa.buffer(), oa.buffer() + oa.length());
	}

	LoadLayoutFromFile(GetStateFilename());
}

QRect CharacterToolForm::GetPaneRect()
{
	return QRect(pos(), size());
}

ExplorerPanel* CharacterToolForm::CreateExplorerPanel()
{
	auto panel = new ExplorerPanel(this, m_system->explorerData.get());
	m_system->explorerPanels.push_back(panel);

	EXPECTED(connect(panel, &ExplorerPanel::destroyed, this, &CharacterToolForm::OnPanelDestroyed));
	EXPECTED(connect(panel, &ExplorerPanel::SignalSelectionChanged, &*m_system->document, &CharacterDocument::OnExplorerSelectionChanged));
	EXPECTED(connect(panel, &ExplorerPanel::SignalActivated, &*m_system->document, &CharacterDocument::OnExplorerActivated));
	EXPECTED(connect(panel, &ExplorerPanel::SignalFocusIn, [this]() { LoadGlobalData(); }));
	EXPECTED(connect(&*m_system->document, &CharacterDocument::SignalCharacterLoaded, panel, &ExplorerPanel::OnAssetLoaded));

	return panel;
}

void CharacterToolForm::OnPanelDestroyed(QObject* obj)
{
	for (auto it = m_system->explorerPanels.begin(); it != m_system->explorerPanels.end(); ++it)
	{
		if (*it == obj)
		{
			m_system->explorerPanels.erase(it);
			return;
		}
	}
}

void CharacterToolForm::OnFocusChanged(QWidget* old, QWidget* now)
{
	m_bHasFocus = false;
	QWidget* parent = parentWidget();
	if (parent)
	{
		while (parent->parentWidget())
			parent = parent->parentWidget();
		m_bHasFocus = parent->isAncestorOf(now);
	}
}

void CharacterToolForm::ResetLayout()
{
	m_system->document->ScrubTime(0.0f, false);
	m_system->document->GetViewportOptions() = ViewportOptions();
	m_system->document->DisplayOptionsChanged();
	m_system->document->GetPlaybackOptions() = PlaybackOptions();
	m_system->document->PlaybackOptionsChanged();
	m_splitViewport->CompressedViewport()->ResetCamera();
	m_splitViewport->OriginalViewport()->ResetCamera();

	ResetDockWidgetsToDefault();

	MemoryIArchive ia;
	if (ia.open(m_defaultLayoutSnapshot.data(), m_defaultLayoutSnapshot.size()))
	{
		SerializeLayout(ia);
	}
}

static QDockWidget* CreateDockWidget(std::vector<QDockWidget*>* dockWidgets, const char* title, const QString& name, QWidget* widget, Qt::DockWidgetArea area, CharacterToolForm* window)
{
	QDockWidget* dock = new CCustomDockWidget(title, window);
	dock->setObjectName(name);
	dock->setWidget(widget);

	window->addDockWidget(area, dock);
	dockWidgets->push_back(dock);

	return dock;
}

void CharacterToolForm::ResetDockWidgetsToDefault()
{
	m_dockWidgetManager->ResetToDefault();

	for (QDockWidget* widget : m_dockWidgets)
	{
		widget->setParent(0);
		removeDockWidget(widget);
		widget->deleteLater();
	}
	m_dockWidgets.clear();

	CreateDockWidget(&m_dockWidgets, "Playback", "playback", m_playbackPanel, Qt::BottomDockWidgetArea, this);
	CreateDockWidget(&m_dockWidgets, "Scene Parameters", "scene_parameters", m_sceneParametersPanel, Qt::RightDockWidgetArea, this)->raise();
	CreateDockWidget(&m_dockWidgets, "Properties", "properties", m_propertiesPanel, Qt::RightDockWidgetArea, this);
	CreateDockWidget(&m_dockWidgets, "Blend Space Preview", "blend_space_preview", m_blendSpacePreview, Qt::RightDockWidgetArea, this)->hide();
	CreateDockWidget(&m_dockWidgets, "Animation Event Presets", "anim_event_presets", m_animEventPresetPanel, Qt::RightDockWidgetArea, this)->hide();

	UpdatePanesMenu();
}

void CharacterToolForm::Serialize(Serialization::IArchive& ar)
{
	ar(*m_system, "system");

	ar(*m_dockWidgetManager, "dockWidgetManager");

	ar(*m_displayParametersPanel, "displayPanel");

	if (ar.isOutput() && m_displayParametersPanel->isVisible())
	{
		m_displayParametersSplitterWidths[0] = m_displayParametersSplitter->sizes()[0];
		m_displayParametersSplitterWidths[1] = m_displayParametersSplitter->sizes()[1];
	}
	ar(m_displayParametersSplitterWidths, "displayParameterSplittersWidths");
	bool displayOptionsVisible = !m_displayParametersPanel->isHidden();
	ar(displayOptionsVisible, "displayOptionsVisible");
	if (ar.isInput())
	{
		m_displayParametersPanel->setVisible(displayOptionsVisible);
		QList<int> widths;
		widths.push_back(m_displayParametersSplitterWidths[0]);
		widths.push_back(m_displayParametersSplitterWidths[1]);
		m_displayParametersSplitter->setSizes(widths);
		m_displayParametersButton->setChecked(displayOptionsVisible);
	}

	if (m_playbackPanel)
		ar(*m_playbackPanel, "playbackPanel");
	if (m_propertiesPanel)
		ar(*m_propertiesPanel, "propertiesPanel");
	if (m_blendSpacePreview)
		ar(*m_blendSpacePreview, "blendSpacePreview");
	if (m_animEventPresetPanel)
		ar(*m_animEventPresetPanel, "animEventPresetPanel");

	ar(m_recentCharacters, "recentCharacters");

	QByteArray windowGeometry;
	QByteArray windowState;
	if (ar.isOutput())
	{
		windowGeometry = saveGeometry();
		windowState = saveState();
	}
	ar(windowGeometry, "windowGeometry");
	ar(windowState, "windowState");
	if (ar.isInput())
	{
		restoreGeometry(windowGeometry);
		restoreState(windowState);
	}

	if (ar.isInput())
	{
		UpdatePanesMenu();
	}
}

void CharacterToolForm::OnIdleUpdate()
{
	if (gViewportPreferences.toolsRenderUpdateMutualExclusive)
	{
		// determine, if CT or any related widget has keyboard focus or is active window
		bool hasCharacterToolOrAnyAccordingWidgetFocus = m_bHasFocus || m_blendSpacePreview->hasFocus() || m_blendSpacePreview->isActiveWindow();
		for (auto const& it : m_dockWidgets)
			hasCharacterToolOrAnyAccordingWidgetFocus = hasCharacterToolOrAnyAccordingWidgetFocus || it->hasFocus() || it->isActiveWindow();

		if (!hasCharacterToolOrAnyAccordingWidgetFocus) return;
	}

	if (m_splitViewport)
	{
		m_splitViewport->OriginalViewport()->Update();
		m_splitViewport->CompressedViewport()->Update();
	}

	m_system->document->IdleUpdate();

	if (m_blendSpacePreview && m_blendSpacePreview->isVisible())
		m_blendSpacePreview->IdleUpdate();
}

static QString GetDirectoryFromPath(const QString& path)
{
	string driveLetter;
	string directory;
	string originalFilename;
	string extension;
	PathUtil::SplitPath(path.toStdString().c_str(), driveLetter, directory, originalFilename, extension);
	return (driveLetter + directory).c_str();
}

void CharacterToolForm::OnExportAnimationLayers()
{
	static QString prevDir;

	CSystemFileDialog::RunParams runParams;
	runParams.title = QObject::tr("Export Animation Layers Setup");
	runParams.initialDir = prevDir;
	runParams.extensionFilters << CExtensionFilter(QObject::tr("XML Files (xml)"), "xml");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("All files (*.*)"), "*");
	const QString fileName(CSystemFileDialog::RunExportFile(runParams, this));
	if (!fileName.isNull())
	{
		prevDir = GetDirectoryFromPath(fileName);
		GetIEditor()->GetSystem()->GetArchiveHost()->SaveXmlFile(
		  fileName.toStdString().c_str(),
		  Serialization::SStruct(m_system->scene->layers),
		  "AnimationLayers");
	}
}

void CharacterToolForm::OnImportAnimationLayers()
{
	static QString prevDir;

	CSystemFileDialog::RunParams runParams;
	runParams.title = QObject::tr("Import Animation Layers Setup");
	runParams.initialDir = prevDir;
	runParams.extensionFilters << CExtensionFilter(QObject::tr("XML Files (xml)"), "xml");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("All files (*.*)"), "*");
	const QString fileName(CSystemFileDialog::RunImportFile(runParams, this));
	if (!fileName.isNull())
	{
		prevDir = GetDirectoryFromPath(fileName);
		GetIEditor()->GetSystem()->GetArchiveHost()->LoadXmlFile(
		  Serialization::SStruct(m_system->scene->layers),
		  fileName.toStdString().c_str());
		m_system->scene->PlaybackLayersChanged(false);
		m_system->scene->SignalChanged(false);
		// fire the signal twice, because CharacterDocument::OnScenePlaybackLayersChanged
		// first selects an animation in the explorer ...
		m_system->scene->PlaybackLayersChanged(false);
	}
}

void CharacterToolForm::OnFileSaveAll()
{
	ActionOutput output;

	m_system->explorerData->SaveAll(&output);

	SaveLayoutToFile(GetStateFilename());

	m_animEventPresetPanel->SavePresets();

	if (!output.errorToDetails.empty())
		output.Show(this);
}

void CharacterToolForm::OnFileNewCharacter()
{
	CEngineFileDialog::RunParams runParams;
	runParams.title = tr("Create Character...");
	runParams.buttonLabel = tr("Create");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("Character Definitions (cdf)"), "cdf");
	QString fileName = CEngineFileDialog::RunGameSave(runParams, this);

	if (fileName.isEmpty())
		return;

	string filename = fileName.toLocal8Bit().data();

	if (!m_system->characterList->AddAndSaveEntry(filename.c_str()))
	{
		CQuestionDialog::SCritical("Error", "Failed to save character definition");
		return;
	}

	m_system->scene->characterPath = filename;
	m_system->scene->SignalCharacterChanged();

	ExplorerEntry* entry = m_system->explorerData->FindEntryByPath(&*m_system->characterList, filename.c_str());
	if (entry)
		m_system->document->SetSelectedExplorerEntries(ExplorerEntries(1, entry), 0);
}

void CharacterToolForm::OnFileOpenCharacter()
{
	CEngineFileDialog::RunParams runParams;
	runParams.initialDir = "Objects/characters";
	runParams.initialFile = m_system->scene->characterPath.c_str();
	runParams.extensionFilters << CExtensionFilter(QObject::tr("Character Definition or Animated Geometry (cdf, cga)"), "cdf", "cga");
	auto qFilename = CEngineFileDialog::RunGameOpen(runParams, nullptr);
	string filename = qFilename.toLocal8Bit().data();

	if (!filename.empty())
	{
		m_system->scene->characterPath = filename;
		m_system->scene->SignalCharacterChanged();
	}
}

void CharacterToolForm::OnLayoutReset()
{
	ResetLayout();
}

static string GetUserLayoutDirectoryPath()
{
	return string(GetIEditor()->GetUserFolder()) + "\\CharacterTool\\Layouts\\";
}

static string GetFilePathForUserLayout(const QString& layoutName)
{
	string filename = GetUserLayoutDirectoryPath();
	filename += layoutName.toLocal8Bit().data();
	filename += ".layout";

	return filename;
}

static std::vector<string> FindLayoutNames()
{
	vector<string> result;
	QDir dir(GetUserLayoutDirectoryPath().c_str());
	QStringList layouts = dir.entryList(QDir::Files, QDir::Name);
	for (size_t i = 0; i < layouts.size(); ++i)
	{
		QString name = layouts[i];
		name.replace(QRegExp("\\.layout$"), "");
		result.push_back(name.toLocal8Bit().data());
	}

	return result;
}

void CharacterToolForm::OnLayoutSave()
{
	QString name = QInputDialog::getText(this, "Save Layout...", "Layout Name:");
	name = name.replace('.', "_");
	name = name.replace(':', "_");
	name = name.replace('\\', "_");
	name = name.replace('/', "_");
	name = name.replace('?', "_");
	name = name.replace('*', "_");
	if (!name.isEmpty())
	{
		SaveLayoutToFile(GetFilePathForUserLayout(name).c_str());
		UpdateLayoutMenu();
	}
}

void CharacterToolForm::OnLayoutSet()
{
	QAction* action = qobject_cast<QAction*>(sender());
	if (action)
	{
		QString name = action->data().toString();
		if (!name.isEmpty())
		{
			LoadLayoutFromFile(GetFilePathForUserLayout(name).c_str());
		}
	}
}

void CharacterToolForm::OnLayoutRemove()
{
	QAction* action = qobject_cast<QAction*>(sender());
	if (action)
	{
		QString name = action->data().toString();
		if (!name.isEmpty())
		{
			QFile::remove(GetFilePathForUserLayout(name).c_str());
			UpdateLayoutMenu();
		}
	}
}

void CharacterToolForm::UpdateLayoutMenu()
{
	m_menuLayout->clear();

	ActionCreator addToLayout(m_menuLayout, this, false);
	vector<string> layouts = FindLayoutNames();
	for (size_t i = 0; i < layouts.size(); ++i)
	{
		QAction* action = addToLayout(layouts[i].c_str(), SLOT(OnLayoutSet()), QVariant(layouts[i].c_str()));
	}
	if (!layouts.empty())
		m_menuLayout->addSeparator();
	m_actionLayoutSaveState = addToLayout("Save Layout...", SLOT(OnLayoutSave()));
	QMenu* removeMenu = m_menuLayout->addMenu("Remove");
	if (layouts.empty())
		removeMenu->setEnabled(false);
	else
	{
		for (size_t i = 0; i < layouts.size(); ++i)
		{
			QAction* action = removeMenu->addAction(layouts[i].c_str());
			action->setData(QVariant(QString(layouts[i].c_str())));
			connect(action, SIGNAL(triggered()), this, SLOT(OnLayoutRemove()));
		}
	}
	m_menuLayout->addSeparator();
	m_actionLayoutReset = addToLayout("&Reset to Default", SLOT(OnLayoutReset()));
}

void CharacterToolForm::ReadViewportOptions(const ViewportOptions& options, const DisplayAnimationOptions& animationOptions)
{

}

void CharacterToolForm::LeaveMode()
{
	if (m_mode)
	{
		m_splitViewport->CompressedViewport()->RemoveConsumer(m_mode);
		m_mode->LeaveMode();
		m_mode = 0;
	}
}

void CharacterToolForm::OnMainFrameAboutToQuit(BroadcastEvent& event)
{
	ExplorerEntries unsavedEntries;

	m_system->explorerData->GetUnsavedEntries(&unsavedEntries);

	vector<string> unsavedFilenames;
	m_system->explorerData->GetSaveFilenames(&unsavedFilenames, unsavedEntries);

	if (!unsavedFilenames.empty())
	{
		AboutToQuitEvent& aboutToQuitEvent = (AboutToQuitEvent&)event;
		aboutToQuitEvent.AddChangeList("Character Tool", unsavedFilenames);

		event.ignore();
	}
}

void CharacterToolForm::LoadGlobalData()
{
	m_system->LoadGlobalData();
}

void CharacterToolForm::OnDisplayParametersButton()
{
	bool visible = m_displayParametersButton->isChecked();
	if (!visible)
	{
		m_displayParametersSplitterWidths[0] = m_displayParametersSplitter->sizes()[0];
		m_displayParametersSplitterWidths[1] = m_displayParametersSplitter->sizes()[1];
	}

	m_displayParametersPanel->setVisible(visible);
	if (visible)
	{
		QList<int> widths;
		widths.push_back(m_displayParametersSplitterWidths[0]);
		widths.push_back(m_displayParametersSplitterWidths[1]);
		m_displayParametersSplitter->setSizes(widths);
	}
}

void CharacterToolForm::OnDisplayOptionsChanged(const DisplayOptions& settings)
{
	SViewportSettings vpSettings(settings.viewport);
	m_splitViewport->CompressedViewport()->SetSettings(vpSettings);
	m_splitViewport->OriginalViewport()->SetSettings(vpSettings);

	bool isOriginalAnimationShown = m_splitViewport->IsSplit();
	bool newShowOriginalAnimation = settings.animation.compressionPreview != COMPRESSION_PREVIEW_COMPRESSED;
	if (newShowOriginalAnimation != isOriginalAnimationShown)
	{
		m_splitViewport->SetSplit(newShowOriginalAnimation);
	}

	// deactivate grid
	vpSettings.grid.showGrid = false;
	m_blendSpacePreview->GetViewport()->SetSettings(vpSettings);

}

void CharacterToolForm::OnClearProxiesButton()
{
	if (CharacterDefinition* cdf = m_system->document->GetLoadedCharacterDefinition())
	{
		cdf->RemoveRagdollAttachments();
		GetPropertiesPanel()->PropertyTree()->revert();
		GetPropertiesPanel()->OnChanged();
		EntryModifiedEvent ev;
		ev.id = m_system->document->GetActiveCharacterEntry()->id;
		m_system->document->OnCharacterModified(ev);
	}
}

void CharacterToolForm::OnPreRenderCompressed(const SRenderContext& context)
{
	m_system->document->SetAuxRenderer(context.pAuxGeom);
	m_system->document->PreRender(context);
}

void CharacterToolForm::OnRenderCompressed(const SRenderContext& context)
{
	m_system->document->SetAuxRenderer(context.pAuxGeom);
	m_system->document->Render(context);
}

void CharacterToolForm::OnPreRenderOriginal(const SRenderContext& context)
{
	m_system->document->SetAuxRenderer(context.pAuxGeom);
	m_system->document->PreRenderOriginal(context);
}

void CharacterToolForm::OnRenderOriginal(const SRenderContext& context)
{
	m_system->document->SetAuxRenderer(context.pAuxGeom);
	m_system->document->RenderOriginal(context);
}

void CharacterToolForm::OnCharacterLoaded()
{
	ICharacterInstance* character = m_system->document->CompressedCharacter();

	const char* filename = m_system->document->LoadedCharacterFilename();
	for (size_t i = 0; i < m_recentCharacters.size(); ++i)
		if (stricmp(m_recentCharacters[i].c_str(), filename) == 0)
		{
			m_recentCharacters.erase(m_recentCharacters.begin() + i);
			--i;
		}

	enum { NUM_RECENT_FILES = 10 };

	m_recentCharacters.insert(m_recentCharacters.begin(), filename);
	if (m_recentCharacters.size() > NUM_RECENT_FILES)
		m_recentCharacters.resize(NUM_RECENT_FILES);
	SaveLayoutToFile(GetStateFilename());

	ExplorerEntries entries;
	if (ExplorerEntry* charEntry = m_system->document->GetActiveCharacterEntry())
		entries.push_back(charEntry);
	m_system->document->SetSelectedExplorerEntries(entries, 0);
}

void CharacterToolForm::UpdateViewportMode(ExplorerEntry* newEntry)
{
	if (m_modeEntry == newEntry)
		return;
	if (!newEntry)
		return;

	if (!m_mode)
	{
		InstallMode(m_modeCharacter.data(), newEntry);
	}
}

void CharacterToolForm::InstallMode(IViewportMode* mode, ExplorerEntry* modeEntry)
{
	LeaveMode();

	m_system->gizmoSink->SetScene(0);
	m_mode = mode;
	m_modeEntry = modeEntry;

	m_modeToolBar->clear();

	if (m_mode)
	{
		SModeContext ctx;
		ctx.window = this;
		ctx.system = m_system;
		ctx.document = m_system->document.get();
		ctx.character = m_system->document->CompressedCharacter();
		ctx.transformPanel = m_transformPanel;
		ctx.toolbar = m_modeToolBar;
		ctx.layerPropertyTrees.resize(GIZMO_LAYER_COUNT);
		for (int layer = 0; layer < GIZMO_LAYER_COUNT; ++layer)
			ctx.layerPropertyTrees[layer] = m_system->characterGizmoManager->Tree((GizmoLayer)layer);

		m_mode->EnterMode(ctx);

		m_splitViewport->CompressedViewport()->AddConsumer(m_mode);
	}
	else
	{
		// dummy toolbar to keep panel recognizable
		QAction* action = m_modeToolBar->addAction(CryIcon("icons:Navigation/Basics_Move.ico"), QString());
		action->setEnabled(false);
		action->setPriority(QAction::LowPriority);
		action = m_modeToolBar->addAction(CryIcon("icons:Navigation/Basics_Rotate.ico"), QString());
		action->setEnabled(false);
		action->setPriority(QAction::LowPriority);
		action = m_modeToolBar->addAction(CryIcon("icons:Navigation/Basics_Scale.ico"), QString());
		action->setEnabled(false);
		action->setPriority(QAction::LowPriority);
	}
}

void CharacterToolForm::OnExplorerSelectionChanged()
{
	ExplorerEntries entries;
	m_system->document->GetSelectedExplorerEntries(&entries);

	if (entries.size() == 1)
	{
		ExplorerEntry* selectedEntry = entries[0];
		if (m_system->document->IsActiveInDocument(selectedEntry))
		{
			InstallMode(m_modeCharacter.data(), selectedEntry);
		}
	}
}

void CharacterToolForm::OnViewportOptionsChanged()
{
	ReadViewportOptions(m_system->document->GetViewportOptions(), m_system->document->GetDisplayOptions()->animation);
}

void CharacterToolForm::OnViewportUpdate()
{
	// This is called when viewport is asking for redraw, e.g. during the resize
	OnIdleUpdate();
}

void CharacterToolForm::OnAnimEventPresetPanelPutEvent()
{

}

bool CharacterToolForm::event(QEvent* ev)
{
	switch (ev->type())
	{
	case QEvent::WindowActivate:
		GetIEditor()->SetActiveView(0);

		m_system->document->EnableAudio(true);
		if (m_splitViewport)
		{
			m_splitViewport->OriginalViewport()->SetForegroundUpdateMode(true);
			m_splitViewport->CompressedViewport()->SetForegroundUpdateMode(true);
		}
		break;
	case QEvent::WindowDeactivate:
		m_system->document->EnableAudio(false);
		if (m_splitViewport)
		{
			m_splitViewport->OriginalViewport()->SetForegroundUpdateMode(false);
			m_splitViewport->CompressedViewport()->SetForegroundUpdateMode(false);
		}
		break;
	case QEvent::FocusIn:
		LoadGlobalData();
		break;
	default:
		break;
	}

	return __super::event(ev);
}

void CharacterToolForm::SerializeLayout(Serialization::IArchive& ar)
{
	Serialization::SContext formContext(ar, this);
	ar(*this, "state");
}

void CharacterToolForm::SaveLayoutToFile(const char* szFilePath)
{
	yasli::JSONOArchive oa;
	SerializeLayout(oa);

	QDir().mkpath(QFileInfo(szFilePath).absolutePath());
	oa.save(szFilePath);
}

void CharacterToolForm::LoadLayoutFromFile(const char* szFilePath)
{
	yasli::JSONIArchive ia;
	if (ia.load(szFilePath))
	{
		SerializeLayout(ia);

		OnViewportOptionsChanged();
		UpdatePanesMenu();
	}
}

void CharacterToolForm::OnFileRecentAboutToShow(QMenu* recentMenu)
{
	recentMenu->clear();

	if (m_recentCharacters.empty())
	{
		recentMenu->addAction("No characters were open recently")->setEnabled(false);
		return;
	}

	for (size_t i = 0; i < m_recentCharacters.size(); ++i)
	{
		const char* fullPath = m_recentCharacters[i].c_str();
		string path;
		string file;
		PathUtil::Split(fullPath, path, file);
		QString text;
		if (i < 10 - 1)
			text.sprintf("&%d. %s\t%s", int(i + 1), file.c_str(), path.c_str());
		else if (i == 10 - 1)
			text.sprintf("1&0. %s\t%s", file.c_str(), path.c_str());
		else
			text.sprintf("%d. %s\t%s", int(i + 1), file.c_str(), path.c_str());
		QAction* action = recentMenu->addAction(text);
		action->setData(QString::fromLocal8Bit(fullPath));
		EXPECTED(connect(action, SIGNAL(triggered()), this, SLOT(OnFileRecent())));
	}
}

CharacterToolForm::~CharacterToolForm()
{
	GetIEditor()->GetGlobalBroadcastManager()->DisconnectObject(this);
	LeaveMode();
	m_system->gizmoSink->SetScene(0);
	m_dockWidgetManager->Clear();
	m_system->explorerPanels.clear();
}

void CharacterToolForm::closeEvent(QCloseEvent* ev)
{
	if (m_closed)
		return;

	ExplorerEntries unsavedEntries;

	m_system->explorerData->GetUnsavedEntries(&unsavedEntries);

	vector<string> unsavedFilenames;
	m_system->explorerData->GetSaveFilenames(&unsavedFilenames, unsavedEntries);

	typedef std::map<string, std::vector<ExplorerEntry*>> UnsavedFilenameToEntries;
	UnsavedFilenameToEntries unsavedFilenameToEntries;
	for (size_t i = 0; i < unsavedFilenames.size(); ++i)
		unsavedFilenameToEntries[unsavedFilenames[i]].push_back(unsavedEntries[i]);

	if (!GetIEditor()->IsMainFrameClosing() && !unsavedEntries.empty())
	{
		DynArray<string> filenamesToSave;
		{
			DynArray<string> filenames;
			filenames.reserve(unsavedFilenameToEntries.size());
			std::transform(unsavedFilenameToEntries.begin(), unsavedFilenameToEntries.end(), std::back_inserter(filenames), [](const auto& it) { return it.first; });
			if (!UnsavedChangesDialog(this, &filenamesToSave, filenames))
			{
				ev->ignore();
				return;
			}
		}

		ActionOutput saveOutput;
		for (const auto& it : unsavedFilenameToEntries)
		{
			const auto& filename = it.first;
			const auto& entries = it.second;

			const auto& handler = std::find(filenamesToSave.begin(), filenamesToSave.end(), filename) != filenamesToSave.end()
			                      ? std::function<void(ExplorerEntry*)>([&](ExplorerEntry* entry) { m_system->explorerData->SaveEntry(&saveOutput, entry); })
			                      : std::function<void(ExplorerEntry*)>([&](ExplorerEntry* entry) { m_system->explorerData->Revert(entry); });

			std::for_each(entries.begin(), entries.end(), handler);
		}

		if (saveOutput.errorCount > 0)
		{
			ev->ignore();
			saveOutput.Show(this);
			return;
		}
	}

	m_closed = true;

	SaveLayoutToFile(GetStateFilename());
	m_animEventPresetPanel->SavePresets();
}

bool CharacterToolForm::eventFilter(QObject* sender, QEvent* ev)
{
	if (sender == m_splitViewport->CompressedViewport())
	{
		if (ev->type() == QEvent::KeyPress)
		{
			QKeyEvent* press = (QKeyEvent*)(ev);

			static QViewport* fullscreenViewport;
			static QWidget* previousParent;
			if (press->key() == Qt::Key_F11)
			{
				QViewport* viewport = m_splitViewport->CompressedViewport();
				if (fullscreenViewport == viewport)
				{
					fullscreenViewport = 0;
					viewport->showFullScreen();
					m_splitViewport->FixLayout();
				}
				else
				{
					previousParent = (QWidget*)viewport->parent();
					fullscreenViewport = viewport;
					viewport->setParent(0);
					//		viewport->resize(960, 1080);
					viewport->resize(1920, 1080);
					viewport->setWindowFlags(Qt::FramelessWindowHint);
					viewport->move(0, 0);
					viewport->show();
					//viewport->showFullScreen();
				}
			}
		}
#if 0
		if (ev->type() == QEvent::KeyPress)
		{
			QKeyEvent* press = (QKeyEvent*)(ev);
			if (press->key() == QKeySequence(Qt::Key_F1))
			{
				static int index;
				char buf[128];
				cry_sprintf(buf, "Objects/%i/test.chr", index);
				m_system->document->CharacterList()->AddCharacterEntry(buf);
				++index;
				return true;
			}
		}
#endif
	}
	return false;
}

void CharacterToolForm::customEvent(QEvent* event)
{
	// TODO: This handler should be removed whenever this editor is refactored to be a CDockableEditor
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		const string& command = commandEvent->GetCommand();
		if (command == "general.help")
		{
			event->setAccepted(EditorUtils::OpenHelpPage(GetPaneTitle()));
		}
	}

	if (!event->isAccepted())
	{
		QWidget::customEvent(event);
	}
}

void CharacterToolForm::OnFileRecent()
{
	if (QAction* action = qobject_cast<QAction*>(sender()))
	{
		QString path = action->data().toString();
		if (!path.isEmpty())
		{
			m_system->scene->characterPath = path.toLocal8Bit().data();
			m_system->scene->SignalCharacterChanged();
		}
	}
}

void CharacterToolForm::OnFileCleanAnimations()
{
	ShowCleanCompiledAnimationsTool(this);
}

void CharacterToolForm::OnFileResaveAnimSettings()
{
	ShowResaveAnimSettingsTool(m_system->animationList.get(), this);
}

void CharacterToolForm::OnDockWidgetsChanged()
{
	UpdatePanesMenu();
}

}

