// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   LensFlareItem.h
//  Created:     12/Dec/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////
#include "BaseLibraryItem.h"

class IOpticsElementBase;

class CLensFlareItem : public CBaseLibraryItem
{
public:

	CLensFlareItem();
	~CLensFlareItem();

	EDataBaseItemType GetType() const
	{
		return EDB_TYPE_FLARE;
	}

	void                  SetName(const string& name);
	void                  SetName(const string& name, bool bRefreshWhenUndo, bool bRefreshWhenRedo);
	void                  Serialize(SerializeContext& ctx);

	void                  CreateOptics();

	IOpticsElementBasePtr GetOptics() const
	{
		return m_pOptics;
	}

	void       ReplaceOptics(IOpticsElementBasePtr pNewData);

	XmlNodeRef CreateXmlData() const;
	void       UpdateLights(IOpticsElementBasePtr pSrcOptics = NULL);

private:

	void        GetLightEntityObjects(std::vector<CEntityObject*>& outEntityLights) const;
	static void AddChildOptics(IOpticsElementBasePtr pParentOptics, XmlNodeRef& pNode);

	IOpticsElementBasePtr m_pOptics;
};

