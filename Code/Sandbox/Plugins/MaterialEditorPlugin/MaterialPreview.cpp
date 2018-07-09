// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialPreview.h"

#include "MaterialEditor.h"
#include "QT/Widgets/QPreviewWidget.h"

#include "AssetSystem/AssetManager.h"
#include "AssetSystem/Browser/AssetBrowserDialog.h"

#include "DragDrop.h"
#include "FilePathUtil.h"

#include <QToolBar>
#include <QEvent.h>

CMaterialPreviewWidget::CMaterialPreviewWidget(CMaterialEditor* pMatEd)
	: m_pMatEd(pMatEd)
{
	m_pMatEd->signalMaterialForEditChanged.Connect(this, &CMaterialPreviewWidget::OnMaterialChanged);

	//TODO: do we need configuration of background color/texture here ?

	auto toolbar = new QToolBar();
	toolbar->addAction(CryIcon("icons:material_editor/sphere.ico"), tr("Sphere Preview"), [this]() { SetPreviewModel("%EDITOR%/Objects/MtlSphere.cgf"); });
	toolbar->addAction(CryIcon("icons:material_editor/cube.ico"), tr("Cube Preview"), [this]() { SetPreviewModel("%EDITOR%/Objects/MtlBox.cgf"); });
	toolbar->addAction(CryIcon("icons:material_editor/plane.ico"), tr("Plane Preview"), [this]() { SetPreviewModel("%EDITOR%/Objects/MtlPlane.cgf"); });
	toolbar->addAction(CryIcon("icons:material_editor/teapot.ico"), tr("Teapot Preview"), [this]() { SetPreviewModel("%EDITOR%/Objects/MtlTeapot.cgf"); });
	toolbar->addAction(CryIcon("icons:General/Search.ico"), tr("Set Custom Preview Model..."), this, &CMaterialPreviewWidget::OnPickPreviewModel);

	//Configure preview widget
	m_pPreviewWidget = new QPreviewWidget();
	m_pPreviewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pPreviewWidget->SetClearColor(QColor(0,0,0));
	m_pPreviewWidget->SetGrid(false);
	m_pPreviewWidget->SetAxis(false);
	m_pPreviewWidget->EnableMaterialPrecaching(true);
	m_pPreviewWidget->LoadFile("%EDITOR%/Objects/mtlsphere.cgf");
	m_pPreviewWidget->SetCameraLookAt(1.6f, Vec3(0.1f, -1.0f, -0.1f));
	m_pPreviewWidget->EnableUpdate(true);
	m_pPreviewWidget->setVisible(false);

	auto layout = new QVBoxLayout();
	layout->addWidget(toolbar);
	layout->addWidget(m_pPreviewWidget);
	layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	setLayout(layout);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &CMaterialPreviewWidget::customContextMenuRequested, this, &CMaterialPreviewWidget::OnContextMenu);

	setAcceptDrops(true);
}

CMaterialPreviewWidget::~CMaterialPreviewWidget()
{
	m_pMatEd->signalMaterialForEditChanged.DisconnectObject(this);
}

void CMaterialPreviewWidget::OnContextMenu()
{
	if (!m_pPreviewWidget->isVisible())
		return;

	if (!m_pPreviewWidget->underMouse())
		return;

	auto menu = new QMenu();

	auto action = menu->addAction("Reset Camera");
	connect(action, &QAction::triggered, [this]() { m_pPreviewWidget->FitToScreen(); });

	action = menu->addAction("Set Custom Preview Model...");
	connect(action, &QAction::triggered, [this]() { OnPickPreviewModel(); });

	menu->popup(QCursor::pos());
}

void CMaterialPreviewWidget::SetPreviewModel(const char* model)
{
	//TODO : save loaded file parameters in personalization!
	m_pPreviewWidget->LoadFile(model);
}

