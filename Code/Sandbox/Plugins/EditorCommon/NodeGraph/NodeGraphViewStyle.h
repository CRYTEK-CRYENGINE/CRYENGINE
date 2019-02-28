// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include "ICryGraphEditor.h"
#include "EditorCommonAPI.h"

#include "NodeGraphStyleItem.h"

#include <unordered_map>

namespace CryGraphEditor {

class CNodeWidgetStyle;
class CConnectionWidgetStyle;
class CNodePinWidgetStyle;

typedef uint32 StyleIdHash;

enum EStyleState : int32
{
	StyleState_Default = 0,
	StyleState_Highlighted,
	StyleState_Selected,
	StyleState_Disabled,

	StyleState_Count
};

class EDITOR_COMMON_API CNodeGraphViewStyle : public QWidget
{
	Q_OBJECT;

public:
	Q_PROPERTY(QColor selectionColor READ GetSelectionColor WRITE SetSelectionColor DESIGNABLE true);
	Q_PROPERTY(QColor highlightColor READ GetHighlightColor WRITE SetHighlightColor DESIGNABLE true);

	Q_PROPERTY(QColor gridBackgroundColor READ GetGridBackgroundColor WRITE SetGridBackgroundColor DESIGNABLE true);
	Q_PROPERTY(QColor gridSegementLineColor READ GetGridSegmentLineColor WRITE SetGridSegmentLineColor DESIGNABLE true);
	Q_PROPERTY(float gridSegementLineWidth READ GetGridSegementLineWidth WRITE SetGridSegementLineWidth DESIGNABLE true);
	Q_PROPERTY(float gridSegemtnsSize READ GetGridSegmentSize WRITE SetGridSegmentSize DESIGNABLE true);

	Q_PROPERTY(QColor gridSubSegmentLineColor READ GetGridSubSegmentLineColor WRITE SetGridSubSegmentLineColor DESIGNABLE true);
	Q_PROPERTY(int32 gridSubSegmentCount READ GetGridSubSegmentCount WRITE SetGridSubSegmentCount DESIGNABLE true);
	Q_PROPERTY(float gridSubSegmentLineWidth READ GetGridSubSegmentLineWidth WRITE SetGridSubSegmentLineWidth DESIGNABLE true);

public:
	CNodeGraphViewStyle(const char* szStyleId);

	void                          RegisterNodeWidgetStyle(CNodeWidgetStyle* pStyle);
	const CNodeWidgetStyle*       GetNodeWidgetStyle(const char* styleId) const;

	void                          RegisterConnectionWidgetStyle(CConnectionWidgetStyle* pStyle);
	const CConnectionWidgetStyle* GetConnectionWidgetStyle(const char* styleId) const;

	void                          RegisterPinWidgetStyle(CNodePinWidgetStyle* pStyle);
	const CNodePinWidgetStyle*    GetPinWidgetStyle(const char* styleId) const;

	// Properties
	const QColor& GetSelectionColor() const                      { return m_selectionColor; }
	void          SetSelectionColor(const QColor color)          { m_selectionColor = color; }

	const QColor& GetHighlightColor() const                      { return m_highlightColor; }
	void          SetHighlightColor(const QColor color)          { m_highlightColor = color; }

	const QColor& GetGridBackgroundColor() const                 { return m_gridBackgroundColor; }
	void          SetGridBackgroundColor(const QColor color)     { m_gridBackgroundColor = color; }

	const QColor& GetGridSegmentLineColor() const                { return m_gridSegmentLineColor; }
	void          SetGridSegmentLineColor(const QColor color)    { m_gridSegmentLineColor = color; }

	float         GetGridSegementLineWidth() const               { return m_gridSegmentLineWidth; }
	void          SetGridSegementLineWidth(float width)          { m_gridSegmentLineWidth = width; }

	float         GetGridSegmentSize() const                     { return m_gridSegmentSize; }
	void          SetGridSegmentSize(float width)                { m_gridSegmentSize = width; }

	const QColor& GetGridSubSegmentLineColor() const             { return m_gridSubSegmentLineColor; }
	void          SetGridSubSegmentLineColor(const QColor color) { m_gridSubSegmentLineColor = color; }

	int32         GetGridSubSegmentCount() const                 { return m_gridSubSegmentCount; }
	void          SetGridSubSegmentCount(int32 count)            { m_gridSubSegmentCount = count; }

	float         GetGridSubSegmentLineWidth() const             { return m_gridSubSegmentLineWidth; }
	void          SetGridSubSegmentLineWidth(float width)        { m_gridSubSegmentLineWidth = width; }

private:
	std::unordered_map<StyleIdHash, CNodeWidgetStyle*>       m_nodeWidgetStylesById;
	std::unordered_map<StyleIdHash, CConnectionWidgetStyle*> m_connectionWidgetStylesById;
	std::unordered_map<StyleIdHash, CNodePinWidgetStyle*>    m_pinWidgetStylesById;

	QColor m_selectionColor;
	QColor m_highlightColor;

	QColor m_gridBackgroundColor;
	QColor m_gridSegmentLineColor;
	float  m_gridSegmentLineWidth;
	float  m_gridSegmentSize;

	QColor m_gridSubSegmentLineColor;
	int32  m_gridSubSegmentCount;
	float  m_gridSubSegmentLineWidth;
};

}

