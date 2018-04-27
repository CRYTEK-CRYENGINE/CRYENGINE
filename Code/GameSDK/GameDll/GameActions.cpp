// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameActions.h"
#include "Game.h"

#define DECL_ACTION(name) name = #name;
CGameActions::CGameActions()
: m_pFilterNoMove(nullptr)
, m_pFilterNoMouse(nullptr)
, m_pFilterMouseX(nullptr)
, m_pFilterMouseY(nullptr)
, m_pFilterTutorialNoMove(nullptr)
, m_pFilterNoWeaponCustomization(nullptr)
, m_pFilterNoFireModeSwitch(nullptr)
, m_pFilterMindControlMenu(nullptr)
, m_pFilterFreezeTime(nullptr)
, m_pFilterHostMigration(nullptr)
, m_pFilterMPPreGameFreeze(nullptr)
, m_pFilterNoVehicleExit(nullptr)
, m_pFilterMPRadio(nullptr)
, m_pFilterMPChat(nullptr)
, m_pFilterCutscene(nullptr)
, m_pFilterCutsceneNoPlayer(nullptr)
, m_pFilterCutsceneTrain(nullptr)
, m_pFilterCutscenePlayerMoving(nullptr)
, m_pFilterVehicleNoSeatChangeAndExit(nullptr)
, m_pFilterNoConnectivity(nullptr)
, m_pFilterIngameMenu(nullptr)
, m_pFilterScoreboard(nullptr)
, m_pFilterStrikePointer(nullptr)
, m_pFilterUseKeyOnly(nullptr)
, m_pFilterInfictionMenu(nullptr)
, m_pFilterMPWeaponCustomizationMenu(nullptr)
, m_pFilterLedgeGrab(nullptr)
, m_pFilterVault(nullptr)
, m_pFilterItemPickup(nullptr)
, m_pFilterNotYetSpawned(nullptr)
, m_pFilterWarningPopup(nullptr)
, m_pFilterButtonMashingSequence(nullptr)
, m_pFilterUIOnly(nullptr)
{
#include "GameActions.actions"
}
#undef DECL_ACTION

CGameActions::~CGameActions()
{
	IActionMapManager* pActionMapManager = g_pGame->GetIGameFramework()->GetIActionMapManager();
	
	if (pActionMapManager)
	{
		pActionMapManager->UnregisterActionMapEventListener(this);
	}
}

void CGameActions::OnActionMapEvent(const SActionMapEvent& event)
{
	if (event.event == SActionMapEvent::eActionMapManagerEvent_ActionMapsInitialized)
	{
		InitActions();
	}
}

void CGameActions::InitActions()
{
	CreateFilterNoMove();
	CreateFilterNoMouse();
	CreateFilterMouseX();
	CreateFilterMouseY();
	CreateTutorialNoMove();
	CreateFilterNoWeaponCustomization();
	CreateFilterNoFireModeSwitch();
	CreateFilterWarningPopup();
	CreateFilterFreezeTime();
	CreateFilterHostMigration();
	CreateFilterMPPreGameFreeze();
	CreateFilterNoVehicleExit();
	CreateFilterMPRadio();
	CreateFilterMPChat();
	CreateFilterCutscene();
	CreateFilterCutsceneNoPlayer();
	CreateFilterCutsceneTrain();
	CreateFilterCutscenePlayerMoving();
	CreateFilterVehicleNoSeatChangeAndExit();
	CreateFilterNoConnectivity();
	CreateFilterIngameMenu();
	CreateFilterScoreboard();
	CreateFilterStrikePointer();
	CreateFilterUseKeyOnly();
	CreateFilterInfictionMenu();
	CreateFilterMPWeaponCustomizationMenu();
	CreateFilterLedgeGrab();
	CreateFilterVault();
	CreateItemPickup();
	CreateFilterNotYetSpawned();
	CreateFilterButtonMashingSequence();
	CreateFilterUIOnly();
}

void CGameActions::CreateFilterNotYetSpawned()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterNotYetSpawned = pActionMapMan->CreateActionFilter("not_yet_spawned", eAFT_ActionFail);
	m_pFilterNotYetSpawned->Filter(binoculars);
	m_pFilterNotYetSpawned->Filter(zoom);
	m_pFilterNotYetSpawned->Filter(xi_zoom);
}

