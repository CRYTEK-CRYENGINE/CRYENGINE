// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ImageHistogramCtrl.h"

// CImageHistogramCtrl

// control tweak constants
namespace ImageHistogram
{
const float kGraphHeightPercent = 0.7f;
const int kGraphMargin = 4;
const COLORREF kBackColor = RGB(100, 100, 100);
const COLORREF kRedSectionColor = RGB(255, 220, 220);
const COLORREF kGreenSectionColor = RGB(220, 255, 220);
const COLORREF kBlueSectionColor = RGB(220, 220, 255);
const COLORREF kSplitSeparatorColor = RGB(100, 100, 0);
const COLORREF kButtonBackColor = RGB(20, 20, 20);
const COLORREF kBtnLightColor = RGB(200, 200, 200);
const COLORREF kBtnShadowColor = RGB(50, 50, 50);
const int kButtonWidth = 40;
const COLORREF kButtonTextColor = RGB(255, 255, 0);
const int kTextLeftSpacing = 4;
const int kTextFontSize = 70;
const char* kTextFontFace = "Arial";
const COLORREF kTextColor = RGB(255, 255, 255);
};

IMPLEMENT_DYNAMIC(CImageHistogramCtrl, CWnd)

CImageHistogramCtrl::CImageHistogramCtrl()
{
	m_graphMargin = ImageHistogram::kGraphMargin;
	m_drawMode = eDrawMode_Luminosity;
	m_imageFormat = eImageFormat_32BPP_RGBA;
	m_backColor = ImageHistogram::kBackColor;
	m_graphHeightPercent = ImageHistogram::kGraphHeightPercent;
	ClearHistogram();
}

CImageHistogramCtrl::~CImageHistogramCtrl()
{
}

BEGIN_MESSAGE_MAP(CImageHistogramCtrl, CWnd)
ON_WM_PAINT()
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_LBUTTONDOWN()
ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

// CImageHistogramCtrl message handlers

