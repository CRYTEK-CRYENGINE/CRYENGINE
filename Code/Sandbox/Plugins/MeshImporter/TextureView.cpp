// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TextureView.h"
#include "ComboBoxDelegate.h"
#include "TextureModel.h"

#include <CryCore/ToolsHelpers/SettingsManagerHelpers.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>

CTextureView::CTextureView(QWidget* pParent)
	: QAdvancedTreeView(QAdvancedTreeView::None, pParent)
	, m_pTextureModel(nullptr)
{
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QTreeView::customContextMenuRequested, this, &CTextureView::CreateContextMenu);
}

void CTextureView::SetModel(CTextureModel* pTextureModel)
{
	m_pTextureModel = pTextureModel;
	setModel(m_pTextureModel);

	m_pComboBoxDelegate.reset(new CComboBoxDelegate(this));
	m_pComboBoxDelegate->SetFillEditorFunction([this](QMenuComboBox* pEditor)
	{
		for (int i = 0; i < CTextureModel::eRcSettings_COUNT; ++i)
		{
		  pEditor->AddItem(CTextureModel::GetDisplayNameOfSettings((CTextureModel::ERcSettings)i));
		}
	});
	setItemDelegateForColumn(CTextureModel::eColumn_RcSettings, m_pComboBoxDelegate.get());

	header()->resizeSection(0, 300);
}

void CTextureView::CreateContextMenu(const QPoint& point)
{
	const QPersistentModelIndex index = indexAt(point);
	if (!index.isValid())
	{
		return;
	}

	CTextureManager::TextureHandle tex = m_pTextureModel->GetTexture(index);

	QMenu* const pMenu = new QMenu(this);
	pMenu->addAction(tr("Convert to TIF"));

	{
		QAction* pAction = pMenu->addAction(tr("Call RC"));
		connect(pAction, &QAction::triggered, [this, tex]()
		{
			// const CTextureManager* const pTextureManager = m_pTextureModel->GetTextureManager();
			// const string workingDir = pTextureManager->GetSourceDirectory();
			// const string filename = PathUtil::Make(workingDir, PathUtil::ReplaceExtension(tex->targetFilePath, "tif"));

			// CResourceCompilerHelper::InvokeResourceCompiler(filename, filename, true, true);
		});
	}

	const QPoint popupLocation = point + QPoint(1, 1); // Otherwise double-right-click immediately executes first option
	pMenu->popup(viewport()->mapToGlobal(popupLocation));
}

