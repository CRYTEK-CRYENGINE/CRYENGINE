// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainTexture.h"
#include "CryEditDoc.h"
#include "Terrain/Dialogs/CreateTerrainPreviewDialog.h"
#include "Dialogs/ToolbarDialog.h"
#include "Dialogs/QNumericBoxDialog.h"
#include "Terrain/SurfaceType.h"
#include "Terrain/Layer.h"
#include "Terrain/TerrainTexGen.h"
#include "Terrain/TerrainManager.h"
#include "Material/MaterialManager.h"
#include "Controls/QuestionDialog.h"
#include "FileDialogs/SystemFileDialog.h"
#include "QtUtil.h"
#include "Controls/SharedFonts.h"
#include "Util/MFCUtil.h"

#include <Preferences/GeneralPreferences.h>
#include <QDir>

// Static member variables
bool CTerrainTextureDialog::m_bUseLighting = true;
bool CTerrainTextureDialog::m_bShowWater = true;

#define IDC_REPORT_CONTROL AFX_IDW_PANE_FIRST + 10

#define IDC_TASKPANEL      1

// Task panel commands.
#define CMD_LAYER_ADD             1
#define CMD_LAYER_DELETE          2
#define CMD_LAYER_MOVEUP          3
#define CMD_LAYER_MOVEDOWN        4
#define CMD_LAYER_LOAD_TEXTURE    5
#define CMD_LAYER_ASIGN_MATERIAL  6
#define CMD_OPEN_MATERIAL_EDITOR  7
#define CMD_LAYER_TEXTURE_INFO    8
#define CMD_LAYER_EXPORT_TEXTURE  9

#define IDC_SHOW_PREVIEW_CHECKBOX 10
#define IDC_LAYER_PREVIEW_BUTTON  11

//#define COLUMN_LAYER_ICON  0
#define COLUMN_LAYER_NAME    0
#define COLUMN_MATERIAL      1
#define COLUMN_MIN_HEIGHT    2
#define COLUMN_MAX_HEIGHT    3
#define COLUMN_MIN_ANGLE     4
#define COLUMN_MAX_ANGLE     5
#define COLUMN_BRIGHTNESS    6
#define COLUMN_TILING        7
#define COLUMN_SORT_ORDER    8
#define COLUMN_SPEC_AMOUNT   9
#define COLUMN_USE_REMESH    10

#define TEXTURE_PREVIEW_SIZE 32

#define SUPPORTED_IMAGES_FILTER      "All Image Files|*.bmp;*.jpg;*.gif;*.pgm;*.raw|All files|*.*||"

//////////////////////////////////////////////////////////////////////////
class CTerrainLayerRecord : public CXTPReportRecord
{
	DECLARE_DYNAMIC(CTerrainLayerRecord)
public:
	string               m_layerName;
	CLayer*               m_pLayer;
	CXTPReportRecordItem* m_pVisibleIconItem;

	class CustomPreviewItem : public CXTPReportRecordItemPreview
	{
	public:
		virtual int GetPreviewHeight(CDC* pDC, CXTPReportRow* pRow, int nWidth)
		{
			CXTPImageManagerIcon* pIcon = pRow->GetControl()->GetImageManager()->GetImage(GetIconIndex(), 0);
			if (!pIcon)
				return 0;
			CSize szImage(pIcon->GetWidth(), pIcon->GetHeight());
			return szImage.cy + 6;
		}
	};

	CTerrainLayerRecord(CLayer* pLayer, int nIndex)
	{
		m_pLayer = pLayer;
		m_layerName = pLayer->GetLayerName();

		string mtlName;
		CSurfaceType* pSurfaceType = pLayer->GetSurfaceType();
		if (pSurfaceType)
		{
			mtlName = pSurfaceType->GetMaterial();
		}

		//CXTPReportRecordItem *pIconItem = AddItem(new CXTPReportRecordItem());
		//pIconItem->SetIconIndex(nIndex);

		AddItem(new CXTPReportRecordItemText(m_layerName))->SetBold(TRUE);
		AddItem(new CXTPReportRecordItemText(mtlName))->AddHyperlink(new CXTPReportHyperlink(0, mtlName.GetLength()));
		AddItem(new CXTPReportRecordItemNumber(m_pLayer->GetLayerStart(), _T("%g")));
		AddItem(new CXTPReportRecordItemNumber(m_pLayer->GetLayerEnd(), _T("%g")));
		AddItem(new CXTPReportRecordItemNumber(m_pLayer->GetLayerMinSlopeAngle(), _T("%g")));
		AddItem(new CXTPReportRecordItemNumber(m_pLayer->GetLayerMaxSlopeAngle(), _T("%g")));
		AddItem(new CXTPReportRecordItemNumber(m_pLayer->GetLayerTiling(), _T("%g")));
		AddItem(new CXTPReportRecordItemNumber(m_pLayer->GetLayerSortOrder(), _T("%g")));
		AddItem(new CXTPReportRecordItemNumber(m_pLayer->GetLayerSpecularAmount(), _T("%g")));
		AddItem(new CXTPReportRecordItemNumber(m_pLayer->GetLayerUseRemeshing(), _T("%g")));

		CustomPreviewItem* pItem = new CustomPreviewItem;
		pItem->SetIconIndex(nIndex);
		SetPreviewItem(pItem);
	}

	virtual void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
		if (pDrawArgs->pColumn)
		{
			if (pDrawArgs->pColumn->GetIndex() == COLUMN_MIN_HEIGHT || pDrawArgs->pColumn->GetIndex() == COLUMN_MAX_HEIGHT)
			{
				pItemMetrics->clrBackground = RGB(230, 255, 230);
			}
			else if (pDrawArgs->pColumn->GetIndex() == COLUMN_MIN_ANGLE || pDrawArgs->pColumn->GetIndex() == COLUMN_MAX_ANGLE)
			{
				pItemMetrics->clrBackground = RGB(230, 255, 255);
			}
			else if (pDrawArgs->pColumn->GetIndex() == COLUMN_BRIGHTNESS || pDrawArgs->pColumn->GetIndex() == COLUMN_TILING || pDrawArgs->pColumn->GetIndex() == COLUMN_SORT_ORDER)
			{
				pItemMetrics->clrBackground = RGB(230, 230, 255);
			}
			else if (pDrawArgs->pColumn->GetIndex() == COLUMN_SPEC_AMOUNT || pDrawArgs->pColumn->GetIndex() == COLUMN_USE_REMESH)
			{
				pItemMetrics->clrBackground = RGB(255, 230, 255);
			}
		}
	}
};
IMPLEMENT_DYNAMIC(CTerrainLayerRecord, CXTPReportRecord);

/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureDialog dialog

//////////////////////////////////////////////////////////////////////////
class CTerrainLayersUndoObject : public IUndoObject
{
public:
	CTerrainLayersUndoObject()
	{
		m_undo.root = GetISystem()->CreateXmlNode("Undo");
		m_redo.root = GetISystem()->CreateXmlNode("Redo");
		m_undo.bLoading = false;
		GetIEditorImpl()->GetTerrainManager()->SerializeLayerSettings(m_undo);
	}
protected:
	virtual const char* GetDescription() { return "Terrain Layers"; };

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_redo.bLoading = false; // save redo
			GetIEditorImpl()->GetTerrainManager()->SerializeLayerSettings(m_redo);
		}
		m_undo.bLoading = true; // load undo
		GetIEditorImpl()->GetTerrainManager()->SerializeLayerSettings(m_undo);

		CTerrainTextureDialog::OnUndoUpdate();
		GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
	}
	virtual void Redo()
	{
		m_redo.bLoading = true; // load redo
		GetIEditorImpl()->GetTerrainManager()->SerializeLayerSettings(m_redo);

		CTerrainTextureDialog::OnUndoUpdate();
		GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
	}

private:
	CXmlArchive m_undo;
	CXmlArchive m_redo;
};
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CTerrainTextureDialog, CToolbarDialog)

static CTerrainTextureDialog * m_gTerrainTextureDialog = 0;

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnUndoUpdate()
{
	if (m_gTerrainTextureDialog)
		m_gTerrainTextureDialog->ReloadLayerList();
}

