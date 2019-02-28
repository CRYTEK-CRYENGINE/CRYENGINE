// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainDialog.h"

#include "Dialogs/ToolbarDialog.h"
#include "Terrain/GenerationParam.h"
#include "Terrain/Noise.h"

#include "TerrainLighting.h"

#include "TopRendererWnd.h"
#include "Vegetation/VegetationMap.h"

#include "Terrain/Heightmap.h"
#include "Terrain/TerrainManager.h"
#include "Terrain/TerrainBrushTool.h"

#include "EditMode/ObjectMode.h"

#include "GameEngine.h"
#include "CryEdit.h"
#include "CryEditDoc.h"
#include <QtUtil.h>
#include "FileDialogs/SystemFileDialog.h"
#include "Controls/QuestionDialog.h"
#include "Util/MFCUtil.h"
#include <Preferences/GeneralPreferences.h>
#include <QDir>

#include <CrySystem/ISystem.h>
#include <CrySystem/IProjectManager.h>

#include "TerrainTextureExport.h"
#include "GameExporter.h"
#include "Terrain/Dialogs/ResizeTerrainTextureDialog.h"
#include "Terrain/Dialogs/ResizeTerrainDialog.h"
#include "Export/ExportManager.h"
#include "Objects/ObjectLoader.h"
#include "CrySandbox/ScopedVariableSetter.h"
#include "Dialogs/QNumericBoxDialog.h"

#define IDW_ROLLUP_PANE AFX_IDW_CONTROLBAR_FIRST + 10

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainDialog, CBaseFrameWnd)

/////////////////////////////////////////////////////////////////////////////
// CTerrainDialog dialog
CTerrainDialog::CTerrainDialog()
{
	// We don't have valid recent terrain generation parameters yet
	m_sLastParam = new SNoiseParams;
	m_sLastParam->bValid = false;
	m_pViewport = 0;

	m_pHeightmap = GetIEditorImpl()->GetHeightmap();
	if (m_pHeightmap)
		m_pHeightmap->signalWaterLevelChanged.Connect(this, &CTerrainDialog::InvalidateTerrain);

	CRect rc(0, 0, 0, 0);
	Create(WS_CHILD | WS_VISIBLE, rc, AfxGetMainWnd());

	GetIEditorImpl()->RegisterNotifyListener(this);
}

CTerrainDialog::~CTerrainDialog()
{
	if (m_pHeightmap)
		m_pHeightmap->signalWaterLevelChanged.DisconnectObject(this);

	GetIEditorImpl()->UnregisterNotifyListener(this);
	GetIEditorImpl()->SetEditTool(0);
	delete m_sLastParam;
}

BEGIN_MESSAGE_MAP(CTerrainDialog, CBaseFrameWnd)
ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
ON_COMMAND(ID_TERRAIN_LOAD, OnTerrainLoad)
ON_COMMAND(ID_TERRAIN_ERASE, OnTerrainErase)
ON_COMMAND(ID_TERRAIN_RESIZE, OnResizeTerrain)
ON_UPDATE_COMMAND_UI(ID_TERRAIN_RESIZE, OnTerrainResizeUpdateUI)
ON_COMMAND(ID_TERRAIN_LIGHT, OnTerrainLight)
ON_COMMAND(ID_TERRAIN_SURFACE, OnTerrainSurface)
ON_COMMAND(ID_TERRAIN_GENERATE, OnTerrainGenerate)
ON_COMMAND(ID_TERRAIN_INVERT, OnTerrainInvert)
ON_COMMAND(ID_FILE_EXPORTHEIGHTMAP, OnExportHeightmap)
ON_COMMAND(ID_MODIFY_MAKEISLE, OnModifyMakeisle)
ON_COMMAND(ID_MODIFY_FLATTEN_LIGHT, OnModifyFlattenLight)
ON_COMMAND(ID_MODIFY_FLATTEN_HEAVY, OnModifyFlattenHeavy)
ON_COMMAND(ID_MODIFY_SMOOTH, OnModifySmooth)
ON_COMMAND(ID_MODIFY_REMOVEWATER, OnModifyRemovewater)
ON_COMMAND(ID_MODIFY_SMOOTHSLOPE, OnModifySmoothSlope)
ON_COMMAND(ID_HEIGHTMAP_SHOWLARGEPREVIEW, OnHeightmapShowLargePreview)
ON_COMMAND(ID_MODIFY_SMOOTHBEACHESCOAST, OnModifySmoothBeachesOrCoast)
ON_COMMAND(ID_MODIFY_NORMALIZE, OnModifyNormalize)
ON_COMMAND(ID_MODIFY_REDUCERANGE, OnModifyReduceRange)
ON_COMMAND(ID_MODIFY_REDUCERANGELIGHT, OnModifyReduceRangeLight)
ON_WM_MOUSEWHEEL()
ON_COMMAND(ID_HOLD, OnHold)
ON_COMMAND(ID_FETCH, OnFetch)
ON_COMMAND(ID_OPTIONS_SHOWMAPOBJECTS, OnOptionsShowMapObjects)
ON_COMMAND(ID_OPTIONS_SHOWWATER, OnOptionsShowWater)
ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWWATER, OnShowWaterUpdateUI)
ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMAPOBJECTS, OnShowMapObjectsUpdateUI)
ON_COMMAND(ID_OPTIONS_SHOWGRID, OnOptionsShowGrid)
ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWGRID, OnShowGridUpdateUI)
ON_COMMAND(ID_OPTIONS_EDITTERRAINCURVE, OnOptionsEditTerrainCurve)
ON_COMMAND(ID_SETWATERLEVEL, OnSetWaterLevel)
ON_COMMAND(ID_MODIFY_SETMAXHEIGHT, OnSetMaxHeight)
ON_COMMAND(ID_MODIFY_SETUNITSIZE, OnSetUnitSize)
ON_COMMAND(ID_TOOLS_CUSTOMIZEKEYBOARD, OnCustomize)
ON_COMMAND(ID_TOOLS_EXPORT_SHORTCUTS, OnExportShortcuts)
ON_COMMAND(ID_TOOLS_IMPORT_SHORTCUTS, OnImportShortcuts)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainDialog message handlers

