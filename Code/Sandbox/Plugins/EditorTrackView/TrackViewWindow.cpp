// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TrackViewWindow.h"

#include "TrackViewComponentsManager.h"
#include "TrackViewBatchRenderDlg.h"
#include "TrackViewSequenceDialog.h"
#include "TrackViewEventsDialog.h"
#include "TrackViewExporter.h"
#include "TrackViewPlugin.h"
#include "Controls/SequenceTabWidget.h"
#include "Timeline.h"

#include "Menu/AbstractMenu.h"
#include "QControls.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QToolBar>
#include <QSplitter>

std::vector<CTrackViewWindow*> CTrackViewWindow::ms_trackViewWindows;

CTrackViewWindow::CTrackViewWindow(QWidget* pParent)
	: CDockableEditor(pParent)
{
	m_pTrackViewCore = new CTrackViewCore();
	m_pTrackViewCore->Init();

	setAttribute(Qt::WA_DeleteOnClose);

	if (CTrackViewComponentsManager* pComponentsManager = m_pTrackViewCore->GetComponentsManager())
	{
		QVBoxLayout* pWindowLayout = new QVBoxLayout();
		pWindowLayout->setContentsMargins(1, 1, 1, 1);

		QMainWindow* pToolBarsFrame = new QMainWindow();
		pToolBarsFrame->addToolBar(Qt::ToolBarArea::TopToolBarArea, pComponentsManager->GetTrackViewPlaybackToolbar());
		pToolBarsFrame->addToolBar(Qt::ToolBarArea::TopToolBarArea, pComponentsManager->GetTrackViewSequenceToolbar());
		pToolBarsFrame->addToolBar(Qt::ToolBarArea::TopToolBarArea, pComponentsManager->GetTrackViewKeysToolbar());

		QToolBar* pCurveEditorToolbar = new QToolBar("Curve Controls");
		pToolBarsFrame->addToolBar(Qt::ToolBarArea::TopToolBarArea, pCurveEditorToolbar);

		pWindowLayout->addWidget(pToolBarsFrame);

		CTrackViewSequenceTabWidget* pSequenceTabWidget = pComponentsManager->GetTrackViewSequenceTabWidget();
		pSequenceTabWidget->SetToolbar(pCurveEditorToolbar);

		QSplitter* pContentLayout = new QSplitter(Qt::Horizontal);
		pContentLayout->setContentsMargins(0, 0, 0, 0);
		pContentLayout->addWidget(pComponentsManager->GetTrackViewSequenceTabWidget());
		pContentLayout->addWidget(pComponentsManager->GetTrackViewPropertyTreeWidget());

		pWindowLayout->addWidget(pContentLayout, 1);

		SetContent(pWindowLayout);
		setObjectName(QStringLiteral("TrackView"));

		pWindowLayout->update();

		const int32 totalSize = pContentLayout->size().width();
		if (totalSize > 240)
		{
			QList<int> sizes;
			sizes << totalSize - 120 << 120;
			pContentLayout->setSizes(sizes);
		}
	}

	stl::push_back_unique(ms_trackViewWindows, this);

	InitMenu();
 	InstallReleaseMouseFilter(this);
}

CTrackViewWindow::~CTrackViewWindow()
{
	CTrackViewPlugin::GetAnimationContext()->SetSequence(nullptr, false, false);

	delete m_pTrackViewCore;
	stl::find_and_erase(ms_trackViewWindows, this);
}

void CTrackViewWindow::SetLayout(const QVariantMap& state)
{
	CEditor::SetLayout(state);

	QVariant unitVar = state.value("unit");
	if (unitVar.isValid())
	{
		m_pTrackViewCore->SetDisplayMode((SAnimTime::EDisplayMode)unitVar.toInt());
	}

	auto tabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	assert(tabWidget);

	QVariant linkEnabledVar = state.value("linkEnabled");
	if (linkEnabledVar.isValid())
	{
		tabWidget->EnableTimelineLink(linkEnabledVar.toBool());
	}

	QVariant dopesheetVisibleVar = state.value("dopesheetVisible");
	if (dopesheetVisibleVar.isValid())
	{
		tabWidget->ShowDopeSheet(dopesheetVisibleVar.toBool());
	}

	QVariant curveVisibleVar = state.value("curveVisible");
	if (curveVisibleVar.isValid())
	{
		tabWidget->ShowCurveEditor(curveVisibleVar.toBool());
	}

	QVariant keyTextVar = state.value("keyText");
	if (keyTextVar.isValid())
	{
		tabWidget->ShowKeyText(keyTextVar.toBool());
	}

	QVariant snapModeVar = state.value("snapMode");
	if (snapModeVar.isValid())
	{
		m_pTrackViewCore->SetSnapMode(static_cast<ETrackViewSnapMode>(snapModeVar.toInt()));
	}
}

