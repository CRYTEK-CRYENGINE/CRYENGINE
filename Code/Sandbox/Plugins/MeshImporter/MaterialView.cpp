// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialView.h"
#include "MaterialModel.h"
#include "MaterialElement.h"
#include "ComboBoxDelegate.h"
#include "FbxScene.h"

#include <QSearchBox.h>

#include <QColorDialog>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QVBoxLayout>

CMaterialView::CMaterialView(CSortedMaterialModel* pModel, QWidget* pParent)
	: QAdvancedTreeView(QAdvancedTreeView::None, pParent), m_pModel(pModel)
{
	setModel(m_pModel);
	setSelectionMode(QAbstractItemView::ExtendedSelection);
	setSortingEnabled(true);

	m_pSubMaterialDelegate.reset(new CComboBoxDelegate(this));
	m_pSubMaterialDelegate->SetFillEditorFunction([pModel](QMenuComboBox* pEditor)
	{
		const FbxTool::CScene* const pScene = pModel->GetScene();

		pEditor->AddItem(tr("Automatic"));
		pEditor->AddItem(tr("Delete"));

		for (int i = 0; i < pScene->GetMaterialCount(); ++i)
		{
			const FbxTool::SMaterial* const pMaterial = pScene->GetMaterialByIndex(i);
			QString text = QString("%1").arg(i);
			const QString name = pModel->GetSubMaterialName(i);
			if (!name.isEmpty())
			{
				text += QString(" (%1)").arg(name);
			}
			pEditor->AddItem(text);
		}
	});
	setItemDelegateForColumn(CMaterialModel::eColumnType_SubMaterial, m_pSubMaterialDelegate.get());

	m_pPhysicalizeDelegate.reset(new CComboBoxDelegate(this));
	m_pPhysicalizeDelegate->SetFillEditorFunction([](QMenuComboBox* pEditor)
	{
		for (int i = 0; i < FbxTool::EMaterialPhysicalizeSetting_COUNT; ++i)
		{
			const char* const szName = FbxTool::MaterialPhysicalizeSettingToString(FbxTool::EMaterialPhysicalizeSetting(i));
			pEditor->AddItem(QObject::tr(szName));
		}
	});
	setItemDelegateForColumn(CMaterialModel::eColumnType_Physicalize, m_pPhysicalizeDelegate.get());

	setEditTriggers(AllEditTriggers);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QTreeView::customContextMenuRequested, this, &CMaterialView::CreateMaterialContextMenu);
}

CMaterialView::~CMaterialView()
{
}

void CMaterialView::SetMaterialContextMenuCallback(const AddMaterialContextMenu& callback)
{
	m_addMaterialContextMenu = callback;
}

void CMaterialView::CreateMaterialContextMenu(const QPoint& point)
{
	const QPersistentModelIndex index = indexAt(point);
	CMaterialElement* const pElement = index.isValid() ? m_pModel->AtIndex(index) : nullptr;
	QMenu* const pMenu = new QMenu(this);

	QAction* const pSelectColor = pMenu->addAction(tr("Select sub-material color"));
	pSelectColor->setEnabled(pElement != nullptr);
	connect(pSelectColor, &QAction::triggered, [=]()
	{
		if (index.isValid())
		{
		  const ColorF& initialColor = m_pModel->AtIndex(index)->GetColor();
		  const QColor color = QColorDialog::getColor(QColor::fromRgbF(initialColor.r, initialColor.g, initialColor.b), this);
		  if (color.isValid())
		  {
		    m_pModel->setData(index, color, CMaterialModel::eItemDataRole_Color);
		  }
		}
	});

	if (m_addMaterialContextMenu)
	{
		m_addMaterialContextMenu(pMenu, pElement);
	}

	const QPoint popupLocation = point + QPoint(1, 1); // Otherwise double-right-click immediately executes first option
	pMenu->popup(viewport()->mapToGlobal(popupLocation));
}

CMaterialViewContainer::CMaterialViewContainer(CSortedMaterialModel* pModel, CMaterialView* pView, QWidget* pParent)
	: QWidget(pParent)
	, m_pView(pView)
	, m_pSearchBox(nullptr)
{
	m_pSearchBox = new QSearchBox(this);
	m_pSearchBox->SetModel(pModel);
	m_pSearchBox->EnableContinuousSearch(true);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->addWidget(m_pSearchBox);
	pLayout->addWidget(m_pView);
	setLayout(pLayout);

	setContentsMargins(0, 0, 0, 0);
	layout()->setContentsMargins(0, 0, 0, 0);
}

CMaterialView* CMaterialViewContainer::GetView()
{
	return m_pView;
}