BOOL CTerrainDialog::OnInitDialog()
{
	CMFCUtils::LoadShortcuts(GetCommandBars(), IDR_TERRAIN, "TerrainEditor");

	// Create and setup the heightmap edit viewport and the toolbars
	GetCommandBars()->GetCommandBarsOptions()->bShowExpandButtonAlways = FALSE;
	GetCommandBars()->EnableCustomization(FALSE);

	CRect rcClient;
	GetClientRect(rcClient);

	CXTPCommandBar* pMenuBar = GetCommandBars()->SetMenu(_T("Menu Bar"), IDR_TERRAIN);
	pMenuBar->SetFlags(xtpFlagStretched);
	pMenuBar->EnableCustomization(TRUE);

	// Create Library toolbar.
	CXTPToolBar* pToolBar1 = GetCommandBars()->Add(_T("ToolBar1"), xtpBarTop);
	pToolBar1->EnableCustomization(FALSE);
	VERIFY(pToolBar1->LoadToolBar(IDR_TERRAIN));

	CXTPToolBar* pToolBar2 = GetCommandBars()->Add(_T("ToolBar2"), xtpBarTop);
	pToolBar2->EnableCustomization(FALSE);
	VERIFY(pToolBar2->LoadToolBar(IDR_BRUSHES));

	DockRightOf(pToolBar2, pToolBar1);

	// Create the status bar.
	{
		UINT indicators[] =
		{
			ID_SEPARATOR,           // status line indicator
		};
		VERIFY(m_wndStatusBar.Create(this, WS_CHILD | WS_VISIBLE | CBRS_BOTTOM));
		VERIFY(m_wndStatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT)));
	}

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for TaskPanel
	m_pDockPane_Rollup = GetDockingPaneManager()->CreatePane(IDW_ROLLUP_PANE, CRect(0, 0, 300, 500), xtpPaneDockRight);
	//m_pDockPane_Rollup->SetOptions(xtpPaneNoCloseable|xtpPaneNoFloatable);

	m_pViewport = new CTopRendererWnd;
	//m_pViewport->SetDlgCtrlID(AFX_IDW_PANE_FIRST);
	//m_pViewport->MoveWindow( CRect(20,50,500,500) );
	//m_pViewport->ModifyStyle( WS_POPUP,WS_CHILD,0 );
	//m_pViewport->SetParent( this );
	//m_pViewport->SetOwner( this );
	m_pViewport->m_bShowHeightmap = true;
	//m_pViewport->ShowWindow( SW_SHOW );
	m_pViewport->SetShowWater(true);
	m_pViewport->SetShowViewMarker(false);

	//////////////////////////////////////////////////////////////////////////
	char szCaption[128];
	cry_sprintf(szCaption, "Heightmap %" PRIu64 "x%" PRIu64, m_pHeightmap->GetWidth(), m_pHeightmap->GetHeight());
	m_wndStatusBar.SetPaneText(0, szCaption);

	AutoLoadFrameLayout("TerrainEditor");

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
LRESULT CTerrainDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (!pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_ROLLUP_PANE:
				pwndDockWindow->SetOptions(xtpPaneNoCloseable);
				break;
			default:
				return FALSE;
			}
		}
		return TRUE;
	}
	else if (wParam == XTP_DPN_CLOSEPANE)
	{
		// get a pointer to the docking pane being closed.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (pwndDockWindow->IsValid())
		{
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CTerrainDialog::OnKickIdle(WPARAM wParam, LPARAM lParam)
{
	if (m_pViewport)
		m_pViewport->Update();

	return FALSE;
}

void CTerrainDialog::OnTerrainLoad()
{
	////////////////////////////////////////////////////////////////////////
	// Load a heightmap from a file
	////////////////////////////////////////////////////////////////////////
	const QDir dir = QDir::cleanPath(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute());
	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = dir.absolutePath();
	runParams.title = QObject::tr("Import Heightmap");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("All Image Files (bmp, tif, pgm, raw, r16)"), QStringList() << "bmp" << "tif" << "pgm" << "raw" << "r16");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("8-bit Bitmap Files (bmp)"), "bmp");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("TIFF Files (tif)"), "tif");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("16-bit PGM Files (pgm)"), "pgm");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("16-bit RAW Files (raw)"), "raw");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("16-bit RAW Files (r16)"), "r16");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("All files (*.*)"), "*");

	QString fileName = CSystemFileDialog::RunImportFile(runParams, nullptr);
	string path = fileName.toStdString().c_str();

	if (!fileName.isEmpty())
	{
		char ext[_MAX_EXT];
		_splitpath(path, NULL, NULL, NULL, ext);

		CWaitCursor wait;

		if (stricmp(ext, ".pgm") == 0)
		{
			m_pHeightmap->LoadPGM(path);
		}
		else if (stricmp(ext, ".raw") == 0 || stricmp(ext, ".r16") == 0)
		{
			m_pHeightmap->LoadRAW(path);
		}
		else
		{
			// Load the heightmap
			m_pHeightmap->LoadImage(path);
		}

		InvalidateTerrain();

		if (m_pViewport)
			m_pViewport->InitHeightmapAlignment();
		InvalidateViewport();
	}
}