QVariantMap CTrackViewWindow::GetLayout() const
{
	QVariantMap state = CEditor::GetLayout();

	state.insert("unit", (int)m_pTrackViewCore->GetCurrentDisplayMode());

	auto tabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	assert(tabWidget);

	state.insert("linkEnabled", tabWidget->IsTimelineLinkEnabled());
	state.insert("dopesheetVisible", tabWidget->IsDopeSheetVisible());
	state.insert("curveVisible", tabWidget->IsCurveEditorVisible());
	state.insert("keyText", tabWidget->IsShowingKeyText());
	state.insert("snapMode", m_pTrackViewCore->GetCurrentSnapMode());
	return state;
}

void CTrackViewWindow::InitMenu()
{
	CTrackViewComponentsManager* pComponentsManager = m_pTrackViewCore->GetComponentsManager();

	const CEditor::MenuItems items[] = {
		CEditor::MenuItems::FileMenu,
		CEditor::MenuItems::EditMenu,CEditor::MenuItems::ViewMenu,  CEditor::MenuItems::New,
		CEditor::MenuItems::Open,    CEditor::MenuItems::Close,     CEditor::MenuItems::Undo,
		CEditor::MenuItems::Redo,    CEditor::MenuItems::Copy,      CEditor::MenuItems::Cut,
		CEditor::MenuItems::Paste,   CEditor::MenuItems::Delete,    CEditor::MenuItems::Duplicate
	};
	AddToMenu(&items[0], CRY_ARRAY_COUNT(items));

	{
		CAbstractMenu* const pFileMenu = GetMenu(CEditor::MenuItems::FileMenu);

		int sec = pFileMenu->GetNextEmptySection();

		auto action = pFileMenu->CreateAction(tr("Delete Sequence..."), sec);
		connect(action, &QAction::triggered, this, &CTrackViewWindow::OnDeleteSequence);

		sec = pFileMenu->GetNextEmptySection();

		action = pFileMenu->CreateAction(tr("Import..."), sec);
		connect(action, &QAction::triggered, this, &CTrackViewWindow::OnImportSequence);

		action = pFileMenu->CreateAction(tr("Export..."), sec);
		connect(action, &QAction::triggered, this, &CTrackViewWindow::OnExportSequence);
	}

	{
		CAbstractMenu* pEditMenu = GetMenu(MenuItems::EditMenu);

		const int sec = pEditMenu->GetNextEmptySection();

		auto action = pEditMenu->CreateAction(tr("New Event"), sec);
		connect(action, &QAction::triggered, this, &CTrackViewWindow::OnNewEvent);

		action = pEditMenu->CreateAction(tr("Show Events"), sec);
		connect(action, &QAction::triggered, this, &CTrackViewWindow::OnShowAllEvents);
	}

	CAbstractMenu* const pPlaybackMenu = GetMenu(tr("Playback"));
	pPlaybackMenu->signalAboutToShow.Connect([pPlaybackMenu, this]()
	{
		pPlaybackMenu->Clear();
		m_pTrackViewCore->GetComponentsManager()->GetTrackViewPlaybackToolbar()->PopulatePlaybackMenu(pPlaybackMenu);
	});

	CAbstractMenu* const pViewMenu = GetMenu(MenuItems::ViewMenu);
	pViewMenu->signalAboutToShow.Connect([pViewMenu, this]()
	{
		pViewMenu->Clear();

		AddToMenu(CEditor::MenuItems::ZoomIn);
		AddToMenu(CEditor::MenuItems::ZoomOut);

		CTrackViewComponentsManager* pComponentsManager = m_pTrackViewCore->GetComponentsManager();

		int sec = pViewMenu->GetNextEmptySection();

		auto action = pViewMenu->CreateAction(tr("Show Dopesheet"), sec);
		action->setCheckable(true);
		action->setChecked(pComponentsManager->GetTrackViewSequenceTabWidget()->IsDopeSheetVisible());
		connect(action, &QAction::triggered, pComponentsManager->GetTrackViewSequenceTabWidget(), &CTrackViewSequenceTabWidget::ShowDopeSheet);

		action = pViewMenu->CreateAction(tr("Show Curve Editor"), sec);
		action->setCheckable(true);
		action->setChecked(pComponentsManager->GetTrackViewSequenceTabWidget()->IsCurveEditorVisible());
		connect(action, &QAction::triggered, pComponentsManager->GetTrackViewSequenceTabWidget(), &CTrackViewSequenceTabWidget::ShowCurveEditor);

		action = pViewMenu->CreateAction(tr("Sequence Properties"), sec);
		connect(action, &QAction::triggered, this, &CTrackViewWindow::OnSequenceProperties);

		sec = pViewMenu->GetNextEmptySection();

		action = pViewMenu->CreateAction(tr("Link Timelines"), sec);
		action->setCheckable(true);
		action->setChecked(pComponentsManager->GetTrackViewSequenceTabWidget()->IsTimelineLinkEnabled());
		connect(action, &QAction::triggered, pComponentsManager->GetTrackViewSequenceTabWidget(), &CTrackViewSequenceTabWidget::EnableTimelineLink);

		action = pViewMenu->CreateAction(tr("Show Key Text"), sec);
		action->setCheckable(true);
		action->setChecked(pComponentsManager->GetTrackViewSequenceTabWidget()->IsShowingKeyText());
		connect(action, &QAction::triggered, pComponentsManager->GetTrackViewSequenceTabWidget(), &CTrackViewSequenceTabWidget::ShowKeyText);

		sec = pViewMenu->GetNextEmptySection();
		pViewMenu->SetSectionName(sec, "Units");

		action = pViewMenu->CreateAction(tr("Ticks"), sec);
		action->setCheckable(true);
		action->setChecked(m_pTrackViewCore->GetCurrentDisplayMode() == SAnimTime::EDisplayMode::Ticks);
		action->connect(action, &QAction::triggered, [this]() { OnUnitsChanged(SAnimTime::EDisplayMode::Ticks); });

		action = pViewMenu->CreateAction(tr("Time"), sec);
		action->setCheckable(true);
		action->setChecked(m_pTrackViewCore->GetCurrentDisplayMode() == SAnimTime::EDisplayMode::Time);
		action->connect(action, &QAction::triggered, [this]() { OnUnitsChanged(SAnimTime::EDisplayMode::Time); });

		action = pViewMenu->CreateAction(tr("Timecode"), sec);
		action->setCheckable(true);
		action->setChecked(m_pTrackViewCore->GetCurrentDisplayMode() == SAnimTime::EDisplayMode::Timecode);
		action->connect(action, &QAction::triggered, [this]() { OnUnitsChanged(SAnimTime::EDisplayMode::Timecode); });

		action = pViewMenu->CreateAction(tr("Frames"), sec);
		action->setCheckable(true);
		action->setChecked(m_pTrackViewCore->GetCurrentDisplayMode() == SAnimTime::EDisplayMode::Frames);
		action->connect(action, &QAction::triggered, [this]() { OnUnitsChanged(SAnimTime::EDisplayMode::Frames); });
	});

	CAbstractMenu* const pToolsMenu = GetMenu(tr("Tools"));
	pToolsMenu->signalAboutToShow.Connect([pToolsMenu, this]()
	{
		pToolsMenu->Clear();
		auto action = pToolsMenu->CreateAction(tr("Render Sequence"));
		action->connect(action, &QAction::triggered, this, &CTrackViewWindow::OnRender);

		int sec = pToolsMenu->GetNextEmptySection();

		action = pToolsMenu->CreateAction(tr(gEnv->pMovieSystem->FindSequence("_LightAnimationSet") != nullptr ? "Open Light Animation Set" : "Create Light Animation Set"), sec);
		action->connect(action, &QAction::triggered, [this]() { OnLightAnimationSet(); });
	});
}

