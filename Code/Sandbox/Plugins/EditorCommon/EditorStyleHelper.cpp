// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorStyleHelper.h"

QColor EditorStyleHelper::paletteColor(QPalette::ColorGroup group, QPalette::ColorRole role)
{
	if (group == QPalette::Disabled)
		return disabledColor(role);
	return paletteColor(role);
}

QColor EditorStyleHelper::paletteColor(QPalette::ColorRole role)
{
	switch (role)
	{
	case QPalette::WindowText:
		return m_windowTextColor;
	case QPalette::Button:
		return m_buttonColor;
	case QPalette::Light:
		return m_lightColor;
	case QPalette::Midlight:
		return m_midlightColor;
	case QPalette::Dark:
		return m_darkColor;
	case QPalette::Mid:
		return m_midColor;
	case QPalette::Text:
		return m_textColor;
	case QPalette::BrightText:
		return m_brightTextColor;
	case QPalette::ButtonText:
		return m_buttonTextColor;
	case QPalette::Base:
		return m_baseColor;
	case QPalette::Window:
		return m_windowColor;
	case QPalette::Shadow:
		return m_shadowColor;
	case QPalette::Highlight:
		return m_highlightColor;
	case QPalette::HighlightedText:
		return m_highlightedTextColor;
	case QPalette::Link:
		return m_linkColor;
	case QPalette::LinkVisited:
		return m_linkVisitedColor;
	case QPalette::AlternateBase:
		return m_alternateBaseColor;
	default:
		qWarning("Called EditorStyleHelper::paletteColor with unknown argument");
		return QColor(255, 0, 255);
	}
}

QColor EditorStyleHelper::disabledColor(QPalette::ColorRole role)
{
	switch (role)
	{
	case QPalette::WindowText:
		return m_disabledWindowTextColor;
	case QPalette::Button:
		return m_disabledButtonColor;
	case QPalette::Light:
		return m_disabledLightColor;
	case QPalette::Midlight:
		return m_disabledMidlightColor;
	case QPalette::Dark:
		return m_disabledDarkColor;
	case QPalette::Mid:
		return m_disabledMidColor;
	case QPalette::Text:
		return m_disabledTextColor;
	case QPalette::BrightText:
		return m_disabledBrightTextColor;
	case QPalette::ButtonText:
		return m_disabledButtonTextColor;
	case QPalette::Base:
		return m_disabledBaseColor;
	case QPalette::Window:
		return m_disabledWindowColor;
	case QPalette::Shadow:
		return m_disabledShadowColor;
	case QPalette::Highlight:
		return m_disabledHighlightColor;
	case QPalette::HighlightedText:
		return m_disabledHighlightedTextColor;
	case QPalette::Link:
		return m_disabledLinkColor;
	case QPalette::LinkVisited:
		return m_disabledLinkVisitedColor;
	case QPalette::AlternateBase:
		return m_disabledAlternateBaseColor;
	default:
		qWarning("Called EditorStyleHelper::disabledColor with unknown argument");
		return QColor(255, 0, 255);
	}
}

void EditorStyleHelper::setPaletteFromColor(QColor c)
{
	QPalette p(c);
	m_windowColor = p.color(QPalette::Active, QPalette::Window);
	m_disabledWindowColor = p.color(QPalette::Disabled, QPalette::Window);
	m_windowTextColor = p.color(QPalette::Active, QPalette::WindowText);
	m_disabledWindowTextColor = p.color(QPalette::Disabled, QPalette::WindowText);
	m_baseColor = p.color(QPalette::Active, QPalette::Base);
	m_disabledBaseColor = p.color(QPalette::Disabled, QPalette::Base);
	m_alternateBaseColor = p.color(QPalette::Active, QPalette::AlternateBase);
	m_disabledAlternateBaseColor = p.color(QPalette::Disabled, QPalette::AlternateBase);
	m_toolTipBaseColor = p.color(QPalette::Active, QPalette::ToolTipBase);
	m_disabledToolTipBaseColor = p.color(QPalette::Disabled, QPalette::ToolTipBase);
	m_toolTipTextColor = p.color(QPalette::Active, QPalette::ToolTipText);
	m_disabledToolTipTextColor = p.color(QPalette::Disabled, QPalette::ToolTipText);
	m_textColor = p.color(QPalette::Active, QPalette::Text);
	m_disabledTextColor = p.color(QPalette::Disabled, QPalette::Text);
	m_buttonColor = p.color(QPalette::Active, QPalette::Button);
	m_disabledButtonColor = p.color(QPalette::Disabled, QPalette::Button);
	m_buttonTextColor = p.color(QPalette::Active, QPalette::ButtonText);
	m_disabledButtonTextColor = p.color(QPalette::Disabled, QPalette::ButtonText);
	m_brightTextColor = p.color(QPalette::Active, QPalette::BrightText);
	m_disabledBrightTextColor = p.color(QPalette::Disabled, QPalette::BrightText);
	m_lightColor = p.color(QPalette::Active, QPalette::Light);
	m_disabledLightColor = p.color(QPalette::Disabled, QPalette::Light);
	m_midlightColor = p.color(QPalette::Active, QPalette::Midlight);
	m_disabledMidlightColor = p.color(QPalette::Disabled, QPalette::Midlight);
	m_darkColor = p.color(QPalette::Active, QPalette::Dark);
	m_disabledDarkColor = p.color(QPalette::Disabled, QPalette::Dark);
	m_midColor = p.color(QPalette::Active, QPalette::Mid);
	m_disabledMidColor = p.color(QPalette::Disabled, QPalette::Mid);
	m_shadowColor = p.color(QPalette::Active, QPalette::Shadow);
	m_disabledShadowColor = p.color(QPalette::Disabled, QPalette::Shadow);
	m_highlightColor = p.color(QPalette::Active, QPalette::Highlight);
	m_disabledHighlightColor = p.color(QPalette::Disabled, QPalette::Highlight);
	m_highlightedTextColor = p.color(QPalette::Active, QPalette::HighlightedText);
	m_disabledHighlightedTextColor = p.color(QPalette::Disabled, QPalette::HighlightedText);
	m_linkColor = p.color(QPalette::Active, QPalette::Link);
	m_disabledLinkColor = p.color(QPalette::Disabled, QPalette::Link);
	m_linkVisitedColor = p.color(QPalette::Active, QPalette::LinkVisited);
	m_disabledLinkVisitedColor = p.color(QPalette::Disabled, QPalette::LinkVisited);
}

