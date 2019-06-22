// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CreateProjectPanel.h"

#include "ProjectManagement/Model/TemplatesModel.h"
#include "ProjectManagement/UI/SelectProjectDialog.h"

#include <FileDialogs/SystemFileDialog.h>
#include <QAdvancedTreeView.h>
#include <QSearchBox.h>
#include <QThumbnailView.h>

#include <QBoxLayout>
#include <QButtonGroup>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>

CCreateProjectPanel::CCreateProjectPanel(CSelectProjectDialog* pParent, bool runOnSandboxInit)
	: QWidget(pParent)
	, m_pParent(pParent)
	, m_pModel(new CTemplatesModel(this, pParent->GetTemplateManager()))
	, m_pSortedModel(new CTemplatesSortProxyModel(this))
	, m_pMainLayout(new QVBoxLayout(this))
	, m_pTreeView(new QAdvancedTreeView)
	, m_pThumbnailView(new QThumbnailsView(nullptr, false, this))
	, m_pCreateProjectBtn(new QPushButton("Create Project", this))
	, m_pOutputRootEdit(new QLineEdit(this))
	, m_pNewProjectNameEdit(new QLineEdit(this))
{
	m_pMainLayout->setMargin(0);
	m_pMainLayout->setSpacing(0);
	setLayout(m_pMainLayout);

	m_pSortedModel->setSourceModel(m_pModel);
	m_pSortedModel->setSortCaseSensitivity(Qt::CaseInsensitive);

	CreateSearchPanel();
	CreateViews();
	CreateOutputRootFolderEditBox();
	CreateProjectNameEditBox();
	CreateDialogButtons(runOnSandboxInit);

	SetViewMode(true);

	UpdateCreateProjectBtn();
}

void CCreateProjectPanel::CreateSearchPanel()
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

void CCreateProjectPanel::CreateViews()
{
	m_pTreeView->setModel(m_pSortedModel);
	m_pTreeView->sortByColumn(CTemplatesModel::eColumn_Name, Qt::AscendingOrder);
	m_pTreeView->setRootIsDecorated(false);
	m_pTreeView->selectionModel()->select(m_pTreeView->model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
	m_pTreeView->header()->setSectionsMovable(false);
	m_pTreeView->header()->resizeSection(CTemplatesModel::eColumn_Name, fontMetrics().width(QStringLiteral("wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww")));
	m_pTreeView->header()->resizeSection(CTemplatesModel::eColumn_Language, fontMetrics().width(QStringLiteral("wwwwwwwwwwww")));

	connect(m_pTreeView, &QAdvancedTreeView::doubleClicked, this, &CCreateProjectPanel::OnItemDoubleClicked);
	connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CCreateProjectPanel::OnSelectionChanged);

	m_pThumbnailView->SetModel(m_pSortedModel);
	m_pThumbnailView->SetItemSizeBounds({ 96, 96 }, { 96, 96 });
	m_pThumbnailView->SetRootIndex(QModelIndex());

	QAbstractItemView* const pView = m_pThumbnailView->GetInternalView();
	pView->setSelectionMode(QAbstractItemView::SingleSelection);
	pView->setSelectionBehavior(QAbstractItemView::SelectRows);
	pView->setSelectionModel(m_pTreeView->selectionModel());
	pView->update();

	connect(pView, &QAdvancedTreeView::doubleClicked, this, &CCreateProjectPanel::OnItemDoubleClicked);

	QHBoxLayout* pLayout = new QHBoxLayout;
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(m_pTreeView);
	pLayout->addWidget(m_pThumbnailView);

	QVBoxLayout* pButtonsLayout = new QVBoxLayout;
	pButtonsLayout->setMargin(0);
	pButtonsLayout->setSpacing(0);
	pButtonsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

	QButtonGroup* pGroup = new QButtonGroup(this);
	CreateListViewButton(pGroup);
	CreateThumbnailButton(pGroup);

	const QList<QAbstractButton*> buttons = pGroup->buttons();
	for (QAbstractButton* pButton : buttons)
	{
		pButtonsLayout->addWidget(pButton);
	}

	pLayout->addLayout(pButtonsLayout);

	m_pMainLayout->addLayout(pLayout);
}

void CCreateProjectPanel::CreateListViewButton(QButtonGroup* pGroup)
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

void CCreateProjectPanel::CreateThumbnailButton(QButtonGroup* pGroup)
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

