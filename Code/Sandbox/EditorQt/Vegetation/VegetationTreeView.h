// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#pragma once

#include "QAdvancedTreeView.h"

class CVegetationTreeView : public QAdvancedTreeView
{
	Q_OBJECT

public:
	explicit CVegetationTreeView(QWidget* pParent = nullptr);

	virtual void startDrag(Qt::DropActions supportedActions) override;

signals:
	void dragStarted();
};

