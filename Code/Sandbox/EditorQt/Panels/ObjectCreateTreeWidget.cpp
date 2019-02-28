// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "ObjectCreateTreeWidget.h"

#include "Material/MaterialManager.h"
#include "QT/Widgets/QPreviewWidget.h"
#include "EditorFramework/PersonalizationManager.h"

#include "CryIcon.h"

#include <QToolButton>
#include <QSplitter>
#include <QAbstractItemView>
#include <QHBoxLayout>

CObjectCreateTreeWidget::CObjectCreateTreeWidget(CObjectClassDesc* pClassDesc, const char* szRegExp, QWidget* pParent)
	: QObjectTreeWidget(pParent, szRegExp)
	, m_pClassDesc(pClassDesc)
{
	m_pShowPreviewButton = new QToolButton();
	m_pShowPreviewButton->setCheckable(true);
	m_pShowPreviewButton->setText(tr("Show Preview"));
	m_pShowPreviewButton->setIconSize(QSize(16, 16));
	m_pShowPreviewButton->setToolTip(tr("Show Preview"));
	m_pShowPreviewButton->setIcon(CryIcon("icons:General/Visibility_False.ico"));

	m_pToolLayout->addWidget(m_pShowPreviewButton);

	m_pPreviewWidget = new QPreviewWidget();
	m_pPreviewWidget->hide(); // initially nothing is selected

	m_pSplitter->addWidget(m_pPreviewWidget);
	m_pSplitter->setStretchFactor(1, 2);

	auto pSelectionModel = m_pTreeView->selectionModel();
	connect(pSelectionModel, &QItemSelectionModel::selectionChanged, [ = ]
	{
		UpdatePreviewWidget();
	});

	connect(m_pShowPreviewButton, &QToolButton::toggled, [ = ]
	{
		UpdatePreviewWidget();
		m_pShowPreviewButton->setIcon(m_pShowPreviewButton->isChecked() ? CryIcon("icons:General/Visibility_True.ico") : CryIcon("icons:General/Visibility_False.ico"));
		GetIEditorImpl()->GetPersonalizationManager()->SetProperty(metaObject()->className(), "showPreview", m_pShowPreviewButton->isChecked());
	});

	bool bShowPreview = true;
	GetIEditorImpl()->GetPersonalizationManager()->GetProperty(metaObject()->className(), "showPreview", bShowPreview);
	m_pShowPreviewButton->setChecked(bShowPreview);
}

void CObjectCreateTreeWidget::UpdatePreviewWidget()
{
	if (!m_pClassDesc)
	{
		return;
	}

	m_pPreviewWidget->hide();
	const auto currentIndex = m_pTreeView->selectionModel()->currentIndex();
	const auto idVariant = currentIndex.data(QObjectTreeView::Id);
	if (idVariant.isValid())
	{
		const auto previewDesc = m_pClassDesc->GetPreviewDesc(idVariant.toString().toLocal8Bit());

		if (m_pShowPreviewButton->isChecked())
		{
			bool bShow = false;
			const QString materialPath(previewDesc.materialFile);
			if (!materialPath.isEmpty())
			{
				m_pPreviewWidget->LoadMaterial(materialPath);
				bShow = true;
			}

			QString path(previewDesc.modelFile);
			if (!path.isEmpty() && path[0] == QChar('/'))
			{
				path = path.mid(1);
			}
			if (!path.isEmpty())
			{
				m_pPreviewWidget->LoadFile(path);
				bShow = true;
			}

			if (previewDesc.pParticleEffect)
			{
				m_pPreviewWidget->LoadParticleEffect(previewDesc.pParticleEffect);
				bShow = true;
			}

			if (bShow)
			{
				m_pPreviewWidget->show();
			}
		}
	}
}

