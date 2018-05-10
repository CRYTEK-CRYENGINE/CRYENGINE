// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannequinDebug.h"
#include "ActionController.h"

#include <CryAISystem/IAIObject.h>

//////////////////////////////////////////////////////////////////////////
static void MN_ReloadAll(IConsoleCmdArgs* pArgs)
{
	CCryAction::GetCryAction()->GetMannequinInterface().ReloadAll();
}

struct SAnimAssetContext
{
	XmlNodeRef                xmlRoot;
	const IAnimationDatabase* pDatabase;
};

void AssetCallback(const SAnimAssetReport& assetReport, void* _context)
{
	SAnimAssetContext* pContext = (SAnimAssetContext*)_context;
	XmlNodeRef xmlAnim = GetISystem()->CreateXmlNode("Anim");
	if (assetReport.pAnimName != NULL)
	{
		xmlAnim->setAttr("AnimID", assetReport.pAnimName);
	}
	if (assetReport.pAnimPath != NULL)
	{
		xmlAnim->setAttr("AnimPath", assetReport.pAnimPath);
	}
	pContext->xmlRoot->addChild(xmlAnim);
}

//////////////////////////////////////////////////////////////////////////
static void MN_ListAssets(IConsoleCmdArgs* pArgs)
{
	const char* filename = "mannequinAnimationAssetUsage.xml";

	if (pArgs->GetArgCount() >= 2)
	{
		filename = pArgs->GetArg(1);
	}

	SAnimAssetContext context;
	context.xmlRoot = GetISystem()->CreateXmlNode("UsedAnims");
	IAnimationDatabaseManager& dbManager = CCryAction::GetCryAction()->GetMannequinInterface().GetAnimationDatabaseManager();
	const int numDBAs = dbManager.GetTotalDatabases();
	for (int i = 0; i < numDBAs; i++)
	{
		const IAnimationDatabase* pDatabase = dbManager.GetDatabase(i);
		context.pDatabase = pDatabase;
		pDatabase->EnumerateAnimAssets(NULL, AssetCallback, &context);
	}
	context.xmlRoot->saveToFile(filename);
}

//////////////////////////////////////////////////////////////////////////
static void MN_DebugAI(IConsoleCmdArgs* pArgs)
{
	const IEntity* pDebugEntity = NULL;

	if (2 <= pArgs->GetArgCount())
	{
		const char* const entityName = pArgs->GetArg(1);
		pDebugEntity = gEnv->pEntitySystem->FindEntityByName(entityName);
	}
	else
	{
		const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
		const Matrix34& viewCameraMatrix = viewCamera.GetMatrix();
		const Vec3 cameraWorldPosition = viewCameraMatrix.GetTranslation();
		const Vec3 cameraWorldDirectionForward = viewCameraMatrix.GetColumn1().GetNormalized();

		float highestDotProduct = -2.0f;

		const CActionController::TActionControllerList& actionControllersList = CActionController::GetGlobalActionControllers();
		for (size_t i = 0; i < actionControllersList.size(); ++i)
		{
			const CActionController* pActionController = actionControllersList[i];
			CRY_ASSERT(pActionController);

			const EntityId entityId = pActionController->GetEntityId();
			if (entityId == 0)
			{
				continue;
			}

			IEntity& entity = pActionController->GetEntity();
			const IAIObject* pAiObject = entity.GetAI();

			const Vec3 entityWorldPosition = pAiObject ? pAiObject->GetPos() : entity.GetWorldPos();
			const Vec3 cameraToEntityWorldDirection = (entityWorldPosition - cameraWorldPosition).GetNormalized();
			const float dotProduct = cameraWorldDirectionForward.Dot(cameraToEntityWorldDirection);

			if ((highestDotProduct < dotProduct) && pAiObject)
			{
				pDebugEntity = &entity;
				highestDotProduct = dotProduct;
			}
		}
	}

	stack_string oldDebugEntityName = "";
	{
		const ICVar* const pMNDebugCVar = gEnv->pConsole->GetCVar("mn_debug");
		const char* const mnDebugEntityName = (pMNDebugCVar ? pMNDebugCVar->GetString() : NULL);
		oldDebugEntityName = (mnDebugEntityName) ? mnDebugEntityName : "";
	}

	const bool wasEnabled = !(oldDebugEntityName.empty());
	if (wasEnabled)
	{
		// Turn Off

		gEnv->pConsole->ExecuteString("i_forcefeedback 1");
		gEnv->pConsole->ExecuteString("c_shakeMult 1");
		gEnv->pConsole->ExecuteString("pl_health.minimalHudEffect 0");
		//gEnv->pConsole->ExecuteString( "r_PostProcessEffects 1"); <-- this crashes in certain levels

		gEnv->pConsole->ExecuteString("ai_debugdraw 0");
		gEnv->pConsole->ExecuteString("ac_debugLocations 0");
		gEnv->pConsole->ExecuteString("ai_drawpathfollower 0");
		gEnv->pConsole->ExecuteString("ca_drawlocator 0");

		gEnv->pConsole->ExecuteString("ac_debugfilter 0");

		gEnv->pConsole->ExecuteString("ai_filteragentname \"\"");

		{
			stack_string cmd;
			cmd.Format("es_debugAnim %s 0", oldDebugEntityName.c_str());
			gEnv->pConsole->ExecuteString(cmd.c_str());
		}

		gEnv->pConsole->ExecuteString("mn_debug 0");
	}

	if (!pDebugEntity)
		return;

	const bool sameDebugEntity = wasEnabled && !strcmp(oldDebugEntityName.c_str(), pDebugEntity->GetName());
	if (!sameDebugEntity)
	{
		// Turn On

		gEnv->pConsole->ExecuteString("i_forcefeedback 0");
		gEnv->pConsole->ExecuteString("c_shakeMult 0");
		gEnv->pConsole->ExecuteString("pl_health.minimalHudEffect 1");
		//gEnv->pConsole->ExecuteString( "r_PostProcessEffects 0"); <-- disabled this as re-enabling it crashes in certain levels

		gEnv->pConsole->ExecuteString("ai_debugdraw 1");
		gEnv->pConsole->ExecuteString("ac_debugLocations 2");
		gEnv->pConsole->ExecuteString("ai_drawpathfollower 1");
		gEnv->pConsole->ExecuteString("ca_drawlocator 1");

		{
			stack_string cmd;
			cmd.Format("ac_debugFilter %s", pDebugEntity->GetName());
			gEnv->pConsole->ExecuteString(cmd.c_str());
		}
		{
			stack_string cmd;
			cmd.Format("ai_filteragentname %s", pDebugEntity->GetName());
			gEnv->pConsole->ExecuteString(cmd.c_str());
		}
		{
			stack_string cmd;
			cmd.Format("es_debugAnim %s 1", pDebugEntity->GetName());
			gEnv->pConsole->ExecuteString(cmd.c_str());
		}
		{
			stack_string cmd;
			cmd.Format("mn_debug %s", pDebugEntity->GetName());
			gEnv->pConsole->ExecuteString(cmd.c_str());
		}
	}
}

