// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"
#include "EditorCommonAPI.h"

#include "NodeGraphStyleItem.h"
#include "NodeGraphStyleUtils.h"
#include "NodeWidgetStyle.h"

#include <QWidget>
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QFont>

namespace CryGraphEditor {

class CNodeWidgetStyle;

class EDITOR_COMMON_API CNodeHeaderWidgetStyle : public QWidget
{
	Q_OBJECT;

public:
	Q_PROPERTY(QFont nameFont READ GetNameFont WRITE SetNameFont DESIGNABLE true);
	Q_PROPERTY(QColor nameColor READ GetNameColor WRITE SetNameColor DESIGNABLE true);
	Q_PROPERTY(QIcon icon READ GetNodeIcon WRITE SetNodeIcon DESIGNABLE true);

	Q_PROPERTY(QSize viewIconSize READ GetNodeIconViewSize WRITE SetNodeIconViewSize DESIGNABLE true);

	Q_PROPERTY(int32 height READ GetHeight WRITE SetHeight DESIGNABLE true);
	Q_PROPERTY(QColor leftColor READ GetLeftColor WRITE SetLeftColor DESIGNABLE true);
	Q_PROPERTY(QColor rightColor READ GetRightColor WRITE SetRightColor DESIGNABLE true);
	Q_PROPERTY(QMargins margins READ GetMargins WRITE SetMargins DESIGNABLE true);

public:
	CNodeHeaderWidgetStyle(CNodeWidgetStyle& nodeWidgetStyle);

	const CNodeWidgetStyle& GetNodeStyle() const { return m_nodeWidgetStyle; }

	//
	const QFont&    GetNameFont() const               { return m_nameFont; }
	void            SetNameFont(const QFont& font)    { m_nameFont = font; }

	const QColor&   GetNameColor() const              { return m_nameColor; }
	void            SetNameColor(const QColor& color) { m_nameColor = color; }

	const QIcon&    GetNodeIcon() const               { return m_nodeIcon; }
	void            SetNodeIcon(const QIcon& icon);

	const QColor&   GetNodeIconMenuColor() const { return m_nodeIconMenuColor; }
	void            SetNodeIconMenuColor(const QColor& color);

	const QSize&    GetNodeIconMenuSize() const { return m_nodeIconMenuSize; }
	void            SetNodeIconMenuSize(const QSize& size);

	const QColor&   GetNodeIconViewDefaultColor() const { return m_nodeIconViewColor[StyleState_Default]; }
	void            SetNodeIconViewDefaultColor(const QColor& color);

	const QColor&   GetNodeIconViewSelectedColor() const { return m_nodeIconViewColor[StyleState_Selected]; }
	void            SetNodeIconViewSelectedColor(const QColor& color);

	const QColor&   GetNodeIconViewHighlightedColor() const { return m_nodeIconViewColor[StyleState_Highlighted]; }
	void            SetNodeIconViewHighlightedColor(const QColor& color);

	const QColor&   GetNodeIconViewDeactivatedColor() const { return m_nodeIconViewColor[StyleState_Disabled]; }
	void            SetNodeIconViewDeactivatedColor(const QColor& color);

	const QSize&    GetNodeIconViewSize() const { return m_nodeIconViewSize; }
	void            SetNodeIconViewSize(const QSize& size);

	int32           GetHeight() const                   { return m_height; }
	void            SetHeight(int32 height)             { m_height = height; }

	const QColor&   GetLeftColor() const                { return m_colorLeft; }
	void            SetLeftColor(const QColor& color)   { m_colorLeft = color; }

	const QColor&   GetRightColor() const               { return m_colorRight; }
	void            SetRightColor(const QColor& color)  { m_colorRight = color;; }

	const QMargins& GetMargins() const                  { return m_margins; }
	void            SetMargins(const QMargins& margins) { m_margins = margins; }

	//
	// TODO: Colors for left + right.
	const QColor& GetSelectionColor() const { return m_nodeWidgetStyle.GetSelectionColor(); }
	const QColor& GetHighlightColor() const { return m_nodeWidgetStyle.GetHighlightColor(); }
	// ~TODO

	const QPixmap& GetNodeIconViewPixmap(EStyleState state) const { return m_nodeIconViewPixmap[state]; }
	const QIcon&   GetNodeIconMenu() const                        { return m_nodeIconMenu; }

protected:
	void GeneratePixmap(EStyleState state);

private:
	const CNodeWidgetStyle& m_nodeWidgetStyle;

	QFont                   m_nameFont;
	QColor                  m_nameColor;
	QIcon                   m_nodeIcon;
	QIcon                   m_nodeIconMenu;
	QColor                  m_nodeIconMenuColor;
	QSize                   m_nodeIconMenuSize;
	QPixmap                 m_nodeIconViewPixmap[StyleState_Count];
	QColor                  m_nodeIconViewColor[StyleState_Count];
	QSize                   m_nodeIconViewSize;

	int32                   m_height;
	QColor                  m_colorLeft;
	QColor                  m_colorRight;
	QMargins                m_margins;
};

}

