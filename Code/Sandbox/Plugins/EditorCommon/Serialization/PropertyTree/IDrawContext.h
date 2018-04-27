// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Color.h"
#include "Rect.h"
#include <CrySerialization/yasli/decorators/IconXPM.h>

class PropertyTree;
class PropertyRow;

namespace property_tree {

enum Font
{
	FONT_NORMAL,
	FONT_BOLD
};

struct Rect;

enum CheckState {
	CHECK_SET,
	CHECK_NOT_SET,
	CHECK_IN_BETWEEN
};


struct Icon
{
	enum Type
	{
		TYPE_EMPTY,
		TYPE_XPM,
		TYPE_FILE
	};
	Type type;
	yasli::string filename;
	yasli::IconXPM xpm;

	Icon()
	: type(TYPE_EMPTY)
	{
	}

	Icon(const char* filename)
	: type(TYPE_FILE)
	, filename(filename)
	{
	}

	Icon(const yasli::IconXPM& xpm)
	: type(TYPE_XPM)
	, xpm(xpm)
	{
	}

	bool isNull() const { return type == TYPE_EMPTY; }
};

enum IconEffect
{
	ICON_NORMAL,
	ICON_DISABLED
};

enum {
	BUTTON_PRESSED = 1 << 0,
	BUTTON_FOCUSED = 1 << 1,
	BUTTON_DISABLED = 1 << 2,
	BUTTON_DROP_DOWN = 1 << 3,
	BUTTON_CENTER_TEXT = 1 << 4,
	BUTTON_NO_FRAME = 1 << 5
};

enum {
	FIELD_PRESSED = 1 << 0,
	FIELD_SELECTED = 1 << 1,
	FIELD_DISABLED = 1 << 2
};

struct IDrawContext
{
	const PropertyTree* tree;
	Rect widgetRect;
	Rect lineRect;
	bool captured;
	bool pressed;

	IDrawContext()
	: tree(0)
	, captured(false)
	, pressed(false)
	{
	}

	virtual void drawControlButton(const Rect& rect, const char* text, int buttonFlags, property_tree::Font font, const Color* colorOverride = 0) = 0;
	virtual void drawButton(const Rect& rect, const char* text, int buttonFlags, property_tree::Font font, const Color* colorOverride = 0) = 0;
	virtual void drawCheck(const Rect& rect, bool disabled, CheckState checked) = 0;
	virtual void drawColor(const Rect& rect, const Color& color) = 0;
	virtual void drawComboBox(const Rect& rect, const char* text, bool disabled) = 0;
	virtual void drawEntry(const Rect& rect, const char* text, bool pathEllipsis, bool grayBackground, int trailingOffset) = 0;
	virtual void drawRowLine(const Rect& rect) = 0;
	virtual void drawHorizontalLine(const Rect& rect) = 0;
	virtual void drawIcon(const Rect& rect, const Icon& icon, IconEffect iconEffect = ICON_NORMAL) = 0;
	virtual void drawButtonWithIcon(const Icon&, const Rect& rect, const char* text, int buttonFlags, property_tree::Font font) = 0;
	virtual void drawLabel(const char* text, Font font, const Rect& rect, bool selected, bool disabled) = 0;
	virtual void drawNumberEntry(const char* text, const Rect& rect, int fieldFlags, double minValue, double maxValue) = 0;
	virtual void drawPlus(const Rect& rect, bool expanded, bool selected, bool grayed) = 0;
	virtual void drawGroupBox(const Rect& rect, int headerHeight) = 0;
	virtual void drawSplitter(const Rect& rect) = 0;
	virtual void drawSelection(const Rect& rect, bool inlinedRow) = 0;
	virtual void drawValueText(bool highlighted, const char* text) = 0;
	virtual void drawValidatorWarningIcon(const Rect& rect) {}
	virtual void drawValidatorErrorIcon(const Rect& rect) {}
	virtual void drawValidators(PropertyRow* row, const Rect& rect) {}
};

}

using namespace property_tree; // temporary