bool CTrackViewWindow::OnNew()
{
	m_pTrackViewCore->NewSequence();
	return true;
}

bool CTrackViewWindow::OnOpen()
{
	CTrackViewSequenceDialog dialog(CTrackViewSequenceDialog::OpenSequences);
	if (dialog.exec())
	{
		for (const CryGUID& seqGuid : dialog.GetSequenceGuids())
		{
			m_pTrackViewCore->OpenSequence(seqGuid);
		}
	}
	return true;
}

bool CTrackViewWindow::OnClose()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		if (m_pTrackViewCore->IsPlaying() || m_pTrackViewCore->IsPaused())
		{
			m_pTrackViewCore->OnStop();
		}

		CTrackViewPlugin::GetAnimationContext()->SetSequence(nullptr, false, false);

		pTabWidget->CloseCurrentTab();
	}

	return true;
}

bool CTrackViewWindow::OnUndo()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->SignalUndo(); //Only triggers internal signals
		}
	}

	//uses global undo
	return false;
}

bool CTrackViewWindow::OnRedo()
{

	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->SignalRedo(); //Only triggers internal signals
		}
	}

	//uses global undo
	return false;
}

bool CTrackViewWindow::OnCopy()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->OnMenuCopy();
		}
	}

	return true;
}

bool CTrackViewWindow::OnCut()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->OnMenuCut();
		}
	}

	return true;
}

