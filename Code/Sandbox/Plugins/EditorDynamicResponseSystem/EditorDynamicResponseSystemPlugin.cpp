// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "EditorDynamicResponseSystemPlugin.h"
#include "DrsEditorMainWindow.h"

#include <IResourceSelectorHost.h>

#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>

#include <CryCore/Platform/platform_impl.inl>
#include <IEditor.h>
#include <CrySystem/ISystem.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>

REGISTER_PLUGIN(CEditorDynamicResponseSystemPlugin);

CEditorDynamicResponseSystemPlugin::CEditorDynamicResponseSystemPlugin()
{
	CRY_ASSERT(gEnv->pDynamicResponseSystem->GetResponseManager());
}
