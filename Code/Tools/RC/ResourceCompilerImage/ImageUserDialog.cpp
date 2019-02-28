// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImageUserDialog.h"        // CImageUserDialog
#include "ImageObject.h"            // ImageObject
#include "resource.h"               // IDD_ ...
#include <assert.h>                 // assert()
#include "ImageCompiler.h"          // CImageCompiler
#include "ImagePreview.h"           // EPreviewMode
#include "RelativeMouseInput.h"     // CRelativeMouseInput
#include "IRCLog.h"                 // IRCLog
#include "tools.h"                  // CenterWindow()

#include <commctrl.h>               // TCITEM
#include <vector>
#include <algorithm>

#include <windowsx.h>               // ComboBox_GetCurSel
#undef SubclassWindow

static const char* const s_pComboboxSeparator = "------------------------------------------------";


namespace
{
class SimpleTextConfigSink
	: public IConfigSink
{
public:
	string m_text;

	virtual ~SimpleTextConfigSink()
	{
	}

	virtual void SetKeyValue(EConfigPriority ePri, const char* key, const char* value)
	{
		m_text += string(key) + "=" + value + "\r\n";
	}
};

class FileMaskConfigSink
	: public IConfigSink
{
public:
	std::vector<string> m_fileMasks;

	FileMaskConfigSink()
	{
	}

	virtual ~FileMaskConfigSink()
	{
	}

	virtual void SetKeyValue(EConfigPriority ePri, const char* key, const char* value)
	{
		if (key && StringHelpers::EqualsIgnoreCase(key, "filemasks"))
		{
			m_fileMasks.clear();

			StringHelpers::Split(value, ",", false, m_fileMasks);
		}
	}
};
}

static volatile bool s_bDialogInitialized;

namespace Reduce
{

enum { kMinReduce = -2 };
enum { kMaxReduce = 5 };

struct PlatformData
{
	int platform;
	string platformName;
	string formatName;
	int reduceValue;
	uint32 width;
	uint32 height;
	bool bMark;

	PlatformData()
		: platform(-1)
		, platformName("?")
		, formatName("?")
		, reduceValue(0)
		, width(0)
		, height(0)
		, bMark(false)
	{
	}
	
