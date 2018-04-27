// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DesignerEditor.h"
#include "Tools/BaseTool.h"
#include "DesignerPanel.h"
#include "Material/MaterialManager.h"
#include "CubeEditorPanel.h"
#include "Controls/QMenuComboBox.h"

#include <QLabel>
#include <QCheckBox>
#include <QGridLayout>
#include <QPushButton>

namespace Designer
{
struct SizePairs
{
	const char* label;
	float       value;
};

#define SPAIRSIZE 10

SizePairs spairs[] =
{
	{ "0.125", 0.125f },
	{ "0.25",  0.25f  },
	{ "0.5",   0.5f   },
	{ "1.0",   1.0f   },
	{ "2.0",   2.0f   },
	{ "4.0",   4.0f   },
	{ "8.0",   8.0f   },
	{ "16.0",  16.0f  },
	{ "32.0",  32.0f  },
	{ "64.0",  64.0f  },
};

CubeEditorPanel::CubeEditorPanel(CubeEditor* pCubeEditor) : m_pCubeEditor(pCubeEditor)
{
	QGridLayout* pGridLayout = new QGridLayout;

	m_pAddButton = new QPushButton("Add");
	m_pRemoveButton = new QPushButton("Remove");
	m_pPaintButton = new QPushButton("Paint");
	m_pBrushSizeComboBox = new QMenuComboBox;
	m_pMergeSideCheckBox = new QCheckBox("Merge Sides");

	QObject::connect(m_pAddButton, &QPushButton::clicked, [ = ] {
			CubeEditor* tool = static_cast<CubeEditor*>(GetDesigner()->GetCurrentTool());
			tool->SetEditMode(eCubeEditorMode_Add);
	  });

	QObject::connect(m_pRemoveButton, &QPushButton::clicked, [ = ] {
			CubeEditor* tool = static_cast<CubeEditor*>(GetDesigner()->GetCurrentTool());
			tool->SetEditMode(eCubeEditorMode_Remove);
	  });

	QObject::connect(m_pPaintButton, &QPushButton::clicked, [ = ] {
			CubeEditor* tool = static_cast<CubeEditor*>(GetDesigner()->GetCurrentTool());
			tool->SetEditMode(eCubeEditorMode_Paint);
	  });

	QObject::connect(m_pMergeSideCheckBox, &QCheckBox::stateChanged, [=](int state)
		{
			CubeEditor* tool = static_cast<CubeEditor*>(GetDesigner()->GetCurrentTool());
			tool->SetSideMerged(state == Qt::Checked);
	  });

	pGridLayout->addWidget(m_pAddButton, 0, 0, Qt::AlignTop);
	pGridLayout->addWidget(m_pRemoveButton, 0, 1, Qt::AlignTop);
	pGridLayout->addWidget(m_pPaintButton, 0, 2, Qt::AlignTop);
	pGridLayout->addWidget(new QLabel("Brush Size"), 1, 0, Qt::AlignTop);
	pGridLayout->addWidget(m_pBrushSizeComboBox, 1, 1, 1, 2, Qt::AlignTop);
	pGridLayout->addWidget(m_pMergeSideCheckBox, 3, 0, 1, 3, Qt::AlignTop);
	pGridLayout->addWidget(new QLabel("CTRL + Wheel : Change mode (Add/Remove/Paint)"), 4, 0, 1, 3, Qt::AlignTop);
	pGridLayout->addWidget(new QLabel("SHIFT + LMB Drag : Axis Placement"), 5, 0, 1, 3, Qt::AlignTop);
	pGridLayout->addWidget(new QLabel("Paint brush assigns material selected in Groups/UV tab"), 6, 0, 1, 3, Qt::AlignTop);

	setLayout(pGridLayout);

	for (int i = 0; i < SPAIRSIZE; ++i)
	{
		m_pBrushSizeComboBox->AddItem(spairs[i].label, QVariant(spairs[i].value));
	}

	m_pBrushSizeComboBox->signalCurrentIndexChanged.Connect(this, &CubeEditorPanel::UpdateCubeSize);
}

void CubeEditorPanel::Update()
{
	CubeEditor* tool = static_cast<CubeEditor*>(GetDesigner()->GetCurrentTool());

	SelectEditMode(tool->GetEditMode());
	m_pMergeSideCheckBox->setCheckState(tool->IsSideMerged() ? Qt::Checked : Qt::Unchecked);

	float cubeSize = tool->GetCubeSize();
	for (int i = 0; i < SPAIRSIZE; ++i)
	{
		if (fabs(cubeSize - spairs[i].value) < 0.01)
		{
			m_pBrushSizeComboBox->SetChecked(i);
			break;
		}
	}
}

void CubeEditorPanel::UpdateCubeSize(int index)
{
	CubeEditor* tool = static_cast<CubeEditor*>(GetDesigner()->GetCurrentTool());
	tool->SetCubeSize(spairs[index].value);
}

void CubeEditorPanel::SelectEditMode(ECubeEditorMode editMode)
{
	m_pAddButton->setChecked(false);
	m_pPaintButton->setChecked(false);
	m_pRemoveButton->setChecked(false);

	if (editMode == eCubeEditorMode_Remove)
	{
		m_pRemoveButton->setChecked(true);
	}
	else if (editMode == eCubeEditorMode_Paint)
	{
		m_pPaintButton->setChecked(true);
	}
	else
	{
		m_pAddButton->setChecked(true);
	}
}

}

