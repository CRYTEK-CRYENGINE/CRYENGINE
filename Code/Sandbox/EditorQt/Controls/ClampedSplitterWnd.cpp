// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ClampedSplitterWnd.h"

IMPLEMENT_DYNAMIC(CClampedSplitterWnd, CXTSplitterWnd)

BEGIN_MESSAGE_MAP(CClampedSplitterWnd, CXTSplitterWnd)
ON_WM_SIZE()
END_MESSAGE_MAP()

void CClampedSplitterWnd::TrackRowSize(int y, int row)
{
	// Overriding base behaviour from CSplitterWnd
	// where if an entry is too small it disappears
	// we want it to instead not get that small

	ClampMovement(y, row, m_nRows, m_pRowInfo);
}

void CClampedSplitterWnd::TrackColumnSize(int x, int col)
{
	// Overriding base behaviour from CSplitterWnd
	// where if an entry is too small it disappears
	// we want it to instead not get that small

	ClampMovement(x, col, m_nCols, m_pColInfo);
}

void CClampedSplitterWnd::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	RequestShowHiddenPanel(m_nCols, m_pColInfo);
	RequestShowHiddenPanel(m_nRows, m_pRowInfo);
}

void CClampedSplitterWnd::ClampMovement(const int offset, const int index, const int numEntries, CRowColInfo* dataSet)
{
	// Asserts to mimic behaviour from CSplitterWnd
	ASSERT_VALID(this);
	ASSERT(numEntries > 1);

	// Determine change in splitter position
	static const int SPLITTER_WIDTH = 6;
	static const int HALF_SPLITTER_WIDTH = 3;
	int prevPos = -HALF_SPLITTER_WIDTH; // First panel only has one half a splitter in it, so negate half
	for (int i = 0; i <= index; i++)
	{
		prevPos += dataSet[i].nCurSize + SPLITTER_WIDTH; // Add the splitter to each
	}
	prevPos -= HALF_SPLITTER_WIDTH; // Last panel only has only half a splitter in it, so negate half
	int splitterPosChange = offset - prevPos;

	if (splitterPosChange > 0)
	{
		// Splitter moving right/down
		int indexID = numEntries - 2;
		int requestedShrink = splitterPosChange;
		int accumulatedMovement = 0;

		while (indexID >= index && requestedShrink > 0)
		{
			RequestPanelShrink(indexID + 1, requestedShrink, accumulatedMovement, dataSet);
			indexID--;
		}

		// Add the accumulated movement to the panel to the left of or above our selected splitter
		dataSet[index].nIdealSize = dataSet[index].nCurSize + accumulatedMovement;
	}
	else if (splitterPosChange < 0)
	{
		// Splitter moving left/up
		int indexID = 0;
		int requestedShrink = splitterPosChange * -1;
		int accumulatedMovement = 0;

		while (indexID <= index && requestedShrink > 0)
		{
			RequestPanelShrink(indexID, requestedShrink, accumulatedMovement, dataSet);
			indexID++;
		}

		// Add the accumulated movement to the panel to the right of or below our selected splitter
		dataSet[index + 1].nIdealSize = dataSet[index + 1].nCurSize + accumulatedMovement;
	}
}

// Attempts to shrink a panel, will not shrink smaller than minimum size
// requestedShrink: The amount we want to reduce from the panel
// accumulatedMovement: We add the amount we reduced from this panel to accumulatedMovement
void CClampedSplitterWnd::RequestPanelShrink(const int index, int& requestedShrink, int& accumulatedMovement, CRowColInfo* dataSet)
{
	// Determine how much we can shrink by, but no more than requested
	int availableChange = MIN(dataSet[index].nCurSize - dataSet[index].nMinSize, requestedShrink);

	// Deduct from amount we are going to shrink, and add to accumulated movement
	requestedShrink -= availableChange;
	accumulatedMovement += availableChange;

	// Apply the change to this panel
	dataSet[index].nIdealSize = dataSet[index].nCurSize - availableChange;
}

// Attempts to resize all panels that wouldn't be visible anymore after the splitter has been resized
// maxData: The maximum number of rows or cols of dataSet
// dataSet: The dataset(rows / cols)
void CClampedSplitterWnd::RequestShowHiddenPanel(const int maxData, CRowColInfo* dataSet)
{
	// The amount of pixel when a resizing should occure
	static const int moveSize = 25;

	if (maxData <= 1 || !dataSet)
	{
		return;
	}

	for (int i = 0; i < maxData; ++i)
	{
		if (dataSet[i].nCurSize < moveSize)
		{
			if (i == 0) // Pick right neighbour (panel)
			{
				if (dataSet[i + 1].nCurSize > moveSize)
				{
					ResizePanels(moveSize, dataSet[i + 1], dataSet[i]);
				}
			}
			else // Pick left neighbour (panel)
			{
				if (dataSet[i - 1].nCurSize > moveSize)
				{
					ResizePanels(moveSize, dataSet[i - 1], dataSet[i]);
				}
			}
		}
	}
}

// Resizes two panels, makes one panel smaller and makes the other bigger by the same amount
// resizeAmount: The amount by which the two panels should be sized
// makeSmaller: The panel to become smaller
// makeBigger: The panel to become bigger
void CClampedSplitterWnd::ResizePanels(const int resizeAmount, CRowColInfo& makeSmaller, CRowColInfo& makeBigger)
{
	makeSmaller.nCurSize -= resizeAmount;
	makeSmaller.nIdealSize = makeSmaller.nCurSize;

	makeBigger.nCurSize += resizeAmount;
	makeBigger.nIdealSize = makeBigger.nCurSize;
}