void CTerrainDialog::OnTerrainErase()
{
	////////////////////////////////////////////////////////////////////////
	// Erase the heightmap
	////////////////////////////////////////////////////////////////////////

	// Ask first
	if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Really erase the heightmap ?")))
	{
		return;
	}

	// Erase it
	CUndo undo("Erase Terrain");
	m_pHeightmap->Clear(false);

	InvalidateTerrain();

	// All layers need to be generated from scratch
	GetIEditorImpl()->GetTerrainManager()->InvalidateLayers();
}

////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnTerrainResizeUpdateUI(CCmdUI* pCmdUI)
{
	pCmdUI->Enable((GetIEditorImpl()->GetDocument()->IsDocumentReady()) ? TRUE : FALSE);
}

void CTerrainDialog::OnTerrainInvert()
{
	////////////////////////////////////////////////////////////////////////
	// Invert the heightmap
	////////////////////////////////////////////////////////////////////////
	CUndo undo("Invert Terrain");
	CWaitCursor wait;

	m_pHeightmap->Invert();

	InvalidateTerrain();
}

void CTerrainDialog::OnTerrainGenerate()
{
	////////////////////////////////////////////////////////////////////////
	// Generate a terrain
	////////////////////////////////////////////////////////////////////////

	SNoiseParams sParam;
	CGenerationParam cDialog;

	if (GetLastParam()->bValid)
	{
		// Use last parameters
		cDialog.LoadParam(GetLastParam());
	}
	else
	{
		// Set default parameters for the dialog
		cDialog.m_sldFrequency = (int) (7.0f * 10); // Feature Size
		cDialog.m_sldFade = (int) (0.46f * 10);     // Bumpiness
		cDialog.m_sldPasses = 8;                    // Detail (Passes)
		cDialog.m_sldRandomBase = 1;                // Variation
		cDialog.m_sldBlur = 0;
		cDialog.m_sldCover = 0;
		cDialog.m_sldSharpness = (int) (0.999f * 1000);
		cDialog.m_sldFrequencyStep = (int) (2.0f * 10);
	}

	// Show the generation parameter dialog
	if (cDialog.DoModal() == IDCANCEL)
		return;

	CLogFile::WriteLine("Generating new terrain...");
	CUndo undo("Generate Terrain");

	// Fill the parameter structure for the terrain generation
	cDialog.FillParam(&sParam);
	sParam.iWidth = m_pHeightmap->GetWidth();
	sParam.iHeight = m_pHeightmap->GetHeight();
	sParam.bBlueSky = false;

	// Save the paramters
	ZeroStruct(*m_sLastParam);

	CWaitCursor wait;
	// Generate
	m_pHeightmap->GenerateTerrain(sParam);

	InvalidateTerrain();
}