	void set(
		int a_platform,
		const char* a_pPlatformName,
		int a_reduceValue,
		bool a_bMark)
	{
		if (!a_pPlatformName || !a_pPlatformName[0])
		{
			assert(0 && "Bad platform data");
			return;
		}
		platform = a_platform;
		platformName = a_pPlatformName;
		formatName = "?";
		reduceValue = a_reduceValue;
		width = 0;
		height = 0;
		bMark = a_bMark;
	}
};

static std::vector<PlatformData> s_platforms;

static CImageCompiler* s_pImageCompiler = 0;



static void sortArray(std::vector<PlatformData>& platforms)
{
	struct PlatformDataSorter
	{
		bool operator()(const PlatformData& l, const PlatformData& r) const
		{
			// Let's keep order of platforms same as in rc.ini
			if (l.platform != r.platform)
			{
				return l.platform < r.platform;
			}
			return l.platformName < r.platformName;
		}
	};

	if (platforms.size() > 1)
	{
		std::sort(platforms.begin(), platforms.end(), PlatformDataSorter());
	}
}


static void fillListbox(
	HWND hTabResolution,
	const std::vector<PlatformData>& platforms)
{
	string tmp;

	const HWND hwnd = GetDlgItem(hTabResolution, IDC_RESOLUTION_LIST);
	assert(hwnd && "No resolution list");
	ListView_DeleteAllItems(hwnd);

	for (size_t i = 0; i < platforms.size(); ++i)
	{
		const PlatformData& p = platforms[i];

		LVITEM lvi;
		memset(&lvi, 0, sizeof(lvi));

		lvi.mask      = LVIF_IMAGE | LVIF_STATE;
		lvi.stateMask = LVIS_SELECTED;
		lvi.iSubItem  = 0;

		lvi.state = p.bMark ? LVIS_SELECTED : 0;
		lvi.iItem = i;
		lvi.iImage = p.reduceValue - kMinReduce;
		ListView_InsertItem(hwnd, &lvi);
										  
		ListView_SetItemText(hwnd, i, 0, "");

		tmp = StringHelpers::Format("%i", p.reduceValue);
		ListView_SetItemText(hwnd, i, 1, (LPSTR)tmp.c_str());

		ListView_SetItemText(hwnd, i, 2, (LPSTR)p.platformName.c_str());

		tmp = StringHelpers::Format("%u", p.width);
		ListView_SetItemText(hwnd, i, 3, (LPSTR)tmp.c_str());

		tmp = StringHelpers::Format("%u", p.height);
		ListView_SetItemText(hwnd, i, 4, (LPSTR)tmp.c_str());

		ListView_SetItemText(hwnd, i, 5, (LPSTR)p.formatName.c_str());

		ListView_SetItemState(hwnd, i, (p.bMark ? LVIS_SELECTED : 0), LVIS_SELECTED);
	}
}


static int grabListboxSelection(
	HWND hTabResolution,
	std::vector<PlatformData>& platforms)
{
	const HWND hwnd = GetDlgItem(hTabResolution, IDC_RESOLUTION_LIST);

	int selCount = 0;
	for (int i = 0; i < (int)platforms.size(); ++i)
	{
		const bool bSelected = (ListView_GetItemState(hwnd, i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
		if (bSelected)
		{
			++selCount;
		}
		platforms[i].bMark = bSelected;
	}

	return selCount;
}


static void convertMapToArray(
	const CImageCompiler* pImageCompiler,
	const CImageProperties::ReduceMap& reduceMap,
	std::vector<PlatformData>& platforms)
{
	{
		std::vector<PlatformData> tmp;
		tmp.reserve(reduceMap.size());

		for (CImageProperties::ReduceMap::const_iterator it = reduceMap.begin(); it != reduceMap.end(); ++it)
		{
			const CImageProperties::ReduceItem& ri = it->second;
			PlatformData pd;
			pd.set(ri.platformIndex, ri.platformName, ri.value, false);
			tmp.push_back(pd);
		}
		sortArray(tmp);

		// let's try to keep existing selection (.bMark)
		platforms.resize(tmp.size());
		for (size_t i = 0; i < platforms.size(); ++i)
		{
			tmp[i].bMark = platforms[i].bMark;
		}

		tmp.swap(platforms);
	}

	for (size_t i = 0; i < platforms.size(); ++i)
	{
		PlatformData& p = platforms[i];

		if (p.platform >= 0)
		{
			// It's a platform from list of known platforms

			EPixelFormat finalFormat;
			bool bFinalCubemap;
			uint32 dwFinalMipCount;
			uint32 dwFinalReduce;

			pImageCompiler->GetFinalImageInfo(p.platform, finalFormat, bFinalCubemap, dwFinalMipCount, dwFinalReduce, p.width, p.height);

			if (finalFormat < 0 || finalFormat >= ePixelFormat_Count)
			{
				assert(0);
				p.formatName = "?format?";
			}
			else
			{
				const PixelFormatInfo* const pFormatInfo = CPixelFormats::GetPixelFormatInfo(finalFormat);
				if (!pFormatInfo || !pFormatInfo->szName)
				{
					assert(0);
					p.formatName = "?format?";
				}
				else
				{
					p.formatName = pFormatInfo->szName;
				}
			}
		}
	}
}


static void convertArrayToMap(
	const CImageCompiler* pImageCompiler,
	const std::vector<PlatformData>& platforms,
	CImageProperties::ReduceMap& reduceMap)
{
	string s;
	string tmp;
	for (int i = 0; i < (int)platforms.size(); ++i)
	{
		tmp = StringHelpers::Format("%s%s:%i", (i ? "," : ""), platforms[i].platformName.c_str(), platforms[i].reduceValue);
		s += tmp;
	}

	pImageCompiler->m_Props.ConvertReduceStringToMap(s, reduceMap);
}


static void applyChangesAndFillListbox(
	CImageCompiler* pImageCompiler,
	HWND hTabResolution,
	std::vector<PlatformData>& platforms)
{
	CImageProperties::ReduceMap reduceMap;
	Reduce::convertArrayToMap(pImageCompiler, platforms, reduceMap);

	pImageCompiler->m_Props.SetReduceResolutionFile(reduceMap);

	// Recompute resolutions
	Reduce::convertMapToArray(pImageCompiler, reduceMap, platforms);

	Reduce::fillListbox(hTabResolution, platforms);
}

} // namespace Reduce


// Class name for this application's window class.
#define WINDOW_CLASSNAME      "UserDialog"

// constructor
CImageUserDialog::CImageUserDialog() 
	: m_iPreviewWidth(256)
	, m_iPreviewHeight(256)
	, m_quitRequest(QuitRequestNone)
{
	m_eWorkerAction = WorkerActionNone;
	m_pImageCompiler = 0;
	m_hTab_Resolution = 0;
	m_hWindow = 0;
	m_bDialogIsUpdating = false;
	m_hWindowThread = 0;
	m_bShowAllPresets = false;
}

// destructor
CImageUserDialog::~CImageUserDialog()
{
	assert(!m_pImageCompiler);			// should be 0 outside of DoModal call
}


bool CImageUserDialog::DoModal(CImageCompiler *inpImageCompiler)
{
	m_pImageCompiler = inpImageCompiler;
	m_eWorkerAction = WorkerActionNone;

	s_bDialogInitialized = false;

	// Start the thread that will control the window and handle the message loop.
	m_hWindowThread = CreateThread(
		0,											//LPSECURITY_ATTRIBUTES lpThreadAttributes,
		0,											//SIZE_T dwStackSize,
		ThreadStart_Static,			//LPTHREAD_START_ROUTINE lpStartAddress,
		this,										//LPVOID lpParameter,
		0,											//DWORD dwCreationFlags,
		0);											//LPDWORD lpThreadId);

	while (!s_bDialogInitialized)
	{
		Sleep(1);
	}

	bool bSuccess = false;

	// Loop until the dialog thread has exited.
	for (;;)
	{
		// Check whether the ui thread is finished - if so, we can return.
		{
			DWORD dwReturnCode = 0;
			GetExitCodeThread(m_hWindowThread, &dwReturnCode);
			if (dwReturnCode != STILL_ACTIVE)
			{
				bSuccess = (dwReturnCode == 0);
				break;
			}
		}

		// Check whether we need to update the preview.
		WorkerAction eAction = CheckAndResetWorkerAction();

		switch (eAction)
		{
		case WorkerActionNone:
			break;

		case WorkerActionUpdatePreviewOnly:
			{
				GetDataFromDialog();
			//SetDataToDialog();		// show settings in dialog

				UpdatePreview(false);
				TriggerRepaint();

				break;
			}

		case WorkerActionUpdatePreview:
			{
				//clear progress before bar is drawn
				m_pImageCompiler->m_Progress.Start();

				{
					LOCK_MONITOR();

					m_PreviewData.bIsUpdating = true;
					if (m_PreviewData.bDelayUpdate)
					{
						TriggerRepaint();
						continue;
					}
				}

				GetDataFromDialog();
				GetInputExtentAndMaximumZoom();
				SetDataToDialog();		// show settings in dialog
				GetDataFromDialog();	

				HDC hPreviewDC = GetDC(this->m_hWindow);
				Draw(hPreviewDC);
				ReleaseDC(this->m_hWindow, hPreviewDC);

				UpdatePreview(true);

				{
					LOCK_MONITOR();
					m_PreviewData.bIsUpdating = false;
				}

				TriggerRepaint();
				break;
			}

		case WorkerActionGenerateOutputAndQuit:
		case WorkerActionGenerateOutput:
			{
				GetDataFromDialog();
				SetDataToDialog(true);								// might need to update preset in UI
				GetDataFromDialog();	

				bool bRet = m_pImageCompiler->RunWithProperties(true);

				if (bRet)
				{
					if (!m_pImageCompiler->UpdateAndSaveConfig())
					{
						MessageBox(this->m_hWindow,"Error while saving the config file (write protected?)","ResourceCompiler Image Error",MB_OK);
					}
					else
					{
						RCLog("Output was generated.");
						if (eAction == WorkerActionGenerateOutputAndQuit)
						{
							m_quitRequest = QuitRequestNormal;
						}
					}
				}
				else
				{
					RCLog("No Output was generated.");
				}

				break;
			}

		case WorkerActionSaveConfigAndQuit:
		case WorkerActionSaveConfig:
			{
				GetDataFromDialog();

				if (!m_pImageCompiler->UpdateAndSaveConfig())
				{
					MessageBox(this->m_hWindow,"Error while saving the config file (write protected?)","ResourceCompiler Image Error",MB_OK);
				}
				else
				{
					if (eAction == WorkerActionSaveConfigAndQuit)
					{
						m_quitRequest = QuitRequestNormal;
					}
					else
					{
						MessageBox(this->m_hWindow,"Config saved","ResourceCompiler Image",MB_OK);
					}
				}
				break;
			}

		case WorkerActionPaint:
			{
				HDC hPreviewDC = GetDC(this->m_hWindow);
				Draw(hPreviewDC);
				ReleaseDC(this->m_hWindow, hPreviewDC);
				break;
			}

		case WorkerActionQuit:
			{
				m_quitRequest = QuitRequestUserCancel;
			}
			break;
		}

		// Wait some time to avoid hogging the CPU.
		Sleep(100);
	}

	m_pImageCompiler = 0;

	return bSuccess;
}


void CImageUserDialog::CreateDialogItems()
{
	LOCK_MONITOR();

	// tab control
	{
		HWND hwnd=GetDlgItem(m_hWindow,IDC_PROPTAB);
		assert(hwnd);
		TCITEM tie;

		tie.mask = TCIF_TEXT | TCIF_IMAGE;
		tie.iImage = -1;

		tie.pszText = "Resolution";
		TabCtrl_InsertItem(hwnd,0,&tie);

		tie.pszText = "MIP Control";
		TabCtrl_InsertItem(hwnd,1,&tie);

		tie.pszText = "RGB to Normal";
		TabCtrl_InsertItem(hwnd,2,&tie);

		tie.pszText = "A to Normal";
		TabCtrl_InsertItem(hwnd,3,&tie);

		// sub windows
		m_BumpToNormalPanel.m_pDlg = this;
		m_BumpToNormalPanel.InitDialog(m_hWindow,false);
		m_BumpToNormalPanel.m_bumpStrengthValue.AddListener(this);
		m_BumpToNormalPanel.m_bumpBlurValue.AddListener(this);
		m_BumpToNormalPanel.m_bumpInvertValue.AddListener(this);

		m_AlphaAsBumpPanel.m_pDlg = this;
		m_AlphaAsBumpPanel.InitDialog(m_hWindow,false);
		m_AlphaAsBumpPanel.m_bumpStrengthValue.AddListener(this);
		m_AlphaAsBumpPanel.m_bumpBlurValue.AddListener(this);
		m_AlphaAsBumpPanel.m_bumpInvertValue.AddListener(this);

		// mipmaps
		{
			m_MIPControlPanel.InitDialog(m_hWindow,false);

			for (int i = 0; i < CUIMIPControlPanel::NUM_CONTROLLED_MIP_MAPS; ++i)
			{
				m_MIPControlPanel.m_MIPAlphaControl[i].GetDocument().AddListener(this);
			}
			m_MIPControlPanel.m_MIPBlurringControl.GetDocument().AddListener(this);
		}

		RECT rect = { 0, 0, 1, 1 };
		TabCtrl_AdjustRect(hwnd, TRUE, &rect);

		m_hTab_Resolution=CreateDialogA(g_hInst,MAKEINTRESOURCE(IDD_TAB_RESOLUTION),hwnd,CImageUserDialog::WndProc/*WndProcRedirect*/);
		assert(m_hTab_Resolution);

		SetWindowLongPtr(m_hTab_Resolution,GWLP_USERDATA,(LONG_PTR)this);

		SetWindowPos(m_hTab_Resolution, HWND_TOP, -rect.left, -rect.top, 0, 0, SWP_NOSIZE);

		{
			const HWND hwnd = GetDlgItem(m_hTab_Resolution, IDC_RESOLUTION_LIST);
			assert(hwnd && "No resolution list");
			ListView_SetExtendedListViewStyle(hwnd, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

			LVCOLUMN lvc;
			memset(&lvc, 0, sizeof(lvc));
			lvc.mask = LVCF_WIDTH | LVCF_FMT;
			lvc.iSubItem = 0;

			lvc.cx = 18;
			lvc.fmt = LVCFMT_LEFT | LVCFMT_FIXED_WIDTH;
			ListView_InsertColumn(hwnd, 0, &lvc);

			lvc.cx = 23;
			lvc.fmt = LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH;
			ListView_InsertColumn(hwnd, 1, &lvc);

			lvc.cx = 51;
			lvc.fmt = LVCFMT_LEFT | LVCFMT_FIXED_WIDTH;
			ListView_InsertColumn(hwnd, 2, &lvc);

			lvc.cx = 40;
			lvc.fmt = LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH;
			ListView_InsertColumn(hwnd, 3, &lvc);

			lvc.cx = 40;
			lvc.fmt = LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH;
			ListView_InsertColumn(hwnd, 4, &lvc);

			lvc.cx = 73;
			lvc.fmt = LVCFMT_LEFT | LVCFMT_FIXED_WIDTH;
			ListView_InsertColumn(hwnd, 5, &lvc);

			const int w = 16;
			const int h = 16;
			const int n = (Reduce::kMaxReduce - Reduce::kMinReduce + 1);

			HBITMAP hbmImage = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_REDUCE));
			HIMAGELIST himl = ImageList_Create(w, h, ILC_COLOR24, n, 1);

			const int res = ImageList_Add(himl, hbmImage, NULL);
			assert(res != -1);

			ListView_SetImageList(hwnd, himl, LVSIL_SMALL);
		}

		// make dialog visible and set active tab
		SetPropertyTab(0);
	}

	// presets
	{
		const ICfgFile* const pIni = m_pImageCompiler->m_CC.pRC->GetIniFile();
		const HWND hwnd = GetDlgItem(m_hWindow, IDC_TEMPLATECOMBO);
		assert(hwnd);

		for (int i = 0; ; ++i)
		{
			const char* const pName = pIni->GetSectionName(i);
			if (!pName)
			{
				break;
			}
			if (pName[0] == 0 || pName[0] == '_')
			{
				continue;
			}

			FileMaskConfigSink sink;
			pIni->CopySectionKeysToConfig(eCP_PriorityRcIni, i, 0, &sink);
			const size_t num = sink.m_fileMasks.size();

			if (!m_FilteredPresets[""].count(pName))
			{
				m_FileOrderedPresets.push_back(pName);
			}

			m_FilteredPresets[""].insert(pName);
			for (size_t pos = 0; pos < num; ++pos)
			{
				m_FilteredPresets[sink.m_fileMasks[pos]].insert(pName);
			}

			SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)pName);
		}

