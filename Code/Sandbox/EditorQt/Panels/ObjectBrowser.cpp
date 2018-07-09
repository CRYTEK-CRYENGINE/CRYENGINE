// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "ObjectBrowser.h"

#include "QT/Widgets/QPreviewWidget.h"

//Qt
#include <QVboxLayout>
#include <QAdvancedTreeView.h>
#include <QHeaderView>
#include <QDrag>
#include <QDragEnterEvent>
#include <QToolButton>
#include <QSplitter>
#include <QFileInfo>
#include <QDir>

//EditorCommon
#include "QSearchBox.h"
#include "EditorFramework/PersonalizationManager.h"
#include "FileDialogs/FileDialogsCommon.h"
#include "FileSystem/FileSystem_Enumerator.h"
#include "FileSystem/FileTreeModel.h"
#include "FileSystem/FileSortProxyModel.h"
#include "FileSystem/FileSystem_FileFilter.h"
#include "IEditorImpl.h"
#include "CryIcon.h"
#include "QAdvancedItemDelegate.h"

namespace Private_ObjectBrowser
{
const static QSet<QString> validGeometryExtensions = []()
{
	QSet<QString> result;
	result << QStringLiteral(CRY_SKEL_FILE_EXT)
	       << QStringLiteral(CRY_SKIN_FILE_EXT)
	       << QStringLiteral(CRY_CHARACTER_DEFINITION_FILE_EXT)
	       << QStringLiteral(CRY_ANIM_GEOMETRY_FILE_EXT)
	       << QStringLiteral(CRY_GEOMETRY_FILE_EXT);
	return result;
} ();

const static QString validParticleEffectExtension("pfx");
}

CObjectBrowser::CObjectBrowser(const char* filters)
	: m_treeView(nullptr)
	, m_previewWidget(nullptr)
	, m_showPreviewButton(nullptr)
	, m_bIsDragTracked(false)
	, m_bHasGeometryFileFilter(false)
	, m_bHasParticleFileFilter(false)
{
	QString filtersString(filters);
	QStringList list = filtersString.split(";", QString::SkipEmptyParts);
	QStringList exts;
	QStringList dirs;
	CorrectSyntax(list, exts, dirs);
	Init(exts, dirs);
}

CObjectBrowser::CObjectBrowser(const QStringList& filters)
	: m_treeView(nullptr)
	, m_previewWidget(nullptr)
	, m_showPreviewButton(nullptr)
	, m_bIsDragTracked(false)
	, m_bHasGeometryFileFilter(false)
	, m_bHasParticleFileFilter(false)
{
	QStringList list(filters);
	QStringList exts;
	QStringList dirs;
	CorrectSyntax(list, exts, dirs);
	Init(exts, dirs);
}

CObjectBrowser::~CObjectBrowser()
{}

void CObjectBrowser::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->source() == m_treeView)
	{
		QDrag* dragObject = m_treeView->findChild<QDrag*>();
		signalOnDragStarted(event, dragObject);

		// We need to connect to the QDrag's destroy to get notified when the drag actually ends
		// this should abort the object create tool
		if (!m_bIsDragTracked)
		{
			m_bIsDragTracked = true;
			connect(dragObject, &QObject::destroyed, [this]()
			{
				singalOnDragEnded();
				m_bIsDragTracked = false;
			});
		}
	}
}

void CObjectBrowser::CorrectSyntax(const QStringList& filters, QStringList& extensions, QStringList& directories)
{
	for (const QString& filter : filters)
	{
		const int dotPosition = filter.lastIndexOf(".");
		if (dotPosition != -1)
		{
			auto ext = filter.mid(dotPosition + 1);
			if (!ext.isEmpty())
				extensions.append(ext);
		}

		const int slashPosition = filter.lastIndexOf("/");
		if (slashPosition != -1)
		{
			auto dir = filter.left(slashPosition);
			dir = QDir::fromNativeSeparators(dir);
			if (!dir.isEmpty() && dir[0] != '/')
			{
				dir = QStringLiteral("/") + dir;
			}
			directories.append(dir.toLower());
		}
	}
}

