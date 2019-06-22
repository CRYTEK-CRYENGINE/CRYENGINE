// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "ICryGraphEditor.h"
#include "QScrollableBox.h"

#include <CrySerialization/IArchive.h>

class QPropertyTreeLegacy;

namespace CryGraphEditor {

class CAbstractNodeGraphViewModelItem;

class EDITOR_COMMON_API CNodeGraphItemPropertiesWidget : public QScrollableBox
{
	Q_OBJECT

public:
	CNodeGraphItemPropertiesWidget(GraphItemSet& items);
	CNodeGraphItemPropertiesWidget(CAbstractNodeGraphViewModelItem& item);
	~CNodeGraphItemPropertiesWidget();

	virtual void showEvent(QShowEvent* pEvent) override;

protected:
	void SetupPropertyTree();

protected:
	QPropertyTreeLegacy*          m_pPropertyTree;
	Serialization::SStructs m_structs;
};

}
