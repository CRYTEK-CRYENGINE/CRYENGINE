// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "EditorCommonInit.h"
#include "IPlugin.h"

#include <CryCore/Platform/platform_impl.inl>

#include <QMetaType>
#include "ProxyModels/ItemModelAttribute.h"

static IEditor* g_editor = nullptr;

IEditor* GetIEditor() { return g_editor; }

void EDITOR_COMMON_API EditorCommon::SetIEditor(IEditor* editor)
{
	g_editor = editor;
	auto system = GetIEditor()->GetSystem();
	gEnv = system->GetGlobalEnvironment();
}

void EDITOR_COMMON_API EditorCommon::Initialize()
{
	ModuleInitISystem(GetIEditor()->GetSystem(), "EditorCommon");

	RegisterPlugin();

	QMetaType::registerComparators<CItemModelAttribute*>();
}

void EDITOR_COMMON_API EditorCommon::Deinitialize()
{
	UnregisterPlugin();
	g_editor = nullptr;
	gEnv = nullptr;
}