void CGameActions::CreateFilterNoMove()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterNoMove = pActionMapMan->CreateActionFilter("no_move", eAFT_ActionFail);
	m_pFilterNoMove->Filter(crouch);
	m_pFilterNoMove->Filter(jump);
	m_pFilterNoMove->Filter(moveleft);
	m_pFilterNoMove->Filter(moveright);
	m_pFilterNoMove->Filter(moveforward);
	m_pFilterNoMove->Filter(moveback);
	m_pFilterNoMove->Filter(sprint);
	m_pFilterNoMove->Filter(xi_movey);
	m_pFilterNoMove->Filter(xi_movex);
}

void CGameActions::CreateFilterNoMouse()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterNoMouse = pActionMapMan->CreateActionFilter("no_mouse", eAFT_ActionFail);
	m_pFilterNoMouse->Filter(attack1);
	m_pFilterNoMouse->Filter(v_attack1);
	m_pFilterNoMouse->Filter(v_attack2);
	m_pFilterNoMouse->Filter(rotateyaw);
	m_pFilterNoMouse->Filter(v_rotateyaw);
	m_pFilterNoMouse->Filter(rotatepitch);
	m_pFilterNoMouse->Filter(v_rotatepitch);
	m_pFilterNoMouse->Filter(nextitem);
	m_pFilterNoMouse->Filter(previtem);
	m_pFilterNoMouse->Filter(toggle_explosive);
	m_pFilterNoMouse->Filter(toggle_weapon);
	m_pFilterNoMouse->Filter(toggle_grenade);
	m_pFilterNoMouse->Filter(handgrenade);
	m_pFilterNoMouse->Filter(zoom);
	m_pFilterNoMouse->Filter(reload);
	m_pFilterNoMouse->Filter(use);
	m_pFilterNoMouse->Filter(xi_grenade);
	m_pFilterNoMouse->Filter(xi_handgrenade);
	m_pFilterNoMouse->Filter(xi_zoom);
	m_pFilterNoMouse->Filter(jump);
	m_pFilterNoMouse->Filter(binoculars);
	m_pFilterNoMouse->Filter(attack1_xi);
	m_pFilterNoMouse->Filter(attack2_xi);
}

void CGameActions::CreateFilterMouseX()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterMouseX = pActionMapMan->CreateActionFilter("no_mouseX", eAFT_ActionFail);
	m_pFilterMouseX->Filter(rotateyaw);
	m_pFilterMouseX->Filter(xi_rotateyaw);
}

void CGameActions::CreateFilterMouseY()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterMouseY = pActionMapMan->CreateActionFilter("no_mouseY", eAFT_ActionFail);
	m_pFilterMouseY->Filter(rotatepitch);
	m_pFilterMouseY->Filter(xi_rotatepitch);
}

void CGameActions::CreateTutorialNoMove()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterWarningPopup = pActionMapMan->CreateActionFilter("tutorial_no_move", eAFT_ActionFail);
	m_pFilterWarningPopup->Filter(moveforward);
	m_pFilterWarningPopup->Filter(moveback);
	m_pFilterWarningPopup->Filter(moveleft);
	m_pFilterWarningPopup->Filter(moveright);
	m_pFilterWarningPopup->Filter(xi_movey);
	m_pFilterWarningPopup->Filter(xi_movex);
}

void CGameActions::CreateFilterNoWeaponCustomization()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterNoWeaponCustomization = pActionMapMan->CreateActionFilter("no_weaponcustomization", eAFT_ActionFail);
	m_pFilterNoWeaponCustomization->Filter(menu_open_customizeweapon);
}

void CGameActions::CreateFilterNoFireModeSwitch()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterNoFireModeSwitch = pActionMapMan->CreateActionFilter("no_firemodeswitch", eAFT_ActionFail);
	m_pFilterNoFireModeSwitch->Filter(weapon_change_firemode);
}

void CGameActions::CreateFilterWarningPopup()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterWarningPopup = pActionMapMan->CreateActionFilter("warning_popup", eAFT_ActionFail);
	m_pFilterWarningPopup->Filter(moveforward);
	m_pFilterWarningPopup->Filter(moveback);
	m_pFilterWarningPopup->Filter(moveleft);
	m_pFilterWarningPopup->Filter(moveright);
	m_pFilterWarningPopup->Filter(xi_rotateyaw);
	m_pFilterWarningPopup->Filter(xi_rotatepitch);
	m_pFilterWarningPopup->Filter(attack1);
	m_pFilterWarningPopup->Filter(attack1_xi);
	m_pFilterWarningPopup->Filter(sprint);
}

