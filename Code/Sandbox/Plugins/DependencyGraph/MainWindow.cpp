// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainWindow.h"
#include "GraphView.h"
#include "GraphViewModel.h"
#include "Model.h"
#include "BoostPythonMacros.h"
#include <AssetSystem/Browser/AssetBrowserDialog.h>
#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetType.h>
#include <AssetSystem/AssetManager.h>

namespace Private_MainWindow
{

static const char* const s_szEditorName = "Dependency Graph";
REGISTER_VIEWPANE_FACTORY(CMainWindow, s_szEditorName, "Tools", false)

//! Show asset dependencies.
//! \param assetPath file path.
void PyAssetDependencyGraph(const char* szAssetPath)
{
	CAsset* pAsset = stricmp(PathUtil::GetExt(szAssetPath), "cryasset") == 0 
		? GetIEditor()->GetAssetManager()->FindAssetForMetadata(szAssetPath)
		: GetIEditor()->GetAssetManager()->FindAssetForFile(szAssetPath);
	if (pAsset)
	{
		CMainWindow::CreateNewWindow(pAsset);
	}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyAssetDependencyGraph, asset, show_dependency_graph, "Shows asset dependencies as a graph", "asset.show_dependency_graph(str cryasset)");

}

const char* CMainWindow::GetEditorName() const
{
	return Private_MainWindow::s_szEditorName;
}

CMainWindow::CMainWindow(QWidget* pParent)
	: CDockableEditor(pParent)
	, m_pModel(new CModel())
	, m_pGraphViewModel(new CGraphViewModel(m_pModel.get()))
{
	setObjectName(GetEditorName());
	InitMenu();
	RegisterDockingWidgets();

	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setContentsMargins(1, 1, 1, 1);
	SetContent(pMainLayout);
}

void CMainWindow::CreateNewWindow(CAsset* asset)
{
	IPane* pane = GetIEditor()->CreateDockable(Private_MainWindow::s_szEditorName);
	if (pane)
	{
		static_cast<CMainWindow*>(pane)->Open(asset);
	}
}

void CMainWindow::InitMenu()
{
	AddToMenu(CEditor::MenuItems::FileMenu);
	AddToMenu(CEditor::MenuItems::Open);
	AddToMenu(CEditor::MenuItems::RecentFiles);
}

void CMainWindow::RegisterDockingWidgets()
{
	EnableDockingSystem();

	auto createGraphView = [this]()
	{
		CGraphView* const pGraphView = new CGraphView(m_pGraphViewModel.get());

		// One-time scene fitting in the view.
		std::shared_ptr<QMetaObject::Connection> pConnection{ new QMetaObject::Connection };
		*pConnection = QObject::connect(pGraphView, &CGraphView::SignalItemsReloaded, [this, pConnection](auto& view)
		{
			if (!view.GetModel()->GetNodeItemCount())
			{
				return;
			}

			view.FitSceneInView();
			QObject::disconnect(*pConnection);
		});

		return pGraphView;
	};
	RegisterDockableWidget("Graph", createGraphView, false, false);
}

void CMainWindow::CreateDefaultLayout(CDockableContainer* pSender)
{
	CRY_ASSERT(pSender);
	pSender->SpawnWidget("Graph");
}

bool CMainWindow::OnOpen()
{
	const std::vector<CAssetType*>& types = m_pModel->GetSupportedTypes();
	std::vector<string> supportedAssetTypeNames;
	supportedAssetTypeNames.reserve(types.size());
	std::transform(types.begin(), types.end(), std::back_inserter(supportedAssetTypeNames), [](const CAssetType* x)
	{
		return x->GetTypeName();
	});

	CAsset* const pAsset = CAssetBrowserDialog::OpenSingleAssetForTypes(supportedAssetTypeNames);
	return Open(pAsset);
}

bool CMainWindow::OnOpenFile(const QString& path)
{
	CAsset* const pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(path.toStdString().c_str());
	return Open(pAsset);
}

bool CMainWindow::OnClose()
{
	return true;
}

bool CMainWindow::Open(CAsset* pAsset)
{
	if (pAsset)
	{
		if (!OnClose())
		{
			// User cancelled closing of currently opened asset.
			return true;
		}
		AddRecentFile(QString(pAsset->GetMetadataFile()));
		m_pModel->OpenAsset(pAsset);

		setWindowTitle(QString("%1: %2").arg(pAsset->GetType()->GetUiTypeName(), pAsset->GetName()));
	}
	return true;
}

