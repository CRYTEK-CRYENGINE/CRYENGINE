// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OpenProjectPanel.h"
#include "ProjectManagement/Model/ProjectsModel.h"
#include "ProjectManagement/UI/SelectProjectDialog.h"

#include <Controls/QuestionDialog.h>
#include <FileDialogs/SystemFileDialog.h>
#include <QAdvancedTreeView.h>
#include <QSearchBox.h>
#include <QThumbnailView.h>
#include <QtUtil.h>

#include <QBoxLayout>
#include <QButtonGroup>
#include <QKeyEvent>
#include <QHeaderView>
#include <QPushButton>
#include <QToolButton>

COpenProjectPanel::COpenProjectPanel(CSelectProjectDialog* pParent, bool runOnSandboxInit)
	: QWidget(pParent)
	, m_pParent(pParent)
	, m_pModel(new CProjectsModel(this, pParent->GetProjectManager()))
	, m_pSortedModel(new CProjectSortProxyModel(this))
	, m_pMainLayout(new QVBoxLayout(this))
	, m_pTreeView(new QAdvancedTreeView)
	, m_pThumbnailView(new QThumbnailsView(nullptr, false, this))
{
	m_pMainLayout->setMargin(0);
	m_pMainLayout->setSpacing(0);
	setLayout(m_pMainLayout);

	m_pSortedModel->setSourceModel(m_pModel);
	m_pSortedModel->setSortCaseSensitivity(Qt::CaseInsensitive);

	CreateSearchPanel();
	CreateViews();
	CreateDialogButtons(runOnSandboxInit);

	SetViewMode(false);
	SelectProject(m_pParent->GetProjectManager().GetLastUsedProject());
}

void COpenProjectPanel::CreateSearchPanel()
{

	QWidget* pSearchBoxContainer = new QWidget();
	pSearchBoxContainer->setObjectName("SearchBoxContainer");

	QHBoxLayout* pSearchBoxLayout = new QHBoxLayout();
	pSearchBoxLayout->setAlignment(Qt::AlignTop);
	pSearchBoxLayout->setMargin(0);
	pSearchBoxLayout->setSpacing(0);

	QSearchBox* pSearchBox = new QSearchBox(this);
	pSearchBox->EnableContinuousSearch(true);
	pSearchBox->SetModel(m_pSortedModel);


	pSearchBoxLayout->addWidget(pSearchBox);
	pSearchBoxContainer->setLayout(pSearchBoxLayout);

	m_pMainLayout->addWidget(pSearchBoxContainer);
}

void COpenProjectPanel::CreateViews()
{
	m_pTreeView->setModel(m_pSortedModel);
	m_pTreeView->sortByColumn(CProjectsModel::eColumn_Name, Qt::AscendingOrder);
	m_pTreeView->setRootIsDecorated(false);

	m_pTreeView->header()->setMinimumSectionSize(24);
	m_pTreeView->header()->setSectionResizeMode(CProjectsModel::eColumn_RunOnStartup, QHeaderView::Fixed);
	m_pTreeView->header()->resizeSection(CProjectsModel::eColumn_RunOnStartup, 24);

	m_pTreeView->header()->resizeSection(CProjectsModel::eColumn_Name, fontMetrics().width(QStringLiteral("wwwwwwwwwwwwwwwwwwwwwwwwwwwww")));
	m_pTreeView->header()->resizeSection(CProjectsModel::eColumn_LastAccessTime, fontMetrics().width(QStringLiteral("wwwwwwwwwwwwwwwwww")));

	connect(m_pTreeView, &QAbstractItemView::activated, this, &COpenProjectPanel::OpenProject);
	connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &COpenProjectPanel::OnSelectionChanged);

	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(m_pTreeView, &QTreeView::customContextMenuRequested, this, &COpenProjectPanel::OnContextMenu);

	m_pThumbnailView->SetModel(m_pSortedModel);
	m_pThumbnailView->SetItemSizeBounds({ 96, 96 }, { 96, 96 });
	m_pThumbnailView->SetRootIndex(QModelIndex());
	m_pThumbnailView->SetDataColumn(CProjectsModel::eColumn_Name);

	m_pThumbnailView->setContextMenuPolicy(Qt::CustomContextMenu);

	QAbstractItemView* const pView = m_pThumbnailView->GetInternalView();
	pView->setSelectionMode(QAbstractItemView::SingleSelection);
	pView->setSelectionBehavior(QAbstractItemView::SelectRows);
	pView->setSelectionModel(m_pTreeView->selectionModel());
	pView->update();

	QObject::connect(pView, &QTreeView::customContextMenuRequested, this, &COpenProjectPanel::OnContextMenu);

	connect(pView, &QAbstractItemView::activated, this, &COpenProjectPanel::OpenProject);

	QHBoxLayout* pLayout = new QHBoxLayout;
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(m_pTreeView);
	pLayout->addWidget(m_pThumbnailView);

	QVBoxLayout* pButtonsLayout = new QVBoxLayout;
	pButtonsLayout->setMargin(0);
	pButtonsLayout->setSpacing(0);
	pButtonsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

	QButtonGroup* pViewModeButtons = new QButtonGroup(this);
	CreateListViewButton(pViewModeButtons);
	CreateThumbnailButton(pViewModeButtons);

	const QList<QAbstractButton*> buttons = pViewModeButtons->buttons();
	for (QAbstractButton* pButton : buttons)
	{
		pButtonsLayout->addWidget(pButton);
	}

	pLayout->addLayout(pButtonsLayout);
	m_pMainLayout->addLayout(pLayout);
}

