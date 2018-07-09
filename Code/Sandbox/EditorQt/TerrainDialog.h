// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Dialogs/BaseFrameWnd.h"
#include "Controls/RollupCtrl.h"

struct SNoiseParams;
class CHeightmap;
class CTopRendererWnd;

/////////////////////////////////////////////////////////////////////////////
// CTerrainDialog dialog
//TODO : This class should be entirely deleted, the UI is not used, but it should be made into a terrain manager 
class CTerrainDialog : public CBaseFrameWnd, public IEditorNotifyListener
{
	DECLARE_DYNCREATE(CTerrainDialog)

public:
	CTerrainDialog();
	~CTerrainDialog();

	SNoiseParams* GetLastParam() { return m_sLastParam; };

	enum { IDD = IDD_TERRAIN };

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

public:
	void  Flatten(float fFactor);
	float ExpCurve(float v, unsigned int iCover, float fSharpness);

	void  InvalidateViewport();
	void  InvalidateTerrain();

	// CBaseFrameWnd implementation
	virtual LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);

	afx_msg LRESULT OnKickIdle(WPARAM wParam, LPARAM);
	
	virtual BOOL OnInitDialog();
	afx_msg void OnTerrainLoad();
	afx_msg void OnTerrainErase();
	afx_msg void OnTerrainResizeUpdateUI(CCmdUI* pCmdUI);
	afx_msg void OnTerrainLight();
	afx_msg void OnTerrainSurface();
	afx_msg void OnTerrainGenerate();
	afx_msg void OnTerrainInvert();
	afx_msg void OnExportHeightmap();
	afx_msg void OnModifyMakeisle();
	afx_msg void OnModifyFlattenLight();
	afx_msg void OnModifyFlattenHeavy();
	afx_msg void OnModifySmooth();
	afx_msg void OnModifyRemovewater();
	afx_msg void OnModifySmoothSlope();
	afx_msg void OnHeightmapShowLargePreview();
	afx_msg void OnModifySmoothBeachesOrCoast();
	afx_msg void OnModifyNormalize();
	afx_msg void OnModifyReduceRange();
	afx_msg void OnModifyReduceRangeLight();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnHold();
	afx_msg void OnFetch();
	afx_msg void OnOptionsShowMapObjects();
	afx_msg void OnOptionsShowWater();
	afx_msg void OnOptionsEditTerrainCurve();
	afx_msg void OnOptionsShowGrid();
	afx_msg void OnSetWaterLevel();
	afx_msg void OnSetMaxHeight();
	afx_msg void OnSetUnitSize();
	afx_msg void OnShowWaterUpdateUI(CCmdUI* pCmdUI);
	afx_msg void OnShowMapObjectsUpdateUI(CCmdUI* pCmdUI);
	afx_msg void OnShowGridUpdateUI(CCmdUI* pCmdUI);
	afx_msg void OnCustomize();
	afx_msg void OnExportShortcuts();
	afx_msg void OnImportShortcuts();

	DECLARE_MESSAGE_MAP()

public:
	void GenerateTerrainTexture();
	void OnResizeTerrain();
	void OnTerrainImportBlock();
	void OnTerrainExportBlock();
	void OnReloadTerrain();
	void OnExportTerrainAreaWithObjects();
	void OnExportTerrainArea();
	void TerrainTextureExport();

private:
	SNoiseParams*    m_sLastParam;
	CHeightmap*      m_pHeightmap;

	CXTPStatusBar    m_wndStatusBar;
	CTopRendererWnd* m_pViewport;

	//Panes
	CXTPDockingPane* m_pDockPane_Rollup;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

