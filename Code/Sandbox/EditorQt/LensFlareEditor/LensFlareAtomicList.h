// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011.
// -------------------------------------------------------------------------
//  File name:   LensFlareAtomicList.h
//  Created:     7/Dec/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

#include "Controls/ImageListCtrl.h"

class CLensFlareEditor;

class CLensFlareAtomicList : public CImageListCtrl
{
public:

	struct CFlareImageItem : public CImageListCtrlItem
	{
		EFlareType flareType;
	};

	CLensFlareAtomicList();
	virtual ~CLensFlareAtomicList();

	void FillAtomicItems();

protected:

	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

	bool OnBeginDrag(UINT nFlags, CPoint point);
	void OnDropItem(CPoint point);
	void OnDraggingItem(CPoint point);
	bool InsertItem(const FlareInfo& flareInfo);

};

