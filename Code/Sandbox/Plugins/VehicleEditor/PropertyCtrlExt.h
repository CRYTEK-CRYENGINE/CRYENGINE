// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __propertyctrlext_h__
#define __propertyctrlext_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Controls/PropertyCtrl.h"

/**
 */
class CPropertyCtrlExt : public CPropertyCtrl
{
	DECLARE_DYNAMIC(CPropertyCtrlExt)

public:
	typedef Functor1<CPropertyItem*> PreSelChangeCallback;

	CPropertyCtrlExt();

	void SetPreSelChangeCallback(PreSelChangeCallback& callback) { m_preSelChangeFunc = callback; }
	void SelectItem(CPropertyItem* item);
	void OnItemChange(CPropertyItem* item);

	void SetVehicleVar(IVariable* pVar) { m_pVehicleVar = pVar; }

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);

	void         OnAddChild();
	void         OnDeleteChild(CPropertyItem* pItem);
	void         OnGetEffect(CPropertyItem* pItem);

	void         ReloadItem(CPropertyItem* pItem);

	PreSelChangeCallback m_preSelChangeFunc;

	IVariable*           m_pVehicleVar;
};

#endif // __propertyctrlext_h__