void CImageHistogramCtrl::OnPaint()
{
	if (!m_hWnd)
		return;

	CPaintDC dc(this);
	CDC dcMem;
	COLORREF penColor;
	CString str, mode;
	CRect rc, rcGraph;
	CPen penSpikes;
	CFont fnt;
	CPen redPen, greenPen, bluePen, alphaPen;

	if (!m_canvas.GetDC().GetSafeHdc())
		return;

	redPen.CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	greenPen.CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
	bluePen.CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
	alphaPen.CreatePen(PS_SOLID, 1, RGB(120, 120, 120));

	dcMem.Attach(m_canvas.GetDC().GetSafeHdc());
	GetClientRect(rc);

	if (!rc.Width() || !rc.Height())
		return;

	dcMem.FillSolidRect(0, 0, rc.Width(), rc.Height(), m_backColor);
	penColor = RGB(0, 0, 0);

	switch (m_drawMode)
	{
	case eDrawMode_Luminosity:
		mode = "Lum";
		break;
	case eDrawMode_OverlappedRGB:
		mode = "Overlap";
		break;
	case eDrawMode_SplitRGB:
		mode = "R|G|B";
		break;
	case eDrawMode_RedChannel:
		mode = "Red";
		penColor = RGB(255, 0, 0);
		break;
	case eDrawMode_GreenChannel:
		mode = "Green";
		penColor = RGB(0, 255, 0);
		break;
	case eDrawMode_BlueChannel:
		mode = "Blue";
		penColor = RGB(0, 0, 255);
		break;
	case eDrawMode_AlphaChannel:
		mode = "Alpha";
		penColor = RGB(120, 120, 120);
		break;
	}

	penSpikes.CreatePen(PS_SOLID, 1, penColor);
	dcMem.SelectObject(GetStockObject(BLACK_PEN));
	dcMem.SelectObject(GetStockObject(WHITE_BRUSH));
	rcGraph.SetRect(m_graphMargin, m_graphMargin, abs(rc.Width() - m_graphMargin), abs(rc.Height() * m_graphHeightPercent));
	dcMem.Rectangle(&rcGraph);
	dcMem.SelectObject(penSpikes);

	int i = 0;
	int graphWidth = rcGraph.Width() != 0 ? abs(rcGraph.Width()) : 1;
	int graphHeight = abs(rcGraph.Height() - 2);
	int graphBottom = abs(rcGraph.bottom - 2);
	int crtX = 0;

	if (m_drawMode != eDrawMode_SplitRGB &&
	    m_drawMode != eDrawMode_OverlappedRGB)
	{
		for (size_t x = 0, xCount = abs(rcGraph.Width() - 2); x < xCount; ++x)
		{
			float scale = 0;

			i = ((float)x / graphWidth) * (kNumColorLevels - 1);
			i = CLAMP(i, 0, kNumColorLevels - 1);

			switch (m_drawMode)
			{
			case eDrawMode_Luminosity:
				{
					if (m_maxLumCount)
					{
						scale = (float)m_lumCount[i] / m_maxLumCount;
					}
					break;
				}

			case eDrawMode_RedChannel:
				{
					if (m_maxCount[0])
					{
						scale = (float)m_count[0][i] / m_maxCount[0];
					}
					break;
				}

			case eDrawMode_GreenChannel:
				{
					if (m_maxCount[1])
					{
						scale = (float)m_count[1][i] / m_maxCount[1];
					}
					break;
				}

			case eDrawMode_BlueChannel:
				{
					if (m_maxCount[2])
					{
						scale = (float)m_count[2][i] / m_maxCount[2];
					}
					break;
				}

			case eDrawMode_AlphaChannel:
				{
					if (m_maxCount[3])
					{
						scale = (float)m_count[3][i] / m_maxCount[3];
					}
					break;
				}
			}

			crtX = rcGraph.left + x + 1;
			dcMem.MoveTo(crtX, graphBottom);
			dcMem.LineTo(crtX, graphBottom - scale * graphHeight);
		}
	}
	else if (m_drawMode == eDrawMode_OverlappedRGB)
	{
		int lastHeight[kNumChannels] = { INT_MAX, INT_MAX, INT_MAX, INT_MAX };
		int heightR, heightG, heightB, heightA;
		float scaleR, scaleG, scaleB, scaleA;
		UINT crtX, prevX = INT_MAX;

		for (size_t x = 0, xCount = abs(rcGraph.Width() - 2); x < xCount; ++x)
		{
			i = ((float)x / graphWidth) * (kNumColorLevels - 1);
			i = CLAMP(i, 0, kNumColorLevels - 1);
			crtX = rcGraph.left + x + 1;
			scaleR = scaleG = scaleB = scaleA = 0;

			if (m_maxCount[0])
			{
				scaleR = (float)m_count[0][i] / m_maxCount[0];
			}

			if (m_maxCount[1])
			{
				scaleG = (float)m_count[1][i] / m_maxCount[1];
			}

			if (m_maxCount[2])
			{
				scaleB = (float)m_count[2][i] / m_maxCount[2];
			}

			if (m_maxCount[3])
			{
				scaleA = (float)m_count[3][i] / m_maxCount[3];
			}

			heightR = graphBottom - scaleR * graphHeight;
			heightG = graphBottom - scaleG * graphHeight;
			heightB = graphBottom - scaleB * graphHeight;
			heightA = graphBottom - scaleA * graphHeight;

			if (lastHeight[0] == INT_MAX)
			{
				lastHeight[0] = heightR;
			}

			if (lastHeight[1] == INT_MAX)
			{
				lastHeight[1] = heightG;
			}

			if (lastHeight[2] == INT_MAX)
			{
				lastHeight[2] = heightB;
			}

			if (lastHeight[3] == INT_MAX)
			{
				lastHeight[3] = heightA;
			}

			if (prevX == INT_MAX)
			{
				prevX = crtX;
			}

			dcMem.SelectObject(redPen);
			dcMem.MoveTo(prevX, lastHeight[0]);
			dcMem.LineTo(crtX, heightR);

			dcMem.SelectObject(greenPen);
			dcMem.MoveTo(prevX, lastHeight[1]);
			dcMem.LineTo(crtX, heightG);

			dcMem.SelectObject(bluePen);
			dcMem.MoveTo(prevX, lastHeight[2]);
			dcMem.LineTo(crtX, heightB);

			dcMem.SelectObject(alphaPen);
			dcMem.MoveTo(prevX, lastHeight[3]);
			dcMem.LineTo(crtX, heightA);

			lastHeight[0] = heightR;
			lastHeight[1] = heightG;
			lastHeight[2] = heightB;
			lastHeight[3] = heightA;
			prevX = crtX;
		}
	}
	else if (m_drawMode == eDrawMode_SplitRGB)
	{
		const float aThird = 1.0f / 3.0f;
		const int aThirdOfNumColorLevels = kNumColorLevels / 3;
		const int aThirdOfWidth = rcGraph.Width() / 3;
		CPen* pPen = NULL;
		float scale = 0, pos = 0;

		// draw 3 blocks so we can see channel spaces
		dcMem.FillSolidRect(rcGraph.left + 1, rcGraph.top + 1, aThirdOfWidth, rcGraph.Height() - 2, ImageHistogram::kRedSectionColor);
		dcMem.FillSolidRect(rcGraph.left + 1 + aThirdOfWidth, rcGraph.top + 1, aThirdOfWidth, rcGraph.Height() - 2, ImageHistogram::kGreenSectionColor);
		dcMem.FillSolidRect(rcGraph.left + 1 + aThirdOfWidth * 2, rcGraph.top + 1, aThirdOfWidth, rcGraph.Height() - 2, ImageHistogram::kBlueSectionColor);

		// 3 split RGB channel histograms
		for (size_t x = 0, xCount = abs(rcGraph.Width() - 2); x < xCount; ++x)
		{
			pos = (float)x / graphWidth;
			i = (float)((int)(pos * kNumColorLevels) % aThirdOfNumColorLevels) / aThirdOfNumColorLevels * kNumColorLevels;
			i = CLAMP(i, 0, kNumColorLevels - 1);
			scale = 0;

			// R
			if (pos < aThird)
			{
				if (m_maxCount[0])
				{
					scale = (float)m_count[0][i] / m_maxCount[0];
				}
				pPen = &redPen;
			}

			// G
			if (pos > aThird && pos < aThird * 2)
			{
				if (m_maxCount[1])
				{
					scale = (float) m_count[1][i] / m_maxCount[1];
				}
				pPen = &greenPen;
			}

			// B
			if (pos > aThird * 2)
			{
				if (m_maxCount[2])
				{
					scale = (float) m_count[2][i] / m_maxCount[2];
				}
				pPen = &bluePen;
			}

			dcMem.SelectObject(*pPen);
			dcMem.MoveTo(rcGraph.left + x + 1, graphBottom);
			dcMem.LineTo(rcGraph.left + x + 1, graphBottom - scale * graphHeight);
		}

		// then draw 3 lines so we separate the channels
		CPen wallPen;

		wallPen.CreatePen(PS_DOT, 1, ImageHistogram::kSplitSeparatorColor);
		dcMem.SelectObject(wallPen);
		dcMem.MoveTo(rcGraph.left + aThirdOfWidth, rcGraph.bottom);
		dcMem.LineTo(rcGraph.left + aThirdOfWidth, rcGraph.top);
		dcMem.MoveTo(rcGraph.left + aThirdOfWidth * 2, rcGraph.bottom);
		dcMem.LineTo(rcGraph.left + aThirdOfWidth * 2, rcGraph.top);
		wallPen.DeleteObject();
	}

	CRect rcBtnMode, rcText;

	rcBtnMode.SetRect(m_graphMargin, rcGraph.Height() + m_graphMargin * 2, m_graphMargin + ImageHistogram::kButtonWidth, rc.Height() - m_graphMargin);
	dcMem.FillSolidRect(&rcBtnMode, ImageHistogram::kButtonBackColor);
	dcMem.Draw3dRect(&rcBtnMode, ImageHistogram::kBtnLightColor, ImageHistogram::kBtnShadowColor);

	rcText = rcBtnMode;
	rcText.left = rcBtnMode.right + ImageHistogram::kTextLeftSpacing;
	rcText.right = rc.Width();

	float mean = 0, stdDev = 0, median = 0;

	switch (m_drawMode)
	{
	case eDrawMode_Luminosity:
	case eDrawMode_SplitRGB:
	case eDrawMode_OverlappedRGB:
		mean = m_meanAvg;
		stdDev = m_stdDevAvg;
		median = m_medianAvg;
		break;
	case eDrawMode_RedChannel:
		mean = m_mean[0];
		stdDev = m_stdDev[0];
		median = m_median[0];
		break;
	case eDrawMode_GreenChannel:
		mean = m_mean[1];
		stdDev = m_stdDev[1];
		median = m_median[1];
		break;
	case eDrawMode_BlueChannel:
		mean = m_mean[2];
		stdDev = m_stdDev[2];
		median = m_median[2];
		break;
	case eDrawMode_AlphaChannel:
		mean = m_mean[3];
		stdDev = m_stdDev[3];
		median = m_median[3];
		break;
	}

	str.Format("Mean: %0.2f StdDev: %0.2f Median: %0.2f", mean, stdDev, median);
	fnt.CreatePointFont(ImageHistogram::kTextFontSize, ImageHistogram::kTextFontFace);
	dcMem.SelectObject(fnt);
	dcMem.SetTextColor(ImageHistogram::kTextColor);
	dcMem.DrawText(str, &rcText, DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS);
	dcMem.SetTextColor(ImageHistogram::kButtonTextColor);
	dcMem.DrawText(mode, &rcBtnMode, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS);
	dcMem.Detach();
	fnt.DeleteObject();
	penSpikes.DeleteObject();
	redPen.DeleteObject();
	greenPen.DeleteObject();
	bluePen.DeleteObject();
	alphaPen.DeleteObject();
	m_canvas.BlitTo(dc.GetSafeHdc());
}