QPalette EditorStyleHelper::palette()
{
	QPalette p;
	p.setColor(QPalette::Window, m_windowColor);
	p.setColor(QPalette::Disabled, QPalette::Window, m_disabledWindowColor);
	p.setColor(QPalette::WindowText, m_windowTextColor);
	p.setColor(QPalette::Disabled, QPalette::WindowText, m_disabledWindowTextColor);
	p.setColor(QPalette::Base, m_baseColor);
	p.setColor(QPalette::Disabled, QPalette::Base, m_disabledBaseColor);
	p.setColor(QPalette::AlternateBase, m_alternateBaseColor);
	p.setColor(QPalette::Disabled, QPalette::AlternateBase, m_disabledAlternateBaseColor);
	p.setColor(QPalette::ToolTipBase, m_toolTipBaseColor);
	p.setColor(QPalette::Disabled, QPalette::ToolTipBase, m_disabledToolTipBaseColor);
	p.setColor(QPalette::ToolTipText, m_toolTipTextColor);
	p.setColor(QPalette::Disabled, QPalette::ToolTipText, m_disabledToolTipTextColor);
	p.setColor(QPalette::Text, m_textColor);
	p.setColor(QPalette::Disabled, QPalette::Text, m_disabledTextColor);
	p.setColor(QPalette::Button, m_buttonColor);
	p.setColor(QPalette::Disabled, QPalette::Button, m_disabledButtonColor);
	p.setColor(QPalette::ButtonText, m_buttonTextColor);
	p.setColor(QPalette::Disabled, QPalette::ButtonText, m_disabledButtonTextColor);
	p.setColor(QPalette::BrightText, m_brightTextColor);
	p.setColor(QPalette::Disabled, QPalette::BrightText, m_disabledBrightTextColor);
	p.setColor(QPalette::Light, m_lightColor);
	p.setColor(QPalette::Disabled, QPalette::Light, m_disabledLightColor);
	p.setColor(QPalette::Midlight, m_midlightColor);
	p.setColor(QPalette::Disabled, QPalette::Midlight, m_disabledMidlightColor);
	p.setColor(QPalette::Dark, m_darkColor);
	p.setColor(QPalette::Disabled, QPalette::Dark, m_disabledDarkColor);
	p.setColor(QPalette::Mid, m_midColor);
	p.setColor(QPalette::Disabled, QPalette::Mid, m_disabledMidColor);
	p.setColor(QPalette::Shadow, m_shadowColor);
	p.setColor(QPalette::Disabled, QPalette::Shadow, m_disabledShadowColor);
	p.setColor(QPalette::Highlight, m_highlightColor);
	p.setColor(QPalette::Disabled, QPalette::Highlight, m_disabledHighlightColor);
	p.setColor(QPalette::HighlightedText, m_highlightedTextColor);
	p.setColor(QPalette::Disabled, QPalette::HighlightedText, m_disabledHighlightedTextColor);
	p.setColor(QPalette::Link, m_linkColor);
	p.setColor(QPalette::Disabled, QPalette::Link, m_disabledLinkColor);
	p.setColor(QPalette::LinkVisited, m_linkVisitedColor);
	p.setColor(QPalette::Disabled, QPalette::LinkVisited, m_disabledLinkVisitedColor);
	return p;
}

EDITOR_COMMON_API EditorStyleHelper* GetStyleHelper()
{
	static EditorStyleHelper* s_instance = new EditorStyleHelper();
	s_instance->ensurePolished();
	return s_instance;
}

