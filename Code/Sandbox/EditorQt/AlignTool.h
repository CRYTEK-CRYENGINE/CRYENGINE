// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __aligntool_h__
#define __aligntool_h__

#if _MSC_VER > 1000
	#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
class CAlignPickCallback : public IPickObjectCallback
{
public:
	CAlignPickCallback() { m_bActive = true; };
	//! Called when object picked.
	virtual void OnPick(CBaseObject* picked);
	//! Called when pick mode cancelled.
	virtual void OnCancelPick();
	//! Return true if specified object is pickable.
	virtual bool OnPickFilter(CBaseObject* filterObject);

	static bool  IsActive()                           { return m_bActive; }

private:
	static bool m_bActive;
};

#endif // __aligntool_h__