		SendMessage(hwnd, CB_SETCURSEL, 0, 0);
	}

	HWND hwnd = GetDlgItem(m_hWindow, IDC_PRESETSHOWALL);
	Button_SetCheck(hwnd, m_bShowAllPresets ? 1 : 0);

	// preview mode
	{
		HWND hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWMODE);
		assert(hwnd);

		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"RGB");
		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"Alpha");
		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"RGB Alpha");
		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"Debug Normals");
		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"RGB*A (HDR)");
		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"RGB*A*A (HDR)");
		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"AlphaTestBlend");
		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"AlphaTestNoBlend");
		SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)"mCIE2RGB");

		SendMessage(hwnd,CB_SETCURSEL,0,0);		// first 0 is setting the used template
	}
}


void CImageUserDialog::SelectPreset(const string &presetName)
{
	string filename = m_pImageCompiler->m_CC.config->GetAsString("overwritefilename", m_pImageCompiler->m_CC.sourceFileNameOnly, m_pImageCompiler->m_CC.sourceFileNameOnly);
	filename = PathUtil::RemoveExtension(filename.c_str());

	const HWND hwnd = GetDlgItem(m_hWindow, IDC_TEMPLATECOMBO);
	SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
	int presetCount = 0;
	int presetIndex = -1;
	std::set<string> matchingPresets;
	std::set<string>::const_iterator p;
	std::list<string>::const_iterator l;

	// Collect names of all matching presets
	std::map<string, std::set<string> >::const_iterator s;
	for (s = m_FilteredPresets.begin(); s != m_FilteredPresets.end(); ++s)
	{
		const string& suffix = s->first;
		if (StringHelpers::MatchesWildcardsIgnoreCase(filename, suffix))
		{
			for (p = s->second.begin(); p != s->second.end(); ++p)
			{
				matchingPresets.insert(*p);
			}
		}
	}

	// Add names of all matching presets to the combobox
	for (p = matchingPresets.begin(); p != matchingPresets.end(); ++p)
	{
		// preset name matches
		if (StringHelpers::EqualsIgnoreCase(*p, presetName))
		{
			presetIndex = presetCount;
		}

		SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)p->c_str());
		++presetCount;
	}

	if (!presetCount || presetIndex == -1 || m_bShowAllPresets)
	{
		const bool bShowAllPresets = !presetCount || m_bShowAllPresets;

		// Add a separator between matching and non-matching names of presets
		if (presetCount)
		{
			SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)s_pComboboxSeparator);
			++presetCount;
		}

		// Add all non-matching names of presets to the combobox
		for (l = m_FileOrderedPresets.begin(); l != m_FileOrderedPresets.end(); ++l)
		{
			if (!matchingPresets.count(*l))
			{
				// preset name matches
				if (StringHelpers::EqualsIgnoreCase(*l, presetName))
				{
					presetIndex = presetCount;
				}

				// show only the preset which is in the file, in case we don't show all of them already
				if (presetIndex != presetCount && !bShowAllPresets)
				{
					continue;
				}

				SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)l->c_str());
				++presetCount;
			}
		}
	}

	SendMessage(hwnd, CB_SETCURSEL, presetIndex, 0);
}


void CImageUserDialog::SetPreviewModes(const CImageProperties& props, const bool bInitalUpdate)
{
	EPreviewMode modes[ePM_Num];
	int modeCount = 0;
	
	const EPreviewMode eRgbkMode = (props.GetRGBKCompression() == 1) ? ePM_RGBLinearK : ePM_RGBSquareK;

	if (props.GetRGBKCompression() > 0)
	{
		modes[modeCount++] = eRgbkMode;
		modes[modeCount++] = ePM_RawRGB;
		modes[modeCount++] = ePM_KKK;
	}
	else if (props.GetColorModel() == CImageProperties::eColorModel_CIE)
	{
		modes[modeCount++] = ePM_RGB;
		modes[modeCount++] = ePM_RawCIE;
	}
	else if (props.GetColorModel() == CImageProperties::eColorModel_IRB)
	{
		modes[modeCount++] = ePM_RGB;
		modes[modeCount++] = ePM_RawIRB;
	}
	else if (props.GetColorModel() == CImageProperties::eColorModel_YCbCr)
	{
		modes[modeCount++] = ePM_RGB;
		modes[modeCount++] = ePM_RawYCbCr;
	}
	else if (props.GetColorModel() == CImageProperties::eColorModel_YFbFr)
	{
		modes[modeCount++] = ePM_RGB;
		modes[modeCount++] = ePM_RawYFbFr;
	}
	else
	{
		modes[modeCount++] = ePM_RGB;
	}

	if (props.GetRGBKCompression() == 0)
	{
		modes[modeCount++] = ePM_AAA;
		modes[modeCount++] = ePM_RGBA;
		modes[modeCount++] = ePM_AlphaTestBlend;
		modes[modeCount++] = ePM_AlphaTestNoBlend;
	}

	if (props.GetMipRenormalize())
	{
		modes[modeCount++] = ePM_NormalLength;
	}

	const HWND hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWMODE);
	SendMessage(hwnd, CB_RESETCONTENT, 0, 0);

	for (int i = 0; i < modeCount; ++i)
	{
		SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)CImagePreview::GetPreviewModeChoiceText(modes[i]));
	}

	const EPreviewMode curMode =
		(bInitalUpdate && props.GetRGBKCompression() > 0)
		? eRgbkMode
		: GetPreviewMode(props, hwnd);

	int selection = -1;
	for (int i = 0; i < modeCount; ++i)
	{
		if (modes[i] == curMode)
		{
			selection = i;
			break;
		}
	}

	SendMessage(hwnd, CB_SETCURSEL, (selection < 0) ? 0 : selection, 0);
}


string CImageUserDialog::GetPreset(HWND control)
{
	const int index = ComboBox_GetCurSel(control);
	const int length = ComboBox_GetLBTextLen(control, index);
	char text[128];
	if (length <= 0 || 
		length + 1 > sizeof(text) ||
		length != ComboBox_GetLBText(control, index, text))
	{
		//assert(0);
	}
	else if (!StringHelpers::Equals(s_pComboboxSeparator, text))
	{
		return text;
	}

	return "";
}


EPreviewMode CImageUserDialog::GetPreviewMode(const CImageProperties& props, HWND control)
{
	const int index = ComboBox_GetCurSel(control);
	const int length = ComboBox_GetLBTextLen(control, index);
	char text[128];
	if (length <= 0 || 
		length + 1 > sizeof(text) ||
		length != ComboBox_GetLBText(control, index, text))
	{
		//assert(0);
	}
	else if (!StringHelpers::Equals(s_pComboboxSeparator, text))
	{
		for (int i = 0; i < ePM_Num; ++i)
		{
			const EPreviewMode pm = (EPreviewMode)i;
			if (!stricmp(text, CImagePreview::GetPreviewModeChoiceText(pm)))
			{
				return pm;
			}
		}
	}

	return props.m_ePreviewModeOriginal;
}


EPreviewMode CImageUserDialog::GetOriginalPreviewMode(EPreviewMode processed)
{
	switch (processed)
	{
	// output only cases
	case ePM_RGB:
	case ePM_RGBLinearK:
	case ePM_RGBSquareK:
	case ePM_KKK:
	case ePM_RawRGB:
	case ePM_RawCIE:
	case ePM_RawIRB:
	case ePM_RawYCbCr:
	case ePM_RawYFbFr:
		return ePM_RGB;
	default:
		break;
	}

	return processed;
}


