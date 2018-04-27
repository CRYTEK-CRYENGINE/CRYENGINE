// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
// Configuration option names - descriptions below are based on default implementations
// For default values, refer to the places these are used.
// If true, an image will be loaded from resources and used for a dragging handle in multi-tab areas.
#define QTWM_AREA_IMAGE_HANDLE "areaUseImageHandle"
// This should provide a path which will be loaded into a QImage to generate area image handle
#define QTWM_AREA_IMAGE_HANDLE_IMAGE "areaImageHandle"
// If true, shows Drag Handle on mutli-tab areas
#define QTWM_AREA_SHOW_DRAG_HANDLE "areaImageHandle"
// If true, sets document mode on areas.
#define QTWM_AREA_DOCUMENT_MODE "areaUseDocumentMode"
// If true, tabs have a close button.
#define QTWM_AREA_TABS_CLOSABLE "areaTabsClosable"
// Contains an enum of type QTabWidget::TabPosition that dictates where the tabs will be shown. Default is QTabWidget::North
#define QTWM_AREA_TAB_POSITION "areaTabPosition"
// If true, complete frames may be dragged from the empty space between the drag handle and the actual tabs, otherwise only the drag handle will work.
#define QTWM_AREA_EMPTY_SPACE_DRAG "areaAllowFrameDragFromEmptySpace"
// Number of milliseconds between requesting a refresh of thumbnails.
#define QTWM_THUMBNAIL_TIMER_INTERVAL "thumbnailTimerInterval"
// QPoint specifying an offset relative to the mouse cursor for tooltips.
#define QTWM_TOOLTIP_OFFSET "tooltipOffset"
// If true, add the window icon of a widget to its tab.
#define QTWM_AREA_TAB_ICONS "areaTabIcons"
// Policy on whether to forget widgets when they are released. See the QTWMReleaseCachingPolicy enum for details.
#define QTWM_RELEASE_POLICY "releasePolicy"
// If true, widgets always have their close method called when hidden. If false, the close method is only called if the widget will be forgotten.
#define QTWM_ALWAYS_CLOSE_WIDGETS "alwaysCloseWidgets"
// If true, wrappers behave as tool windows that always stay on top of the main window; otherwise they have no parent and may be hidden behind the main window.
#define QTWM_WRAPPERS_ARE_CHILDREN "wrappersAreChildren"
// During a drag, the currently hovered area will be brought to the top of the window stack after this many milliseconds. Disabled if <= 0.
#define QTWM_RAISE_DELAY "raiseDelay"
// If true, a wrapper which only contains a single area with a single tab will have its title updated to match that tab.
#define QTWM_RETITLE_WRAPPER "retitleWrapper"
// If true, an area containing only a single tab will have a special frame in place of the tab bar.
#define QTWM_SINGLE_TAB_FRAME "singleTabFrame"
// If true, all windows in the application will be brought to the front when the user switches from another application to this.
// Only used if QTWM_WRAPPERS_ARE_CHILDREN is false.
#define QTWM_BRING_ALL_TO_FRONT "bringAllToFront"
// If true, only immediate neighbors will be given space from a widget that gets removed from a splitter.
#define QTWM_PRESERVE_SPLITTER_SIZES "preserveSplitterSizes"
// If true, together with tabs tools can be docked also to rollup bars
#define QTWM_SUPPORT_SIMPLE_TOOLS "supportSimpleTools"
// These should provide a QIcon. They are included in the configuration to allow for wrapping, custom paths, etc.
#define QTWM_TAB_CLOSE_ICON "tabCloseIcon"
#define QTWM_SINGLE_TAB_FRAME_CLOSE_ICON "tabFrameCloseIcon"

// These should provide a path which will be loaded into a QImage to generate the drop target graphics.
#define QTWM_DROPTARGET_TOP "droptargetTop"
#define QTWM_DROPTARGET_BOTTOM "droptargetBottom"
#define QTWM_DROPTARGET_LEFT "droptargetLeft"
#define QTWM_DROPTARGET_RIGHT "droptargetRight"
#define QTWM_DROPTARGET_SPLIT_TOP "droptargetSplitTop"
#define QTWM_DROPTARGET_SPLIT_BOTTOM "droptargetSplitBottom"
#define QTWM_DROPTARGET_SPLIT_LEFT "droptargetSplitLeft"
#define QTWM_DROPTARGET_SPLIT_RIGHT "droptargetSplitRight"
#define QTWM_DROPTARGET_COMBINE "droptargetCombine"

#include <qglobal.h> // need to include here, so we know QT_VERSION
#if QT_VERSION <= 0x050000
#include <QVariant>
#define Q_DECL_OVERRIDE
#endif

#ifndef QTWM_DLL
#define QTWM_DLL
#endif // ! QTWM_DLL



#if defined(_WIN32) && defined(QTWM_DLL)
#ifdef QToolWindowManager_EXPORTS
#define QTOOLWINDOWMANAGER_EXPORT __declspec(dllexport)
#else
#define QTOOLWINDOWMANAGER_EXPORT __declspec(dllimport)
#endif
#else
#define QTOOLWINDOWMANAGER_EXPORT
#endif

enum QTWMReleaseCachingPolicy
{
	rcpKeep,  // Always keep widgets in internal cache, even after closing them.
	rcpWidget,// Close and forget widgets that have the attribute WA_DeleteOnClose set.
	rcpForget,// Always close and forget widgets when their tabs are closed.
	rcpDelete // Always close widgets when their tabs are closed, and call deleteLater on them.
};

enum QTWMWrapperAreaType
{
	watTabs,
	watRollups
};

enum QTWMToolType
{
	ttStandard,
	ttSimple
};


#include <QWidget>
#include <QMainWindow>
#include <QApplication>

template<class T>
T findClosestParent(QWidget* widget)
{
	while (widget)
	{
		if (qobject_cast<T>(widget))
		{
			return qobject_cast<T>(widget);
		}
		widget = widget->parentWidget();
	}
	return 0;
}

template<class T>
T findFurthestParent(QWidget* widget)
{
	QWidget* result = nullptr;
	while (widget)
	{
		if (qobject_cast<T>(widget))
		{
			result = widget;
		}
		widget = widget->parentWidget();
	}
	return qobject_cast<T>(result);
}

QTOOLWINDOWMANAGER_EXPORT void registerMainWindow(QMainWindow* w);
QTOOLWINDOWMANAGER_EXPORT QMainWindow* getMainWindow();

QTOOLWINDOWMANAGER_EXPORT QWidget* windowBelow(QWidget* w);

QTOOLWINDOWMANAGER_EXPORT QIcon getIcon(const QVariantMap& config, QString key, QIcon fallback);

QTOOLWINDOWMANAGER_EXPORT bool configHasValue(const QVariantMap& config, QString key);

#define CALL_PROTECTED_VOID_METHOD_1ARG(_class, _instancepointer, _method, _arg1type, _arg1)\
{class newClass : public _class\
{\
public:\
	void _method(_arg1type a1)\
	{\
		_class::_method(a1);\
	}\
};\
((newClass*)_instancepointer)->_method(_arg1);}