void CGameActions::CommonCreateFilterFreeze(IActionFilter *pFilter)
{
	// Common freeze filters between FreezeTime and MPPreGameFreeze
	pFilter->Filter(menu_scoreboard_open);
	pFilter->Filter(menu_scoreboard_close);

	pFilter->Filter(menu_open_customizeweapon);
	pFilter->Filter(menu_close_customizeweapon);
	
	pFilter->Filter(menu_scrollup);
	pFilter->Filter(menu_scrolldown);
	pFilter->Filter(menu_fcommand1);
	pFilter->Filter(menu_fcommand2);

	pFilter->Filter(menu_switchtab_left);
	pFilter->Filter(menu_switchtab_right);

	pFilter->Filter(menu_up);
	pFilter->Filter(menu_down);
	pFilter->Filter(menu_left);
	pFilter->Filter(menu_right);

	pFilter->Filter(menu_toggle_barrel);
	pFilter->Filter(menu_toggle_bottom);
	pFilter->Filter(menu_toggle_scope);
	pFilter->Filter(menu_toggle_ammo);

	pFilter->Filter(menu_confirm);
	pFilter->Filter(menu_confirm2);
	pFilter->Filter(menu_back);
	pFilter->Filter(menu_delete);
	pFilter->Filter(menu_apply);
	pFilter->Filter(menu_default);
	pFilter->Filter(menu_back_select);

	pFilter->Filter(menu_input_1);
	pFilter->Filter(menu_input_2);

	pFilter->Filter(menu_assetpause);
	pFilter->Filter(menu_assetzoom);

	pFilter->Filter(menu_friends_open);
}


void CGameActions::CreateFilterFreezeTime()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterFreezeTime = pActionMapMan->CreateActionFilter("freezetime", eAFT_ActionPass);

	CommonCreateFilterFreeze(m_pFilterFreezeTime);
}

void CGameActions::CreateFilterHostMigration()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterHostMigration = pActionMapMan->CreateActionFilter("hostmigration", eAFT_ActionPass);

	CommonCreateFilterFreeze(m_pFilterHostMigration);
}

void CGameActions::CreateFilterMPPreGameFreeze()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterMPPreGameFreeze = pActionMapMan->CreateActionFilter("pregamefreeze", eAFT_ActionPass);

	CommonCreateFilterFreeze(m_pFilterMPPreGameFreeze);

	m_pFilterMPPreGameFreeze->Filter(rotateyaw);
	m_pFilterMPPreGameFreeze->Filter(rotatepitch);
	m_pFilterMPPreGameFreeze->Filter(xi_rotateyaw);
	m_pFilterMPPreGameFreeze->Filter(xi_rotatepitch);

	m_pFilterMPPreGameFreeze->Filter(hud_openchat);
	m_pFilterMPPreGameFreeze->Filter(hud_openteamchat);
}

void CGameActions::CreateFilterNoVehicleExit()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterNoVehicleExit = pActionMapMan->CreateActionFilter("no_vehicle_exit", eAFT_ActionFail);
	m_pFilterNoVehicleExit->Filter(use);
}

void CGameActions::CreateFilterMPRadio()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterMPRadio = pActionMapMan->CreateActionFilter("mp_radio", eAFT_ActionFail);
	m_pFilterNoMouse->Filter(toggle_explosive);
	m_pFilterNoMouse->Filter(toggle_weapon);
	m_pFilterNoMouse->Filter(toggle_grenade);
	m_pFilterMPRadio->Filter(handgrenade);
	m_pFilterMPRadio->Filter(v_changeseat1);
	m_pFilterMPRadio->Filter(v_changeseat2);
	m_pFilterMPRadio->Filter(v_changeseat3);
	m_pFilterMPRadio->Filter(v_changeseat4);
	m_pFilterMPRadio->Filter(v_changeseat5);
	m_pFilterMPRadio->Filter(v_changeseat);
}

void CGameActions::CreateFilterMPChat()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterMPChat = pActionMapMan->CreateActionFilter("mp_chat", eAFT_ActionPass);

	// Disabled all actions when typing a chat message, add any exceptions here
}

