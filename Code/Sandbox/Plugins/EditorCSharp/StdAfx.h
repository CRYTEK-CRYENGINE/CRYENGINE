// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#include <CryCore/Platform/platform.h>

//TODO : remove this, but there are many compilation errors. The particle editor should not include MFC.
#define CRY_USE_MFC
#include <CryCore/Platform/CryAtlMfc.h>

#include "EditorCommon.h"
#include "IEditor.h"
#include "IPlugin.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderer.h>
#include "Expected.h"

#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsLinearLayout>
#include <QGraphicsGridLayout>
#include <QGraphicsWidget>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QStyleOption>
#include <QMenu>
#include <QVariant>
#include <QTextCursor>
#include <QDrag>
#include <QMimeData>
#include <QClipboard>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushbutton>
#include <QToolButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpacerItem>
#include <QSplitter>
#include <QFileDialog>

#include "QtUtil.h"
#include "QtViewPane.h"
#include "Serialization.h"
#include "UndoStack.h"

#include <Serialization/QPropertyTree/QPropertyTree.h>
#include "IResourceSelectorHost.h"

IEditor*               GetIEditor();
