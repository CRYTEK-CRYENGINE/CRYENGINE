// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "SaveLevelDialog.h"

#include "LevelFilterModel.h"
#include "LevelFileUtils.h"

#include <CryString/StringUtils.h>
#include "Util/FileUtil.h"
#include "FileSystem/OsFileSystemModels/AdvancedFileSystemModel.h"

#include <QAdvancedTreeView.h>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "Controls/QuestionDialog.h"

struct CSaveLevelDialog::Implementation
{
	Implementation(CSaveLevelDialog* pDialog, const QString& title)
		: m_pDialog(pDialog)
		, m_bIsFirstPaint(true)
	{
		m_models.Setup(pDialog);
		m_ui.Setup(m_models, pDialog, title);
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
		}

		QModelIndex GetLevelIndexFromPath(const QString& path) const
		{
			auto fileIndex = pFileSystem->index(path);
			auto levelIndex = pLevelFilter->mapFromSource(fileIndex);
			return levelIndex;
		}

		CAdvancedFileSystemModel* pFileSystem;
		CLevelFilterModel*        pLevelFilter;
	};

	struct Ui
	{
		void Setup(const Models& models, QWidget* parent, const QString& title)
		{
			parent->setWindowTitle(title);

			auto levelDir = LevelFileUtils::GetUserBasePath();
			// Attempt to make the path to the level dir in case it doesn't exist. mkpath will only return false if
			// the path didn't exist and was also unable to create it.
			QDir dir(levelDir);
			if (!dir.mkpath(levelDir))
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "No levels folder found in current project");

			auto pTreeView = new QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), parent);
			pTreeView->setModel(models.pLevelFilter);
			pTreeView->setRootIndex(models.GetLevelIndexFromPath(levelDir));

			// hiding all columns except for "name", like in the "Open Level" dialog
			for (int i = 1; i < models.pLevelFilter->columnCount(); ++i)
			{
				pTreeView->hideColumn(i);
			}

			auto pNameLabel = new QLabel(tr("Level Name (e.g. MyFolder/MyName)"), parent);
			auto pNameEdit = new QLineEdit(parent);

			auto pActionSaveButton = new QPushButton(tr("Save"), parent);
			auto pActionCancelButton = new QPushButton(tr("Cancel"), parent);

			auto pButtonLayout = new QHBoxLayout();
			pButtonLayout->setContentsMargins(0, 5, 0, 5);
			pButtonLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
			pButtonLayout->addWidget(pActionSaveButton);
			pButtonLayout->addWidget(pActionCancelButton);

			auto pLayout = new QVBoxLayout(parent);
			pLayout->setContentsMargins(5, 5, 5, 5);
			pLayout->addWidget(pTreeView);
			pLayout->addWidget(pNameLabel);
			pLayout->addWidget(pNameEdit);
			pLayout->addLayout(pButtonLayout);

			connect(
			  pTreeView->selectionModel(),
			  &QItemSelectionModel::currentRowChanged,
			  [pNameEdit](const QModelIndex& index)
			{
				auto path = index.data(QFileSystemModel::FilePathRole).toString();
				pNameEdit->setText(LevelFileUtils::ConvertAbsoluteToUserPath(path));
			});

			m_pTreeView = pTreeView;
			m_pNameEdit = pNameEdit;
			m_pActionSaveButton = pActionSaveButton;
			m_pActionCancelButton = pActionCancelButton;
		}

		QAdvancedTreeView* m_pTreeView;
		QLineEdit*         m_pNameEdit;
		QPushButton*       m_pActionSaveButton;
		QPushButton*       m_pActionCancelButton;
	};

	void ConnectApi()
	{
		connect(m_ui.m_pActionCancelButton, &QPushButton::clicked, m_pDialog, &CSaveLevelDialog::reject);
		connect(m_ui.m_pActionSaveButton, &QPushButton::clicked, [this]
		{
			auto userPath = m_ui.m_pNameEdit->text();
			if (!CheckUserLevelPath(userPath))
			{
			  return;
			}
			auto absolutePath = LevelFileUtils::ConvertUserToAbsolutePath(userPath);
			auto absoluteFile = LevelFileUtils::GetSaveLevelFile(absolutePath);
			m_absoluteLevelFile = absoluteFile;
			m_pDialog->accept();
		});
	}

	void SelectLevelFile(const QString& levelName)
	{
		auto lastLoadedLevelAbsolutePath = QFileInfo(LevelFileUtils::ConvertEngineToAbsolutePath(levelName)).absolutePath();
		auto levelIndex = m_models.GetLevelIndexFromPath(lastLoadedLevelAbsolutePath);
		m_ui.m_pTreeView->setCurrentIndex(levelIndex);
		if (m_bIsFirstPaint)
		{
			connect(
			  m_pDialog,
			  &CSaveLevelDialog::AfterFirstPaint,
			  m_pDialog,
			  [this]
			{
				m_ui.m_pTreeView->scrollTo(m_ui.m_pTreeView->currentIndex(), QTreeView::PositionAtCenter);
			}, Qt::QueuedConnection);
		}
	}

	bool CheckUserLevelPath(const QString& userPath)
	{
		const auto filename = PathUtil::GetFileName(userPath.toStdString().c_str());
		if (!CryStringUtils::IsValidFileName(filename.c_str()))
		{
			ShowUserError(tr("Please enter a valid level name (standard English alphanumeric characters only)"));
			return false;
		}
		if (!LevelFileUtils::IsValidLevelPath(userPath))
		{
			ShowUserError(tr("Please enter a valid level location."));
			return false;
		}
		const auto cleanedPath = QDir::cleanPath(userPath);
		const auto absolutePath = LevelFileUtils::ConvertUserToAbsolutePath(cleanedPath);
		const auto enginePath = LevelFileUtils::ConvertAbsoluteToEnginePath(absolutePath);

		if (CFileUtil::FileExists(enginePath.toStdString().c_str()))
		{
			ShowUserError(tr("A file with that name already exists"));
			return false;
		}
		if (LevelFileUtils::IsAnyParentPathLevel(cleanedPath))
		{
			ShowUserError(tr("You cannot save levels inside levels."));
			return false;
		}
		if (LevelFileUtils::IsAnySubFolderLevel(absolutePath))
		{
			ShowUserError(tr("You cannot save a level in a folder with sub folders that contain levels."));
			return false;
		}
		if (LevelFileUtils::IsPathToLevel(absolutePath))
		{
			const auto message = tr("Do you really want to overwrite '%1'?").arg(userPath);

			if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(tr("Overwrite existing Level?"), message))
			{
				return false;
			}
		}
		return true;
	}

	void ShowUserError(const QString& message)
	{
		CQuestionDialog::SCritical(tr("Error"), message);
	}

	CSaveLevelDialog* m_pDialog;
	Models            m_models;
	Ui                m_ui;

	QString           m_absoluteLevelFile;
	bool              m_bIsFirstPaint;
};

