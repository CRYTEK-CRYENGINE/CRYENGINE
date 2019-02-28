// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PerfHUD.cpp
//  Created:     26/08/2009 by Timur.
//  Description: Button implementation in the MiniGUI
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>

#ifdef USE_PERFHUD

	#include "PerfHUD.h"
	#include "MiniGUI/MiniInfoBox.h"
	#include "MiniGUI/MiniMenu.h"
	#include "MiniGUI/MiniTable.h"
	#include <CrySystem/IConsole.h>
	#include <Cry3DEngine/I3DEngine.h>
	#include <CrySystem/ITimer.h>
	#include <CrySystem/Scaleform/IScaleformHelper.h>
	#include <CryParticleSystem/IParticles.h>
	#include "System.h"

	#include <CryExtension/CryCreateClassInstance.h>
	#include <CryRenderer/IRenderAuxGeom.h>

	#define PERFHUD_CONFIG_FILE "Config/PerfHud_PC.xml"

using namespace minigui;

const float CPerfHUD::OVERSCAN_X = 15;
const float CPerfHUD::OVERSCAN_Y = 15;

const ColorB CPerfHUD::COL_NORM = ColorB(255, 255, 255, 255);
const ColorB CPerfHUD::COL_WARN = ColorB(255, 255, 0, 255);
const ColorB CPerfHUD::COL_ERROR = ColorB(255, 0, 0, 255);

const float CPerfHUD::TEXT_SIZE_NORM = 14.f;
const float CPerfHUD::TEXT_SIZE_WARN = 18.f;
const float CPerfHUD::TEXT_SIZE_ERROR = 26.f;

const float CPerfHUD::ACTIVATE_TIME_FROM_GAME = 1.f;
const float CPerfHUD::ACTIVATE_TIME_FROM_HUD = 0.1f;

CRYREGISTER_SINGLETON_CLASS(CPerfHUD)
CPerfHUD::CPerfHUD() :
	m_menuStartX(OVERSCAN_X),
	m_menuStartY(OVERSCAN_Y),
	m_hudCreated(false),
	m_L1Pressed(false),
	m_L2Pressed(false),
	m_R1Pressed(false),
	m_R2Pressed(false),
	m_changingState(false),
	m_hwMouseEnabled(false),
	m_triggersDownStartTime(-1.f),
	m_hudState(eHudOff),
	m_hudLastState(eHudOff)
{
	m_widgets.reserve(ICryPerfHUDWidget::eWidget_Num);
}

