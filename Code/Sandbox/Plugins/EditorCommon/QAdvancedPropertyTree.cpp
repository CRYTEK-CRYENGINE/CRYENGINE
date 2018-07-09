// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QAdvancedPropertyTree.h"

#include "IEditor.h"
#include "EditorFramework/PersonalizationManager.h"
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <QEvent>

QAdvancedPropertyTree::QAdvancedPropertyTree(const QString& moduleName, QWidget* const pParent /*= nullptr*/)
	: QPropertyTree(pParent)
	, m_moduleName(moduleName)
{
	connect(this, &QAdvancedPropertyTree::signalSerialized, this, &QAdvancedPropertyTree::LoadPersonalization);
}

QAdvancedPropertyTree::~QAdvancedPropertyTree()
{
}

void QAdvancedPropertyTree::LoadPersonalization()
{
	const auto pPersonalizationManager = GetIEditor()->GetPersonalizationManager();
	if (pPersonalizationManager)
	{
		// disconnect personalization saving signal as we'll actually change the layout of the property tree right now
		disconnect(this, &QAdvancedPropertyTree::signalSizeChanged, this, &QAdvancedPropertyTree::SavePersonalization);
		const auto pRoot = model()->root();
		for (auto pRow : * pRoot)
		{
			const auto value = pPersonalizationManager->GetProperty(m_moduleName, pRow->name());
			expandRow(pRow, value.isValid() ? value.toBool() : true);
		}

		// reconnect saving signal
		// saving will occur whenever the user changes the expanded/collapsed state of a node in the property tree
		connect(this, &QAdvancedPropertyTree::signalSizeChanged, this, &QAdvancedPropertyTree::SavePersonalization);
		// We only want to do this once
		disconnect(this, &QAdvancedPropertyTree::signalSerialized, this, &QAdvancedPropertyTree::LoadPersonalization);
	}
}

void QAdvancedPropertyTree::SavePersonalization()
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