void CImageUserDialog::SetDataToDialog(const bool bInitalUpdate)
{
	m_bDialogIsUpdating = true;

	HWND hwnd;

	// preview part in main window -------------------------------------

	hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWON); assert(hwnd);
	Button_SetCheck(hwnd, m_pImageCompiler->m_Props.m_bPreview ? 1 : 0);

	hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWFILTERED); assert(hwnd);
	Button_SetCheck(hwnd, m_pImageCompiler->m_Props.m_bPreviewFiltered ? 1 : 0);

	hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWTILED); assert(hwnd);
	Button_SetCheck(hwnd, m_pImageCompiler->m_Props.m_bPreviewTiled ? 1 : 0);

	SetPreviewModes(m_pImageCompiler->m_Props, bInitalUpdate);

	hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWMODETEXT); assert(hwnd);
	SetWindowText(hwnd, CImagePreview::GetPreviewModeDescription(GetPreviewMode(m_pImageCompiler->m_Props, hwnd)));

	// main window -----------------------------------------------------
	hwnd = GetDlgItem(m_hWindow, IDC_PRESETSHOWALL); assert(hwnd);
	Button_SetCheck(hwnd, m_bShowAllPresets ? 1 : 0);

	SelectPreset(m_pImageCompiler->m_Props.GetPreset());

	m_BumpToNormalPanel.SetDataToDialog(m_pImageCompiler->m_Props.m_BumpToNormal, bInitalUpdate);
	m_AlphaAsBumpPanel.SetDataToDialog(m_pImageCompiler->m_Props.m_AlphaAsBump, bInitalUpdate);
	m_MIPControlPanel.SetDataToDialog(m_pImageCompiler->m_CC.config, m_pImageCompiler->m_Props, bInitalUpdate);

	// sub window: resolution ------------------------------------------

	hwnd = GetDlgItem(m_hTab_Resolution,IDC_SUPRESSENGINEREDUCE); assert(hwnd);
	Button_SetCheck(hwnd, m_pImageCompiler->m_Props.GetSupressEngineReduce() ? 1 : 0);
	EnableWindow(hwnd, !m_pImageCompiler->m_CC.config->HasKey("ser", eCP_PriorityAll & (~eCP_PriorityFile)));

	{
		CImageProperties::ReduceMap reduceMap;
		m_pImageCompiler->m_Props.GetReduceResolutionFile(reduceMap);

		Reduce::convertMapToArray(m_pImageCompiler, reduceMap, Reduce::s_platforms);
		Reduce::applyChangesAndFillListbox(m_pImageCompiler, m_hTab_Resolution, Reduce::s_platforms);
	}

	// -----------------------------------------------------------------

	m_bDialogIsUpdating = false;
}

bool CImageUserDialog::PreviewGenerationCanceled()
{
	LOCK_MONITOR();

	//check whether interface thread has been closed (window closed)
	{
		DWORD dwReturnCode = 0;
		GetExitCodeThread(m_hWindowThread, &dwReturnCode);
		if (dwReturnCode != STILL_ACTIVE)
			return true;
	}

	// Tiago: commented out because we have too many preview window updates
	//ZoomPreviewUpdateCheck();

	const bool bCancel = (m_eWorkerAction != WorkerActionNone && m_eWorkerAction != WorkerActionPaint && m_eWorkerAction != WorkerActionUpdatePreview);

	return bCancel;
}

void CImageUserDialog::ZoomPreviewUpdateCheck()
{
	bool bUpdatePreview = false;

	//if currently updating preview data and the zoom is reduced so MIPs are not needed, cancel processing and trigger update
	if (m_PreviewData.bIsUpdating)
	{
		if (m_PreviewData.iCurMaxScale64 > m_PreviewData.iScale64 && m_PreviewData.iScale64 < DEFAULT_SCALE64)
			bUpdatePreview = true;
	}

	//if zoomed out from partial view, trigger update
	if (m_PreviewData.iCurMaxScale64 > m_PreviewData.iScale64 && m_PreviewData.iCurMaxScale64 > m_PreviewData.iInitialScale64)
		bUpdatePreview = true;

	//if zoomed in beyond previous measures, preview data must be updated (to calculate skipped MIPs)
	if (m_PreviewData.iCurMaxScale64 < m_PreviewData.iScale64 && m_PreviewData.iCurMaxScale64 < DEFAULT_SCALE64)
		bUpdatePreview = true;

	if (bUpdatePreview)
		TriggerUpdatePreview(false);
}

void CImageUserDialog::UpdateProgressBar(float fProgress)
{
	if (PreviewGenerationCanceled())
	{
		return;
	}

	assert(fProgress >= 0);
	if (fProgress > 1.0f)
	{
		fProgress = 1.0f;
	}

	char inszTxt[8];
	cry_sprintf(inszTxt, "%3.0f%%", fProgress * 100.0f);

	RECT rectProgressBorder;
	rectProgressBorder.left = m_iPreviewWidth + 9;
	rectProgressBorder.top = m_iPreviewHeight - 14;
	rectProgressBorder.right = rectProgressBorder.left + m_iPreviewWidth;
	rectProgressBorder.bottom = m_iPreviewHeight;

	RECT rectProgressBar = rectProgressBorder;
	rectProgressBar.left += 1;
	rectProgressBar.top += 1;
	rectProgressBar.right -= 1;
	rectProgressBar.bottom -= 1;

	RECT rectProgressCount;
	rectProgressCount.left = m_iPreviewWidth + 9;
	rectProgressCount.top = rectProgressBorder.top - 1;
	rectProgressCount.right = rectProgressCount.left + m_iPreviewWidth;
	rectProgressCount.bottom = rectProgressCount.top + 18;

	HDC hdc=GetDC(m_hWindow);
	FillRect(hdc, &rectProgressBorder, GetSysColorBrush(COLOR_BACKGROUND));
	FillRect(hdc, &rectProgressBar, GetSysColorBrush(COLOR_BTNSHADOW));
	rectProgressBar.right = rectProgressBar.left + (int)((m_iPreviewWidth - 2) * fProgress);
	FillRect(hdc, &rectProgressBar, GetSysColorBrush(COLOR_SCROLLBAR));

	SetBkMode(hdc,TRANSPARENT);
	SetTextAlign(hdc,TA_CENTER);
	ExtTextOut(hdc,(rectProgressCount.left + rectProgressCount.right) / 2, rectProgressCount.top, ETO_CLIPPED, &rectProgressCount, inszTxt, strlen(inszTxt), 0);
	ReleaseDC(m_hWindow,hdc);
}

void CImageUserDialog::GetDataFromDialog()
{
	HWND hwnd;
	// preview part in main window
	//m_pImageCompiler->m_Props.SetToDefault(false);

	hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWON); assert(hwnd);
	m_pImageCompiler->m_Props.m_bPreview = Button_GetCheck(hwnd) != 0;

	hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWFILTERED); assert(hwnd);
	m_pImageCompiler->m_Props.m_bPreviewFiltered = Button_GetCheck(hwnd) != 0;

	hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWTILED); assert(hwnd);
	m_pImageCompiler->m_Props.m_bPreviewTiled = Button_GetCheck(hwnd) != 0;

	hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWMODE); assert(hwnd);
	m_pImageCompiler->m_Props.m_ePreviewModeProcessed = GetPreviewMode(m_pImageCompiler->m_Props, hwnd);
	m_pImageCompiler->m_Props.m_ePreviewModeOriginal = GetOriginalPreviewMode(m_pImageCompiler->m_Props.m_ePreviewModeProcessed);

	hwnd = GetDlgItem(m_hWindow, IDC_TEMPLATECOMBO); assert(hwnd);
	m_pImageCompiler->m_Props.SetPreset(GetPreset(hwnd));

	// Resolution tab
	{
		hwnd = GetDlgItem(m_hTab_Resolution,IDC_SUPRESSENGINEREDUCE);assert(hwnd);
		m_pImageCompiler->m_Props.SetSupressEngineReduce(Button_GetCheck(hwnd) != 0);

		CImageProperties::ReduceMap reduceMap;
		Reduce::convertArrayToMap(m_pImageCompiler, Reduce::s_platforms, reduceMap);
		m_pImageCompiler->m_Props.SetReduceResolutionFile(reduceMap);
	}

	m_BumpToNormalPanel.GetDataFromDialog(m_pImageCompiler->m_Props.m_BumpToNormal);
	m_AlphaAsBumpPanel.GetDataFromDialog(m_pImageCompiler->m_Props.m_AlphaAsBump);
	m_MIPControlPanel.GetDataFromDialog(m_pImageCompiler->m_Props);
}