void CTerrainDialog::OnExportHeightmap()
{
	const QDir dir = QDir::cleanPath(gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryAbsolute());

	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = dir.absolutePath();
	runParams.title = QObject::tr("Export Heightmap");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("16-bit RAW Files (raw)"), "raw");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("16-bit RAW Files (r16)"), "r16");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("16-bit PGM Files (pgm)"), "pgm");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("8-bit Bitmap Files (bmp)"), "bmp");

	const QString path = CSystemFileDialog::RunExportFile(runParams, nullptr);
	string filePath = path.toStdString().c_str();

	if (!path.isEmpty())
	{
		CWaitCursor wait;
		CLogFile::WriteLine("Exporting heightmap...");
		string str = path.toStdString().c_str();

		char ext[_MAX_EXT];
		_splitpath(str, NULL, NULL, NULL, ext);

		if (stricmp(ext, ".pgm") == 0)
		{
			m_pHeightmap->SavePGM(filePath);
		}
		else if (stricmp(ext, ".raw") == 0 || stricmp(ext, ".r16") == 0)
		{
			m_pHeightmap->SaveRAW(filePath);
		}
		else
		{
			// BMP or others
			m_pHeightmap->SaveImage(filePath);
		}
	}
}

void CTerrainDialog::OnModifySmoothBeachesOrCoast()
{
	////////////////////////////////////////////////////////////////////////
	// Make smooth beaches or a smooth coast
	////////////////////////////////////////////////////////////////////////
	CWaitCursor wait;
	CUndo undo("Smooth Beaches");

	// Call the smooth beaches function of the heightmap class
	m_pHeightmap->MakeBeaches();
	InvalidateTerrain();
}

void CTerrainDialog::OnModifyMakeisle()
{
	////////////////////////////////////////////////////////////////////////
	// Convert the heightmap to an island
	////////////////////////////////////////////////////////////////////////
	CWaitCursor wait;
	CUndo undo("Make Isle");

	// Call the make isle fucntion of the heightmap class
	m_pHeightmap->MakeIsle();

	InvalidateTerrain();
}

void CTerrainDialog::Flatten(float fFactor)
{
	////////////////////////////////////////////////////////////////////////
	// Increase the number of flat areas on the heightmap
	////////////////////////////////////////////////////////////////////////

	CWaitCursor wait;

	// Call the flatten function of the heigtmap class
	m_pHeightmap->Flatten(fFactor);

	InvalidateTerrain();
}

void CTerrainDialog::OnModifyFlattenLight()
{
	CUndo undo("Flatten Terrain (Light)");
	Flatten(0.75f);
}

void CTerrainDialog::OnModifyFlattenHeavy()
{
	CUndo undo("Flatten Terrain (Heavy)");
	Flatten(0.5f);
}

void CTerrainDialog::OnModifyRemovewater()
{
	//////////////////////////////////////////////////////////////////////
	// Remove all water areas from the heightmap
	//////////////////////////////////////////////////////////////////////
	CWaitCursor wait;
	CUndo undo("Remove Water");
	CLogFile::WriteLine("Removing water areas from heightmap...");

	// Using a water level <=0, if we reload the environment, it will
	// cause in method CTerrain::InitTerrainWater the SAFE_DELETE(m_pOcean);
	m_pHeightmap->SetWaterLevel(WATER_LEVEL_UNKNOWN);
}

void CTerrainDialog::OnModifySmoothSlope()
{
	//////////////////////////////////////////////////////////////////////
	// Remove areas with high slope from the heightmap
	//////////////////////////////////////////////////////////////////////
	CWaitCursor wait;
	CUndo undo("Modify Smooth Slope");

	// Call the smooth slope function of the heightmap class
	m_pHeightmap->SmoothSlope();

	InvalidateTerrain();
}

void CTerrainDialog::OnModifySmooth()
{
	//////////////////////////////////////////////////////////////////////
	// Smooth the heightmap
	//////////////////////////////////////////////////////////////////////
	CWaitCursor wait;
	CUndo undo("Smooth Terrain");

	m_pHeightmap->Smooth();
	InvalidateTerrain();
}

