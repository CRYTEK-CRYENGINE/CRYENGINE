// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <Serialization/QPropertyTree/QPropertyTree.h>
#include "Expected.h"
#include "Serialization.h"
#include "DisplayParametersPanel.h"
#include <QBoxLayout>
#include <QViewport.h>
#include <CryAnimation/ICryAnimation.h>
#include "CharacterDocument.h"

namespace CharacterTool
{

DisplayParametersPanel::DisplayParametersPanel(QWidget* parent, CharacterDocument* document, Serialization::SContextLink* context)
	: QWidget(parent)
	, m_displayOptions(document->GetDisplayOptions())
	, m_document(document)
	, m_changingDisplayOptions(false)
{
	EXPECTED(connect(document, &CharacterDocument::SignalDisplayOptionsChanged, this, &DisplayParametersPanel::OnDisplayOptionsUpdated));
	EXPECTED(connect(document, &CharacterDocument::SignalCharacterLoaded, this, &DisplayParametersPanel::OnDisplayOptionsUpdated));

	QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
	layout->setMargin(0);
	layout->setSpacing(0);

	m_propertyTree = new QPropertyTree(this);
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	treeStyle.groupRectangle = false;
	m_propertyTree->setTreeStyle(treeStyle);
	m_propertyTree->setSizeHint(QSize(220, 100));
	m_propertyTree->setExpandLevels(0);
	m_propertyTree->setSliderUpdateDelay(5);
	m_propertyTree->setAutoRevert(false);
	m_propertyTree->setArchiveContext(context);
	m_propertyTree->setValueColumnWidth(0.6f);
	m_propertyTree->attach(Serialization::SStruct(*m_displayOptions));
	EXPECTED(connect(m_propertyTree, &QPropertyTree::signalChanged, this, &DisplayParametersPanel::OnPropertyTreeChanged));
	EXPECTED(connect(m_propertyTree, &QPropertyTree::signalContinuousChange, this, &DisplayParametersPanel::OnPropertyTreeChanged));
	layout->addWidget(m_propertyTree, 1);
}

DisplayParametersPanel::~DisplayParametersPanel()
{

}

void DisplayParametersPanel::Serialize(Serialization::IArchive& ar)
{
	ar(*m_propertyTree, "propertyTree");
}

void DisplayParametersPanel::OnDisplayOptionsUpdated()
{
	if (m_changingDisplayOptions)
		return;

	m_propertyTree->revertNoninterrupting();
}

void DisplayParametersPanel::OnPropertyTreeChanged()
{
	m_changingDisplayOptions = true;
	if (m_document)
		m_document->DisplayOptionsChanged();
	m_changingDisplayOptions = false;
}

}

