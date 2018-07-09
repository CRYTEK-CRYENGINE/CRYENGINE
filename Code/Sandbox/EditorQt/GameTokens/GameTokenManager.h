// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GameTokenManager_h__
#define __GameTokenManager_h__
#pragma once

#include "BaseLibraryManager.h"

class CGameTokenItem;
class CGameTokenLibrary;

/** Manages Particle libraries and systems.
 */
class SANDBOX_API CGameTokenManager : public CBaseLibraryManager
{
public:
	CGameTokenManager();
	~CGameTokenManager();

	// Clear all prototypes and material libraries.
	void ClearAll();

	//! Save level game tokens
	void Save(bool bBackup);

	//! Load level game tokens
	bool Load();

	//! Export particle systems to game.
	void Export(XmlNodeRef& node);

	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

protected:
	virtual CBaseLibraryItem* MakeNewItem();
	virtual CBaseLibrary*     MakeNewLibrary();
	//! Root node where this library will be saved.
	virtual string           GetRootNodeName();
	//! Path to libraries in this manager.
	virtual string           GetLibsPath();

	string m_libsPath;
};

#endif // __GameTokenManager_h__

