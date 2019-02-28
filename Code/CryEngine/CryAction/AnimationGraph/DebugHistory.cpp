// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DebugHistory.h"
//#include "PersistantDebug.h"

std::set<CDebugHistoryManager*>* CDebugHistoryManager::m_allhistory;

static int g_currentlyVisibleCount = 0;

//--------------------------------------------------------------------------------

void Draw2DLine(float x1, float y1, float x2, float y2, ColorF color, float fThickness)
{
	IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom();

	x1 /= gEnv->pRenderer->GetOverlayWidth();
	y1 /= gEnv->pRenderer->GetOverlayHeight();
	x2 /= gEnv->pRenderer->GetOverlayWidth();
	y2 /= gEnv->pRenderer->GetOverlayHeight();

	ColorB rgba((uint8)(color.r * 255.0f), (uint8)(color.g * 255.0f), (uint8)(color.b * 255.0f), (uint8)(color.a * 255.0f));
	pAux->DrawLine(Vec3(x1, y1, 0), rgba, Vec3(x2, y2, 0), rgba, fThickness);
}

//--------------------------------------------------------------------------------

CDebugHistory::CDebugHistory(const char* name, int size)
	: m_szName(name)
	, m_show(false)
	, m_layoutTopLeft(Vec2(100.0f, 100.0f))
	, m_layoutExtent(Vec2(100.0f, 100.0f))
	, m_layoutMargin(0.0f)
	, m_scopeOuterMax(0.0f)
	, m_scopeOuterMin(0.0f)
	, m_scopeInnerMax(0.0f)
	, m_scopeInnerMin(0.0f)
	, m_scopeCurMax(0.0f)
	, m_scopeCurMin(0.0f)
	, m_colorCurveNormal(ColorF(1, 1, 1, 1))
	, m_colorCurveClamped(ColorF(1, 1, 1, 1))
	, m_colorBox(ColorF(0.2f, 0.2f, 0.2f, 0.2f))
	, m_colorGridLine(ColorF(0.0f, 0.0f, 1.0f, 0.2f))
	, m_colorGridNumber(ColorF(1.0f, 1.0f, 1.0f, 1.0f))
	, m_colorName(ColorF(1, 1, 0, 1.0f))
	, m_wantedGridLineCountX(4)
	, m_wantedGridLineCountY(3)
	, m_gridLineCount(0)
	, m_pValues(nullptr)
	, m_maxCount(size)
	, m_head(0)
	, m_count(0)
	, m_scopeRefreshDelay(0)
	, m_gridRefreshDelay(0)
	, m_hasDefaultValue(false)
	, m_defaultValue(0.0f)
	, m_gotValue(false)
{
	SetVisibility(false);

	for (int i = 0; i < GRIDLINE_MAXCOUNT; ++i)
	{
		m_gridLine[i] = 0.0f;
	}
}

//--------------------------------------------------------------------------------

CDebugHistory::~CDebugHistory()
{
	SetVisibility(false);
}

//--------------------------------------------------------------------------------

void CDebugHistory::SetName(const char* newName)
{
	m_szName = newName;
}

//--------------------------------------------------------------------------------

void CDebugHistory::SetVisibility(bool show)
{
	if (m_show != show)
	{
		if (show)
			g_currentlyVisibleCount++;
		else
			g_currentlyVisibleCount--;
	}

	m_show = show;

	if (show)
	{
		if (m_pValues == NULL)
		{
			m_pValues = new float[m_maxCount];
			ClearHistory();
		}
	}
	else
	{
		if (m_pValues != NULL)
		{
			delete[] m_pValues;
			m_pValues = NULL;
		}
	}
}

//--------------------------------------------------------------------------------

void CDebugHistory::SetupLayoutAbs(float leftx, float topy, float width, float height, float margin)
{
	//float RefWidth = 800.0f;
	//float RefHeight = 600.0f;
	float RefWidth = 1024.0f;
	float RefHeight = 768.0f;
	SetupLayoutRel(leftx / RefWidth, topy / RefHeight, width / RefWidth, height / RefHeight, margin / RefHeight);
}

//--------------------------------------------------------------------------------

