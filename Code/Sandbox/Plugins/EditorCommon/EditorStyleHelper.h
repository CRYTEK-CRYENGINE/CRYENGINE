// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <QWidget>
#include <QPalette>
#include <QColor>

#define STYLE_PROP(type, read, write)    \
  type m_ ## read;                       \
  type read() { return m_ ## read; }     \
  void write(type v) { m_ ## read = v; } \
  Q_PROPERTY(type read READ read WRITE write);

class EDITOR_COMMON_API EditorStyleHelper : public QWidget
{
	Q_OBJECT;
public:
	STYLE_PROP(QColor, errorColor, setErrorColor);
	STYLE_PROP(QColor, successColor, setSuccessColor);
	STYLE_PROP(QColor, warningColor, setWarningColor);
	STYLE_PROP(QColor, viewportHeaderBackground, setViewportHeaderBackground);
	STYLE_PROP(QColor, propertyRowCurveFill, setPropertyRowCurveFill);

	// Icon tinting
	STYLE_PROP(QColor, iconTint, setIconTint);
	STYLE_PROP(QColor, disabledIconTint, setDisabledIconTint);
	STYLE_PROP(QColor, activeIconTint, setActiveIconTint);
	STYLE_PROP(QColor, selectedIconTint, setSelectedIconTint);
	STYLE_PROP(QColor, alternateSelectedIconTint, alternateSelectedIconTint);
	STYLE_PROP(QColor, alternateHoverIconTint, alternateHoverIconTint);
	STYLE_PROP(QColor, contrastingIconTint, contrastingIconTint);

	// QTreeView
	STYLE_PROP(int, treeViewItemMarginTop, treeViewItemMarginTop);

	// CTimeline colors
	STYLE_PROP(QColor, timelineTrackColor, setTimelineTrackColor);
	STYLE_PROP(QColor, timelineOutsideTrackColor, setTimelineOutsideTrackColor);
	STYLE_PROP(QColor, timelineDescriptionTrackColor, setTimelineDescriptionTrackColor);
	STYLE_PROP(QColor, timelineCompositeTrackColor, setTimelineCompositeTrackColor);
	STYLE_PROP(QColor, timelineSelectionColor, setTimelineSelectionColor);
	STYLE_PROP(QColor, timelineDisabledColor, setTimelineDisabledColor);
	STYLE_PROP(QColor, timelineDescriptionTextColor, setTimelineDescriptionTextColor);
	STYLE_PROP(QColor, timelineInnerTickLines, setTimelineInnerTickLines);
	STYLE_PROP(QColor, timelineOuterTickLines, setTimelineOuterTickLines);
	STYLE_PROP(QColor, timelineBottomLines, setTimelineBottomLines);
	STYLE_PROP(QColor, timelineToggleColor, setTimelineToggleColor);
	STYLE_PROP(QColor, timelineClipPen, setTimelineClipPen);
	STYLE_PROP(QColor, timelineUnclipPen, setTimelineUnclipPen);
	STYLE_PROP(QColor, timelineClipBrush, setTimelineClipBrush);
	STYLE_PROP(QColor, timelineSelectedClip, setTimelineSelectedClip);
	STYLE_PROP(QColor, timelineSelectedClipFocused, setTimelineSelectedClipFocused);
	STYLE_PROP(QColor, timelineTreeLines, setTimelineTreeLines);
	STYLE_PROP(QColor, timelineTreeText, setTimelineTreeText);
	STYLE_PROP(QColor, timelineSplitterNormal, setTimelineSplitterNormal);
	STYLE_PROP(QColor, timelineSplitterSelected, setTimelineSplitterSelected);
	STYLE_PROP(QColor, timelineSplitterMoving, setTimelineSplitterMoving);
	STYLE_PROP(QColor, timelineSelectionLines, setTimelineSelectionLines);
	STYLE_PROP(QColor, timelineSelectionLinesFocused, setTimelineSelectionLinesFocused);

	// Curve editor
	STYLE_PROP(QColor, curveRangeHighlight, setCurveRangeHighlight);
	STYLE_PROP(QColor, curveExtrapolated, setCurveExtrapolated);
	STYLE_PROP(QColor, curveGrid, setCurveGrid);
	STYLE_PROP(QColor, curveText, setCurveText);
	STYLE_PROP(QColor, curveTangent, setCurveTangent);
	STYLE_PROP(QColor, curvePoint, setCurvePoint);
	STYLE_PROP(QColor, curveSelectedPoint, setCurveSelectedPoint);

	// Drawing primitives
	STYLE_PROP(QColor, rulerBackground, setRulerBackground);
	STYLE_PROP(QColor, rulerInnerTicks, setRulerInnerTicks);
	STYLE_PROP(QColor, rulerOuterTicks, setRulerOuterTicks);
	STYLE_PROP(QColor, rulerInnerTickText, setRulerInnerTickText);
	STYLE_PROP(QColor, rulerOuterTickText, setRulerOuterTickText);
	STYLE_PROP(QColor, timeSliderBackground, setTimeSliderBackground);
	STYLE_PROP(QColor, timeSliderBackgroundFocused, setTimeSliderBackgroundFocused);
	STYLE_PROP(QColor, timeSliderText, setTimeSliderText);
	STYLE_PROP(QColor, timeSliderLine, setTimeSliderLine);
	STYLE_PROP(QColor, timeSliderLineBase, setTimeSliderLineBase);

	// Particle Editor
	STYLE_PROP(QColor, particleEdge, setParticleEdge);
	STYLE_PROP(QColor, particleEdgeDisabled, setParticleEdgeDisabled);
	STYLE_PROP(QColor, particleText, setParticleText);
	STYLE_PROP(QColor, particleTextDisabled, setParticleTextDisabled);

	// QPalette color roles
	STYLE_PROP(QColor, windowColor, setWindowColor);
	STYLE_PROP(QColor, disabledWindowColor, setDisabledWindowColor);
	STYLE_PROP(QColor, windowTextColor, setWindowTextColor);
	STYLE_PROP(QColor, disabledWindowTextColor, setDisabledWindowTextColor);
	STYLE_PROP(QColor, baseColor, setBaseColor);
	STYLE_PROP(QColor, disabledBaseColor, setDisabledBaseColor);
	STYLE_PROP(QColor, alternateBaseColor, setAlternateBaseColor);
	STYLE_PROP(QColor, disabledAlternateBaseColor, setDisabledAlternateBaseColor);
	STYLE_PROP(QColor, toolTipBaseColor, setToolTipBaseColor);
	STYLE_PROP(QColor, disabledToolTipBaseColor, setDisabledToolTipBaseColor);
	STYLE_PROP(QColor, toolTipTextColor, setToolTipTextColor);
	STYLE_PROP(QColor, disabledToolTipTextColor, setDisabledToolTipTextColor);
	STYLE_PROP(int, toolTipMarginTop, setToolTipMarginTop);
	STYLE_PROP(int, toolTipMarginLeft, setToolTipMarginLeft);
	STYLE_PROP(int, toolTipMarginRight, setToolTipMarginRight);
	STYLE_PROP(int, toolTipMarginBottom, setToolTipMarginBottom);
	STYLE_PROP(QColor, textColor, setTextColor);
	STYLE_PROP(QColor, disabledTextColor, setDisabledTextColor);
	STYLE_PROP(QColor, buttonColor, setButtonColor);
	STYLE_PROP(QColor, disabledButtonColor, setDisabledButtonColor);
	STYLE_PROP(QColor, buttonTextColor, setButtonTextColor);
	STYLE_PROP(QColor, disabledButtonTextColor, setDisabledButtonTextColor);
	STYLE_PROP(QColor, brightTextColor, setBrightTextColor);
	STYLE_PROP(QColor, disabledBrightTextColor, setDisabledBrightTextColor);
	STYLE_PROP(QColor, lightColor, setLightColor);
	STYLE_PROP(QColor, disabledLightColor, setDisabledLightColor);
	STYLE_PROP(QColor, midlightColor, setMidlightColor);
	STYLE_PROP(QColor, disabledMidlightColor, setDisabledMidlightColor);
	STYLE_PROP(QColor, darkColor, setDarkColor);
	STYLE_PROP(QColor, disabledDarkColor, setDisabledDarkColor);
	STYLE_PROP(QColor, midColor, setMidColor);
	STYLE_PROP(QColor, disabledMidColor, setDisabledMidColor);
	STYLE_PROP(QColor, shadowColor, setShadowColor);
	STYLE_PROP(QColor, disabledShadowColor, setDisabledShadowColor);
	STYLE_PROP(QColor, highlightColor, setHighlightColor);
	STYLE_PROP(QColor, disabledHighlightColor, setDisabledHighlightColor);
	STYLE_PROP(QColor, highlightedTextColor, setHighlightedTextColor);
	STYLE_PROP(QColor, disabledHighlightedTextColor, setDisabledHighlightedTextColor);
	STYLE_PROP(QColor, linkColor, setLinkColor);
	STYLE_PROP(QColor, disabledLinkColor, setDisabledLinkColor);
	STYLE_PROP(QColor, linkVisitedColor, setLinkVisitedColor);
	STYLE_PROP(QColor, disabledLinkVisitedColor, setDisabledLinkVisitedColor);

	// Utility methods
	void     setPaletteFromColor(QColor c);
	Q_PROPERTY(QColor paletteFromColor READ buttonColor WRITE setPaletteFromColor);
	QPalette palette();
	QColor   paletteColor(QPalette::ColorGroup group, QPalette::ColorRole role);
	QColor   paletteColor(QPalette::ColorRole role);
	QColor   disabledColor(QPalette::ColorRole role);
};

#undef STYLE_PROP

EDITOR_COMMON_API EditorStyleHelper* GetStyleHelper();

