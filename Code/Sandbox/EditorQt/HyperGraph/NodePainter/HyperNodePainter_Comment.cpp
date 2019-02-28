// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperNodePainter_Comment.h"

#include "HyperGraph/FlowGraphPreferences.h"

namespace
{

struct SAssetsComment
{
	SAssetsComment() :
		font(L"Tahoma", 10.13f),
		brushBackground(Gdiplus::Color(235, 255, 245)),
		brushBackgroundSelected(Gdiplus::Color(250, 255, 250)),
		brushText(Gdiplus::Color(25, 25, 25)),
		brushTextSelected(Gdiplus::Color(0, 0, 0)),
		penOutline(GET_GDI_COLOR(gFlowGraphColorPreferences.colorNodeOutline)),
		penOutlineSelected(GET_GDI_COLOR(gFlowGraphColorPreferences.colorNodeOutlineSelected), 3.0f)
	{
		sf.SetAlignment(Gdiplus::StringAlignmentCenter);
	}

	Gdiplus::Font         font;
	Gdiplus::SolidBrush   brushBackground;
	Gdiplus::SolidBrush   brushBackgroundSelected;
	Gdiplus::SolidBrush   brushText;
	Gdiplus::SolidBrush   brushTextSelected;
	Gdiplus::Pen          penOutline;
	Gdiplus::Pen          penOutlineSelected;
	Gdiplus::StringFormat sf;
};

}

void CHyperNodePainter_Comment::Paint(CHyperNode* pNode, CDisplayList* pList)
{
	static SAssetsComment* pAssets = 0;
	if (!pAssets)
		pAssets = new SAssetsComment();

	if (pNode->GetBlackBox()) //hide node
		return;

	CDisplayRectangle* pBackground = pList->Add<CDisplayRectangle>()
	                                 ->SetHitEvent(eSOID_ShiftFirstOutputPort);

	CString text = pNode->GetName();
	text.Replace('_', ' ');
	text.Replace("\\n", "\r\n");

	CDisplayString* pString = pList->Add<CDisplayString>()
	                          ->SetHitEvent(eSOID_ShiftFirstOutputPort)
	                          ->SetText(text)
	                          ->SetFont(&pAssets->font)
	                          ->SetStringFormat(&pAssets->sf);

	if (pNode->IsSelected())
	{
		pString->SetBrush(&pAssets->brushTextSelected);
		pBackground->SetFilled(&pAssets->brushBackgroundSelected);
		pBackground->SetStroked(&pAssets->penOutlineSelected);
	}
	else
	{
		pString->SetBrush(&pAssets->brushText);
		pBackground->SetFilled(&pAssets->brushBackground);
		pBackground->SetStroked(&pAssets->penOutline);
	}

	Gdiplus::RectF rect = pString->GetBounds(pList->GetGraphics());
	const float heightOriginal = rect.Height;
	rect.Width += 32.0f;
	rect.Height = floor((rect.Height / GRID_SIZE) + 0.5f) * GRID_SIZE;
	rect.X = rect.Y = 0.0f;
	pBackground->SetRect(rect);
	pString->SetLocation(Gdiplus::PointF(rect.Width * 0.5f, ((float)rect.Height - heightOriginal) / 2));

	pBackground->SetHitEvent(eSOID_Title);
}

