// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SequencerDopeSheet.h"
#include "Controls\MemDC.h"
#include "MannPreferences.h"
#include "SequencerUndo.h"

#include "FragmentTrack.h"

#define KEY_TEXT_COLOR      RGB(0, 0, 50)
#define INACTIVE_TEXT_COLOR RGB(128, 128, 128)

// CSequencerKeyList

IMPLEMENT_DYNAMIC(CSequencerDopeSheet, CSequencerDopeSheetBase)
CSequencerDopeSheet::CSequencerDopeSheet()
{
	m_leftOffset = 30;

	m_itemWidth = 1000;
}

CSequencerDopeSheet::~CSequencerDopeSheet()
{
}

BEGIN_MESSAGE_MAP(CSequencerDopeSheet, CSequencerDopeSheetBase)
END_MESSAGE_MAP()

// CSequencerKeyList message handlers

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheet::DrawTrack(int item, CDC* dc, CRect& rcItem)
{
	CPen pen(PS_SOLID, 1, RGB(120, 120, 120));
	CPen* prevPen = dc->SelectObject(&pen);
	dc->MoveTo(rcItem.left, rcItem.bottom);
	dc->LineTo(rcItem.right, rcItem.bottom);
	dc->SelectObject(prevPen);

	CSequencerTrack* track = GetTrack(item);
	if (!track)
	{
		CSequencerNode* pAnimNode = GetNode(item);
		if (pAnimNode && GetItem(item).paramId == 0)
		{
			// Draw Anim node track.
			DrawNodeItem(pAnimNode, dc, rcItem);
		}
		return;
	}

	CRect rcInner = rcItem;
	rcInner.left = max(rcItem.left, m_leftOffset - m_scrollOffset.x);
	rcInner.right = min(rcItem.right, (m_scrollMax + m_scrollMin) - m_scrollOffset.x + m_leftOffset * 2);

	CRect rcInnerDraw(rcInner.left - 6, rcInner.top, rcInner.right + 6, rcInner.bottom);
	ColorB trackColor = track->GetColor();

	CRect rc = rcInnerDraw;
	rc.DeflateRect(0, 1, 0, 0);

	if (IsSelectedItem(item))
	{
		XTPPaintManager()->GradientFill(dc, rc, RGB(trackColor.r, trackColor.g, trackColor.b),
		                                RGB(trackColor.r / 2, trackColor.g / 2, trackColor.b / 2), FALSE);
	}
	else
	{
		dc->FillSolidRect(rc.left, rc.top, rc.Width(), rc.Height(),
		                  RGB(trackColor.r, trackColor.g, trackColor.b));
	}

	// Left outside
	CRect rcOutside = rcItem;
	rcOutside.right = rcInnerDraw.left - 1;
	rcOutside.DeflateRect(1, 1, 1, 0);
	dc->SelectObject(m_bkgrBrushEmpty);

	XTPPaintManager()->GradientFill(dc, rcOutside, RGB(210, 210, 210), RGB(180, 180, 180), FALSE);

	// Right outside.
	rcOutside = rcItem;
	rcOutside.left = rcInnerDraw.right + 1;
	rcOutside.DeflateRect(1, 1, 1, 0);

	XTPPaintManager()->GradientFill(dc, rcOutside, RGB(210, 210, 210), RGB(180, 180, 180), FALSE);

	// Get time range of update rectangle.
	Range timeRange = GetTimeRange(rcItem);

	// Draw keys in time range.
	DrawKeys(track, dc, rcInner, timeRange, DRAW_BACKGROUND);
	DrawKeys(track, dc, rcInner, timeRange, (EDSRenderFlags)(DRAW_HANDLES | DRAW_NAMES));

	// Draw tick marks in time range.
	DrawTicks(dc, rcInner, timeRange);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheet::DrawKeys(CSequencerTrack* track, CDC* dc, CRect& rc, Range& timeRange, EDSRenderFlags renderFlags)
{
	enum EBlendType
	{
		B_Entry,
		B_Exit
	};
	char keydesc[1024];
	int prevKeyPixel = -10000;

	const ESequencerParamType paramType = track->GetParameterType();

	CFont* prevFont = dc->SelectObject(m_descriptionFont);

	dc->SetTextColor(KEY_TEXT_COLOR);
	dc->SetBkMode(TRANSPARENT);

	// Draw keys.
	const int numKeys = track->GetNumKeys();
	for (int i = 0; i < numKeys; i++)
	{
		const float keyTime = track->GetKeyTime(i);
		const int xKeyTime = TimeToClient(keyTime);
		if (xKeyTime - 10 > rc.right)
			continue;
		const float nextKeyTime = i + 1 < numKeys ? track->GetKeyTime(i + 1) : FLT_MAX;

		int xClipStart = xKeyTime;
		int xClipCurrent = xKeyTime;

		const SKeyColour& keyCol = track->GetKeyColour(i);
		const COLORREF colourBase = RGB(keyCol.base[0], keyCol.base[1], keyCol.base[2]);

		// Get info about this key.
		const char* description = NULL;
		float tempDuration = 0;
		track->GetKeyInfo(i, description, tempDuration);
		const float duration = tempDuration;  // so that I can avoid using a non-const version, helpful for learning the code
		const bool hasDuration = (duration > 0.0f);
		const float keyTimeEnd = MIN(keyTime + duration, nextKeyTime);
		const int xKeyTimeEnd = TimeToClient(keyTimeEnd);

		bool showBlend = false;
		bool isLooping = false;
		float startTime = 0.0;
		EBlendType blendType = B_Entry;
		bool limitTextToDuration = false;
		bool hasValidAnim = true;
		if (paramType == SEQUENCER_PARAM_ANIMLAYER)
		{
			CClipKey clipKey;
			track->GetKey(i, &clipKey);

			startTime = clipKey.startTime;
			isLooping = (clipKey.animFlags & CA_LOOP_ANIMATION);
			if (isLooping)
			{
				// re-use tempDuration
				tempDuration = clipKey.GetDuration();
			}

			if (clipKey.blendDuration > 0.0f)
			{
				const float endt = min(keyTime + clipKey.blendDuration, m_timeRange.end);
				xClipCurrent = TimeToClient(endt);
				showBlend = true;
				if (clipKey.animRef.IsEmpty())
					blendType = B_Exit;
			}
			hasValidAnim = clipKey.HasValidFile();
		}
		else if (paramType == SEQUENCER_PARAM_PROCLAYER)
		{
			CProcClipKey procKey;
			track->GetKey(i, &procKey);

			if (procKey.blendDuration > 0.0f)
			{
				const float endt = min(keyTime + procKey.blendDuration, m_timeRange.end);
				xClipCurrent = TimeToClient(endt);
				showBlend = true;
				if (procKey.typeNameHash.IsEmpty())
					blendType = B_Exit;
			}
		}
		else if (paramType == SEQUENCER_PARAM_FRAGMENTID)
		{
			CFragmentKey fragKey;
			track->GetKey(i, &fragKey);

			const float keyStartTime = (fragKey.transition) ? (max(keyTime, fragKey.tranStartTime) + fragKey.transitionTime) : keyTime;
			xClipCurrent = xClipStart = TimeToClient(keyStartTime);
			limitTextToDuration = fragKey.transition;
		}
		else if (paramType == SEQUENCER_PARAM_TRANSITIONPROPS)
		{
			CTransitionPropertyKey propsKey;
			track->GetKey(i, &propsKey);

			limitTextToDuration = (propsKey.prop == CTransitionPropertyKey::eTP_Transition);
			showBlend = false;

			//--- Draw additional indicator markers for select times
			if (propsKey.prop == CTransitionPropertyKey::eTP_Select)
			{
				const CTransitionPropertyTrack* pPropTrack = (CTransitionPropertyTrack*)track;

				float cycleTime = 0.0f;
				if (propsKey.tranFlags & SFragmentBlend::Cyclic)
				{
					float cycle = (propsKey.m_time - propsKey.prevClipStart) / max(propsKey.prevClipDuration, 0.1f);
					cycle = (float)(int)cycle;                                                // 0-based index of the cycle the propsKey is set in
					cycleTime = (cycle * propsKey.prevClipDuration) + propsKey.prevClipStart; // time of the start of the cycle this propsKey is set in
				}

				const float selectStart = TimeToClient(propsKey.m_time);
				float selectEnd = -1;

				const uint32 numBlends = pPropTrack->GetNumBlendsForKey(i);
				for (uint32 b = 0; b < numBlends; b++)
				{
					const SFragmentBlend* pFragmentBlend = pPropTrack->GetAlternateBlendForKey(i, b);
					if (pFragmentBlend)
					{
						const float blendSelectTime = (pFragmentBlend->flags & SFragmentBlend::Cyclic) ?
						                              cycleTime + (pFragmentBlend->selectTime * propsKey.prevClipDuration) :
						                              propsKey.prevClipStart + pFragmentBlend->selectTime;

						const COLORREF colourSelect = RGB(255, 0, 128);
						const int xBlend = TimeToClient(blendSelectTime);
						const CPoint p1(xBlend - 5, rc.bottom - 5);
						const CPoint p2(xBlend + 5, rc.bottom - 5);
						const CPoint p3(xBlend, rc.bottom + 2);
						XTPPaintManager()->Triangle(dc, p1, p2, p3, colourSelect);

						if (b == propsKey.blend.blendIdx + 1)
						{
							// the end of the current select region is when the next blend starts (assuming sorted blends)
							selectEnd = TimeToClient(blendSelectTime);
						}
					}
				}

				// Draw a bar representing the range in which this transition will be used
				if (selectEnd < 0.f)
				{
					// we didn't find a next blend in the list
					selectEnd = TimeToClient(propsKey.prevClipStart + propsKey.prevClipDuration);
				}

				const COLORREF colourSelect = RGB(255, 0, 128);
				XTPPaintManager()->Line(dc, selectStart, rc.bottom - 10, selectEnd, rc.bottom - 10, colourSelect);
				XTPPaintManager()->Line(dc, selectStart, rc.bottom - 5, selectStart, rc.bottom - 10, colourSelect);
				XTPPaintManager()->Line(dc, selectEnd, rc.bottom - 5, selectEnd, rc.bottom - 10, colourSelect);
			}
		}

		// this value might differ from "duration" if "isLooping" is true
		const float nonLoopedDuration = tempDuration;

		if (renderFlags & DRAW_BACKGROUND)
		{
			// draw the key body based on it's type and it's place in the sequence along the track
			if (hasDuration)
			{
				const float endt = min(keyTimeEnd, m_timeRange.end);
				int x1 = TimeToClient(endt);
				if (x1 < 0)
				{
					if (xClipCurrent > 0)
						x1 = rc.right;
				}

				// this selects from the palette of images available
				// limited to the number of them which is sadly hardcoded in SequencerDopeSheetBase.cpp, line ~202
				int imgType = limitTextToDuration ? SEQBMP_TRANSITION : SEQBMP_ERROR;
				if (!limitTextToDuration)
				{
					switch (paramType)
					{
					case SEQUENCER_PARAM_TAGS:
						imgType = SEQBMP_TAGS;
						break;
					case SEQUENCER_PARAM_PARAMS:
						imgType = SEQBMP_PARAMS;
						break;
					case SEQUENCER_PARAM_ANIMLAYER:
						if (hasValidAnim)
							imgType = SEQBMP_ANIMLAYER;
						else
							imgType = SEQBMP_ERROR;
						break;
					case SEQUENCER_PARAM_PROCLAYER:
						imgType = SEQBMP_PROCLAYER;
						break;
					case SEQUENCER_PARAM_FRAGMENTID:
						imgType = SEQBMP_FRAGMENTID;
						break;
					case SEQUENCER_PARAM_TRANSITIONPROPS:
						imgType = SEQBMP_PARAMS;
						break;
					}
					;
				}

				// images are ordered (sel,unsel),(sel,unsel),(sel,unsel),(sel,unsel)
				// so need to shift the sequence then determine offset or not.
				const int imageSeq = (i % 4) << 1;
				const int imageOffset = track->IsKeySelected(i) ? 0 : 1;

				const int start = clamp_tpl(xKeyTime, int(rc.left), int(rc.right));
				const int width = min(x1 - start, rc.Width());
				m_imageSeqKeyBody[imgType].DrawEx(dc, (imageSeq + imageOffset), CPoint(start, rc.top + 2), CSize(width, rc.Height() - 2), CLR_DEFAULT, CLR_DEFAULT, ILD_SCALE);

				dc->MoveTo(x1, rc.top);
				dc->LineTo(x1, rc.bottom);
			}
			dc->MoveTo(xKeyTime, rc.top);
			dc->LineTo(xKeyTime, rc.bottom);
		}

		if (renderFlags & DRAW_HANDLES)
		{
			// -- draw "frame" == 4 triangles at each corner + lines connecting them.
			const int numSecondarySelPts = track->GetNumSecondarySelPts(i);
			if (hasDuration && B_Exit != blendType) // drawFrame
			{
				// find where the next key begins
				const int nexti = (i + 1);
				const bool hasNextKey = (nexti < track->GetNumKeys());
				const float nextKeyTime = hasNextKey ? track->GetKeyTime(nexti) : keyTimeEnd;
				const float blendOutStartTime = hasNextKey ? nextKeyTime : keyTimeEnd;

				float timeNext2ndKey = keyTimeEnd;
				if (hasNextKey && track->GetNumSecondarySelPts(nexti) > 0)
				{
					timeNext2ndKey = max(track->GetSecondaryTime(nexti, CClipTrack::eCKSS_BlendIn), nextKeyTime);
				}
				const float blendOutEndTime = timeNext2ndKey;
				const int xBlendOutStart = TimeToClient(blendOutStartTime);
				const int xBlendOutEnd = TimeToClient(blendOutEndTime);

				// when & where is our blend marker
				float time2ndKey = keyTime;
				if (numSecondarySelPts > 0)
				{
					const int id2ndPt = track->GetSecondarySelectionPt(i, keyTime, keyTimeEnd);
					time2ndKey = id2ndPt ? track->GetSecondaryTime(i, id2ndPt) : keyTime;
				}
				const float blendInEndTime = max(time2ndKey, keyTime);
				const int xBlendInEnd = TimeToClient(blendInEndTime);

				CRect lrc = rc;
				lrc.DeflateRect(1, 1, 1, 1);

				if (isLooping)
				{
					const int x1stLoopEnd = TimeToClient(keyTime + nonLoopedDuration);
					const int xNonLoopEnd = TimeToClient(keyTime + nonLoopedDuration + startTime);
					int endXPos = x1stLoopEnd;
					const int xLoopStep = max(xNonLoopEnd - xKeyTime, 2);
					const int yPos = max(0L, rc.top + rc.bottom - 10L) / 2; // -10 to avoid overlap with the text in the middle of the clip
					CPen pen(PS_DOT, 1, RGB(100, 100, 100));
					dc->SelectObject(&pen);
					while (endXPos < xKeyTimeEnd)
					{
						dc->MoveTo(endXPos, rc.top);
						dc->LineTo(endXPos, yPos);
						// next position
						endXPos += xLoopStep;
					}
				}

				const bool canEditKey = track->CanEditKey(i);
				const COLORREF triColour = (track->IsKeySelected(i)) ? RGB(0, 255, 0) : (canEditKey ? RGB(50, 50, 100) : RGB(100, 100, 100));
				CPen pen(PS_SOLID, 1, triColour);
				dc->SelectObject(&pen);

				// draw | vertical start line
				dc->MoveTo(xBlendInEnd, lrc.top);
				dc->LineTo(xKeyTime, lrc.bottom);

				// draw -- horizontal top line
				dc->MoveTo(xBlendInEnd, lrc.top);
				dc->LineTo(xBlendOutStart, lrc.top);

				// draw | vertical end line
				dc->MoveTo(xBlendOutStart, lrc.top);
				dc->LineTo(xBlendOutEnd, lrc.bottom);
			}

			// -- draw the vertical blend time seperator lines
			for (int s = 0; s < numSecondarySelPts; s++)
			{
				const float secondaryTime = track->GetSecondaryTime(i, s + 1);
				if (secondaryTime > 0.0f)
				{
					const int x2 = TimeToClient(secondaryTime);

					CPen penThin(PS_SOLID, 1, RGB(80, 100, 80));
					dc->SelectObject(&penThin);
					dc->MoveTo(x2 + 1, rc.top);
					dc->LineTo(x2 + 1, rc.bottom);
					dc->MoveTo(x2 - 1, rc.top);
					dc->LineTo(x2 - 1, rc.bottom);
				}
			}
		}

		if ((renderFlags & DRAW_NAMES) && description)
		{
			if (hasDuration)
			{
				keydesc[0] = '\0';
			}
			else
			{
				cry_strcpy(keydesc, "{");
			}
			cry_strcat(keydesc, description);
			if (!hasDuration)
			{
				cry_strcat(keydesc, "}");
			}
			// Draw key description text.
			// Find next key.
			int x1 = rc.right; // End of dope sheet
			int nextKey = track->NextKeyByTime(i);
			if (nextKey > 0)
			{
				float nextKeyTime = track->GetKeyTime(nextKey);
				if (paramType == SEQUENCER_PARAM_FRAGMENTID)
				{
					CFragmentKey nextFragKey;
					track->GetKey(nextKey, &nextFragKey);
					if (nextFragKey.transition)
					{
						nextKeyTime = max(nextKeyTime, nextFragKey.tranStartTime);
					}

					CFragmentTrack* pFragTrack = (CFragmentTrack*)track;
					int nextFragID = pFragTrack->GetNextFragmentKey(i);
					if (nextFragID >= 0)
					{
						nextKeyTime = track->GetKeyTime(nextFragID);
					}
					else
					{
						nextKeyTime = rc.right;
					}
				}
				x1 = TimeToClient(nextKeyTime) - 10;
			}

			const int hasBlendTime = track->GetNumSecondarySelPts(i) > 0;
			const int blendEnd = hasBlendTime ? TimeToClient(track->GetSecondaryTime(i, 1)) : xClipStart;
			const int textStart = max(int(rc.left), blendEnd) + 4;
			int textEnd = x1;
			if (limitTextToDuration)
			{
				textEnd = min(x1, xKeyTimeEnd);
			}
			CRect textRect(textStart, rc.top, textEnd, rc.bottom);
			dc->DrawText(keydesc, strlen(keydesc), textRect, DT_LEFT | DT_END_ELLIPSIS | DT_VCENTER | DT_SINGLELINE);
		}

		// don't display key handles that aren't on screen, only applies to zero length keys now anyway
		if (xKeyTime < 0)
			continue;

		if (renderFlags & DRAW_HANDLES)
		{
			if (track->CanEditKey(i) && !hasDuration)
			{
				int imageID = 0;
				if (abs(xKeyTime - prevKeyPixel) < 2)
				{
					// If multiple keys on the same time.
					imageID = 2;
				}
				else if (track->IsKeySelected(i))
				{
					imageID = 1;
				}

				const int iconOffset = (gMannequinPreferences.trackSize / 2) - 8;

				m_imageList.Draw(dc, imageID, CPoint(xKeyTime - 6, rc.top + 2 + iconOffset), ILD_TRANSPARENT);
			}

			prevKeyPixel = xKeyTime;
		}
	}
	dc->SelectObject(prevFont);
}

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheet::FirstKeyFromPoint(CPoint point, bool exact) const
{
	const int item = ItemFromPoint(point);
	if (item < 0)
		return -1;

	const float t1 = TimeFromPointUnsnapped(CPoint(point.x - 4, point.y));
	const float t2 = TimeFromPointUnsnapped(CPoint(point.x + 4, point.y));

	CSequencerTrack* track = GetTrack(item);
	if (!track)
		return -1;

	const int numKeys = track->GetNumKeys();

	for (int i = numKeys - 1; i >= 0; i--)
	{
		const float time = track->GetKeyTime(i);
		if (time >= t1 && time <= t2)
		{
			return i;
		}
	}

	if (!exact)
	{
		typedef std::vector<std::pair<int, float>> TGoodKeys;
		TGoodKeys goodKeys;
		for (int i = numKeys - 1; i >= 0; i--)
		{
			const float time = track->GetKeyTime(i);
			const float duration = track->GetKeyDuration(i);
			if (time + duration >= t1 && time <= t2)
			{
				//return i;
				goodKeys.push_back(std::pair<int, float>(i, time));
			}
		}

		// pick the "best" key
		if (!goodKeys.empty())
		{
			int bestKey = goodKeys.front().first;
			float prevStart = -10000.0f;
			float prevEnd = 10000.0f;
			for (TGoodKeys::const_iterator itey = goodKeys.begin(); itey != goodKeys.end(); ++itey)
			{
				const int currKey = (*itey).first;
				const float duration = track->GetKeyDuration(currKey);
				const float start = (*itey).second;
				const float end = start + duration;
				// is this key covered by the previous best key?
				if ((prevStart <= start) && (prevEnd >= end))
				{
					// yup, so to allow it to be picked at all we make this the new best key
					prevStart = start;
					prevEnd = end;
					bestKey = currKey;
				}
			}

			return bestKey;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheet::LastKeyFromPoint(CPoint point, bool exact) const
{
	const int item = ItemFromPoint(point);
	if (item < 0)
		return -1;

	const float t1 = TimeFromPointUnsnapped(CPoint(point.x - 4, point.y));
	const float t2 = TimeFromPointUnsnapped(CPoint(point.x + 4, point.y));

	CSequencerTrack* track = GetTrack(item);
	if (!track)
		return -1;

	const int numKeys = track->GetNumKeys();

	for (int i = numKeys - 1; i >= 0; i--)
	{
		const float time = track->GetKeyTime(i);
		if (time >= t1 && time <= t2)
		{
			return i;
		}
	}

	if (!exact)
	{
		typedef std::vector<std::pair<int, float>> TGoodKeys;
		TGoodKeys goodKeys;
		for (int i = 0; i < numKeys; ++i)
		{
			const float time = track->GetKeyTime(i);
			const float duration = track->GetKeyDuration(i);
			if (time + duration >= t1 && time <= t2)
			{
				//return i;
				goodKeys.push_back(std::pair<int, float>(i, time));
			}
		}

		// pick the "best" key
		if (!goodKeys.empty())
		{
			int bestKey = (goodKeys.end() - 1)->first;
			float prevStart = -10000.0f;
			float prevEnd = 10000.0f;
			for (TGoodKeys::const_reverse_iterator itey = goodKeys.rbegin(); itey != goodKeys.rend(); ++itey)
			{
				const int currKey = (*itey).first;
				const float duration = track->GetKeyDuration(currKey);
				const float start = (*itey).second;
				const float end = start + duration;
				// is this key covered by the previous best key?
				if ((prevStart <= start) && (prevEnd >= end))
				{
					// yup, so to allow it to be picked at all we make this the new best key
					prevStart = start;
					prevEnd = end;
					bestKey = currKey;
				}
			}

			return bestKey;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
int CSequencerDopeSheet::NumKeysFromPoint(CPoint point) const
{
	int item = ItemFromPoint(point);
	if (item < 0)
		return -1;

	float t1 = TimeFromPointUnsnapped(CPoint(point.x - 4, point.y));
	float t2 = TimeFromPointUnsnapped(CPoint(point.x + 4, point.y));

	CSequencerTrack* track = GetTrack(item);
	if (!track)
		return -1;

	int count = 0;
	int numKeys = track->GetNumKeys();
	for (int i = 0; i < numKeys; i++)
	{
		float time = track->GetKeyTime(i);
		if (time >= t1 && time <= t2)
		{
			++count;
		}
	}
	return count;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheet::SelectKeys(const CRect& rc)
{
	// put selection rectangle from client to item space.
	CRect rci = rc;
	rci.OffsetRect(m_scrollOffset);

	Range selTime = GetTimeRange(rci);

	CRect rcItem;
	for (int i = 0; i < GetCount(); i++)
	{
		GetItemRect(i, rcItem);
		// Decrease item rectangle a bit.
		rcItem.DeflateRect(4, 4, 4, 4);
		// Check if item rectanle intersects with selection rectangle in y axis.
		if ((rcItem.top >= rc.top && rcItem.top <= rc.bottom) ||
		    (rcItem.bottom >= rc.top && rcItem.bottom <= rc.bottom) ||
		    (rc.top >= rcItem.top && rc.top <= rcItem.bottom) ||
		    (rc.bottom >= rcItem.top && rc.bottom <= rcItem.bottom))
		{
			CSequencerTrack* track = GetTrack(i);
			if (!track)
				continue;

			// Check which keys we intersect.
			for (int j = 0; j < track->GetNumKeys(); j++)
			{
				float time = track->GetKeyTime(j);
				if (selTime.IsInside(time))
				{
					track->SelectKey(j, true);
					m_bAnySelected = true;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerDopeSheet::DrawNodeItem(CSequencerNode* pAnimNode, CDC* dc, CRect& rcItem)
{
	CFont* prevFont = dc->SelectObject(m_descriptionFont);

	dc->SetTextColor(KEY_TEXT_COLOR);
	dc->SetBkMode(TRANSPARENT);

	CRect textRect = rcItem;
	textRect.left += 4;
	textRect.right -= 4;

	string sAnimNodeName = pAnimNode->GetName();
	dc->DrawText(sAnimNodeName.c_str(), sAnimNodeName.length(), textRect, DT_LEFT | DT_END_ELLIPSIS | DT_VCENTER | DT_SINGLELINE);

	dc->SelectObject(prevFont);
}

