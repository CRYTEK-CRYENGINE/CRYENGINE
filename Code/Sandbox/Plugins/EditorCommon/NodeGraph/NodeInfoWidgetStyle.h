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

class CNodeWidgetStyle;

class EDITOR_COMMON_API CNodeInfoWidgetStyle : private QWidget
{
	Q_OBJECT;

public:
	Q_PROPERTY(QFont nameFont READ GetFont WRITE SetFont DESIGNABLE true);
	Q_PROPERTY(QString errorText READ GetErrorText WRITE SetErrorText DESIGNABLE true);
	Q_PROPERTY(QColor errorTextColor READ GetErrorTextColor WRITE SetErrorTextColor DESIGNABLE true);
	Q_PROPERTY(QColor errorBackgroundColor READ GetErrorBackgroundColor WRITE SetErrorBackgroundColor DESIGNABLE true);
	Q_PROPERTY(QString warningText READ GetWarningText WRITE SetWarningText DESIGNABLE true);
	Q_PROPERTY(QColor warningTextColor READ GetWarningTextColor WRITE SetWarningTextColor DESIGNABLE true);
	Q_PROPERTY(QColor warningBackgroundColor READ GetWarningBackgroundColor WRITE SetWarningBackgroundColor DESIGNABLE true);

	Q_PROPERTY(int32 height READ GetHeight WRITE SetHeight DESIGNABLE true);

public:
	CNodeInfoWidgetStyle(CNodeWidgetStyle& nodeWidgetStyle);

	const CNodeWidgetStyle& GetNodeStyle() const { return m_nodeWidgetStyle; }

	//
	const QFont&  GetFont() const                                { return m_font; }
	void          SetFont(const QFont& font)                     { m_font = font; }

	const QString GetErrorText() const                           { return m_errorText; }
	void          SetErrorText(const QString& text)              { m_errorText = text; }
	const QColor& GetErrorTextColor() const                      { return m_errorTextColor; }
	void          SetErrorTextColor(const QColor& color)         { m_errorTextColor = color; }
	const QColor& GetErrorBackgroundColor() const                { return m_errorBackgroundColor; }
	void          SetErrorBackgroundColor(const QColor& color)   { m_errorBackgroundColor = color; }

	const QString GetWarningText() const                         { return m_warningText; }
	void          SetWarningText(const QString& text)            { m_warningText = text; }
	const QColor& GetWarningTextColor() const                    { return m_warningTextColor; }
	void          SetWarningTextColor(const QColor& color)       { m_warningTextColor = color; }
	const QColor& GetWarningBackgroundColor() const              { return m_warningBackgroundColor; }
	void          SetWarningBackgroundColor(const QColor& color) { m_warningBackgroundColor = color; }

	int32         GetHeight() const                              { return m_height; }
	void          SetHeight(int32 height)                        { m_height = height; }

private:
	const CNodeWidgetStyle& m_nodeWidgetStyle;

	QFont                   m_font;
	QString                 m_errorText;
	QColor                  m_errorTextColor;
	QColor                  m_errorBackgroundColor;
	QString                 m_warningText;
	QColor                  m_warningTextColor;
	QColor                  m_warningBackgroundColor;

	int32                   m_height;
};

}