void CImageHistogramCtrl::ComputeHistogram(BYTE* pImageData, UINT aWidth, UINT aHeight, EImageFormat aFormat)
{
	ClearHistogram();
	UINT r, g, b, a;
	int lumIndex = 0;
	UINT pixelCount = aWidth * aHeight;

	m_imageFormat = aFormat;

	while (pixelCount--)
	{
		r = g = b = a = 0;

		switch (aFormat)
		{
		case eImageFormat_32BPP_RGBA:
			{
				r = *pImageData;
				g = *(pImageData + 1);
				b = *(pImageData + 2);
				a = *(pImageData + 3);
				pImageData += 4;
				break;
			}

		case eImageFormat_32BPP_BGRA:
			{
				b = *pImageData;
				g = *(pImageData + 1);
				r = *(pImageData + 2);
				a = *(pImageData + 3);
				pImageData += 4;
				break;
			}

		case eImageFormat_32BPP_ARGB:
			{
				a = *(pImageData);
				r = *(pImageData + 1);
				g = *(pImageData + 2);
				b = *(pImageData + 3);
				pImageData += 4;
				break;
			}

		case eImageFormat_32BPP_ABGR:
			{
				a = *(pImageData);
				b = *(pImageData + 1);
				g = *(pImageData + 2);
				r = *(pImageData + 3);
				pImageData += 4;
				break;
			}

		case eImageFormat_24BPP_RGB:
			{
				r = *pImageData;
				g = *(pImageData + 1);
				b = *(pImageData + 2);
				pImageData += 3;
				break;
			}

		case eImageFormat_24BPP_BGR:
			{
				r = *pImageData;
				g = *(pImageData + 1);
				b = *(pImageData + 2);
				pImageData += 3;
				break;
			}

		case eImageFormat_8BPP:
			{
				r = *pImageData;
				++pImageData;
				break;
			}
		}

		++m_count[0][r];
		++m_count[1][g];
		++m_count[2][b];
		++m_count[3][a];

		lumIndex = (r + b + g) / 3;
		lumIndex = CLAMP(lumIndex, 0, kNumColorLevels - 1);
		++m_lumCount[lumIndex];

		if (m_maxCount[0] < m_count[0][r])
		{
			m_maxCount[0] = m_count[0][r];
		}

		if (m_maxCount[1] < m_count[1][g])
		{
			m_maxCount[1] = m_count[1][g];
		}

		if (m_maxCount[2] < m_count[2][b])
		{
			m_maxCount[2] = m_count[2][b];
		}

		if (m_maxCount[3] < m_count[3][a])
		{
			m_maxCount[3] = m_count[3][a];
		}

		if (m_maxLumCount < m_lumCount[lumIndex])
		{
			m_maxLumCount = m_lumCount[lumIndex];
		}
	}

	ComputeStatisticsForChannel(0);
	ComputeStatisticsForChannel(1);
	ComputeStatisticsForChannel(2);
	ComputeStatisticsForChannel(3);

	m_meanAvg = (m_mean[0] + m_mean[1] + m_mean[2]) / 3;
	m_stdDevAvg = (m_stdDev[0] + m_stdDev[1] + m_stdDev[2]) / 3;
	m_medianAvg = (m_median[0] + m_median[1] + m_median[2]) / 3;
}