bool CTrackViewWindow::OnPaste()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->OnMenuPaste();
		}
	}

	return true;
}

bool CTrackViewWindow::OnDelete()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->OnMenuDelete();
		}
	}

	return true;
}

bool CTrackViewWindow::OnDuplicate()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->OnMenuDuplicate();
		}
	}

	return true;
}

bool CTrackViewWindow::OnZoomIn()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->ZoomStep(1);
		}
	}
	return true;
}

bool CTrackViewWindow::OnZoomOut()
{
	auto pTabWidget = m_pTrackViewCore->GetComponentsManager()->GetTrackViewSequenceTabWidget();
	if (pTabWidget)
	{
		auto pTimeline = pTabWidget->GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimeline->ZoomStep(-1);
		}
	}
	return true;
}

void CTrackViewWindow::OnExportSequence()
{
	CTrackViewPlugin::GetExporter()->ExportToFile(true);
}

void CTrackViewWindow::OnImportSequence()
{
	CTrackViewPlugin::GetExporter()->ImportFromFile();
}

void CTrackViewWindow::OnUnitsChanged(SAnimTime::EDisplayMode mode)
{
	m_pTrackViewCore->SetDisplayMode(mode);
	m_pTrackViewCore->UpdateProperties();
}

void CTrackViewWindow::OnRender()
{
	CTrackViewBatchRenderDlg dlg;
	dlg.exec();
}

void CTrackViewWindow::OnDeleteSequence()
{
	CTrackViewSequenceDialog dialog(CTrackViewSequenceDialog::DeleteSequences);
	dialog.exec();
}

void CTrackViewWindow::OnSequenceProperties()
{
	if (CTrackViewComponentsManager* pManager = m_pTrackViewCore->GetComponentsManager())
	{
		if (CTrackViewSequenceTabWidget* pTabWidget = pManager->GetTrackViewSequenceTabWidget())
		{
			if (CTrackViewSequence* pSequence = pTabWidget->GetActiveSequence())
			{
				pTabWidget->ShowSequenceProperties(pSequence);
			}
		}
	}
}

void CTrackViewWindow::OnNewEvent()
{
	if (CTrackViewComponentsManager* pManager = m_pTrackViewCore->GetComponentsManager())
	{
		if (CTrackViewSequenceTabWidget* pTabWidget = pManager->GetTrackViewSequenceTabWidget())
		{
			if (CTrackViewSequence* pSequence = pTabWidget->GetActiveSequence())
			{
				CTrackViewNewEventDialog dialog(*pSequence);
				if (dialog.exec())
				{
					pSequence->AddTrackEvent(dialog.GetEventName().c_str());
				}
			}
		}
	}
}

void CTrackViewWindow::OnShowAllEvents()
{
	if (CTrackViewComponentsManager* pManager = m_pTrackViewCore->GetComponentsManager())
	{
		if (CTrackViewSequenceTabWidget* pTabWidget = pManager->GetTrackViewSequenceTabWidget())
		{
			if (CTrackViewSequence* pSequence = pTabWidget->GetActiveSequence())
			{
				CTrackViewEventsDialog dialog(*pSequence);
				dialog.exec();
			}
		}
	}
}

void CTrackViewWindow::OnLightAnimationSet()
{
	CTrackViewSequence* pLightAnimSeq = CTrackViewPlugin::GetSequenceManager()->GetSequenceByName("_LightAnimationSet");
	if (!pLightAnimSeq)
	{
		pLightAnimSeq = CTrackViewPlugin::GetSequenceManager()->CreateSequence("_LightAnimationSet");
		pLightAnimSeq->SetFlags(IAnimSequence::eSeqFlags_LightAnimationSet);
	}

	m_pTrackViewCore->OpenSequence(pLightAnimSeq->GetGUID());
}

void CTrackViewWindow::keyPressEvent(QKeyEvent* e)
{
	if (e->key() == Qt::Key_Space)
	{
		if (!m_pTrackViewCore->IsPlaying())
			m_pTrackViewCore->OnPlayPause(true);
		else
			m_pTrackViewCore->OnPlayPause(m_pTrackViewCore->IsPaused());
	}
}