namespace mannequin
{
namespace debug
{
static int mn_logToFile = 1;

//////////////////////////////////////////////////////////////////////////
void RegisterCommands()
{
	REGISTER_COMMAND("mn_reload", MN_ReloadAll, VF_CHEAT, "Reloads animation databases");
	REGISTER_COMMAND("mn_listAssets", MN_ListAssets, VF_CHEAT, "Lists all the currently referenced animation assets");
	REGISTER_COMMAND("mn_DebugAI", MN_DebugAI, VF_CHEAT, "");
	REGISTER_CVAR3("mn_LogToFile", mn_logToFile, 0, VF_CHEAT, "Dumps all mann event logging to the file");
}

#if MANNEQUIN_DEBUGGING_ENABLED

static const int maxSavedMsgs = 10;
enum ESavedMsgType {SMT_WARNING, SMT_ERROR};
struct SSavedMsg
{
	ESavedMsgType savedMsgType;
	char          savedMsg[MAX_WARNING_LENGTH];
	uint32        renderFrame;
	bool          isNew;
	bool          isUsed;
};
static SSavedMsg savedMsgs[maxSavedMsgs];
static int savedMsgIndex = 0;
static int lastRenderFrame = 0;

//====================================================================
// MannLog
//====================================================================
void Log(const IActionController& actionControllerI, const char* format, ...)
{
	const CActionController& actionController = (const CActionController&)actionControllerI;
	char outputBufferLog[MAX_WARNING_LENGTH];

	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog, format, args);
	va_end(args);

	if (mn_logToFile)
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, 0, 0, "Mann: %s", outputBufferLog);
	}

	if (actionController.GetFlag(AC_DebugDraw))
	{
		savedMsgs[savedMsgIndex].savedMsgType = SMT_WARNING;
		cry_strcpy(savedMsgs[savedMsgIndex].savedMsg, outputBufferLog);
		savedMsgs[savedMsgIndex].renderFrame = gEnv->pRenderer ? gEnv->pRenderer->GetFrameID() : 0;
		savedMsgs[savedMsgIndex].isNew = true;
		savedMsgs[savedMsgIndex].isUsed = true;
		savedMsgIndex = (savedMsgIndex + 1) % maxSavedMsgs;
	}
}

void DrawDebug()
{
	if (!gEnv->pRenderer)
		return;

	const ColorF fNew(1.0f, 1.0f, 1.0f, 1.0f);
	const ColorF fOld(0.6f, 0.6f, 0.6f, 1.0f);
	const float xPosOrigin = 900.0f;
	const float yPosOrigin = 300.0f;
	static float fontSize = 1.2f;
	const uint32 renderFrame = gEnv->pRenderer->GetFrameID();

	float xPos = xPosOrigin;
	float yPos = yPosOrigin;

	for (uint32 i = savedMsgIndex; i < maxSavedMsgs; i++)
	{
		if (savedMsgs[i].isUsed)
		{
			const ColorF& drawCol = (savedMsgs[i].isNew) ? fNew : fOld;
			IRenderAuxText::Draw2dLabel(xPos, yPos, fontSize, drawCol, false, "%s", savedMsgs[i].savedMsg);
			savedMsgs[i].isNew = false;
			yPos += 20.0f;
		}
	}
	for (uint32 i = 0; i < savedMsgIndex; i++)
	{
		if (savedMsgs[i].isUsed)
		{
			const ColorF& drawCol = (savedMsgs[i].isNew) ? fNew : fOld;
			IRenderAuxText::Draw2dLabel(xPos, yPos, fontSize, drawCol, false, "%s", savedMsgs[i].savedMsg);
			savedMsgs[i].isNew = false;
			yPos += 20.0f;
		}
	}
	lastRenderFrame = renderFrame;
}
#endif //!MANNEQUIN_DEBUGGING_ENABLED

}
}
