// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainTextureExport.h"
#include "Terrain/TerrainTexGen.h"
#include "Qt/Widgets/QWaitProgress.h"
#include "CryEditDoc.h"
#include "ResizeResolutionDialog.h"
#include "Viewport.h"
#include "Terrain/TerrainManager.h"
#include "TerrainLighting.h"
#include "Controls/QuestionDialog.h"
#include "Util/FileUtil.h"

#include <EditorFramework/PersonalizationManager.h>
#include <FileDialogs/SystemFileDialog.h>
#include <FilePathUtil.h>
#include <QtUtil.h>

#define TERRAIN_PREVIEW_RESOLUTION 256
const char* szFileProperty = "File";

/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureExport dialog

//CDC CTerrainTextureExport::m_dcLightmap;

CTerrainTextureExport::CTerrainTextureExport(CWnd* pParent /*=NULL*/)
	: CDialog(CTerrainTextureExport::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTerrainTextureExport)
	//}}AFX_DATA_INIT

	m_cx = 19;
	m_cy = 24;

	m_bSelectMode = false;
	rcSel.left = rcSel.top = rcSel.right = rcSel.bottom = -1;

	m_pTexGen = new CTerrainTexGen;
	m_pTexGen->Init(TERRAIN_PREVIEW_RESOLUTION, true);
}

CTerrainTextureExport::~CTerrainTextureExport()
{
	delete m_pTexGen;
}

void CTerrainTextureExport::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainTextureExport)
	DDX_Control(pDX, IDC_IMPORT, m_importBtn);
	DDX_Control(pDX, IDC_EXPORT, m_exportBtn);
	DDX_Control(pDX, IDCANCEL, m_closeBtn);
	DDX_Control(pDX, IDC_CHANGE, m_changeResolutionBtn);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTerrainTextureExport, CDialog)
//{{AFX_MSG_MAP(CTerrainTextureExport)
ON_WM_PAINT()
ON_COMMAND(IDC_EXPORT, OnExport)
ON_COMMAND(IDC_IMPORT, OnImport)
ON_COMMAND(IDC_CHANGE, OnChangeResolutionBtn)
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureExport message handlers

//////////////////////////////////////////////////////////////////////////
BOOL CTerrainTextureExport::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_terrain.Allocate(TERRAIN_PREVIEW_RESOLUTION, TERRAIN_PREVIEW_RESOLUTION);

	string filePath(GET_PERSONALIZATION_PROPERTY(CTerrainTextureExport, szFileProperty).toString().toLocal8Bit());
	GetDlgItem(IDC_FILE)->SetWindowText(filePath);

	CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();
	string maskInfoText;
	maskInfoText.Format("RGB(%dx%d)",
	                    pRGBLayer->CalcMinRequiredTextureExtend(),
	                    pRGBLayer->CalcMinRequiredTextureExtend());
	GetDlgItem(IDC_INFO_TEXT)->SetWindowText(maskInfoText);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CTerrainTextureExport::OnPaint()
{
	CPaintDC dc(this);
	DrawPreview(dc);
}