void COpenProjectPanel::CreateListViewButton(QButtonGroup* pGroup)
{
	QToolButton* pButton = new QToolButton;
	connect(pButton, &QToolButton::clicked, this, [this]() { SetViewMode(false); });
	pButton->setIcon(CryIcon("icons:common/general_view_list.ico"));
	pButton->setCheckable(true);
	pButton->setAutoRaise(true);
	pButton->setToolTip("Shows Details");
	pButton->setChecked(true);
	pGroup->addButton(pButton);
}

void COpenProjectPanel::CreateThumbnailButton(QButtonGroup* pGroup)
{
	QToolButton* pButton = new QToolButton;
	connect(pButton, &QToolButton::clicked, this, [this]() { SetViewMode(true); });
	pButton->setIcon(CryIcon("icons:common/general_view_thumbnail.ico"));
	pButton->setCheckable(true);
	pButton->setAutoRaise(true);
	pButton->setToolTip("Shows Thumbnails");
	pButton->setChecked(false);
	pGroup->addButton(pButton);
}

void COpenProjectPanel::CreateDialogButtons(bool runOnSandboxInit)
{
	m_pAddProjectBtn = new QPushButton("Add Existing...");
	connect(m_pAddProjectBtn, &QPushButton::clicked, this, &COpenProjectPanel::OnAddProject);

	m_pOpenProjectBtn = new QPushButton("Open", this);
	m_pOpenProjectBtn->setDefault(true);
	m_pOpenProjectBtn->setDisabled(m_pTreeView->selectionModel()->selection().count() == 0);
	connect(m_pOpenProjectBtn, &QPushButton::clicked, this, &COpenProjectPanel::OnLoadProjectPressed);

	QPushButton* pQuitBtn = new QPushButton(runOnSandboxInit ? "Quit" : "Cancel", this);
	connect(pQuitBtn, SIGNAL(clicked()), m_pParent, SLOT(reject()));

	QHBoxLayout* pButtonsLayout = new QHBoxLayout;
	pButtonsLayout->setMargin(0);
	pButtonsLayout->setSpacing(0);
	pButtonsLayout->addWidget(m_pAddProjectBtn);
	pButtonsLayout->addStretch();
	pButtonsLayout->addWidget(m_pOpenProjectBtn, 0, Qt::AlignRight);
	pButtonsLayout->addWidget(pQuitBtn, 0, Qt::AlignRight);

	m_pMainLayout->addLayout(pButtonsLayout);
}

void COpenProjectPanel::OnAddProject()
{
	CSystemFileDialog::OpenParams openParams(CSystemFileDialog::OpenFile);
	openParams.title = "Add Existing Project";
	openParams.initialDir = GetDefaultProjectsRootFolder();
	openParams.extensionFilters.push_back(CExtensionFilter({ "Project File" }, { "cryproject" }));

	CSystemFileDialog dialog(openParams, this);
	if (QDialog::Rejected == dialog.Execute())
	{
		return;
	}

	auto files = dialog.GetSelectedFiles();
	if (files.size() != 1)
	{
		assert(false);
		return;
	}

	const string fullPathToProject = files[0].toStdString().c_str();

	const auto& projects = m_pParent->GetProjectManager().GetProjects();
	for (const auto& proj : projects)
	{
		if (proj.fullPathToCryProject.CompareNoCase(fullPathToProject) == 0)
		{
			SelectProject(&proj);
			return;
		}
	}

	const auto& hiddenProjects = m_pParent->GetProjectManager().GetHiddenProjects();
	for (const auto& proj : hiddenProjects)
	{
		if (proj.fullPathToCryProject.CompareNoCase(fullPathToProject) == 0)
		{
			CQuestionDialog::SWarning("Error", "The specified project is not compatible with this version of CRYENGINE.", QDialogButtonBox::Ok, QDialogButtonBox::Ok, this);
			return;
		}
	}

	m_pParent->GetProjectManager().ImportProject(fullPathToProject);
	SelectProject(m_pParent->GetProjectManager().GetLastUsedProject());
}