void CImageUserDialog::UpdateWindowTitle()
{
	LOCK_MONITOR();

	assert(m_pImageCompiler);

	string zoom;
	zoom.Format("  Zoom:%d%%", (100 * m_PreviewData.iScale64) / 64);

	const string filename = m_pImageCompiler->m_CC.config->GetAsString("overwritefilename", m_pImageCompiler->m_CC.sourceFileNameOnly, m_pImageCompiler->m_CC.sourceFileNameOnly);

	const string title = filename + zoom;
	
	SetWindowText(m_hWindow,title.c_str());
}


//
void CImageUserDialog::Draw(HDC inHdc)
{
	LOCK_MONITOR();

	int iGap = 4;
	RECT rec;

	rec.left   = 0;
	rec.top    = 0;
	rec.right  = rec.left + m_iPreviewWidth;
	rec.bottom = rec.top  + m_iPreviewHeight;

	bool bPreviewCMConverted = m_pImageCompiler->m_Props.GetCubemap();
	bool bPreviewToGamma = m_pImageCompiler->m_Props.GetColorSpace().first == CImageProperties::eOutputColorSpace_Srgb;
	bool bPreviewStretched = m_pImageCompiler->m_Props.GetConfigAsBool("previewstretched", true, true);

	// left side = original
	const ImageObject* original = m_pImageCompiler->GetInputImage();
	if (!original)
	{
		m_ImagePreview.PrintTo(inHdc, rec, "Load failed", "(format is not supported by driver)");
		return;
	}

	// original may change shape
	if (bPreviewCMConverted)
	{
		ImageToProcess image(original->CopyImage());
		image.ConvertProbe(m_pImageCompiler->m_Props.GetPowOf2());
		original = image.get();
		image.forget();
	}

	if (m_pImageCompiler->m_Props.GetPowOf2() && !original->HasPowerOfTwoSizes())
	{
		char str[32];
		cry_sprintf(str,"(%dx%d)", original->GetWidth(0), original->GetHeight(0));
		m_ImagePreview.PrintTo(inHdc, rec, "Image size is not power of two", str);
		return;
	}

	// Q: Why can't original image be gamma-corrected?
	// Q: Why can't original image be RGBK?
	m_ImagePreview.AssignImage(original);
	m_ImagePreview.AssignPreset(&m_pImageCompiler->m_Props, false, true);

	if (!m_ImagePreview.BlitTo(inHdc, rec, m_PreviewData.fShiftX, m_PreviewData.fShiftY, m_PreviewData.iScale64))
	{
		FillRect(inHdc, &rec, GetSysColorBrush(COLOR_3DFACE));
	}

	rec.left  = m_iPreviewWidth + iGap * 2 + 1;
	rec.right = rec.left + m_iPreviewWidth;

	// right side = destination
	if (m_PreviewData.bIsUpdating)
	{
		UpdateProgressBar(m_pImageCompiler->m_Progress.Get());
	}
	else if (m_pImageCompiler->m_Props.m_bPreview)
	{
		const ImageObject* destination = m_pImageCompiler->GetFinalImage();
		if (!destination)
		{
			m_ImagePreview.PrintTo(inHdc, rec, "Conversion failed", "(check rc_log.log for details)");
			return;
		}

		// Q: Why can't destination image be RGBA?
		m_ImagePreview.AssignImage(destination);
		m_ImagePreview.AssignPreset(&m_pImageCompiler->m_Props, bPreviewToGamma, false);

		// Make it stretch to the same size as the original image,
		// this can happen if the destination platform has limitations
		// on the highest mipmap-level.
		if (bPreviewStretched)
		{
			m_ImagePreview.AssignScale(original);
		}

		if (!m_ImagePreview.BlitTo(inHdc, rec, m_PreviewData.fOldShiftX, m_PreviewData.fOldShiftY, m_PreviewData.iScale64))
		{
			FillRect(inHdc, &rec, GetSysColorBrush(COLOR_3DFACE));
		}
	}
	else
	{
		m_ImagePreview.PrintTo(inHdc, rec, "Preview is disabled", "(leftmost checkbox in Preview pane)");
	}
}


void CImageUserDialog::MouseMessage(const DWORD indwButtons, const int iniRelX , const int iniRelY, int iniRelZ)
{
	// early out if no processing required because no mouse buttons are currently and previously pressed and not zooming
	if (!indwButtons && !m_PreviewData.dwMouseButtonsOld && !iniRelZ)
		return;

	assert(m_pImageCompiler);
	const ImageObject* original = m_pImageCompiler->GetInputImage();

	uint32 iWidth = 0;
	uint32 iHeight = 0;
	uint32 iMips = 0;

	if (original)
	{
		original->GetExtentTransformed(iWidth, iHeight, iMips, m_pImageCompiler->m_Props);
	}

	float fOldShiftX = m_PreviewData.fShiftX;
	float fOldShiftY = m_PreviewData.fShiftY;
	int iOldScale64 = m_PreviewData.iScale64;

	m_PreviewData.fShiftX -= iniRelX * (64.0f / m_PreviewData.iScale64 / iWidth);
	m_PreviewData.fShiftY -= iniRelY * (64.0f / m_PreviewData.iScale64 / iHeight);

	while (iniRelZ)
	{
		if (iniRelZ > 0)
		{
			m_PreviewData.iScale64 /= 2;
			iniRelZ -= WHEEL_DELTA;
		}
		else
		{
			m_PreviewData.iScale64 *= 2;
			iniRelZ += WHEEL_DELTA;
		}
	}

	if (m_PreviewData.iScale64 < 1)				m_PreviewData.iScale64 = 1;
	if (m_PreviewData.iScale64 > 16*16*4)	m_PreviewData.iScale64 = 16*16*4;

	bool bNoMovementPossible = false;
	if (original)
	{
		m_ImagePreview.AssignImage(original);

		bNoMovementPossible = m_ImagePreview.ClampBlitOffset(m_iPreviewWidth, m_iPreviewHeight, m_PreviewData.fShiftX, m_PreviewData.fShiftY, m_PreviewData.iScale64);
	}

	LOCK_MONITOR();

	m_PreviewData.bDelayUpdate = false;

	const bool bMousePOld    = (m_PreviewData.dwMouseButtonsOld & 0x0001) || (m_PreviewData.dwMouseButtonsOld & 0x0002);
	const bool bMousePNow    = (indwButtons                     & 0x0001) || (indwButtons                     & 0x0002);
	const bool bChangedShift = (fOldShiftX != m_PreviewData.fShiftX || fOldShiftY != m_PreviewData.fShiftY);
	const bool bChangedScale = (iOldScale64 != m_PreviewData.iScale64);

	if (bChangedShift && !bMousePOld && !m_PreviewData.bIsUpdating)
	{
		m_PreviewData.fOldShiftX = m_PreviewData.fShiftX;
		m_PreviewData.fOldShiftY = m_PreviewData.fShiftY;
	}

	if (bChangedShift || bChangedScale)
	{
		UpdateWindowTitle();			// Zoom:%d

		RECT rect;

		GetClientRect(m_hWindow, &rect);

		rect.bottom = m_iPreviewHeight;
		rect.right = m_iPreviewWidth;

		// update window
		InvalidateRect(m_hWindow, &rect, bNoMovementPossible);
		UpdateWindow(m_hWindow);

		if (m_PreviewData.bIsUpdating)
		{
			TriggerUpdatePreview(false);
		}
		else
		{
			ZoomPreviewUpdateCheck();
		}

		if (bMousePOld)
		{
			m_PreviewData.bDelayUpdate = true;
		}
	}

	if (bMousePOld && !bMousePNow && (m_PreviewData.iScale64 > m_PreviewData.iInitialScale64))
	{
		float deltaX = fabsf(m_PreviewData.fOldShiftX - m_PreviewData.fShiftX);
		float deltaY = fabsf(m_PreviewData.fOldShiftY - m_PreviewData.fShiftY); 

		if (deltaX > 0.0001f || deltaY > 0.0001f)
		{
			TriggerUpdatePreview(false);
		}

		//if (m_PreviewData.fOldShiftX!=m_PreviewData.fShiftX || m_PreviewData.fOldShiftX!=m_PreviewData.fShiftY)
		//	TriggerUpdatePreview(false);
	}

	m_PreviewData.dwMouseButtonsOld = indwButtons;
}