void CTerrainTextureExport::PreviewToTile(uint32& outX, uint32& outY, uint32 x, uint32 y)
{
	uint32 dwTileCountY = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer()->GetTileCountY();

	// rotate 90 deg
	outX = dwTileCountY - 1 - y;
	outY = x;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::DrawPreview(CDC& dc)
{
	//CPaintDC dc(this); // device context for painting
	CPen cGrayPen(PS_SOLID, 1, 0x007F7F7F);
	CPen cRedPen(PS_SOLID, 1, 0x004040ff);
	CPen cGreenPen(PS_SOLID, 1, 0x0000ff00);
	CBrush brush(0x00808080);

	CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();

	uint32 dwTileCountX = pRGBLayer->GetTileCountX();
	uint32 dwTileCountY = pRGBLayer->GetTileCountY();
	if (dwTileCountX == 0 || dwTileCountY == 0)
		return;

	// Generate a preview if we don't have one
	if (!m_dcLightmap.m_hDC)
		OnGenerate();

	// Draw the preview
	dc.BitBlt(m_cx, m_cy, TERRAIN_PREVIEW_RESOLUTION, TERRAIN_PREVIEW_RESOLUTION, &m_dcLightmap, 0, 0, SRCCOPY);

	CPen* prevPen = dc.SelectObject(&cGrayPen);
	{
		dc.SetBkMode(TRANSPARENT);
		for (int x = 0; x < dwTileCountX; x++)
			for (int y = 0; y < dwTileCountY; y++)
			{
				RECT rc = {
					m_cx + x * TERRAIN_PREVIEW_RESOLUTION / dwTileCountX,       m_cy + y * TERRAIN_PREVIEW_RESOLUTION / dwTileCountY,
					m_cx + (x + 1) * TERRAIN_PREVIEW_RESOLUTION / dwTileCountX, m_cy + (y + 1) * TERRAIN_PREVIEW_RESOLUTION / dwTileCountY
				};

				uint32 dwTileX;
				uint32 dwTileY;
				PreviewToTile(dwTileX, dwTileY, x, y);
				uint32 dwLocalSize = pRGBLayer->GetTileResolution(dwTileX, dwTileY);
				dc.SetTextColor(RGB(SATURATEB(dwLocalSize / 8), 0, 0));

				if (rcSel.left <= x && rcSel.right >= x && rcSel.top <= y && rcSel.bottom >= y)
				{
					RECT rc = {
						m_cx + x * TERRAIN_PREVIEW_RESOLUTION / dwTileCountX + 4,       m_cy + y * TERRAIN_PREVIEW_RESOLUTION / dwTileCountY + 4,
						m_cx + (x + 1) * TERRAIN_PREVIEW_RESOLUTION / dwTileCountX - 4, m_cy + (y + 1) * TERRAIN_PREVIEW_RESOLUTION / dwTileCountY - 4
					};
					dc.FillRect(&rc, &brush);
				}

				char str[32];
				if (dwLocalSize == 1024)
					cry_sprintf(str, "1k");
				else if (dwLocalSize == 2048)
					cry_sprintf(str, "2k");
				else if (dwLocalSize == 4096)
					cry_sprintf(str, "4k");
				else
					cry_sprintf(str, "%d", dwLocalSize);
				dc.DrawText(str, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
			}
	}

	dc.SelectObject(&cGreenPen);
	for (int x = 0; x <= dwTileCountX; x++)
	{
		dc.MoveTo(m_cx + x * TERRAIN_PREVIEW_RESOLUTION / dwTileCountX, m_cy);
		dc.LineTo(m_cx + x * TERRAIN_PREVIEW_RESOLUTION / dwTileCountX, m_cy + TERRAIN_PREVIEW_RESOLUTION);
	}
	for (int y = 0; y <= dwTileCountY; y++)
	{
		dc.MoveTo(m_cx, m_cy + y * TERRAIN_PREVIEW_RESOLUTION / dwTileCountY);
		dc.LineTo(m_cx + TERRAIN_PREVIEW_RESOLUTION, m_cy + y * TERRAIN_PREVIEW_RESOLUTION / dwTileCountY);
	}

	if (rcSel.top >= 0)
	{
		RECT rc = {
			m_cx + rcSel.left * TERRAIN_PREVIEW_RESOLUTION / dwTileCountX + 1,        m_cy + rcSel.top * TERRAIN_PREVIEW_RESOLUTION / dwTileCountY + 1,
			m_cx + (rcSel.right + 1) * TERRAIN_PREVIEW_RESOLUTION / dwTileCountX - 1, m_cy + (rcSel.bottom + 1) * TERRAIN_PREVIEW_RESOLUTION / dwTileCountY - 1
		};

		dc.SelectObject(&cRedPen);

		dc.MoveTo(rc.left, rc.top);
		dc.LineTo(rc.right, rc.top);
		dc.LineTo(rc.right, rc.bottom);
		dc.LineTo(rc.left, rc.bottom);
		dc.LineTo(rc.left, rc.top);
	}

	dc.SelectObject(prevPen);
}

void CTerrainTextureExport::OnGenerate()
{
	RECT rcUpdate;

	BeginWaitCursor();

	LightingSettings* ls = GetIEditorImpl()->GetDocument()->GetLighting();

	int skyQuality = ls->iHemiSamplQuality;
	ls->iHemiSamplQuality = 0;

	// Create a DC and a bitmap
	if (!m_dcLightmap.m_hDC)
		VERIFY(m_dcLightmap.CreateCompatibleDC(NULL));
	if (!m_bmpLightmap.m_hObject)
		VERIFY(m_bmpLightmap.CreateBitmap(TERRAIN_PREVIEW_RESOLUTION, TERRAIN_PREVIEW_RESOLUTION, 1, 32, NULL));
	m_dcLightmap.SelectObject(&m_bmpLightmap);

	// Calculate the lighting.
	CImageEx terrainImgTmp;
	terrainImgTmp.Allocate(TERRAIN_PREVIEW_RESOLUTION, TERRAIN_PREVIEW_RESOLUTION);
	m_pTexGen->InvalidateLighting();
	m_pTexGen->GenerateSurfaceTexture(ETTG_LIGHTING | ETTG_QUIET | ETTG_ABGR | ETTG_BAKELIGHTING | ETTG_NOTEXTURE | ETTG_SHOW_WATER, terrainImgTmp);

	m_terrain.RotateOrt(terrainImgTmp, 1);

	// put m_terrain into m_bmpLightmap
	{
		BITMAPINFO bi;

		ZeroStruct(bi);
		bi.bmiHeader.biSize = sizeof(bi);
		bi.bmiHeader.biWidth = m_terrain.GetWidth();
		bi.bmiHeader.biHeight = -m_terrain.GetHeight();
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biCompression = BI_RGB;

		SetDIBits(m_dcLightmap.m_hDC, (HBITMAP)m_bmpLightmap.GetSafeHandle(), 0, m_terrain.GetHeight(), m_terrain.GetData(), &bi, false);
	}

	// Update the preview
	::SetRect(&rcUpdate, 10, 10, 20 + TERRAIN_PREVIEW_RESOLUTION, 30 + TERRAIN_PREVIEW_RESOLUTION);
	InvalidateRect(&rcUpdate, FALSE);
	UpdateWindow();
	ls->iHemiSamplQuality = skyQuality;

	EndWaitCursor();
}

//////////////////////////////////////////////////////////////////////////
string CTerrainTextureExport::BrowseTerrainTexture(bool bIsSave)
{
	string filePath(GET_PERSONALIZATION_PROPERTY(CTerrainTextureExport, szFileProperty).toString().toLocal8Bit());
	if (filePath.empty())
		filePath = CFileUtil::FormatInitialFolderForFileDialog(PathUtil::GamePathToCryPakPath("", true).c_str()) + "terraintex.bmp";

	CSystemFileDialog::RunParams runParams;
	runParams.extensionFilters << CExtensionFilter("Bitmap Image File (*.bmp)", "bmp");
	runParams.initialFile = QString::fromLocal8Bit(filePath.c_str());

	if (bIsSave)
	{
		QString file = CSystemFileDialog::RunExportFile(runParams, nullptr);
		if (!file.isEmpty())
			return file.toLocal8Bit().data();
	}
	else
	{
		QString file = CSystemFileDialog::RunImportFile(runParams, nullptr);
		if (!file.isEmpty())
			return file.toLocal8Bit().data();
	}

	return "";
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::ImportExport(bool bIsImport, bool bIsClipboard)
{
	string filePath;
	if (!bIsClipboard)
	{
		filePath = BrowseTerrainTexture(!bIsImport);
		SET_PERSONALIZATION_PROPERTY(CTerrainTextureExport, szFileProperty, filePath.c_str());
		if (filePath.empty())
			return;

		GetDlgItem(IDC_FILE)->SetWindowText(filePath);
	}

	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();

	uint32 dwTileCountX = pRGBLayer->GetTileCountX();
	uint32 dwTileCountY = pRGBLayer->GetTileCountY();

	uint32 left;
	uint32 top;
	uint32 right;
	uint32 bottom;

	PreviewToTile(right, top, rcSel.left, rcSel.top);
	PreviewToTile(left, bottom, rcSel.right, rcSel.bottom);

	uint32 square;
	pRGBLayer->ImportExportBlock(bIsClipboard ? 0 : filePath, left, top, right + 1, bottom + 1, &square, bIsImport);

	if (bIsImport)
	{
		if (square > 2048 * 2048)
		{
			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Save a level and generate terrain texture to see the result."));
		}
		else
		{
			RECT rc = {
				left* pHeightmap->GetWidth() / dwTileCountX,         top * pHeightmap->GetHeight() / dwTileCountY,
				(right + 1) * pHeightmap->GetWidth() / dwTileCountX, (bottom + 1) * pHeightmap->GetHeight() / dwTileCountY
			};
			pHeightmap->UpdateLayerTexture(rc);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnExport()
{
	if (rcSel.top < 0)
		return;

	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	if (!pHeightmap->IsAllocated())
		return;

	ImportExport(false /*bIsImport*/);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnImport()
{
	if (rcSel.top < 0)
		return;

	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	if (!pHeightmap->IsAllocated())
		return;

	ImportExport(true /*bIsImport*/);

	GetIEditorImpl()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnLButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonDown(nFlags, point);

	CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();

	uint32 dwTileCountX = pRGBLayer->GetTileCountX();
	uint32 dwTileCountY = pRGBLayer->GetTileCountY();
	if (dwTileCountX == 0 || dwTileCountY == 0)
		return;

	int x = (point.x - m_cx) * dwTileCountX / TERRAIN_PREVIEW_RESOLUTION;
	int y = (point.y - m_cy) * dwTileCountY / TERRAIN_PREVIEW_RESOLUTION;

	if (x >= 0 && y >= 0 && x < dwTileCountX && y < dwTileCountY)
	{
		if (rcSel.left == x && rcSel.top == y && rcSel.right == x && rcSel.bottom == y)
		{
			rcSel.left = rcSel.top = rcSel.right = rcSel.bottom = -1;
		}
		else
		{
			rcSel.left = x;
			rcSel.top = y;
			rcSel.right = x;
			rcSel.bottom = y;
			m_bSelectMode = true;
			SetCapture();
		}
	}

	DrawPreview(*(GetDC()));
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bSelectMode)
	{
		ReleaseCapture();
		m_bSelectMode = false;
	}
	CDialog::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnMouseMove(UINT nFlags, CPoint point)
{
	CDialog::OnMouseMove(nFlags, point);

	if (!m_bSelectMode)
		return;

	if (nFlags & MK_LBUTTON)
	{
		CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();

		uint32 dwTileCountX = pRGBLayer->GetTileCountX();
		uint32 dwTileCountY = pRGBLayer->GetTileCountY();

		int x = (point.x - m_cx) * dwTileCountX / TERRAIN_PREVIEW_RESOLUTION;
		int y = (point.y - m_cy) * dwTileCountY / TERRAIN_PREVIEW_RESOLUTION;

		if (x >= 0 && y >= 0 && x >= rcSel.left && y >= rcSel.top && x < dwTileCountX && y < dwTileCountY
		    && (x != rcSel.right || y != rcSel.bottom))
		{
			rcSel.right = x;
			rcSel.bottom = y;
			DrawPreview(*(GetDC()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnChangeResolutionBtn()
{
	if (rcSel.left == -1 || rcSel.top == -1)
	{
		CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("Error: You have to select a sector."));
		return;
	}

	uint32 dwTileX;
	uint32 dwTileY;
	PreviewToTile(dwTileX, dwTileY, rcSel.left, rcSel.top);

	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();

	CResizeResolutionDialog dlg;
	uint32 dwCurSize = pHeightmap->GetRGBLayer()->GetTileResolution(dwTileX, dwTileY);
	dlg.SetSize(dwCurSize);
	if (dlg.DoModal() != IDOK)
		return;

	uint32 dwNewSize = dlg.GetSize();

	for (uint32 y = rcSel.top; y <= rcSel.bottom; ++y)
		for (uint32 x = rcSel.left; x <= rcSel.right; ++x)
		{
			PreviewToTile(dwTileX, dwTileY, x, y);

			uint32 dwOldSize = pHeightmap->GetRGBLayer()->GetTileResolution(dwTileX, dwTileY);
			if (dwOldSize == dwNewSize || dwNewSize < 64 || dwNewSize > 2048)
				continue;

			GetIEditorImpl()->GetTerrainManager()->GetHeightmap()->GetRGBLayer()->ChangeTileResolution(dwTileX, dwTileY, dwNewSize);
			GetIEditorImpl()->GetDocument()->SetModifiedFlag(TRUE);

			// update terrain preview image in the dialog
			DrawPreview(*(GetDC()));

			// update 3D Engine display
			I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();

			int nTerrainSize = p3DEngine->GetTerrainSize();
			int nTexSectorSize = p3DEngine->GetTerrainTextureNodeSizeMeters();
			uint32 dwCountX = pHeightmap->GetRGBLayer()->GetTileCountX();
			uint32 dwCountY = pHeightmap->GetRGBLayer()->GetTileCountY();

			if (!nTexSectorSize || !nTerrainSize || !dwCountX)
				continue;

			int numTexSectorsX = nTerrainSize / dwCountX / nTexSectorSize;
			int numTexSectorsY = nTerrainSize / dwCountY / nTexSectorSize;

			for (int iY = 0; iY < numTexSectorsY; ++iY)
				for (int iX = 0; iX < numTexSectorsX; ++iX)
					pHeightmap->UpdateSectorTexture(CPoint(iX + dwTileX * numTexSectorsY, iY + dwTileY * numTexSectorsX), 0, 0, 1, 1);
		}

	GetIEditorImpl()->GetActiveView()->Update();
}

