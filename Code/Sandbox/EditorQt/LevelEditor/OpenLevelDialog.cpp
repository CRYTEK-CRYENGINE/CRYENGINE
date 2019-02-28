// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "OpenLevelDialog.h"

#include "LevelFilterModel.h"
#include "LevelFileUtils.h"
#include "LevelAssetType.h"
#include "CryEditDoc.h"

#include "QSearchBox.h"
#include "ProxyModels/DeepFilterProxyModel.h"
#include "FileSystem/OsFileSystemModels/AdvancedFileSystemModel.h"
#include "QAdvancedItemDelegate.h"

#include <QFileSystemModel>
#include <QAdvancedTreeView.h>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "Controls/QuestionDialog.h"

struct COpenLevelDialog::Implementation
{
	Implementation(COpenLevelDialog* pDialog)
		: m_pDialog(pDialog)
		, m_bIsFirstPaint(true)
	{
		m_models.Setup(pDialog);
		m_ui.Setup(m_models, pDialog);
		ConnectApi();
	}

	struct Models
	{
		void Setup(QObject* parent)
		{
			const auto levelDir = LevelFileUtils::GetUserBasePath();

			pFileSystem = new CAdvancedFileSystemModel(parent);
			pFileSystem->setRootPath(levelDir); // monitor changes in this folder
			pFileSystem->setFilter(QDir::AllDirs | QDir::Dirs | QDir::NoDotAndDotDot);

			pLevelFilter = new CLevelFilterModel(parent);
			pLevelFilter->setSourceModel(pFileSystem);

			auto filterBehavior = QDeepFilterProxyModel::BehaviorFlags(QDeepFilterProxyModel::AcceptIfChildMatches | QDeepFilterProxyModel::AcceptIfParentMatches);
			pDeepFilter = new QDeepFilterProxyModel(filterBehavior, parent);
			pDeepFilter->setSourceModel(pLevelFilter);

			auto levelDirFileSystemIndex = pFileSystem->index(levelDir);
			auto levelDirLevelFilterIndex = pLevelFilter->mapFromSource(levelDirFileSystemIndex);
			pDeepFilter->SetSourceRootIndex(levelDirLevelFilterIndex);
		}

		QModelIndex GetDeepFilterIndexFromPath(const QString& path) const
		{
			auto fileIndex = pFileSystem->index(path);
			auto levelIndex = pLevelFilter->mapFromSource(fileIndex);
			auto deepFilterIndex = pDeepFilter->mapFromSource(levelIndex);
			return deepFilterIndex;
		}

		CAdvancedFileSystemModel* pFileSystem;
		CLevelFilterModel*       pLevelFilter;
		QDeepFilterProxyModel*   pDeepFilter;
	};

	struct Ui
	{
		void Setup(const Models& models, QWidget* parent)
		{
			parent->setWindowTitle(tr("Open Level"));

			// Treeview should expand level folders on double click
			// which is the default behaviour of QTreeView (expandsOnDoubleClick == true)
			auto pTreeView = new QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), parent);
			// Use QAdvancedItemDelegate so we can support custom tinting icons on selection
			pTreeView->setItemDelegate(new QAdvancedItemDelegate(pTreeView));
			pTreeView->setModel(models.pDeepFilter);
			const auto levelDir = LevelFileUtils::GetUserBasePath();
			auto levelDirDeepFilterIndex = models.GetDeepFilterIndexFromPath(levelDir);
			pTreeView->setRootIndex(levelDirDeepFilterIndex);
			for (int i = 1; i < models.pDeepFilter->columnCount(); ++i)
			{
				pTreeView->hideColumn(i);
			}

			auto pActionOpenButton = new QPushButton(tr("Open"), parent);
			auto pActionCancelButton = new QPushButton(tr("Cancel"), parent);

			auto pButtonLayout = new QHBoxLayout();
			pButtonLayout->setContentsMargins(1, 1, 1, 1);
			pButtonLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
			pButtonLayout->addWidget(pActionOpenButton);
			pButtonLayout->addWidget(pActionCancelButton);

			auto pFilterBox = new QSearchBox(parent);
			pFilterBox->SetModel(models.pDeepFilter);
			pFilterBox->EnableContinuousSearch(true);
			pFilterBox->SetAutoExpandOnSearch(pTreeView);

			// Currently no line edit input for setting the level is implemented. The
			// tree view must be used to open a level.
			//const auto lblLevelName = new QLabel(tr("Level Name (e.g. MyFolder/MyName)"), this);
			//const auto leLevelName = new QLineEdit(this);