//////////////////////////////////////////////////////////////////////////
CTerrainTextureDialog::CTerrainTextureDialog(CWnd* pParent /*=NULL*/)
	: CToolbarDialog(CTerrainTextureDialog::IDD, pParent)
	, m_pAssignMaterialLink(NULL)
{
	static bool bFirstInstance = true;

	m_gTerrainTextureDialog = this;

	// No current layer at first
	m_pCurrentLayer = nullptr;
	CTerrainManager* pTerrainManager = GetIEditorImpl()->GetTerrainManager();
	if (pTerrainManager)
	{
		for (int i = 0; i < pTerrainManager->GetLayerCount(); i++)
		{
			if (pTerrainManager->GetLayer(i)->IsSelected())
			{
				m_pCurrentLayer = pTerrainManager->GetLayer(i);
				break;
			}
		}
	}

	// Paint it gray
	if (bFirstInstance)
	{
		bFirstInstance = false; // Leave it the next time
	}

	Create(CTerrainTextureDialog::IDD, AfxGetMainWnd());

	m_bIgnoreNotify = false;
	GetIEditorImpl()->RegisterNotifyListener(this);
	GetIEditorImpl()->GetMaterialManager()->AddListener(this);
	GetIEditorImpl()->GetTerrainManager()->signalSelectedLayerChanged.Connect(this, &CTerrainTextureDialog::SelectedLayerChanged);
}

//////////////////////////////////////////////////////////////////////////
CTerrainTextureDialog::~CTerrainTextureDialog()
{
	GetIEditorImpl()->GetTerrainManager()->signalSelectedLayerChanged.DisconnectObject(this);
	GetIEditorImpl()->GetMaterialManager()->RemoveListener(this);
	GetIEditorImpl()->UnregisterNotifyListener(this);
	ClearData();

	m_gTerrainTextureDialog = 0;

	CompressLayers();

	//GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain(0,0,
	//	GetIEditorImpl()->GetHeightmap()->GetWidth(),
	//	GetIEditorImpl()->GetHeightmap()->GetHeight(),false,true);

	GetIEditorImpl()->UpdateViews(eUpdateHeightmap);
}

void CTerrainTextureDialog::SelectedLayerChanged(CLayer* pLayer)
{
	m_pCurrentLayer = pLayer;
}

void CTerrainTextureDialog::ClearData()
{
	if (m_wndReport.m_hWnd)
	{
		m_wndReport.GetSelectedRows()->Clear();
		m_wndReport.GetRecords()->RemoveAll();
	}

	// Release all layer masks.
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
		GetIEditorImpl()->GetTerrainManager()->GetLayer(i)->ReleaseTempResources();

	m_pCurrentLayer = 0;
}

BEGIN_MESSAGE_MAP(CTerrainTextureDialog, CToolbarDialog)
//{{AFX_MSG_MAP(CTerrainTextureDialog)
ON_WM_SIZE()
ON_BN_CLICKED(IDC_LOAD_TEXTURE, OnLoadTexture)
ON_BN_CLICKED(IDC_IMPORT, OnImport)
ON_BN_CLICKED(IDC_EXPORT, OnExport)
ON_BN_CLICKED(ID_FILE_REFINETERRAINTEXTURETILES, OnRefineTerrainTextureTiles)
ON_BN_CLICKED(ID_FILE_EXPORTLARGEPREVIEW, OnFileExportLargePreview)
ON_COMMAND(ID_PREVIEW_APPLYLIGHTING, OnApplyLighting)
ON_UPDATE_COMMAND_UI(ID_PREVIEW_APPLYLIGHTING, OnUpdateApplyLighting)
ON_COMMAND(ID_LAYER_SETWATERLEVEL, OnSetWaterLevel)

ON_BN_CLICKED(IDC_TTS_HOLD, OnHold)
ON_BN_CLICKED(IDC_TTS_FETCH, OnFetch)
ON_BN_CLICKED(IDC_USE_LAYER, OnUseLayer)
//	ON_COMMAND(ID_OPTIONS_SETLAYERBLENDING, OnOptionsSetLayerBlending)
ON_BN_CLICKED(IDC_AUTO_GEN_MASK, OnAutoGenMask)
ON_BN_CLICKED(IDC_LOAD_MASK, OnLoadMask)
ON_BN_CLICKED(IDC_EXPORT_MASK, OnExportMask)
ON_COMMAND(ID_PREVIEW_SHOWWATER, OnShowWater)
ON_UPDATE_COMMAND_UI(ID_PREVIEW_SHOWWATER, OnUpdateShowWater)

ON_COMMAND(ID_TOOLS_CUSTOMIZEKEYBOARD, OnCustomize)
ON_COMMAND(ID_TOOLS_EXPORT_SHORTCUTS, OnExportShortcuts)
ON_COMMAND(ID_TOOLS_IMPORT_SHORTCUTS, OnImportShortcuts)

ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)

ON_NOTIFY(NM_CLICK, IDC_REPORT_CONTROL, OnReportClick)
ON_NOTIFY(NM_RCLICK, IDC_REPORT_CONTROL, OnReportRClick)
ON_NOTIFY(XTP_NM_REPORT_SELCHANGED, IDC_REPORT_CONTROL, OnReportSelChange)
ON_NOTIFY(XTP_NM_REPORT_HYPERLINK, IDC_REPORT_CONTROL, OnReportHyperlink)
ON_NOTIFY(XTP_NM_REPORT_VALUECHANGED, IDC_REPORT_CONTROL, OnReportPropertyChanged)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureDialog message handlers

