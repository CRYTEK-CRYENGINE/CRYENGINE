// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if _MSC_VER > 1000
	#pragma once
	#pragma pack(push)
	#pragma pack(1)
#endif // _MSC_VER > 1000

#ifndef MemDCh
	#define MemDCh

/*
   This is a slightly optimised version of the popular CMemDC class.
   For the fastest redraw put a bitmap in your Dialog, Control or View header:
   CBitmap Bitmap;
   In the .cpp file:
   #include "MemDC.h"
   and then call the following just once:
   Bitmap.CreateCompatibleBitmap(pDC, Rect.Width(), Rect.Height());
   For a Modal dialog call the above in OnInitDlg otherwise in your class's constructor.
   If you don't have pDC use GetDC() which returns the DC of the screen.

   If you're using OnPaint() use the following two lines:
   CPaintDC  PaintDC(this); // device context for painting
   CMemDC dc(PaintDC, &Bitmap);
   Then draw everything to dc - and thats it!

   You don't need the Bitmap: you can just create the CMemDC with:

   void CMyDlg::OnPaint() {
   CPaintDC  PaintDC(this); // device context for painting
   CMemDC dc(PaintDC);

   it'll be slower for complex animations, but fine for most Controls.

   The same applies to OwnerDraw Buttons, create the MemDC object as follows:
   void CMemDCTesterDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct) {
   CMemDC dc(lpDrawItemStruct, &Bitmap);

   Theres also a constructor for Views:
   void CMyView::OnDraw(CDC* pDC) {
   CMemDC dc(pDC, &Bitmap);


   Remember the following tips:

   if you're filling the background yourself:
   Remember to either do it in OnEraseBkgnd or to not do it at all use:
   BOOL CMyDlgOrControl::OnEraseBkgnd(CDC* pDC) {return TRUE;}

   If you're filling it black or white use:
   PatBlt(Rect.left, Rect.top, Rect.Width(), Rect.Height(), BLACKNESS);
   or:
   PatBlt(Rect.left, Rect.top, Rect.Width(), Rect.Height(), WHITENESS);
   to fill with the current dialog background colour use:
   dc.FillSolidRect(&Rect, GetSysColor(COLOR_3DFACE)); //Draw in dialogs background colour

   To draw text using the same font as the dialog box use:
   CGdiObject*  OldFont=dc.SelectObject(GetParent()->GetFont());
   then use dc.DrawText(...);
   and finish with
   dc.SelectObject(OldFont);

   To draw using the system dialog font use:
   CGdiObject* OldFont=dc.SelectStockObject(ANSI_VAR_FONT);
   then use dc.DrawText(...);
   and finish with
   dc.SelectObject(&OldFont);

   It is best to use a class to encapsulate the clean-up:
   class CUseDialogFont {
   CGdiObject* OldFont;
   CDC* pDC;
   public:
   CUseDialogFont(CDC* _pDC) {pDC=_pDC; OldFont=pDC->SelectStockObject(ANSI_VAR_FONT);}
   ~CUseDialogFont()          {pDC->SelectObject(&OldFont);}
   };

   Then in your code just use:
   CUseDialogFont Font(&dc);
   That way you can 'return' at any time safe in the knowledge that the original font will be selected.
 */
class CMemoryDC : public CDC
{
public:
	CMemoryDC(CPaintDC& dc, CBitmap* pBmp = NULL) : isMemDC(!dc.IsPrinting())
	{
		//if(dc.GetWindow()) (dc.GetWindow())->GetClientRect(&Rect);
		//else
		Rect = dc.m_ps.rcPaint;
		Set(&dc, Paint, pBmp);
	}
	CMemoryDC(CPaintDC& dc, CRect& rect, CBitmap* pBmp = NULL) : isMemDC(!dc.IsPrinting())
	{
		Rect = rect;
		Set(&dc, Paint, pBmp);
	}
	CMemoryDC(CDC* pDC, CBitmap* pBmp = NULL)
	{
		isMemDC = !pDC->IsPrinting();
		if (isMemDC)
		{
			pDC->GetClipBox(&Rect); //For Views
			pDC->LPtoDP(&Rect);
		}
		Set(pDC, Draw, pBmp);
	}
	CMemoryDC(LPDRAWITEMSTRUCT lpDrawItemStruct, CBitmap* pBmp = NULL) : isMemDC(true)
	{
		Rect = lpDrawItemStruct->rcItem;
		Set(CDC::FromHandle(lpDrawItemStruct->hDC), DrawItem, pBmp);
	}
	~CMemoryDC()
	{
		// Destructor copies the contents of the mem DC to the original DC
		if (isMemDC)
		{
			pDC->BitBlt(Rect.left, Rect.top, Rect.Width(), Rect.Height(), this, Rect.left, Rect.top, SRCCOPY);
			SelectObject(OldBitmap);
		}
	}

	CMemoryDC* operator->() { return this; } // Allow usage as a pointer
	operator CMemoryDC*()   { return this; } // Allow usage as a pointer

private:
	enum dcType {Paint, Draw, DrawItem};

	void Set(CDC* _pDC, dcType Type, CBitmap* pBmp = NULL)
	{
		ASSERT_VALID(_pDC);
		pDC = _pDC;
		OldBitmap = NULL;
		if (isMemDC)
		{
			CreateCompatibleDC(pDC);
			if (pBmp != NULL) OldBitmap = SelectObject(pBmp); //User passed bitmap, use it
			else                                              //Create our own bitmap
			{
				CRect rc;
				pDC->GetWindow()->GetClientRect(rc);
				Bitmap.CreateCompatibleBitmap(pDC, rc.Width(), rc.Height());
				OldBitmap = SelectObject(&Bitmap);
			}
			if (Type == Draw)
			{
				SetMapMode(pDC->GetMapMode());
				pDC->DPtoLP(&Rect);
				SetWindowOrg(Rect.left, Rect.top);
			}
		}
		else   // Make a copy of the relevant parts of the current DC for printing
		{
			m_bPrinting = pDC->m_bPrinting;
			m_hDC = pDC->m_hDC;
			m_hAttribDC = pDC->m_hAttribDC;
		}
	}

	CBitmap  Bitmap;     // Offscreen bitmap
	CBitmap* OldBitmap;  // bitmap originally found in CMemDC
	CDC*     pDC;        // Saves CDC passed in constructor
	CRect    Rect;       // Rectangle of drawing area.
	bool     isMemDC;    // TRUE if CDC really is a Memory DC.
};

	#if _MSC_VER >= 1000
		#pragma pack(pop)
	#endif // _MSC_VER >= 1000

#endif //ndef MemDCh

