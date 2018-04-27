// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"
#include "EditorCommonAPI.h"

#include "NodeGraphStyleItem.h"

#include <QColor>
#include <QMargins>

namespace CryGraphEditor {

class CNodeHeaderWidgetStyle;
class CNodeContentWidgetStyle;
class CNodeInfoWidgetStyle;

class EDITOR_COMMON_API CNodeWidgetStyle : public CNodeGraphViewStyleItem
{
	Q_OBJECT;

	friend class CNodeHeaderWidgetStyle;
	friend class CNodeContentWidgetStyle;
	friend class CNodeInfoWidgetStyle;

public:
	Q_PROPERTY(QColor borderColor READ GetBorderColor WRITE SetBorderColor DESIGNABLE true);
	Q_PROPERTY(int32 borderWidth READ GetBorderWidth WRITE SetBorderWidth DESIGNABLE true);
	Q_PROPERTY(QColor backgroundColor READ GetBackgroundColor WRITE SetBackgroundColor DESIGNABLE true);
	Q_PROPERTY(QMargins margins READ GetMargins WRITE SetMargins DESIGNABLE true);

public:
	CNodeWidgetStyle(const char* szStyleId, CNodeGraphViewStyle& viewStyle);

	const QColor&                  GetBorderColor() const                 { return m_borderColor; }
	void                           SetBorderColor(const QColor& color)    { m_borderColor = color; }

	int32                          GetBorderWidth() const                 { return m_borderWidth; }
	void                           SetBorderWidth(int32 width)            { m_borderWidth = width; }

	const QColor&                  GetBackgroundColor() const             { return m_backgroundColor; }
	void                           SetBackgroundColor(const QColor color) { m_backgroundColor = color; }

	QMargins                       GetMargins() const                     { return m_margins; }
	void                           SetMargins(QMargins margins)           { m_margins = margins; }

	const CNodeHeaderWidgetStyle&  GetHeaderWidgetStyle() const           { return *m_pHeaderStyle; }
	const CNodeContentWidgetStyle& GetContentWidgetStyle() const          { return *m_pContentStyle; }
	const CNodeInfoWidgetStyle&    GetInfoWidgetStyle() const             { return *m_pInfoWidgetStyle; }

	CNodeHeaderWidgetStyle&        GetHeaderWidgetStyle()                 { return *m_pHeaderStyle; }
	CNodeContentWidgetStyle&       GetContentWidgetStyle()                { return *m_pContentStyle; }
	CNodeInfoWidgetStyle&          GetInfoWidgetStyle()                   { return *m_pInfoWidgetStyle; }

	const QColor& GetSelectionColor() const              { return GetViewStyle()->GetSelectionColor(); }
	const QColor& GetHighlightColor() const              { return GetViewStyle()->GetHighlightColor(); }
	const QIcon&  GetMenuIcon() const;

protected:
	virtual CNodeHeaderWidgetStyle*  CreateHeaderWidgetStyle();
	virtual CNodeContentWidgetStyle* CreateContentWidgetStyle();
	virtual CNodeInfoWidgetStyle*    CreateInfoWidgetStyle();

private:
	CNodeHeaderWidgetStyle*  m_pHeaderStyle;
	CNodeContentWidgetStyle* m_pContentStyle;
	CNodeInfoWidgetStyle*    m_pInfoWidgetStyle;

	QColor                   m_borderColor;
	int32                    m_borderWidth;
	QColor                   m_backgroundColor;
	QMargins                 m_margins;
};

}