void CGameActions::CreateFilterCutscene()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterCutscene = pActionMapMan->CreateActionFilter("cutscene", eAFT_ActionFail);	
	m_pFilterCutscene->Filter(binoculars);
}

void CGameActions::CreateFilterCutscenePlayerMoving()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterCutscenePlayerMoving = pActionMapMan->CreateActionFilter("cutscene_player_moving", eAFT_ActionPass);	
	m_pFilterCutscenePlayerMoving->Filter(rotateyaw);
	m_pFilterCutscenePlayerMoving->Filter(rotatepitch);
	m_pFilterCutscenePlayerMoving->Filter(xi_rotateyaw);
	m_pFilterCutscenePlayerMoving->Filter(xi_rotatepitch);
	m_pFilterCutscenePlayerMoving->Filter(moveleft);
	m_pFilterCutscenePlayerMoving->Filter(moveright);
	m_pFilterCutscenePlayerMoving->Filter(moveforward);
	m_pFilterCutscenePlayerMoving->Filter(moveback);
	m_pFilterCutscenePlayerMoving->Filter(xi_movex);
	m_pFilterCutscenePlayerMoving->Filter(xi_movey);
	m_pFilterCutscenePlayerMoving->Filter(skip_cutscene);
}


void CGameActions::CreateFilterCutsceneNoPlayer()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterCutsceneNoPlayer = pActionMapMan->CreateActionFilter("cutscene_no_player", eAFT_ActionPass);
	
	// Disabled all actions when in cutscene, add any exceptions here

	//Camera input for cinematics
	m_pFilterCutsceneNoPlayer->Filter(rotateyaw);
	m_pFilterCutsceneNoPlayer->Filter(rotatepitch);
	m_pFilterCutsceneNoPlayer->Filter(xi_rotateyaw);
	m_pFilterCutsceneNoPlayer->Filter(xi_rotatepitch);
	m_pFilterCutsceneNoPlayer->Filter(skip_cutscene);
	m_pFilterCutsceneNoPlayer->Filter(skip_loadingscreen);
	m_pFilterCutsceneNoPlayer->Filter(skip_loadingscreen_switched);
	m_pFilterCutsceneNoPlayer->Filter(attack1_cine);
	m_pFilterCutsceneNoPlayer->Filter(attack2_cine);

	m_pFilterCutsceneNoPlayer->Filter(menu_scrollup);
	m_pFilterCutsceneNoPlayer->Filter(menu_scrolldown);
	m_pFilterCutsceneNoPlayer->Filter(menu_switchtab_left);
	m_pFilterCutsceneNoPlayer->Filter(menu_switchtab_right);
	m_pFilterCutsceneNoPlayer->Filter(menu_up);
	m_pFilterCutsceneNoPlayer->Filter(menu_down);
	m_pFilterCutsceneNoPlayer->Filter(menu_left);
	m_pFilterCutsceneNoPlayer->Filter(menu_right);
	m_pFilterCutsceneNoPlayer->Filter(menu_map_zoomout);
	m_pFilterCutsceneNoPlayer->Filter(menu_map_zoomin);
	m_pFilterCutsceneNoPlayer->Filter(menu_confirm);
	m_pFilterCutsceneNoPlayer->Filter(menu_confirm2);
	m_pFilterCutsceneNoPlayer->Filter(menu_back);
	m_pFilterCutsceneNoPlayer->Filter(menu_exit);
	m_pFilterCutsceneNoPlayer->Filter(menu_delete);
	m_pFilterCutsceneNoPlayer->Filter(menu_apply);
	m_pFilterCutsceneNoPlayer->Filter(menu_default);
	m_pFilterCutsceneNoPlayer->Filter(menu_back_select);
	m_pFilterCutsceneNoPlayer->Filter(menu_tab);
	m_pFilterCutsceneNoPlayer->Filter(menu_friends_open);
	m_pFilterCutsceneNoPlayer->Filter(menu_input_1);
	m_pFilterCutsceneNoPlayer->Filter(menu_input_2);
	m_pFilterCutsceneNoPlayer->Filter(menu_assetpause);
	m_pFilterCutsceneNoPlayer->Filter(menu_assetzoom);
	m_pFilterCutsceneNoPlayer->Filter(minigame_decrypt_input_1);
	m_pFilterCutsceneNoPlayer->Filter(minigame_decrypt_input_2);
	m_pFilterCutsceneNoPlayer->Filter(minigame_decrypt_input_3);
	m_pFilterCutsceneNoPlayer->Filter(minigame_decrypt_input_4);
	m_pFilterCutsceneNoPlayer->Filter(minigame_decrypt_quit);
	m_pFilterCutsceneNoPlayer->Filter(minigame_ingame_quit);
}