void CTerrainDialog::OnModifyNormalize()
{
	////////////////////////////////////////////////////////////////////////
	// Normalize the heightmap
	////////////////////////////////////////////////////////////////////////
	CWaitCursor wait;
	CUndo undo("Normalize Terrain");

	m_pHeightmap->Normalize();

	InvalidateTerrain();
}

void CTerrainDialog::OnModifyReduceRange()
{
	////////////////////////////////////////////////////////////////////////
	// Reduce the value range of the heightmap (Heavy)
	////////////////////////////////////////////////////////////////////////
	CWaitCursor wait;
	CUndo undo("Modify Range (Heavy)");

	m_pHeightmap->LowerRange(0.8f);

	InvalidateTerrain();
}

void CTerrainDialog::OnModifyReduceRangeLight()
{
	////////////////////////////////////////////////////////////////////////
	// Reduce the value range of the heightmap (Light)
	////////////////////////////////////////////////////////////////////////
	CWaitCursor wait;
	CUndo undo("Modify Range (Light)");

	m_pHeightmap->LowerRange(0.95f);

	InvalidateTerrain();
}

void CTerrainDialog::OnHeightmapShowLargePreview()
{
	////////////////////////////////////////////////////////////////////////
	// Show a full-size version of the heightmap
	////////////////////////////////////////////////////////////////////////

	BeginWaitCursor();

	CLogFile::WriteLine("Exporting heightmap...");

	UINT iWidth = m_pHeightmap->GetWidth();
	UINT iHeight = m_pHeightmap->GetHeight();

	float fMaxHeight = m_pHeightmap->GetMaxHeight();

	CImageEx image;
	image.Allocate(iHeight, iWidth);   // swap x with y
	// Allocate memory to export the heightmap
	DWORD* pImageData = (DWORD*)image.GetData();

	// Get a pointer to the heightmap data
	t_hmap* pHeightmap = m_pHeightmap->GetData();

	// Write heightmap into the image data array
	for (UINT y = 0; y < iHeight; ++y)
	{
		for (UINT x = 0; x < iWidth; ++x)
		{
			// Get a normalized grayscale value from the heigthmap
			uint8 iColor = (uint8)__min(pHeightmap[x + y * iWidth] / fMaxHeight * 255.0f, 255.0f);

			// Create a BGR grayscale value and store it in the image
			// data array
			pImageData[y + (iWidth - 1 - x) * iHeight] =   // swap x with y
			                                             (iColor << 16) | (iColor << 8) | iColor;
		}
	}

	// Save the heightmap into the bitmap
	string temp = gEditorFilePreferences.strStandardTempDirectory.c_str();
	string tempDirectory = PathUtil::AddBackslash(temp.GetString());
	CFileUtil::CreateDirectory(tempDirectory.GetBuffer());

	string imageName = "HeightmapPreview.bmp";
	bool bOk = CImageUtil::SaveImage(tempDirectory + imageName, image);

	EndWaitCursor();

	if (bOk)
	{
		// Show the heightmap
		::ShellExecute(::GetActiveWindow(), "open", imageName.GetBuffer(),
		               "", tempDirectory.GetBuffer(), SW_SHOWMAXIMIZED);
	}
}

BOOL CTerrainDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	////////////////////////////////////////////////////////////////////////
	// Forward mouse wheel messages to the drawing window
	////////////////////////////////////////////////////////////////////////

	return __super::OnMouseWheel(nFlags, zDelta, pt);
}

void CTerrainDialog::OnTerrainLight()
{
	////////////////////////////////////////////////////////////////////////
	// Show the terrain lighting tool
	////////////////////////////////////////////////////////////////////////
	GetIEditorImpl()->OpenView(LIGHTING_TOOL_WINDOW_NAME);
}

void CTerrainDialog::OnTerrainSurface()
{
	////////////////////////////////////////////////////////////////////////
	// Show the terrain texture dialog
	////////////////////////////////////////////////////////////////////////

	GetIEditorImpl()->OpenView("Terrain Texture Layers");
}

void CTerrainDialog::OnHold()
{
	// Hold the current heightmap state
	m_pHeightmap->Hold();
}

void CTerrainDialog::OnFetch()
{
	int iResult;

	// Ask first
	iResult = MessageBox("Do you really want to restore the previous heightmap state ?",
		"Fetch", MB_YESNO | MB_ICONQUESTION);

	// Abort
	if (iResult == IDNO)
		return;

	// Restore the old heightmap state
	m_pHeightmap->Fetch();

	// We modified the document
	GetIEditorImpl()->SetModifiedFlag();

	// All layers need to be generated from scratch
	GetIEditorImpl()->GetTerrainManager()->InvalidateLayers();

	InvalidateTerrain();
}

