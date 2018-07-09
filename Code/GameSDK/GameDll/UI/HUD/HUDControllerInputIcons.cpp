// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "HUDControllerInputIcons.h"

#include "HUDUtils.h"
#include "IActionMapManager.h"
#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include "Utility/CryWatch.h"
#include <CryString/StringUtils.h>

#if defined(_RELEASE)
#define WATCH_CONTROLLER_INPUT_ICON_FAILURES			(0)
#define ASSERT_ON_CONTROLLER_INPUT_ICON_FAILURES	(0)
#elif defined(PROFILE)
#define WATCH_CONTROLLER_INPUT_ICON_FAILURES			(CRY_WATCH_ENABLED && 1)
#define ASSERT_ON_CONTROLLER_INPUT_ICON_FAILURES	(0)
#else
#define WATCH_CONTROLLER_INPUT_ICON_FAILURES			(CRY_WATCH_ENABLED && 1)
#define ASSERT_ON_CONTROLLER_INPUT_ICON_FAILURES	(1)
#endif

void CControllerInputRenderInfo::Clear()
{
	m_type = kCITV_none;
	m_text[0] = '\0';
	m_texture = NULL;
}

void CControllerInputRenderInfo::operator=(const CControllerInputRenderInfo & fromThis)
{
	m_type = fromThis.m_type;
	memcpy (m_text, fromThis.m_text, sizeof(m_text));
	m_texture = fromThis.m_texture;
}

bool CControllerInputRenderInfo::SetIcon(const char * text)
{
	m_type = kCITV_icon;
	m_texture = GetTexture( text );
	assert(m_texture);
	return (m_texture != NULL);
}

bool CControllerInputRenderInfo::SetText(const char * text)
{
	m_type = kCITV_text;
	return cry_strcpy(m_text, CHUDUtils::LocalizeString(text, NULL, NULL));
}

bool CControllerInputRenderInfo::CreateForInput(const char * mapName, const char * inputName)
{
	CRY_ASSERT(gEnv);
	CRY_ASSERT(gEnv->pGameFramework);
	CRY_ASSERT(gEnv->pGameFramework->GetIActionMapManager());

	IActionMap* pActionMap = gEnv->pGameFramework->GetIActionMapManager()->GetActionMap(mapName);
	if (pActionMap)
	{
		const IActionMapAction* pAction = pActionMap->GetAction(ActionId(inputName));
		if (!pAction)
			return false;

		int iNumActionInputs = pAction->GetNumActionInputs();
		for (int i = 0; i < iNumActionInputs; i++)
		{
			const SActionInput* pActionInput = pAction->GetActionInput(i);
			CRY_ASSERT(pActionInput != NULL);

			// TODO: Make data driven!

#if CRY_PLATFORM_ORBIS
			if (0 == strcmp(pActionInput->input, "pad_square"))
			{
				return SetIcon("controller_face_left");
			}
			else if (0 == strcmp(pActionInput->input, "pad_cross"))
			{
				return SetIcon("controller_face_down");
			}
			else if (0 == strcmp(pActionInput->input, "pad_circle"))
			{
				return SetIcon("controller_face_right");
			}
			else if (0 == strcmp(pActionInput->input, "pad_triangle"))
			{
				return SetIcon("controller_face_up");
			}
			else if (0 == strcmp(pActionInput->input, "pad_r2"))
			{
				return SetText("@ui_inputName_rightTrigger");	// TODO: An icon would be better...
			}
			else if (0 == strcmp(pActionInput->input, "pad_l2"))
			{
				return SetText("@ui_inputName_leftTrigger");	// TODO: An icon would be better...
			}
			else if (0 == strcmp(pActionInput->input, "pad_r3"))
			{
				return SetText("@ui_inputName_clickRightStick");	// TODO: An icon would be better...
			}
			else if (0 == strcmp(pActionInput->input, "pad_l3"))
			{
				return SetText("@ui_inputName_clickLeftStick");	// TODO: An icon would be better...
			}
#else
			if (0 == strcmp(pActionInput->input, "xi_x"))
			{
				return SetIcon("controller_face_left");
			}
			else if (0 == strcmp(pActionInput->input, "xi_a"))
			{
				return SetIcon("controller_face_down");
			}
			else if (0 == strcmp(pActionInput->input, "xi_b"))
			{
				return SetIcon("controller_face_right");
			}
			else if (0 == strcmp(pActionInput->input, "xi_y"))
			{
				return SetIcon("controller_face_up");
			}
			else if (0 == strcmp(pActionInput->input, "xi_triggerr_btn"))
			{
				return SetText("@ui_inputName_rightTrigger");	// TODO: An icon would be better...
			}
			else if (0 == strcmp(pActionInput->input, "xi_triggerl_btn"))
			{
				return SetText("@ui_inputName_leftTrigger");	// TODO: An icon would be better...
			}
			else if (0 == strcmp(pActionInput->input, "xi_thumbr"))
			{
				return SetText("@ui_inputName_clickRightStick");	// TODO: An icon would be better...
			}
			else if (0 == strcmp(pActionInput->input, "xi_thumbl"))
			{
				return SetText("@ui_inputName_clickLeftStick");	// TODO: An icon would be better...
			}
#endif
		}
	}

#if WATCH_CONTROLLER_INPUT_ICON_FAILURES || ASSERT_ON_CONTROLLER_INPUT_ICON_FAILURES
	string error;

	if (pActionMap)
	{
		const IActionMapAction* pAction = pActionMap->GetAction(ActionId(inputName));
		if (pAction)
		{
			int iNumActionInputs = pAction->GetNumActionInputs();
			if (iNumActionInputs > 0)
			{			
				error.Format("%d input(s) =", iNumActionInputs);
			}
			else
			{
				error = "no such input";
			}

			for (int i = 0; i < iNumActionInputs; i++)
			{
				const SActionInput* pActionInput = pAction->GetActionInput(i);
				CRY_ASSERT(pActionInput != NULL);

				error = string(error + " '" + pActionInput->input.c_str() + "'");
			}
		}
		else
		{
			error.Format("\"%s\" action not found", mapName);
		}
	}
	else
	{
		error.Format("\"%s\" action map not found", mapName);
	}

#if WATCH_CONTROLLER_INPUT_ICON_FAILURES
	CryWatch("No HUD prompt icon/text for \"%s.%s\": %s", mapName, inputName, error.c_str());
#endif

#if ASSERT_ON_CONTROLLER_INPUT_ICON_FAILURES
	CRY_ASSERT_MESSAGE(false, string().Format("No HUD prompt icon/text for \"%s.%s\": %s", mapName, inputName, error.c_str()));
#endif

#endif

	return false;
}

// "This is just a dummy function. If it will be needed implement simple xml reading for textures <-> message mapping"
ITexture* CControllerInputRenderInfo::GetTexture(const char* name)
{
	static bool isInit = false;
	static std::map<string, ITexture*> textures;
	if (!isInit)
	{
		// TODO: read some xml for icon texture definitions...
		isInit = true;
	}
	std::map<string, ITexture*>::const_iterator it = textures.find(name);
	return it != textures.end() ? it->second : NULL;
}