void CGameActions::CreateFilterCutsceneTrain()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterCutsceneTrain = pActionMapMan->CreateActionFilter("cutscene_train", eAFT_ActionFail);

	m_pFilterCutsceneTrain->Filter(skip_cutscene);

	m_pFilterCutsceneTrain->Filter(crouch);
	m_pFilterCutsceneTrain->Filter(jump);
	m_pFilterCutsceneTrain->Filter(moveleft);
	m_pFilterCutsceneTrain->Filter(moveright);
	m_pFilterCutsceneTrain->Filter(moveforward);
	m_pFilterCutsceneTrain->Filter(moveback);
	m_pFilterCutsceneTrain->Filter(sprint);
	m_pFilterCutsceneTrain->Filter(xi_movey);
	m_pFilterCutsceneTrain->Filter(xi_movex);
}

void CGameActions::CreateFilterVehicleNoSeatChangeAndExit()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterVehicleNoSeatChangeAndExit = pActionMapMan->CreateActionFilter("vehicle_no_seat_change_and_exit", eAFT_ActionFail);
	m_pFilterVehicleNoSeatChangeAndExit->Filter(v_exit);
	m_pFilterVehicleNoSeatChangeAndExit->Filter(v_changeseat);
	m_pFilterVehicleNoSeatChangeAndExit->Filter(v_changeseat1);
	m_pFilterVehicleNoSeatChangeAndExit->Filter(v_changeseat2);
	m_pFilterVehicleNoSeatChangeAndExit->Filter(v_changeseat3);
	m_pFilterVehicleNoSeatChangeAndExit->Filter(v_changeseat4);
	m_pFilterVehicleNoSeatChangeAndExit->Filter(v_changeseat5);
}

void CGameActions::CreateFilterNoConnectivity()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterNoConnectivity = pActionMapMan->CreateActionFilter("no_connectivity", eAFT_ActionPass);
	m_pFilterNoConnectivity->Filter(scores);
}

void CGameActions::CreateFilterIngameMenu()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterIngameMenu = pActionMapMan->CreateActionFilter("ingame_menu", eAFT_ActionPass);

	// Disabled all actions when in ingame menu, add any exceptions here

	// Menu actions
	m_pFilterIngameMenu->Filter(menu_open_customizeweapon);
	m_pFilterIngameMenu->Filter(menu_close_customizeweapon);
	m_pFilterIngameMenu->Filter(menu_friends_open);
	
	m_pFilterIngameMenu->Filter(menu_map_zoomout);
	m_pFilterIngameMenu->Filter(menu_map_zoomin);

	m_pFilterIngameMenu->Filter(menu_scrollup);
	m_pFilterIngameMenu->Filter(menu_scrolldown);
	m_pFilterIngameMenu->Filter(menu_fcommand1);
	m_pFilterIngameMenu->Filter(menu_fcommand2);

	m_pFilterIngameMenu->Filter(menu_switchtab_left);
	m_pFilterIngameMenu->Filter(menu_switchtab_right);

	m_pFilterIngameMenu->Filter(menu_toggle_barrel);
	m_pFilterIngameMenu->Filter(menu_toggle_bottom);
	m_pFilterIngameMenu->Filter(menu_toggle_scope);
	m_pFilterIngameMenu->Filter(menu_toggle_ammo);

	m_pFilterIngameMenu->Filter(menu_confirm);
	m_pFilterIngameMenu->Filter(menu_confirm2);
	m_pFilterIngameMenu->Filter(menu_back);
	m_pFilterIngameMenu->Filter(menu_exit);
	m_pFilterIngameMenu->Filter(menu_delete);
	m_pFilterIngameMenu->Filter(menu_apply);
	m_pFilterIngameMenu->Filter(menu_default);
	m_pFilterIngameMenu->Filter(menu_back_select);

	m_pFilterIngameMenu->Filter(menu_input_1);
	m_pFilterIngameMenu->Filter(menu_input_2);
	
	m_pFilterIngameMenu->Filter(menu_assetpause);
	m_pFilterIngameMenu->Filter(menu_assetzoom);

	m_pFilterIngameMenu->Filter(menu_up);
	m_pFilterIngameMenu->Filter(menu_down);
	m_pFilterIngameMenu->Filter(menu_left);
	m_pFilterIngameMenu->Filter(menu_right);

	m_pFilterIngameMenu->Filter(voice_chat_talk);

	m_pFilterIngameMenu->Filter(skip_cutscene);
	m_pFilterIngameMenu->Filter(skip_loadingscreen);
	m_pFilterIngameMenu->Filter(skip_loadingscreen_switched);
}


