// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QAdvancedPropertyTreeLegacy.h"

#include "IEditor.h"
#include "EditorFramework/PersonalizationManager.h"
#include <Serialization/PropertyTreeLegacy/PropertyTreeModel.h>
#include <QEvent>

QAdvancedPropertyTreeLegacy::QAdvancedPropertyTreeLegacy(const QString& moduleName, QWidget* const pParent /*= nullptr*/)
	: QPropertyTreeLegacy(pParent)
	, m_moduleName(moduleName)
{
	connect(this, &QAdvancedPropertyTreeLegacy::signalSerialized, this, &QAdvancedPropertyTreeLegacy::LoadPersonalization);
}

QAdvancedPropertyTreeLegacy::~QAdvancedPropertyTreeLegacy()
{
}

void QAdvancedPropertyTreeLegacy::LoadPersonalization()
{
	const auto pPersonalizationManager = GetIEditor()->GetPersonalizationManager();
	if (pPersonalizationManager)
	{
		// disconnect personalization saving signal as we'll actually change the layout of the property tree right now
		disconnect(this, &QAdvancedPropertyTreeLegacy::signalSizeChanged, this, &QAdvancedPropertyTreeLegacy::SavePersonalization);
		const auto pRoot = model()->root();
		for (auto pRow : * pRoot)
		{
			const auto value = pPersonalizationManager->GetProperty(m_moduleName, pRow->name());
			expandRow(pRow, value.isValid() ? value.toBool() : true);
		}

		// reconnect saving signal
		// saving will occur whenever the user changes the expanded/collapsed state of a node in the property tree
		connect(this, &QAdvancedPropertyTreeLegacy::signalSizeChanged, this, &QAdvancedPropertyTreeLegacy::SavePersonalization);
		// We only want to do this once
		disconnect(this, &QAdvancedPropertyTreeLegacy::signalSerialized, this, &QAdvancedPropertyTreeLegacy::LoadPersonalization);
	}
}

void QAdvancedPropertyTreeLegacy::SavePersonalization()
{
	const auto pPersonalizationManager = GetIEditor()->GetPersonalizationManager();
	if (pPersonalizationManager)
	{
		const auto pRoot = model()->root();
		for (auto pRow : * pRoot)
		{
			pPersonalizationManager->SetProperty(m_moduleName, pRow->name(), pRow->expanded());
		}
	}
}
