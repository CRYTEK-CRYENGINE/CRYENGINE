// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/SharedData.h"

namespace ACE
{
namespace Impl
{
struct IImpl;
} // namespace Impl

class CAssetsManager;
class CContextManager;
class CImplManager;
class CListenerManager;
class CAsset;
class CControl;
class CLibrary;
class CFolder;
class CContext;
class CMainWindow;
class CSystemControlsWidget;
class CPropertiesWidget;
class CMiddlewareDataWidget;
class CFileMonitorMiddleware;
class CContextWidget;
class CNameValidator;

using Assets = std::vector<CAsset*>;
using Controls = std::vector<CControl*>;
using Libraries = std::vector<CLibrary*>;
using Folders = std::vector<CFolder*>;
using FileNames = std::set<string>;
using AssetNames = std::vector<string>;

using Contexts = std::vector<CContext*>;
extern Contexts g_contexts;

using ContextIds = std::vector<CryAudio::ContextId>;
extern ContextIds g_activeUserDefinedContexts;

extern CAssetsManager g_assetsManager;
extern CContextManager g_contextManager;
extern CImplManager g_implManager;
extern CListenerManager g_listenerManager;
extern Impl::IImpl* g_pIImpl;
extern CMainWindow* g_pMainWindow;
extern CSystemControlsWidget* g_pSystemControlsWidget;
extern CPropertiesWidget* g_pPropertiesWidget;
extern CMiddlewareDataWidget* g_pMiddlewareDataWidget;
extern CFileMonitorMiddleware* g_pFileMonitorMiddleware;
extern CContextWidget* g_pContextWidget;
extern CNameValidator g_nameValidator;

extern SImplInfo g_implInfo;

constexpr uint8 g_currentFileVersion = 5;

constexpr char const* g_szEditorName = "Audio Controls Editor";
constexpr char const* g_szLibraryNodeTag = "Library";
constexpr char const* g_szFoldersNodeTag = "Folders";
constexpr char const* g_szControlsNodeTag = "Controls";
constexpr char const* g_szFolderTag = "Folder";
constexpr char const* g_szPathAttribute = "path";
constexpr char const* g_szDescriptionAttribute = "description";

extern ControlIds g_importedItemIds;
} // namespace ACE