BOOL CTerrainTextureDialog::OnInitDialog()
{
	m_wndTaskPanel.SetTerrainTextureDialog(this);

	/*
	   VERIFY( InitCommandBars() );
	   CMFCUtils::LoadShortcuts(GetCommandBars(), IDR_LAYER, "TerrainTextureDialog");
	   CXTPCommandBar* pMenuBar = GetCommandBars()->SetMenu( _T("Menu Bar"),IDR_LAYER );
	   pMenuBar->SetFlags(xtpFlagStretched);
	   pMenuBar->EnableCustomization(TRUE);*/

	m_pCurrentLayer = 0;

	__super::OnInitDialog();

	CWaitCursor wait;

	// Invalidate layer masks.
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		// Add the name of the layer
		GetIEditorImpl()->GetTerrainManager()->GetLayer(i)->InvalidateMask();
	}

	//InitTaskPanel();
	InitReportCtrl();

	// Load the layer list from the document
	ReloadLayerList();

	OnGeneratePreview();

	RecalcLayout();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::InitReportCtrl()
{
	CRect rc;
	ScreenToClient(rc);

	class CTTDReportPaintManager : public CXTPReportPaintManager
	{
	protected:
		int DrawLink(int* pnCurrDrawPos, XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, CXTPReportHyperlink* pHyperlink, CString strText, CRect rcLink, int nFlag)
		{
			CFont* pFont = 0;
			CFont* pOldFont = 0;
			if (pDrawArgs && pDrawArgs->pDC && (pFont = pDrawArgs->pDC->GetCurrentFont()))
			{
				if (!HFONT(newFont))
				{
					LOGFONT logFont;
					pFont->GetLogFont(&logFont);
					logFont.lfUnderline = TRUE;
					newFont.CreateFontIndirect(&logFont);
				}
				pOldFont = pDrawArgs->pDC->SelectObject(&newFont);
			}

			int nRes = CXTPReportPaintManager::DrawLink(pnCurrDrawPos, pDrawArgs, pHyperlink, strText, rcLink, nFlag);
			if (pFont)
				pDrawArgs->pDC->SelectObject(pOldFont);
			return nRes;
		}
	private:
		CFont newFont;
	};
	m_wndReport.SetPaintManager(new CTTDReportPaintManager());

	VERIFY(m_wndReport.Create(WS_CHILD | WS_TABSTOP | WS_VISIBLE | WM_VSCROLL, rc, this, IDC_REPORT_CONTROL));
	m_wndReport.ModifyStyleEx(0, WS_EX_STATICEDGE);

	//  Add sample columns
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_LAYER_NAME, _T("Layer"), 150, TRUE, XTP_REPORT_NOICON, FALSE))->SetEditable(TRUE);
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_MATERIAL, _T("Material"), 300, TRUE, XTP_REPORT_NOICON, FALSE))->SetEditable(FALSE);
	/*
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_MIN_HEIGHT, _T("Min Height"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_MAX_HEIGHT, _T("Max Height"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_MIN_ANGLE, _T("Min Angle"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_MAX_ANGLE, _T("Max Angle"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_BRIGHTNESS, _T("Brightness"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_TILING, _T("Base Tiling (test)"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_SORT_ORDER, _T("Sort order (test)"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_SPEC_AMOUNT, _T("Specular Amount (test)"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	   m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_USE_REMESH, _T("UseRemeshing"), 70, TRUE,XTP_REPORT_NOICON,FALSE))->SetAlignment(DT_CENTER);
	 */
	m_wndReport.SetMultipleSelection(TRUE);
	m_wndReport.SkipGroupsFocus(TRUE);
	m_wndReport.AllowEdit(TRUE);
	m_wndReport.EditOnClick(FALSE);

	m_wndReport.EnablePreviewMode(TRUE);

	m_wndReport.GetPaintManager()->m_clrHyper = ::GetSysColor(COLOR_HIGHLIGHT);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::InitTaskPanel()
{
	CRect rc(0, 0, 0, 0);
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// Task panel.
	//////////////////////////////////////////////////////////////////////////
	m_wndTaskPanel.Create(WS_CHILD | WS_BORDER | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, this, IDC_TASKPANEL);

	//m_taskImageList.Create( IDR_DB_GAMETOKENS_BAR,16,1,RGB(192,192,192) );
	//VERIFY( CMFCUtils::LoadTrueColorImageList( m_taskImageList,IDR_DB_GAMETOKENS_BAR,15,RGB(192,192,192) ) );
	//m_wndTaskPanel.SetImageList( &m_taskImageList );
	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	m_wndTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(TRUE);
	m_wndTaskPanel.SetAnimation(xtpTaskPanelAnimationNo);

	m_previewLayerTextureButton.Create("", WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, CRect(0, 0, 130, 130), &m_wndTaskPanel, IDC_LAYER_PREVIEW_BUTTON);
	//////////////////////////////////////////////////////////////////////////

	// Add default tasks.
	CXTPTaskPanelGroupItem* pItem = NULL;
	CXTPTaskPanelGroup* pGroup = NULL;
	{
		pGroup = m_wndTaskPanel.AddGroup(1);
		pGroup->SetCaption("Layer Tasks");

		pItem = pGroup->AddLinkItem(CMD_LAYER_ADD);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption("Add Layer");
		pItem = pGroup->AddLinkItem(CMD_LAYER_DELETE);
		pItem->SetCaption("Delete Layer");
		pItem = pGroup->AddLinkItem(CMD_LAYER_MOVEUP);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption("Move Layer Up");
		pItem = pGroup->AddLinkItem(CMD_LAYER_MOVEDOWN);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption("Move Layer Down");
		m_pAssignMaterialLink = pGroup->AddLinkItem(CMD_LAYER_ASIGN_MATERIAL);
		m_pAssignMaterialLink->SetType(xtpTaskItemTypeLink);
		m_pAssignMaterialLink->SetCaption("Assign Material");
		UpdateAssignMaterialItem();
	}

	{
		pGroup = m_wndTaskPanel.AddGroup(2);
		pGroup->SetCaption("Layer Info");
		m_pLayerInfoText = pGroup->AddTextItem("Layer Info");
	}

	{
		pGroup = m_wndTaskPanel.AddGroup(2);
		pGroup->SetCaption("Layer Texture");

		pItem = pGroup->AddLinkItem(CMD_LAYER_LOAD_TEXTURE);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem->SetCaption("Change Layer Texture");
		pItem = pGroup->AddControlItem(m_previewLayerTextureButton);
		m_pTextureInfoText = pGroup->AddTextItem("Texture Info");
	}

	{
		//////////////////////////////////////////////////////////////////////////
		pGroup = m_wndTaskPanel.AddGroup(3);
		pGroup->SetCaption("Options");
		m_showPreviewCheck.Create("Show Preview", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 60, 16), this, IDC_SHOW_PREVIEW_CHECKBOX);
		m_showPreviewCheck.SetFont(CFont::FromHandle(SMFCFonts::GetInstance().hSystemFont));
		m_showPreviewCheck.SetParent(&m_wndTaskPanel);
		m_showPreviewCheck.SetOwner(this); // Akward but needed to route msgs to this dialog
		m_showPreviewCheck.SetCheck(TRUE);
		pItem = pGroup->AddControlItem(m_showPreviewCheck);
	}
	//////////////////////////////////////////////////////////////////////////
}

BOOL CTerrainTextureDialog::CXTPTaskPanelSpecific::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam))
	{
	case IDC_SHOW_PREVIEW_CHECKBOX:
		{
			if (m_pTerrainTextureDialog)
				m_pTerrainTextureDialog->ReloadLayerList();
		}
		break;
	}
	return __super::OnCommand(wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::ReloadLayerList()
{
	LOADING_TIME_PROFILE_SECTION;
	////////////////////////////////////////////////////////////////////////
	// Fill the layer list box with the data from the document
	////////////////////////////////////////////////////////////////////////

	unsigned int i;

	//////////////////////////////////////////////////////////////////////////
	// Populate report control
	//////////////////////////////////////////////////////////////////////////
	m_wndReport.SetRedraw(FALSE);

	GeneratePreviewImageList();

	m_wndReport.GetSelectedRows()->Clear();
	m_wndReport.GetRecords()->RemoveAll();
	m_wndReport.SetRedraw(TRUE);

	for (i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
		m_wndReport.AddRecord(new CTerrainLayerRecord(pLayer, i));
	}

	m_wndReport.EnablePreviewMode(TRUE);

	m_wndReport.Populate();

	m_wndReport.GetSelectedRows()->Clear();
	int count = m_wndReport.GetRows()->GetCount();
	for (int i = 0; i < count; i++)
	{
		CTerrainLayerRecord* pRec = DYNAMIC_DOWNCAST(CTerrainLayerRecord, m_wndReport.GetRows()->GetAt(i)->GetRecord());
		if (pRec && pRec->m_pLayer && pRec->m_pLayer->IsSelected())
		{
			if (!m_pCurrentLayer)
				m_pCurrentLayer = pRec->m_pLayer;
			m_wndReport.GetSelectedRows()->Add(m_wndReport.GetRows()->GetAt(i));
		}
	}

	m_wndReport.SetRedraw(TRUE);
	//////////////////////////////////////////////////////////////////////////

	UpdateControlData();

	GetIEditorImpl()->GetTerrainManager()->ReloadSurfaceTypes();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnSelchangeLayerList()
{
	CLayer* pLayer = NULL;
	// Get the layer associated with the selection
	POSITION pos = m_wndReport.GetSelectedRows()->GetFirstSelectedRowPosition();
	while (pos)
	{
		CTerrainLayerRecord* pRec = DYNAMIC_DOWNCAST(CTerrainLayerRecord, m_wndReport.GetSelectedRows()->GetNextSelectedRow(pos)->GetRecord());
		if (!pRec)
			continue;
		pLayer = GetIEditorImpl()->GetTerrainManager()->FindLayer(pRec->m_layerName);
		if (pLayer)
			break;
	}

	// Unselect all layers.
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		GetIEditorImpl()->GetTerrainManager()->GetLayer(i)->SetSelected(false);
	}

	// Set it as the current one
	m_pCurrentLayer = pLayer;

	if (m_pCurrentLayer)
		m_pCurrentLayer->SetSelected(true);

	//	m_bMaskPreviewValid = false;

	// Update the controls with the data from the layer
	UpdateControlData();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::CompressLayers()
{
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		CLayer* layer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);

		layer->CompressMask();
	}
}

void CTerrainTextureDialog::EnableControls()
{
	////////////////////////////////////////////////////////////////////////
	// Enable / disable the current based of if at least one layer is
	// present and activated
	////////////////////////////////////////////////////////////////////////

	BOOL bEnable = m_pCurrentLayer != NULL;
	if (CMenu* pMenu = GetMenu())
	{
		if (CMenu* pSubmenu = pMenu->GetSubMenu(1))
		{
			pSubmenu->EnableMenuItem(IDC_LOAD_TEXTURE, bEnable ? MF_ENABLED : MF_GRAYED);
			pSubmenu->EnableMenuItem(ID_LAYER_EXPORTTEXTURE, bEnable ? MF_ENABLED : MF_GRAYED);
			pSubmenu->EnableMenuItem(IDC_REMOVE_LAYER, bEnable ? MF_ENABLED : MF_GRAYED);
		}
	}

	BOOL bHaveLayers = GetIEditorImpl()->GetTerrainManager()->GetLayerCount() != 0 ? TRUE : FALSE;

	/*
	   // Only enable export and generate preview option when we have layer(s)
	   GetDlgItem(IDC_EXPORT)->EnableWindow( bHaveLayers );
	   GetMenu()->GetSubMenu(0)->EnableMenuItem(IDC_EXPORT, bHaveLayers ? MF_ENABLED : MF_GRAYED);
	   GetMenu()->GetSubMenu(2)->EnableMenuItem(ID_FILE_EXPORTLARGEPREVIEW, bHaveLayers ? MF_ENABLED : MF_GRAYED);
	 */
}

