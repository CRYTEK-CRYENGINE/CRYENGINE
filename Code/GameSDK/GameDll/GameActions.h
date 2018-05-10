// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IActionMapManager.h>

#define DECL_ACTION(name) ActionId name;
class CGameActions : public IActionMapEventListener
{
public:
	CGameActions();
	~CGameActions();

	// IActionMapEventListener
	virtual void OnActionMapEvent(const SActionMapEvent& event);
	// ~IActionMapEventListener

#include "GameActions.actions"

	void InitActions();
	ILINE IActionFilter*	FilterNoMove() const {	return m_pFilterNoMove;	}
	ILINE IActionFilter*	FilterNoMouse() const {	return m_pFilterNoMouse;	}
	ILINE IActionFilter*	FilterMouseX() const { return m_pFilterMouseX; }
	ILINE IActionFilter*	FilterMouseY() const { return m_pFilterMouseY; }
	ILINE IActionFilter*	FilterTutorialNoMove() const {	return m_pFilterTutorialNoMove;	}
	ILINE IActionFilter*	FilterNoWeaponCustomization() const {	return m_pFilterNoWeaponCustomization;	}
	ILINE IActionFilter*	FilterNoFireModeSwitch() const {	return m_pFilterNoFireModeSwitch;	}
	ILINE IActionFilter*	FilterWarningPopup() const {	return m_pFilterWarningPopup;	}
	ILINE IActionFilter*	FilterMindControlMenu() const {	return m_pFilterMindControlMenu;	}
	ILINE IActionFilter*	FilterFreezeTime() const {	return m_pFilterFreezeTime;	}
	ILINE IActionFilter*	FilterHostMigration() const {	return m_pFilterHostMigration;	}
	ILINE IActionFilter*	FilterMPPreGameFreeze() const {	return m_pFilterMPPreGameFreeze;	}
	ILINE IActionFilter*	FilterNoVehicleExit() const {	return m_pFilterNoVehicleExit;	}
	ILINE IActionFilter*	FilterMPRadio() const {	return m_pFilterMPRadio;	}
	ILINE IActionFilter*	FilterMPChat() const {	return m_pFilterMPChat;	}
	ILINE IActionFilter*	FilterCutscene() const {	return m_pFilterCutscene;	}
	ILINE IActionFilter*	FilterCutsceneNoPlayer() const {	return m_pFilterCutsceneNoPlayer;	}
	ILINE IActionFilter*  FilterCutsceneTrain() const { return m_pFilterCutsceneTrain; }
	ILINE IActionFilter*	FilterCutscenePlayerMoving() const {	return m_pFilterCutscenePlayerMoving;	}
	ILINE IActionFilter*	FilterVehicleNoSeatChangeAndExit() const {	return m_pFilterVehicleNoSeatChangeAndExit;	}
	ILINE IActionFilter*	FilterNoConnectivity() const {	return m_pFilterNoConnectivity;	}
	ILINE IActionFilter*	FilterIngameMenu() const { return m_pFilterIngameMenu; }
	ILINE IActionFilter*	FilterScoreboard() const { return m_pFilterScoreboard; }
	ILINE IActionFilter*	FilterStrikePointer() const { return m_pFilterStrikePointer; }
	ILINE IActionFilter*	FilterUseKeyOnly() const { return m_pFilterUseKeyOnly; }
	ILINE IActionFilter*  FilterInfictionMenu()const {return m_pFilterInfictionMenu;}
	ILINE IActionFilter*  FilterMPWeaponCustomizationMenu()const {return m_pFilterMPWeaponCustomizationMenu;}
	ILINE IActionFilter*  FilterLedgeGrab()const {return m_pFilterLedgeGrab;}
	ILINE IActionFilter*  FilterVault()const {return m_pFilterVault;}
	ILINE IActionFilter*  FilterItemPickup()const {return m_pFilterItemPickup;}
	ILINE IActionFilter*  FilterNotYetSpawned()const {return m_pFilterNotYetSpawned;}
	ILINE IActionFilter*  FilterButtonMashingSequence() const { return m_pFilterButtonMashingSequence; }

private:
	void	CreateFilterNoMove();
	void	CreateFilterNoMouse();
	void	CreateFilterMouseX();
	void	CreateFilterMouseY();
	void	CreateTutorialNoMove();
	void	CreateFilterNoWeaponCustomization();
	void	CreateFilterNoFireModeSwitch();
	void	CreateFilterWarningPopup();
	void	CommonCreateFilterFreeze(IActionFilter *pFilter);
	void	CreateFilterFreezeTime();
	void	CreateFilterHostMigration();
	void	CreateFilterMPPreGameFreeze();
	void	CreateFilterNoVehicleExit();
	void	CreateFilterMPRadio();
	void	CreateFilterMPChat();
	void	CreateFilterCutscene();
	void	CreateFilterCutsceneNoPlayer();
	void  CreateFilterCutsceneTrain();
	void	CreateFilterCutscenePlayerMoving();
	void	CreateFilterVehicleNoSeatChangeAndExit();
	void	CreateFilterNoConnectivity();
	void CreateFilterIngameMenu();
	void CreateFilterScoreboard();
	void CreateFilterStrikePointer();
	void CreateFilterUseKeyOnly();
	void CreateFilterInfictionMenu();
	void CreateFilterMPWeaponCustomizationMenu();
	void CreateFilterLedgeGrab();
	void CreateFilterVault();
	void CreateItemPickup();
	void CreateFilterNotYetSpawned();
	void CreateFilterButtonMashingSequence();
	void CreateFilterUIOnly();

	IActionFilter*	m_pFilterNoMove;
	IActionFilter*	m_pFilterNoMouse;
	IActionFilter*	m_pFilterMouseX;
	IActionFilter*	m_pFilterMouseY;
	IActionFilter*	m_pFilterTutorialNoMove;
	IActionFilter*	m_pFilterNoWeaponCustomization;
	IActionFilter*	m_pFilterNoFireModeSwitch;
	IActionFilter*	m_pFilterWarningPopup;
	IActionFilter*	m_pFilterMindControlMenu;
	IActionFilter*	m_pFilterFreezeTime;
	IActionFilter*	m_pFilterHostMigration;
	IActionFilter*	m_pFilterMPPreGameFreeze;
	IActionFilter*	m_pFilterNoVehicleExit;
	IActionFilter*	m_pFilterMPRadio;
	IActionFilter*	m_pFilterMPChat;
	IActionFilter*	m_pFilterCutscene;
	IActionFilter*	m_pFilterCutsceneNoPlayer;
	IActionFilter*  m_pFilterCutsceneTrain;
	IActionFilter*	m_pFilterCutscenePlayerMoving;
	IActionFilter*	m_pFilterVehicleNoSeatChangeAndExit;
	IActionFilter*	m_pFilterNoConnectivity;
	IActionFilter*  m_pFilterIngameMenu;
	IActionFilter*  m_pFilterScoreboard;
	IActionFilter*	m_pFilterStrikePointer;
	IActionFilter*	m_pFilterUseKeyOnly;
	IActionFilter*  m_pFilterInfictionMenu;
	IActionFilter*  m_pFilterMPWeaponCustomizationMenu;
	IActionFilter*  m_pFilterLedgeGrab;
	IActionFilter*  m_pFilterVault;
	IActionFilter*  m_pFilterItemPickup;
	IActionFilter*  m_pFilterNotYetSpawned; 
	IActionFilter*  m_pFilterButtonMashingSequence;
	IActionFilter*	m_pFilterUIOnly;
};
#undef DECL_ACTION

extern CGameActions* g_pGameActions;