void CGameActions::CreateFilterScoreboard()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterScoreboard = pActionMapMan->CreateActionFilter("scoreboard", eAFT_ActionPass);

	// Disabled all actions when in scoreboard menu, add any exceptions here
	m_pFilterScoreboard->Filter(menu_scrollup);
	m_pFilterScoreboard->Filter(menu_scrolldown);
	m_pFilterScoreboard->Filter(menu_fcommand1);
	m_pFilterScoreboard->Filter(menu_fcommand2);

	m_pFilterScoreboard->Filter(menu_confirm);
	m_pFilterScoreboard->Filter(menu_confirm2);
	m_pFilterScoreboard->Filter(menu_back);
	m_pFilterScoreboard->Filter(menu_delete);
	m_pFilterScoreboard->Filter(menu_apply);
	m_pFilterScoreboard->Filter(menu_default);
	m_pFilterScoreboard->Filter(menu_back_select);
	m_pFilterScoreboard->Filter(menu_scoreboard_open);
	m_pFilterScoreboard->Filter(menu_scoreboard_close);

	m_pFilterScoreboard->Filter(menu_input_1);
	m_pFilterScoreboard->Filter(menu_input_2);

	m_pFilterScoreboard->Filter(menu_up);
	m_pFilterScoreboard->Filter(menu_down);
	m_pFilterScoreboard->Filter(menu_left);
	m_pFilterScoreboard->Filter(menu_right);

	m_pFilterScoreboard->Filter(voice_chat_talk);

	// Player can still move (On PC)
	m_pFilterScoreboard->Filter(moveleft);
	m_pFilterScoreboard->Filter(moveright);
	m_pFilterScoreboard->Filter(moveforward);
	m_pFilterScoreboard->Filter(moveback);
	m_pFilterScoreboard->Filter(xi_movex);
	m_pFilterScoreboard->Filter(xi_movey);
}

void CGameActions::CreateFilterStrikePointer()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterStrikePointer = pActionMapMan->CreateActionFilter("strikePointerDeployed", eAFT_ActionFail);

	m_pFilterStrikePointer->Filter(attack1);
	m_pFilterStrikePointer->Filter(attack1_xi);
}

void CGameActions::CreateFilterUseKeyOnly()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterUseKeyOnly = pActionMapMan->CreateActionFilter("useKeyOnly", eAFT_ActionPass);

	m_pFilterUseKeyOnly->Filter(use);
}

