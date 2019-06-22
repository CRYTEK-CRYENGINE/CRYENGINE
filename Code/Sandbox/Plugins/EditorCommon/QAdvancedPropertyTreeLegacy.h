// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>

class PROPERTY_TREE_API QAdvancedPropertyTreeLegacy : public QPropertyTreeLegacy
{
	Q_OBJECT

public:
	explicit QAdvancedPropertyTreeLegacy(const QString& moduleName, QWidget* const pParent = nullptr);
	virtual ~QAdvancedPropertyTreeLegacy();

public slots:
	void LoadPersonalization();
	void SavePersonalization();

private:
	QString m_moduleName;
};
