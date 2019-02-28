// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"
#include "EditorCommonAPI.h"

#include <QWidget>

namespace CryGraphEditor {

class CNodeGraphViewStyle;

typedef uint32 StyleIdHash;

class EDITOR_COMMON_API CNodeGraphViewStyleItem : public QWidget
{
	Q_OBJECT;

public:
	CNodeGraphViewStyleItem(const char* szStyleId);

	const char*          GetId() const               { return m_styleId.c_str(); }
	StyleIdHash          GetIdHash() const           { return m_styleIdHash; }

	CNodeGraphViewStyle* GetViewStyle() const        { return m_pViewStyle; }

	QWidget*             GetParent() const           { return parentWidget(); }
	void                 SetParent(QWidget* pParent) { setParent(pParent); }
	void                 SetParent(CNodeGraphViewStyle* pViewStyle);

private:
	const string         m_styleId;
	const StyleIdHash    m_styleIdHash;
	CNodeGraphViewStyle* m_pViewStyle;
};

}

