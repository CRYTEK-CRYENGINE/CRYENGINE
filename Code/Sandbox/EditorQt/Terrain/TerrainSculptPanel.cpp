// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <QGridLayout>
#include <Serialization/QPropertyTree/QPropertyTree.h>

#include "CryIcon.h"
#include "Terrain/TerrainSculptPanel.h"
#include "QT/Widgets/QEditToolButton.h"
#include "TerrainMoveTool.h"

QTerrainSculptButtons::QTerrainSculptButtons(QWidget* parent)
	: QWidget(parent)
	, m_buttonCount(0)
{
	QGridLayout* localLayout = new QGridLayout();
	setLayout(localLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	localLayout->setAlignment(localLayout, Qt::AlignTop);

	AddTool(RUNTIME_CLASS(CFlattenTool), "Flatten");
	AddTool(RUNTIME_CLASS(CSmoothTool), "Smooth");
	AddTool(RUNTIME_CLASS(CRiseLowerTool), "Raise/Lower");
	AddTool(RUNTIME_CLASS(CTerrainMoveTool), "Move");
	AddTool(RUNTIME_CLASS(CMakeHolesTool), "Make Holes");
	AddTool(RUNTIME_CLASS(CFillHolesTool), "Fill Holes");
}

void QTerrainSculptButtons::AddTool(CRuntimeClass* pRuntimeClass, const char* text)
{
	QString icon = QString("Terrain_%1.ico").arg(text);
	icon.replace(" ", "_");
	icon.replace("/", "_");
	icon = "icons:TerrainEditor/" + icon;

	QEditToolButton* pToolButton = new QEditToolButton(nullptr);
	pToolButton->SetToolClass(pRuntimeClass, nullptr, &mTerrainBrush);
	pToolButton->setText(text);
	pToolButton->setIcon(CryIcon(icon));
	pToolButton->setIconSize(QSize(24, 24));
	pToolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	pToolButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	((QGridLayout*)layout())->addWidget(pToolButton, m_buttonCount / 2, m_buttonCount % 2);
	++m_buttonCount;
}

QTerrainSculptPanel::QTerrainSculptPanel(QWidget* parent)
	: QEditToolPanel(parent)
{
	QVBoxLayout* localLayout = new QVBoxLayout();
	localLayout->setContentsMargins(1, 1, 1, 1);
	setLayout(localLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	localLayout->setAlignment(localLayout, Qt::AlignTop);

	localLayout->addWidget(new QTerrainSculptButtons());
	m_pPropertyTree = new QPropertyTree();
	localLayout->addWidget(m_pPropertyTree);
}

bool QTerrainSculptPanel::CanEditTool(CEditTool* pTool)
{
	if (!pTool)
		return false;

	return pTool->IsKindOf(RUNTIME_CLASS(CTerrainTool)) || pTool->IsKindOf(RUNTIME_CLASS(CTerrainMoveTool));
}

