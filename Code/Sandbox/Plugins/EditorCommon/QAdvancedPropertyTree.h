// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/QPropertyTree/QPropertyTree.h>

class PROPERTY_TREE_API QAdvancedPropertyTree : public QPropertyTree
{
	Q_OBJECT

public:
	explicit QAdvancedPropertyTree(const QString& moduleName, QWidget* const pParent = nullptr);
	virtual ~QAdvancedPropertyTree();

public slots:
	void LoadPersonalization();
	void SavePersonalization();

private:
	QString m_moduleName;
};

