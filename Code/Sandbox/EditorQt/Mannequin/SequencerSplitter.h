// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __sequencersplitter_h__
#define __sequencersplitter_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "..\Controls\ClampedSplitterWnd.h"

// CSequencerSplitter

class CSequencerSplitter : public CClampedSplitterWnd
{
	DECLARE_DYNAMIC(CSequencerSplitter)

	virtual CWnd * GetActivePane(int* pRow = NULL, int* pCol = NULL)
	{
		return GetFocus();
	}

	void SetPane(int row, int col, CWnd* pWnd, SIZE sizeInit);
	// Ovveride this for flat look.
	void OnDrawSplitter(CDC* pDC, ESplitType nType, const CRect& rectArg);

public:
	CSequencerSplitter();
	virtual ~CSequencerSplitter();

protected:
	DECLARE_MESSAGE_MAP()
};

#endif // __sequencersplitter_h__

