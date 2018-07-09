// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QLineEdit>

namespace CrySchematycEditor {

template<typename TItem>
class CNameEditor : public QLineEdit
{
public:
	CNameEditor(TItem& item, QWidget* pParent = nullptr)
		: QLineEdit(pParent)
		, m_item(item)
	{
		setFrame(true);
	}

	virtual void focusInEvent(QFocusEvent* pEvent) override
	{
		setText(m_item.GetName());
		selectAll();
		QLineEdit::focusInEvent(pEvent);
	}

private:
	TItem& m_item;
};

}

