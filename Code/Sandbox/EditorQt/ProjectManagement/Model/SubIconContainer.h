// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QMetaType>
#include <array>

// Container with sub-icons of project languages
class CSubIconContainer
{
public:
	CSubIconContainer();

	QVariantList GetIcons(const string& language, bool isStartupProject) const;

private:
	std::array<QVariant, 4> m_languageIcons;
	QVariant                m_startupIcon;
};
