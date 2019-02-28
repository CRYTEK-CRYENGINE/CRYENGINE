// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Plugin.h"
#include "GraphView.h"
#include "GraphViewModel.h"
#include "NodeGraph\AbstractNodeGraphViewModel.h"
#include "NodeGraph\NodeGraphViewBackground.h"

#include <FileDialogs/ExtensionFilter.h>
#include <FileDialogs/SystemFileDialog.h>

namespace Private_GraphView
{

static const QString s_initialFilePropertyName = QStringLiteral("initialFile");

static void ImportAsset(const CAssetNodeBase& assetNode)
{
	CRY_ASSERT(assetNode.CanBeImported());

	const std::vector<string> extensions = assetNode.GetSourceFileExtensions();

	ExtensionFilterVector filters;
	QStringList extensionsList;

	for (const string& ext : extensions)
	{
		const QString extension = QtUtil::ToQString(ext);
		extensionsList << extension;
		filters << CExtensionFilter(QObject::tr("%1 files").arg(extension.toUpper()), extension);
	}
	if (!filters.isEmpty())
	{
		filters.prepend(CExtensionFilter(QObject::tr("All supported file types (%1)").arg(extensionsList.join(L',')), extensionsList));
	}

	const QString initialFilePath = CDependencyGraph::GetInstance()->GetPersonalizationProperty(s_initialFilePropertyName).toString();

	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = QFileInfo(initialFilePath).absolutePath();
	runParams.initialFile = initialFilePath;
	runParams.title = QObject::tr("Select file to import");
	runParams.extensionFilters << filters;

	const string sourceFilepath = QtUtil::ToString(CSystemFileDialog::RunImportFile(runParams));
	assetNode.ImportAsset(sourceFilepath);

	CDependencyGraph::GetInstance()->SetPersonalizationProperty(s_initialFilePropertyName, QtUtil::ToQString(sourceFilepath));
}

}

CGraphView::CGraphView(CryGraphEditor::CNodeGraphViewModel* p)
	: CryGraphEditor::CNodeGraphView()
{
	SetModel(p);

	m_pSearchPopupContent = new CDictionaryWidget();
	m_pSearchPopup.reset(new QPopupWidget("AssetSearchPopup", m_pSearchPopupContent));

	QObject::connect(m_pSearchPopupContent, &CDictionaryWidget::OnEntryClicked, this, &CGraphView::OnContextMenuEntryClicked);
}

bool CGraphView::PopulateNodeContextMenu(CryGraphEditor::CAbstractNodeItem& node, QMenu& menu)
{
	const CAssetNodeBase* pAssetNode = node.Cast<CAssetNodeBase>();

	if (pAssetNode->GetAsset())
	{
		QAction* pAction = menu.addAction(QObject::tr("Show in Asset Browser"));
		QObject::connect(pAction, &QAction::triggered, this, [pAssetNode]()
		{
			GetIEditor()->ExecuteCommand("asset.show_in_browser '%s'", pAssetNode->GetPath().c_str());
		});
	}

	if (pAssetNode->CanBeEdited())
	{
		QAction* pAction = menu.addAction(QObject::tr("Edit Asset"));
		QObject::connect(pAction, &QAction::triggered, this, [pAssetNode]()
		{
			pAssetNode->EditAsset();
		});
	}
	else if (pAssetNode->CanBeCreated())
	{
		QAction* pAction = menu.addAction(QObject::tr("Create Asset"));
		QObject::connect(pAction, &QAction::triggered, this, [pAssetNode]()
		{
			pAssetNode->CreateAsset();
		});
	}
	else if (pAssetNode->CanBeImported())
	{
		QAction* pAction = menu.addAction(QObject::tr("Import Asset"));
		QObject::connect(pAction, &QAction::triggered, this, [pAssetNode, this]()
		{
			Private_GraphView::ImportAsset(*pAssetNode);
		});
	}

	return true;
}

void CGraphView::ShowGraphContextMenu(QPointF screenPos)
{
	CryGraphEditor::CNodeGraphViewModel* pModel = GetModel();
	if (!pModel)
	{
		return;
	}

	const QPoint parent = mapFromGlobal(QPoint(screenPos.x(), screenPos.y()));
	const QPointF sceenPos = mapToScene(parent);
	const QRectF viewRect = mapToScene(viewport()->geometry()).boundingRect();
	if (viewRect.contains(sceenPos))
	{
		CAbstractDictionary* pAvailableNodesDictionary = pModel->GetRuntimeContext().GetAvailableNodesDictionary();
		if (pAvailableNodesDictionary)
		{
			m_pSearchPopupContent->AddDictionary(*pAvailableNodesDictionary);
			m_pSearchPopup->ShowAt(QPoint(screenPos.x(), screenPos.y()));
		}
	}
}

void CGraphView::OnContextMenuEntryClicked(CAbstractDictionaryEntry& entry)
{
	CryGraphEditor::CAbstractNodeItem* pAssetNode = GetModel()->GetNodeItemById(entry.GetIdentifier());
	MAKE_SURE(pAssetNode, return );
	CryGraphEditor::CNodeWidget* pWidget = GetNodeWidget(*pAssetNode);
	if (pWidget)
	{
		SelectWidget(*pWidget);
		ensureVisible(pWidget);
		m_pSearchPopup->hide();
	}
}

CAssetWidget::CAssetWidget(CAssetNodeBase& item, CryGraphEditor::CNodeGraphView& view)
	: CryGraphEditor::CNodeWidget(item, view)
{
}

void CAssetWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* pEvent)
{
	const CAssetNodeBase* pAssetNode = GetItem().Cast<CAssetNodeBase>();

	if (pAssetNode->CanBeCreated())
	{
		if (CQuestionDialog::SQuestion(tr("Create Asset"), tr("Do you want to create asset?")) == QDialogButtonBox::Yes)
		{
			pAssetNode->CreateAsset();
		}
	}
	else if (pAssetNode->CanBeImported())
	{
		if (CQuestionDialog::SQuestion(tr("Import Asset"), tr("Do you want to import asset?")) == QDialogButtonBox::Yes)
		{
			Private_GraphView::ImportAsset(*pAssetNode);
		}
	}

	CryGraphEditor::CNodeWidget::mouseDoubleClickEvent(pEvent);
}

