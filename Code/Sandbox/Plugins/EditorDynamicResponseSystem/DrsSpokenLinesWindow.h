// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <CrySerialization/Forward.h>

class QPropertyTree;
class QPushButton;

class CSpokenLinesWidget : public QWidget
{
public:
	CSpokenLinesWidget();
	void SetAutoUpdateActive(bool bValue);

	void Serialize(Serialization::IArchive& ar);

protected:
	QPropertyTree* m_pPropertyTree;
	QPushButton*   m_pUpdateButton;

	int            m_SerializationFilter;
};

