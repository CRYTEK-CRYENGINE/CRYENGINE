// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DesignerDockable.h"
#include <QVBoxLayout>
#include "DesignerPanel.h"
#include "DesignerSubPanel.h"
#include "IEditor.h"
#include "Objects/SelectionGroup.h"
#include "Core/Helper.h"
#include "Tools/ToolCommon.h"
#include "EditorCommon/QtUtil.h"

namespace Designer
{

DesignerDockable::DesignerDockable(QWidget* pParent)
	: CDockableWidget(pParent)
{
	SetupUI();
}

DesignerDockable::~DesignerDockable()
{
};

void DesignerDockable::SetupUI()
{
	DesignerPanel* pPanel = new DesignerPanel(this);
	pPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	DesignerSubPanel* pSubPanel = new DesignerSubPanel(this);
	pSubPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	QVBoxLayout* pScrollAreaLayout = new QVBoxLayout;
	pScrollAreaLayout->setSpacing(0);
	pScrollAreaLayout->setContentsMargins(0, 0, 0, 0);
	pScrollAreaLayout->addWidget(pPanel);
	pScrollAreaLayout->addWidget(pSubPanel);
	pScrollAreaLayout->addStretch(1);

	QScrollArea* pScrollArea = QtUtil::MakeScrollable(pScrollAreaLayout);
	pScrollArea->widget();

	QVBoxLayout* pMainLayout = new QVBoxLayout;
	pMainLayout->setSpacing(0);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(pScrollArea);
	setLayout(pMainLayout);
}

}


