// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeGraphStyleUtils.h"

namespace CryGraphEditor {

namespace StyleUtils {

QIcon CreateIcon(const char* szIcon, QColor color)
{
	return CryIcon(szIcon, {
				{ QIcon::Mode::Normal, color }
	    });
}

QPixmap CreateIconPixmap(const char* szIcon, QColor color, QSize size)
{
	return CreateIconPixmap(CreateIcon(szIcon, color), size);
}

QPixmap CreateIconPixmap(const CryIcon& icon, QSize size)
{
	return icon.pixmap(size.width(), size.height(), QIcon::Normal);
}

QPixmap ColorizePixmap(QPixmap pixmap, QColor color)
{
	const QSize size = pixmap.size();

	return CryIcon(pixmap, {
				{ QIcon::Mode::Normal, color }
	    }).pixmap(size, QIcon::Normal);
}

QColor ColorMuliply(QColor a, QColor b)
{
	QColor r;
	r.setRedF(1.0f - (1.0f - a.redF()) * (1.0f - b.redF()));
	r.setGreenF(1.0f - (1.0f - a.greenF()) * (1.0f - b.greenF()));
	r.setBlueF(1.0f - (1.0f - a.blueF()) * (1.0f - b.blueF()));
	r.setAlphaF(1.0f - (1.0f - a.alphaF()) * (1.0f - b.alphaF()));
	return r;
}

}

}

