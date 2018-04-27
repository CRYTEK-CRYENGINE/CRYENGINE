// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __particlemanager_h__
#define __particlemanager_h__
#pragma once

#include "BaseLibraryManager.h"

class CParticleItem;
class CParticleLibrary;
struct SExportParticleEffect;

/** Manages Particle libraries and systems.
 */
class SANDBOX_API CParticleManager : public CBaseLibraryManager
{
public:
	CParticleManager();
	~CParticleManager();

	// Clear all prototypes and material libraries.
	void         ClearAll();

	virtual void DeleteItem(IDataBaseItem* pItem);

	//////////////////////////////////////////////////////////////////////////
	// Materials.
	//////////////////////////////////////////////////////////////////////////
	//! Loads a new material from a xml node.
	void PasteToParticleItem(CParticleItem* pItem, XmlNodeRef& node, bool bWithChilds);

	//! Export particle systems to game.
	void Export(XmlNodeRef& node);

	//! Root node where this library will be saved.
	virtual string GetRootNodeName();

protected:
	void                      AddExportItem(std::vector<SExportParticleEffect>& effects, CParticleItem* pItem, int parent);
	virtual CBaseLibraryItem* MakeNewItem();
	virtual CBaseLibrary*     MakeNewLibrary();

	bool OnPickParticle(const string& oldValue, string& newValue);

	//! Path to libraries in this manager.
	virtual string GetLibsPath();

	string m_libsPath;
};

#endif // __particlemanager_h__