void CTerrainTextureDialog::UpdateControlData()
{
	////////////////////////////////////////////////////////////////////////
	// Update the controls with the data from the current layer
	////////////////////////////////////////////////////////////////////////

	Layers layers;
	GetSelectedLayers(layers);

	if (layers.size() == 1)
	{
		CLayer* pSelLayer = layers[0];
		// Allocate the memory for the texture

		CImageEx preview;
		preview.Allocate(128, 128);
		if (pSelLayer->GetTexturePreviewImage().IsValid())
			CImageUtil::ScaleToFit(pSelLayer->GetTexturePreviewImage(), preview);
		preview.SwapRedAndBlue();
		CBitmap bmp;
		VERIFY(bmp.CreateBitmap(128, 128, 1, 32, NULL));
		bmp.SetBitmapBits(preview.GetSize(), (DWORD*)preview.GetData());
		m_previewLayerTextureButton.SetBitmap(bmp);
		/*
		   string textureInfoText;

		            textureInfoText.Format( "%s\n%i x %i LayerId %i", LPCTSTR(pSelLayer->GetTextureFilename()),
		   pSelLayer->GetTextureWidth(),pSelLayer->GetTextureHeight(), pSelLayer->GetCurrentLayerId());
		            m_pTextureInfoText->SetCaption( textureInfoText );

		            int nNumSurfaceTypes = GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypeCount();

		   string maskInfoText;
		   int maskRes = pSelLayer->GetMaskResolution();
		   maskInfoText.Format( "Layer Mask: %dx%d\nSurface Type Count %d",maskRes,maskRes,nNumSurfaceTypes );

		   m_pLayerInfoText->SetCaption( maskInfoText );
		 */
	}

	// Update the controls
	EnableControls();
}

void CTerrainTextureDialog::OnLoadTexture()
{
	if (!m_pCurrentLayer)
		return;

	GetIEditorImpl()->SetEditTool(0);
	////////////////////////////////////////////////////////////////////////
	// Load a texture from a BMP file
	////////////////////////////////////////////////////////////////////////
	string file = m_pCurrentLayer->GetTextureFilenameWithPath();

	bool res = CFileUtil::SelectSingleFile(EFILE_TYPE_TEXTURE, file);

	if (res)
	{
		CUndo undo("Load Layer Texture");
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

		// Load the texture
		if (!m_pCurrentLayer->LoadTexture(file))
			CQuestionDialog::SWarning(QObject::tr("Error"), QObject::tr("Error while loading the texture !"));

		ReloadLayerList();
	}

	// Regenerate the preview
	OnGeneratePreview();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnGeneratePreview()
{
	////////////////////////////////////////////////////////////////////////
	// Generate all layer mask and create a preview of the texture
	////////////////////////////////////////////////////////////////////////

	// Allocate the memory for the texture
	CImageEx preview;
	preview.Allocate(FINAL_TEX_PREVIEW_PRECISION_CX, FINAL_TEX_PREVIEW_PRECISION_CY);

	// Generate the terrain texture
	int tflags = ETTG_ABGR | ETTG_USE_LIGHTMAPS;
	if (m_bUseLighting)
		tflags |= ETTG_LIGHTING;
	if (m_bShowWater)
		tflags |= ETTG_SHOW_WATER;

	// Write the texture data into the bitmap
	//m_bmpFinalTexPrev.SetBitmapBits( preview.GetSize(),(DWORD*)preview.GetData() );
}

void CTerrainTextureDialog::OnFileExportLargePreview()
{
	////////////////////////////////////////////////////////////////////////
	// Show a large preview of the final texture
	////////////////////////////////////////////////////////////////////////

	if (!gEnv->p3DEngine->GetITerrain())
	{
		CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("Terrain is not presented in the current level."));
		return;
	}

	CCreateTerrainPreviewDialog::Result initial;
	initial.resolution = 1024;
	CCreateTerrainPreviewDialog dialog(initial);

	// Query the size of the preview
	if (dialog.exec() != QDialog::Accepted)
	{
		return;
	}

	auto result = dialog.GetResult();

	CryLog("Exporting large terrain texture preview (%ix%i)...",
	                     result.resolution, result.resolution);

	// Allocate the memory for the texture
	CImageEx image;
	if (!image.Allocate(result.resolution, result.resolution))
	{
		return;
	}

	// Generate the terrain texture
	int tflags = ETTG_INVALIDATE_LAYERS | ETTG_STATOBJ_SHADOWS | ETTG_BAKELIGHTING;
	if (m_bUseLighting)
		tflags |= ETTG_LIGHTING;
	if (m_bShowWater)
		tflags |= ETTG_SHOW_WATER;

	CTerrainTexGen texGen;
	bool bReturn = texGen.GenerateSurfaceTexture(tflags, image);

	if (!bReturn)
	{
		CLogFile::WriteLine("Error while generating terrain texture preview");
		CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("Can't generate terrain texture."));
		return;
	}

	unsigned int* pPixData = image.GetData(), * pPixDataEnd = &(image.GetData())[image.GetWidth() * image.GetHeight()];

	// Swap R and B
	while (pPixData != pPixDataEnd)
	{
		*pPixData++ = ((*pPixData & 0x00FF0000) >> 16) | (*pPixData & 0x0000FF00) | ((*pPixData & 0x000000FF) << 16);
	}

	// Save the texture to disk
	string temp = (const char*)gEditorFilePreferences.strStandardTempDirectory;
	string tempDirectory = PathUtil::AddBackslash(temp.GetString());
	CFileUtil::CreateDirectory(tempDirectory.GetBuffer());

	string imageName = "TexturePreview.bmp";
	bool bOk = CImageUtil::SaveImage(tempDirectory + imageName, image);
	if (bOk)
	{
		// Show the heightmap
		::ShellExecute(::GetActiveWindow(), "open", imageName.GetBuffer(), "", tempDirectory.GetBuffer(), SW_SHOWMAXIMIZED);
	}
	else
	{
		CQuestionDialog::SWarning(QObject::tr("Error"), QObject::tr("Can't save preview bitmap!"));
	}
}

