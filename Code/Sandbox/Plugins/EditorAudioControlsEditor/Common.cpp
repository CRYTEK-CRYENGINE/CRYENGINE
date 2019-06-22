// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Common.h"
#include "AssetsManager.h"
#include "ContextManager.h"
#include "ImplManager.h"
#include "ListenerManager.h"
#include "NameValidator.h"

namespace ACE
{
Contexts g_contexts;
ContextIds g_activeUserDefinedContexts;

CAssetsManager g_assetsManager;
CContextManager g_contextManager;
CImplManager g_implManager;
CListenerManager g_listenerManager;
Impl::IImpl* g_pIImpl = nullptr;
CMainWindow* g_pMainWindow = nullptr;
CSystemControlsWidget* g_pSystemControlsWidget = nullptr;
CPropertiesWidget* g_pPropertiesWidget = nullptr;
CMiddlewareDataWidget* g_pMiddlewareDataWidget = nullptr;
CFileMonitorMiddleware* g_pFileMonitorMiddleware = nullptr;
CContextWidget* g_pContextWidget = nullptr;
CNameValidator g_nameValidator;

SImplInfo g_implInfo;

ControlIds g_importedItemIds;
} // namespace ACE