void CImageHistogramCtrl::ComputeStatisticsForChannel(int aIndex)
{
	int hits = 0;
	int total = 0;

	//
	// mean
	// std deviation
	//
	for (size_t i = 0; i < kNumColorLevels; ++i)
	{
		hits = m_count[aIndex][i];
		m_mean[aIndex] += i * hits;
		m_stdDev[aIndex] += i * i * hits;
		total += hits;
	}

	total = (total ? total : 1);
	m_mean[aIndex] = (float) m_mean[aIndex] / total;
	m_stdDev[aIndex] = m_stdDev[aIndex] / total - m_mean[aIndex] * m_mean[aIndex];
	m_stdDev[aIndex] = m_stdDev[aIndex] <= 0 ? 0 : sqrtf(m_stdDev[aIndex]);

	int halfTotal = total / 2;
	int median = 0, v = 0;

	// find median value
	for (; median < kNumColorLevels; ++median)
	{
		v += m_count[aIndex][median];

		if (v >= halfTotal)
		{
			break;
		}
	}

	m_median[aIndex] = median;
}

void CImageHistogramCtrl::ClearHistogram()
{
	const int kSize = kNumColorLevels * sizeof(UINT);

	memset(m_count[0], 0, kSize);
	memset(m_count[1], 0, kSize);
	memset(m_count[2], 0, kSize);
	memset(m_count[3], 0, kSize);
	memset(m_lumCount, 0, kSize);
	m_maxCount[0] = 0;
	m_maxCount[1] = 0;
	m_maxCount[2] = 0;
	m_maxCount[3] = 0;
	m_maxLumCount = 0;
	memset(m_mean, 0, kNumChannels * sizeof(float));
	memset(m_stdDev, 0, kNumChannels * sizeof(float));
	memset(m_median, 0, kNumChannels * sizeof(float));
	m_meanAvg = m_stdDevAvg = m_medianAvg = 0;
}

