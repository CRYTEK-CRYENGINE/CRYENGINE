// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Dialogs/ToolbarDialog.h"
#include "IDataBaseManager.h"

// forward declarations.
class CLayer;

// Internal resolution of the final texture preview
#define FINAL_TEX_PREVIEW_PRECISION_CX 256
#define FINAL_TEX_PREVIEW_PRECISION_CY 256

// Hold / fetch temp file
//#define HOLD_FETCH_FILE_TTS "Temp\\HoldStateTemp.lay"

/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureDialog dialog

enum SurfaceGenerateFlags
{
	GEN_USE_LIGHTING    = 1,
	GEN_SHOW_WATER      = 1 << 2,
	GEN_SHOW_WATERCOLOR = 1 << 3,
	GEN_KEEP_LAYERMASKS = 1 << 4,
	GEN_ABGR            = 1 << 5,
	GEN_STATOBJ_SHADOWS = 1 << 6
};

class CTerrainTextureDialog : public CToolbarDialog, public IEditorNotifyListener, public IDataBaseManagerListener
{
	DECLARE_DYNCREATE(CTerrainTextureDialog)

public:
	void ReloadLayerList();

	CTerrainTextureDialog(CWnd* pParent = NULL);   // standard constructor
	~CTerrainTextureDialog();

	static void OnUndoUpdate();
	static void StoreLayersUndo();

	enum { IDD = IDD_DATABASE };
	typedef std::vector<CLayer*> Layers;

	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener implementation
	//////////////////////////////////////////////////////////////////////////
	// Called by the editor to notify the listener about the specified event.
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	//////////////////////////////////////////////////////////////////////////

	// IDataBaseManagerListener
	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event);
	// ~IDataBaseManagerListener

	void OnAssignMaterial();

protected:

	class CXTPTaskPanelSpecific : public CXTPTaskPanel
	{
	public:
		CXTPTaskPanelSpecific() : CXTPTaskPanel(), m_pTerrainTextureDialog(0){}
		~CXTPTaskPanelSpecific(){};

		void SetTerrainTextureDialog(CTerrainTextureDialog* pTerrainTextureDialog) { m_pTerrainTextureDialog = pTerrainTextureDialog; }
		BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	private:
		CTerrainTextureDialog* m_pTerrainTextureDialog;
	};

	virtual void OnOK()     {};
	virtual void OnCancel() {};

	void         ClearData();

	// Dialog control related functions
	void UpdateControlData();
	void EnableControls();
	void CompressLayers();

	void InitReportCtrl();
	void InitTaskPanel();
	void GeneratePreviewImageList();
	void RecalcLayout();
	void SelectLayer(CLayer* pLayer, bool bSelectUI = true);

	// Assign selected material to the selected layers.
	void UpdateAssignMaterialItem();
	void SelectedLayerChanged(CLayer* pLayer);
	bool GetSelectedLayers(Layers& layers);

	// Generated message map functions
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg void    OnSelchangeLayerList();
	afx_msg void    OnLoadTexture();
	afx_msg void    OnImport();
	afx_msg void    OnExport();
	afx_msg void    OnFileExportLargePreview();
	afx_msg void    OnGeneratePreview();
	afx_msg void    OnApplyLighting();
	afx_msg void    OnUpdateApplyLighting(CCmdUI* pCmdUI);
	afx_msg void    OnSetWaterLevel();
	afx_msg void    OnLayerExportTexture();
	afx_msg void    OnHold();
	afx_msg void    OnFetch();
	afx_msg void    OnUseLayer();
	//afx_msg void OnOptionsSetLayerBlending();
	afx_msg void    OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void    OnAutoGenMask();
	afx_msg void    OnLoadMask();
	afx_msg void    OnExportMask();
	afx_msg void    OnShowWater();
	afx_msg void    OnUpdateShowWater(CCmdUI* pCmdUI);
	afx_msg void    OnRefineTerrainTextureTiles();

	afx_msg void    OnLayersNewItem();
	afx_msg void    OnLayersDeleteItem();
	afx_msg void    OnLayersMoveItemUp();
	afx_msg void    OnLayersMoveItemDown();
	afx_msg void    OnDuplicateItem();

	afx_msg void    OnCustomize();
	afx_msg void    OnExportShortcuts();
	afx_msg void    OnImportShortcuts();

	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);

	afx_msg void    OnReportClick(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void    OnReportRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void    OnReportSelChange(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void    OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void    OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void    OnReportPropertyChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/);

	afx_msg void    PostNcDestroy();

private:

	CXTPTaskPanelSpecific m_wndTaskPanel;

	// The currently active layer (syncronized with the list box selection)
	CLayer* m_pCurrentLayer;

	// Apply lighting to the previews ?
	static bool m_bUseLighting;

	// Show water in the preview ?
	static bool             m_bShowWater;

	CXTPReportControl       m_wndReport;

	CImageList              m_imageList;

	CButton                 m_showPreviewCheck;
	CStatic                 m_previewLayerTextureButton;
	CXTPTaskPanelGroupItem* m_pTextureInfoText;
	CXTPTaskPanelGroupItem* m_pLayerInfoText;
	CXTPTaskPanelGroupItem* m_pAssignMaterialLink;

	bool                    m_bIgnoreNotify;
};