void CDebugHistory::SetupLayoutRel(float leftx, float topy, float width, float height, float margin)
{
	m_layoutTopLeft.x = leftx  * gEnv->pRenderer->GetOverlayWidth();
	m_layoutTopLeft.y = topy   * gEnv->pRenderer->GetOverlayHeight();
	m_layoutExtent.x  = width  * gEnv->pRenderer->GetOverlayWidth();
	m_layoutExtent.y  = height * gEnv->pRenderer->GetOverlayHeight();
	m_layoutMargin    = margin * gEnv->pRenderer->GetOverlayHeight();
}

//--------------------------------------------------------------------------------

void CDebugHistory::SetupScopeExtent(float outermin, float outermax, float innermin, float innermax)
{
	m_scopeOuterMax = outermax;
	m_scopeOuterMin = outermin;
	m_scopeInnerMax = innermax;
	m_scopeInnerMin = innermin;
	m_scopeCurMax = m_scopeInnerMax;
	m_scopeCurMin = m_scopeInnerMin;
}

//--------------------------------------------------------------------------------

void CDebugHistory::SetupScopeExtent(float outermin, float outermax)
{
	m_scopeOuterMax = outermax;
	m_scopeOuterMin = outermin;
	m_scopeInnerMax = outermin;
	m_scopeInnerMin = outermax;
	m_scopeCurMax = m_scopeInnerMax;
	m_scopeCurMin = m_scopeInnerMin;
}

//--------------------------------------------------------------------------------

void CDebugHistory::SetupColors(ColorF curvenormal, ColorF curveclamped, ColorF box, ColorF gridline, ColorF gridnumber, ColorF name)
{
	m_colorCurveNormal = curvenormal;
	m_colorCurveClamped = curveclamped;
	m_colorBox = box;
	m_colorGridLine = gridline;
	m_colorGridNumber = gridnumber;
	m_colorName = name;
}
void CDebugHistory::SetGridlineCount(int nGridlinesX, int nGridlinesY)
{
	m_wantedGridLineCountX = nGridlinesX + 1;
	m_wantedGridLineCountY = nGridlinesY + 1;
}

//--------------------------------------------------------------------------------

void CDebugHistory::ClearHistory()
{
	m_head = 0;
	m_count = 0;
	m_scopeCurMax = m_scopeInnerMax;
	m_scopeCurMin = m_scopeInnerMin;
}

//--------------------------------------------------------------------------------

void CDebugHistory::AddValue(float value)
{
	if (m_pValues == NULL)
		return;

	// Store value in history
	m_head = (m_head + 1) % m_maxCount;
	m_pValues[m_head] = value;
	if (m_count < m_maxCount)
		m_count++;

	if (m_scopeRefreshDelay == 0)
	{
		UpdateExtent();

		static int delay = 1;
		m_scopeRefreshDelay = delay;
	}
	else
	{
		m_scopeRefreshDelay--;
	}

	if (m_gridRefreshDelay == 0)
	{
		UpdateGridLines();
		static int delay = 5;
		m_gridRefreshDelay = delay;
	}
	else
	{
		m_gridRefreshDelay--;
	}
}

//--------------------------------------------------------------------------------

void CDebugHistory::UpdateExtent()
{
	m_scopeCurMax = m_scopeInnerMax;
	m_scopeCurMin = m_scopeInnerMin;

	for (int i = 0; i < m_count; ++i)
	{
		int j = (m_head - i + m_maxCount) % m_maxCount;
		float value = m_pValues[j];
		if (value < m_scopeCurMin)
		{
			m_scopeCurMin = value;
			if (m_scopeCurMin < m_scopeOuterMin)
			{
				m_scopeCurMin = m_scopeOuterMin;
			}
		}
		if (value > m_scopeCurMax)
		{
			m_scopeCurMax = value;
			if (m_scopeCurMax > m_scopeOuterMax)
			{
				m_scopeCurMax = m_scopeOuterMax;
			}
		}
	}
	if (abs(m_scopeCurMax - m_scopeCurMin) < 0.0005f)
	{
		float scopeCurMid = 0.5f * (m_scopeCurMin + m_scopeCurMax);
		m_scopeCurMin = max(scopeCurMid - 0.001f, m_scopeOuterMin);
		m_scopeCurMax = min(scopeCurMid + 0.001f, m_scopeOuterMax);
	}
}

