// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"
#include "EditorCommonAPI.h"

#include "NodeGraphStyleItem.h"

#include <QWidget>
#include <QColor>
#include <QMargins>

#include <unordered_map>

namespace CryGraphEditor {

class CNodePinWidgetStyle;
class CNodeWidgetStyle;

class EDITOR_COMMON_API CNodeContentWidgetStyle : protected QWidget
{
	Q_OBJECT;

public:
	Q_PROPERTY(QColor backgroundColor READ GetBackgroundColor WRITE SetBackgroundColor DESIGNABLE true);
	Q_PROPERTY(QMargins margins READ GetMargins WRITE SetMargins DESIGNABLE true);

public:
	CNodeContentWidgetStyle(CNodeWidgetStyle& nodeWidgetStyle);

	const CNodeWidgetStyle&    GetNodeWidgetStyle() const              { return m_nodeWidgetStyle; }

	const QColor&              GetBackgroundColor() const              { return m_backgroundColor; }
	void                       SetBackgroundColor(const QColor& color) { m_backgroundColor = color; }

	const QMargins&            GetMargins() const                      { return m_margins; }
	void                       SetMargins(const QMargins& margins)     { m_margins = margins; }

	void                       RegisterPinWidgetStyle(CNodePinWidgetStyle* pStyle);
	const CNodePinWidgetStyle* GetPinWidgetStyle(const char* styleId) const;

private:
	const CNodeWidgetStyle& m_nodeWidgetStyle;

	QColor                  m_backgroundColor;
	QMargins                m_margins;

	std::unordered_map<StyleIdHash, CNodePinWidgetStyle*> m_pinWidgetStylesById;
};

}