void CImageHistogramCtrl::CopyComputedDataFrom(CImageHistogramCtrl* pCtrl)
{
	const int kSize = kNumColorLevels * sizeof(UINT);

	memcpy(m_count[0], pCtrl->m_count[0], kSize);
	memcpy(m_count[1], pCtrl->m_count[1], kSize);
	memcpy(m_count[2], pCtrl->m_count[2], kSize);
	memcpy(m_count[3], pCtrl->m_count[3], kSize);
	memcpy(m_lumCount, pCtrl->m_lumCount, kSize);
	memcpy(m_maxCount, pCtrl->m_maxCount, kNumChannels * sizeof(UINT));
	memcpy(m_mean, pCtrl->m_mean, kNumChannels * sizeof(float));
	memcpy(m_stdDev, pCtrl->m_stdDev, kNumChannels * sizeof(float));
	memcpy(m_median, pCtrl->m_median, kNumChannels * sizeof(float));
	m_maxLumCount = pCtrl->m_maxLumCount;
	m_meanAvg = pCtrl->m_meanAvg;
	m_stdDevAvg = pCtrl->m_stdDevAvg;
	m_medianAvg = pCtrl->m_medianAvg;
	m_imageFormat = pCtrl->m_imageFormat;
}

void CImageHistogramCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	HDC hDC = ::GetDC(NULL);

	if (!m_canvas.GetDC().GetSafeHdc())
	{
		m_canvas.Create(hDC, cx, cy);
	}
	else
	{
		m_canvas.Resize(hDC, cx, cy);
	}

	::ReleaseDC(NULL, hDC);
}

BOOL CImageHistogramCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

void CImageHistogramCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	CPoint pt;
	CMenu mnu;

	GetCursorPos(&pt);
	mnu.CreatePopupMenu();
	mnu.AppendMenu(MF_BYPOSITION, 1, "Luminosity");
	mnu.AppendMenu(MF_BYPOSITION, 2, "Overlapped RGBA");
	mnu.AppendMenu(MF_BYPOSITION, 3, "Split RGB");
	mnu.AppendMenu(MF_BYPOSITION, 4, "Red Channel");
	mnu.AppendMenu(MF_BYPOSITION, 5, "Green Channel");
	mnu.AppendMenu(MF_BYPOSITION, 6, "Blue Channel");
	mnu.AppendMenu(MF_BYPOSITION, 7, "Alpha Channel");

	int index = mnu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, this);

	switch (index)
	{
	case 1:
		m_drawMode = eDrawMode_Luminosity;
		break;
	case 2:
		m_drawMode = eDrawMode_OverlappedRGB;
		break;
	case 3:
		m_drawMode = eDrawMode_SplitRGB;
		break;
	case 4:
		m_drawMode = eDrawMode_RedChannel;
		break;
	case 5:
		m_drawMode = eDrawMode_GreenChannel;
		break;
	case 6:
		m_drawMode = eDrawMode_BlueChannel;
		break;
	case 7:
		m_drawMode = eDrawMode_AlphaChannel;
		break;
	}

	mnu.DestroyMenu();
	RedrawWindow();

	CWnd::OnLButtonDown(nFlags, point);
}

void CImageHistogramCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	CWnd::OnMouseMove(nFlags, point);
}

