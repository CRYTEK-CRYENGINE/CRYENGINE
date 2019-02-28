// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __MANNEQUININTERFACE_H__
#define __MANNEQUININTERFACE_H__

#include "ICryMannequin.h"

class CProceduralClipFactory;

class CMannequinInterface : public IMannequin
{
public:
	CMannequinInterface();
	~CMannequinInterface();

	// IMannequin
	virtual void                         UnloadAll();
	virtual void                         ReloadAll();

	virtual IAnimationDatabaseManager&   GetAnimationDatabaseManager();
	virtual IActionController*           CreateActionController(IEntity* pEntity, SAnimationContext& context);
	virtual IActionController*           FindActionController(const IEntity& entity);
	virtual IMannequinEditorManager*     GetMannequinEditorManager();
	virtual CMannequinUserParamsManager& GetMannequinUserParamsManager();
	virtual IProceduralClipFactory&      GetProceduralClipFactory();

	virtual void                         AddMannequinGameListener(IMannequinGameListener* pListener);
	virtual void                         RemoveMannequinGameListener(IMannequinGameListener* pListener);
	virtual uint32                       GetNumMannequinGameListeners();
	virtual IMannequinGameListener*      GetMannequinGameListener(uint32 idx);
	virtual void                         SetSilentPlaybackMode(bool bSilentPlaybackMode);
	virtual bool                         IsSilentPlaybackMode() const;
	// ~IMannequin

private:
	void RegisterCVars();

private:
	class CAnimationDatabaseManager*        m_pAnimationDatabaseManager;
	std::vector<IMannequinGameListener*>    m_mannequinGameListeners;
	CMannequinUserParamsManager             m_userParamsManager;
	std::unique_ptr<CProceduralClipFactory> m_pProceduralClipFactory;
	bool m_bSilentPlaybackMode;
};

#endif //!__MANNEQUININTERFACE_H__