void CMaterialPreviewWidget::OnPickPreviewModel()
{
	CAssetBrowserDialog dialog({ "Mesh" }, CAssetBrowserDialog::Mode::OpenSingleAsset);

	const auto& loadedFile = m_pPreviewWidget->GetLoadedFile();
	if (!loadedFile.isEmpty())
	{
		CAssetManager* const pManager = CAssetManager::GetInstance();

		const CAsset* pAsset = pManager->FindAssetForFile(loadedFile.toStdString().c_str());
		if (pAsset)
		{
			dialog.SelectAsset(*pAsset);
		}
	}

	if (dialog.Execute())
	{
		if (CAsset* pSelectedAsset = dialog.GetSelectedAsset())
		{
			SetPreviewModel(pSelectedAsset->GetFile(0));
		}
	}
}

void CMaterialPreviewWidget::OnMaterialChanged(CMaterial* pEditorMaterial)
{
	if (pEditorMaterial)
	{
		m_pPreviewWidget->SetMaterial(pEditorMaterial);
		m_pPreviewWidget->setVisible(true);
		m_pPreviewWidget->FitToScreen();
	}
	else
	{
		m_pPreviewWidget->setVisible(false);
	}
}

void CMaterialPreviewWidget::dragEnterEvent(QDragEnterEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData("Assets"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("Assets");
		QDataStream stream(byteArray);

		QVector<quintptr> tmp;
		stream >> tmp;

		QVector<CAsset*> assets = *reinterpret_cast<QVector<CAsset*>*>(&tmp);
			
		if (assets.size() == 1 && strcmp(assets[0]->GetType()->GetTypeName(), "Mesh") == 0)
		{
			CDragDropData::ShowDragText(this, tr("Set Custom Preview Model"));
			pEvent->acceptProposedAction();
			return;
		}
	}
	
	if (pDragDropData->HasCustomData("EngineFilePaths"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);

		QStringList engineFilePaths;
		stream >> engineFilePaths;

		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (engineFilePaths.size() == 1 && QFileInfo(engineFilePaths[0]).suffix() == meshType->GetFileExtension())
		{
			CDragDropData::ShowDragText(this, tr("Set Custom Preview Model"));
			pEvent->acceptProposedAction();
			return;
		}
	}
	
	if (pDragDropData->HasFilePaths())
	{
		const auto filePaths = pDragDropData->GetFilePaths();
		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (filePaths.size() == 1 && QFileInfo(filePaths[0]).suffix() == meshType->GetFileExtension())
		{
			CDragDropData::ShowDragText(this, tr("Set Custom Preview Model"));
			pEvent->acceptProposedAction();
			return;
		}
	}
}

void CMaterialPreviewWidget::dropEvent(QDropEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData("Assets"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("Assets");
		QDataStream stream(byteArray);

		QVector<quintptr> tmp;
		stream >> tmp;

		QVector<CAsset*> assets = *reinterpret_cast<QVector<CAsset*>*>(&tmp);

		if (assets.size() == 1 && strcmp(assets[0]->GetType()->GetTypeName(), "Mesh") == 0)
		{
			SetPreviewModel(assets[0]->GetFile(0));
			pEvent->acceptProposedAction();
			return;
		}
	}

	if (pDragDropData->HasCustomData("EngineFilePaths"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);

		QStringList engineFilePaths;
		stream >> engineFilePaths;

		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (engineFilePaths.size() == 1 && QFileInfo(engineFilePaths[0]).suffix() == meshType->GetFileExtension())
		{
			SetPreviewModel(engineFilePaths[0].toStdString().c_str());
			pEvent->acceptProposedAction();
			return;
		}
	}

	if (pDragDropData->HasFilePaths())
	{
		const auto filePaths = pDragDropData->GetFilePaths();
		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (filePaths.size() == 1 && QFileInfo(filePaths[0]).suffix() == meshType->GetFileExtension())
		{
			SetPreviewModel(PathUtil::ToGamePath(filePaths[0]).toStdString().c_str());
			pEvent->acceptProposedAction();
			return;
		}
	}
}

