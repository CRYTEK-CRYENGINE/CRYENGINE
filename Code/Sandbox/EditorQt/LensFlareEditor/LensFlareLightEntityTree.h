// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   LensFlareLightEntityTree.h
//  Created:     18/Dec/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

#include "ILensFlareListener.h"

class CLensFlareLightEntityTree : public CXTTreeCtrl, public ILensFlareChangeItemListener
{
public:
	CLensFlareLightEntityTree();
	~CLensFlareLightEntityTree();

	void OnLensFlareDeleteItem(CLensFlareItem* pLensFlareItem);
	void OnLensFlareChangeItem(CLensFlareItem* pLensFlareItem);
	void OnObjectEvent(const std::vector<CBaseObject*>& objects, const CObjectEvent& event);

protected:

	HTREEITEM FindItem(HTREEITEM hStartItem, const CBaseObject* pObject) const;
	void      AddLightEntity(const CEntityObject* pEntity);

	_smart_ptr<CLensFlareItem> m_pLensFlareItem;

	DECLARE_MESSAGE_MAP()

	void OnTvnItemDoubleClicked(NMHDR* pNMHDR, LRESULT* pResult);
};

