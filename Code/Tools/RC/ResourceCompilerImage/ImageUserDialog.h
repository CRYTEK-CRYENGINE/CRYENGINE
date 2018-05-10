// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RelativeMouseInput.h"						// CRelativeMouseInput
#include "UIBumpmapPanel.h"								// CUIBumpmapPanel
#include "UIMIPControlPanel.h"						// CUIMIPControlPanel
#include "Monitor.h"
#include "ImageProperties.h"
#include "TinyDocument.h"

#define DEFAULT_SCALE64 64

class CImageCompiler;

//--------------------------------------------------------------------------------------------------
//structure used to store current preview data for calculation and rendering
//--------------------------------------------------------------------------------------------------
struct SPreviewData
{
	float	fShiftX;							//shift in x dir, in texture coordinates [0..1] initially 0.5
	float	fShiftY;							//shift in y dir, in texture coordinates [0..1] initially 0.5
	float	fOldShiftX;						//previous shift in x to recognize movements
	float	fOldShiftY;						//previous shift in y to recognize movements
	int		iScale64;							//current scale (fixpoint)
	int		iCurMaxScale64;				//current maximum scale for which mips are available
	int		iInitialScale64;			//initial scale for current preview image
	int		iOrigWidth;						//original width of current preview image
	int		iOrigHeight;					//original height of current preview image
	bool	bIsUpdating;					//indicates whether preview is currently being updated
	bool	bDelayUpdate;					//indicates whether preview update should be delayed
	DWORD	dwMouseButtonsOld;		//previous state of mouse buttons, for appropriate update on shift movements and update delaying

	SPreviewData()
	{
		fShiftX = 0.5f;
		fShiftY = 0.5f;
		fOldShiftX = 0.5f;
		fOldShiftY = 0.5f;
		iScale64 = DEFAULT_SCALE64;
		iCurMaxScale64 = DEFAULT_SCALE64;
		iInitialScale64 = DEFAULT_SCALE64;
		iOrigWidth = 0;
		iOrigHeight = 0;
		bIsUpdating = false;
		bDelayUpdate = false;
		dwMouseButtonsOld = 0;
	}
};

class CImageUserDialog : public Monitor, public TinyDocument<float>::Listener, public TinyDocument<bool>::Listener
{
public:
	SPreviewData m_PreviewData;

public:
	//! constructor
	CImageUserDialog();
	//! destructor
	virtual ~CImageUserDialog();

	//!
	bool DoModal(CImageCompiler *inpImageCompiler);
	
	// Returns
	//   true if preview conversion should be canceled
	bool PreviewGenerationCanceled();
	void UpdateProgressBar(const float fProgress);

	// Stores
	//   current input extent and maximum zoom levels
	void GetInputExtentAndMaximumZoom();
	// Returns
	//   number of MIP levels that can be skipped because preview display is scaled down (zero if no MIPs available)
	int GetPreviewReduceResolution(uint32 width, uint32 height);
	// Returns
	//   top/left included, right/bottom excluded
	RECT GetPreviewRectangle(const uint32 mipLevel = 0);
	// Returns
	//   window handle
	HWND GetHWND() const { return m_hWindow; }

private: // ---------------------------------------------------------------------------------------

	enum WorkerAction
	{
		WorkerActionNone,
		WorkerActionPaint,
		WorkerActionUpdatePreviewOnly,
		WorkerActionUpdatePreview,
		WorkerActionGenerateOutput,
		WorkerActionGenerateOutputAndQuit,
		WorkerActionSaveConfig,
		WorkerActionSaveConfigAndQuit,
		WorkerActionQuit
	};

	enum QuitRequest
	{
		QuitRequestNone,
		QuitRequestNormal,
		QuitRequestUserCancel
	};

	WorkerAction          m_eWorkerAction;
	char                  m_quitRequest;

	CImageCompiler *      m_pImageCompiler;       //!< this class is not responsible for destruction
	CImagePreview         m_ImagePreview;

	HWND                  m_hWindow;              //!< main window handle

	// sub windows (in the tab control)
	HWND                  m_hTab_Resolution;      //!< window handle
	CUIBumpmapPanel       m_BumpToNormalPanel;    //
	CUIBumpmapPanel       m_AlphaAsBumpPanel;     //
	CUIMIPControlPanel    m_MIPControlPanel;      //!<

	const int             m_iPreviewWidth;        //!<
	const int             m_iPreviewHeight;       //!<

	CRelativeMouseInput   m_RelMouse;             //!< for relative mouse movement
	bool                  m_bDialogIsUpdating;    //!<

	HANDLE                m_hWindowThread;

	// Maps filename suffices to a set of presets. Guaranteed to contain
	// the empty suffix mapping to the set of all presets.
	std::map<string, std::set<string> > m_FilteredPresets;
	// The list of presets to show in order of preference (default is parse-order)
	std::list<string> m_FileOrderedPresets;
	bool m_bShowAllPresets;

private: // ---------------------------------------------------------------------------------------
	//!
	static INT_PTR CALLBACK WndProc_ChooseResolution(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	//!
	void Draw( HDC inHdc );

	//!
	void MouseMessage(const DWORD indwButtons, const int iniRelX , const int iniRelY, int iniRelZ);

	//!
	void UpdateWindowTitle();
	void UpdatePreview(const bool bFullConvert);

	void CreateDialogItems();

	void SelectPreset(const string &presetName);
	void SetPreviewModes(const CImageProperties& props, const bool bInitalUpdate);

	string GetPreset(HWND control);
	EPreviewMode GetPreviewMode(const CImageProperties& props, HWND control);
	EPreviewMode GetOriginalPreviewMode(EPreviewMode processed);

	void GetDataFromDialog();

	//! makes the dialog visible too
	void SetDataToDialog(const bool bInitalUpdate = false);
	void SetPropertyTab(const int iniNo); 

	void UpdateBumpStrength();
	void UpdateBumpBlur();

	static DWORD WINAPI ThreadStart_Static(void* pThis);
	DWORD ThreadStart();

public:
	void TriggerUpdatePreview(bool bFull);
	void TriggerGenerateOutput(bool bQuit);
	void TriggerSaveConfig(bool bQuit);
	void TriggerRepaint();
	void TriggerCancel();

	WorkerAction CheckAndResetWorkerAction();

	virtual void OnTinyDocumentChanged(TinyDocument<float>* pDocument);
	virtual void OnTinyDocumentChanged(TinyDocument<bool>* pDocument);

	void ZoomPreviewUpdateCheck();
};
