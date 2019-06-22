// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ViewportHelperWnd.h"

#include "Serialization/Decorators/EditorActionButton.h"
#include "Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h"
#include "Viewport.h"
#include <QLabel>
#include <QPushButton>

CViewportHelperWnd::CViewportHelperWnd(QWidget* pParent, CViewport* pViewport)
	: CDockableWidgetT<QScrollableBox>{pParent}
	, m_pViewport{pViewport}
{
	m_propertyTree = new QPropertyTreeLegacy(this);
	m_propertyTree->setAutoRevert(false);
	m_propertyTree->setAggregateMouseEvents(false);
	m_propertyTree->setFullRowContainers(true);
	m_propertyTree->setExpandLevels(1);
	m_propertyTree->setUndoEnabled(false);
	m_propertyTree->attach(Serialization::SStruct(*this));
	addWidget(m_propertyTree);

	QLabel* pLabel = new QLabel("* Global setting\nDoes not hide with helpers button", this);
	addWidget(pLabel);

	QPushButton* pResetBtn = new QPushButton("Reset to Defaults", this);
	QObject::connect(pResetBtn, &QPushButton::clicked, this, [=] { OnReset(); });
	addWidget(pResetBtn);
}

void CViewportHelperWnd::Serialize(Serialization::IArchive& ar)
{
	auto& settings = m_pViewport->GetHelperSettings();

	bool prevShowPrefabHelper = settings.showPrefabHelper;
	settings.Serialize(ar);
	
	if (prevShowPrefabHelper != settings.showPrefabHelper)
	{
		// Update Prefab children if parent has been changed
		m_propertyTree->revert();
	}
}

void CViewportHelperWnd::OnReset()
{
	m_pViewport->GetHelperSettings() = SViewportHelperSettings{};
	m_propertyTree->revert();
}

void CViewportHelperWnd::showEvent(QShowEvent* e)
{
	QWidget::showEvent(e);
	// Need to do that here, after widget is shown to make sure the contents are stretched to the correct dimensions.
	// QPropertyTree does take a size event before showing but for some reason the size it receives is not correct.
	// Intermediate widgets will have set the sizes state accordingly after they are shown, so
	// setting this here ensures that QPropertyTree::updateHeights is called.
	// If this is not done, then the last properties of the object are "eaten" by the layout until a new refresh/resize forces
	// QPropertyTree::updateHeights to be called.
	// Interestingly enough, there is no size event to QPropertyTree after the correct size is set, which leads me to believe that some
	// Qt bug might be in the works (or the weird hierarchy we use with QScrollArea confuses the whole system)
	m_propertyTree->setSizeToContent(true);

	// The tree consists entirely from CheckBoxes. Put them close to the right border
	m_propertyTree->setPropertySplitterPos(220);
}
