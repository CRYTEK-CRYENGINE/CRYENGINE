// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <QVBoxLayout>
#include <Serialization/QPropertyTree/QPropertyTree.h>

#include "CryIcon.h"
#include "MinimapPanel.h"
#include "QT/Widgets/QEditToolButton.h"
#include "TerrainMiniMapTool.h"

QMinimapButtons::QMinimapButtons(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* localLayout = new QVBoxLayout();
	setLayout(localLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	localLayout->setAlignment(localLayout, Qt::AlignTop);

	AddTool(RUNTIME_CLASS(CTerrainMiniMapTool), "Mini Map");
}

void QMinimapButtons::AddTool(CRuntimeClass* pRuntimeClass, const char* text)
{
	QString icon = QString("icons:TerrainEditor/Terrain_%1.ico").arg(text);
	icon.replace(" ", "_");

	QEditToolButton* pToolButton = new QEditToolButton(nullptr);
	pToolButton->SetToolClass(pRuntimeClass);
	pToolButton->setText(text);
	pToolButton->setIcon(CryIcon(icon));
	pToolButton->setIconSize(QSize(24, 24));
	pToolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	pToolButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	layout()->addWidget(pToolButton);
}

QMinimapPanel::QMinimapPanel(QWidget* parent)
	: QEditToolPanel(parent)
{
	QVBoxLayout* localLayout = new QVBoxLayout();
	localLayout->setContentsMargins(1, 1, 1, 1);
	setLayout(localLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	localLayout->setAlignment(localLayout, Qt::AlignTop);

	localLayout->addWidget(new QMinimapButtons());
	m_pPropertyTree = new QPropertyTree();
	localLayout->addWidget(m_pPropertyTree);
}

bool QMinimapPanel::CanEditTool(CEditTool* pTool)
{
	if (!pTool)
		return false;

	return pTool->IsKindOf(RUNTIME_CLASS(CTerrainMiniMapTool));
}