void CObjectBrowser::Init(const QStringList& extensions, const QStringList& directories)
{
	auto fileFilterFunc = [](const FileSystem::SnapshotPtr&, const FileSystem::FilePtr& file)
	{
		const auto fileName = QFileInfo(file->provider->fullName).baseName();
		QRegularExpressionMatch match;
		const auto index = fileName.lastIndexOf(QRegularExpression(QStringLiteral("_lod[0-9]*"), QRegularExpression::CaseInsensitiveOption), -1, &match);
		return (index < 0) || ((index + match.captured().size()) < fileName.size());
	};

	FileSystem::SFileFilter fileFilter;
	fileFilter.fileExtensions = extensions.toSet();
	fileFilter.directories = directories.toVector();
	fileFilter.fileFilterCallbacks << fileFilterFunc;
	fileFilter.skipEmptyDirectories = true;

	auto enumerator = GetIEditorImpl()->GetFileSystemEnumerator();

	auto fileModel = new CFileTreeModel(*enumerator, fileFilter, this);
	auto fileSortModel = new CFileSortProxyModel(this);

	fileSortModel->setSourceModel(fileModel);

	QSearchBox* searchBox = new QSearchBox();
	auto searchFunction = [=](const QString& input)
	{
		FileSystem::SFileFilter newFilter = fileFilter;
		if (!input.isEmpty())
		{
			newFilter.fileFilterCallbacks <<[=](const FileSystem::SnapshotPtr&, const FileSystem::FilePtr& file)
				  {
				  return file->keyName.contains(input, Qt::CaseInsensitive);
			  };
		}
		fileModel->SetFilter(newFilter);

		if (!input.isEmpty())
		{
			m_treeView->expandAll();
		}
	};
	searchBox->SetSearchFunction(std::function<void(const QString&)>(searchFunction));
	searchBox->EnableContinuousSearch(true);

	std::function<void(QModelIndexList&)> onDragFn = [this, fileModel, fileSortModel](QModelIndexList& indices)
	{
		for (int i = indices.count() - 1; i >= 0; --i)
		{
			QModelIndex& index = indices[i];
			QVariant var = index.data(CFileTreeModel::eRole_EnginePath);
			if (fileModel->hasChildren(fileSortModel->mapToSource(index)) || !var.isValid())
			{
				indices.removeAt(i);
			}
		}
	};
	FileDialogs::FilesTreeView* pTreeView = new FileDialogs::FilesTreeView(fileSortModel);
	pTreeView->setItemDelegate(new QAdvancedItemDelegate(pTreeView));
	pTreeView->onDragFilter.Connect(onDragFn);
	m_treeView = pTreeView;
	m_treeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_treeView->header()->setStretchLastSection(true);
	m_treeView->setDragDropMode(QAbstractItemView::DragOnly);
	m_treeView->setFocusPolicy(Qt::FocusPolicy::NoFocus);
	for (int i = CFileTreeModel::eColumn_Name + 1; i < CFileTreeModel::eColumn_Count; ++i)
	{
		m_treeView->header()->hideSection(i);
	}

	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(1, 1, 1, 1);
	layout->setSpacing(0);
	setLayout(layout);

	const auto fileExtIntersection = fileFilter.fileExtensions & Private_ObjectBrowser::validGeometryExtensions;
	m_bHasGeometryFileFilter = !fileExtIntersection.empty();
	m_bHasParticleFileFilter = fileFilter.fileExtensions.contains(Private_ObjectBrowser::validParticleEffectExtension);
	const auto canShowPreview = m_bHasGeometryFileFilter || m_bHasParticleFileFilter;

	if (canShowPreview)
	{
		m_previewWidget = new QPreviewWidget();
		m_previewWidget->hide(); // initially no file is selected

		m_showPreviewButton = new QToolButton();
		m_showPreviewButton->setCheckable(true);
		m_showPreviewButton->setText(tr("Show Preview"));
		m_showPreviewButton->setIconSize(QSize(16, 16));
		m_showPreviewButton->setToolTip(tr("Show Preview"));
		m_showPreviewButton->setIcon(CryIcon("icons:General/Visibility_False.ico"));

		auto toolLayout = new QHBoxLayout();
		toolLayout->setContentsMargins(0, 0, 0, 0);
		toolLayout->setSpacing(1);
		toolLayout->addWidget(searchBox);
		toolLayout->addWidget(m_showPreviewButton);

		auto splitter = new QSplitter();
		splitter->setOrientation(Qt::Vertical);
		splitter->addWidget(m_treeView);
		splitter->addWidget(m_previewWidget);

		splitter->setStretchFactor(1, 2);

		layout->addLayout(toolLayout);
		layout->addWidget(splitter);

		auto selectionModel = m_treeView->selectionModel();
		connect(selectionModel, &QItemSelectionModel::selectionChanged, [ = ]
		{
			UpdatePreviewWidget();
		});

		connect(m_showPreviewButton, &QToolButton::toggled, [ = ]
		{
			UpdatePreviewWidget();
			m_showPreviewButton->setIcon(m_showPreviewButton->isChecked() ? CryIcon("icons:General/Visibility_True.ico") : CryIcon("icons:General/Visibility_False.ico"));
			GetIEditorImpl()->GetPersonalizationManager()->SetProperty(metaObject()->className(), "showPreview", m_showPreviewButton->isChecked());
		});

		bool bShowPreview = true;
		GetIEditorImpl()->GetPersonalizationManager()->GetProperty(metaObject()->className(), "showPreview", bShowPreview);
		m_showPreviewButton->setChecked(bShowPreview);
	}
	else
	{
		layout->addWidget(searchBox);
		layout->addWidget(m_treeView);
	}

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	connect(m_treeView, &QTreeView::doubleClicked, [this, fileModel, fileSortModel](const QModelIndex& index)
	{
		if (fileModel->hasChildren(fileSortModel->mapToSource(index)))
			return;

		QVariant var = index.data(CFileTreeModel::eRole_EnginePath);
		if (var.isValid())
		{
		  auto path = var.toString();
		  if (!path.isEmpty() && path[0] == QChar('/'))
		  {
		    path = path.mid(1);
		  }
		  auto pathStr = path.toStdString();
		  signalOnDoubleClickFile(pathStr.c_str());
		}
	});

	setAcceptDrops(true);
}

void CObjectBrowser::UpdatePreviewWidget()
{
	if (!m_previewWidget)
	{
		return;
	}
	m_previewWidget->hide();
	const QVariant var = m_treeView->selectionModel()->currentIndex().data(CFileTreeModel::eRole_EnginePath);
	if (var.isValid())
	{
		auto path = var.toString();
		if (!path.isEmpty() && path[0] == QChar('/'))
		{
			path = path.mid(1);
		}
		const QFileInfo fileInfo(path);
		if (m_showPreviewButton->isChecked())
		{
			bool bShow = false;
			if (Private_ObjectBrowser::validGeometryExtensions.contains(fileInfo.suffix()))
			{
				m_previewWidget->LoadFile(path);
				bShow = true;
			}
			else if (Private_ObjectBrowser::validParticleEffectExtension == fileInfo.suffix())
			{
				m_previewWidget->LoadParticleEffect(path);
				bShow = true;
			}
			if (bShow)
			{
				m_previewWidget->show();
			}
		}
	}
}

