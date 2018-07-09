// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IconXPMCache.h"
#include "QPropertyTree.h"
#include <memory>

class QPropertyTree;
class QPainter;

namespace property_tree {

class QDrawContext : public IDrawContext
{
public:
	QDrawContext(QPropertyTree* tree, QPainter* painter)
	: tree_(tree)
	, painter_(painter)
	, iconCache_(new IconXPMCache())
	{

		this->tree = tree;
	}

	void drawControlButton(const Rect& rect, const char* text, int buttonFlags, property_tree::Font font, const Color* colorOverride = 0) override;
	void drawButton(const Rect& rect, const char* text, int buttonFlags, property_tree::Font font, const Color* colorOverride = 0) override;
	void drawButtonWithIcon(const Icon&, const Rect& rect, const char* text, int buttonFlags, property_tree::Font font) override;
	void drawCheck(const Rect& rect, bool disabled, CheckState checked) override;
	void drawColor(const Rect& rect, const Color& color) override;
	void drawComboBox(const Rect& rect, const char* text, bool disabled) override;
	void drawEntry(const Rect& rect, const char* text, bool pathEllipsis, bool grayBackground, int trailingOffset) override;
	void drawRowLine(const Rect& rect) override;
	void drawHorizontalLine(const Rect& rect) override;
	void drawIcon(const Rect& rect, const Icon& icon, IconEffect iconEffect) override;
	void drawLabel(const char* text, Font font, const Rect& rect, bool selected, bool disabled) override;
	void drawNumberEntry(const char* text, const Rect& rect, int fieldFlags, double minValue, double maxValue) override;
	void drawPlus(const Rect& rect, bool expanded, bool selected, bool grayed) override;
	void drawGroupBox(const Rect& rect, int headerHeight) override;
	void drawSplitter(const Rect& rect) override;
	void drawSelection(const Rect& rect, bool inlinedRow) override;
	void drawValueText(bool highlighted, const char* text) override;
	void drawValidatorWarningIcon(const Rect& rect) override;
	void drawValidatorErrorIcon(const Rect& rect) override;
	void drawValidators(PropertyRow* row, const Rect& totalRect) override;

	QPropertyTree* tree_;
	QPainter* painter_;
	std::auto_ptr<IconXPMCache> iconCache_;
};

void fillRoundRectangle(QPainter& p, const QBrush& brush, const QRect& r, const QColor& borderColor, int radius);
void drawRoundRectangle(QPainter& p, const QRect &_r, unsigned int color, int radius, int width);
QRect toQRect(const Rect& r);
Rect fromQRect(const QRect& r);
QPoint toQPoint(const Point& p);
Point fromQPoint(const QPoint& p);

}

