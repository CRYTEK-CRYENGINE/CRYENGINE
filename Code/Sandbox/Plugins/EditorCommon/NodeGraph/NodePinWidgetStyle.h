// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"
#include "EditorCommonAPI.h"

#include "NodeGraphStyleItem.h"
#include "NodeGraphStyleUtils.h"

#include <QWidget>
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QFont>

namespace CryGraphEditor {

class EDITOR_COMMON_API CNodePinWidgetStyle : public CNodeGraphViewStyleItem
{
	Q_OBJECT;

public:
	Q_PROPERTY(QFont textFont READ GetTextFont WRITE SetTextFont DESIGNABLE true);
	Q_PROPERTY(QColor color READ GetColor WRITE SetColor DESIGNABLE true);
	Q_PROPERTY(QSize iconSize READ GetIconSize WRITE SetIconSize DESIGNABLE true);
	Q_PROPERTY(QIcon icon READ GetIcon WRITE SetIcon DESIGNABLE true);

public:
	CNodePinWidgetStyle(const char* szStyleId, CNodeGraphViewStyle& viewStyle);

	// QProperties
	const QFont&   GetTextFont() const            { return m_textFont; }
	void           SetTextFont(const QFont& font) { m_textFont = font; }

	const QColor&  GetColor() const               { return m_color; }
	void           SetColor(const QColor& color);

	const QSize&   GetIconSize() const { return m_iconSize; }
	void           SetIconSize(const QSize& size);

	const QIcon&   GetIcon() const { return m_icon; }
	void           SetIcon(const QIcon& icon);

	const QPointF& GetIconOffset() const                    { return m_iconOffset; }
	void           SetIconOffset(const QPointF& iconOffset) { m_iconOffset = iconOffset; }

	//
	const QPixmap& GetIconPixmap(EStyleState state) const { return m_iconPixmap[state]; }

	QColor         GetSelectionColor() const              { return GetViewStyle()->GetSelectionColor(); }
	QColor         GetHighlightColor() const              { return m_highlightColor; }

protected:
	void GeneratePixmaps();

private:
	QFont   m_textFont;
	QColor  m_color;
	QSize   m_iconSize;
	QPixmap m_iconPixmap[StyleState_Count];
	QPointF m_iconOffset;

	QIcon   m_icon;
	QColor  m_highlightColor;
};

}