//--------------------------------------------------------------------------------

void CDebugHistory::UpdateGridLines()
{
	if ((m_colorGridLine.a == 0.0f) || (m_colorGridNumber.a == 0.0f))
		return;

	float scopeCurSpan = (m_scopeCurMax - m_scopeCurMin);
	if (scopeCurSpan == 0.0f)
		return;

	m_gridLineCount = 2;
	m_gridLine[0] = m_scopeCurMin;
	m_gridLine[1] = m_scopeCurMax;

	const uint8 HISTOGRAM_SLOTS = 50;
	uint16 histogramCount[HISTOGRAM_SLOTS];
	float histogramSum[HISTOGRAM_SLOTS];

	for (int s = 0; s < HISTOGRAM_SLOTS; ++s)
	{
		histogramCount[s] = 0;
		histogramSum[s] = 0.0f;
	}
	for (int i = 0; i < m_count; ++i)
	{
		int j = (m_head - i + m_maxCount) % m_maxCount;
		float v = m_pValues[j];
		int s = (int)((float)(HISTOGRAM_SLOTS - 1) * ((v - m_scopeCurMin) / scopeCurSpan));
		s = max(0, min(HISTOGRAM_SLOTS - 1, s));
		histogramSum[s] += v;
		histogramCount[s]++;
	}

	for (int s = 0; s < HISTOGRAM_SLOTS; ++s)
	{
		float count = (float)histogramCount[s];
		if (count > 0.0f)
			histogramSum[s] /= count;
		else
			histogramSum[s] = 0.0f;
	}

	static int minThresholdMul = 1;
	static int minThresholdDiv = 4;
	int minThreshold = ((m_maxCount / (GRIDLINE_MAXCOUNT - 2)) * minThresholdMul) / minThresholdDiv;
	for (int i = 0; i < GRIDLINE_MAXCOUNT; ++i)
	{
		int highest = -1;
		for (int s = 0; s < HISTOGRAM_SLOTS; ++s)
		{
			if (((highest == -1) || (histogramCount[s] > histogramCount[highest])) &&
			    (histogramCount[s] > minThreshold))
			{
				bool uniqueEnough = true;
				static float minSpacing = 1.0f / 8.0f; // percent of whole extent, TODO: should be based on font size versus layout height
				float deltaThreshold = minSpacing * (m_scopeCurMax - m_scopeCurMin);
				for (int j = 0; j < m_gridLineCount; ++j)
				{
					float delta = abs(histogramSum[s] - m_gridLine[j]);
					if (delta < deltaThreshold)
					{
						uniqueEnough = false;
						break;
					}
				}
				if (uniqueEnough)
				{
					highest = s;
				}
			}
		}

		if (highest != -1)
		{
			histogramCount[highest] = 0;
			m_gridLine[m_gridLineCount] = histogramSum[highest];
			m_gridLineCount++;
		}
	}

	m_gotValue = true;
}

//--------------------------------------------------------------------------------

