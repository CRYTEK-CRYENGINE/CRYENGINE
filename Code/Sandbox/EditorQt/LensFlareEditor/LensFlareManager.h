// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   LensFlareManager.h
//  Created:     7/Dec/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

#include "BaseLibraryManager.h"

class IOpticsElementBase;
class CLensFlareEditor;

class SANDBOX_API CLensFlareManager : public CBaseLibraryManager
{
public:
	CLensFlareManager();
	virtual ~CLensFlareManager();

	void              ClearAll();

	virtual bool      LoadFlareItemByName(const string& fullItemName, IOpticsElementBasePtr pDestOptics);
	void              Modified();
	//! Path to libraries in this manager.
	string           GetLibsPath();
	IDataBaseLibrary* LoadLibrary(const string& filename, bool bReload = false);

private:
	CBaseLibraryItem* MakeNewItem();
	CBaseLibrary*     MakeNewLibrary();

	bool OnPickFlare(const string& oldValue, string& newValue);

	//! Root node where this library will be saved.
	string GetRootNodeName();
	string m_libsPath;
};