void COpenProjectPanel::SelectProject(const SProjectDescription* pProject)
{
	m_pTreeView->selectionModel()->clear();
	auto idx = m_pModel->IndexFromProject(pProject);
	idx = m_pSortedModel->mapFromSource(idx);
	m_pTreeView->selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void COpenProjectPanel::SetViewMode(bool thumbnailMode)
{
	m_pTreeView->setVisible(!thumbnailMode);
	m_pThumbnailView->setVisible(thumbnailMode);
}

void COpenProjectPanel::OnContextMenu(const QPoint& pos)
{
	const QAbstractItemView* pView = m_pTreeView->isVisible() ? m_pTreeView : m_pThumbnailView->GetInternalView();
	QModelIndex index = pView->indexAt(pos);

	QMenu* pMenu = new QMenu;

	if (index.isValid())
	{
		const SProjectDescription* pDescription = m_pModel->ProjectFromIndex(m_pSortedModel->mapToSource(index));

		QAction* pAction = pMenu->addAction("Show in File Explorer...");
		connect(pAction, &QAction::triggered, this, [=] { QtUtil::OpenInExplorer(pDescription->rootFolder.c_str()); });

		pAction = pMenu->addAction(pDescription->startupProject ? "Unset as Startup Project" : "Set as Startup Project");
		pAction->setIcon(CryIcon("icons:General/Startup_Project.ico"));
		connect(pAction, &QAction::triggered, this, [=] {m_pParent->GetProjectManager().ToggleStartupProperty(*pDescription); });

		pAction = pMenu->addAction("Delete");
		pAction->setIcon(CryIcon("icons:General/Folder_Remove.ico"));
		connect(pAction, &QAction::triggered, this, [=] { OnDeleteProject(pDescription); });

		pMenu->addSeparator();
	}

	QAction* pAction = pMenu->addAction("Add Existing...");
	connect(pAction, &QAction::triggered, this, [=] { OnAddProject(); });

	pMenu->popup(pView->viewport()->mapToGlobal(pos));
}

void COpenProjectPanel::OnDeleteProject(const SProjectDescription* pDescription)
{
	QString titleText = "Warning";
	QString messageText = "<b>Deletion</b><br>"
	                      "Are you sure you want to remove that project from Project Browser?";

	CQuestionDialog question;

	bool removeFromDisk = false;
	question.AddCheckBox("Remove project from disk (permanently)", &removeFromDisk);

	question.SetupWarning(titleText, messageText, QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No);
	if (QDialogButtonBox::StandardButton::Yes != question.Execute())
	{
		return;
	}

	m_pParent->GetProjectManager().DeleteProject(*pDescription, removeFromDisk);
	m_pTreeView->selectionModel()->clearSelection();
}

void COpenProjectPanel::OnLoadProjectPressed()
{
	auto indexes = m_pTreeView->selectionModel()->selectedIndexes();
	if (indexes.empty())
	{
		return;
	}
	OpenProject(indexes.at(0));
}

void COpenProjectPanel::OpenProject(const QModelIndex& index)
{
	if (!index.isValid())
	{
		CRY_ASSERT(false);
		return;
	}

	m_pParent->OpenProject(m_pModel->ProjectFromIndex(m_pSortedModel->mapToSource(index)));
}

void COpenProjectPanel::OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	m_pOpenProjectBtn->setEnabled(!selected.empty());
}

void COpenProjectPanel::keyPressEvent(QKeyEvent* pEvent)
{
	if (pEvent->key() == Qt::Key_Delete)
	{
		auto lst = m_pTreeView->selectionModel()->selectedIndexes();
		if (!lst.empty())
		{
			const SProjectDescription* pDescription = m_pModel->ProjectFromIndex(m_pSortedModel->mapToSource(lst.front()));
			OnDeleteProject(pDescription);

			pEvent->setAccepted(true);
			return;
		}
	}
	
	QWidget::keyPressEvent(pEvent);
}

void COpenProjectPanel::paintEvent(QPaintEvent*)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}