void CDebugHistory::Render()
{
	if (!m_show)
		return;

	if (!m_gotValue && m_hasDefaultValue)
		AddValue(m_defaultValue);
	m_gotValue = false;

	if (m_colorBox.a > 0.0f)
	{
		float x1 = m_layoutTopLeft.x;
		float y1 = m_layoutTopLeft.y;
		float x2 = m_layoutTopLeft.x + m_layoutExtent.x;
		float y2 = m_layoutTopLeft.y + m_layoutExtent.y;

		Draw2DLine(x1, y1, x2, y1, m_colorBox);
		Draw2DLine(x1, y2, x2, y2, m_colorBox);
		Draw2DLine(x1, y1, x1, y2, m_colorBox);
		Draw2DLine(x2, y1, x2, y2, m_colorBox);
	}

	float scopeExtent = (m_scopeCurMax - m_scopeCurMin);

	if (m_colorGridLine.a > 0.0f)
	{
		float x1 = m_layoutTopLeft.x;
		float y1 = m_layoutTopLeft.y + m_layoutMargin;
		float x2 = m_layoutTopLeft.x + m_layoutExtent.x;
		float y2 = m_layoutTopLeft.y + m_layoutExtent.y - m_layoutMargin * 2.0f;

		for (int i = 0; i < m_gridLineCount; i++)
		{
			float y = m_gridLine[i];
			float scopefractiony = (scopeExtent != 0.0f) ? (y - m_scopeCurMin) / scopeExtent : 0.5f;
			float screeny = LERP(y2, y1, scopefractiony);
			Draw2DLine(x1, screeny, x2, screeny, m_colorGridLine);
		}

		for (int i = 1; i < m_wantedGridLineCountX; i++)
		{
			float scopefractionx = (float)i / (float)m_wantedGridLineCountX;
			float screenx = LERP(x2, x1, scopefractionx);
			Draw2DLine(screenx, y1, screenx, y2, m_colorGridLine);
		}
		for (int i = 1; i < m_wantedGridLineCountY; i++)
		{
			float scopefractiony = (float)i / (float)m_wantedGridLineCountY;
			float screeny = LERP(y2, y1, scopefractiony);
			Draw2DLine(x1, screeny, x2, screeny, m_colorGridLine);
		}
	}

	if (m_colorGridNumber.a > 0.0f)
	{
		float x1 = m_layoutTopLeft.x;
		float y1 = m_layoutTopLeft.y + m_layoutMargin;
		//		float x2 = m_layoutTopLeft.x + m_layoutExtent.x;
		float y2 = m_layoutTopLeft.y + m_layoutExtent.y - m_layoutMargin * 2.0f;

		char* gridNumberPrecision = "%f";
		if (scopeExtent >= 100.0f)
		{
			gridNumberPrecision = "%.0f";
		}
		else if (scopeExtent >= 10.0f)
		{
			gridNumberPrecision = "%.1f";
		}
		else if (scopeExtent >= 1.0f)
		{
			gridNumberPrecision = "%.2f";
		}
		else if (scopeExtent > 0.1f)
		{
			gridNumberPrecision = "%.3f";
		}
		else if (scopeExtent < 0.1f)
		{
			gridNumberPrecision = "%.4f";
		}

		for (int i = 0; i < m_gridLineCount; i++)
		{
			static float offsety = -7; // should be based on font size
			static float offsetx = 30; // should be based on font size
			float y = m_gridLine[i];
			float scopefractiony = (scopeExtent != 0.0f) ? (y - m_scopeCurMin) / scopeExtent : 0.5f;
			static float slots = 8.0f; // TODO: should be based on font size versus layout height
			float x = x1 + offsetx * (float)((uint8)(scopefractiony * (slots - 1.0f)) & 1);
			float screeny = LERP(y2, y1, scopefractiony);

			CryFixedStringT<32> label;
			label.Format(gridNumberPrecision, y);

			IRenderAuxText::DrawText(Vec3(x, screeny + offsety, 0.5f), 1.4f, m_colorGridNumber, eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_IgnoreOverscan, label.c_str());
		}
	}

	if (m_colorCurveNormal.a > 0.0f)
	{
		float x1 = m_layoutTopLeft.x;
		float y1 = m_layoutTopLeft.y + m_layoutMargin;
		float x2 = m_layoutTopLeft.x + m_layoutExtent.x;
		float y2 = m_layoutTopLeft.y + m_layoutExtent.y - m_layoutMargin * 2.0f;

		float prevscreenx = 0.0f;
		float prevscreeny = 0.0f;

		for (int i = 0; i < m_count; i++)
		{
			int j = (m_head - i + m_maxCount) % m_maxCount;
			float y = m_pValues[j];
			float scopefractiony = (scopeExtent != 0.0f) ? (y - m_scopeCurMin) / scopeExtent : 0.5f;
			float screeny = LERP(y2, y1, scopefractiony);
			float scopefractionx = (float)i / (float)m_maxCount;
			float screenx = LERP(x2, x1, scopefractionx);

			if (i > 0)
			{
				Draw2DLine(screenx, screeny, prevscreenx, prevscreeny, m_colorCurveNormal);
			}

			prevscreenx = screenx;
			prevscreeny = screeny;
		}
	}

	if (m_colorName.a > 0.0f)
	{
		const int offsety = -12;

		IRenderAuxText::DrawText(Vec3(m_layoutTopLeft.x + 0.5f * m_layoutExtent.x, m_layoutTopLeft.y + offsety, 0.5f), 1.2f, m_colorName, eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Center | eDrawText_IgnoreOverscan, m_szName);
	}
}