/*
   bool CTerrainTextureDialog::GenerateSurface(DWORD *pSurface, UINT iWidth, UINT iHeight, int flags,CBitArray *pLightingBits, float **ppHeightmapData)
   {
   ////////////////////////////////////////////////////////////////////////
   // Generate the surface texture with the current layer and lighting
   // configuration and write the result to pSurface. Also give out the
   // results of the terrain lighting if pLightingBit is not NULL. Also,
   // if ppHeightmapData is not NULL, the allocated heightmap data will
   // be stored there instead destroing it at the end of the function
   ////////////////////////////////////////////////////////////////////////
   bool bUseLighting = flags & GEN_USE_LIGHTING;
   bool bShowWater = flags & GEN_SHOW_WATER;
   bool bShowWaterColor = flags & GEN_SHOW_WATERCOLOR;
   bool bConvertToABGR = flags & GEN_ABGR;
        bool bCalcStatObjShadows = flags & GEN_STATOBJ_SHADOWS;
        bool bKeepLayerMasks = flags & GEN_KEEP_LAYERMASKS;

   CHeightmap *heightmap = GetIEditorImpl()->GetHeightmap();
   float waterLevel = heightmap->GetWaterLevel();

   uint32 i, iTexX, iTexY;
   char szStatusBuffer[128];
   bool bGenPreviewTexture = true;
   COLORREF crlfNewCol;
   CBrush brshBlack(BLACK_BRUSH);
   int iBlend;
   CTerrainLighting cLighting;
   DWORD *pTextureDataWrite = NULL;
   CLayer *pLayerShortcut = NULL;
   int iFirstUsedLayer;
   float *pHeightmapData = NULL;

   ASSERT(iWidth);
   ASSERT(iHeight);
   ASSERT(!IsBadWritePtr(pSurface, iWidth * iHeight * sizeof(DWORD)));

   if (iWidth == 0 || iHeight == 0)
    return false;

   m_doc = GetIEditorImpl()->GetDocument();


   // Display an hourglass cursor
   BeginWaitCursor();

   CLogFile::WriteLine("Generating texture surface...");

   ////////////////////////////////////////////////////////////////////////
   // Search for the first layer that is used
   ////////////////////////////////////////////////////////////////////////

   iFirstUsedLayer = -1;

   for (i=0; i < m_doc->GetLayerCount(); i++)
   {
    // Have we founf the first used layer ?
    if (m_doc->GetLayer(i)->IsInUse())
    {
      iFirstUsedLayer = i;
      break;
    }
   }

   // Abort if there is no used layer
   if (iFirstUsedLayer == -1)
    return false;

   ////////////////////////////////////////////////////////////////////////
   // Generate the layer masks
   ////////////////////////////////////////////////////////////////////////

   // Status message
   GetIEditorImpl()->SetStatusText("Scaling heightmap...");

   // Allocate memory for the heightmap data
   pHeightmapData = new float[iWidth * iHeight];
   assert(pHeightmapData);

   // Retrieve the heightmap data
   m_doc->m_cHeightmap.GetDataEx(pHeightmapData, iWidth, true, true);

   int t0 = GetTickCount();

   bool bProgress = iWidth >= 1024;

   CWaitProgress wait( "Blending Layers",bProgress );

   CLayer *tempWaterLayer = 0;
   if (bShowWater)
   {
    // Apply water level.
    // Add a temporary water layer to the list
    tempWaterLayer = new CLayer;
    //water->LoadTexture(MAKEINTRESOURCE(IDB_WATER), 128, 128);
    tempWaterLayer->FillWithColor( m_doc->GetWaterColor(),8,8 );
    tempWaterLayer->GenerateWaterLayer16(pHeightmapData,iWidth, iHeight, waterLevel );
    m_doc->AddLayer( tempWaterLayer );
   }

   CByteImage layerMask;

   ////////////////////////////////////////////////////////////////////////
   // Generate the masks and the texture.
   ////////////////////////////////////////////////////////////////////////
   int numLayers = m_doc->GetLayerCount();
   for (i=iFirstUsedLayer; i<(int) numLayers; i++)
   {
    CLayer *layer = m_doc->GetLayer(i);

    // Skip the layer if it is not in use
    if (!layer->IsInUse())
      continue;

    if (!layer->HasTexture())
      continue;

    if (bProgress)
    {
      wait.Step( i*100/numLayers );
    }

    // Status message
    cry_sprintf(szStatusBuffer, "Updating layer %i of %i...", i + 1, m_doc->GetLayerCount());
    GetIEditorImpl()->SetStatusText(szStatusBuffer);

    // Cache surface texture in.
    layer->PrecacheTexture();

    // Generate the mask for the current layer
    if (i != iFirstUsedLayer)
    {
      CFloatImage hmap;
      hmap.Attach( pHeightmapData,iWidth,iHeight );
      // Generate a normal layer from the user's parameters, stream from disk if it exceeds a given size
      if (!layer->UpdateMask( hmap,layerMask ))
        continue;
    }
    cry_sprintf(szStatusBuffer, "Blending layer %i of %i...", i + 1, m_doc->GetLayerCount());
    GetIEditorImpl()->SetStatusText(szStatusBuffer);

    // Set the write pointer (will be incremented) for the surface data
    DWORD *pTex = pSurface;

    uint32 layerWidth = layer->GetTextureWidth();
    uint32 layerHeight = layer->GetTextureHeight();

    if (i == iFirstUsedLayer)
    {
      // Draw the first layer, without layer mask.
      for (iTexY=0; iTexY < iHeight; iTexY++)
      {
        uint32 layerY = iTexY % layerHeight;
        for (iTexX=0; iTexX < iWidth; iTexX++)
        {
          // Get the color of the tiling texture at this position
 * pTex++ = layer->GetTexturePixel( iTexX % layerWidth,layerY );
        }
      }
    }
    else
    {
      // Draw the current layer with layer mask.
      for (iTexY=0; iTexY < iHeight; iTexY++)
      {
        uint32 layerY = iTexY % layerHeight;
        for (iTexX=0; iTexX < iWidth; iTexX++)
        {
          // Scale the current preview coordinate to the layer mask and get the value.
          iBlend = layerMask.ValueAt(iTexX,iTexY);
          // Check if this pixel should be drawn.
          if (iBlend == 0)
          {
            pTex++;
            continue;
          }

          // Get the color of the tiling texture at this position
          crlfNewCol = layer->GetTexturePixel( iTexX % layerWidth,layerY );

          // Just overdraw when the opaqueness of the new layer is maximum
          if (iBlend == 255)
          {
 * pTex = crlfNewCol;
          }
          else
          {
            // Blend the layer into the existing color, iBlend is the blend factor taken from the layer
 * pTex =  (((255 - iBlend) * (*pTex & 0x000000FF)	+  (crlfNewCol & 0x000000FF)        * iBlend) >> 8)      |
              ((((255 - iBlend) * (*pTex & 0x0000FF00) >>  8) + ((crlfNewCol & 0x0000FF00) >>  8) * iBlend) >> 8) << 8 |
              ((((255 - iBlend) * (*pTex & 0x00FF0000) >> 16) + ((crlfNewCol & 0x00FF0000) >> 16) * iBlend) >> 8) << 16;
          }
          pTex++;
        }
      }
    }

    if (!bKeepLayerMasks)
    {
      layer->ReleaseTempResources();
    }
   }

   if (tempWaterLayer)
   {
    m_doc->RemoveLayer(tempWaterLayer);
   }

   int t1 = GetTickCount();
   CryLog( "Texture surface layers blended in %dms",t1-t0 );

   if (bProgress)
    wait.Stop();

   //	string str;
   //	str.Format( "Time %dms",t1-t0 );
   //	MessageBox( str,"Time",MB_OK );

   ////////////////////////////////////////////////////////////////////////
   // Light the texture
   ////////////////////////////////////////////////////////////////////////

   if (bUseLighting)
   {
    CByteImage *shadowMap = 0;
    if (bCalcStatObjShadows)
    {
      CLogFile::WriteLine("Generating shadows of static objects..." );
      GetIEditorImpl()->SetStatusText( "Generating shadows of static objects..." );
      shadowMap = new CByteImage;
      if (!shadowMap->Allocate( iWidth,iHeight ))
      {
        delete shadowMap;
        return false;
      }
      shadowMap->Clear();
      float shadowAmmount = 255.0f*m_doc->GetLighting()->iShadowIntensity/100.0f;
      Vec3 sunVector = m_doc->GetLighting()->GetSunVector();

      GetIEditorImpl()->GetVegetationMap()->GenerateShadowMap( *shadowMap,shadowAmmount,sunVector );
    }

    CLogFile::WriteLine("Generating Terrain Lighting..." );
    GetIEditorImpl()->SetStatusText( "Generating Terrain Lighting..." );


    // Calculate the lighting. Fucntion will also use pLightingBits if present
   //		cLighting.LightArray16(iWidth, iHeight, pSurface, pHeightmapData,
   //			m_doc->GetLighting(), pLightingBits,shadowMap );

    if (shadowMap)
      delete shadowMap;

    int t2 = GetTickCount();
    CryLog( "Texture lighted in %dms",t2-t1 );
   }

   // After lighting add Colored Water layer.
   if (bShowWaterColor)
   {
    // Apply water level.
    // Add a temporary water layer to the list
    CLayer *water = new CLayer;
    //water->LoadTexture(MAKEINTRESOURCE(IDB_WATER), 128, 128);
    water->FillWithColor( m_doc->GetWaterColor(),128,128 );
    water->GenerateWaterLayer16(pHeightmapData,iWidth, iHeight, waterLevel );

    // Set the write pointer (will be incremented) for the surface data
    DWORD *pTex = pSurface;

    uint32 layerWidth = water->GetTextureWidth();
    uint32 layerHeight = water->GetTextureHeight();

    // Draw the first layer, without layer mask.
    for (iTexY=0; iTexY < iHeight; iTexY++)
    {
      uint32 layerY = iTexY % layerHeight;
      for (iTexX=0; iTexX < iWidth; iTexX++)
      {
        // Get the color of the tiling texture at this position
        if (water->GetLayerMaskPoint(iTexX,iTexY) > 0)
        {
 * pTex = water->GetTexturePixel( iTexX % layerWidth,layerY );
        }
        pTex++;
      }
    }
    delete water;
   }


   if (bConvertToABGR)
   {
    GetIEditorImpl()->SetStatusText( "Convert surface texture to ABGR..." );
    // Set the write pointer (will be incremented) for the surface data
    pTextureDataWrite = pSurface;
    for (iTexY=0; iTexY<(int) iHeight; iTexY++)
    {
      for (iTexX=0; iTexX<(int) iWidth; iTexX++)
      {
 * pTextureDataWrite++ = ((* pTextureDataWrite & 0x00FF0000) >> 16) |
                                (* pTextureDataWrite & 0x0000FF00) |
                                ((* pTextureDataWrite & 0x000000FF) << 16);
      }
    }
   }


   ////////////////////////////////////////////////////////////////////////
   // Finished
   ////////////////////////////////////////////////////////////////////////

   // Should we return or free the heightmap data ?
   if (ppHeightmapData)
   {
 * ppHeightmapData = pHeightmapData;
    pHeightmapData = NULL;
   }
   else
   {
    // Free the heightmap data
    delete [] pHeightmapData;
    pHeightmapData = NULL;
   }

   // We are finished with the calculations
   EndWaitCursor();
   GetIEditorImpl()->SetStatusText("Ready");

   int t2 = GetTickCount();
   CryLog( "Texture surface generate in %dms",t2-t0 );

   return true;
   }
 */

