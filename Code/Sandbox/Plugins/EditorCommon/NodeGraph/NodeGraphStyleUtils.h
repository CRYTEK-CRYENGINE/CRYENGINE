// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QColor>
#include <QSize>
#include <QPixmap>
#include <QRectF>

namespace CryGraphEditor {

namespace StyleUtils {

QIcon   CreateIcon(const char* szIcon, QColor color);
QPixmap CreateIconPixmap(const char* szIcon, QColor color, QSize size);
QPixmap CreateIconPixmap(const CryIcon& icon, QSize size);

QPixmap ColorizePixmap(QPixmap pixmap, QColor color);
QColor  ColorMuliply(QColor a, QColor b);

}

}

inline QRectF& operator+=(QRectF& a, const QRectF& b)
{
	a.adjust(b.left(), b.top(), b.right(), b.bottom());
	return a;
}

inline QRectF operator+(const QRectF& a, const QRectF& b)
{
	return a.adjusted(b.left(), b.top(), b.right(), b.bottom());
}

inline QRectF& operator+=(QRectF& a, const QRect& b)
{
	a.adjust(b.left(), b.top(), b.right(), b.bottom());
	return a;
}

inline QRectF operator+(const QRectF& a, const QRect& b)
{
	return a.adjusted(b.left(), b.top(), b.right(), b.bottom());
}