/////////////////////////////////////////////////////////////////////////////
// Options
void CTerrainDialog::OnShowWaterUpdateUI(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_pViewport->GetShowWater() ? 1 : 0);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnShowMapObjectsUpdateUI(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_pViewport->m_bShowStatObjects ? 1 : 0);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnShowGridUpdateUI(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_pViewport->GetShowGrid() ? 1 : 0);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnOptionsShowMapObjects()
{
	m_pViewport->m_bShowStatObjects = !m_pViewport->m_bShowStatObjects;

	// Update the draw window
	InvalidateViewport();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnOptionsShowWater()
{
	m_pViewport->SetShowWater(!m_pViewport->GetShowWater());
	// Update the draw window
	InvalidateViewport();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnOptionsShowGrid()
{
	m_pViewport->SetShowGrid(!m_pViewport->GetShowGrid());
	// Update the draw window
	InvalidateViewport();
}

void CTerrainDialog::OnOptionsEditTerrainCurve()
{
}

void CTerrainDialog::OnSetWaterLevel()
{
	////////////////////////////////////////////////////////////////////////
	// Let the user change the current water level
	////////////////////////////////////////////////////////////////////////
	// Get the water level from the document and set it as default into
	// the dialog
	float fPreviousWaterLevel = GetIEditorImpl()->GetHeightmap()->GetWaterLevel();
	QNumericBoxDialog dlg(QObject::tr("Set Water Height"), fPreviousWaterLevel);

	// Show the dialog
	if (dlg.exec() == QDialog::Accepted)
	{
		// Retrieve the new water level from the dialog and save it in the document
		float waterLevel = dlg.GetValue();
		CUndo undo("Modify Water Level");
		GetIEditorImpl()->GetHeightmap()->SetWaterLevel(waterLevel);
	}
}

void CTerrainDialog::OnSetMaxHeight()
{
	////////////////////////////////////////////////////////////////////////
	// Let the user change the current water level
	////////////////////////////////////////////////////////////////////////
	// Get the water level from the document and set it as default into
	// the dialog
	float fValue = GetIEditorImpl()->GetHeightmap()->GetMaxHeight();
	QNumericBoxDialog dlg(QObject::tr("Set Max Terrain Height"), fValue);

	dlg.SetRange(1.0f, 1024.f * 8);
	dlg.RestrictToInt();

	// Show the dialog
	if (dlg.exec() == QDialog::Accepted)
	{
		// Retrieve the new water level from the dialog and save it in the document
		fValue = dlg.GetValue();
		CUndo undo("Set Max Height");
		GetIEditorImpl()->GetHeightmap()->SetMaxHeight(fValue);

		InvalidateTerrain();
	}
}

void CTerrainDialog::OnSetUnitSize()
{
	////////////////////////////////////////////////////////////////////////
	// Let the user change the current water level
	////////////////////////////////////////////////////////////////////////
	// Get the water level from the document and set it as default into
	// the dialog
	float fValue = GetIEditorImpl()->GetHeightmap()->GetUnitSize();
	QNumericBoxDialog dlg(QObject::tr("Set Unit Size (Meters per unit)"), fValue);
	dlg.RestrictToInt();

	// Show the dialog
	if (dlg.exec() == QDialog::Accepted)
	{
		// Retrieve the new water level from the dialog and save it in the document
		fValue = dlg.GetValue();
		GetIEditorImpl()->GetHeightmap()->SetUnitSize(fValue);

		InvalidateTerrain();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::InvalidateTerrain()
{
	GetIEditorImpl()->SetModifiedFlag();
	GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain(true);

	// All layers need to be generated from scratch
	GetIEditorImpl()->GetTerrainManager()->InvalidateLayers();

	GetIEditorImpl()->GetGameEngine()->ReloadEnvironment();

	InvalidateViewport();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::InvalidateViewport()
{
	LOADING_TIME_PROFILE_SECTION;
	m_pViewport->Invalidate();
	GetIEditorImpl()->UpdateViews(eUpdateHeightmap);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnEndNewScene:
	case eNotify_OnEndSceneOpen:
	case eNotify_OnTerrainRebuild:
		m_pViewport->InitHeightmapAlignment();
		InvalidateViewport();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnCustomize()
{
	CMFCUtils::ShowShortcutsCustomizeDlg(GetCommandBars(), IDR_TERRAIN, "TerrainEditor");
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnExportShortcuts()
{
	CMFCUtils::ExportShortcuts(GetCommandBars()->GetShortcutManager());
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnImportShortcuts()
{
	CMFCUtils::ImportShortcuts(GetCommandBars()->GetShortcutManager(), "TerrainEditor");
}

//////////////////////////////////////////////////////////////////////////

void CTerrainDialog::GenerateTerrainTexture()
{
	CGameEngine* pGameEngine = GetIEditorImpl()->GetGameEngine();
	if (!pGameEngine->IsLevelLoaded())
	{
		if (pGameEngine->GetLevelPath().IsEmpty())
		{
			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Open or create a level first."));
			return;
		}
		// If level not loaded first fast export terrain.
		CGameExporter gameExporter;
		gameExporter.Export(eExp_ReloadTerrain);
	}

	if (!GetIEditorImpl()->GetDocument()->IsDocumentReady())
	{
		return;
	}

	CResizeTerrainTextureDialog::Result initial;
	initial.resolution = 4096; // 4096x4096 is default
	CResizeTerrainTextureDialog textureDialog(initial);

	// Query the size of the preview
	if (textureDialog.exec() != QDialog::Accepted)
	{
		return;
	}

	auto dialogResult = textureDialog.GetResult();

	SGameExporterSettings settings;
	settings.iExportTexWidth = dialogResult.resolution;
	settings.bHiQualityCompression = dialogResult.isHighQualityCompression;
	settings.fBrMultiplier = dialogResult.colorMultiplier;
	settings.eExportEndian = eLittleEndian;

	CScopedVariableSetter<bool> autoBackupEnabledChange(gEditorFilePreferences.autoSaveEnabled, false);

	CGameExporter gameExporter(settings);
	gameExporter.Export(eExp_SurfaceTexture | eExp_ReloadTerrain, ".");
}

void CTerrainDialog::TerrainTextureExport()
{
	GetIEditorImpl()->SetEditTool(0);

	CTerrainTextureExport cDialog;
	cDialog.DoModal();
}

void CTerrainDialog::OnExportTerrainArea()
{
	AABB bbox;
	GetIEditorImpl()->GetSelectedRegion(bbox);
	if (!GetIEditorImpl()->GetDocument() || !GetIEditorImpl()->GetDocument() || !bbox.IsNonZero())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "No Terrain Area is selected. Use \"Select Terrain\" tool to select the terrain area.");
		CQuestionDialog::SWarning(QObject::tr("No Terrain Area"), QObject::tr("No Terrain Area is selected. Use \"Select Terrain\" tool to select the terrain area."));
		return;
	}

	CExportManager* pExportManager = static_cast<CExportManager*>(GetIEditorImpl()->GetExportManager());
	string levelName = GetIEditorImpl()->GetGameEngine()->GetLevelName();
	string levelPath = GetIEditorImpl()->GetGameEngine()->GetLevelPath();
	pExportManager->Export(levelName + "_terrain", "obj", levelPath, false, false, true);
}

void CTerrainDialog::OnExportTerrainAreaWithObjects()
{
	AABB bbox;
	GetIEditorImpl()->GetSelectedRegion(bbox);
	if (!GetIEditorImpl()->GetDocument() || !GetIEditorImpl()->GetDocument() || !bbox.IsNonZero())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "No Terrain Area is selected. Use \"Select Terrain\" tool to select the terrain area.");
		CQuestionDialog::SWarning(QObject::tr("No Terrain Area"), QObject::tr("No Terrain Area is selected. Use \"Select Terrain\" tool to select the terrain area."));
		return;
	}

	CExportManager* pExportManager = static_cast<CExportManager*>(GetIEditorImpl()->GetExportManager());
	string levelName = GetIEditorImpl()->GetGameEngine()->GetLevelName();
	string levelPath = GetIEditorImpl()->GetGameEngine()->GetLevelPath();
	pExportManager->Export(levelName, "obj", levelPath, false, true, true);
}

void CTerrainDialog::OnReloadTerrain()
{
	// Fast export.
	CGameExporter gameExporter;
	gameExporter.Export(eExp_ReloadTerrain);
	// Export does it. GetIEditorImpl()->GetGameEngine()->ReloadLevel();
}

void CTerrainDialog::OnTerrainExportBlock()
{
	// TODO: Add your command handler code here
	AABB box;
	GetIEditorImpl()->GetSelectedRegion(box);
	if (!GetIEditorImpl()->GetDocument() || !GetIEditorImpl()->IsDocumentReady() || !box.IsNonZero())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "No Terrain Area is selected. Use \"Select Terrain\" tool to select the terrain area.");
		CQuestionDialog::SWarning(QObject::tr("No Terrain Area"), QObject::tr("No Terrain Area is selected. Use \"Select Terrain\" tool to select the terrain area."));
		return;
	}

	CSystemFileDialog::RunParams runParams;
	runParams.title = QObject::tr("Export Terrain Block");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("Terrain Block files (trb)"), "trb");

	QString path = CSystemFileDialog::RunExportFile(runParams, nullptr);

	if (!path.isEmpty())
	{
		CWaitCursor wait;

		CPoint p1 = GetIEditorImpl()->GetHeightmap()->WorldToHmap(box.min);
		CPoint p2 = GetIEditorImpl()->GetHeightmap()->WorldToHmap(box.max);
		CRect rect(p1, p2);

		CXmlArchive ar("Root");
		GetIEditorImpl()->GetHeightmap()->ExportBlock(rect, ar, true, true);

		// Save selected objects.
		const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();
		XmlNodeRef objRoot = ar.root->newChild("Objects");
		CObjectArchive objAr(GetIEditorImpl()->GetObjectManager(), objRoot, false);

		// Save all objects to XML.
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject* obj = sel->GetObject(i);
			objAr.node = objRoot->newChild("Object");
			obj->Serialize(objAr);
		}

		ar.Save(path.toStdString().c_str());
	}
}

