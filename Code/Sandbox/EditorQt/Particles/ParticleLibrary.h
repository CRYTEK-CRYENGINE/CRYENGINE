// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __particlelibrary_h__
#define __particlelibrary_h__
#pragma once

#include "BaseLibrary.h"

/** Library of prototypes.
 */
class SANDBOX_API CParticleLibrary : public CBaseLibrary
{
public:
	CParticleLibrary(CBaseLibraryManager* pManager) : CBaseLibrary(pManager) {};
	virtual bool Save();
	virtual bool Load(const string& filename);
	virtual void Serialize(XmlNodeRef& node, bool bLoading);
private:
};

#endif // __particlelibrary_h__