void CTerrainTextureDialog::OnImport()
{
	////////////////////////////////////////////////////////////////////////
	// Import layer settings from a file
	////////////////////////////////////////////////////////////////////////

	const QDir dir(QtUtil::GetAppDataFolder());

	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = dir.absolutePath();
	runParams.title = QObject::tr("Import Layers");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("Layer Files (lay)"), "lay");

	QString fileName = CSystemFileDialog::RunImportFile(runParams, nullptr);
	string path = fileName.toStdString().c_str();

	if (!fileName.isEmpty())
	{
		CryLog("Importing layer settings from %s", path.GetString());

		CUndo undo("Import Texture Layers");
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

		CXmlArchive ar;
		if (ar.Load(path))
		{
			GetIEditorImpl()->GetTerrainManager()->SerializeSurfaceTypes(ar);
			GetIEditorImpl()->GetTerrainManager()->SerializeLayerSettings(ar);
		}

		// Load the layers into the dialog
		ReloadLayerList();
	}
}

void CTerrainTextureDialog::OnRefineTerrainTextureTiles()
{
	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr("Error"), QObject::tr("Refine TerrainTexture?\r\n"
	                                                                                                          "(all terrain texture tiles become split in 4 parts so a tile with 2048x2048\r\n"
	                                                                                                          "no longer limits the resolution) You need to save afterwards!")))
	{
		if (!GetIEditorImpl()->GetTerrainManager()->GetRGBLayer()->RefineTiles())
		{
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("TerrainTexture refine failed (make sure current data is saved)"));
		}
		else
		{
			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Successfully refined TerrainTexture - Save is now required!"));
		}
	}
}

void CTerrainTextureDialog::OnExport()
{
	////////////////////////////////////////////////////////////////////////
	// Export layer settings to a file
	////////////////////////////////////////////////////////////////////////

	if (0 >= GetIEditorImpl()->GetTerrainManager()->GetLayerCount())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "No layers exist. You have to create some Terrain Layers first.");
		CQuestionDialog::SWarning(QObject::tr("No Layers"), QObject::tr("No layers exist. You have to create some Terrain Layers first."));
		return;
	}

	const QDir dir(QtUtil::GetAppDataFolder());

	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = dir.absolutePath();
	runParams.title = QObject::tr("Export Layers");
	runParams.extensionFilters << CExtensionFilter(QObject::tr("Layer Files (lay)"), "lay");

	const QString filePath = CSystemFileDialog::RunExportFile(runParams, nullptr);
	string path = filePath.toStdString().c_str();

	if (!filePath.isEmpty())
	{
		CryLog("Exporting layer settings to %s", path);

		CXmlArchive ar("LayerSettings");
		GetIEditorImpl()->GetTerrainManager()->SerializeSurfaceTypes(ar);
		GetIEditorImpl()->GetTerrainManager()->SerializeLayerSettings(ar);
		ar.Save(path);
	}
}

void CTerrainTextureDialog::OnApplyLighting()
{
	////////////////////////////////////////////////////////////////////////
	// Toggle between the on / off for the apply lighting state
	////////////////////////////////////////////////////////////////////////

	m_bUseLighting = !m_bUseLighting;
	OnGeneratePreview();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnUpdateApplyLighting(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bUseLighting ? TRUE : FALSE);
}

void CTerrainTextureDialog::OnShowWater()
{
	////////////////////////////////////////////////////////////////////////
	// Toggle between the on / off for the show water state
	////////////////////////////////////////////////////////////////////////

	m_bShowWater = !m_bShowWater;
	OnGeneratePreview();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnUpdateShowWater(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bShowWater ? TRUE : FALSE);
}

void CTerrainTextureDialog::OnSetWaterLevel()
{
	////////////////////////////////////////////////////////////////////////
	// Let the user change the current water level
	////////////////////////////////////////////////////////////////////////

	// the dialog
	QNumericBoxDialog dlg(QObject::tr("Set Water Level"), GetIEditorImpl()->GetHeightmap()->GetWaterLevel());

	// Show the dialog
	if (dlg.exec() == QDialog::Accepted)
	{
		// Retrieve the new water level from the dialog and save it in the document
		GetIEditorImpl()->GetHeightmap()->SetWaterLevel(dlg.GetValue());

		// We modified the document
		GetIEditorImpl()->SetModifiedFlag();

		OnGeneratePreview();
	}
}

void CTerrainTextureDialog::OnHold()
{
	////////////////////////////////////////////////////////////////////////
	// Make a temporary save of the current layer state
	////////////////////////////////////////////////////////////////////////
	string strFilename(gEditorFilePreferences.strStandardTempDirectory);

	strFilename = PathUtil::AddBackslash(strFilename.GetString());
	strFilename += "HoldStateTemp.lay";

	CFileUtil::CreateDirectory(gEditorFilePreferences.strStandardTempDirectory.c_str());
	CXmlArchive ar("LayerSettings");
	GetIEditorImpl()->GetTerrainManager()->SerializeLayerSettings(ar);
	ar.Save(strFilename);
}

void CTerrainTextureDialog::OnFetch()
{
	////////////////////////////////////////////////////////////////////////
	// Load a previous save of the layer state
	////////////////////////////////////////////////////////////////////////

	string strFilename(gEditorFilePreferences.strStandardTempDirectory);

	strFilename = PathUtil::AddBackslash(strFilename.GetString());
	strFilename += "HoldStateTemp.lay";

	if (!PathFileExists(strFilename.GetBuffer()))
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("You have to use 'Hold' before using 'Fetch' !"));
		return;
	}

	if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("The document contains unsaved data, really fetch old state ?")))
	{
		return;
	}

	CUndo undo("Fetch Layers");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

	CXmlArchive ar;
	ar.Load(strFilename.GetBuffer());
	GetIEditorImpl()->GetTerrainManager()->SerializeLayerSettings(ar);

	// Load the layers into the dialog
	ReloadLayerList();
}

void CTerrainTextureDialog::OnUseLayer()
{
	////////////////////////////////////////////////////////////////////////
	// Click on the 'Use' checkbox of the current layer
	////////////////////////////////////////////////////////////////////////
	if (!m_pCurrentLayer)
		return;

	CButton ctrlButton;

	ASSERT(!IsBadReadPtr(m_pCurrentLayer, sizeof(CLayer)));

	// Change the internal in use value of the selected layer
	VERIFY(ctrlButton.Attach(GetDlgItem(IDC_USE_LAYER)->m_hWnd));
	m_pCurrentLayer->SetInUse((ctrlButton.GetCheck() == 1) ? true : false);
	ctrlButton.Detach();

	// Regenerate the preview
	OnGeneratePreview();
}

void CTerrainTextureDialog::OnAutoGenMask()
{
	if (!m_pCurrentLayer)
		return;

	CUndo undo("Layer Autogen");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

	m_pCurrentLayer->SetAutoGen(!m_pCurrentLayer->IsAutoGen());
	UpdateControlData();

	// Regenerate the preview
	OnGeneratePreview();
}

