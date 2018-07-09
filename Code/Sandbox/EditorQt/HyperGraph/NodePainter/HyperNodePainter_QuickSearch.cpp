// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperNodePainter_QuickSearch.h"

#include "HyperGraph/Nodes/QuickSearchNode.h"
#include "HyperGraph/HyperGraph.h"
#include "HyperGraph/FlowGraphPreferences.h"

namespace
{
struct SQuickSearchAssets
{
	SQuickSearchAssets() :
		font(L"Tahoma", 10.0f, Gdiplus::FontStyleBold),
		brushBackground(GET_GDI_COLOR(gFlowGraphColorPreferences.colorQuickSearchBackground)),
		brushTextCount(GET_GDI_COLOR(gFlowGraphColorPreferences.colorQuickSearchCountText)),
		brushTextResult(GET_GDI_COLOR(gFlowGraphColorPreferences.colorQuickSearchResultText)),
		penBorder(GET_GDI_COLOR(gFlowGraphColorPreferences.colorQuickSearchBorder))
	{
		sf.SetAlignment(Gdiplus::StringAlignmentCenter);
		sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
		sf.SetTrimming(Gdiplus::StringTrimmingEllipsisPath);
	}

	void Update()
	{
		brushBackground.SetColor(GET_GDI_COLOR(gFlowGraphColorPreferences.colorQuickSearchBackground));
		brushTextCount.SetColor(GET_GDI_COLOR(gFlowGraphColorPreferences.colorQuickSearchCountText));
		brushTextResult.SetColor(GET_GDI_COLOR(gFlowGraphColorPreferences.colorQuickSearchResultText));
		penBorder.SetColor(GET_GDI_COLOR(gFlowGraphColorPreferences.colorQuickSearchBorder));
	}

	Gdiplus::Font         font;
	Gdiplus::SolidBrush   brushBackground;
	Gdiplus::SolidBrush   brushTextCount;
	Gdiplus::SolidBrush   brushTextResult;
	Gdiplus::StringFormat sf;
	Gdiplus::Pen          penBorder;
};

}

void CHyperNodePainter_QuickSearch::Paint(CHyperNode* pNode, CDisplayList* pList)
{
	static SQuickSearchAssets* pAssets = 0;
	if (!pAssets)
		pAssets = new SQuickSearchAssets();

	pAssets->Update();

	CDisplayRectangle* pBackground = pList->Add<CDisplayRectangle>()
	                                 ->SetHitEvent(eSOID_ShiftFirstOutputPort)
	                                 ->SetStroked(&pAssets->penBorder);

	CString text = pNode->GetName();
	text.Replace('_', ' ');
	text.Replace("\\n", "\r\n");

	CDisplayString* pString = pList->Add<CDisplayString>()
	                          ->SetHitEvent(eSOID_ShiftFirstOutputPort)
	                          ->SetText(text)
	                          ->SetBrush(&pAssets->brushTextResult)
	                          ->SetFont(&pAssets->font)
	                          ->SetStringFormat(&pAssets->sf);

	pBackground->SetFilled(&pAssets->brushBackground);

	CQuickSearchNode* pQuickSearchNode = static_cast<CQuickSearchNode*>(pNode);
	CString resultCount = "";
	resultCount.Format("%d / %d", pQuickSearchNode->GetIndex(), pQuickSearchNode->GetSearchResultCount());
	CDisplayString* pSearchCountString = pList->Add<CDisplayString>()
	                                     ->SetHitEvent(eSOID_ShiftFirstOutputPort)
	                                     ->SetText(resultCount)
	                                     ->SetBrush(&pAssets->brushTextCount)
	                                     ->SetFont(&pAssets->font)
	                                     ->SetStringFormat(&pAssets->sf);

	Gdiplus::RectF rect;
	rect.Width = 300.0f;
	rect.Height = 50.0f;
	rect.X = rect.Y = 0.0f;
	pBackground->SetRect(rect);
	rect.Y = 13.0f;

	Gdiplus::RectF rectString = pString->GetBounds(pList->GetGraphics());

	if (rectString.Width > rect.Width)
	{
		text.Delete(0, text.ReverseFind(':'));
		CString trimmedText = "...";
		trimmedText.Append(text);
		pString->SetText(trimmedText);
	}

	pString->SetRect(rect);
	pString->SetLocation(Gdiplus::PointF(150.0f, 32.0f));
	pSearchCountString->SetLocation(Gdiplus::PointF(150.0f, 32.0f));
	pBackground->SetHitEvent(eSOID_Title);
	pNode->SetRect(rect);

	Gdiplus::PointF start(0.0f, 32.0f);
	Gdiplus::PointF end(300.0f, 32.0f);

	pList->AddLine(start, end, &pAssets->penBorder);

	start.Y = 14.0f;
	end.Y = 14.0f;
	pList->AddLine(start, end, &pAssets->penBorder);
}