void CImageUserDialog::UpdatePreview(const bool bFullConvert)
{
	m_PreviewData.iCurMaxScale64 =  m_PreviewData.iScale64;

	// preview images
	{
		RECT rect;

		GetClientRect(m_hWindow, &rect);
		rect.bottom = m_iPreviewHeight;

		bool bErase = false;
		if (bFullConvert)
		{
			SetDataToDialog(true);																// might need to update preset in UI
			bErase = m_pImageCompiler->RunWithProperties(false);  // do image conversion
		}
		
		//InvalidateRect(m_hWindow,&rect,bErase);               // don't erase background
		(void)bErase;
	}

	// info text left
	{
		string sLeftInfo = m_pImageCompiler->GetInfoStringUI(true);

		HWND hwnd = GetDlgItem(m_hWindow, IDC_LEFTPREVIEWINFO);
		assert(hwnd);
		SetWindowText(hwnd, sLeftInfo.c_str());
	}

	// info text right
	if (!PreviewGenerationCanceled())
	{
		string sRightInfo = m_pImageCompiler->GetInfoStringUI(false);

		HWND hwnd = GetDlgItem(m_hWindow, IDC_RIGHTPREVIEWINFO);
		assert(hwnd);
		SetWindowText(hwnd, sRightInfo.c_str());

		m_PreviewData.fOldShiftX = m_PreviewData.fShiftX;
		m_PreviewData.fOldShiftY = m_PreviewData.fShiftY;
	}
}


void CImageUserDialog::SetPropertyTab( const int iniNo )
{
	LOCK_MONITOR();

	assert(iniNo>=0 && iniNo<=3);
	assert(m_BumpToNormalPanel.GetHWND());
	assert(m_hTab_Resolution);
	assert(m_AlphaAsBumpPanel.GetHWND());
	assert(m_MIPControlPanel.GetHWND());

	HWND hwnd=GetDlgItem(m_hWindow,IDC_PROPTAB);
	assert(hwnd);

	TabCtrl_SetCurSel(hwnd,iniNo);

	ShowWindow(m_hTab_Resolution, ((iniNo==0)?SW_SHOW:SW_HIDE));
	ShowWindow(m_MIPControlPanel.GetHWND(), ((iniNo==1)?SW_SHOW:SW_HIDE));
	ShowWindow(m_BumpToNormalPanel.GetHWND(), ((iniNo==2)?SW_SHOW:SW_HIDE));
	ShowWindow(m_AlphaAsBumpPanel.GetHWND(), ((iniNo==3)?SW_SHOW:SW_HIDE));

	switch (iniNo)
	{
	case 0:
		SetFocus(m_hTab_Resolution);
		break;
	case 1:
		SetFocus(m_MIPControlPanel.GetHWND());
		break;
	case 2:
		SetFocus(m_BumpToNormalPanel.GetHWND());
		break;
	case 3:
		SetFocus(m_AlphaAsBumpPanel.GetHWND());
		break;
	}
}

int CImageUserDialog::GetPreviewReduceResolution(uint32 dwWidth, uint32 dwHeight)
{
	const int factor = m_PreviewData.iCurMaxScale64 << m_pImageCompiler->m_Props.GetRequestedResolutionReduce(dwWidth, dwHeight);

	if (factor >= DEFAULT_SCALE64)
	{
		return 0;
	}

	return m_pImageCompiler->m_Props.GetMipMaps() ? ((int)(log((float)DEFAULT_SCALE64)/gf_ln2) - (int)(log((float)factor)/gf_ln2)) : 0;
}

RECT CImageUserDialog::GetPreviewRectangle(const uint32 mipLevel)
{
	RECT rect;

	int iWidth = m_PreviewData.iOrigWidth >> mipLevel;
	if (iWidth < 1)
	{
		iWidth = 1;
	}
	int iHeight = m_PreviewData.iOrigHeight >> mipLevel;
	if (iHeight < 1)
	{
		iHeight = 1;
	}
	const int iScale = m_PreviewData.iCurMaxScale64 << mipLevel;

	float fCoordinateShiftX = 0.5f;
	float fCoordinateShiftY = 0.5f;
	if (m_PreviewData.iCurMaxScale64 > m_PreviewData.iInitialScale64)
	{
		float fScaleShiftX = 1.0f;
		float fScaleShiftY = 1.0f;
		if (iWidth != iHeight)
		{
			if (iWidth < iHeight)
				fScaleShiftX = (float)(iHeight / iWidth);
			else
				fScaleShiftY = (float)(iWidth / iHeight);
		}
	
		const float fTemp = m_PreviewData.iInitialScale64 / (float)m_PreviewData.iCurMaxScale64;
		fCoordinateShiftX *= fTemp * fScaleShiftX;
		fCoordinateShiftY *= fTemp * fScaleShiftY;
	}

	const int iTemp = (std::max(m_iPreviewWidth, m_iPreviewHeight) * DEFAULT_SCALE64) / iScale;

	rect.left = (long)((m_PreviewData.fShiftX - fCoordinateShiftX) * iWidth);
	rect.right = rect.left + iTemp;
	rect.right = (rect.right > iWidth) ? iWidth : rect.right;
	rect.top = (long)((m_PreviewData.fShiftY - fCoordinateShiftY) * iHeight);
	rect.bottom = rect.top + iTemp;
	rect.bottom = (rect.bottom > iHeight) ? iHeight : rect.bottom;

	//additional border pixels for mipmap generation
	const LONG maxBorderSize = 16;
	rect.left -= std::min(maxBorderSize, rect.left);
	rect.right += std::min(maxBorderSize, iWidth - rect.right);
	rect.top -= std::min(maxBorderSize, rect.top);
	rect.bottom += std::min(maxBorderSize, iHeight - rect.bottom);

	// expand to include full 4x4 blocks
	rect.left = rect.left & (~0x3);
	rect.top = rect.top & (~0x3);
	rect.right = (rect.right + 3) & (~0x3);
	rect.bottom = (rect.bottom + 3) & (~0x3);

	return rect;
}

void CImageUserDialog::GetInputExtentAndMaximumZoom()
{
	uint32 iTexWidth = 0;
	uint32 iTexHeight = 0;
	uint32 iTexMips = 0;
	int iScale64 = DEFAULT_SCALE64;

	if (m_pImageCompiler->GetInputImage())
	{
		m_pImageCompiler->GetInputImage()->GetExtentTransformed(iTexWidth, iTexHeight, iTexMips, m_pImageCompiler->m_Props);
	}

	m_PreviewData.iOrigWidth  = iTexWidth;
	m_PreviewData.iOrigHeight = iTexHeight;

	while ((iTexWidth > 256 || iTexHeight > 256) && iScale64 != 1)
	{
		iTexWidth >>= 1;
		iTexHeight >>= 1;

		iScale64 >>= 1;
	}

	m_PreviewData.iCurMaxScale64  = iScale64;
	m_PreviewData.iInitialScale64 = iScale64;
}

INT_PTR CALLBACK CImageUserDialog::WndProc_ChooseResolution( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			// Let's find what resolutions we will have for all possible "reduce" values [-2; 5]
			// We will use GetFinalImageInfo() which requires platform specifide. For that we can
			// use arbitrary platform from the list of available platforms, because usually 
			// resulting width and height do not depend on platform (it's not exactly true
			// for platforms having some specific "min" and "max" texture sizes, but showing 
			// proper per-platform resolutions for our multi-select list of platforms is 10x
			// more complicated).

			CImageProperties::ReduceMap savedReduceMap;
			Reduce::s_pImageCompiler->m_Props.GetReduceResolutionFile(savedReduceMap);

			for (int i = Reduce::kMinReduce; i <= Reduce::kMaxReduce; ++i)
			{
				char str[128];
				cry_sprintf(str, "%i", i);

				{
					CImageProperties::ReduceMap workReduceMap;
					Reduce::s_pImageCompiler->m_Props.ConvertReduceStringToMap(string(str), workReduceMap);
					Reduce::s_pImageCompiler->m_Props.SetReduceResolutionFile(workReduceMap);
				}

				const int arbitraryPlatform = 0;

				EPixelFormat finalFormat;
				bool bFinalCubemap;
				uint32 dwFinalMipCount;
				uint32 dwFinalReduce;
				uint32 dwWidth;
				uint32 dwHeight;

				Reduce::s_pImageCompiler->GetFinalImageInfo(arbitraryPlatform, finalFormat, bFinalCubemap, dwFinalMipCount, dwFinalReduce, dwWidth, dwHeight);

				cry_sprintf(str, "%u x %u  (%i)", dwWidth, dwHeight, i);

				int id;
				switch (i - Reduce::kMinReduce)
				{
				case 0:  id = IDC_CHOOSERESOLUTION_BTN0; break;
				case 1:  id = IDC_CHOOSERESOLUTION_BTN1; break;
				case 2:  id = IDC_CHOOSERESOLUTION_BTN2; break;
				case 3:  id = IDC_CHOOSERESOLUTION_BTN3; break;
				case 4:  id = IDC_CHOOSERESOLUTION_BTN4; break;
				case 5:  id = IDC_CHOOSERESOLUTION_BTN5; break;
				case 6:  id = IDC_CHOOSERESOLUTION_BTN6; break;
				default: id = IDC_CHOOSERESOLUTION_BTN7; break;
				}
				const HWND h = GetDlgItem(hWnd, id);
				SetWindowText(h, str);
			}

			Reduce::s_pImageCompiler->m_Props.SetReduceResolutionFile(savedReduceMap);
			return TRUE;
		}

	case WM_COMMAND:
		{
			int idx;

			switch (LOWORD(wParam))
			{
			case IDC_CHOOSERESOLUTION_BTN0: idx = 0; break;
			case IDC_CHOOSERESOLUTION_BTN1: idx = 1; break;
			case IDC_CHOOSERESOLUTION_BTN2: idx = 2; break;
			case IDC_CHOOSERESOLUTION_BTN3: idx = 3; break;
			case IDC_CHOOSERESOLUTION_BTN4: idx = 4; break;
			case IDC_CHOOSERESOLUTION_BTN5: idx = 5; break;
			case IDC_CHOOSERESOLUTION_BTN6: idx = 6; break;
			case IDC_CHOOSERESOLUTION_BTN7: idx = 7; break;
			case IDCANCEL: idx = -1; break;
			default: idx = -2; break;
			}

			if (idx >= -1)
			{
				EndDialog(hWnd, idx);
				return FALSE;
			}

			return TRUE;
		}

	case WM_CLOSE:
		// Alt-F4 or [x] or System menu's Close or something else
		EndDialog(hWnd, -1);
		return FALSE;

	default:
		return FALSE;
	}
}