			auto pLayout = new QVBoxLayout(parent);
			pLayout->setContentsMargins(1, 1, 1, 1);
			pLayout->addWidget(pFilterBox);
			pLayout->addWidget(pTreeView);
			//layout->addWidget(lblLevelName);
			//layout->addWidget(leLevelName);
			pLayout->addLayout(pButtonLayout);

			m_pTreeView = pTreeView;
			m_pActionOpenButton = pActionOpenButton;
			m_pActionCancelButton = pActionCancelButton;
		}

		QAdvancedTreeView* m_pTreeView;
		QPushButton*       m_pActionOpenButton;
		QPushButton*       m_pActionCancelButton;
	};

	void ConnectApi()
	{
		connect(m_ui.m_pActionCancelButton, &QPushButton::clicked, m_pDialog, &COpenLevelDialog::reject);
		connect(m_ui.m_pActionOpenButton, &QPushButton::clicked, [this]
		{
			const auto currentIndex = m_ui.m_pTreeView->currentIndex();
			OpenIndex(currentIndex);
		});
		connect(m_ui.m_pTreeView, &QTreeView::doubleClicked, [this](const QModelIndex& index)
		{
			// Source of pDeepFilter is pLevelFilter
			const auto levelIndex = m_models.pDeepFilter->mapToSource(index);
			if (m_models.pLevelFilter->IsIndexLevel(levelIndex))
			{
			  OpenIndex(index);
			}
		});
	}

	void OpenIndex(const QModelIndex& index)
	{
		if (!index.isValid())
		{
			return;
		}
		const auto levelPath = index.data(QFileSystemModel::FilePathRole).toString();
		const auto levelFile = LevelFileUtils::FindLevelFile(levelPath);
		// No checking of file extension required because FindLevelFile
		// filters valid level extensions
		if (!QFileInfo::exists(levelFile))
		{
			QString ext(CLevelType::GetFileExtensionStatic());
			CQuestionDialog::SCritical(tr("Error"), tr("Could not locate a valid level file (*.%1)").arg(ext));
			return;
		}
		m_absoluteLevelFile = levelFile;
		m_pDialog->accept();
	}

	void SelectLevelFile(const QString& levelName)
	{
		auto lastLoadedLevelAbsolutePath = QFileInfo(LevelFileUtils::ConvertEngineToAbsolutePath(levelName)).absolutePath();
		auto levelIndex = m_models.GetDeepFilterIndexFromPath(lastLoadedLevelAbsolutePath);
		m_ui.m_pTreeView->setCurrentIndex(levelIndex);

		connect(
		  m_pDialog,
		  &COpenLevelDialog::AfterFirstPaint,
		  m_pDialog,
		  [this]
		{
			m_ui.m_pTreeView->scrollTo(m_ui.m_pTreeView->currentIndex(), QTreeView::PositionAtCenter);
		}, Qt::QueuedConnection);
	}

	COpenLevelDialog* m_pDialog;
	Models            m_models;
	Ui                m_ui;

	QString           m_absoluteLevelFile;
	bool              m_bIsFirstPaint;
};

COpenLevelDialog::COpenLevelDialog()
	: CEditorDialog(QStringLiteral("OpenLevelDialog"))
	, p(new Implementation(this))
{
}

COpenLevelDialog::~COpenLevelDialog()
{
}

QString COpenLevelDialog::GetAcceptedLevelFile() const
{
	auto absoluteFile = p->m_absoluteLevelFile;
	auto engineLevelFile = LevelFileUtils::ConvertAbsoluteToEnginePath(absoluteFile);
	return engineLevelFile;
}

QString COpenLevelDialog::GetAcceptedUserLevelFile() const
{
	auto absoluteFile = p->m_absoluteLevelFile;
	auto userLevelFile = LevelFileUtils::ConvertAbsoluteToUserPath(absoluteFile);
	return userLevelFile;
}

QString COpenLevelDialog::GelAcceptedAbsoluteLevelFile() const
{
	return p->m_absoluteLevelFile;
}

void COpenLevelDialog::SelectLevelFile(const QString& levelFile)
{
	p->SelectLevelFile(levelFile);
}

void COpenLevelDialog::paintEvent(QPaintEvent* event)
{
	QDialog::paintEvent(event);
	if (p->m_bIsFirstPaint)
	{
		p->m_bIsFirstPaint = false;
		AfterFirstPaint();
	}
}

