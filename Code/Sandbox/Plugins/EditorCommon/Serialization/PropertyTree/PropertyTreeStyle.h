// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Serialization.h"
#include <CrySerialization/yasli/decorators/Range.h>

struct PropertyTreeStyle
{
	bool compact;
	bool packCheckboxes;
	bool fullRowMode;
	bool horizontalLines;
	bool doNotIndentSecondLevel;
	bool groupShadows;
	bool groupRectangle;
	bool alignLabelsToRight;
	bool selectionRectangle;
	bool showSliderCursor;
	bool propertySplitter;
	int propertySplitterHalfWidth;
	float valueColumnWidth;
	float rowSpacing;
	unsigned char levelShadowOpacity;
	float levelIndent;
	float firstLevelIndent;
	float groupShade;
	float sliderSaturation;
	float scrollbarIndent;

	PropertyTreeStyle()
	: compact(false)
	, packCheckboxes(true)
	, fullRowMode(false)
	, valueColumnWidth(.59f)
	, rowSpacing(1.1f)
	, horizontalLines(true)
	, firstLevelIndent(0.75f)
	, levelIndent(0.75f)
	, levelShadowOpacity(36)
	, doNotIndentSecondLevel(false)
	, groupShadows(false)
	, sliderSaturation(0.0f)
	, groupShade(0.15f)
	, groupRectangle(false)
	, propertySplitter(false)
	, propertySplitterHalfWidth(5)
	, alignLabelsToRight(false)
	, selectionRectangle(true)
	, showSliderCursor(true)
	, scrollbarIndent(1.0f)
	{
	}

	void YASLI_SERIALIZE_METHOD(yasli::Archive& ar)
	{
		using yasli::Range;
		ar.doc("Here you can define appearance of QPropertyTree control.");

		ar(valueColumnWidth, "valueColumnWidth", "Value Column Width");		ar.doc("Defines a ratio of the value / name columns. Normalized.");
		ar(Range(rowSpacing, 0.5f, 3.0f), "rowSpacing", "Row Spacing"); ar.doc("Height of one row (line) in text-height units.");
		ar(alignLabelsToRight, "alignLabelsToRight", "Right Alignment");
		ar(Range(levelIndent, 0.0f, 3.0f), "levelIndent", "Level Indent"); ar.doc("Indentation of a every next level in text-height units.");

		ar(Range(firstLevelIndent, 0.0f, 3.0f), "firstLevelIndent", "First Level Indent"); ar.doc("Indentation of a very first level in text-height units.");
		ar(Range(sliderSaturation, 0.0f, 1.0f), "sliderSaturation", "Slider Saturation");	
		ar(levelShadowOpacity, "levelShadowOpacity", "Level Shadow Opacity"); ar.doc("Amount of background darkening that gets added to each next nested level.");

		ar(selectionRectangle, "selectionRectangle", "Selection Rectangle"); ar.doc("Show selection rectangle instead of just highlighting text.");
		ar(compact, "compact", "Compact"); ar.doc("Compact mode removes expansion pluses from the level and reduces inner padding. Useful for narrowing the widget.");
		ar(packCheckboxes, "packCheckboxes", "Pack Checkboxes"); ar.doc("Arranges checkboxes in two columns, when possible.");

		ar(horizontalLines, "horizontalLines", "Horizontal Lines"); ar.doc("Show thin line that connects row name with its value.");

		ar(doNotIndentSecondLevel, "doNotIndentSecondLevel", "Do not indent second level");	
		ar(groupShadows, "groupShadows", "Group Shadows");	
		ar(groupRectangle, "groupRectangle", "Group Rectangle");	

		ar(Range(groupShade, -1.0f, 1.0f), "groupShade", "Group Shade");	ar.doc("Shade of the group.");
	}
};


