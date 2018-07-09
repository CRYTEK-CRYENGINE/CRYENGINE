// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperNodePainter_CommentBox.h"

#define TRANSPARENT_ALPHA 50   // 0..255
#define BORDER_SIZE       2.f
#define FONT_SIZE_SMALL   15
#define FONT_SIZE_MEDIUM  20
#define FONT_SIZE_BIG     40

// those are used to create a table with a range of font sizes
#define SMALLEST_FONT 10
#define BIGGEST_FONT  200
#define FONT_STEP     5
#define NUM_FONTS     (((BIGGEST_FONT - SMALLEST_FONT) / FONT_STEP) + 1)

namespace
{
struct SAssetsCommentBox
{
	SAssetsCommentBox()
		: blackBrush(Gdiplus::Color::Black)
	{
		assert((SMALLEST_FONT + (NUM_FONTS - 1) * FONT_STEP) == BIGGEST_FONT);          // checks that FONT_STEP does match the size limits
		sf.SetAlignment(Gdiplus::StringAlignmentCenter);

		for (int i = 0; i < NUM_FONTS; ++i)
		{
			float size = SMALLEST_FONT + i * FONT_STEP;
			m_fonts[i].reset(new Gdiplus::Font(L"Tahoma", size));
		}
	}
	~SAssetsCommentBox()
	{
	}

	std::auto_ptr<Gdiplus::Font> m_fonts[NUM_FONTS];   // only fonts that are actually used are created, although there is room in the array for pointers to all of them
	Gdiplus::StringFormat        sf;
	Gdiplus::SolidBrush          blackBrush;
};
}