void CPerfHUD::Destroy()
{
	m_widgets.clear();

	m_rootMenus.clear();

	IMiniGUIPtr pGUI;
	if (CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
	{
		pGUI->RemoveAllCtrl();
	}
}

//
// CONSOLE COMMAND CALLBACKS
//

int CPerfHUD::m_sys_perfhud = 0;
int CPerfHUD::m_sys_perfhud_pause = 0;

void CPerfHUD::CVarChangeCallback(ICVar* pCvar)
{
	ICryPerfHUD* pPerfHud = gEnv->pSystem->GetPerfHUD();

	if (pPerfHud)
	{
		int val = pCvar->GetIVal();
		//Check for invalid value
		if (val >= 0 && val < eHudNumStates)
		{
			pPerfHud->SetState((EHudState)val);
		}
	}
}

SET_WIDGET_DEF(Warnings, eWidget_Warnings)
SET_WIDGET_DEF(RenderSummary, eWidget_RenderStats)
SET_WIDGET_DEF(RenderBatchStats, eWidget_RenderBatchStats)
SET_WIDGET_DEF(FpsBuckets, eWidget_FpsBuckets)
SET_WIDGET_DEF(Particles, eWidget_Particles)
SET_WIDGET_DEF(PakFile, eWidget_PakFile)

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::Init()
{
	m_sys_perfhud_prev = 0;

	if (gEnv->pConsole)
	{
		REGISTER_CVAR2_CB("sys_perfhud", &m_sys_perfhud, 0, VF_ALWAYSONCHANGE, "PerfHUD 0:off, 1:In focus, 2:Out of focus", CVarChangeCallback);

		REGISTER_CVAR2("sys_perfhud_pause", &m_sys_perfhud_pause, 0, VF_NULL, "Toggle FPS Buckets exclusive / inclusive");

		SET_WIDGET_COMMAND("stats_Warnings", Warnings);
		SET_WIDGET_COMMAND("stats_RenderSummary", RenderSummary);
		SET_WIDGET_COMMAND("stats_RenderBatchStats", RenderBatchStats);
		SET_WIDGET_COMMAND("stats_FpsBuckets", FpsBuckets);
		SET_WIDGET_COMMAND("stats_Particles", Particles);
		SET_WIDGET_COMMAND("stats_PakFile", PakFile);
	}

	if (gEnv->pInput)
	{
		gEnv->pInput->AddEventListener(this);
	}

	IMiniGUIPtr pGUI;
	if (CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
	{
		InitUI(pGUI.get());
	}
}

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::LoadBudgets()
{
	XmlNodeRef budgets = gEnv->pSystem->LoadXmlFromFile(PERFHUD_CONFIG_FILE);

	if (budgets)
	{
		const int nWidgets = m_widgets.size();

		for (int i = 0; i < nWidgets; i++)
		{
			m_widgets[i]->LoadBudgets(budgets);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::SaveStats(const char* filename)
{
	XmlNodeRef rootNode = GetISystem()->CreateXmlNode("PerfHudStats");

	if (rootNode)
	{
		const int nWidgets = m_widgets.size();

		for (int i = 0; i < nWidgets; i++)
		{
			m_widgets[i]->SaveStats(rootNode);
		}

		if (!filename)
			filename = "PerfHudStats.xml";
		rootNode->saveToFile(filename);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::ResetWidgets()
{
	TWidgetIterator itWidget;
	for (itWidget = m_widgets.begin(); itWidget != m_widgets.end(); ++itWidget)
	{
		(*itWidget)->Reset();
	}
}

void CPerfHUD::AddWidget(ICryPerfHUDWidget* pWidget)
{
	assert(pWidget);
	static int s_widgetUID = ICryPerfHUDWidget::eWidget_Num;

	if (pWidget->m_id == -1)
	{
		pWidget->m_id = s_widgetUID++;
	}

	m_widgets.push_back(pWidget);
}

void CPerfHUD::RemoveWidget(ICryPerfHUDWidget* pWidget)
{
	TWidgetIterator widgetIter = m_widgets.begin();
	TWidgetIterator widgetEnd = m_widgets.end();

	while (widgetIter != widgetEnd)
	{
		if (pWidget == (*widgetIter).get())
		{
			m_widgets.erase(widgetIter);
			break;
		}
		++widgetIter;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::Done()
{
	if (gEnv->pInput)
	{
		gEnv->pInput->RemoveEventListener(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::Draw()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	if (m_hudState != m_hudLastState)
	{
		//restore gui state if the last state was off
		bool bRestoreState = (m_hudLastState == eHudOff);

		Show(bRestoreState);
		m_hudLastState = m_hudState;
	}

	if (m_hudState != eHudOff)
	{
		//Pause
		if (m_sys_perfhud_pause)
		{
			if ((gEnv->pRenderer->GetFrameID(false) % 40) < 20)
			{
				float col[4] = { 1.f, 1.f, 0.f, 1.f };
				IRenderAuxText::Draw2dLabel(500.f, 220.f, 2.f, col, false, "PefHUD Paused");
			}
		}
		else //Update
		{
			const int nWidgets = m_widgets.size();

			for (int i = 0; i < nWidgets; i++)
			{
				if (m_widgets[i]->ShouldUpdate())
				{
					m_widgets[i]->Update();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::Show(bool bRestoreState)
{
	IMiniGUIPtr pGUI;
	if (!CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
		return;

	switch (m_hudState)
	{
	case eHudInFocus:
		{
			pGUI->SetEventListener(this);

			if (!m_hudCreated)
			{
				InitUI(pGUI.get());
			}
			else if (bRestoreState)
			{
				pGUI->RestoreState();
			}
			pGUI->SetEnabled(true);
			pGUI->SetInFocus(true);

			int nRootMenus = m_rootMenus.size();
			for (int i = 0; i < nRootMenus; i++)
			{
				m_rootMenus[i]->SetVisible(1);
			}
		}
		break;

	case eHudOutOfFocus:
		{
			if (bRestoreState)
			{
				pGUI->RestoreState();
			}

			int nRootMenus = m_rootMenus.size();
			for (int i = 0; i < nRootMenus; i++)
			{
				m_rootMenus[i]->SetVisible(0);
			}

			pGUI->SetEventListener(0);
			pGUI->SetEnabled(true);
			pGUI->SetInFocus(false);
		}
		break;

	case eHudOff:
		{
			pGUI->SaveState();
			pGUI->Reset();
			pGUI->SetEnabled(false);
		}
		break;

	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::InitUI(IMiniGUI* pGUI)
{
	assert(m_hudCreated == false);

	IMiniCtrl* pMenu;
	XmlNodeRef perfXML = gEnv->pSystem->LoadXmlFromFile(PERFHUD_CONFIG_FILE);

	//
	// RENDERING MENU
	//
	pMenu = CreateMenu("Rendering");

	IMiniCtrl* pDebugMenu = CreateMenu("Debug", pMenu);
	CreateCVarMenuItem(pDebugMenu, "Wireframe", "r_wireframe", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Overdraw", "r_MeasureOverdrawScale", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Freeze Camera", "e_CameraFreeze", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Post Effects", "r_PostProcessEffects", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Deferred decals debug", "r_deferredDecalsDebug", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Shadows", "e_Shadows", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Ocean", "e_WaterOcean", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Particles", "e_Particles", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Terrain", "e_Terrain", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Brushes", "e_Brushes", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Vegetation", "e_Vegetation", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Characters", "ca_DrawChr", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Coverage Buffer", "e_CoverageBuffer", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Sun", "e_Sun", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Unlit", "r_Unlit", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Disable Normal Maps", "r_texbindmode", 0, 5);
	CreateCVarMenuItem(pDebugMenu, "Env Probes", "r_deferredShadingEnvProbes", 0, 1);
	CreateCVarMenuItem(pDebugMenu, "Lighting View", "r_texbindmode", 0, 11);
	CreateCVarMenuItem(pDebugMenu, "Normal and Lighting View", "r_texbindmode", 0, 6);
	CreateCVarMenuItem(pDebugMenu, "Default Material", "e_defaultMaterial", 0, 1);

	//this cvar is not created when perfHUD is initialised - so it doesn't work
	//CreateCVarMenuItem(pDebugMenu, "MFX visual debug", "mfx_DebugVisual", 0, 1 );

	IMiniCtrl* pStatsMenu = CreateMenu("Stats", pMenu);
	//Render Info
	CRenderStatsWidget* pRenderStats = new CRenderStatsWidget(pStatsMenu, this);

	if (pRenderStats)
	{
		if (perfXML)
		{
			pRenderStats->LoadBudgets(perfXML);
		}
		m_widgets.push_back(pRenderStats);
	}

	CRenderBatchWidget* pRenderBatchStats = new CRenderBatchWidget(pStatsMenu, this);

	if (pRenderBatchStats)
	{
		m_widgets.push_back(pRenderBatchStats);
	}

	#ifndef _RELEASE
	CreateCVarMenuItem(pStatsMenu, "Debug Gun", "e_debugDraw", 0, 16);
	#endif
	CreateCVarMenuItem(pStatsMenu, "Poly / Lod info", "e_debugDraw", 0, 1);
	CreateCVarMenuItem(pStatsMenu, "Texture Memory Usage", "e_debugDraw", 0, 4);
	CreateCVarMenuItem(pStatsMenu, "Detailed Render Stats", "r_Stats", 0, 1);
	CreateCVarMenuItem(pStatsMenu, "Shader Stats", "r_profiler", 0, 2);
	CreateCVarMenuItem(pStatsMenu, "Flash Stats", "sys_flash_info", 0, 1);

	//
	// SYSTEM MENU
	//
	pMenu = CreateMenu("System");

	//FPS Buckets
	CWarningsWidget* pWarningsWidget = new CWarningsWidget(pMenu, this);

	if (pWarningsWidget)
	{
		m_widgets.push_back(pWarningsWidget);
	}

	CreateCVarMenuItem(pMenu, "Profiler", "profile", 0, 1);
	CreateCVarMenuItem(pMenu, "Thread Summary", "r_profiler", 0, 1);
	CreateCVarMenuItem(pMenu, "Track File Access", "sys_PakLogInvalidFileAccess", 0, 1);

	//FPS Buckets
	CFpsWidget* pFpsBuckets = new CFpsWidget(pMenu, this);

	if (pFpsBuckets)
	{
		if (perfXML)
		{
			pFpsBuckets->LoadBudgets(perfXML);
		}
		m_widgets.push_back(pFpsBuckets);
	}

	//
	// STREAMING MENU
	//
	pMenu = CreateMenu("Streaming");
	CreateCVarMenuItem(pMenu, "Streaming Debug", "sys_streaming_debug", 0, 1);
	CreateCVarMenuItem(pMenu, "Loaded Geometry Info", "e_streamcgfdebug", 0, 3);
	CreateCVarMenuItem(pMenu, "Texture Load/Unload Debug", "r_TexBindMode", 0, 9);
	CreateCVarMenuItem(pMenu, "Textures by Size", "r_TexturesStreamingDebug", 0, 4);
	CreateCVarMenuItem(pMenu, "Textures by Prio", "r_TexturesStreamingDebug", 0, 5);

	//
	// SETUP MENU
	//
	pMenu = CreateMenu("Setup");
	CreateCallbackMenuItem(pMenu, "Reset HUD", CPerfHUD::ResetCallback, NULL);
	CreateCallbackMenuItem(pMenu, "Reload Budgets", CPerfHUD::ReloadBudgetsCallback, NULL);
	CreateCallbackMenuItem(pMenu, "Save Stats", CPerfHUD::SaveStatsCallback, NULL);
	CreateCVarMenuItem(pMenu, "Pause PerfHUD", "sys_perfhud_pause", 0, 1);

	//
	// EXTERNAL WIDGETS
	//
	if (gEnv->pParticleManager)
	{
		gEnv->pParticleManager->CreatePerfHUDWidget();
	}
	if (gEnv->pCryPak)
	{
		gEnv->pCryPak->CreatePerfHUDWidget();
	}

	//save default windows
	pGUI->SaveState();

	m_hudCreated = true;
}
//////////////////////////////////////////////////////////////////////////
// GUI CREATION HELPER FUNCS
//////////////////////////////////////////////////////////////////////////

IMiniCtrl* CPerfHUD::CreateMenu(const char* name, IMiniCtrl* pParent)
{
	assert(name);

	IMiniGUIPtr pGUI;
	if (CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
	{
		bool subMenu = false;

		if (pParent)
		{
			assert(pParent->GetType() == eCtrlType_Menu);
			subMenu = true;
		}

		const float ButtonWidth = 10.f; //arbitrary, button will be scaled based on contained text

		int ctrlFlags = 0;

		if (!subMenu)
		{
			ctrlFlags = eCtrl_TextAlignCentre | eCtrl_AutoResize;
		}

		Rect rcMenuBtn = Rect(m_menuStartX, m_menuStartY, m_menuStartX + ButtonWidth, m_menuStartY + 20);

		IMiniCtrl* pMenu = pGUI->CreateCtrl(pParent, 1, eCtrlType_Menu, ctrlFlags, rcMenuBtn, name);

		if (pMenu && !subMenu)
		{
			const float MenuBtnSepration = 10.f;

			//Update Menu Positions
			rcMenuBtn = pMenu->GetRect();
			m_menuStartX = rcMenuBtn.right + MenuBtnSepration;
			m_menuStartY = rcMenuBtn.top;

			m_rootMenus.push_back(pMenu);
		}

		return pMenu;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CPerfHUD::CreateCVarMenuItem(IMiniCtrl* pMenu, const char* name, const char* controlVar, float controlVarOff, float controlVarOn)
{
	assert(pMenu && name && controlVar);
	assert(pMenu->GetType() == eCtrlType_Menu);

	IMiniGUIPtr pGUI;
	if (CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
	{
		IMiniCtrl* pCtrl = pGUI->CreateCtrl(pMenu, 100, eCtrlType_Button, eCtrl_CheckButton, Rect(0, 0, 100, 20), name);

		if (pCtrl)
		{
			if (controlVar)
			{
				pCtrl->SetControlCVar(controlVar, controlVarOff, controlVarOn);
			}
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPerfHUD::CreateCallbackMenuItem(IMiniCtrl* pMenu, const char* name, ClickCallback clickCallback, void* pCallbackData)
{
	assert(pMenu && name && clickCallback);
	assert(pMenu->GetType() == eCtrlType_Menu);

	IMiniGUIPtr pGUI;
	if (CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
	{
		IMiniCtrl* pCtrl = pGUI->CreateCtrl(pMenu, 100, eCtrlType_Button, 0 /*eCtrl_CheckButton*/, Rect(0, 0, 100, 20), name);

		if (pCtrl)
		{
			pCtrl->SetClickCallback(clickCallback, pCallbackData);
			return true;
		}
	}
	return false;

}

//////////////////////////////////////////////////////////////////////////
IMiniInfoBox* CPerfHUD::CreateInfoMenuItem(IMiniCtrl* pMenu, const char* name, RenderCallback renderCallback, const Rect& rect, bool onAtStart)
{
	assert(pMenu->GetType() == eCtrlType_Menu);

	IMiniGUIPtr pGUI;
	if (CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
	{
		IMiniCtrl* pCtrl = pGUI->CreateCtrl(pMenu, 100, eCtrlType_Button, eCtrl_CheckButton, Rect(0, 0, 100, 20), name);

		int infoFlags = eCtrl_Moveable | eCtrl_CloseButton;

		if (!renderCallback)
		{
			infoFlags |= eCtrl_AutoResize;
		}

		CMiniInfoBox* pInfo = (CMiniInfoBox*)pGUI->CreateCtrl(NULL, 200, eCtrlType_InfoBox, infoFlags, rect, name);

		if (pCtrl)
		{
			pCtrl->SetConnectedCtrl(pInfo);

			if (onAtStart)
			{
				pCtrl->SetFlag(eCtrl_Checked);
			}
			else
			{
				pInfo->SetVisible(false);
			}

			if (renderCallback)
			{
				pInfo->SetRenderCallback(renderCallback);
			}

			return pInfo;
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IMiniTable* CPerfHUD::CreateTableMenuItem(IMiniCtrl* pMenu, const char* name)
{
	assert(pMenu && name);
	assert(pMenu->GetType() == eCtrlType_Menu);

	IMiniGUIPtr pGUI;
	if (CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
	{
		CMiniTable* pTable = (CMiniTable*)pGUI->CreateCtrl(NULL, 200, eCtrlType_Table, eCtrl_AutoResize | eCtrl_Moveable | eCtrl_CloseButton, Rect(50, 100, 400, 350), name);

		if (pTable)
		{
			IMiniCtrl* pCtrl = pGUI->CreateCtrl(pMenu, 100, eCtrlType_Button, eCtrl_CheckButton, Rect(0, 0, 100, 20), name);

			if (pCtrl)
			{
				pCtrl->SetConnectedCtrl(pTable);

				pTable->SetVisible(false);

				return pTable;
			}
		}
	}
	return NULL;
}

IMiniCtrl* CPerfHUD::GetMenu(const char* name)
{
	int nRootMenus = m_rootMenus.size();

	for (int i = 0; i < nRootMenus; i++)
	{
		if (strcmp(m_rootMenus[i]->GetTitle(), name) == 0)
		{
			return m_rootMenus[i];
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CPerfHUD::OnCommand(SCommand& cmd)
{
	//CryLog( "Button Clicked = %d",cmd.nCtrlID );
}

//////////////////////////////////////////////////////////////////////////
bool CPerfHUD::OnInputEvent(const SInputEvent& rInputEvent)
{
	const EKeyId L1 = eKI_XI_ShoulderL;
	//const EKeyId L2 = eKI_XI_TriggerLBtn;
	const EKeyId R1 = eKI_XI_ShoulderR;
	//const EKeyId R2 = eKI_XI_TriggerRBtn;
	const EKeyId X = eKI_XI_X;

	//keyboard shortcuts
	if (rInputEvent.deviceType == eIDT_Keyboard && rInputEvent.state == eIS_Pressed)
	{
		switch (rInputEvent.keyId)
		{
		//Cycle modes
		case eKI_Print:
			if (rInputEvent.modifiers & (eMM_LAlt | eMM_LCtrl))
			{
				SetNextState();
			}
			break;

		//toggle pause
		case eKI_ScrollLock:
			m_sys_perfhud_pause ^= 1;
			break;
		}
	}

	if (rInputEvent.deviceType == eIDT_Gamepad &&
	    rInputEvent.deviceIndex == 0)
	{
		int frameNum = gEnv->pRenderer->GetFrameID(false);

		if (rInputEvent.state == eIS_Pressed)
		{
			bool checkState = false;

			switch (rInputEvent.keyId)
			{
			case L1:
				m_L1Pressed = true;
				checkState = true;
				break;

			/*case L2:
			   m_L2Pressed = true;
			   checkState = true;
			   break;*/

			case R1:
				m_R1Pressed = true;
				checkState = true;
				break;

			/*case R2:
			   m_R2Pressed = true;
			   checkState = true;
			   break;*/

			case X:
				if (m_changingState)
				{
					SetNextState();
				}
				break;
			}

			//if(checkState&&m_L1Pressed&&m_L2Pressed&&m_R1Pressed&&m_R2Pressed)
			if (checkState && m_L1Pressed && m_R1Pressed)
			{
				m_triggersDownStartTime = gEnv->pTimer->GetAsyncCurTime();
			}
		}
		else if (rInputEvent.state == eIS_Down)
		{
			float activateTime = (m_hudState == eHudOff) ? ACTIVATE_TIME_FROM_GAME : ACTIVATE_TIME_FROM_HUD;

			if (m_triggersDownStartTime > 0.f && gEnv->pTimer->GetAsyncCurTime() - m_triggersDownStartTime > activateTime)
			{
				m_changingState = true;

				const char* hudStateStr = NULL;

				switch (m_hudState)
				{
				case eHudInFocus:
					hudStateStr = "CryPerfHUD Edit Mode";
					break;

				case eHudOutOfFocus:
					hudStateStr = "CryPerfHUD Game Mode";
					break;

				case eHudOff:
					hudStateStr = "CryPerfHUD Off";
					break;

				default:
					hudStateStr = "CryPerfHud unknown";
					break;
				}

				float col[4] = { 1.f, 1.f, 1.f, 1.f };
				IRenderAuxText::Draw2dLabel(450.f, 200.f, 2.f, col, false, "%s", hudStateStr);
				IRenderAuxText::Draw2dLabel(450.f, 220.f, 2.f, col, false, "Press X to change Mode");
			}
		}
		else if (rInputEvent.state == eIS_Released)
		{
			bool triggerReleased = false;

			switch (rInputEvent.keyId)
			{
			case L1:
				m_L1Pressed = false;
				triggerReleased = true;
				break;

			/*case L2:
			   m_L2Pressed = false;
			   triggerReleased=true;
			   break;*/

			case R1:
				m_R1Pressed = false;
				triggerReleased = true;
				break;

				/*case R2:
				   m_R2Pressed = false;
				   triggerReleased=true;
				   break;*/
			}

			if (triggerReleased)
			{
				m_triggersDownStartTime = 0.f;

				if (m_changingState)
				{
					m_changingState = false;

					//workaround: hardware mouse resets all input states when enabled
					//this breaks perfhud selection mode (as triggers are released)
					//don't enable mouse until we've finished selection mode
					if (m_hudState == eHudInFocus)
					{
						if (!m_hwMouseEnabled)
						{
							gEnv->pHardwareMouse->IncrementCounter();
							gEnv->pHardwareMouse->SetHardwareMousePosition(gEnv->pRenderer->GetOverlayWidth() * 0.5f, gEnv->pRenderer->GetOverlayHeight() * 0.5f);
							m_hwMouseEnabled = true;
						}
					}
					else if (m_hwMouseEnabled)
					{
						gEnv->pHardwareMouse->DecrementCounter();
						m_hwMouseEnabled = false;
					}
				}
			}
		}

		if (m_hudState == eHudInFocus)
		{
			//PerfHUD takes control of the input
			return true;
		}
	}
	return false;
}

void CPerfHUD::SetNextState()
{
	if (m_sys_perfhud != eHudOff)
	{
		m_hudState = (EHudState)((m_hudState + 1) % eHudNumStates);
		//CryLogAlways("Setting new HUD state: %d", m_hudState);

		//what to do about cvar?
		//ICVar *pPerfCVar = GetISystem()->GetIConsole()->GetCVar("sys_perfhud");
		//pPerfCVar->Set( 1 );
	}
}

void CPerfHUD::SetState(EHudState state)
{
	if (state != m_hudState)
	{
		if (m_hwMouseEnabled)
		{
			if (state != eHudInFocus)
			{
				gEnv->pHardwareMouse->DecrementCounter();
				m_hwMouseEnabled = false;
			}
		}
		else if (state == eHudInFocus)
		{
			gEnv->pHardwareMouse->IncrementCounter();
			gEnv->pHardwareMouse->SetHardwareMousePosition(gEnv->pRenderer->GetOverlayWidth() * 0.5f, gEnv->pRenderer->GetOverlayHeight() * 0.5f);
			m_hwMouseEnabled = true;
		}

		m_hudState = state;
	}
}

void CPerfHUD::Reset()
{
	IMiniGUIPtr pGUI;
	if (CryCreateClassInstanceForInterface(cryiidof<IMiniGUI>(), pGUI))
	{
		pGUI->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
// CLICK CALLBACKS
//////////////////////////////////////////////////////////////////////////

void CPerfHUD::ResetCallback(void* data, bool status)
{
	ICryPerfHUDPtr pPerfHUD;
	if (CryCreateClassInstanceForInterface(cryiidof<ICryPerfHUD>(), pPerfHUD))
	{
		pPerfHUD->Reset();
	}
}

void CPerfHUD::ReloadBudgetsCallback(void* data, bool status)
{
	ICryPerfHUDPtr pPerfHUD;
	if (CryCreateClassInstanceForInterface(cryiidof<ICryPerfHUD>(), pPerfHUD))
	{
		pPerfHUD->LoadBudgets();
	}
}

void CPerfHUD::SaveStatsCallback(void* data, bool status)
{
	ICryPerfHUDPtr pPerfHUD;
	if (CryCreateClassInstanceForInterface(cryiidof<ICryPerfHUD>(), pPerfHUD))
	{
		pPerfHUD->SaveStats();
	}
}

//////////////////////////////////////////////////////////////////////////
// RENDER CALLBACKS
//////////////////////////////////////////////////////////////////////////

/*
   void CPerfHUD::DisplayRenderInfoCallback(const Rect& rect)
   {
   IMiniGUIPtr pGUI;
   if (CryCreateClassInstanceForInterface( cryiidof<IMiniGUI>(),pGUI ))
   {
    //right aligned display
    float x = rect.right;
    float y = rect.top;
    float step = 13.f;

    gEnv->p3DEngine->DisplayInfo(x, y, step, true);
   }
   }*/

void CPerfHUD::EnableWidget(ICryPerfHUDWidget::EWidgetID id, int mode)
{
	const int nWidgets = m_widgets.size();

	for (int i = 0; i < nWidgets; i++)
	{
		if (m_widgets[i]->m_id == id)
		{
			m_widgets[i]->Enable(mode);
			return;
		}
	}
}

void CPerfHUD::DisableWidget(ICryPerfHUDWidget::EWidgetID id)
{
	const int nWidgets = m_widgets.size();

	for (int i = 0; i < nWidgets; i++)
	{
		if (m_widgets[i]->m_id == id)
		{
			m_widgets[i]->Disable();
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Widget Specific Interface
//////////////////////////////////////////////////////////////////////////
void CPerfHUD::AddWarning(float duration, const char* fmt, va_list argList)
{
	if (m_hudState != eHudOff)
	{
		//could cache warnings window ptr for efficiency
		const int nWidgets = m_widgets.size();

		for (int i = 0; i < nWidgets; i++)
		{
			if (m_widgets[i]->m_id == ICryPerfHUDWidget::eWidget_Warnings)
			{
				CWarningsWidget* warnings = (CWarningsWidget*)m_widgets[i].get();

				if (warnings->ShouldUpdate())
				{
					warnings->AddWarningV(duration, fmt, argList);
					break;
				}
			}
		}
	}
}

bool CPerfHUD::WarningsWindowEnabled() const
{
	const int nWidgets = m_widgets.size();

	for (int i = 0; i < nWidgets; i++)
	{
		if (m_widgets[i]->m_id == ICryPerfHUDWidget::eWidget_Warnings)
		{
			return m_widgets[i]->ShouldUpdate();
		}
	}

	return false;
}

const std::vector<ICryPerfHUD::PerfBucket>* CPerfHUD::GetFpsBuckets(float& totalTime) const
{
	const int nWidgets = m_widgets.size();

	for (int i = 0; i < nWidgets; i++)
	{
		if (m_widgets[i]->m_id == ICryPerfHUDWidget::eWidget_FpsBuckets)
		{
			return ((CFpsWidget*)m_widgets[i].get())->GetFpsBuckets(totalTime);
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//FPS Buckets Widget
//////////////////////////////////////////////////////////////////////////

int CFpsWidget::m_cvarPerfHudFpsExclusive = 0;

CFpsWidget::CFpsWidget(IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud) : ICryPerfHUDWidget(eWidget_FpsBuckets)
{
	m_fpsBucketSize = 5.f;
	m_fpsBudget = 30.f;
	m_dpBudget = 2500.f;
	m_dpBucketSize = 250.f;

	//FPS Buckets
	IMiniCtrl* pFPSMenu = pPerfHud->CreateMenu("FPS", pParentMenu);
	m_pInfoBox = pPerfHud->CreateInfoMenuItem(pFPSMenu, "FPS Buckets", NULL, Rect(850, 395, 860, 405));
	pPerfHud->CreateCallbackMenuItem(pFPSMenu, "Reset Buckets", CFpsWidget::ResetCallback, this);

	//Display framerates buckets as inclusive or exclusive
	REGISTER_CVAR2("sys_perfhud_fpsBucketsExclusive", &m_cvarPerfHudFpsExclusive, 0, VF_CHEAT, "Toggle FPS Buckets exclusive / inclusive");

	Init();
}

void CFpsWidget::LoadBudgets(XmlNodeRef PerfXML)
{
	if (PerfXML)
	{
		XmlNodeRef xmlNode;

		//FPS / GPU - explicit bucket values
		if ((xmlNode = PerfXML->findChild("fpsBucketValues")))
		{
			m_perfBuckets[BUCKET_FPS].buckets.clear();
			m_perfBuckets[BUCKET_GPU].buckets.clear();

			uint32 nBuckets = xmlNode->getChildCount();

			for (uint32 i = 0; i < nBuckets; i++)
			{
				XmlNodeRef bucketVal = xmlNode->getChild(i);

				float target;
				bucketVal->getAttr("value", target);

				ICryPerfHUD::PerfBucket bucket(target);

				m_perfBuckets[BUCKET_FPS].buckets.push_back(bucket);
				m_perfBuckets[BUCKET_GPU].buckets.push_back(bucket);
			}

			m_perfBuckets[BUCKET_FPS].totalTime = 0.f;
			m_perfBuckets[BUCKET_GPU].totalTime = 0.f;
		}
		//Auto generated buckets based on max fps and bucket size
		else if ((xmlNode = PerfXML->findChild("fpsBucketMax")))
		{
			xmlNode->getAttr("value", m_fpsBudget);

			if ((xmlNode = PerfXML->findChild("fpsBucketSize")))
			{
				xmlNode->getAttr("value", m_fpsBucketSize);
			}
			else
			{
				m_fpsBucketSize = 5;
			}

			m_perfBuckets[BUCKET_FPS].buckets.clear();
			m_perfBuckets[BUCKET_GPU].buckets.clear();

			float targetFPS = m_fpsBudget;

			for (uint32 i = 0; i < NUM_FPS_BUCKETS_DEFAULT; i++)
			{
				ICryPerfHUD::PerfBucket bucket(targetFPS);

				m_perfBuckets[BUCKET_FPS].buckets.push_back(bucket);
				m_perfBuckets[BUCKET_GPU].buckets.push_back(bucket);

				targetFPS -= m_fpsBucketSize;
			}

			m_perfBuckets[BUCKET_FPS].totalTime = 0.f;
			m_perfBuckets[BUCKET_GPU].totalTime = 0.f;
		}

		//DP buckets - explicit bucket values
		if ((xmlNode = PerfXML->findChild("dpBucketValues")))
		{
			m_perfBuckets[BUCKET_DP].buckets.clear();

			uint32 nBuckets = xmlNode->getChildCount();

			for (uint32 i = 0; i < nBuckets; i++)
			{
				XmlNodeRef bucketVal = xmlNode->getChild(i);

				float target;
				bucketVal->getAttr("value", target);

				ICryPerfHUD::PerfBucket bucket(target);

				m_perfBuckets[BUCKET_DP].buckets.push_back(bucket);
			}
			m_perfBuckets[BUCKET_DP].totalTime = 0.f;
		}
		//DPs auto values
		else if ((xmlNode = PerfXML->findChild("drawPrimBucketMax")))
		{
			xmlNode->getAttr("value", m_dpBudget);

			//Auto generated buckets based on max dp and bucket size
			if ((xmlNode = PerfXML->findChild("dpBucketSize")))
			{
				xmlNode->getAttr("value", m_dpBucketSize);
			}
			else
			{
				m_dpBucketSize = 250.f;
			}

			m_perfBuckets[BUCKET_DP].buckets.clear();

			float nDPs = m_dpBudget - ((NUM_FPS_BUCKETS_DEFAULT - 1) * m_dpBucketSize);

			for (uint32 i = 0; i < NUM_FPS_BUCKETS_DEFAULT; i++)
			{
				ICryPerfHUD::PerfBucket bucket(nDPs);

				m_perfBuckets[BUCKET_DP].buckets.push_back(bucket);

				nDPs += m_dpBucketSize;
			}
			m_perfBuckets[BUCKET_DP].totalTime = 0.f;
		}
	}
}

template<bool LESS_THAN>
void CFpsWidget::UpdateBuckets(PerfBucketsStat& bucketStat, float frameTime, const char* name, float stat)
{
	const uint32 numBuckets = bucketStat.buckets.size();

	if (frameTime > 0.f)
	{
		bucketStat.totalTime += frameTime;

		for (uint32 i = 0; i < numBuckets; i++)
		{
			if (LESS_THAN)
			{
				if (stat <= bucketStat.buckets[i].target)
				{
					bucketStat.buckets[i].timeAtTarget += frameTime;
				}
			}
			else
			{
				if (stat >= bucketStat.buckets[i].target)
				{
					bucketStat.buckets[i].timeAtTarget += frameTime;
				}
			}
		}
	}

	if (bucketStat.totalTime > 0.f)
	{
		char entryBuffer[CMiniInfoBox::MAX_TEXT_LENGTH];

		cry_sprintf(entryBuffer, "%s: %.2f", name, stat);
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);

		//render exclusive stats
		if (m_cvarPerfHudFpsExclusive)
		{
			for (uint32 i = 0; i < numBuckets; i++)
			{
				//inclusive time
				float timeAtTarget = bucketStat.buckets[i].timeAtTarget;

				if (i > 0)
				{
					//exclusive time
					timeAtTarget -= bucketStat.buckets[i - 1].timeAtTarget;

					//Add info to gui
					float percentAtTarget = 100.f * (timeAtTarget / bucketStat.totalTime);
					cry_sprintf(entryBuffer, "%.2f%%%% of time %.1f -> %.1f FPS", percentAtTarget, bucketStat.buckets[i].target, bucketStat.buckets[i - 1].target);
				}
				else
				{
					float percentAtTarget = 100.f * (bucketStat.buckets[i].timeAtTarget / bucketStat.totalTime);
					cry_sprintf(entryBuffer, "%.2f%%%% of time >= %.1f FPS", percentAtTarget, bucketStat.buckets[i].target);
				}
				m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
			}
		}
		else //render inclusive stats
		{
			for (uint32 i = 0; i < numBuckets; i++)
			{
				float percentAtTarget = 100.f * (bucketStat.buckets[i].timeAtTarget / bucketStat.totalTime);
				cry_sprintf(entryBuffer, "%.2f%%%% of time %s %.1f", percentAtTarget, LESS_THAN ? "<=" : ">=", bucketStat.buckets[i].target);
				m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
			}
		}
	}
}

void CFpsWidget::Update()
{
	m_pInfoBox->ClearEntries();

	//
	// FPS
	//
	{
		float frameTime = gEnv->pTimer->GetRealFrameTime();
		UpdateBuckets<false>(m_perfBuckets[BUCKET_FPS], frameTime, "FPS", 1.f / frameTime);
	}

	//
	// GPU FPS
	//
	{
		float gpuFrameTime = gEnv->pRenderer->GetGPUFrameTime();

		if (gpuFrameTime > 0.f)
		{
			m_pInfoBox->AddEntry("", CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
			UpdateBuckets<false>(m_perfBuckets[BUCKET_GPU], gpuFrameTime, "GPU FPS", 1.f / gpuFrameTime);
		}
	}

	//
	// Draw Call
	//
	{
		//ugly, but buckets are float only at the moment
		float nDPs = (float)gEnv->pRenderer->GetCurrentNumberOfDrawCalls();
		m_pInfoBox->AddEntry("", CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
		UpdateBuckets<true>(m_perfBuckets[BUCKET_DP], 1.f, "DPs", nDPs);
	}
}

//Init buckets with default values
void CFpsWidget::Init()
{
	float targetFPS = m_fpsBudget;
	float nDPs = m_dpBudget - ((NUM_FPS_BUCKETS_DEFAULT - 1) * m_dpBucketSize);

	for (uint32 i = 0; i < NUM_FPS_BUCKETS_DEFAULT; i++)
	{
		ICryPerfHUD::PerfBucket bucket(targetFPS);

		m_perfBuckets[BUCKET_FPS].buckets.push_back(bucket);
		m_perfBuckets[BUCKET_GPU].buckets.push_back(bucket);

		bucket.target = (float)nDPs;

		m_perfBuckets[BUCKET_DP].buckets.push_back(bucket);

		targetFPS -= m_fpsBucketSize;
		nDPs += m_dpBucketSize;
	}

	for (uint32 i = 0; i < BUCKET_TYPE_NUM; i++)
	{
		m_perfBuckets[i].totalTime = 0.f;
	}
}

//Clear bucket totals
void CFpsWidget::Reset()
{
	for (uint32 i = 0; i < BUCKET_TYPE_NUM; i++)
	{
		PerfBucketsStat& stat = m_perfBuckets[i];

		const uint32 nBuckets = stat.buckets.size();

		//Init fps buckets
		for (uint32 j = 0; j < nBuckets; j++)
		{
			stat.buckets[j].timeAtTarget = 0.f;
		}

		stat.totalTime = 0.f;
	}
}

void CFpsWidget::ResetCallback(void* data, bool status)
{
	assert(data);
	((CFpsWidget*)data)->Reset();
}

bool CFpsWidget::ShouldUpdate()
{
	return !m_pInfoBox->IsHidden();
}

//TODO
void CFpsWidget::SaveStats(XmlNodeRef statsXML)
{
	if (statsXML)
	{
		const char* perfBucketTypeStr[] =
		{
			"BUCKET_FPS",
			"BUCKET_GPU",
			"BUCKET_DP",
		};

		XmlNodeRef fpsNode;

		for (int i = 0; i < BUCKET_TYPE_NUM; i++)
		{
			PerfBucketsStat& perfBucket = m_perfBuckets[i];

			if (perfBucket.totalTime > 0.f)
			{
				if ((fpsNode = statsXML->newChild(perfBucketTypeStr[i])))
				{
					XmlNodeRef child;

					const uint32 numBuckets = perfBucket.buckets.size();

					for (uint32 j = 0; j < numBuckets; j++)
					{
						float percentAtTarget = 100.f * (perfBucket.buckets[j].timeAtTarget / perfBucket.totalTime);

						if ((child = fpsNode->newChild("bucket")))
						{
							child->setAttr("target", perfBucket.buckets[j].target);
						}

						if ((child = fpsNode->newChild("percentAtTime")))
						{
							child->setAttr("value", percentAtTarget);
						}
					}
				}
			}
		}
	}
}

const std::vector<ICryPerfHUD::PerfBucket>* CFpsWidget::GetFpsBuckets(float& totalTime) const
{
	totalTime = m_perfBuckets[BUCKET_FPS].totalTime;
	return &m_perfBuckets[BUCKET_FPS].buckets;
}

//////////////////////////////////////////////////////////////////////////
//Render Stats Widget
//////////////////////////////////////////////////////////////////////////

CRenderStatsWidget::CRenderStatsWidget(IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud) : ICryPerfHUDWidget(eWidget_RenderStats)
{
	m_pPerfHUD = pPerfHud;
	m_fpsBudget = 30.f;
	m_dpBudget = 2000;
	m_polyBudget = 500000;
	m_postEffectBudget = 3;
	m_shadowCastBudget = 2;
	m_particlesBudget = 1000;
	m_pInfoBox = NULL;
	m_buildNum = 0;

	ZeroStruct(m_runtimeData);

	m_pInfoBox = pPerfHud->CreateInfoMenuItem(pParentMenu, "Scene Summary", NULL, Rect(45, 350, 100, 400) /*, true*/);

	//Get build number from BuildName.txt
	//FILE *buildFile = fxopen("BuildName.txt", "r");
	CCryFile buildFile;

	if (buildFile.Open("./BuildName.txt", "rb", ICryPak::FOPEN_ONDISK | ICryPak::FOPEN_HINT_QUIET))
	{
		size_t fileSize = buildFile.GetLength();

		if (fileSize > 0 && fileSize < 64)
		{
			char buffer[64] = { 0 };

			buildFile.ReadRaw(buffer, fileSize);

			if (buffer[0])
			{
				const char* ptr = strchr(buffer, '(');

				if (ptr)
				{
					ptr++;
					m_buildNum = atoi(ptr);
				}
			}
		}
	}
}

void CRenderStatsWidget::Update()
{
	IRenderer* pRenderer = gEnv->pRenderer;

	//Clear old entries
	m_pInfoBox->ClearEntries();
	ZeroStruct(m_runtimeData);

	//
	// Render Type
	//
	const char* pRenderType = NULL;

	switch (gEnv->pRenderer->GetRenderType())
	{
#if CRY_PLATFORM_DURANGO
	case ERenderType::Direct3D11:
		pRenderType = "Xbox - DX11";
		break;
	case ERenderType::Direct3D12:
		pRenderType = "Xbox - DX12";
		break;
#else
	case ERenderType::Direct3D11:
		pRenderType = "PC - DX11";
		break;
	case ERenderType::Direct3D12:
		pRenderType = "PC - DX12";
		break;
#endif
#if CRY_PLATFORM_MOBILE
	case ERenderType::OpenGL:
		pRenderType = "Mobile - OpenGL";
		break;
	case ERenderType::Vulkan:
		pRenderType = "Mobile - Vulkan";
		break;
#else
	case ERenderType::OpenGL:
		pRenderType = "PC - OpenGL";
		break;
	case ERenderType::Vulkan:
		pRenderType = "PC - Vulkan";
		break;
#endif
	case ERenderType::GNM:
		pRenderType = "PS4 - GNM";
		break;
	case ERenderType::Undefined:
	default:
		assert(0);
		pRenderType = "Undefined";
		break;
	}

	const char* buildType = NULL;

	#if defined(_RELEASE)
		buildType = "RELEASE";
	#elif defined(_DEBUG)
		buildType = "DEBUG";
	#else
		buildType = "PROFILE"; //Assume profile?
	#endif

	char entryBuffer[CMiniInfoBox::MAX_TEXT_LENGTH];

	//cry_sprintf(entryBuffer, "%s - %s - Build: %d", pRenderType, buildType, m_buildNum);
	//m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);

	//
	// FPS
	//
	m_runtimeData.fps = min(9999.f, gEnv->pTimer->GetFrameRate());

	cry_sprintf(entryBuffer, "FPS: %.2f (%.2f)", m_runtimeData.fps, m_fpsBudget);

	if (m_runtimeData.fps >= m_fpsBudget)
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
	}
	else
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
		CryPerfHUDWarning(1.f, "FPS Too Low: %.2f", m_runtimeData.fps);

		//PerfHud  / AuxRenderer causes us to be RenderThread limited
		//Need to investigate vertex buffer locks in AuxRenderer before enabling

		/*
		   // Fran: please call me before re-enabling this code

		   SRenderTimes renderTimes;
		   pRenderer->GetRenderTimes(renderTimes);

		   //wait for main is never 0
		   if(renderTimes.fWaitForMain>renderTimes.fWaitForRender && renderTimes.fWaitForMain>renderTimes.fWaitForGPU)
		   {
		   m_pInfoBox->AddEntry("Main Thread Limited", CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_ERROR);
		   m_pPerfHud->AddWarning("Main Thread Limited",1.f);
		   }
		   else if(renderTimes.fWaitForRender>renderTimes.fWaitForGPU)
		   {
		   m_pInfoBox->AddEntry("Render Thread Limited", CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_ERROR);
		   m_pPerfHud->AddWarning("Render Thread Limited",1.f);
		   }
		   else
		   {
		   m_pInfoBox->AddEntry("GPU Limited", CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_ERROR);
		   m_pPerfHud->AddWarning("GPU Limited",1.f);
		   }*/
	}

	//
	// GPU Time
	//
	float gpuTime = pRenderer->GetGPUFrameTime();

	if (gpuTime > 0.f)
	{
		float gpuFPS = 1.f / gpuTime;

		cry_sprintf(entryBuffer, "GPU FPS: %.2f (%.2f)", gpuFPS, m_fpsBudget);
		if (gpuFPS >= m_fpsBudget)
		{
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
		}
		else
		{
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
			CryPerfHUDWarning(1.f, "GPU FPS Too Low: %.2f", gpuFPS);
		}
	}

	//
	// HDR
	//
	bool bHDRModeEnabled = false;
	pRenderer->EF_Query(EFQ_HDRModeEnabled, bHDRModeEnabled);
	if (bHDRModeEnabled)
	{
		m_runtimeData.hdrEnabled = true;
		if (!m_runtimeData.hdrEnabled)
		{
			CryPerfHUDWarning(1.f, "HDR Disabled");
		}
	}

	if (!gEnv->IsEditor())
	{
		//
		// Render Thread
		//
		static ICVar* pMultiThreaded = gEnv->pConsole->GetCVar("r_MultiThreaded");
		if (pMultiThreaded && pMultiThreaded->GetIVal() > 0)
		{
			//cry_sprintf(entryBuffer, "Render Thread Enabled");
			//m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
			m_runtimeData.renderThreadEnabled = true;
		}
		else
		{
			cry_sprintf(entryBuffer, "Render Thread Disabled");
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
			m_runtimeData.renderThreadEnabled = false;
			CryPerfHUDWarning(1.f, "Render Thread Disabled");
		}
	}

	//
	// Camera
	//
	Matrix33 m = Matrix33(GetISystem()->GetViewCamera().GetMatrix());
	m_runtimeData.cameraRot = RAD2DEG(Ang3::GetAnglesXYZ(m));
	m_runtimeData.cameraPos = GetISystem()->GetViewCamera().GetPosition();

	//
	// Polys / Draw Prims
	//
	int nShadowVolPolys;
	int nPolys;
	pRenderer->GetPolyCount(nPolys, nShadowVolPolys);
	m_runtimeData.nPolys = nPolys;
	m_runtimeData.nDrawPrims = pRenderer->GetCurrentNumberOfDrawCalls();

	cry_sprintf(entryBuffer, "Draw Calls: %u (%u)", m_runtimeData.nDrawPrims, m_dpBudget);

	if (m_runtimeData.nDrawPrims <= m_dpBudget)
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
	}
	else
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
		CryPerfHUDWarning(1.f, "Too Many Draw Calls: %u", m_runtimeData.nDrawPrims);
	}

	cry_sprintf(entryBuffer, "Num Tris: %u (%u)", m_runtimeData.nPolys, m_polyBudget);

	if (m_runtimeData.nPolys <= m_polyBudget)
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
	}
	else
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
		CryPerfHUDWarning(1.f, "Too Many Tris: %d", m_runtimeData.nPolys);
	}

	//
	// Post Effects
	//

	gEnv->pRenderer->EF_Query(EFQ_NumActivePostEffects, m_runtimeData.nPostEffects);
	cry_sprintf(entryBuffer, "Num Post Effects: %u (%u)", m_runtimeData.nPostEffects, m_postEffectBudget);

	if (m_runtimeData.nPostEffects <= m_postEffectBudget)
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
	}
	else
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
		CryPerfHUDWarning(1.f, "Too Many Post Effects: %d", m_runtimeData.nPostEffects);
	}

	//
	// Forward Lights
	//
	PodArray<SRenderLight*>* pLights = gEnv->p3DEngine->GetDynamicLightSources();
	const uint32 nDynamicLights = pLights->Count();

	if (nDynamicLights)
	{
		uint32 nShadowCastingLights = 0;

		for (uint32 i = 0; i < nDynamicLights; i++)
		{
			SRenderLight* pLight = pLights->GetAt(i);

			if (pLight->m_Flags & DLF_CASTSHADOW_MAPS)
			{
				nShadowCastingLights++;
			}
		}

		cry_sprintf(entryBuffer, "Num Forward Lights: %u (~)", nDynamicLights);
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);

		if (nShadowCastingLights)
		{
			cry_sprintf(entryBuffer, "Num Forward Shadow Casting Lights: %u (%u)", nShadowCastingLights, m_shadowCastBudget);

			if (nShadowCastingLights <= m_shadowCastBudget)
			{
				m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
			}
			else
			{
				m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
				CryPerfHUDWarning(1.f, "Too Many Fwd Shadow Casting Lights: %d", nShadowCastingLights);
			}
		}

		m_runtimeData.nFwdLights = nDynamicLights;
		m_runtimeData.nFwdShadowLights = nShadowCastingLights;
	}

	//
	// Particles
	//
	IParticleManager* pParticleMgr = gEnv->p3DEngine->GetParticleManager();

	if (pParticleMgr)
	{
		SParticleCounts particleCounts;
		pParticleMgr->GetCounts(particleCounts);

		m_runtimeData.nParticles = (uint32)particleCounts.particles.rendered;

		cry_sprintf(entryBuffer, "Num Particles Rendered: %d (%u)", (int)particleCounts.particles.rendered, m_particlesBudget);

		if (particleCounts.particles.rendered <= m_particlesBudget)
		{
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
		}
		else
		{
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
			CryPerfHUDWarning(1.f, "Too Many Particles: %d", (int)particleCounts.particles.rendered);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if (gEnv->p3DEngine)
	{
		I3DEngine::SObjectsStreamingStatus objStats;
		gEnv->p3DEngine->GetObjectsStreamingStatus(objStats);

		float fMeshRequiredMB = (float)(objStats.nMemRequired) / (1024 * 1024);
		cry_sprintf(entryBuffer, "Mesh Required: %.2f (%dMB)", fMeshRequiredMB, objStats.nMeshPoolSize);
		if (fMeshRequiredMB < objStats.nMeshPoolSize)
		{
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
		}
		else
		{
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
			CryPerfHUDWarning(1.f, "Too Many Geometry: %.2fMB", fMeshRequiredMB);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if (gEnv->pRenderer)
	{
		STextureStreamingStats textureStats(true);
		gEnv->pRenderer->EF_Query(EFQ_GetTexStreamingInfo, textureStats);

		float fTexRequiredMB = (float)(textureStats.nRequiredStreamedTexturesSize) / (1024 * 1024);
		cry_sprintf(entryBuffer, "Textures Required: %.2f (%" PRISIZE_T "MB)", fTexRequiredMB, textureStats.nMaxPoolSize / (1024 * 1024));
		if (fTexRequiredMB < textureStats.nMaxPoolSize)
		{
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
		}
		else
		{
			m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
			CryPerfHUDWarning(1.f, "Too Many Textures: %.2fMB", fTexRequiredMB);
		}
	}
}
bool CRenderStatsWidget::ShouldUpdate()
{
	if (!m_pInfoBox->IsHidden() ||
	    m_pPerfHUD->WarningsWindowEnabled())
	{
		return true;
	}
	return false;
}

void CRenderStatsWidget::LoadBudgets(XmlNodeRef perfXML)
{
	if (perfXML)
	{
		XmlNodeRef xmlNode;

		if ((xmlNode = perfXML->findChild("fps")))
		{
			xmlNode->getAttr("value", m_fpsBudget);
		}

		if ((xmlNode = perfXML->findChild("drawPrim")))
		{
			xmlNode->getAttr("value", m_dpBudget);
		}

		if ((xmlNode = perfXML->findChild("tris")))
		{
			xmlNode->getAttr("value", m_polyBudget);
		}

		if ((xmlNode = perfXML->findChild("postEffects")))
		{
			xmlNode->getAttr("value", m_postEffectBudget);
		}

		if ((xmlNode = perfXML->findChild("shadowCastingLights")))
		{
			xmlNode->getAttr("value", m_shadowCastBudget);
		}

		if ((xmlNode = perfXML->findChild("particles")))
		{
			xmlNode->getAttr("value", m_particlesBudget);
		}
	}
}

void CRenderStatsWidget::SaveStats(XmlNodeRef statsXML)
{
	if (!ShouldUpdate())
	{
		//Force update of stats, widget may not be currently enabled
		Update();
	}

	if (statsXML)
	{
		XmlNodeRef renderNode;

		if ((renderNode = statsXML->newChild("RenderStats")))
		{
			XmlNodeRef child;

			if ((child = renderNode->newChild("fps")))
			{
				child->setAttr("value", m_runtimeData.fps);
			}

			if ((child = renderNode->newChild("hdr")))
			{
				child->setAttr("value", m_runtimeData.hdrEnabled);
			}

			if ((child = renderNode->newChild("renderThread")))
			{
				child->setAttr("value", m_runtimeData.renderThreadEnabled);
			}

			if ((child = renderNode->newChild("cameraPos")))
			{
				child->setAttr("x", m_runtimeData.cameraPos.x);
				child->setAttr("y", m_runtimeData.cameraPos.y);
				child->setAttr("z", m_runtimeData.cameraPos.z);
			}

			if ((child = renderNode->newChild("cameraRot")))
			{
				child->setAttr("x", m_runtimeData.cameraRot.x);
				child->setAttr("y", m_runtimeData.cameraRot.y);
				child->setAttr("z", m_runtimeData.cameraRot.z);
			}

			if ((child = renderNode->newChild("drawPrims")))
			{
				child->setAttr("value", m_runtimeData.nDrawPrims);
			}

			if ((child = renderNode->newChild("numPolys")))
			{
				child->setAttr("value", m_runtimeData.nPolys);
			}

			if ((child = renderNode->newChild("numPostEffects")))
			{
				child->setAttr("value", m_runtimeData.nPostEffects);
			}

			if ((child = renderNode->newChild("numFwdLights")))
			{
				child->setAttr("value", m_runtimeData.nFwdLights);
			}

			if ((child = renderNode->newChild("numFwdShadowLights")))
			{
				child->setAttr("value", m_runtimeData.nFwdShadowLights);
			}

			if ((child = renderNode->newChild("numDefLights")))
			{
				child->setAttr("value", m_runtimeData.nDefLights);
			}

			if ((child = renderNode->newChild("numDefShadowLights")))
			{
				child->setAttr("value", m_runtimeData.nDefShadowLights);
			}

			if ((child = renderNode->newChild("numDefCubeMaps")))
			{
				child->setAttr("value", m_runtimeData.nDefCubeMaps);
			}

			if ((child = renderNode->newChild("numParticles")))
			{
				child->setAttr("value", m_runtimeData.nParticles);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//Streaming Stats Widget
//////////////////////////////////////////////////////////////////////////

CStreamingStatsWidget::CStreamingStatsWidget(IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud) : ICryPerfHUDWidget(eWidget_StreamingStats)
{
	m_pPerfHUD = pPerfHud;
	m_pInfoBox = pPerfHud->CreateInfoMenuItem(pParentMenu, "Streaming", NULL, Rect(45, 200, 100, 300), true);

	//m_maxMeshSizeArroundMB = 0;
	//m_maxTextureSizeArroundMB = 0;
}

//////////////////////////////////////////////////////////////////////////
void CStreamingStatsWidget::Update()
{
	IRenderer* pRenderer = gEnv->pRenderer;

	char entryBuffer[CMiniInfoBox::MAX_TEXT_LENGTH] = { 0 };

	//Clear old entries
	m_pInfoBox->ClearEntries();

	I3DEngine::SObjectsStreamingStatus objStats;
	gEnv->p3DEngine->GetObjectsStreamingStatus(objStats);

	float fMeshRequiredMB = (float)(objStats.nMemRequired) / (1024 * 1024);
	cry_sprintf(entryBuffer, "Mesh Required: %.2f (%dMB)", fMeshRequiredMB, objStats.nMeshPoolSize);
	if (fMeshRequiredMB < objStats.nMeshPoolSize)
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_NORM, CPerfHUD::TEXT_SIZE_NORM);
	}
	else
	{
		m_pInfoBox->AddEntry(entryBuffer, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_NORM);
		CryPerfHUDWarning(1.f, "Too Many Geometry: %.2fMB", fMeshRequiredMB);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CStreamingStatsWidget::ShouldUpdate()
{
	if (!m_pInfoBox->IsHidden() ||
	    m_pPerfHUD->WarningsWindowEnabled())
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CStreamingStatsWidget::LoadBudgets(XmlNodeRef perfXML)
{
	/*
	   if(perfXML)
	   {
	   XmlNodeRef xmlNode;
	   if( (xmlNode=perfXML->findChild("MeshSizeArround")) )
	   {
	   xmlNode->getAttr("value", m_maxMeshSizeArroundMB);
	   }
	   if( (xmlNode=perfXML->findChild("TextureSizeArround")) )
	   {
	   xmlNode->getAttr("value", m_maxTextureSizeArroundMB);
	   }
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
//Warnings Widget
//////////////////////////////////////////////////////////////////////////

CWarningsWidget::CWarningsWidget(IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud) : ICryPerfHUDWidget(eWidget_Warnings)
{
	m_pInfoBox = pPerfHud->CreateInfoMenuItem(pParentMenu, "Warnings", NULL, Rect(890, 150, 900, 200) /*, true*/);

	m_nMainThreadId = CryGetCurrentThreadId();
}

void CWarningsWidget::Reset()
{
	m_warnings.clear();
}

void CWarningsWidget::Update()
{
	//must be Main thread for MT warnings to work
	assert(CryGetCurrentThreadId() == m_nMainThreadId);

	//Update from multithreaded queue
	while (!m_threadWarnings.empty())
	{
		SWarning warning;
		if (m_threadWarnings.try_pop(warning))
		{
			AddWarning(warning.remainingDuration, warning.text);
		}
	}

	float frameTime = gEnv->pTimer->GetRealFrameTime();

	//delete old warnings
	// [K01]: fixing Linux crash
	TSWarnings::iterator iter = m_warnings.begin();
	while (iter != m_warnings.end())
	{
		SWarning* pW = &(*iter);
		pW->remainingDuration -= frameTime;

		if (pW->remainingDuration <= 0.f)
			iter = m_warnings.erase(iter);
		else
			++iter;
	}

	int nWarnings = m_warnings.size();
	m_pInfoBox->ClearEntries();

	//display warnings
	for (int i = 0; i < nWarnings; i++)
	{
		m_pInfoBox->AddEntry(m_warnings[i].text, CPerfHUD::COL_ERROR, CPerfHUD::TEXT_SIZE_WARN);
	}
}

bool CWarningsWidget::ShouldUpdate()
{
	return !m_pInfoBox->IsHidden();
}

void CWarningsWidget::SaveStats(XmlNodeRef statsXML)
{
	if (statsXML)
	{
		const int nWarnings = m_warnings.size();

		if (nWarnings > 0)
		{
			XmlNodeRef warningNode;

			if ((warningNode = statsXML->newChild("warnings")))
			{
				XmlNodeRef child;

				for (int i = 0; i < nWarnings; i++)
				{
					if ((child = warningNode->newChild("warning")))
					{
						child->setAttr("value", m_warnings[i].text);
					}
				}
			}
		}
	}
}

void CWarningsWidget::AddWarningV(float duration, const char* fmt, va_list argList)
{
	char warningText[WARNING_LENGTH];

	cry_vsprintf(warningText, fmt, argList);

	AddWarning(duration, warningText);
}

void CWarningsWidget::AddWarning(float duration, const char* warning)
{
	if (CryGetCurrentThreadId() == m_nMainThreadId)
	{
		const size_t nWarnings = m_warnings.size();
		bool bNewWarning = true;

		ptrdiff_t nCompareLen = strlen(warning);
		const char* s = strchr(warning, ':');
		if (s)
		{
			nCompareLen = s - warning;
		}

		for (size_t i = 0; i < nWarnings; i++)
		{
			if (strncmp(m_warnings[i].text, warning, nCompareLen) == 0)
			{
				//warning already exists, update duration
				m_warnings[i].remainingDuration = duration;
				cry_strcpy(m_warnings[i].text, warning);
				bNewWarning = false;
				break;
			}
		}

		if (bNewWarning)
		{
			SWarning newWarning;
			newWarning.remainingDuration = duration;
			cry_strcpy(newWarning.text, warning);

			m_warnings.push_back(newWarning);
		}
	}
	else
	{
		//add to thread safe queue, warning will be added next update
		SWarning newWarning;
		newWarning.remainingDuration = duration;
		cry_strcpy(newWarning.text, warning);
		m_threadWarnings.push(newWarning);
	}

}

//////////////////////////////////////////////////////////////////////////
//Render Batch Stats Widget
//////////////////////////////////////////////////////////////////////////

CRenderBatchWidget::CRenderBatchWidget(IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud) : ICryPerfHUDWidget(eWidget_RenderBatchStats)
{
	m_pTable = pPerfHud->CreateTableMenuItem(pParentMenu, "Render Batch Stats");

	m_pRStatsCVar = GetISystem()->GetIConsole()->GetCVar("r_stats");

	m_displayMode = DISPLAY_MODE_BATCH_STATS;

	m_pTable->RemoveColumns();
	m_pTable->AddColumn("Name");
	m_pTable->AddColumn("DPs");
	m_pTable->AddColumn("Instances");
	m_pTable->AddColumn("ZPass");
	m_pTable->AddColumn("Shadows");
	m_pTable->AddColumn("General");
	m_pTable->AddColumn("Transparent");
	m_pTable->AddColumn("Misc");
}

void CRenderBatchWidget::Reset()
{
}

void CRenderBatchWidget::SaveStats(XmlNodeRef statsXML)
{
}

void CRenderBatchWidget::Enable(int mode)
{
	mode = min(mode, DISPLAY_MODE_NUM - 1);
	EDisplayMode newMode = (EDisplayMode)mode;

	bool bValidMode = false;

	if (m_displayMode != newMode)
	{
		//workaround for now,
		//since we poke renderer in order to gather stats
		switch (m_displayMode)
		{
		case DISPLAY_MODE_BATCH_STATS:
			gEnv->pRenderer->CollectDrawCallsInfo(false);
			break;

		case DISPLAY_MODE_GPU_TIMES:
			m_pRStatsCVar->Set(0);
			break;
		}

		switch (newMode)
		{
		case DISPLAY_MODE_BATCH_STATS:
			m_pTable->RemoveColumns();
			m_pTable->AddColumn("Name");
			m_pTable->AddColumn("DPs");
			m_pTable->AddColumn("Instances");
			m_pTable->AddColumn("ZPass");
			m_pTable->AddColumn("Shadows");
			m_pTable->AddColumn("General");
			m_pTable->AddColumn("Transparent");
			m_pTable->AddColumn("Misc");
			m_displayMode = newMode;
			break;

		case DISPLAY_MODE_GPU_TIMES:
			m_pTable->RemoveColumns();
			m_pTable->AddColumn("Name");
			m_pTable->AddColumn("Num Batches");
			m_pTable->AddColumn("Num Verts");
			m_pTable->AddColumn("Num Tris");
			m_displayMode = newMode;
			break;

		default:
			CryLogAlways("[Render Batch Stats] Attempting to set incorrect display mode set: %d", mode);
			break;
		}
	}

	m_pTable->Hide(false);
}

void CRenderBatchWidget::Disable()
{
	//ensure renderer is not doing unnecessary work
	m_pRStatsCVar->Set(0);
	gEnv->pRenderer->CollectDrawCallsInfo(false);
	m_pTable->Hide(true);
}

bool CRenderBatchWidget::ShouldUpdate()
{
	return !m_pTable->IsHidden();
}

void CRenderBatchWidget::Update()
{
	switch (m_displayMode)
	{
	case DISPLAY_MODE_BATCH_STATS:
		Update_ModeBatchStats();
		break;
	case DISPLAY_MODE_GPU_TIMES:
		Update_ModeGpuTimes();
		break;
	default:
		CryLogAlways("[Render Batch Stats]Incorrect Display mode set: %d", m_displayMode);
		break;
	}
}

void CRenderBatchWidget::Update_ModeBatchStats()
{
	#if !defined(_RELEASE)
	IRenderer* pRenderer = gEnv->pRenderer;

	pRenderer->CollectDrawCallsInfo(true);

	typedef std::map<string, BatchInfoPerPass>           BatchMap;
	typedef std::map<string, BatchInfoPerPass>::iterator BatchMapItor;

	//sorted Map elements
	std::vector<BatchInfoPerPass*> sortedBatchList;

	BatchMap batchMap;

	//mesh totals
	static const char* strMeshTotals = "TOTAL (Mesh)";
	BatchInfoPerPass totalsMesh;
	totalsMesh.col.set(255, 255, 255, 255);
	totalsMesh.name = strMeshTotals;

	int nDPs = gEnv->pRenderer->GetCurrentNumberOfDrawCalls();

	//scene totals
	static const char* strSceneTotals = "TOTAL (Scene)";

	BatchInfoPerPass totalsScene;
	totalsScene.col.set(0, 255, 255, 255);
	totalsScene.name = strSceneTotals;
	totalsScene.nBatches = nDPs;

	int unknownDPs = nDPs;

	m_pTable->ClearTable();

	IRenderer::RNDrawcallsMapMesh& drawCallsInfo = gEnv->pRenderer->GetDrawCallsInfoPerMesh();

	auto pEnd = drawCallsInfo.end();
	auto pItor = drawCallsInfo.begin();

	//Per RenderNode Stats
	for (; pItor != pEnd; ++pItor)
	{
		IRenderer::SDrawCallCountInfo& drawInfo = pItor->second;

		uint32 nDrawcalls = drawInfo.nShadows + drawInfo.nZpass + drawInfo.nGeneral + drawInfo.nTransparent + drawInfo.nMisc;

		const char* pRenderNodeName = drawInfo.meshName;
		const char* pNameShort = strrchr(pRenderNodeName, '/');

		if (pNameShort)
		{
			pRenderNodeName = pNameShort + 1;
		}

		BatchInfoPerPass batch;

		batch.name = pRenderNodeName;
		batch.nBatches = nDrawcalls;
		batch.nInstances = 1;
		batch.nZpass = drawInfo.nZpass;
		batch.nShadows = drawInfo.nShadows;
		batch.nGeneral = drawInfo.nGeneral;
		batch.nTransparent = drawInfo.nTransparent;
		batch.nMisc = drawInfo.nMisc;

		BatchMapItor batchIter = batchMap.find(string(pRenderNodeName));

		//already an entry for this stat obj - append details
		if (batchIter != batchMap.end())
		{
			BatchInfoPerPass& currentBatch = batchIter->second;
			currentBatch += batch;
		}
		else
		{
			//insert new entry into map
			std::pair<BatchMapItor, bool> newElem = batchMap.insert(BatchMapItor::value_type(string(pRenderNodeName), batch));
			BatchInfoPerPass& newBatch = newElem.first->second;
			sortedBatchList.push_back(&newBatch);
		}

		totalsMesh += batch;
	}

	unknownDPs -= totalsMesh.nBatches;

	//Flash
	{
		unsigned int numDPs = 0;
		unsigned int numTris = 0;
		if (gEnv->pScaleformHelper)
		{
			gEnv->pScaleformHelper->GetFlashRenderStats(numDPs, numTris);
		}

		if (numDPs)
		{
			static const char* s_strFlash = "FLASH";

			static BatchInfoPerPass s_flashBatch;

			s_flashBatch.Reset();
			s_flashBatch.name = s_strFlash;
			s_flashBatch.nBatches = numDPs;
			s_flashBatch.nInstances = 1;
			s_flashBatch.col.set(255, 255, 0, 255);

			unknownDPs -= s_flashBatch.nBatches;

			sortedBatchList.push_back(&s_flashBatch);
		}
	}

	//Particles
	if (gEnv->pConsole->GetCVar("e_Particles")->GetIVal() != 0)
	{
		SParticleCounts CurCounts;
		gEnv->pParticleManager->GetCounts(CurCounts);

		static const char* s_strParticles = "PARTICLES";

		static BatchInfoPerPass s_particlesBatch;

		s_particlesBatch.Reset();
		s_particlesBatch.name = s_strParticles;
		s_particlesBatch.nBatches = (uint16)CurCounts.emitters.rendered;
		s_particlesBatch.nInstances = 1;
		s_particlesBatch.col.set(255, 255, 0, 255);

		unknownDPs -= s_particlesBatch.nBatches;

		sortedBatchList.push_back(&s_particlesBatch);
	}

	//
	// Unknown counts (sceneDP - sum of batches)
	//
	//could be -ve or +ve (-ve for conditional rendering)
	if (unknownDPs != 0)
	{
		static const char* s_strUnknown = "Unknown";
		static BatchInfoPerPass s_unknownBatch;
		s_unknownBatch.Reset();
		s_unknownBatch.name = s_strUnknown;
		s_unknownBatch.nBatches = max(0, unknownDPs);
		s_unknownBatch.col.set(255, 255, 0, 255);
		sortedBatchList.push_back(&s_unknownBatch);
	}

	//Scene Totals
	m_pTable->AddData(0, totalsScene.col, totalsScene.name);
	m_pTable->AddData(1, totalsScene.col, "%d", totalsScene.nBatches);
	m_pTable->AddData(2, totalsScene.col, "%d", totalsScene.nInstances);
	m_pTable->AddData(3, totalsScene.col, "%d", totalsScene.nZpass);
	m_pTable->AddData(4, totalsScene.col, "%d", totalsScene.nShadows);
	m_pTable->AddData(5, totalsScene.col, "%d", totalsScene.nGeneral);
	m_pTable->AddData(6, totalsScene.col, "%d", totalsScene.nTransparent);
	m_pTable->AddData(7, totalsScene.col, "%d", totalsScene.nMisc);

	//Mesh Totals
	m_pTable->AddData(0, totalsMesh.col, totalsMesh.name);
	m_pTable->AddData(1, totalsMesh.col, "%d", totalsMesh.nBatches);
	m_pTable->AddData(2, totalsMesh.col, "%d", totalsMesh.nInstances);
	m_pTable->AddData(3, totalsMesh.col, "%d", totalsMesh.nZpass);
	m_pTable->AddData(4, totalsMesh.col, "%d", totalsMesh.nShadows);
	m_pTable->AddData(5, totalsMesh.col, "%d", totalsMesh.nGeneral);
	m_pTable->AddData(6, totalsMesh.col, "%d", totalsMesh.nTransparent);
	m_pTable->AddData(7, totalsMesh.col, "%d", totalsMesh.nMisc);

	if (int nBatches = sortedBatchList.size())
	{
		std::sort(sortedBatchList.begin(), sortedBatchList.end(), BatchInfoSortPerPass());

		for (int i = 0; i < nBatches; i++)
		{
			BatchInfoPerPass* batch = sortedBatchList[i];

			int nInstances = max(1, (int)batch->nInstances);

			m_pTable->AddData(0, batch->col, batch->name);

			//Due to different render meshes with the same name, averaged
			//stats are a bit confusing, disabling for now
			if (0 && batch->nInstances > 1)
			{
				m_pTable->AddData(1, batch->col, "%d (%d)", batch->nBatches, batch->nBatches / nInstances);
				m_pTable->AddData(2, batch->col, "%d", batch->nInstances);
				m_pTable->AddData(3, batch->col, "%d (%d)", batch->nZpass, batch->nZpass / nInstances);
				m_pTable->AddData(4, batch->col, "%d (%d)", batch->nShadows, batch->nShadows / nInstances);
				m_pTable->AddData(5, batch->col, "%d (%d)", batch->nGeneral, batch->nGeneral / nInstances);
				m_pTable->AddData(6, batch->col, "%d (%d)", batch->nTransparent, batch->nTransparent / nInstances);
				m_pTable->AddData(7, batch->col, "%d (%d)", batch->nMisc, batch->nMisc / nInstances);
			}
			else
			{
				m_pTable->AddData(1, batch->col, "%d", batch->nBatches);
				m_pTable->AddData(2, batch->col, "%d", batch->nInstances);
				m_pTable->AddData(3, batch->col, "%d", batch->nZpass);
				m_pTable->AddData(4, batch->col, "%d", batch->nShadows);
				m_pTable->AddData(5, batch->col, "%d", batch->nGeneral);
				m_pTable->AddData(6, batch->col, "%d", batch->nTransparent);
				m_pTable->AddData(7, batch->col, "%d", batch->nMisc);
			}
		}
	}
	#else
	m_pTable->ClearTable();
	m_pTable->AddData(0, ColorB(255, 0, 0, 255), "Not supported in Release builds");
	#endif//RELEASE (DO_RENDERSTATS - not available in CrySystem)
}

void CRenderBatchWidget::Update_ModeGpuTimes()
{
	m_pTable->ClearTable();
	m_pTable->AddData(0, ColorB(255, 0, 0, 255), "Not supported for this platform");
}

#endif //USE_PERFHUD
