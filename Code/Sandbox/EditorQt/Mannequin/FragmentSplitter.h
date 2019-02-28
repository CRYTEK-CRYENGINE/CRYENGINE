// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __fragmentsplitter_h__
#define __fragmentsplitter_h__

#include "..\Controls\ClampedSplitterWnd.h"

#if _MSC_VER > 1000
	#pragma once
#endif

// CFragmentSplitter

class CFragmentSplitter : public CClampedSplitterWnd
{
	DECLARE_DYNAMIC(CFragmentSplitter)

public:
	CFragmentSplitter() {};
	virtual ~CFragmentSplitter() {};

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnSetFocus(CWnd* pOldWnd);
};

#endif // __fragmentsplitter_h__