void CHyperNodePainter_CommentBox::Paint(CHyperNode* pNode, CDisplayList* pList)
{
	static SAssetsCommentBox assets;

	if (pNode->GetBlackBox()) //hide node
		return;

	const CHyperNode::Ports& inputs = *pNode->GetInputs();
	bool bFilled = false;
	bool bDisplayBox = true;
	int fontSize = 0;
	Vec3 rawColor(0, 0, 0);
	for (int i = 0; i < inputs.size(); ++i)
	{
		const CHyperNodePort& port = inputs[i];
		if (strcmp(port.pVar->GetName(), "TextSize") == 0)
			port.pVar->Get(fontSize);
		if (strcmp(port.pVar->GetName(), "DisplayFilled") == 0)
			port.pVar->Get(bFilled);
		if (strcmp(port.pVar->GetName(), "DisplayBox") == 0)
			port.pVar->Get(bDisplayBox);
		if (strcmp(port.pVar->GetName(), "Color") == 0)
			port.pVar->Get(rawColor);
	}

	rawColor = rawColor * 255;

	if (pNode->IsSelected())
		rawColor = rawColor * 1.2f;
	rawColor.x = rawColor.x > 255 ? 255 : rawColor.x;
	rawColor.y = rawColor.y > 255 ? 255 : rawColor.y;
	rawColor.z = rawColor.z > 255 ? 255 : rawColor.z;

	Gdiplus::Color colorTransparent(TRANSPARENT_ALPHA, rawColor.x, rawColor.y, rawColor.z);
	Gdiplus::Color colorSolid(rawColor.x, rawColor.y, rawColor.z);

	m_brushSolid.SetColor(colorSolid);
	m_brushTransparent.SetColor(colorTransparent);

	CDisplayRectangle* pBackground = pList->Add<CDisplayRectangle>();
	CDisplayRectangle* pBorderUp = pList->Add<CDisplayRectangle>();
	CDisplayRectangle* pBorderDown = pList->Add<CDisplayRectangle>();
	CDisplayRectangle* pBorderRight = pList->Add<CDisplayRectangle>();
	CDisplayRectangle* pBorderLeft = pList->Add<CDisplayRectangle>();
	CDisplayRectangle* pBorderUpLeft = pList->Add<CDisplayRectangle>();
	CDisplayRectangle* pBorderDownLeft = pList->Add<CDisplayRectangle>();
	CDisplayRectangle* pBorderUpRight = pList->Add<CDisplayRectangle>();
	CDisplayRectangle* pBorderDownRight = pList->Add<CDisplayRectangle>();

	pBackground->SetHitEvent(eSOID_InputTransparent);

	if (bDisplayBox)
	{
		pBorderUp->SetHitEvent(eSOID_Border_Up)->SetFilled(&m_brushSolid);
		pBorderDown->SetHitEvent(eSOID_Border_Down)->SetFilled(&m_brushSolid);
		pBorderRight->SetHitEvent(eSOID_Border_Right)->SetFilled(&m_brushSolid);
		pBorderLeft->SetHitEvent(eSOID_Border_Left)->SetFilled(&m_brushSolid);
		pBorderUpLeft->SetHitEvent(eSOID_Border_UpLeft)->SetFilled(&m_brushSolid);
		pBorderDownLeft->SetHitEvent(eSOID_Border_DownLeft)->SetFilled(&m_brushSolid);
		pBorderUpRight->SetHitEvent(eSOID_Border_UpRight)->SetFilled(&m_brushSolid);
		pBorderDownRight->SetHitEvent(eSOID_Border_DownRight)->SetFilled(&m_brushSolid);
		if (bFilled)
			pBackground->SetFilled(&m_brushTransparent);
	}
	else
	{
		pBorderUp->SetHitEvent(eSOID_InputTransparent);
		pBorderDown->SetHitEvent(eSOID_InputTransparent);
		pBorderRight->SetHitEvent(eSOID_InputTransparent);
		pBorderLeft->SetHitEvent(eSOID_InputTransparent);
		pBorderUpLeft->SetHitEvent(eSOID_InputTransparent);
		pBorderDownLeft->SetHitEvent(eSOID_InputTransparent);
		pBorderUpRight->SetHitEvent(eSOID_InputTransparent);
		pBorderDownRight->SetHitEvent(eSOID_InputTransparent);
	}

	CString text = pNode->GetName();
	text.Replace('_', ' ');
	text.Replace("\\n", "\r\n");

	// choose the font size depending on the size specified for this commentBox, and the current zoom
	int normalSize;
	switch (fontSize)
	{
	case 1:
		normalSize = FONT_SIZE_SMALL;
		break;
	case 2:
		normalSize = FONT_SIZE_MEDIUM;
		break;
	case 3:
		normalSize = FONT_SIZE_BIG;
		break;
	default:
		normalSize = FONT_SIZE_MEDIUM;
		break;
	}
	float desiredFontSize = normalSize * AccessStaticVar_ZoomFactor();
	int fontIndex = (desiredFontSize - SMALLEST_FONT) / FONT_STEP;
	fontIndex = CLAMP(fontIndex, 0, NUM_FONTS - 1);

	Gdiplus::Font* pFont = assets.m_fonts[fontIndex].get();

	CDisplayString* pStringShadow = pList->Add<CDisplayTitle>()
	                                ->SetHitEvent(eSOID_Title)
	                                ->SetText(text)
	                                ->SetBrush(&assets.blackBrush)
	                                ->SetFont(pFont)
	                                ->SetStringFormat(&assets.sf);

	CDisplayString* pString = pList->Add<CDisplayTitle>()
	                          ->SetHitEvent(eSOID_Title)
	                          ->SetText(text)
	                          ->SetBrush(&m_brushSolid)
	                          ->SetFont(pFont)
	                          ->SetStringFormat(&assets.sf);

	Gdiplus::Graphics* pG = pList->GetGraphics();
	Gdiplus::RectF stringBounds = pString->GetBounds(pG);
	pString->SetRect(0, 0, stringBounds.Width, stringBounds.Height);

	float shadowDisp = AccessStaticVar_ZoomFactor();
	pStringShadow->SetRect(shadowDisp, shadowDisp, stringBounds.Width, stringBounds.Height);

	if (!pNode->GetResizeBorderRect())
		return;

	// recalcs stringsborder size and pos if needed
	Gdiplus::RectF rectFillArea = *pNode->GetResizeBorderRect();

	rectFillArea.Y = pNode->GetRect().Height - rectFillArea.Height;
	float posYText = rectFillArea.Y - (stringBounds.Height + shadowDisp);
	pString->SetRect(0, posYText, stringBounds.Width, stringBounds.Height);
	pStringShadow->SetRect(shadowDisp, posYText + shadowDisp, stringBounds.Width, stringBounds.Height);

	float borderThickness = AccessStaticVar_ZoomFactor() * BORDER_SIZE;                           // adjust border thickness depending of zoom
	borderThickness = min(borderThickness, min(rectFillArea.Height / 2, rectFillArea.Width / 2)); // to avoid overlapping with big zoom out levels

	//...background
	Gdiplus::RectF rectTemp = rectFillArea;
	rectTemp.X += borderThickness;
	rectTemp.Y += borderThickness;
	rectTemp.Width -= borderThickness * 2;
	rectTemp.Height -= borderThickness * 2;
	pBackground->SetRect(rectTemp);

	//...borders
	rectTemp = rectFillArea;
	rectTemp.Height = borderThickness;
	pBorderUp->SetRect(rectTemp);

	rectTemp = rectFillArea;
	rectTemp.Y = rectTemp.GetBottom() - borderThickness;
	rectTemp.Height = borderThickness;
	pBorderDown->SetRect(rectTemp);

	rectTemp = rectFillArea;
	rectTemp.X = rectTemp.GetRight() - borderThickness;
	rectTemp.Width = borderThickness;
	pBorderRight->SetRect(rectTemp);

	rectTemp = rectFillArea;
	rectTemp.Width = borderThickness;
	pBorderLeft->SetRect(rectTemp);

	//...corners
	rectTemp = rectFillArea;
	rectTemp.Width = rectTemp.Height = borderThickness * 2;
	pBorderUpLeft->SetRect(rectTemp);

	rectTemp.X = rectFillArea.GetRight() - borderThickness * 2;
	rectTemp.Y = rectFillArea.Y;
	pBorderUpRight->SetRect(rectTemp);

	rectTemp.X = rectFillArea.X;
	rectTemp.Y = rectFillArea.GetBottom() - borderThickness * 2;
	pBorderDownLeft->SetRect(rectTemp);

	rectTemp.X = rectFillArea.GetRight() - borderThickness * 2;
	rectTemp.Y = rectFillArea.GetBottom() - borderThickness * 2;
	pBorderDownRight->SetRect(rectTemp);
}