void CTerrainTextureDialog::OnLoadMask()
{
	if (!m_pCurrentLayer)
		return;

	string file;
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_TEXTURE, file))
	{
		CUndo undo("Load Layer Mask");
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

		// Load the texture
		if (!m_pCurrentLayer->LoadMask(file))
			CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("Error while loading the texture !"));

		// Update the texture information files
		UpdateControlData();
	}

	// Regenerate the preview
	OnGeneratePreview();
}

void CTerrainTextureDialog::OnExportMask()
{
	if (!m_pCurrentLayer)
		return;

	// Export current layer mask to bitmap.
	string filename;
	if (CFileUtil::SelectSaveFile(SUPPORTED_IMAGES_FILTER, "bmp", "", filename))
	{
		// Tell the layer to export its mask.
		m_pCurrentLayer->ExportMask(filename);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnLayersNewItem()
{
	CUndo undo("New Terrain Layer");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

	// Add the layer
	CLayer* pNewLayer = new CLayer;
	pNewLayer->SetLayerName("NewLayer");
	pNewLayer->LoadTexture("%ENGINE%/EngineAssets/Textures/white.dds");
	pNewLayer->AssignMaterial("%ENGINE%/EngineAssets/Materials/material_terrain_default");
	pNewLayer->GetOrRequestLayerId();

	GetIEditorImpl()->GetTerrainManager()->AddLayer(pNewLayer);

	ReloadLayerList();

	SelectLayer(pNewLayer);

	// Update the controls with the data from the layer
	UpdateControlData();

	// Regenerate the preview
	OnGeneratePreview();

	m_bIgnoreNotify = true;
	GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
	GetIEditorImpl()->Notify(eNotify_OnTextureLayerChange);
	m_bIgnoreNotify = false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnLayersDeleteItem()
{
	if (!m_pCurrentLayer)
	{
		MessageBox("No target layers selected");
		return;
	}

	CUndo undo("Delete Terrain Layer");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

	CLayer* pLayerToDelete = m_pCurrentLayer;

	SelectLayer(0, true);

	// Find the layer inside the layer list in the document and remove it.
	GetIEditorImpl()->GetTerrainManager()->RemoveLayer(pLayerToDelete);

	ReloadLayerList();

	// Regenerate the preview
	OnGeneratePreview();

	m_bIgnoreNotify = true;
	GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
	GetIEditorImpl()->Notify(eNotify_OnTextureLayerChange);
	m_bIgnoreNotify = false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnLayersMoveItemUp()
{
	CUndo undo("Move Terrain Layer Up");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

	if (!m_pCurrentLayer)
		return;

	CLayer* pLayer = m_pCurrentLayer;

	int nIndexCur = -1;
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		if (GetIEditorImpl()->GetTerrainManager()->GetLayer(i) == m_pCurrentLayer)
		{
			nIndexCur = i;
			break;
		}
	}

	if (nIndexCur < 1)
		return;

	GetIEditorImpl()->GetTerrainManager()->SwapLayers(nIndexCur, nIndexCur - 1);

	ReloadLayerList();

	SelectLayer(pLayer, true);

	m_bIgnoreNotify = true;
	GetIEditorImpl()->Notify(eNotify_OnTextureLayerChange);
	m_bIgnoreNotify = false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnLayersMoveItemDown()
{
	CUndo undo("Move Terrain Layer Down");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

	if (!m_pCurrentLayer)
		return;

	CLayer* pLayer = m_pCurrentLayer;

	int nIndexCur = -1;
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		if (GetIEditorImpl()->GetTerrainManager()->GetLayer(i) == m_pCurrentLayer)
		{
			nIndexCur = i;
			break;
		}
	}

	if (nIndexCur < 0 || nIndexCur >= GetIEditorImpl()->GetTerrainManager()->GetLayerCount() - 1)
		return;

	GetIEditorImpl()->GetTerrainManager()->SwapLayers(nIndexCur, nIndexCur + 1);

	ReloadLayerList();

	SelectLayer(pLayer, true);

	m_bIgnoreNotify = true;
	GetIEditorImpl()->Notify(eNotify_OnTextureLayerChange);
	m_bIgnoreNotify = false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnDuplicateItem()
{
	if (!m_pCurrentLayer)
	{
		MessageBox("No target layers selected");
		return;
	}

	CUndo undo("Duplicate Terrain Layer");
	GetIEditor()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

	CLayer* pNewLayer = m_pCurrentLayer->Duplicate();

	GetIEditorImpl()->GetTerrainManager()->AddLayer(pNewLayer);

	ReloadLayerList();

	SelectLayer(pNewLayer);

	// Update the controls with the data from the layer
	UpdateControlData();

	// Regenerate the preview
	OnGeneratePreview();

	m_bIgnoreNotify = true;
	GetIEditor()->Notify(eNotify_OnInvalidateControls);
	GetIEditor()->Notify(eNotify_OnTextureLayerChange);
	m_bIgnoreNotify = false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::GeneratePreviewImageList()
{
	if (m_imageList.GetSafeHandle())
		m_imageList.DeleteImageList();
	VERIFY(m_imageList.Create(TEXTURE_PREVIEW_SIZE, TEXTURE_PREVIEW_SIZE, ILC_COLOR32, 0, 1));

	// Allocate the memory for the texture
	CImageEx preview;
	preview.Allocate(TEXTURE_PREVIEW_SIZE, TEXTURE_PREVIEW_SIZE);
	preview.Fill(128);

	CBitmap bmp;
	VERIFY(bmp.CreateBitmap(TEXTURE_PREVIEW_SIZE, TEXTURE_PREVIEW_SIZE, 1, 32, NULL));

	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);

		CImageEx& previewImage = pLayer->GetTexturePreviewImage();
		if (previewImage.IsValid())
			CImageUtil::ScaleToFit(previewImage, preview);
		preview.SwapRedAndBlue();

		// Write the texture data into the bitmap
		bmp.SetBitmapBits(preview.GetSize(), (DWORD*)preview.GetData());

		int nRes = m_imageList.Add(&bmp, RGB(255, 0, 255));
	}

	m_wndReport.SetImageList(&m_imageList);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::RecalcLayout()
{
	if (m_wndTaskPanel.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		/*
		   rc.top += 30;

		   CRect rcTask(rc);
		   rcTask.right = 200;
		   rc.left = rcTask.right+1;
		   m_wndTaskPanel.MoveWindow(rcTask);
		 */
	}

	if (m_wndReport)
	{
		CRect rc;
		GetClientRect(rc);
		m_wndReport.MoveWindow(rc);
	}
}

//////////////////////////////////////////////////////////////////////////
LRESULT CTerrainTextureDialog::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			switch (nCmdID)
			{
			case CMD_LAYER_ADD:
				OnLayersNewItem();
				break;
			case CMD_LAYER_DELETE:
				OnLayersDeleteItem();
				break;
			case CMD_LAYER_MOVEUP:
				OnLayersMoveItemUp();
				break;
			case CMD_LAYER_MOVEDOWN:
				OnLayersMoveItemDown();
				break;
			case CMD_LAYER_LOAD_TEXTURE:
				OnLoadTexture();
				break;
			case CMD_LAYER_ASIGN_MATERIAL:
				OnAssignMaterial();
				break;
			case CMD_OPEN_MATERIAL_EDITOR:
				GetIEditorImpl()->OpenView("Material Editor");
				break;
			}
		}
		break;

	case XTP_TPN_RCLICK:
		break;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainTextureDialog::GetSelectedLayers(Layers& layers)
{
	layers.clear();
	POSITION pos = m_wndReport.GetSelectedRows()->GetFirstSelectedRowPosition();
	while (pos)
	{
		CTerrainLayerRecord* pRec = DYNAMIC_DOWNCAST(CTerrainLayerRecord, m_wndReport.GetSelectedRows()->GetNextSelectedRow(pos)->GetRecord());
		if (pRec && pRec->m_pLayer)
			layers.push_back(pRec->m_pLayer);
	}
	return !layers.empty();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnReportSelChange(NMHDR* pNotifyStruct, LRESULT* result)
{
	CLayer* pLayer = NULL;

	Layers layers;
	if (GetSelectedLayers(layers))
		pLayer = layers[0];

	if (pLayer != m_pCurrentLayer)
		SelectLayer(pLayer, true);

	*result = 0;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnReportKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNMHDR;
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnReportClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->pColumn && pItemNotify->pColumn->GetIndex() == 0)
	{

	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnReportRClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	*result = 0;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->nHyperlink >= 0)
	{
		CTerrainLayerRecord* pRecord = DYNAMIC_DOWNCAST(CTerrainLayerRecord, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			if (pItemNotify->pColumn->GetIndex() == COLUMN_MATERIAL)
			{
				string mtlName;
				CSurfaceType* pSurfaceType = pRecord->m_pLayer->GetSurfaceType();
				if (pSurfaceType)
				{
					mtlName = pSurfaceType->GetMaterial();
				}

				IMaterial* pMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(mtlName, false);
				if (pMtl)
				{
					GetIEditorImpl()->GetMaterialManager()->GotoMaterial(pMtl);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::SelectLayer(CLayer* pLayer, bool bSelectUI)
{
	// Unselect all layers.
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		GetIEditorImpl()->GetTerrainManager()->GetLayer(i)->SetSelected(false);
	}

	// Set it as the current one
	m_pCurrentLayer = pLayer;

	if (m_pCurrentLayer)
		m_pCurrentLayer->SetSelected(true);

	//	m_bMaskPreviewValid = false;

	if (bSelectUI)
	{
		m_wndReport.GetSelectedRows()->Clear();
		// Get the layer associated with the selection
		int count = m_wndReport.GetRows()->GetCount();
		for (int i = 0; i < count; i++)
		{
			CTerrainLayerRecord* pRec = DYNAMIC_DOWNCAST(CTerrainLayerRecord, m_wndReport.GetRows()->GetAt(i)->GetRecord());
			if (pRec && pRec->m_pLayer && pRec->m_pLayer->IsSelected())
			{
				m_wndReport.GetSelectedRows()->Add(m_wndReport.GetRows()->GetAt(i));
				break;
			}
		}

		// Update the controls with the data from the layer
		UpdateControlData();
	}

	m_bIgnoreNotify = true;
	GetIEditorImpl()->Notify(eNotify_OnSelectionChange);
	m_bIgnoreNotify = false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnReportPropertyChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	CUndo undo("Texture Layer Change");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

	CTerrainLayerRecord* pRec = DYNAMIC_DOWNCAST(CTerrainLayerRecord, pItemNotify->pItem->GetRecord());
	if (!pRec)
		return;
	CLayer* pLayer = pRec->m_pLayer;
	if (!pLayer)
		return;

	if (pItemNotify->pColumn->GetIndex() == COLUMN_LAYER_NAME)
	{
		string newLayerName = ((CXTPReportRecordItemText*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerName(newLayerName);
	}
	if (pItemNotify->pColumn->GetIndex() == COLUMN_MIN_HEIGHT)
	{
		double value = ((CXTPReportRecordItemNumber*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerStart(value);
	}
	if (pItemNotify->pColumn->GetIndex() == COLUMN_MAX_HEIGHT)
	{
		double value = ((CXTPReportRecordItemNumber*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerEnd(value);
	}
	if (pItemNotify->pColumn->GetIndex() == COLUMN_MIN_ANGLE)
	{
		double value = ((CXTPReportRecordItemNumber*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerMinSlopeAngle(value);
	}
	if (pItemNotify->pColumn->GetIndex() == COLUMN_MAX_ANGLE)
	{
		double value = ((CXTPReportRecordItemNumber*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerMaxSlopeAngle(value);
	}
	if (pItemNotify->pColumn->GetIndex() == COLUMN_TILING)
	{
		double value = ((CXTPReportRecordItemNumber*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerTiling(value);
	}
	if (pItemNotify->pColumn->GetIndex() == COLUMN_SORT_ORDER)
	{
		double value = ((CXTPReportRecordItemNumber*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerSortOrder(value);
	}
	if (pItemNotify->pColumn->GetIndex() == COLUMN_SPEC_AMOUNT)
	{
		double value = ((CXTPReportRecordItemNumber*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerSpecularAmount(value);
	}
	if (pItemNotify->pColumn->GetIndex() == COLUMN_USE_REMESH)
	{
		double value = ((CXTPReportRecordItemNumber*)pItemNotify->pItem)->GetValue();
		pRec->m_pLayer->SetLayerUseRemeshing(value);
	}

	m_bIgnoreNotify = true;
	GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
	m_bIgnoreNotify = false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnAssignMaterial()
{
	Layers layers;
	if (GetSelectedLayers(layers))
	{
		CUndo undo("Assign Layer Material");
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);

		CMaterial* pMaterial = GetIEditorImpl()->GetMaterialManager()->GetCurrentMaterial();
		assert(pMaterial != NULL);
		if (pMaterial == NULL)
		{
			return;
		}

		for (int i = 0; i < layers.size(); i++)
		{
			layers[i]->AssignMaterial(pMaterial->GetName());
		}
	}
	else
	{
		MessageBox("No target layers selected");
	}
	ReloadLayerList();

	GetIEditorImpl()->GetTerrainManager()->ReloadSurfaceTypes();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	if (event == EDB_ITEM_EVENT_SELECTED)
	{
		UpdateAssignMaterialItem();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::UpdateAssignMaterialItem()
{
	if (m_pAssignMaterialLink == NULL)
	{
		return;
	}

	CMaterial* pMaterial = GetIEditorImpl()->GetMaterialManager()->GetCurrentMaterial();
	bool materialSelected = (pMaterial != NULL);

	m_pAssignMaterialLink->SetEnabled(materialSelected);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (m_bIgnoreNotify)
		return;
	switch (event)
	{
	case eNotify_OnBeginNewScene:
	case eNotify_OnBeginSceneOpen:
		ClearData();
		break;
	case eNotify_OnEndNewScene:
	case eNotify_OnEndSceneOpen:
		ReloadLayerList();
		break;

	case eNotify_OnSelectionChange:
		{
			for (int i = 0, cnt = GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i < cnt; ++i)
			{
				CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
				if (pLayer && pLayer->IsSelected())
				{
					if (pLayer != m_pCurrentLayer)
						SelectLayer(pLayer);
					break;
				}
			}
		}
		break;

	case eNotify_OnInvalidateControls:
		{
			CLayer* pLayer = 0;
			for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
			{
				if (GetIEditorImpl()->GetTerrainManager()->GetLayer(i)->IsSelected())
				{
					pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
					break;
				}
			}

			if (pLayer)
			{
				int count = m_wndReport.GetRows()->GetCount();
				for (int i = 0; i < count; i++)
				{
					CTerrainLayerRecord* pRec = DYNAMIC_DOWNCAST(CTerrainLayerRecord, m_wndReport.GetRows()->GetAt(i)->GetRecord());
					if (pRec && pRec->m_pLayer == pLayer)
					{
						((CXTPReportRecordItemNumber*)(pRec->GetItem(COLUMN_MIN_HEIGHT)))->SetValue(pLayer->GetLayerStart());
						((CXTPReportRecordItemNumber*)(pRec->GetItem(COLUMN_MAX_HEIGHT)))->SetValue(pLayer->GetLayerEnd());
						((CXTPReportRecordItemNumber*)(pRec->GetItem(COLUMN_MIN_ANGLE)))->SetValue(pLayer->GetLayerMinSlopeAngle());
						((CXTPReportRecordItemNumber*)(pRec->GetItem(COLUMN_MAX_ANGLE)))->SetValue(pLayer->GetLayerMaxSlopeAngle());

						m_wndReport.GetSelectedRows()->Clear();
						// Get the layer associated with the selection
						int count = m_wndReport.GetRows()->GetCount();
						for (int i = 0; i < count; i++)
						{
							CTerrainLayerRecord* pRec = DYNAMIC_DOWNCAST(CTerrainLayerRecord, m_wndReport.GetRows()->GetAt(i)->GetRecord());
							if (pRec && pRec->m_pLayer && pRec->m_pLayer->IsSelected())
							{
								m_wndReport.GetSelectedRows()->Add(m_wndReport.GetRows()->GetAt(i));
								break;
							}
						}
						break;
					}
				}
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnCustomize()
{
	CMFCUtils::ShowShortcutsCustomizeDlg(GetCommandBars(), IDR_LAYER, "TerrainTextureDialog");
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnExportShortcuts()
{
	CMFCUtils::ExportShortcuts(GetCommandBars()->GetShortcutManager());
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureDialog::OnImportShortcuts()
{
	CMFCUtils::ImportShortcuts(GetCommandBars()->GetShortcutManager(), "TerrainTextureDialog");
}

void CTerrainTextureDialog::StoreLayersUndo()
{
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersUndoObject);
}