CSaveLevelDialog::CSaveLevelDialog(const QString& title)
	: CEditorDialog(QStringLiteral("SaveLevelDialog"))
	, p(new Implementation(this, title))
{
}

CSaveLevelDialog::~CSaveLevelDialog()
{
}

QString CSaveLevelDialog::GetAcceptedLevelFile() const
{
	auto absoluteFile = p->m_absoluteLevelFile;
	auto engineLevelFile = LevelFileUtils::ConvertAbsoluteToEnginePath(absoluteFile);
	return engineLevelFile;
}

QString CSaveLevelDialog::GetAcceptedUserLevelFile() const
{
	auto absoluteFile = p->m_absoluteLevelFile;
	auto userLevelFile = LevelFileUtils::ConvertAbsoluteToUserPath(absoluteFile);
	return userLevelFile;
}

QString CSaveLevelDialog::GelAcceptedAbsoluteLevelFile() const
{
	return p->m_absoluteLevelFile;
}

void CSaveLevelDialog::SelectLevelFile(const QString& levelName)
{
	p->SelectLevelFile(levelName);
}

void CSaveLevelDialog::paintEvent(QPaintEvent* event)
{
	QDialog::paintEvent(event);
	if (p->m_bIsFirstPaint)
	{
		p->m_bIsFirstPaint = false;
		AfterFirstPaint();
	}
}