//--------------------------------------------------------------------------------
IDebugHistory* CDebugHistoryManager::CreateHistory(const char* id, const char* name)
{
	if (!name)
		name = id;

	MapIterator it = m_histories.find(CONST_TEMP_STRING(id));
	CDebugHistory* history = (it != m_histories.end()) ? (*it).second : NULL;
	if (history == NULL)
	{
		history = new CDebugHistory(name, 100);
		m_histories.insert(std::make_pair(id, history));
	}

	return history;
}

//--------------------------------------------------------------------------------

void CDebugHistoryManager::RemoveHistory(const char* id)
{
	MapIterator it = m_histories.find(CONST_TEMP_STRING(id));
	if (it != m_histories.end())
	{
		delete it->second;
		m_histories.erase(it);
	}
}

//--------------------------------------------------------------------------------

IDebugHistory* CDebugHistoryManager::GetHistory(const char* id)
{
	MapIterator it = m_histories.find(CONST_TEMP_STRING(id));
	CDebugHistory* history = (it != m_histories.end()) ? (*it).second : NULL;
	return history;
}

//--------------------------------------------------------------------------------

void CDebugHistoryManager::Render(bool bSetupRenderer)
{
	if (m_histories.empty())
		return;

	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (bSetupRenderer)
		CDebugHistoryManager::SetupRenderer();

	MapIterator it = m_histories.begin();
	while (it != m_histories.end())
	{
		CDebugHistory* history = (*it).second;
		history->Render();

		++it;
	}
}

//--------------------------------------------------------------------------------

void CDebugHistoryManager::Release()
{
	delete this;
}

//--------------------------------------------------------------------------------

void CDebugHistoryManager::GetMemoryUsage(ICrySizer* s) const
{
	s->AddContainer(m_histories);
	s->Add(*this);
}

//--------------------------------------------------------------------------------

void CDebugHistoryManager::SetupRenderer()
{
	const int screenw = gEnv->pRenderer->GetOverlayWidth();
	const int screenh = gEnv->pRenderer->GetOverlayHeight();

	IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
	flags.SetMode2D3DFlag(e_Mode2D);
	pAux->SetRenderFlags(flags);
}

//--------------------------------------------------------------------------------

void CDebugHistoryManager::RenderAll()
{
	if (g_currentlyVisibleCount == 0)
		return;

	if (m_allhistory)
	{
		SetupRenderer();
		for (std::set<CDebugHistoryManager*>::iterator iter = m_allhistory->begin(); iter != m_allhistory->end(); ++iter)
			(*iter)->Render(false);
	}
}

//--------------------------------------------------------------------------------

void CDebugHistoryManager::LayoutHelper(const char* id, const char* name, bool visible, float minout, float maxout, float minin, float maxin, float x, float y, float w /*=1.0f*/, float h /*=1.0f*/)
{
	if (!visible)
	{
		this->RemoveHistory(id);
		return;
	}

	IDebugHistory* pDH = this->GetHistory(id);
	if (pDH != NULL)
		return;

	if (name == NULL)
		name = id;

	pDH = this->CreateHistory(id, name);
	pDH->SetupScopeExtent(minout, maxout, minin, maxin);
	pDH->SetVisibility(true);

	static float x0 = 0.01f;
	static float y0 = 0.05f;
	static float x1 = 0.99f;
	static float y1 = 1.00f;
	static float dx = x1 - x0;
	static float dy = y1 - y0;
	static float xtiles = 6.0f;
	static float ytiles = 4.0f;
	pDH->SetupLayoutRel(x0 + dx * x / xtiles,
	                    y0 + dy * y / ytiles,
	                    dx * w * 0.95f / xtiles,
	                    dy * h * 0.93f / ytiles,
	                    dy * 0.02f / ytiles);
}

//--------------------------------------------------------------------------------