void CTerrainDialog::OnTerrainImportBlock()
{
	// TODO: Add your command handler code here
	CSystemFileDialog::RunParams runParams;
	runParams.title = QObject::tr("Import Terrain Block");

	runParams.extensionFilters << CExtensionFilter(QObject::tr("Terrain Block files (trb)"), "trb");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("All files"), "*");

	QString fileName = CSystemFileDialog::RunImportFile(runParams, nullptr);

	if (!fileName.isEmpty())
	{
		CWaitCursor wait;
		CXmlArchive* ar = new CXmlArchive;
		if (!ar->Load(fileName.toStdString().c_str()))
		{
			CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("Loading of Terrain Block file failed"));
			delete ar;
			return;
		}

		// Import terrain area.
		CUndo undo("Import Terrain Area");

		CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
		pHeightmap->ImportBlock(*ar, CPoint(0, 0), false);
		// Load selection from archive.
		XmlNodeRef objRoot = ar->root->findChild("Objects");
		if (objRoot)
		{
			GetIEditorImpl()->ClearSelection();
			CObjectArchive ar(GetIEditorImpl()->GetObjectManager(), objRoot, true);
			GetIEditorImpl()->GetObjectManager()->LoadObjects(ar, true);
		}

		delete ar;
		ar = 0;

		/*
		// Archive will be deleted within Move tool.
		CTerrainMoveTool *mt = new CTerrainMoveTool;
		mt->SetArchive( ar );
		GetIEditorImpl()->SetEditTool( mt );
		*/
	}
}

