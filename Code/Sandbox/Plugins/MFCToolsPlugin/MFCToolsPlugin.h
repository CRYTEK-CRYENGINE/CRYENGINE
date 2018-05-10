// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//As this is not initialized as a real plugin, it needs to behave similarly to EditorCommon and be initialized manually at the right time
namespace MFCToolsPlugin
{
	//!Sets editor and gEnv, needs both to be initialized
	PLUGIN_API void SetEditor(IEditor* editor);

	PLUGIN_API void Initialize();
	PLUGIN_API void Deinitialize();
}