INT_PTR CALLBACK CImageUserDialog::WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	CImageUserDialog *This = reinterpret_cast<CImageUserDialog*>((intptr_t)(LONG_PTR)GetWindowLongPtr(hWnd,GWLP_USERDATA));

	switch(uMsg)
	{
		case WM_HSCROLL:
			if(	LOWORD(wParam)==TB_PAGEDOWN || LOWORD(wParam)==TB_PAGEUP ||				//
					LOWORD(wParam)==TB_THUMBTRACK ||																	// mouse drag
					LOWORD(wParam)==TB_LINEDOWN || LOWORD(wParam)==TB_LINEUP ||				//
					LOWORD(wParam)==TB_THUMBPOSITION)																	// mousewheel
			{
				This->TriggerUpdatePreview(false);
			}
			return TRUE;

		case WM_NOTIFY:
			{
				WORD wID=LOWORD(wParam);

				switch(wID)
				{
				case IDC_PROPTAB:
					if (This) 
					{
						LockHandle lh(This->Lock());

						HWND hwnd=GetDlgItem(hWnd,IDC_PROPTAB);
						assert(hwnd);
						int iPage = TabCtrl_GetCurSel(hwnd); 

						This->SetPropertyTab(iPage);
					}
					return TRUE;
				}
			}
			return FALSE;

		case WM_COMMAND:											//
			{
				LockHandle lh(This->Lock());

				WORD wID=LOWORD(wParam);

				switch(wID)
				{
					// TODO: Move generation to main thread.
					case IDC_GENERATEOUTPUT:
						This->TriggerGenerateOutput(false);
						return(TRUE);
					case IDC_ABOUT:
						{
							char buffer[2048];
							This->m_pImageCompiler->m_CC.pRC->GetGenericInfo(buffer, sizeof(buffer), "\r\n");
							MessageBoxA(hWnd, buffer, "About",  MB_OK);
						}
						return(TRUE);
					case IDOK:
						This->TriggerSaveConfig(true);
						return(TRUE);

					case IDCANCEL:
//						PostQuitMessage(0);
						This->TriggerCancel();
						return(TRUE);

					case IDC_ZOOMIN:
						This->MouseMessage(0,0,0,-WHEEL_DELTA);
						return(TRUE);

					case IDC_ZOOMOUT:
						This->MouseMessage(0,0,0,WHEEL_DELTA);
						return(TRUE);

					case IDC_BTN_SHOWPRESETINFO:
						{
							HWND hWnd2 = GetDlgItem(hWnd, IDC_TEMPLATECOMBO);
							assert(hWnd2);
							const int index = ComboBox_GetCurSel(hWnd2);
							const int length = ComboBox_GetLBTextLen(hWnd2, index);
							char presetName[128];
							if (length <= 0 || 
								length + 1 > sizeof(presetName) ||
								length != ComboBox_GetLBText(hWnd2, index, presetName))
							{
								assert(0);
								return TRUE;
							}

							const CImageCompiler* const pIC = This->m_pImageCompiler;
							const int sectionIndex = pIC->m_CC.pRC->GetIniFile()->FindSection(presetName);

							if (sectionIndex < 0)
							{
								assert(0);
								return TRUE;
							}

							SimpleTextConfigSink sink;
							pIC->m_CC.pRC->GetIniFile()->CopySectionKeysToConfig(eCP_PriorityRcIni, sectionIndex, 0, &sink);

							const string title = string("Preset: ") + presetName;
							MessageBoxA(hWnd, sink.m_text.c_str(), title, MB_OK);
						}
						return TRUE;

					case IDC_RESOLUTION_BTN_CHOOSE:
					case IDC_RESOLUTION_BTN_HIGHER:
					case IDC_RESOLUTION_BTN_LOWER:
						{
							const int selCount = Reduce::grabListboxSelection(This->m_hTab_Resolution, Reduce::s_platforms);
							if (selCount <= 0)
							{
								MessageBoxA(hWnd, "You must select at least one row in the list first.", "RC",  MB_OK | MB_ICONWARNING);
								return TRUE;
							}

							if (wID == IDC_RESOLUTION_BTN_CHOOSE)
							{
								Reduce::s_pImageCompiler = This->m_pImageCompiler;
								const int res = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CHOOSERESOLUTION), hWnd, WndProc_ChooseResolution);
								Reduce::s_pImageCompiler = 0;
								if (res < 0)
								{
									return TRUE;									
								}
								assert(Reduce::kMinReduce + res <= Reduce::kMaxReduce);
								for (int i = 0; i < (int)Reduce::s_platforms.size(); ++i)
								{
									if (Reduce::s_platforms[i].bMark)
									{
										Reduce::s_platforms[i].reduceValue = Reduce::kMinReduce + res;
									}
								}
							}
							else
							{
								const int add = (wID == IDC_RESOLUTION_BTN_HIGHER) ? -1 : +1;
								for (int i = 0; i < (int)Reduce::s_platforms.size(); ++i)
								{
									if (Reduce::s_platforms[i].bMark)
									{
										int newValue = Reduce::s_platforms[i].reduceValue + add;
										Util::clampMinMax(newValue, (int)Reduce::kMinReduce, (int)Reduce::kMaxReduce);
										Reduce::s_platforms[i].reduceValue = newValue;
									}
								}
							}

							Reduce::applyChangesAndFillListbox(This->m_pImageCompiler, This->m_hTab_Resolution, Reduce::s_platforms);

							This->TriggerUpdatePreview(true);
						}
						return(TRUE);

					case IDC_RESOLUTION_BTN_DELPLATFORM:
						{
							const int selCount = Reduce::grabListboxSelection(This->m_hTab_Resolution, Reduce::s_platforms);
							if (selCount <= 0)
							{
								MessageBoxA(hWnd, "You must select at least one row in the list first.", "RC",  MB_OK | MB_ICONWARNING);
								return TRUE;
							}

							for (int i = 0; i < (int)Reduce::s_platforms.size(); ++i)
							{
								if (Reduce::s_platforms[i].bMark && Reduce::s_platforms[i].platform >= 0)
								{
									MessageBoxA(hWnd, "Deleting active platforms is not allowed.", "RC",  MB_OK | MB_ICONWARNING);
									return TRUE;
								}
							}

							for (int i = 0; i < (int)Reduce::s_platforms.size(); ++i)
							{
								if (Reduce::s_platforms[i].bMark)
								{
									Reduce::s_platforms.erase(Reduce::s_platforms.begin() + i);
									--i;
								}
							}
							
							Reduce::applyChangesAndFillListbox(This->m_pImageCompiler, This->m_hTab_Resolution, Reduce::s_platforms);

							This->TriggerUpdatePreview(true);
						}
						return(TRUE);

					// check box change, update all
					case IDC_PRESETSHOWALL:								// preset-filtering has changed
						// refill and select the preset
						{
							HWND hWnd2 = GetDlgItem(hWnd, IDC_PRESETSHOWALL);
							assert(hWnd2);
							This->m_bShowAllPresets = (Button_GetCheck(hWnd2) != 0);
							This->SelectPreset(This->m_pImageCompiler->m_Props.GetPreset());
						}
						return(TRUE);

					// combo box change, update all
					case IDC_TEMPLATECOMBO:									// preset has changed
					case IDC_MIPMAPS:                       // mipmaps have changed
					case IDC_MIPMETHOD:                     // mipmap filtering method has changed
					case IDC_MIPOP:                         // mipmap operation method has changed
						// prevent refresh when opening the drop-out
						if(HIWORD(wParam)==CBN_SELCHANGE)
						{
							Reduce::grabListboxSelection(This->m_hTab_Resolution, Reduce::s_platforms);
							This->TriggerUpdatePreview(true);
						}
						return(TRUE);

					// check box change, update all
					case IDC_SUPRESSENGINEREDUCE:           // image flags have changed
					case IDC_MAINTAINALPHACOVERAGE:         // maintainalphacoverage has changed
					case IDC_NM_BUMPINVERT:                 // bump map invert has changed
						Reduce::grabListboxSelection(This->m_hTab_Resolution, Reduce::s_platforms);
						This->TriggerUpdatePreview(true);
						return(TRUE);

					case IDC_PREVIEWMODE:										// preview-mode has changed
						// prevent refresh when opening the drop-out
						if(HIWORD(wParam)==CBN_SELCHANGE)
						{
							Reduce::grabListboxSelection(This->m_hTab_Resolution, Reduce::s_platforms);
							This->TriggerUpdatePreview(false);
						}
						return(TRUE);

					// preview changes
					case IDC_PREVIEWON:
					case IDC_PREVIEWTILED:
					case IDC_PREVIEWFILTERED:
						Reduce::grabListboxSelection(This->m_hTab_Resolution, Reduce::s_platforms);
						This->TriggerUpdatePreview(false);
						return(TRUE);
				}
			}
			return(TRUE);

		// set the color for warning line
		case WM_CTLCOLORSTATIC:
			if ((HWND)lParam == GetDlgItem(hWnd,IDC_WARNINGLINE))
			{
				SetBkMode((HDC)wParam,TRANSPARENT);
				SetTextColor((HDC)wParam, RGB(255,0,0));
				return (INT_PTR)CreateSolidBrush(GetSysColor(COLOR_MENU));
			}
			return(TRUE);

		case WM_PAINT:											//
			{
				const BOOL bErase = FALSE;
				if (GetUpdateRect(hWnd, NULL, bErase) != 0)
				{
					PAINTSTRUCT paint;
					BeginPaint(hWnd, &paint);
					EndPaint(hWnd, &paint);
					if (This)
					{
						This->TriggerRepaint();
					}
				}
				return(FALSE);
			}

		case WM_ERASEBKGND:
			return(FALSE);

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			{
				assert(This);
				if(!This)return(FALSE);
				LockHandle lh(This->Lock());

				int iY=(int)HIWORD(lParam);

				if(iY<=This->m_iPreviewHeight)
				{
					This->m_RelMouse.OnButtonDown(hWnd);

					SetFocus(This->m_hWindow);
				}
			}
			return(TRUE);

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
			{
				assert(This);
				if(!This)return(FALSE);
				LockHandle lh(This->Lock());

				if(wParam==0)
					This->m_RelMouse.OnButtonUp();	
			}
			return(TRUE);

		case WM_MOUSEMOVE:
			{
				assert(This);
				if(!This)return(FALSE);
				LockHandle lh(This->Lock());

				bool bButtonDown=(wParam&MK_LBUTTON)!=0 || (wParam&MK_MBUTTON)!=0 || (wParam&MK_RBUTTON)!=0;

				int iY=(int)HIWORD(lParam);

				if(iY<=This->m_iPreviewHeight || This->m_RelMouse.IsCaptured())
				{
					int relX,relY;

					This->m_RelMouse.OnMouseMove(hWnd,bButtonDown,relX,relY);
					This->MouseMessage((DWORD)wParam,relX,relY,0);
				}
			}
			return(TRUE);

		case WM_MOUSEWHEEL:
			{
				assert(This);
				if(!This)return(FALSE);
				LockHandle lh(This->Lock());

				int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

				This->MouseMessage(0,0,0,-zDelta);
			}
			return(TRUE);

		case WM_SHOWWINDOW:
			// HACK: return TRUE to tell Windows that we didn't process the message (it helps
			// to avoid *some* cases of freezing our dialogs when windows or tabs are switching).
			return(TRUE);

		case WM_ACTIVATE:
			// HACK: return TRUE to tell Windows that we didn't process the message (it helps
			// to avoid *some* cases of freezing our dialogs when windows are switching).
			return(TRUE);

		case WM_CLOSE:
			{
				LockHandle lh(This->Lock());
				This->m_quitRequest = QuitRequestUserCancel;
			}
			return (TRUE);
	}

	return FALSE;
}