void CTerrainDialog::OnResizeTerrain()
{
	if (!GetIEditorImpl()->GetDocument()->IsDocumentReady())
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Please wait until previous operation will be finished."));
		return;
	}

	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();

	CResizeTerrainDialog::Result initial;
	initial.heightmap.resolution = pHeightmap->GetWidth();
	initial.heightmap.unitSize = pHeightmap->GetUnitSize();

	CResizeTerrainDialog dialog(initial);
	if (dialog.exec() != QDialog::Accepted)
	{
		return;
	}

	pHeightmap->ClearModSectors();

	IUndoManager* undoMgr = GetIEditorImpl()->GetIUndoManager();
	if (undoMgr)
	{
		undoMgr->Flush();
	}
	auto result = dialog.GetResult();
	int resolution = result.heightmap.resolution;
	float unitSize = result.heightmap.unitSize;

	if (resolution != pHeightmap->GetWidth() || unitSize != pHeightmap->GetUnitSize())
	{
		pHeightmap->Resize(resolution, resolution, unitSize, false);

		SGameExporterSettings settings;
		settings.iExportTexWidth = result.texture.resolution;
		settings.bHiQualityCompression = result.texture.isHighQualityCompression;
		settings.fBrMultiplier = result.texture.colorMultiplier;
		settings.eExportEndian = eLittleEndian;

		CScopedVariableSetter<bool> autoBackupEnabledChange(gEditorFilePreferences.autoSaveEnabled, false);

		CGameExporter gameExporter(settings);
		gameExporter.Export(eExp_SurfaceTexture | eExp_ReloadTerrain, ".");
	}

	GetIEditorImpl()->Notify(eNotify_OnTerrainRebuild);
}
