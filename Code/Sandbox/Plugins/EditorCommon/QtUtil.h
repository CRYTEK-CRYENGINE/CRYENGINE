// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QString>
#include <QColor>
#include <QSize>
#include <QPoint>
#include <QScrollArea>
#include <QStyle>

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>
#include <CryString/CryString.h>
#include <CryString/UnicodeFunctions.h>
#include "EditorCommonAPI.h"

class QAbstractItemView;
class QAbstractItemModel;
class QWidget;
class QObject;
class QLayout;
class QMenu;
class QModelIndex;
class CMergingProxyModel;

namespace QtUtil
{
// From QString to CryString
inline CryStringT<char> ToString(const QString& str)
{
	return Unicode::Convert<CryStringT<char>>(str);
}

// From CryString to QString
inline QString ToQString(const CryStringT<char>& str)
{
	return Unicode::Convert<QString>(str);
}

// From const char * to QString
inline QString ToQString(const char* str, size_t len = -1)
{
	if (len == -1) len = strlen(str);
	return Unicode::Convert<QString>(str, str + len);
}
// From CryString to QString.
// Suitable for user-input or potentially invalid strings.
inline QString ToQStringSafe(const CryStringT<char>& str)
{
	return Unicode::ConvertSafe<Unicode::eErrorRecovery_FallbackWin1252ThenReplace, QString>(str);
}

// From const char * to QString.
// Suitable for user-input or potentially invalid strings.
inline QString ToQStringSafe(const char* str, size_t len = -1)
{
	if (len == -1) len = strlen(str);
	return Unicode::ConvertSafe<Unicode::eErrorRecovery_FallbackWin1252ThenReplace, QString>(str, str + len);
}

// Cry ColorB to Qt Color
inline QColor ToQColor(const ColorB& color)
{
	return QColor(color.r, color.g, color.b, color.a);
}

// Cry ColorF to Qt Color
inline QColor ToQColor(const ColorF& color)
{
	return QColor(color.r * 255, color.g * 255, color.b * 255, color.a * 255);
}

inline ColorB ToColorB(const QColor& color)
{
	return ColorB(color.red(), color.green(), color.blue(), color.alpha());
}

inline ColorB ToColorF(const QColor& color)
{
	return ColorF(color.red() / 255.f, color.green() / 255.f, color.blue() / 255.f, color.alpha() / 255.f);
}

// interpolate between two Qt colors
inline QColor InterpolateColor(const QColor& a, const QColor& b, float k)
{
	float mk = 1.0f - k;
	return QColor(a.red() * mk + b.red() * k,
	              a.green() * mk + b.green() * k,
	              a.blue() * mk + b.blue() * k,
	              a.alpha() * mk + b.alpha() * k);
}

// Desaturate a Qt Color
inline QColor DesaturateColor(QColor color)
{
	const float average = (color.redF() + color.greenF() + color.blueF()) * (1.0f / 3.0f);
	return QColor(average * 255.0f, average * 255.0f, average * 255.0f);
}

EDITOR_COMMON_API void       DrawStatePixmap(QPainter* painter, const QRect& iconRect, const QPixmap& pixmap, QStyle::State state);

EDITOR_COMMON_API QVariant   ToQVariant(const QSize& size);
EDITOR_COMMON_API QSize      ToQSize(const QVariant& variant);
EDITOR_COMMON_API QVariant   ToQVariant(const QByteArray& byteArray);
EDITOR_COMMON_API QByteArray ToQByteArray(const QVariant& variant);

EDITOR_COMMON_API QString    GetAppDataFolder();

EDITOR_COMMON_API bool       IsModifierKeyDown(int modifier);
EDITOR_COMMON_API bool       IsMouseButtonDown(int button);

//helper to format code identifiers for UI display
EDITOR_COMMON_API QString CamelCaseToUIString(const char* camelCaseStr);

//will return the mouse position in coordinates relative to the widget
EDITOR_COMMON_API QPoint       GetMousePosition(QWidget* widget);

EDITOR_COMMON_API QPoint       GetMouseScreenPosition();

EDITOR_COMMON_API bool         IsParentOf(QObject* parent, QObject* child);

EDITOR_COMMON_API QScrollArea* MakeScrollable(QWidget* widget);
EDITOR_COMMON_API QScrollArea* MakeScrollable(QLayout* widget);

//! Opens an Explorer/Finder window at specific location, selects file if exists
EDITOR_COMMON_API void         OpenInExplorer(const char* path);
//! Lets the operating system open the file for edit with the associated application
EDITOR_COMMON_API void		   OpenFileForEdit(const char* filePath);

EDITOR_COMMON_API void         RecursiveInstallEventFilter(QWidget* pListener, QWidget* pWatched);

//! Will create the menu action based on the path separated by '/'. Last section of the path will be the action text.
EDITOR_COMMON_API QAction* AddActionFromPath(const QString& menuPath, QMenu* parentMenu);

//! Converts a value from Qt scale to pixel scale.
EDITOR_COMMON_API int PixelScale(const QWidget* widget, int qtValue);

//! Converts a value from Qt scale to pixel scale.
EDITOR_COMMON_API float PixelScale(const QWidget* widget, float qtValue);

//! Converts a value from Qt scale to pixel scale.
EDITOR_COMMON_API qreal PixelScale(const QWidget* widget, qreal qtValue);

//! Scales a QPoint from Qt scale to Pixel scale.
EDITOR_COMMON_API QPoint PixelScale(const QWidget* widget, const QPoint& qtValue);

//! Scales a QPointF from Qt scale to Pixel scale.
EDITOR_COMMON_API QPointF PixelScale(const QWidget* widget, const QPointF& qtValue);

//! Converts a value from pixel scale to Qt scale.
EDITOR_COMMON_API int QtScale(const QWidget* widget, int pixelValue);

//! Converts a value from pixel scale to Qt scale.
EDITOR_COMMON_API float QtScale(const QWidget* widget, float pixelValue);

//! Converts a value from pixel scale to Qt scale.
EDITOR_COMMON_API qreal QtScale(const QWidget* widget, qreal pixelValue);

//! Scales a QPoint from pixel scale to Qt scale.
EDITOR_COMMON_API QPoint QtScale(const QWidget* widget, const QPoint& pixelValue);

//! Scales a QPointF from pixel scale to Qt scale.
EDITOR_COMMON_API QPointF QtScale(const QWidget* widget, const QPointF& pixelValue);

//! Application-wide hiding of mouse. The system uses reference counting internally
EDITOR_COMMON_API void HideApplicationMouse();
EDITOR_COMMON_API void ShowApplicationMouse();

//! Generalized version of QAbstractProxyModel::mapFromSource.
//! This function returns true, if there exists a path of proxy models p0 -> p1 -> ... pN, such that
//! 1. p(i+1) is a source model of p(i)
//! 2. p0 = pProxy
//! 3. index.model() is a source model of pN.
//! 4. Each p(i) is a sub-class of either QAbtractProxyModel or CMergingProxyModel.
//! If true is returned, \p out is a model index of \p pProxy that corresponds to the source model
//! index \p index.
//! Basically, \p out this is found by a repeated application of mapFromSource.
EDITOR_COMMON_API bool MapFromSourceIndirect(const QAbstractItemModel* pProxyModel, const QModelIndex& sourceIndexIn, QModelIndex& proxyIndexOut);

//! Other implementation of MapFromSourceIndirect which takes a view as parameter.
//! Takes a source model index and returns an index in the view's model
EDITOR_COMMON_API bool MapFromSourceIndirect(const QAbstractItemView* pView, const QModelIndex& sourceIndexIn, QModelIndex& viewIndexOut);
}