DWORD CImageUserDialog::ThreadStart_Static(void* pThis)
{
	return static_cast<CImageUserDialog*>(pThis)->ThreadStart();
}

DWORD CImageUserDialog::ThreadStart()
{
	// zoom out if image is too big
	GetInputExtentAndMaximumZoom();

	m_PreviewData.iScale64 = m_PreviewData.iInitialScale64;

	m_hWindow = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_IMAGEUSERDIALOG), 0, CImageUserDialog::WndProc);
	if (!m_hWindow) 
	{
		m_pImageCompiler = 0;
		s_bDialogInitialized = true;
		return 1;
	}

	// store this pointer	
	SetWindowLongPtr(m_hWindow, GWLP_USERDATA, (LONG_PTR)this);

	CreateDialogItems();
	CenterWindow(m_hWindow);

	SetDataToDialog(true);

	UpdateWindowTitle();
	TriggerUpdatePreview(true);

	// bringing the dialog to the top (otherwise, if a key is in pressed state
	// during launching RC from Photoshop, the dialog will be occluded
	// by the Photoshop's window)
	SetWindowPos(m_hWindow, HWND_TOPMOST  , 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetWindowPos(m_hWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

	// update preview and warning subtitles
	{
		string sLeftInfo = m_pImageCompiler->GetInfoStringUI(true);

		HWND hwnd1 = GetDlgItem(m_hWindow, IDC_LEFTPREVIEWINFO);     assert(hwnd1);
		HWND hwnd2 = GetDlgItem(m_hWindow, IDC_RIGHTPREVIEWINFO);    assert(hwnd2);
		HWND hwndW = GetDlgItem(m_hWindow, IDC_WARNINGLINE);         assert(hwndW);

		SetWindowText(hwnd1, sLeftInfo.c_str());
		SetWindowText(hwnd2, "");
		SetWindowText(hwndW, "");
	}

	s_bDialogInitialized = true;

	// message loop
	while (m_quitRequest == QuitRequestNone)
	{
		MSG Msg;

		// using PeekMessage instead of blocking GetMessage call to be able to recognize quitting without getting input
		if (PeekMessage(&Msg, NULL, 0, WM_USER, PM_REMOVE))
		{
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}

		Sleep(1);
	}

	DestroyWindow(m_hWindow);
	m_hWindow = 0;

	return (m_quitRequest == QuitRequestNormal) ? 0 : 1;
}

void CImageUserDialog::TriggerUpdatePreview(bool bFull)
{
	LOCK_MONITOR();

	HWND hwnd = GetDlgItem(m_hWindow, IDC_PREVIEWON); 
	assert(hwnd);

	m_pImageCompiler->m_Props.m_bPreview = (Button_GetCheck(hwnd) != 0);
	if (m_pImageCompiler->m_Props.m_bPreview)
	{
		m_eWorkerAction = (bFull ? WorkerActionUpdatePreview : WorkerActionUpdatePreviewOnly);
	}
	else
	{
		if (!m_bDialogIsUpdating)
		{
			GetDataFromDialog();
			SetDataToDialog();
			GetDataFromDialog();
		}
	}
}

void CImageUserDialog::TriggerGenerateOutput(bool bQuit)
{
	LOCK_MONITOR();

	if (bQuit)
		m_eWorkerAction = WorkerActionGenerateOutputAndQuit;
	else
		m_eWorkerAction = WorkerActionGenerateOutput;
}

void CImageUserDialog::TriggerSaveConfig(bool bQuit)
{
	LOCK_MONITOR();

	if (bQuit)
		m_eWorkerAction = WorkerActionSaveConfigAndQuit;
	else
		m_eWorkerAction = WorkerActionSaveConfig;
}

void CImageUserDialog::TriggerRepaint()
{
	LOCK_MONITOR();

	if (m_eWorkerAction == WorkerActionNone)
		m_eWorkerAction = WorkerActionPaint;
}

CImageUserDialog::WorkerAction CImageUserDialog::CheckAndResetWorkerAction()
{
	LOCK_MONITOR();

	WorkerAction eAction = m_eWorkerAction;
	m_eWorkerAction = WorkerActionNone;
	return eAction;
}

void CImageUserDialog::OnTinyDocumentChanged(TinyDocument<float>* pDocument)
{
	TriggerUpdatePreview(true);
}

void CImageUserDialog::OnTinyDocumentChanged(TinyDocument<bool>* pDocument)
{
	TriggerUpdatePreview(true);
}

void CImageUserDialog::TriggerCancel()
{
	LOCK_MONITOR();

	m_eWorkerAction = WorkerActionQuit;
}