void CGameActions::CreateFilterInfictionMenu()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterInfictionMenu = pActionMapMan->CreateActionFilter("infiction_menu", eAFT_ActionPass);

	// Disabled all actions when in ingame menu, add any exceptions here

	// Menu actions
	m_pFilterInfictionMenu->Filter(menu_open_customizeweapon);
	m_pFilterInfictionMenu->Filter(menu_close_customizeweapon);
	
	m_pFilterInfictionMenu->Filter(menu_scrollup);
	m_pFilterInfictionMenu->Filter(menu_scrolldown);
	m_pFilterInfictionMenu->Filter(menu_fcommand1);
	m_pFilterInfictionMenu->Filter(menu_fcommand2);

	m_pFilterInfictionMenu->Filter(menu_switchtab_left);
	m_pFilterInfictionMenu->Filter(menu_switchtab_right);

	m_pFilterInfictionMenu->Filter(menu_toggle_barrel);
	m_pFilterInfictionMenu->Filter(menu_toggle_bottom);
	m_pFilterInfictionMenu->Filter(menu_toggle_scope);
	m_pFilterInfictionMenu->Filter(menu_toggle_ammo);

	m_pFilterInfictionMenu->Filter(menu_confirm);
	m_pFilterInfictionMenu->Filter(menu_confirm2);
	m_pFilterInfictionMenu->Filter(menu_back);
	m_pFilterInfictionMenu->Filter(menu_delete);
	m_pFilterInfictionMenu->Filter(menu_apply);
	m_pFilterInfictionMenu->Filter(menu_default);
	m_pFilterInfictionMenu->Filter(menu_back_select);

	m_pFilterInfictionMenu->Filter(menu_input_1);
	m_pFilterInfictionMenu->Filter(menu_input_2);

	m_pFilterInfictionMenu->Filter(menu_assetpause);
	m_pFilterInfictionMenu->Filter(menu_assetzoom);

	m_pFilterInfictionMenu->Filter(menu_toggle_index_finger);
	m_pFilterInfictionMenu->Filter(menu_toggle_middle_finger);
	m_pFilterInfictionMenu->Filter(menu_toggle_ring_finger);
	m_pFilterInfictionMenu->Filter(menu_toggle_ring_finger_switched);
	m_pFilterInfictionMenu->Filter(menu_toggle_pink);

	//movement
	m_pFilterInfictionMenu->Filter(xi_rotateyaw);
	m_pFilterInfictionMenu->Filter(xi_rotatepitch);
	m_pFilterInfictionMenu->Filter(moveleft);
	m_pFilterInfictionMenu->Filter(moveright);
	m_pFilterInfictionMenu->Filter(moveforward);
	m_pFilterInfictionMenu->Filter(moveback);
	m_pFilterInfictionMenu->Filter(xi_movex);
	m_pFilterInfictionMenu->Filter(xi_movey);

	m_pFilterInfictionMenu->Filter(mouse_wheel_infiction_close);
}

void CGameActions::CreateFilterMPWeaponCustomizationMenu()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterMPWeaponCustomizationMenu = pActionMapMan->CreateActionFilter("mp_weapon_customization_menu", eAFT_ActionPass);

	// Disabled all actions when in ingame menu, add any exceptions here

	// Menu actions
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_open_customizeweapon);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_close_customizeweapon);

	m_pFilterMPWeaponCustomizationMenu->Filter(menu_scrollup);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_scrolldown);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_fcommand1);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_fcommand2);

	m_pFilterMPWeaponCustomizationMenu->Filter(menu_switchtab_left);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_switchtab_right);

	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_barrel);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_bottom);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_scope);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_ammo);

	m_pFilterMPWeaponCustomizationMenu->Filter(menu_confirm);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_confirm2);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_back);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_delete);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_apply);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_default);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_back_select);

	m_pFilterMPWeaponCustomizationMenu->Filter(menu_input_1);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_input_2);

	m_pFilterMPWeaponCustomizationMenu->Filter(menu_assetpause);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_assetzoom);

	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_index_finger);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_middle_finger);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_ring_finger);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_ring_finger_switched);
	m_pFilterMPWeaponCustomizationMenu->Filter(menu_toggle_pink);

	//movement
	m_pFilterMPWeaponCustomizationMenu->Filter(xi_rotateyaw);
	m_pFilterMPWeaponCustomizationMenu->Filter(xi_rotatepitch);
	m_pFilterMPWeaponCustomizationMenu->Filter(moveleft);
	m_pFilterMPWeaponCustomizationMenu->Filter(moveright);
	m_pFilterMPWeaponCustomizationMenu->Filter(moveforward);
	m_pFilterMPWeaponCustomizationMenu->Filter(moveback);
	m_pFilterMPWeaponCustomizationMenu->Filter(xi_movex);
	m_pFilterMPWeaponCustomizationMenu->Filter(xi_movey);

	m_pFilterMPWeaponCustomizationMenu->Filter(mouse_wheel_infiction_close);

	m_pFilterMPWeaponCustomizationMenu->Filter(special);
	m_pFilterMPWeaponCustomizationMenu->Filter(attack1_xi);
}