void CCreateProjectPanel::CreateOutputRootFolderEditBox()
{
	QLabel* pLabel = new QLabel("Location", this);
	pLabel->setFixedWidth(100);

	m_pOutputRootEdit->setText(GetDefaultProjectsRootFolder());
	m_pOutputRootEdit->setReadOnly(true);

	QPushButton* pSelectFolderBtn = new QPushButton(this);
	pSelectFolderBtn->setIcon(CryIcon("icons:General/Folder.ico"));
	pSelectFolderBtn->setToolTip("Select output folder");
	pSelectFolderBtn->setFixedWidth(24);
	connect(pSelectFolderBtn, &QPushButton::clicked, this, &CCreateProjectPanel::OnChangeOutputRootFolder);

	QHBoxLayout* pLayout = new QHBoxLayout;
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(pLabel);
	pLayout->addWidget(m_pOutputRootEdit);
	pLayout->addWidget(pSelectFolderBtn);

	m_pMainLayout->addLayout(pLayout);
}

void CCreateProjectPanel::CreateProjectNameEditBox()
{
	QLabel* pLabel = new QLabel("Name", this);
	pLabel->setFixedWidth(100);

	m_pNewProjectNameEdit->setText("My Project");
	m_pNewProjectNameEdit->setReadOnly(false);

	connect(m_pNewProjectNameEdit, &QLineEdit::textChanged, this, &CCreateProjectPanel::OnNewProjectNameChanged);

	QHBoxLayout* pLayout = new QHBoxLayout;
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(pLabel);
	pLayout->addWidget(m_pNewProjectNameEdit);

	m_pMainLayout->addLayout(pLayout);
}

void CCreateProjectPanel::CreateDialogButtons(bool runOnSandboxInit)
{
	QPushButton* pQuitBtn = new QPushButton(runOnSandboxInit ? "Quit" : "Cancel", this);
	connect(pQuitBtn, SIGNAL(clicked()), m_pParent, SLOT(reject()));

	connect(m_pCreateProjectBtn, &QPushButton::clicked, this, &CCreateProjectPanel::OnCreateProjectPressed);

	QHBoxLayout* pButtonsLayout = new QHBoxLayout;
	pButtonsLayout->setAlignment(Qt::AlignRight | Qt::AlignBottom);
	pButtonsLayout->setMargin(0);
	pButtonsLayout->setSpacing(0);
	pButtonsLayout->addWidget(m_pCreateProjectBtn);
	pButtonsLayout->addWidget(pQuitBtn);

	m_pMainLayout->addLayout(pButtonsLayout);
}

void CCreateProjectPanel::SetViewMode(bool thumbnailMode)
{
	m_pTreeView->setVisible(!thumbnailMode);
	m_pThumbnailView->setVisible(thumbnailMode);
}

void CCreateProjectPanel::OnCreateProjectPressed()
{
	auto indexes = m_pTreeView->selectionModel()->selectedIndexes();
	if (indexes.empty())
	{
		return;
	}
	OnItemDoubleClicked(indexes.at(0));
}

void CCreateProjectPanel::OnItemDoubleClicked(const QModelIndex& index)
{
	if (!m_pCreateProjectBtn->isEnabled())
	{
		return;
	}

	if (!index.isValid())
	{
		CRY_ASSERT(false);
		return;
	}

	auto idx = m_pSortedModel->mapToSource(index);
	const STemplateDescription* pDescr = m_pModel->GetTemplateDescr(idx);
	if (!pDescr)
	{
		return;
	}

	m_pParent->CreateAndOpenProject(*pDescr, m_pOutputRootEdit->text(), m_pNewProjectNameEdit->text());
}

void CCreateProjectPanel::OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	UpdateCreateProjectBtn();
}

void CCreateProjectPanel::OnChangeOutputRootFolder()
{
	CSystemFileDialog::OpenParams openParams(CSystemFileDialog::SelectDirectory);
	openParams.title = "Select project template";
	openParams.initialDir = m_pOutputRootEdit->text();

	CSystemFileDialog dialog(openParams, this);
	if (dialog.Execute())
	{
		auto files = dialog.GetSelectedFiles();
		CRY_ASSERT(files.size() == 1);
		m_pOutputRootEdit->setText(files.front());
	}
}

void CCreateProjectPanel::OnNewProjectNameChanged(const QString& newProjectName)
{
	UpdateCreateProjectBtn();
}

void CCreateProjectPanel::UpdateCreateProjectBtn()
{
	bool enabled = true;
	if (m_pTreeView->selectionModel()->selectedIndexes().empty())
	{
		enabled = false;
	}

	if (m_pNewProjectNameEdit->text().isEmpty())
	{
		enabled = false;
	}

	if (QDir(m_pOutputRootEdit->text() + '/' + m_pNewProjectNameEdit->text()).exists())
	{
		enabled = false;
	}

	m_pCreateProjectBtn->setEnabled(enabled);
}

void CCreateProjectPanel::paintEvent(QPaintEvent*)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}