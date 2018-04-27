// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform_impl.inl>
#include "IEditorClassFactory.h"
#include "ICommandManager.h"
#include "IPlugin.h"
#include "IResourceSelectorHost.h"
#include "AnimationCompressionManager.h"
#include "CharacterTool/CharacterToolForm.h"
#include "QtViewPane.h"

// just for CGFContent:
#include <CryRenderer/VertexFormats.h>
#include <CryMath/Cry_Geo.h>
#include <CryCore/TypeInfo_impl.h>
#include <CryCore/Common_TypeInfo.h>
#include <Cry3DEngine/IIndexedMesh_info.h>
#include <Cry3DEngine/CGF/CGFContent_info.h>
// ^^^

#include "Serialization.h"

#include "CharacterTool/CharacterToolForm.h"
#include "CharacterTool/CharacterToolSystem.h"


void Log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	GetIEditor()->GetSystem()->GetILog()->LogV(ILog::eAlways, format, args);
	va_end(args);
}


CharacterTool::System* g_pCharacterToolSystem;

// ---------------------------------------------------------------------------

class CEditorAnimationPlugin : public IPlugin
{
public:
	CEditorAnimationPlugin()
	{
		g_pCharacterToolSystem = new CharacterTool::System();
		g_pCharacterToolSystem->Initialize();

		const ICVar* pUseImgCafCVar = gEnv->pConsole->GetCVar("ca_UseIMG_CAF");
		const bool useImgCafSet = (pUseImgCafCVar && pUseImgCafCVar->GetIVal());
		if (useImgCafSet)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[EditorAnimation] Animation compilation disabled: 'ca_UseIMG_CAF' should be set to zero at load time for compilation to work.");
		}
		else
		{
			g_pCharacterToolSystem->animationCompressionManager.reset(new CAnimationCompressionManager());
		}
	}

	~CEditorAnimationPlugin()
	{
		if (g_pCharacterToolSystem)
		{
			delete g_pCharacterToolSystem;
			g_pCharacterToolSystem = 0;
		}
	}

	// implements IPlugin
	int32       GetPluginVersion()                        { return 0x01; }
	const char* GetPluginName()                           { return "Editor Animation"; }
	const char* GetPluginDescription()					  { return "Animation tools and Character Tool"; }
};

//////////////////////////////////////////////////////////////////////////-

REGISTER_PLUGIN(CEditorAnimationPlugin);
