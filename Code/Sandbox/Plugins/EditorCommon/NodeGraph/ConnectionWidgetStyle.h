// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"
#include "EditorCommonAPI.h"

#include "NodeGraphStyleItem.h"

namespace CryGraphEditor {

class CNodePinWidgetStyle;

class EDITOR_COMMON_API CConnectionWidgetStyle : public CNodeGraphViewStyleItem
{
	Q_OBJECT;

public:
	Q_PROPERTY(QColor color READ GetColor WRITE SetColor DESIGNABLE true);
	Q_PROPERTY(qreal width READ GetWidth WRITE SetWidth DESIGNABLE true);
	Q_PROPERTY(qreal bezier READ GetBezier WRITE SetBezier DESIGNABLE true);

	Q_PROPERTY(bool usePinColor READ GetUsePinColors WRITE SetUsePinColors DESIGNABLE true);

public:
	CConnectionWidgetStyle(const char* szStyleId, CNodeGraphViewStyle& viewStyle);

	// QProperties
	const QColor& GetColor() const                   { return m_color; }
	void          SetColor(const QColor& color)      { m_color = color; }

	qreal         GetWidth() const                   { return m_width; }
	void          SetWidth(qreal width)              { m_width = width; }

	qreal         GetBezier() const                  { return m_bezier; }
	void          SetBezier(qreal bezier)            { m_bezier = bezier; }

	bool          GetUsePinColors() const            { return m_usePinColors; }
	void          SetUsePinColors(bool usePinColors) { m_usePinColors = usePinColors; }

	//
	QColor GetSelectionColor() const { return GetViewStyle()->GetSelectionColor(); }
	QColor GetHighlightColor() const { return GetViewStyle()->GetHighlightColor(); }

private:
	QColor m_color;
	qreal  m_width;
	qreal  m_bezier;
	bool   m_usePinColors;
};

}

