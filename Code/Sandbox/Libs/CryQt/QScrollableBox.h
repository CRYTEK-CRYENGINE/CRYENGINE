// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "CryQtAPI.h"
#include "CryQtCompatibility.h"
#include <QWidget>

class QScrollArea;
class QVBoxLayout;

class CRYQT_API QScrollableBox : public QWidget
{
	Q_OBJECT
public:
	QScrollableBox(QWidget* parent = nullptr);
	virtual ~QScrollableBox() {}
	void addWidget(QWidget*);
	void removeWidget(QWidget*);
	void insertWidget(int i, QWidget*);
	void clearWidgets();
	int indexOf(QWidget*);

protected:
	QScrollArea* m_scrollArea;
	QVBoxLayout* m_layout;
};

