// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef GLOBAL_H
#define GLOBAL_H

#undef QT_NO_STL
#undef QT_NO_STL_WCHAR

#ifndef _WIN64
#define _WIN64
#endif

#ifndef _WIN32
#define _WIN32
#endif

#ifndef NULL
#define NULL    0
#endif
#define QT_QTWINMIGRATE_EXPORT

#define QTOOLWINDOWMANAGER_EXPORT

// Include both pyside and pyside 2 headers. The one not used will be ignored.
#include "pyside2_global.h"
#include "pyside_global.h"

// Must include all .h files for classes we want to wrap.
#include "CryIcon.h"
#include "QCollapsibleFrame.h"
#include "QRollupBar.h"
#include "QScrollableBox.h"
#include "QToolWindowManager/IToolWindowArea.h"
#include "QToolWindowManager/IToolWindowDragHandler.h"
#include "QToolWindowManager/IToolWindowWrapper.h"
#include "QToolWindowManager/QCustomWindowFrame.h"
#include "QToolWindowManager/QToolWindowArea.h"
#include "QToolWindowManager/QToolWindowRollupBarArea.h"
#include "QToolWindowManager/QToolWindowCustomWrapper.h"
#include "QToolWindowManager/QToolWindowDragHandlerDropTargets.h"
#include "QToolWindowManager/QToolWindowDragHandlerNinePatch.h"
#include "QToolWindowManager/QToolWindowManager.h"
#include "QToolWindowManager/QToolWindowManagerCommon.h"
#include "QToolWindowManager/QToolWindowTabBar.h"
#include "QToolWindowManager/QToolWindowWrapper.h"


#endif // GLOBAL_H