void CGameActions::CreateFilterLedgeGrab()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterLedgeGrab = pActionMapMan->CreateActionFilter("ledge_grab", eAFT_ActionFail);
	m_pFilterLedgeGrab->Filter(attack1);
	m_pFilterLedgeGrab->Filter(v_attack1);
	m_pFilterLedgeGrab->Filter(v_attack2);
	m_pFilterLedgeGrab->Filter(rotateyaw);
	m_pFilterLedgeGrab->Filter(v_rotateyaw);
	m_pFilterLedgeGrab->Filter(rotatepitch);
	m_pFilterLedgeGrab->Filter(v_rotatepitch);
	m_pFilterLedgeGrab->Filter(nextitem);
	m_pFilterLedgeGrab->Filter(previtem);
	m_pFilterLedgeGrab->Filter(toggle_explosive);
	m_pFilterLedgeGrab->Filter(toggle_weapon);
	m_pFilterLedgeGrab->Filter(toggle_grenade);
	m_pFilterLedgeGrab->Filter(handgrenade);
	m_pFilterLedgeGrab->Filter(zoom);
	m_pFilterLedgeGrab->Filter(reload);
	m_pFilterLedgeGrab->Filter(use);
	m_pFilterLedgeGrab->Filter(xi_grenade);
	m_pFilterLedgeGrab->Filter(xi_handgrenade);
	m_pFilterLedgeGrab->Filter(xi_zoom);
	m_pFilterLedgeGrab->Filter(jump);
	m_pFilterLedgeGrab->Filter(binoculars);
	m_pFilterLedgeGrab->Filter(attack1_xi);
	m_pFilterLedgeGrab->Filter(attack2_xi);

	m_pFilterLedgeGrab->Filter(xi_rotateyaw);
	m_pFilterLedgeGrab->Filter(xi_rotatepitch);
	m_pFilterLedgeGrab->Filter(moveleft);
	m_pFilterLedgeGrab->Filter(moveright);
	m_pFilterLedgeGrab->Filter(moveforward);
	m_pFilterLedgeGrab->Filter(moveback);
	m_pFilterLedgeGrab->Filter(xi_movex);
	m_pFilterLedgeGrab->Filter(xi_movey);
}

void CGameActions::CreateFilterVault()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterVault = pActionMapMan->CreateActionFilter("vault", eAFT_ActionFail);
	m_pFilterVault->Filter(rotateyaw);
	m_pFilterVault->Filter(v_rotateyaw);
	m_pFilterVault->Filter(rotatepitch);
	m_pFilterVault->Filter(v_rotatepitch);
	m_pFilterVault->Filter(nextitem);
	m_pFilterVault->Filter(previtem);
	m_pFilterVault->Filter(toggle_explosive);
	m_pFilterVault->Filter(toggle_weapon);
	m_pFilterVault->Filter(toggle_grenade);
	m_pFilterVault->Filter(handgrenade);
	m_pFilterVault->Filter(zoom);
	m_pFilterVault->Filter(reload);
	m_pFilterVault->Filter(use);
	m_pFilterVault->Filter(xi_grenade);
	m_pFilterVault->Filter(xi_handgrenade);
	m_pFilterVault->Filter(xi_zoom);
	m_pFilterVault->Filter(jump);
	m_pFilterVault->Filter(binoculars);

	m_pFilterVault->Filter(xi_rotateyaw);
	m_pFilterVault->Filter(xi_rotatepitch);
}

void CGameActions::CreateItemPickup()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterItemPickup = pActionMapMan->CreateActionFilter("ItemPickup", eAFT_ActionFail);
	m_pFilterItemPickup->Filter(use);
	m_pFilterItemPickup->Filter(heavyweaponremove);
	m_pFilterItemPickup->Filter(itemPickup);
}

void CGameActions::CreateFilterButtonMashingSequence()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterButtonMashingSequence = pActionMapMan->CreateActionFilter("button_mashing_sequence", eAFT_ActionPass);
}

void CGameActions::CreateFilterUIOnly()
{
	IActionMapManager* pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();

	m_pFilterUIOnly = pActionMapMan->CreateActionFilter("only_ui", eAFT_ActionPass);	
	m_pFilterUIOnly->Filter(ui_toggle_pause);
	m_pFilterUIOnly->Filter(ui_up);
	m_pFilterUIOnly->Filter(ui_down);
	m_pFilterUIOnly->Filter(ui_left);
	m_pFilterUIOnly->Filter(ui_right);
	m_pFilterUIOnly->Filter(ui_click);
	m_pFilterUIOnly->Filter(ui_back);
	m_pFilterUIOnly->Filter(ui_confirm);
	m_pFilterUIOnly->Filter(ui_reset);
}
