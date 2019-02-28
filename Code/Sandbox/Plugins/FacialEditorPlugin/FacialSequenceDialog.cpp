// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>
#include "FacialSequenceDialog.h"
#include "Dialogs/QStringDialog.h"
#include "MultiLineStringDlg.h"
#include "PhonemeAnalyzer.h"
#include <CrySandbox/ScopedVariableSetter.h>
#include "Controls/TreeCtrlUtils.h"
#include "Controls/QuestionDialog.h"
#include <CryCore/Containers/VectorSet.h>
#include "FilePathUtil.h"
#include "Util/MFCUtil.h"
#include "IUndoManager.h"
#include "Resource.h"
#include <QDialogButtonBox>

#define IDC_ANIM_SEQUENCES AFX_IDW_PANE_FIRST
#define IDC_ANIM_CURVES    AFX_IDW_PANE_FIRST
#define IDC_WAVE_CTRL      AFX_IDW_PANE_FIRST + 16

#define IDC_TIMELINE       11
#define IDC_PHONEMES       12

IMPLEMENT_DYNAMIC(CFacialChannelTreeCtrl, CXTTreeCtrl)
IMPLEMENT_DYNAMIC(CFacialSequenceDialog, CCustomFrameWnd)

#define IDC_TREE_CONTROL        AFX_IDW_PANE_FIRST
#define IDC_TABCTRL             AFX_IDW_PANE_FIRST + 1

#define ITEM_COLOR_DISABLED     RGB(160, 160, 160)
#define ITEM_COLOR_MORPH_TARGET RGB(0, 0, 160)

//////////////////////////////////////////////////////////////////////////
string CreateIDForChannel(IFacialAnimChannel* pChannel)
{
	string id;
	id.reserve(1000);
	for (IFacialAnimChannel* pAncestor = pChannel; pAncestor; pAncestor = pAncestor->GetParent())
	{
		id += pAncestor->GetName();
		id += "*(";
	}
	return id;
}

class CSequenceDialogDropTarget : public COleDropTarget
{
public:
	CSequenceDialogDropTarget(CFacialSequenceDialog* dlg) : m_dlg(dlg) {}

	virtual DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		point = ConvertToTreeClientCoords(pWnd, point);
		if (m_dlg->m_pContext->GetChannelDescriptorFromDataSource(pDataObject).size() != 0)
			return DROPEFFECT_MOVE;
		else if (m_dlg->m_pContext->GetExpressionNameFromDataSource(pDataObject).size() != 0)
			return DROPEFFECT_COPY;
		return DROPEFFECT_NONE;
	}

	virtual void OnDragLeave(CWnd* pWnd)
	{
	}

	virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		point = ConvertToTreeClientCoords(pWnd, point);
		if (m_dlg->m_pContext->GetChannelDescriptorFromDataSource(pDataObject).size() != 0)
			return DROPEFFECT_MOVE;
		else if (m_dlg->m_pContext->GetExpressionNameFromDataSource(pDataObject).size() != 0)
			return DROPEFFECT_COPY;
		return DROPEFFECT_NONE;
	}

	virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		point = ConvertToTreeClientCoords(pWnd, point);
		HTREEITEM hItem = m_dlg->m_channelsCtrl.HitTest(point);

		if (!hItem)
		{
			// fixed root node point - this makes sure you can drop expression anywhere in the sequence window and a new node will be created
			point = ConvertToTreeClientCoords(pWnd, CPoint(57, 37));
			hItem = m_dlg->m_channelsCtrl.HitTest(point);
		}

		IFacialAnimChannel* pChannel = (IFacialAnimChannel*)m_dlg->m_channelsCtrl.GetItemData(hItem);
		while (pChannel && !pChannel->IsGroup())
			pChannel = pChannel->GetParent();
		if (m_dlg->m_pContext)
		{
			m_dlg->m_pContext->HandleDropToSequence(pChannel, pDataObject);
			return TRUE;
		}
		return FALSE;
	}

private:
	CPoint ConvertToTreeClientCoords(CWnd* pWnd, const CPoint& point)
	{
		CPoint point2 = point;
		pWnd->ClientToScreen(&point2);
		m_dlg->m_channelsCtrl.ScreenToClient(&point2);
		return point2;
	}

	CFacialSequenceDialog* m_dlg;
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFacialChannelTreeCtrl, CXTTreeCtrl)
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

CFacialChannelTreeCtrl::CFacialChannelTreeCtrl()
{
	m_pContext = 0;
}

CFacialChannelTreeCtrl::~CFacialChannelTreeCtrl()
{}

//////////////////////////////////////////////////////////////////////////
void CFacialChannelTreeCtrl::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;

	if (m_hWnd)
		Reload();
};

//////////////////////////////////////////////////////////////////////////
void CFacialChannelTreeCtrl::Reload()
{
	VectorSet<string> selectedChannelIDs;
	selectedChannelIDs.SwapElementsWithVector(m_selectedChannelIDs);

	if (!m_imageList.GetSafeHandle())
	{
		CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_FACED_LIBTREE, 16, RGB(255, 0, 255));
		SetImageList(&m_imageList, TVSIL_NORMAL);
	}

	SetRedraw(FALSE);
	DeleteAllItems();
	m_itemsMap.clear();

	if (!m_pContext || !m_pContext->pLibrary)
		return;

	int nImage = 0;
	COLORREF col = 0;

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	if (pSequence)
	{
		HTREEITEM hRoot = InsertItem("Root", nImage, nImage, TVI_ROOT);

		int i;
		std::vector<IFacialAnimChannel*> channels;
		channels.resize(pSequence->GetChannelCount());
		for (i = 0; i < pSequence->GetChannelCount(); i++)
			channels[i] = pSequence->GetChannel(i);

		while (!channels.empty())
		{
			IFacialAnimChannel* pChannel = channels.back();

			HTREEITEM hParent = hRoot;
			if (pChannel->GetParent())
			{
				hParent = stl::find_in_map(m_itemsMap, pChannel->GetParent(), hRoot);
				if (hParent == hRoot) // Parent was not yet added, skip turn.
				{
					channels.pop_back();
					channels.insert(channels.begin(), pChannel);
					continue;
				}
			}

			CString name = pChannel->GetName();
			if (pChannel->IsGroup())
			{
				nImage = 0;
			}
			else
			{
				nImage = 1;
			}

			HTREEITEM hItem = InsertItem(name, nImage, nImage, hParent);

			if (col != 0)
				SetItemColor(hItem, col);

			m_itemsMap[pChannel] = hItem;
			SetItemData(hItem, (DWORD_PTR)pChannel);

			if (pChannel->GetFlags() & IFacialAnimChannel::FLAG_UI_EXTENDED)
				Expand(hItem, TVE_EXPAND);

			if (selectedChannelIDs.find(CreateIDForChannel(pChannel)) != selectedChannelIDs.end())
				SelectItem(hItem);
			channels.pop_back();

			SortChildren(hParent);
		}

		if (hRoot)
		{
			Expand(hRoot, TVE_EXPAND);
			if (selectedChannelIDs.find("") != selectedChannelIDs.end())
				SelectItem(hRoot);
		}
	}

	SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CFacialChannelTreeCtrl::SelectChannel(IFacialAnimChannel* pChannel, bool bExclusive)
{
	if (bExclusive)
	{
		SelectAll(FALSE);
	}
	HTREEITEM hItem = stl::find_in_map(m_itemsMap, pChannel, NULL);
	if (hItem && (bExclusive || GetSelectedItem() != hItem))
	{
		SelectItem(hItem);
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialChannelTreeCtrl::GetSelectedChannel()
{
	HTREEITEM hItem = GetSelectedItem();
	if (hItem)
	{
		return (IFacialAnimChannel*)GetItemData(hItem);
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CFacialSequencePropertiesDialog
//////////////////////////////////////////////////////////////////////////
class CFacialSequencePropertiesDialog : public CDialog
{
public:
	// Dialog Data
	enum { IDD = IDD_FACIAL_SEQUENCE_TIME };

	CFacialSequencePropertiesDialog(CWnd* pParent = NULL) : CDialog(IDD, pParent)
	{
		timeRange.Set(0, 0);
	}
	~CFacialSequencePropertiesDialog()
	{
	}

public:
	bool  bRangeFromSound;
	Range timeRange;

protected:
	DECLARE_MESSAGE_MAP();
	virtual void DoDataExchange(CDataExchange* pDX)
	{
		CDialog::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_FROM_SOUND, m_fromSoundBtn);
	}

	virtual BOOL OnInitDialog()
	{
		CDialog::OnInitDialog();

		m_startTimeCtrl.Create(this, IDC_START_TIME);
		m_endTimeCtrl.Create(this, IDC_END_TIME);

		m_startTimeCtrl.SetRange(-(1e+10), (1e+10));
		m_startTimeCtrl.SetInteger(true);
		m_endTimeCtrl.SetRange(-(1e+10), (1e+10));
		m_endTimeCtrl.SetInteger(true);

		m_startTimeCtrl.SetValue(timeRange.start * FACIAL_EDITOR_FPS);
		m_endTimeCtrl.SetValue(timeRange.end * FACIAL_EDITOR_FPS);

		m_fromSoundBtn.SetCheck((bRangeFromSound) ? BST_CHECKED : BST_UNCHECKED);
		if (bRangeFromSound)
		{
			m_startTimeCtrl.EnableWindow(FALSE);
			m_endTimeCtrl.EnableWindow(FALSE);
		}

		UpdateData(TRUE);
		return TRUE;
	}
	virtual void OnOK()
	{
		bRangeFromSound = m_fromSoundBtn.GetCheck() == BST_CHECKED;
		timeRange.start = m_startTimeCtrl.GetValue() / FACIAL_EDITOR_FPS;
		timeRange.end = m_endTimeCtrl.GetValue() / FACIAL_EDITOR_FPS;
		CDialog::OnOK();
	}

	afx_msg void OnFromSound()
	{
		bRangeFromSound = m_fromSoundBtn.GetCheck() == BST_CHECKED;
		if (bRangeFromSound)
		{
			m_startTimeCtrl.EnableWindow(FALSE);
			m_endTimeCtrl.EnableWindow(FALSE);
		}
		else
		{
			m_startTimeCtrl.EnableWindow(TRUE);
			m_endTimeCtrl.EnableWindow(TRUE);
		}
	}

private:
	CNumberCtrl m_startTimeCtrl;
	CNumberCtrl m_endTimeCtrl;
	CButton     m_fromSoundBtn;
};
BEGIN_MESSAGE_MAP(CFacialSequencePropertiesDialog, CDialog)
ON_COMMAND(IDC_FROM_SOUND, OnFromSound)
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFacialSequenceDialog, CCustomFrameWnd)
ON_WM_SIZE()
ON_NOTIFY(NM_RCLICK, IDC_ANIM_SEQUENCES, OnTreeRClick)
ON_NOTIFY(TVN_SELCHANGED, IDC_ANIM_SEQUENCES, OnTreeSelChanged)
ON_NOTIFY(TVN_ITEMEXPANDED, IDC_ANIM_SEQUENCES, OnTreeItemExpanded)
ON_NOTIFY(TLN_START_CHANGE, IDC_TIMELINE, OnTimelineChangeStart)
ON_NOTIFY(SPLN_TIME_START_CHANGE, IDC_ANIM_CURVES, OnTimelineChangeStart)
ON_NOTIFY(TLN_END_CHANGE, IDC_TIMELINE, OnTimelineChangeEnd)
ON_NOTIFY(SPLN_TIME_START_CHANGE, IDC_ANIM_CURVES, OnTimelineChangeEnd)
ON_NOTIFY(TLN_CHANGE, IDC_TIMELINE, OnTimelineChange)
ON_NOTIFY(TLN_DELETE, IDC_TIMELINE, OnTimelineDeletePressed)

ON_NOTIFY(SPLN_BEFORE_CHANGE, IDC_ANIM_CURVES, OnSplineBeginChange)
ON_NOTIFY(SPLN_CHANGE, IDC_ANIM_CURVES, OnSplineChange)
ON_NOTIFY(SPLN_SCROLL_ZOOM, IDC_ANIM_CURVES, OnSplineScrollZoom)
ON_NOTIFY(SPLN_TIME_CHANGE, IDC_ANIM_CURVES, OnSplineTimeChange)
ON_NOTIFY(WAVCTRLN_SCROLL_ZOOM, IDC_WAVE_CTRL, OnWaveScrollZoom)
ON_NOTIFY(WAVCTRLN_TIME_CHANGE, IDC_WAVE_CTRL, OnWaveCtrlTimeChange)
ON_NOTIFY(WAVECTRLN_RCLICK, IDC_WAVE_CTRL, OnWaveRClick)
ON_NOTIFY(WAVECTRLN_BEGIN_MOVE_WAVEFORM, IDC_WAVE_CTRL, OnBeginMoveWaveform)
ON_NOTIFY(WAVECTRLN_MOVE_WAVEFORMS, IDC_WAVE_CTRL, OnMoveWaveforms)
ON_NOTIFY(WAVECTRLN_RESET_CHANGES, IDC_WAVE_CTRL, OnResetWaveformChanges)
ON_NOTIFY(WAVECTRLN_END_MOVE_WAVEFORM, IDC_WAVE_CTRL, OnEndWaveformChanges)

ON_NOTIFY(PHONEMECTRLN_BEFORE_CHANGE, IDC_PHONEMES, OnPhonemesBeforeChange)
ON_NOTIFY(PHONEMECTRLN_CHANGE, IDC_PHONEMES, OnPhonemesChange)
ON_NOTIFY(PHONEMECTRLN_PREVIEW, IDC_PHONEMES, OnPhonemesPreview)
ON_NOTIFY(PHONEMECTRLN_CLEAR, IDC_PHONEMES, OnPhonemesClear)
ON_NOTIFY(PHONEMECTRLN_BAKE, IDC_PHONEMES, OnPhonemesBake)

ON_COMMAND_RANGE(ID_TANGENT_IN_ZERO, ID_SPLINE_SNAP_GRID_Y, OnSplineCmd)
ON_UPDATE_COMMAND_UI_RANGE(ID_TANGENT_IN_ZERO, ID_SPLINE_SNAP_GRID_Y, OnSplineCmdUpdateUI)

ON_COMMAND(ID_FACE_SEQ_PLAY, OnPlay)
ON_UPDATE_COMMAND_UI(ID_FACE_SEQ_PLAY, OnPlayUpdate)
ON_COMMAND(ID_FACE_SEQ_STOP, OnStop)
ON_COMMAND(ID_FACE_SEQ_PROPERTIES, OnSequenceProperties)
ON_COMMAND(ID_FACE_SEQ_SPEED, OnSpeedChange)
ON_EN_KILLFOCUS(ID_FACE_SEQ_SPEED, OnSpeedChange)
ON_COMMAND(ID_FACE_SEQ_FRAME, OnFrameChange)
ON_EN_KILLFOCUS(ID_FACE_SEQ_FRAME, OnFrameChange)

ON_COMMAND(ID_ZERO_ALL, OnZeroAll)
ON_COMMAND(ID_KEY_ALL, OnKeyAll)
ON_COMMAND(ID_FACE_SEQ_SKEL_ANIM, OnSkeletonAnim)
ON_COMMAND(ID_FACE_SEQ_CAM_ANIM, OnCameraAnim)
ON_COMMAND(ID_FACE_SEQ_SOUND_OVERLAP, OnOverlapSounds)
ON_COMMAND(ID_CONTROL_AMPLITUDE, OnControlAmplitude)
ON_COMMAND(ID_SMOOTH_SPLINE_VALUE, OnSmoothSplineSigmaChange)
ON_COMMAND(ID_KEY_CLEANUP_SPLINE_VALUE, OnKeyCleanupErrorMaxChange)
ON_COMMAND(ID_REMOVE_NOISE_SPLINE_VALUE, OnRemoveNoiseSigmaChange)
ON_COMMAND(ID_SMOOTH_SPLINE, SmoothKeys)
ON_COMMAND(ID_KEY_CLEANUP_SPLINE, CleanupKeys)
ON_COMMAND(ID_REMOVE_NOISE_SPLINE, RemoveNoiseFromKeys)

ON_WM_MOUSEWHEEL()

ON_WM_DESTROY()
ON_NOTIFY(TVN_BEGINDRAG, IDC_ANIM_SEQUENCES, OnBeginDrag)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// CFacialSequenceDialog
//////////////////////////////////////////////////////////////////////////
CFacialSequenceDialog::CFacialSequenceDialog()
	: m_bIgnoreSplineChangeEvents(false)
{
	m_pContext = 0;
	m_pTimeEdit = 0;
	m_pFrameEdit = 0;
	m_pCurrent = 0;
	GetIEditor()->RegisterNotifyListener(this);

	m_pDropTarget = 0;
	m_pAnimSkeletonButton = 0;
	m_pAnimCameraButton = 0;
	m_pOverlapSoundsButton = 0;
	m_pControlAmplitudeButton = 0;

	m_fSmoothingSigma = 0.1f;
	m_fNoiseThreshold = 0.1f;
	m_fKeyCleanupThreshold = 0.1f;
}

//////////////////////////////////////////////////////////////////////////
CFacialSequenceDialog::~CFacialSequenceDialog()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::Update()
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	float fTimelineLength = (m_pContext ? m_pContext->GetTimelineLength() : 0);
	float fCurrentTime = (m_pContext ? m_pContext->GetSequenceTime() : 0.0f);
	m_waveCtrl.UpdatePlayback();
	float fSoundTime = (m_waveCtrl ? m_waveCtrl.GetTimeMarker() : fCurrentTime);

	//{
	//	char buffer[1024];
	//	cry_sprintf(buffer, "Getting time = %f\n", m_pContext->GetSequenceTime());
	//	OutputDebugString(buffer);
	//}
	if (m_waveCtrl && pSequence && fSoundTime > fTimelineLength)
	{
		m_waveCtrl.StopPlayback();
		m_waveCtrl.StartPlayback();
		fSoundTime = 0.0f;
	}

	if (m_pContext && fCurrentTime != fSoundTime)
	{
		//CryLogAlways("Update(): fSoundTime = %f", fSoundTime);
		m_pContext->SetSequenceTime(fSoundTime);
	}

}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::LoadSequenceSound(const string& filename)
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	if (m_pContext && pSequence)
	{
		{
			CUndo undo("Load Sound File");
			m_pContext->StoreSequenceUndo();

			int soundEntryIndex = pSequence->GetSoundEntryCount();
			pSequence->InsertSoundEntry(soundEntryIndex);
			pSequence->GetSoundEntry(soundEntryIndex)->SetSoundFile(filename);
			pSequence->GetSoundEntry(soundEntryIndex)->SetStartTime(m_waveCtrl.FindEndOfWaveforms());

			m_pContext->bSequenceModfied = true;
		}

		m_pContext->SendEvent(EFD_EVENT_SOUND_CHANGE, 0);
		bool bRangeFromSound = pSequence->GetFlags() & IFacialAnimSequence::FLAG_RANGE_FROM_SOUND;
		if (bRangeFromSound)
		{
			m_pContext->GetSequence()->SetTimeRange(Range(0, m_waveCtrl.CalculateTimeRange()));
			m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::KeyAllSplines()
{
	m_splineCtrl.KeyAll();
	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::SelectAllKeys()
{
	m_splineCtrl.SelectAll();
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialSequenceDialog::OnInitDialog()
{
	BOOL bRes = __super::OnInitDialog();

	m_pDropTarget = new CSequenceDialogDropTarget(this);
	m_pDropTarget->Register(this);

	LoadAccelTable(MAKEINTRESOURCE(IDR_FACED_MENU));

	CRect rc;
	GetClientRect(rc);

	//////////////////////////////////////////////////////////////////////////
	// Initialize the command bars
	if (!InitCommandBars())
		return FALSE;

	//////////////////////////////////////////////////////////////////////////
	CXTPToolBar* pSplineBar = GetCommandBars()->Add(_T("Spline Toolbar"), xtpBarTop);
	VERIFY(pSplineBar->LoadToolBar(IDR_SPLINE_EDIT_BAR));

	for (int i = 0; i < pSplineBar->GetControlCount(); ++i)
	{
		// remove useless buttons
		if (pSplineBar->GetControl(i)->GetID() < ID_RECORD)
		{
			pSplineBar->GetControls()->Remove(i);
			--i;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	CXTPToolBar* pSequenceBar = GetCommandBars()->Add(_T("Sequence Toolbar"), xtpBarTop);
	VERIFY(pSequenceBar->LoadToolBar(IDR_FACE_SEQUENCE));
	pSequenceBar->SetFlags(xtpFlagRightAlign);
	//////////////////////////////////////////////////////////////////////////

	CXTPToolBar* pSplineExtraBar = GetCommandBars()->Add(_T("Spline Extra Toolbar"), xtpBarTop);
	VERIFY(pSplineExtraBar->LoadToolBar(IDR_SPLINE_EXTRA_TOOLBAR));

	// Initialize sequence controls.
	{
		struct ButtonDescription
		{
			ButtonDescription(CXTPToolBar* pToolbar, CXTPControlEdit*& control, int id, XTPControlType type, const string& text, int width, unsigned flags)
				: pToolbar(pToolbar), control(control), id(id), type(type), text(text), width(width), flags(flags) {}
			CXTPToolBar*      pToolbar;
			XTPControlType    type;
			string            text;
			int               width;
			CXTPControlEdit*& control;
			int               id;
			unsigned          flags;
		};
		ButtonDescription buttonDescriptions[] = {
			ButtonDescription(pSequenceBar,    m_pTimeEdit,             ID_FACE_SEQ_TIME,             xtpControlEdit, "000:00.000", 80, xtpFlagManualUpdate),
			ButtonDescription(pSequenceBar,    m_pFrameEdit,            ID_FACE_SEQ_FRAME,            xtpControlEdit, "0",          60, xtpFlagManualUpdate),
			ButtonDescription(pSequenceBar,    m_pSpeedEdit,            ID_FACE_SEQ_SPEED,            xtpControlEdit, "100",        40, 0),
			ButtonDescription(pSplineExtraBar, m_pSmoothValueEdit,      ID_SMOOTH_SPLINE_VALUE,       xtpControlEdit, "",           60, 0),
			ButtonDescription(pSplineExtraBar, m_pCleanupKeysValueEdit, ID_KEY_CLEANUP_SPLINE_VALUE,  xtpControlEdit, "",           60, 0),
			ButtonDescription(pSplineExtraBar, m_pRemoveNoiseValueEdit, ID_REMOVE_NOISE_SPLINE_VALUE, xtpControlEdit, "",           60, 0)
		};
		enum {BUTTON_DESCRIPTION_COUNT = sizeof(buttonDescriptions) / sizeof(buttonDescriptions[0])};

		for (int i = 0; i < BUTTON_DESCRIPTION_COUNT; ++i)
		{
			CXTPControl* pCtrl = buttonDescriptions[i].pToolbar->GetControls()->FindControl(xtpControlButton, buttonDescriptions[i].id, TRUE, FALSE);
			if (pCtrl)
			{
				ASSERT(buttonDescriptions[i].type == xtpControlEdit);
				CXTPControlEdit* control = (CXTPControlEdit*)buttonDescriptions[i].pToolbar->GetControls()->SetControlType(pCtrl->GetIndex(), buttonDescriptions[i].type);
				buttonDescriptions[i].control = control;
				control->ShowLabel(FALSE);
				control->SetEditText(buttonDescriptions[i].text.c_str());
				control->SetFlags(buttonDescriptions[i].flags);
				control->SetWidth(buttonDescriptions[i].width);
			}
		}
	}
	DisplayPlaybackSpeedInToolbar();

	UpdateKeyCleanupErrorMax();
	UpdateSmoothSplineSigma();
	UpdateRemoveNoiseSigma();

	{
		class ButtonDescription
		{
		public:
			ButtonDescription(CXTPToolBar* pToolbar, int id, const char* caption, CXTPControlButton** button)
				: pToolbar(pToolbar), id(id), caption(caption), button(button) {}

			CXTPToolBar*        pToolbar;
			int                 id;
			const char*         caption;
			CXTPControlButton** button;
		};

		ButtonDescription buttonDescriptions[] = {
			ButtonDescription(pSplineExtraBar, ID_ZERO_ALL,               "Zero All",         0),
			ButtonDescription(pSplineExtraBar, ID_KEY_ALL,                "Key All",          0),
			ButtonDescription(pSplineExtraBar, ID_CONTROL_AMPLITUDE,      "Amplitude",        &m_pControlAmplitudeButton),
			ButtonDescription(pSequenceBar,    ID_FACE_SEQ_SKEL_ANIM,     "Animate Skeleton", &m_pAnimSkeletonButton),
			ButtonDescription(pSequenceBar,    ID_FACE_SEQ_CAM_ANIM,      "Sequence Camera",  &m_pAnimCameraButton),
			ButtonDescription(pSequenceBar,    ID_FACE_SEQ_SOUND_OVERLAP, "Overlap Sounds",   &m_pOverlapSoundsButton),
			ButtonDescription(pSplineExtraBar, ID_SMOOTH_SPLINE,          "Smooth:",          0),
			ButtonDescription(pSplineExtraBar, ID_KEY_CLEANUP_SPLINE,     "Cleanup Keys:",    0),
			ButtonDescription(pSplineExtraBar, ID_REMOVE_NOISE_SPLINE,    "Remove Noise:",    0)
		};
		enum {NUM_BUTTONS = sizeof(buttonDescriptions) / sizeof(buttonDescriptions[0])};

		for (int buttonIndex = 0; buttonIndex < NUM_BUTTONS; ++buttonIndex)
		{
			CXTPControl* pCtrl = buttonDescriptions[buttonIndex].pToolbar->GetControls()->FindControl(xtpControlButton, buttonDescriptions[buttonIndex].id, TRUE, FALSE);
			if (pCtrl)
			{
				int nIndex = pCtrl->GetIndex();
				CXTPControlButton* pDerivedControl = static_cast<CXTPControlButton*>(buttonDescriptions[buttonIndex].pToolbar->GetControls()->SetControlType(nIndex, xtpControlButton));
				pDerivedControl->SetCaption(buttonDescriptions[buttonIndex].caption);
				pDerivedControl->SetStyle(xtpButtonCaption);
				pDerivedControl->SetEnabled(TRUE);
				if (buttonDescriptions[buttonIndex].button)
					*buttonDescriptions[buttonIndex].button = pDerivedControl;
			}
		}
	}

	UpdateSkeletonAnimationStatus();
	UpdateCameraAnimationStatus();
	UpdateOverlapSoundStatus();

	DockRightOf(pSplineBar, pSequenceBar);
	DockRightOf(pSplineExtraBar, pSplineBar);

	m_splitWnd.CreateStatic(this, 1, 2, WS_CHILD | WS_VISIBLE, AFX_IDW_PANE_FIRST);
	m_splitWnd2.CreateStatic(&m_splitWnd, 2, 1, WS_CHILD | WS_VISIBLE, AFX_IDW_PANE_FIRST);

	m_channelsCtrl.Create(WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_LINESATROOT,
	                      rc, &m_splitWnd, IDC_ANIM_SEQUENCES);
	m_channelsCtrl.EnableMultiSelect(TRUE);

	m_splineCtrl.Create(WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rc, &m_splitWnd2, IDC_ANIM_CURVES);
	m_splineCtrl.SetSplineSet(this);
	m_splineCtrl.SetValueRange(Range(-1.0f, 1.0f));
	m_timelineCtrl.Create(WS_CHILD | WS_VISIBLE, rc, this, IDC_TIMELINE);
	m_timelineCtrl.SetMarkerStyle(CTimelineCtrl::MARKER_STYLE_FRAMES);
	m_timelineCtrl.SetFPS(FACIAL_EDITOR_FPS);
	m_timelineCtrl.SetTrackingSnapToFrames(true);
	m_splineCtrl.SetTimelineCtrl(&m_timelineCtrl);

	m_timelineCtrl.SetParent(&m_splineCtrl);
	m_timelineCtrl.SetOwner(this);
	m_splineCtrl.SetOwner(this);

	m_waveCtrl.Create(WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rc, &m_splitWnd2, IDC_WAVE_CTRL);
	m_waveCtrl.SetOwner(this);

	m_phonemesCtrl.Create(WS_CHILD | WS_VISIBLE, rc, &m_waveCtrl, IDC_PHONEMES);
	m_waveCtrl.SetBottomWnd(&m_phonemesCtrl, 35);
	m_phonemesCtrl.SetOwner(this);

	m_splitWnd2.SetPane(0, 0, &m_splineCtrl, CSize(100, 100));
	m_splitWnd2.SetPane(1, 0, &m_waveCtrl, CSize(100, 50));
	m_splitWnd2.SetRowInfo(0, 130, 0);
	m_splitWnd2.SetRowInfo(1, 80, 0);
	m_splitWnd2.RecalcLayout();

	m_splitWnd.SetPane(0, 0, &m_channelsCtrl, CSize(300, 100));
	m_splitWnd.SetPane(0, 1, &m_splitWnd2, CSize(300, 100));

	SyncZoom();

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::SetContext(CFacialEdContext* pContext)
{
	assert(pContext);
	m_pContext = pContext;
	if (m_pContext)
		m_pContext->RegisterListener(this);

	m_channelsCtrl.SetContext(pContext);
	if (m_waveCtrl)
		m_waveCtrl.SetContext(pContext);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* pNM = (NMTREEVIEW*)pNMHDR;
	if (m_pContext)
	{
		IFacialAnimChannel* pChannel = 0;
		if (pNM->itemNew.hItem != NULL)
			pChannel = (IFacialAnimChannel*)pNM->itemNew.lParam;
		m_pCurrent = pChannel;

		OnSelectionChange();
	}
	if (pNM->itemNew.hItem)
	{
		m_channelsCtrl.SetItemState(pNM->itemNew.hItem, TVIS_BOLD, TVIS_BOLD);
	}
	if (pNM->itemOld.hItem)
	{
		m_channelsCtrl.SetItemState(pNM->itemOld.hItem, 0, TVIS_BOLD);
	}
	*pResult = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnTreeItemExpanded(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* pNM = (NMTREEVIEW*)pNMHDR;
	if (m_pContext && pNM->itemNew.hItem)
	{
		IFacialAnimChannel* pChannel = (IFacialAnimChannel*)pNM->itemNew.lParam;
		if (pChannel)
		{
			if (pNM->action == TVE_EXPAND)
				pChannel->SetFlags(pChannel->GetFlags() | IFacialAnimChannel::FLAG_UI_EXTENDED);
			else if (pNM->action == TVE_COLLAPSE)
				pChannel->SetFlags(pChannel->GetFlags() & (~IFacialAnimChannel::FLAG_UI_EXTENDED));
		}
	}
	*pResult = TRUE;
}

enum
{
	MENU_CMD_ADD_SELECTED = 1,
	MENU_CMD_NEW_FOLDER,
	MENU_CMD_REMOVE_SELECTED,
	MENU_CMD_RENAME_FOLDER,
	MENU_CMD_ANALYZE_PHONEMES,
	MENU_CMD_CLEAR_ALL_WAVEFORMS,
	MENU_CMD_ADD_PHONEME_STRENGTH,
	MENU_CMD_ADD_VERTEX_DRAG,
	MENU_CMD_ADD_BALANCE,
	MENU_CMD_ADD_CATEGORY_BALANCE,
	MENU_CMD_ADD_LIPSYNC_CATEGORY_STRENGTH,
	MENU_CMD_ADD_PROCEDURAL_STRENGTH,
	MENU_CMD_CLEANUP_KEYS,
	MENU_CMD_SMOOTH_KEYS,
	MENU_CMD_REMOVE_NOISE,
	MENU_CMD_ADD_LAYER,
	MENU_CMD_DELETE_LAYER,
	MENU_CMD_COLLAPSE_LAYER,
	MENU_CMD_REMOVE_SOUND_ENTRY,
	MENU_CMD_INSERT_SHAPE,
	MENU_CMD_INSERT_SHAPE_SERIES
};

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* pNM = (NMTREEVIEW*)pNMHDR;
	*pResult = TRUE;

	if (!m_pContext)
		return;

	CPoint point;
	::GetCursorPos(&point);
	m_channelsCtrl.ScreenToClient(&point);
	UINT uFlags;
	HTREEITEM hItem = m_channelsCtrl.HitTest(point, &uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		m_pCurrent = (IFacialAnimChannel*)m_channelsCtrl.GetItemData(hItem);
	}

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	menu.AppendMenu(MF_STRING, MENU_CMD_NEW_FOLDER, _T("New Folder"));
	if (m_pCurrent && m_pCurrent->IsGroup())
	{
		menu.AppendMenu(MF_STRING, MENU_CMD_RENAME_FOLDER, _T("Rename Folder"));
	}

	// create main menu items
	if (m_pContext->pSelectedEffector && m_pContext->pSelectedEffector->GetType() != EFE_TYPE_GROUP)
		menu.AppendMenu(MF_STRING, MENU_CMD_ADD_SELECTED, _T("Add Selected Expression"));

	menu.AppendMenu(MF_STRING, MENU_CMD_REMOVE_SELECTED, _T("Remove"));

	menu.AppendMenu(MF_STRING, MENU_CMD_ADD_BALANCE, _T("Add Balance"));

	if (m_pContext->pSelectedEffector && m_pContext->pSelectedEffector->GetType() == EFE_TYPE_GROUP)
		menu.AppendMenu(MF_STRING, MENU_CMD_ADD_CATEGORY_BALANCE, _T("Add Balance for Selected Expression Category"));

	if (m_pContext->pSelectedEffector && m_pContext->pSelectedEffector->GetType() == EFE_TYPE_GROUP)
		menu.AppendMenu(MF_STRING, MENU_CMD_ADD_LIPSYNC_CATEGORY_STRENGTH, _T("Add Lipsync Strength for Selected Expression Category"));

	menu.AppendMenu(MF_STRING, MENU_CMD_CLEANUP_KEYS, _T("Cleanup Keys"));
	menu.AppendMenu(MF_STRING, MENU_CMD_SMOOTH_KEYS, _T("Smooth Keys"));
	menu.AppendMenu(MF_STRING, MENU_CMD_REMOVE_NOISE, _T("Remove Noise"));

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	if (pSequence)
	{
		bool bHavePhonemeStrength = false;
		bool bHaveVertexDrag = false;
		bool bHaveProceduralStrength = false;
		for (int i = 0; i < pSequence->GetChannelCount(); i++)
		{
			if (pSequence->GetChannel(i)->GetFlags() & IFacialAnimChannel::FLAG_PHONEME_STRENGTH)
				bHavePhonemeStrength = true;
			if (pSequence->GetChannel(i)->GetFlags() & IFacialAnimChannel::FLAG_VERTEX_DRAG)
				bHaveVertexDrag = true;
			if (pSequence->GetChannel(i)->GetFlags() & IFacialAnimChannel::FLAG_PROCEDURAL_STRENGTH)
				bHaveProceduralStrength = true;
		}
		if (!bHavePhonemeStrength)
		{
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, MENU_CMD_ADD_PHONEME_STRENGTH, _T("Add Phoneme Strength"));
		}
		if (!bHaveVertexDrag)
		{
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, MENU_CMD_ADD_VERTEX_DRAG, _T("Add Morph Target Vertex Drag"));
		}
		if (!bHaveProceduralStrength)
		{
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, MENU_CMD_ADD_PROCEDURAL_STRENGTH, _T("Add Procedural Animation"));
		}
	}

	if (!m_pCurrent || m_pCurrent->IsGroup() || m_pCurrent->GetInterpolatorCount() == 1)
	{
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, MENU_CMD_ADD_LAYER, _T("Add Layer"));
	}

	if (!m_pCurrent || m_pCurrent->IsGroup() || m_pCurrent->GetInterpolatorCount() > 1)
	{
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, MENU_CMD_DELETE_LAYER, _T("Delete Layer"));
		menu.AppendMenu(MF_STRING, MENU_CMD_COLLAPSE_LAYER, _T("Collapse Layer"));
	}

	if (m_pCurrent && !m_pCurrent->IsGroup())
	{
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, MENU_CMD_INSERT_SHAPE, _T("Insert Shape"));
	}

	if (!m_pCurrent || m_pCurrent->IsGroup())
	{
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, MENU_CMD_INSERT_SHAPE_SERIES, _T("Insert Visime Shapes"));
	}

	CPoint pos;
	GetCursorPos(&pos);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, pos.x, pos.y, this);
	switch (cmd)
	{
	case MENU_CMD_ADD_SELECTED:
		OnAddSelectedEffector();
		break;
	case MENU_CMD_NEW_FOLDER:
		OnNewFolder();
		break;
	case MENU_CMD_REMOVE_SELECTED:
		OnRemoveSelected();
		break;
	case MENU_CMD_RENAME_FOLDER:
		OnRenameFolder();
		break;
	case MENU_CMD_ADD_PHONEME_STRENGTH:
		AddPhonemeStrengthChannel();
		break;
	case MENU_CMD_ADD_VERTEX_DRAG:
		AddVertexDragChannel();
		break;
	case MENU_CMD_ADD_BALANCE:
		AddBalanceChannel();
		break;
	case MENU_CMD_ADD_CATEGORY_BALANCE:
		AddCategoryBalanceChannel();
		break;
	case MENU_CMD_ADD_LIPSYNC_CATEGORY_STRENGTH:
		AddLipsyncCategoryStrengthChannel();
		break;
	case MENU_CMD_ADD_PROCEDURAL_STRENGTH:
		AddProceduralStrengthChannel();
		break;
	case MENU_CMD_CLEANUP_KEYS:
		CleanupKeys();
		break;
	case MENU_CMD_SMOOTH_KEYS:
		SmoothKeys();
		break;
	case MENU_CMD_REMOVE_NOISE:
		RemoveNoiseFromKeys();
		break;
	case MENU_CMD_ADD_LAYER:
		AddLayerToChannel();
		break;
	case MENU_CMD_DELETE_LAYER:
		DeleteLayerOfChannel();
		break;
	case MENU_CMD_COLLAPSE_LAYER:
		CollapseLayersForChannel();
		break;
	case MENU_CMD_INSERT_SHAPE:
		InsertShapeIntoChannel();
		break;
	case MENU_CMD_INSERT_SHAPE_SERIES:
		InsertVisimeSeriesIntoChannel();
		break;
	}

	*pResult = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnWaveRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	WaveGraphCtrlRClickNotification* notification = reinterpret_cast<WaveGraphCtrlRClickNotification*>(pNMHDR);

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	menu.AppendMenu(MF_STRING, MENU_CMD_CLEAR_ALL_WAVEFORMS, _T("Clear all Waveforms"));
	if (notification->waveformIndex >= 0)
	{
		menu.AppendMenu(MF_STRING, MENU_CMD_ANALYZE_PHONEMES, _T("Lip Sync With Text"));
		menu.AppendMenu(MF_STRING, MENU_CMD_REMOVE_SOUND_ENTRY, _T("Remove Sound"));
	}

	CPoint pos;
	GetCursorPos(&pos);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, pos.x, pos.y, this);
	switch (cmd)
	{
	case MENU_CMD_CLEAR_ALL_WAVEFORMS:
		ClearAllSoundEntries();
		break;
	case MENU_CMD_ANALYZE_PHONEMES:
		if (notification->waveformIndex >= 0)
			LipSync(notification->waveformIndex);
		break;
	case MENU_CMD_REMOVE_SOUND_ENTRY:
		if (notification->waveformIndex >= 0)
			RemoveSoundEntry(notification->waveformIndex);
		break;
	}

	*pResult = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnBeginMoveWaveform(NMHDR* pNMHDR, LRESULT* pResult)
{
	GetIEditor()->GetIUndoManager()->Begin();
	if (m_pContext)
		m_pContext->StoreSequenceUndo(FE_SEQ_CHANGE_SOUND_TIMES);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnMoveWaveforms(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_pContext)
		m_pContext->StoreSequenceUndo(FE_SEQ_CHANGE_SOUND_TIMES);
	WaveGraphCtrlWaveformChangeNotification* pNotification = reinterpret_cast<WaveGraphCtrlWaveformChangeNotification*>(pNMHDR);
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);

	int soundEntryCount = (pSequence ? pSequence->GetSoundEntryCount() : 0);

	// Keep a list of places where we have placed a sound entry.
	std::vector<float> invalidRegionCentres;
	invalidRegionCentres.reserve(soundEntryCount);
	std::vector<float> invalidRegionExtents;
	invalidRegionExtents.reserve(soundEntryCount);

	// Keep a list of the waveforms that have not been definitely placed.
	std::vector<int> unresolvedWaveforms(soundEntryCount);
	for (int i = 0; i < soundEntryCount; ++i)
		unresolvedWaveforms[i] = i;

	// Continue moving waveforms until there are no overlaps.
	int waveformToMovePositionInUnresolvedWaveforms = pNotification->waveformIndex;
	float deltaTime = pNotification->deltaTime;
	while (waveformToMovePositionInUnresolvedWaveforms >= 0)
	{
		int waveformToMoveIndex = unresolvedWaveforms[waveformToMovePositionInUnresolvedWaveforms];

		// Move the waveform.
		IFacialAnimSoundEntry* pSoundEntry = (pSequence ? pSequence->GetSoundEntry(waveformToMoveIndex) : 0);
		if (pSoundEntry)
			pSoundEntry->SetStartTime(pSoundEntry->GetStartTime() + deltaTime);

		// Remove the waveform from the list of unresolved waveforms.
		unresolvedWaveforms[waveformToMovePositionInUnresolvedWaveforms] = unresolvedWaveforms.back();
		unresolvedWaveforms.pop_back();

		// Mark the area as invalid.
		float soundEntryStart = (pSoundEntry ? pSoundEntry->GetStartTime() : 0.0f);
		float soundEntryEnd = soundEntryStart + m_waveCtrl.GetWaveformLength(waveformToMoveIndex);
		invalidRegionCentres.push_back((soundEntryStart + soundEntryEnd) * 0.5f);
		invalidRegionExtents.push_back(fabs(soundEntryStart - soundEntryEnd) * 0.5f);

		// Loop through the unresolved waveforms looking for any that are in an invalid region.
		waveformToMovePositionInUnresolvedWaveforms = -1;
		for (int unresolvedIndex = 0, unresolvedCount = unresolvedWaveforms.size(); waveformToMovePositionInUnresolvedWaveforms < 0 && unresolvedIndex < unresolvedCount; ++unresolvedIndex)
		{
			int unresolvedWaveformIndex = unresolvedWaveforms[unresolvedIndex];
			IFacialAnimSoundEntry* pUnresolvedSoundEntry = (pSequence ? pSequence->GetSoundEntry(unresolvedWaveformIndex) : 0);
			float unresolvedStart = (pUnresolvedSoundEntry ? pUnresolvedSoundEntry->GetStartTime() : 0);
			float unresolvedEnd = unresolvedStart + m_waveCtrl.GetWaveformLength(unresolvedWaveformIndex);
			float unresolvedCentre = (unresolvedStart + unresolvedEnd) * 0.5f, unresolvedExtent = fabs(unresolvedStart - unresolvedEnd) * 0.5f;

			int direction = 0; // The first time we are stopped we are free to choose a direction in which to move.
			const float borderEpsilon = 0.001f;
			for (bool foundPlace = false; !foundPlace; )
			{
				foundPlace = true;
				for (int invalidRegionIndex = 0, invalidRegionCount = invalidRegionCentres.size(); invalidRegionIndex < invalidRegionCount; ++invalidRegionIndex)
				{
					if (fabs(unresolvedCentre - invalidRegionCentres[invalidRegionIndex]) < (unresolvedExtent + invalidRegionExtents[invalidRegionIndex]))
					{
						direction = (direction != 0 ? direction : (unresolvedCentre > invalidRegionCentres[invalidRegionIndex] ? 1 : -1)); // Pick a direction if we haven't already.
						unresolvedCentre = invalidRegionCentres[invalidRegionIndex] + (unresolvedExtent + invalidRegionExtents[invalidRegionIndex] + borderEpsilon) * direction;
						foundPlace = false;
						waveformToMovePositionInUnresolvedWaveforms = unresolvedIndex;
					}
				}
			}
			if (waveformToMovePositionInUnresolvedWaveforms >= 0)
				deltaTime = unresolvedCentre - unresolvedExtent - (pUnresolvedSoundEntry ? pUnresolvedSoundEntry->GetStartTime() : 0);
		}
	}

	ReloadWaveCtrlSounds();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnResetWaveformChanges(NMHDR* pNMHDR, LRESULT* pResult)
{
	GetIEditor()->GetIUndoManager()->Restore();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnEndWaveformChanges(NMHDR* pNMHDR, LRESULT* pResult)
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	bool bRangeFromSound = (pSequence ? pSequence->GetFlags() & IFacialAnimSequence::FLAG_RANGE_FROM_SOUND : false);
	if (bRangeFromSound)
	{
		if (m_pContext)
			m_pContext->StoreSequenceUndo();
		m_pContext->GetSequence()->SetTimeRange(Range(0, m_waveCtrl.CalculateTimeRange()));
		m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
	}

	GetIEditor()->GetIUndoManager()->Accept("Sound Time Changes");
	ReloadPhonemeCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::AddPhonemeStrengthChannel()
{
	IFacialAnimChannel* pLipSyncGroup = GetLipSyncGroup();
	if (!pLipSyncGroup)
		return;

	CUndo undo("Add Phoneme Strength Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	IFacialAnimChannel* pChannel = m_pContext->GetSequence()->CreateChannel();
	pChannel->SetName("Phoneme Strength");
	pChannel->SetFlags(IFacialAnimChannel::FLAG_PHONEME_STRENGTH);
	pChannel->SetParent(pLipSyncGroup);

	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
	SelectChannel(pChannel, true);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::AddVertexDragChannel()
{
	if (!m_pContext || !m_pContext->GetSequence())
		return;

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();

	// Find Lip sync channel.
	IFacialAnimChannel* pLipSyncGroup = NULL;
	for (int i = 0; i < pSequence->GetChannelCount(); i++)
	{
		if ((pSequence->GetChannel(i)->GetFlags() & IFacialAnimChannel::FLAG_GROUP) &&
		    (stricmp(pSequence->GetChannel(i)->GetName(), "LipSync") == 0))
		{
			pLipSyncGroup = pSequence->GetChannel(i);
			break;
		}
	}

	CUndo undo("Add Vertex Drag Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	if (!pLipSyncGroup)
	{
		pLipSyncGroup = m_pContext->GetSequence()->CreateChannelGroup();
		pLipSyncGroup->SetName("LipSync");
	}

	IFacialAnimChannel* pChannel = m_pContext->GetSequence()->CreateChannel();
	pChannel->SetName("Vertex Drag");
	pChannel->SetFlags(IFacialAnimChannel::FLAG_VERTEX_DRAG);
	pChannel->SetParent(pLipSyncGroup);

	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
	SelectChannel(pChannel, true);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::AddBalanceChannel()
{
	if (!m_pContext || !m_pContext->GetSequence())
		return;

	CUndo undo("Add Balance Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();

	IFacialAnimChannel* pChannel = m_pContext->GetSequence()->CreateChannel();
	pChannel->SetName("Balance");
	pChannel->SetFlags(IFacialAnimChannel::FLAG_BALANCE);
	if (m_pCurrent && m_pCurrent->IsGroup())
		pChannel->SetParent(m_pCurrent);

	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
	SelectChannel(pChannel, true);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::AddCategoryBalanceChannel()
{
	if (!m_pContext || !m_pContext->GetSequence())
		return;

	assert(m_pContext->pSelectedEffector);
	if (!m_pContext->pSelectedEffector)
		return;

	CUndo undo("Add Category Balance Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();

	IFacialAnimChannel* pChannel = m_pContext->GetSequence()->CreateChannel();
	pChannel->SetEffector(m_pContext->pSelectedEffector);
	string name;
	name.Format("Balance (%s)", m_pContext->pSelectedEffector->GetName());
	pChannel->SetName(name.c_str());
	pChannel->SetFlags(IFacialAnimChannel::FLAG_BALANCE | IFacialAnimChannel::FLAG_CATEGORY_BALANCE);
	if (m_pCurrent && m_pCurrent->IsGroup())
		pChannel->SetParent(m_pCurrent);

	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
	SelectChannel(pChannel, true);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::AddLipsyncCategoryStrengthChannel()
{
	if (!m_pContext || !m_pContext->GetSequence())
		return;

	assert(m_pContext->pSelectedEffector);
	if (!m_pContext->pSelectedEffector)
		return;

	CUndo undo("Add Lipsync Category Strength Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();

	IFacialAnimChannel* pChannel = m_pContext->GetSequence()->CreateChannel();
	pChannel->SetEffector(m_pContext->pSelectedEffector);
	string name;
	name.Format("Lipsync Category Strength (%s)", m_pContext->pSelectedEffector->GetName());
	pChannel->SetName(name.c_str());
	pChannel->SetFlags(IFacialAnimChannel::FLAG_LIPSYNC_CATEGORY_STRENGTH);
	if (m_pCurrent && m_pCurrent->IsGroup())
		pChannel->SetParent(m_pCurrent);

	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
	SelectChannel(pChannel, true);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::AddProceduralStrengthChannel()
{
	IFacialAnimChannel* pLipSyncGroup = GetLipSyncGroup();
	if (!pLipSyncGroup)
		return;

	CUndo undo("Add Procedural Strength Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	IFacialAnimChannel* pChannel = m_pContext->GetSequence()->CreateChannel();
	pChannel->SetName("Procedural Animation Strength");
	pChannel->SetFlags(IFacialAnimChannel::FLAG_PROCEDURAL_STRENGTH);
	pChannel->SetParent(pLipSyncGroup);

	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
	SelectChannel(pChannel, true);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::CleanupKeys()
{
	using namespace TreeCtrlUtils;

	//CNumberDlg dlg(this, m_fKeyCleanupThreshold, "Select Key Error Threshold");
	//dlg.SetRange(0.0f, 5.0f);
	//if (dlg.DoModal() == IDOK)
	{
		//m_fKeyCleanupThreshold = dlg.GetValue();
		//UpdateKeyCleanupErrorMax();

		CUndo undo("Cleanup Keys");
		m_splineCtrl.StoreUndo();

		for (SelectedTreeItemIterator it = BeginSelectedTreeItems(&m_channelsCtrl), end = EndSelectedTreeItems(&m_channelsCtrl); it != end; ++it)
			std::for_each(BeginTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *it), EndTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *it), std::bind2nd(std::mem_fun(&IFacialAnimChannel::CleanupKeys), m_fKeyCleanupThreshold));

		m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE, 0);
		m_pContext->bSequenceModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::SmoothKeys()
{
	using namespace TreeCtrlUtils;

	//CNumberDlg dlg(this, m_fSmoothingSigma, "Select Smoothing Value:");
	//dlg.SetRange(0.0f, 5.0f);
	//if (dlg.DoModal() == IDOK)
	{
		//m_fSmoothingSigma = dlg.GetValue();
		//UpdateSmoothSplineSigma();

		CUndo undo("Smooth Keys");
		m_splineCtrl.StoreUndo();

		for (SelectedTreeItemIterator it = BeginSelectedTreeItems(&m_channelsCtrl), end = EndSelectedTreeItems(&m_channelsCtrl); it != end; ++it)
			std::for_each(BeginTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *it), EndTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *it), std::bind2nd(std::mem_fun(&IFacialAnimChannel::SmoothKeys), m_fSmoothingSigma));

		m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE, 0);
		m_pContext->bSequenceModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::RemoveNoiseFromKeys()
{
	using namespace TreeCtrlUtils;

	//CNumberDlg dlg(this, m_fNoiseThreshold, "Noise Threshold Value:");
	//dlg.SetRange(0.0f, 5.0f);
	//if (dlg.DoModal() == IDOK)
	{
		//m_fNoiseThreshold = dlg.GetValue();
		//UpdateRemoveNoiseSigma();

		CUndo undo("Smooth Keys");
		m_splineCtrl.StoreUndo();

		for (SelectedTreeItemIterator itSel = BeginSelectedTreeItems(&m_channelsCtrl), end = EndSelectedTreeItems(&m_channelsCtrl); itSel != end; ++itSel)
		{
			for (TreeItemDataIterator<IFacialAnimChannel, RecursiveTreeItemIteratorTraits> it = BeginTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel), end = EndTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel); it != end; ++it)
				(*it)->RemoveNoise(0.1f, m_fNoiseThreshold);
		}

		m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE, 0);
		m_pContext->bSequenceModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::AddLayerToChannel()
{
	using namespace TreeCtrlUtils;

	CUndo undo("Add Layer to Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	for (SelectedTreeItemIterator itSel = BeginSelectedTreeItems(&m_channelsCtrl), end = EndSelectedTreeItems(&m_channelsCtrl); itSel != end; ++itSel)
	{
		for (TreeItemDataIterator<IFacialAnimChannel, RecursiveTreeItemIteratorTraits> it = BeginTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel), end = EndTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel); it != end; ++it)
			(*it)->AddInterpolator();
	}

	OnSelectionChange();

	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE, 0);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::DeleteLayerOfChannel()
{
	using namespace TreeCtrlUtils;

	CUndo undo("Delete Layer");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	for (SelectedTreeItemIterator itSel = BeginSelectedTreeItems(&m_channelsCtrl), end = EndSelectedTreeItems(&m_channelsCtrl); itSel != end; ++itSel)
	{
		for (TreeItemDataIterator<IFacialAnimChannel, RecursiveTreeItemIteratorTraits> it = BeginTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel), end = EndTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel); it != end; ++it)
		{
			IFacialAnimChannel* pChannel = *it;
			if (pChannel && pChannel->GetInterpolatorCount() > 1)
				pChannel->DeleteInterpolator(1);
		}
	}

	OnSelectionChange();

	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE, 0);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::CollapseLayersForChannel()
{
	using namespace TreeCtrlUtils;

	CUndo undo("Collapse Layer");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	for (SelectedTreeItemIterator itSel = BeginSelectedTreeItems(&m_channelsCtrl), end = EndSelectedTreeItems(&m_channelsCtrl); itSel != end; ++itSel)
	{
		for (TreeItemDataIterator<IFacialAnimChannel, RecursiveTreeItemIteratorTraits> it = BeginTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel), end = EndTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel); it != end; ++it)
		{
			IFacialAnimChannel* pChannel = *it;
			if (pChannel && pChannel->GetInterpolatorCount() > 1)
			{
				ISplineInterpolator* pSpline = pChannel->GetInterpolator(0);
				ISplineInterpolator* pLayerSpline = pChannel->GetInterpolator(1);

				// First loop through all the keys in the layer spline and add keys to the detail spline at that point.
				for (int keyIndex = 0, keyCount = pLayerSpline->GetKeyCount(); keyIndex < keyCount; ++keyIndex)
				{
					float v;
					float time = pLayerSpline->GetKeyTime(keyIndex);
					pSpline->InterpolateFloat(time, v);
					pSpline->InsertKeyFloat(time, v);
				}

				// Now loop through all the keys in the detail spline.
				for (int keyIndex = 0, keyCount = pSpline->GetKeyCount(); keyIndex < keyCount; ++keyIndex)
				{
					float v1, v2;
					pSpline->GetKeyValueFloat(keyIndex, v1);
					pLayerSpline->InterpolateFloat(pSpline->GetKeyTime(keyIndex), v2);
					pSpline->SetKeyValueFloat(keyIndex, v1 + v2);
				}

				pChannel->DeleteInterpolator(1);
			}
		}
	}

	OnSelectionChange();

	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE, 0);
	m_pContext->bSequenceModfied = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::InsertShapeIntoChannel()
{
	static const int shapeRange = 5;
	static const int firstShapeKeyFrameOffset = -((shapeRange - 1) >> 1);
	static const int lastShapeKeyFrameOffset = (shapeRange >> 1);

	float currentTime = (m_pContext ? m_pContext->GetSequenceTime() : 0.0f);
	int currentFrame = int(FacialEditorSnapTimeToFrame(currentTime) * FACIAL_EDITOR_FPS + 0.5f);
	int rangeStart = currentFrame + firstShapeKeyFrameOffset;
	int rangeEnd = currentFrame + lastShapeKeyFrameOffset;

	// Check for existing keys in the range we are about to overwrite.
	ISplineInterpolator* pSpline = (m_pCurrent ? m_pCurrent->GetLastInterpolator() : 0);
	int firstExistingKey = -1, numExistingKeys = 0;
	if (pSpline)
		pSpline->FindKeysInRange(float(rangeStart) / FACIAL_EDITOR_FPS, float(rangeEnd) / FACIAL_EDITOR_FPS, firstExistingKey, numExistingKeys);

	if (numExistingKeys > 0)
	{

		if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("The shape will overwrite existing keys.\r\nDo you want to continue?")))
		{
			return;
		}
	}

	{
		CUndo undo("Insert Shape");
		if (m_pContext)
			m_pContext->StoreSequenceUndo();

		// Create the three keys.
		int keyFrames[] = { rangeStart, currentFrame, rangeEnd };
		float keyValues[] = { 0.0f, 1.0f, 0.0f };
		IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
		float sequenceStart = (pSequence ? pSequence->GetTimeRange().start : 0.0f);
		float sequenceEnd = (pSequence ? pSequence->GetTimeRange().end : 0.0f);
		for (int key = 0; key < 3; ++key)
		{
			float frameTime = float(keyFrames[key]) / FACIAL_EDITOR_FPS;
			if (pSpline && frameTime >= sequenceStart && frameTime <= sequenceEnd)
				pSpline->InsertKeyFloat(frameTime, keyValues[key]);
		}

		m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE, 0);
		m_pContext->bSequenceModfied = true;
	}
}

//////////////////////////////////////////////////////////////////////////
struct ShapeToAddEntry
{
	ShapeToAddEntry(IFacialAnimChannel* pChannel, int mainFrame) : pChannel(pChannel), mainFrame(mainFrame) {}
	IFacialAnimChannel* pChannel;
	int                 mainFrame;
};

struct KeyToAddEntry
{
	KeyToAddEntry(IFacialAnimChannel* pChannel, int frame, float value)
		: pChannel(pChannel), frame(frame), value(value) {}

	IFacialAnimChannel* pChannel;
	int                 frame;
	float               value;
};

void CFacialSequenceDialog::InsertVisimeSeriesIntoChannel()
{
	static const int shapeRange = 5;
	static const int firstShapeKeyFrameOffset = -((shapeRange - 1) >> 1);
	static const int lastShapeKeyFrameOffset = (shapeRange >> 1);

	// Ask the user what visimes he wants to place.
	QStringDialog dlg("Enter visime string:");
	string visimeString;
	if (dlg.exec() == QDialog::Accepted)
		visimeString = dlg.GetString();
	else
		return;

	// Scan the string to create a list of visimes to place.
	std::vector<string> tokens;
	int position = 0;
	for (;; )
	{
		int token_start = visimeString.find_first_not_of(" \t", position);
		if (token_start == string::npos)
			break;
		int next_whitespace = visimeString.find_first_of(" \t", token_start);
		int token_end = (next_whitespace == string::npos ? int(visimeString.size()) : next_whitespace);
		tokens.push_back(visimeString.substr(token_start, token_end - token_start));
		position = token_end;
	}

	// Create a list of visime channels to manipulate.
	std::vector<IFacialAnimChannel*> visimeChannels(tokens.size());
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	std::set<string> missingVisimes;
	for (int tokenIndex = 0, tokenCount = int(tokens.size()); tokenIndex < tokenCount; ++tokenIndex)
	{
		string effectorName = "Visime_" + tokens[tokenIndex];

		// Search for this visime in the channels.
		IFacialAnimChannel* pVisimeChannel = 0;
		for (int channelIndex = 0, channelCount = (pSequence ? pSequence->GetChannelCount() : 0); channelIndex < channelCount; ++channelIndex)
		{
			IFacialAnimChannel* pChannel = (pSequence ? pSequence->GetChannel(channelIndex) : 0);
			const char* szEffectorName = (pChannel ? pChannel->GetEffectorName() : 0);
			if (stricmp((szEffectorName ? szEffectorName : ""), effectorName) == 0)
				pVisimeChannel = pChannel;
		}

		if (!pVisimeChannel)
			missingVisimes.insert(effectorName);

		visimeChannels[tokenIndex] = pVisimeChannel;
	}

	// Create a list of shapes to add.
	float currentTime = (m_pContext ? m_pContext->GetSequenceTime() : 0.0f);
	int currentFrame = int(FacialEditorSnapTimeToFrame(currentTime) * FACIAL_EDITOR_FPS + 0.5f);
	std::vector<ShapeToAddEntry> shapesToAdd;
	for (int shapeIndex = 0, shapeCount = int(visimeChannels.size()); shapeIndex < shapeCount; ++shapeIndex)
	{
		if (visimeChannels[shapeIndex])
			shapesToAdd.push_back(ShapeToAddEntry(visimeChannels[shapeIndex], currentFrame));
		currentFrame += (shapeRange >> 1);
	}

	// Check for existing keys in the range we are about to overwrite.
	bool keysWillBeOverwritten = false;
	for (int shapeIndex = 0, shapeCount = int(shapesToAdd.size()); shapeIndex < shapeCount; ++shapeIndex)
	{
		IFacialAnimChannel* pChannel = shapesToAdd[shapeIndex].pChannel;
		ISplineInterpolator* pSpline = (pChannel ? pChannel->GetLastInterpolator() : 0);
		int firstExistingKey = -1, numExistingKeys = 0;
		int rangeStart = shapesToAdd[shapeIndex].mainFrame + firstShapeKeyFrameOffset;
		int rangeEnd = shapesToAdd[shapeIndex].mainFrame + lastShapeKeyFrameOffset;
		if (pSpline)
			pSpline->FindKeysInRange(float(rangeStart) / FACIAL_EDITOR_FPS, float(rangeEnd) / FACIAL_EDITOR_FPS, firstExistingKey, numExistingKeys);

		if (numExistingKeys > 0)
			keysWillBeOverwritten = true;
	}

	if (keysWillBeOverwritten)
	{
		if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("The shapes will overwrite existing keys.\r\nDo you want to continue?")))
		{
			return;
		}
	}

	// Create a list of keys to add.
	std::vector<KeyToAddEntry> keysToAdd;
	for (int shapeIndex = 0, shapeCount = int(shapesToAdd.size()); shapeIndex < shapeCount; ++shapeIndex)
	{
		keysToAdd.push_back(KeyToAddEntry(shapesToAdd[shapeIndex].pChannel, shapesToAdd[shapeIndex].mainFrame + firstShapeKeyFrameOffset, 0.0f));
		keysToAdd.push_back(KeyToAddEntry(shapesToAdd[shapeIndex].pChannel, shapesToAdd[shapeIndex].mainFrame, 1.0f));
		keysToAdd.push_back(KeyToAddEntry(shapesToAdd[shapeIndex].pChannel, shapesToAdd[shapeIndex].mainFrame + lastShapeKeyFrameOffset, 0.0f));
	}

	{
		CUndo undo("Insert Visime Shapes");
		if (m_pContext)
			m_pContext->StoreSequenceUndo();

		float sequenceStart = (pSequence ? pSequence->GetTimeRange().start : 0.0f);
		float sequenceEnd = (pSequence ? pSequence->GetTimeRange().end : 0.0f);
		for (int keyIndex = 0, keyCount = int(keysToAdd.size()); keyIndex < keyCount; ++keyIndex)
		{
			float frameTime = float(keysToAdd[keyIndex].frame) / FACIAL_EDITOR_FPS;
			IFacialAnimChannel* pChannel = keysToAdd[keyIndex].pChannel;
			if (pChannel && frameTime >= sequenceStart && frameTime <= sequenceEnd)
			{
				ISplineInterpolator* pSpline = (pChannel ? pChannel->GetLastInterpolator() : 0);
				if (pSpline)
					pSpline->InsertKeyFloat(frameTime, keysToAdd[keyIndex].value);
			}
		}

		m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE, 0);
		m_pContext->bSequenceModfied = true;

		if (!missingVisimes.empty())
		{
			string missingVisimeMessage = "The following visimes could not be found in the sequence:";
			for (std::set<string>::iterator missingVisimePosition = missingVisimes.begin(), missingVisimeEnd = missingVisimes.end(); missingVisimePosition != missingVisimeEnd; ++missingVisimePosition)
			{
				missingVisimeMessage += "\n";
				missingVisimeMessage += *missingVisimePosition;
			}

			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr(missingVisimeMessage));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialSequenceDialog::GetLipSyncGroup()
{
	if (!m_pContext || !m_pContext->GetSequence())
		return 0;

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();

	// Find Lip sync channel.
	IFacialAnimChannel* pLipSyncGroup = NULL;
	for (int i = 0; i < pSequence->GetChannelCount(); i++)
	{
		if ((pSequence->GetChannel(i)->GetFlags() & IFacialAnimChannel::FLAG_GROUP) &&
		    (stricmp(pSequence->GetChannel(i)->GetName(), "LipSync") == 0))
		{
			pLipSyncGroup = pSequence->GetChannel(i);
			break;
		}
	}
	if (!pLipSyncGroup)
	{
		pLipSyncGroup = m_pContext->GetSequence()->CreateChannelGroup();
		pLipSyncGroup->SetName("LipSync");
	}

	return pLipSyncGroup;
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialSequenceDialog::GetBakedLipSyncGroup()
{
	if (!m_pContext || !m_pContext->GetSequence())
		return 0;

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();

	// Find baked phoneme channel.
	IFacialAnimChannel* pBakedGroup = NULL;
	for (int i = 0; i < pSequence->GetChannelCount(); i++)
	{
		if ((pSequence->GetChannel(i)->GetFlags() & IFacialAnimChannel::FLAG_GROUP) &&
		    (stricmp(pSequence->GetChannel(i)->GetName(), "BakedLipSync") == 0))
		{
			pBakedGroup = pSequence->GetChannel(i);
			break;
		}
	}
	if (!pBakedGroup)
	{
		pBakedGroup = m_pContext->GetSequence()->CreateChannelGroup();
		pBakedGroup->SetName("BakedLipSync");
		pBakedGroup->SetFlags(pBakedGroup->GetFlags() | IFacialAnimChannel::FLAG_BAKED_LIPSYNC_GROUP);
	}

	return pBakedGroup;
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialSequenceDialog::GetChannelFromSpline(ISplineInterpolator* pSpline)
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	IFacialAnimChannel* pCorrectChannel = 0;
	for (int channelIndex = 0; pSequence && channelIndex < pSequence->GetChannelCount(); ++channelIndex)
	{
		IFacialAnimChannel* pChannel = (pSequence ? pSequence->GetChannel(channelIndex) : 0);
		for (int i = 0, count = pChannel->GetInterpolatorCount(); i < count; ++i)
		{
			if (pChannel && pChannel->GetInterpolator(i) == pSpline)
				pCorrectChannel = pChannel;
		}
	}

	return pCorrectChannel;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::DisplayPlaybackSpeedInToolbar()
{
	string text;
	text.Format("%d%%", int((100 * m_waveCtrl.GetPlaybackSpeed()) + 0.5f));
	if (m_pSpeedEdit)
		m_pSpeedEdit->SetEditText(text.c_str());
}

//////////////////////////////////////////////////////////////////////////
float CFacialSequenceDialog::ReadPlaybackSpeedFromToolbar()
{
	float value = m_waveCtrl.GetPlaybackSpeed();
	if (m_pSpeedEdit)
	{
		CString text = m_pSpeedEdit->GetEditText();
		const char* buf = text.GetString();
		char* end;
		int percentage = strtol(buf, &end, 10);
		if (buf != end)
			value = float(percentage) / 100.0f;
	}
	return value;
}

//////////////////////////////////////////////////////////////////////////
float CFacialSequenceDialog::ReadFrameFromToolbar()
{
	float value = (m_pContext ? m_pContext->GetSequenceTime() * FACIAL_EDITOR_FPS : 0.0f);
	if (m_pFrameEdit)
	{
		CString text = m_pFrameEdit->GetEditText();
		const char* buf = text.GetString();
		char* end;
		int frameNo = strtol(buf, &end, 10);
		if (buf != end)
			value = frameNo / FACIAL_EDITOR_FPS;
	}
	return value;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::UpdateSkeletonAnimationStatus()
{
	if (m_pAnimSkeletonButton && m_pContext)
		m_pAnimSkeletonButton->SetChecked(m_pContext->GetAnimateSkeleton());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::UpdateCameraAnimationStatus()
{
	if (m_pAnimCameraButton && m_pContext)
		m_pAnimCameraButton->SetChecked(m_pContext->GetAnimateCamera());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::UpdateOverlapSoundStatus()
{
	if (m_pOverlapSoundsButton && m_pContext)
		m_pOverlapSoundsButton->SetChecked(m_pContext->GetOverlapSounds());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::UpdateControlAmplitudeStatus()
{
	if (m_pControlAmplitudeButton)
		m_pControlAmplitudeButton->SetChecked(m_splineCtrl.GetControlAmplitude());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::ReloadWaveCtrlSounds()
{
	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	while (m_waveCtrl.GetWaveformCount())
		m_waveCtrl.DeleteWaveform(0);
	if (!pSequence)
		return;
	for (int soundEntryIndex = 0, soundEntryCount = pSequence->GetSoundEntryCount(); soundEntryIndex < soundEntryCount; ++soundEntryIndex)
	{
		int waveformIndex = m_waveCtrl.AddWaveform();
		m_waveCtrl.SetWaveformTime(waveformIndex, pSequence->GetSoundEntry(soundEntryIndex)->GetStartTime());
		m_waveCtrl.LoadWaveformSound(waveformIndex, pSequence->GetSoundEntry(soundEntryIndex)->GetSoundFile());
		m_waveCtrl.SetWaveformTextString(waveformIndex, pSequence->GetSoundEntry(soundEntryIndex)->GetSentence()->GetText());
	}
	m_waveCtrl.DeleteUnusedSounds();
	m_waveCtrl.RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
class ChannelIndexPair
{
public:
	ChannelIndexPair() {}
	ChannelIndexPair(IFacialAnimChannel* pChannel, int index) : pChannel(pChannel), index(index) {}
	bool operator<(const ChannelIndexPair& other) const  { return pChannel < other.pChannel; }
	bool operator==(const ChannelIndexPair& other) const { return pChannel == other.pChannel; }
	bool operator!=(const ChannelIndexPair& other) const { return pChannel != other.pChannel; }

	IFacialAnimChannel* pChannel;
	int                 index;
};
_smart_ptr<IFacialAnimSequence> CFacialSequenceDialog::CreateSequenceWithSelectedExpressions()
{
	IFacialAnimSequence* pSequence = 0;
	if (m_pContext)
		pSequence = m_pContext->GetSequence();

	_smart_ptr<IFacialAnimSequence> pNewSequence = 0;
	if (pSequence)
	{
		std::vector<bool> selectedChannels;
		selectedChannels.resize(pSequence->GetChannelCount());
		std::fill(selectedChannels.begin(), selectedChannels.end(), false);

		typedef std::vector<ChannelIndexPair> SortedChannelIndexMap;
		SortedChannelIndexMap sortedChannelIndexMap;
		sortedChannelIndexMap.resize(pSequence->GetChannelCount());
		for (int i = 0; i < pSequence->GetChannelCount(); ++i)
			sortedChannelIndexMap[i] = ChannelIndexPair(pSequence->GetChannel(i), i);
		std::sort(sortedChannelIndexMap.begin(), sortedChannelIndexMap.end());

		class SelectedItemRecurser
		{
		public:
			SelectedItemRecurser(CFacialChannelTreeCtrl* pTreeCtrl, SortedChannelIndexMap& channelIndexMap, std::vector<bool>& selectedChannels)
				: pTreeCtrl(pTreeCtrl), channelIndexMap(channelIndexMap), selectedChannels(selectedChannels)
			{
			}

			void operator()()
			{
				Recurse(pTreeCtrl->GetRootItem());
			}

		private:
			void Recurse(HTREEITEM item)
			{
				IFacialAnimChannel* pChannel = (IFacialAnimChannel*)pTreeCtrl->GetItemData(item);
				if (pChannel && pTreeCtrl->IsSelected(item))
				{
					int index = (*stl::binary_find(channelIndexMap.begin(), channelIndexMap.end(), ChannelIndexPair(pChannel, 0))).index;
					selectedChannels[index] = true;
				}

				for (HTREEITEM hChildItem = pTreeCtrl->GetChildItem(item); hChildItem != 0; hChildItem = pTreeCtrl->GetNextItem(hChildItem, TVGN_NEXT))
					Recurse(hChildItem);
			}

			CFacialChannelTreeCtrl* pTreeCtrl;
			SortedChannelIndexMap&  channelIndexMap;
			std::vector<bool>&      selectedChannels;
		};

		SelectedItemRecurser(&m_channelsCtrl, sortedChannelIndexMap, selectedChannels)();

		std::vector<bool> channelsToOutput(selectedChannels);

		for (size_t i = 0; i < channelsToOutput.size(); ++i)
		{
			for (IFacialAnimChannel* pChannel = pSequence->GetChannel(i); pChannel; pChannel = pChannel->GetParent())
			{
				int parentIndex = (*stl::binary_find(sortedChannelIndexMap.begin(), sortedChannelIndexMap.end(), ChannelIndexPair(pChannel, 0))).index;
				if (selectedChannels[parentIndex])
					channelsToOutput[i] = true;

				if (selectedChannels[i])
					channelsToOutput[parentIndex] = true;
			}
		}

		pNewSequence = GetISystem()->GetIAnimationSystem()->GetIFacialAnimation()->CreateSequence();
		std::vector<IFacialAnimChannel*> clones;
		clones.resize(channelsToOutput.size());
		std::fill(clones.begin(), clones.end(), static_cast<IFacialAnimChannel*>(0));
		for (size_t i = 0; i < channelsToOutput.size(); ++i)
		{
			if (channelsToOutput[i])
			{
				IFacialAnimChannel* pChannel = pSequence->GetChannel(i);
				IFacialAnimChannel* pNewChannel = 0;
				if (pChannel->IsGroup())
					pNewChannel = pNewSequence->CreateChannelGroup();
				else
					pNewChannel = pNewSequence->CreateChannel();

				pNewChannel->SetName(pChannel->GetName());
				pNewChannel->SetFlags(pChannel->GetFlags());
				pNewChannel->SetEffector(pChannel->GetEffector());

				ISplineInterpolator* pOriginalSpline = pChannel->GetLastInterpolator();
				ISplineInterpolator* pCloneSpline = pNewChannel->GetLastInterpolator();
				if (pOriginalSpline && pCloneSpline)
				{
					while (pCloneSpline->GetKeyCount())
						pCloneSpline->RemoveKey(0);
					for (int key = 0; key < pOriginalSpline->GetKeyCount(); ++key)
					{
						ISplineInterpolator::ValueType value;
						pOriginalSpline->GetKeyValue(key, value);
						int cloneKeyIndex = pCloneSpline->InsertKey(FacialEditorSnapTimeToFrame(pOriginalSpline->GetKeyTime(key)), value);
						pCloneSpline->SetKeyFlags(cloneKeyIndex, pOriginalSpline->GetKeyFlags(key));
						ISplineInterpolator::ValueType tin, tout;
						pOriginalSpline->GetKeyTangents(key, tin, tout);
						pCloneSpline->SetKeyTangents(cloneKeyIndex, tin, tout);
					}
				}

				clones[i] = pNewChannel;
			}
		}

		for (size_t i = 0; i < channelsToOutput.size(); ++i)
		{
			if (channelsToOutput[i])
			{
				IFacialAnimChannel* pChannel = pSequence->GetChannel(i);
				IFacialAnimChannel* pNewChannel = clones[i];

				if (pChannel->GetParent())
				{
					int parentIndex = (*stl::binary_find(sortedChannelIndexMap.begin(), sortedChannelIndexMap.end(), ChannelIndexPair(pChannel->GetParent(), 0))).index;
					pNewChannel->SetParent(clones[parentIndex]);
				}
			}
		}
	}

	return pNewSequence;
}

//////////////////////////////////////////////////////////////////////////
ISplineInterpolator* CFacialSequenceDialog::GetSplineFromID(const string& id)
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	IFacialAnimChannel* pCorrectChannel = 0;
	for (int channelIndex = 0; pSequence && channelIndex < pSequence->GetChannelCount(); ++channelIndex)
	{
		IFacialAnimChannel* pChannel = (pSequence ? pSequence->GetChannel(channelIndex) : 0);
		if (pChannel && CreateIDForChannel(pChannel) == id)
			pCorrectChannel = pChannel;
	}

	return (pCorrectChannel ? pCorrectChannel->GetLastInterpolator() : 0);
}

//////////////////////////////////////////////////////////////////////////
string CFacialSequenceDialog::GetIDFromSpline(ISplineInterpolator* pSpline)
{
	IFacialAnimChannel* pChannel = GetChannelFromSpline(pSpline);
	return (pChannel ? CreateIDForChannel(pChannel) : "");
}

//////////////////////////////////////////////////////////////////////////
int CFacialSequenceDialog::GetSplineCount() const
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	int totalCurves = 0;
	for (int channelIndex = 0; pSequence && channelIndex < pSequence->GetChannelCount(); ++channelIndex)
	{
		IFacialAnimChannel* pChannel = (pSequence ? pSequence->GetChannel(channelIndex) : 0);
		if (pChannel && (pChannel->GetFlags() & IFacialAnimChannel::FLAG_GROUP) == 0)
			++totalCurves;
	}

	return totalCurves;
}

//////////////////////////////////////////////////////////////////////////
int CFacialSequenceDialog::GetKeyCountAtTime(float time, float threshold) const
{
	//IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);

	//int totalKeys = 0;
	//for (int channelIndex = 0; pSequence && channelIndex < pSequence->GetChannelCount(); ++channelIndex)
	//{
	//	IFacialAnimChannel* pChannel = (pSequence ? pSequence->GetChannel(channelIndex) : 0);
	//	ISplineInterpolator* pSpline = (pChannel ? pChannel->GetInterpolator() : 0);
	//	for (int key = 0; pSpline && key < pSpline->GetKeyCount(); ++key)
	//	{
	//		float keyTime = (pSpline ? pSpline->GetKeyTime(key) : 0.0f);
	//		if (fabsf(keyTime - time) < threshold)
	//			++totalKeys;
	//	}
	//}

	//return totalKeys;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		Update();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnAddSelectedEffector()
{
	if (!m_pContext || !m_pContext->pSelectedEffector)
		return;

	assert(m_pContext->pSelectedEffector);
	if (!m_pContext->pSelectedEffector)
		return;

	CUndo undo("Add Selected Effector");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	IFacialAnimChannel* pChannel = m_pContext->GetSequence()->CreateChannel();
	pChannel->SetEffector(m_pContext->pSelectedEffector);
	if (m_pCurrent && m_pCurrent->IsGroup())
		pChannel->SetParent(m_pCurrent);
	m_pContext->bSequenceModfied = true;
	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
	SelectChannel(pChannel, true);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnNewFolder()
{
	if (!m_pContext)
		return;

	QStringDialog dlg("New Folder Name");
	if (dlg.exec() == QDialog::Accepted)
	{
		CUndo undo("Add New Folder");
		if (m_pContext)
			m_pContext->StoreSequenceUndo();

		IFacialAnimChannel* pChannel = m_pContext->GetSequence()->CreateChannelGroup();
		pChannel->SetName(dlg.GetString());
		if (m_pCurrent && m_pCurrent->IsGroup())
			pChannel->SetParent(m_pCurrent);
		m_pContext->bSequenceModfied = true;
		m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
		SelectChannel(pChannel, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnRenameFolder()
{
	if (!m_pCurrent)
		return;

	QStringDialog dlg("Change Folder Name");
	dlg.SetString(m_pCurrent->GetName());
	if (dlg.exec() == QDialog::Accepted)
	{
		CUndo undo("Rename Folder");
		if (m_pContext)
			m_pContext->StoreSequenceUndo();

		m_pContext->bSequenceModfied = true;
		m_pCurrent->SetName(dlg.GetString());
		m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnRemoveSelected()
{
	if (!m_pContext)
		return;

	CUndo undo("Remove Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	// Update selection.
	HTREEITEM hItem = m_channelsCtrl.GetFirstSelectedItem();
	while (hItem)
	{
		IFacialAnimChannel* pChannel = (IFacialAnimChannel*)m_channelsCtrl.GetItemData(hItem);
		if (pChannel)
		{
			m_pContext->bSequenceModfied = true;
			m_pContext->GetSequence()->RemoveChannel(pChannel);
			m_channelsCtrl.SetItemData(hItem, NULL);
		}
		HTREEITEM hNextItem = m_channelsCtrl.GetNextSelectedItem(hItem);
		m_channelsCtrl.DeleteItem(hItem);
		hItem = hNextItem;
	}
	SelectChannel(0, false);
	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::SelectChannel(IFacialAnimChannel* pChannel, bool bExclusive)
{
	m_channelsCtrl.SelectChannel(pChannel, bExclusive);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::SelectChannels(int numChannels, IFacialAnimChannel** ppChannels, bool bExclusive)
{
	bool bClearedSelection = false;
	if (bExclusive)
	{
		SelectChannel(0, true);
		bClearedSelection = true;
	}
	for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
	{
		IFacialAnimChannel* pChannel = (ppChannels ? ppChannels[channelIndex] : 0);
		if (pChannel)
		{
			m_channelsCtrl.SelectChannel(pChannel, bExclusive && !bClearedSelection);
			bClearedSelection = bExclusive;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSelectionChange()
{
	using namespace TreeCtrlUtils;

	m_splineCtrl.RemoveAllSplines();

	// Set all items to be black, in case they were colour-coded before.
	for (RecursiveTreeItemIterator it = BeginTreeItemsRecursive(&m_channelsCtrl), end = EndTreeItemsRecursive(&m_channelsCtrl); it != end; ++it)
		m_channelsCtrl.SetItemColor(*it, RGB(0, 0, 0));

	m_channelsCtrl.m_selectedChannelIDs.clear();
	if (m_channelsCtrl.IsSelected(m_channelsCtrl.GetRootItem()))
		m_channelsCtrl.m_selectedChannelIDs.push_back("");
	for (SelectedTreeItemDataIterator<IFacialAnimChannel> itSel = BeginSelectedTreeItemData<IFacialAnimChannel>(&m_channelsCtrl),
	     endSel = EndSelectedTreeItemData<IFacialAnimChannel>(&m_channelsCtrl); itSel != endSel; ++itSel)
		m_channelsCtrl.m_selectedChannelIDs.push_back(CreateIDForChannel(*itSel));

	static const COLORREF colours[] = {
		RGB(255, 0,   0),
		RGB(0,   255, 0),
		RGB(0,   0,   255),
		RGB(255, 0,   255),
		RGB(0,   255, 255)
	};
	enum {NUM_COLOURS = sizeof(colours) / sizeof(colours[0])};

	// Update selection.
	HTREEITEM hItem = m_channelsCtrl.GetFirstSelectedItem();
	IFacialAnimChannel* pSelectedChannel = 0;
	int colourIndex = 0;
	m_pContext->ClearHighlightedChannels();
	for (SelectedTreeItemIterator itSel = BeginSelectedTreeItems(&m_channelsCtrl), endSel = EndSelectedTreeItems(&m_channelsCtrl); itSel != endSel; ++itSel)
	{
		for (RecursiveItemDataIteratorType<IFacialAnimChannel>::type it = BeginTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel),
		     end = EndTreeItemDataRecursive<IFacialAnimChannel>(&m_channelsCtrl, *itSel); it != end; ++it)
		{
			IFacialAnimChannel* pChannel = *it;
			HTREEITEM hItem = it.GetTreeItem();

			if (pChannel && pChannel->GetLastInterpolator())
			{
				if (!pSelectedChannel)
					pSelectedChannel = pChannel;
				m_pContext->AddHighlightedChannel(pChannel);

				// Add the spline with the colour code.
				m_splineCtrl.AddSpline(pChannel->GetLastInterpolator(), (pChannel->GetInterpolatorCount() > 1 ? pChannel->GetInterpolator(0) : 0), colours[colourIndex]);
				m_channelsCtrl.SetItemColor(hItem, colours[colourIndex]);
				colourIndex = (colourIndex + 1) % NUM_COLOURS;
			}
		}
	}

	m_pContext->SelectChannel(pSelectedChannel);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnTimelineChangeStart(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_waveCtrl)
		m_waveCtrl.BeginScrubbing();

	*pResult = FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnTimelineChangeEnd(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_waveCtrl)
		m_waveCtrl.EndScrubbing();

	*pResult = FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnTimelineChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	float fTime = m_timelineCtrl.GetTimeMarker();
	if (m_waveCtrl)
		m_waveCtrl.SetTimeMarker(fTime);
	if (m_pContext)
		m_pContext->SetSequenceTime(fTime);

	// Send a special facial event to redraw preview window to make preview smooth.
	if (m_pContext)
		m_pContext->SendEvent(EFD_EVENT_REDRAW_PREVIEW);

	if (m_timelineCtrl)
		m_timelineCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_splineCtrl)
		m_splineCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_waveCtrl)
		m_waveCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_phonemesCtrl)
		m_phonemesCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);

	*pResult = FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnTimelineDeletePressed(NMHDR* pNMHDR, LRESULT* pResult)
{
	m_splineCtrl.RemoveSelectedKeyTimes();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnWaveCtrlTimeChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	float fTime = m_waveCtrl.GetTimeMarker();
	if (m_pContext)
		m_pContext->SetSequenceTime(fTime);

	// Send a special facial event to redraw preview window to make preview smooth.
	if (m_pContext)
		m_pContext->SendEvent(EFD_EVENT_REDRAW_PREVIEW);

	if (m_timelineCtrl)
		m_timelineCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_splineCtrl)
		m_splineCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_waveCtrl)
		m_waveCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_phonemesCtrl)
		m_phonemesCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);

	*pResult = FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::SyncZoom()
{
	Vec2 waveZoom = m_waveCtrl.GetZoom();
	Vec2 waveOrigin = m_waveCtrl.GetScrollOffset();
	m_waveCtrl.SetZoom(Vec2(m_splineCtrl.GetZoom().x, waveZoom.y));
	m_waveCtrl.SetScrollOffset(Vec2(m_splineCtrl.GetScrollOffset().x, waveOrigin.y));
	m_phonemesCtrl.SetZoom(m_splineCtrl.GetZoom().x / 1000.0f);
	m_phonemesCtrl.SetScrollOffset(m_splineCtrl.GetScrollOffset().x * 1000.0f);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSplineScrollZoom(NMHDR* pNMHDR, LRESULT* pResult)
{
	SyncZoom();
	*pResult = FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSplineTimeChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	float fTime = m_splineCtrl.GetTimeMarker();
	if (m_pContext)
		m_pContext->SetSequenceTime(fTime);

	// Send a special facial event to redraw preview window to make preview smooth.
	if (m_pContext)
		m_pContext->SendEvent(EFD_EVENT_REDRAW_PREVIEW);

	if (m_timelineCtrl)
		m_timelineCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_splineCtrl)
		m_splineCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_waveCtrl)
		m_waveCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);
	if (m_phonemesCtrl)
		m_phonemesCtrl.RedrawWindow(NULL, NULL, RDW_UPDATENOW);

	*pResult = FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnWaveScrollZoom(NMHDR* pNMHDR, LRESULT* pResult)
{
	//m_splineCtrl.SetZoom( m_waveCtrl.GetZoom() );
	//m_splineCtrl.SetScrollOffset( m_waveCtrl.GetScrollOffset() );
	*pResult = FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSplineChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_pContext)
	{
		m_pContext->bSequenceModfied = true;

		CScopedVariableSetter<bool> variableSetter(m_bIgnoreSplineChangeEvents, true);

		m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSplineBeginChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_pContext)
		m_pContext->StopPreviewingEffector();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSplineCmd(UINT cmd)
{
	m_splineCtrl.OnUserCommand(cmd);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSplineCmdUpdateUI(CCmdUI* pCmdUI)
{
	switch (pCmdUI->m_nID)
	{
	case ID_SPLINE_SNAP_GRID_X:
		pCmdUI->SetCheck((m_splineCtrl.IsSnapTime()) ? TRUE : FALSE);
		break;
	case ID_SPLINE_SNAP_GRID_Y:
		pCmdUI->SetCheck((m_splineCtrl.IsSnapValue()) ? TRUE : FALSE);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnPlay()
{
	if (m_pContext)
		m_pContext->PlaySequence(!m_pContext->IsPlayingSequence());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnPlayUpdate(CCmdUI* pCmdUI)
{
	if (m_pContext)
	{
		pCmdUI->SetCheck((m_pContext->IsPlayingSequence()) ? 1 : 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnStop()
{
	if (m_pContext && m_pContext->GetSequence())
	{
		if (m_waveCtrl)
			m_waveCtrl.StopPlayback();

		m_pContext->PlaySequence(false);
		m_pContext->SetSequenceTime(m_pContext->GetSequence()->GetTimeRange().start);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	if (!m_pContext)
		return;

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	if (!pSequence)
		return;

	switch (event)
	{
	case EFD_EVENT_SEQUENCE_LOAD:
		{
			m_splineCtrl.FitSplineToViewWidth();
			m_splineCtrl.FitSplineToViewHeight();
		}
		break;
	case EFD_EVENT_SEQUENCE_UNDO:
	case EFD_EVENT_SEQUENCE_CHANGE:
		{
			m_channelsCtrl.Reload();
			OnSelectionChange();

			Range r = pSequence->GetTimeRange();
			if (m_timelineCtrl)
				m_timelineCtrl.SetTimeRange(r);
			if (m_splineCtrl)
				m_splineCtrl.SetTimeRange(r);
			if (m_waveCtrl)
			{
				m_waveCtrl.SetTimeRange(r);
				ReloadWaveCtrlSounds();
			}
			OnSelectionChange();
			OnSoundChanged();
			OnTimeChanged();
			break;
		}
	case EFD_EVENT_SEQUENCE_TIME_RANGE_CHANGE:
		{
			Range r = pSequence->GetTimeRange();
			if (m_timelineCtrl)
				m_timelineCtrl.SetTimeRange(r);
			if (m_splineCtrl)
				m_splineCtrl.SetTimeRange(r);
			if (m_waveCtrl)
			{
				m_waveCtrl.SetTimeRange(r);
			}
		}
		break;

	case EFD_EVENT_SEQUENCE_TIMES_CHANGE:
		{
			if (m_waveCtrl)
				ReloadWaveCtrlSounds();
			if (m_phonemesCtrl)
				ReloadPhonemeCtrl();
		}
		break;
	case EFD_EVENT_SEQUENCE_TIME:
		OnTimeChanged();
		break;
	case EFD_EVENT_SOUND_CHANGE:
		OnSoundChanged();
		break;
	case EFD_EVENT_SPLINE_CHANGE_CURRENT:
		m_splineCtrl.RedrawWindowAroundMarker();
		break;
	case EFD_EVENT_SPLINE_CHANGE:
		if (!m_bIgnoreSplineChangeEvents)
			m_splineCtrl.SplinesChanged();
		break;
	case EFD_EVENT_START_CHANGING_SPLINES:
		SelectChannels(nChannelCount, ppChannels, false);
		break;
	case EFD_SPLINES_NEED_ACTIVATING:
		SelectChannels(nChannelCount, ppChannels, true);
		break;
	case EFD_EVENT_SEQUENCE_PLAY_OR_STOP:
		{
			if (m_waveCtrl && m_pContext->IsPlayingSequence())
				m_waveCtrl.StartPlayback();
			else if (m_waveCtrl)
				m_waveCtrl.PausePlayback();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnTimeChanged()
{
	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	if (!pSequence)
		return;

	float fCurrentTime = (m_pContext ? m_pContext->GetSequenceTime() : 0.0f);

	if (m_timelineCtrl)
		m_timelineCtrl.SetTimeMarker(fCurrentTime);
	if (m_splineCtrl)
		m_splineCtrl.SetTimeMarker(fCurrentTime);
	if (m_waveCtrl && m_waveCtrl.GetTimeMarker() != fCurrentTime)
		m_waveCtrl.SetTimeMarker(fCurrentTime);

	if (m_phonemesCtrl)
		m_phonemesCtrl.SetTimeMarker(fCurrentTime);
	if (m_pTimeEdit)
	{
		CString timestr;
		int nmsec = fCurrentTime * 1000.0f;
		int nsec = nmsec / 1000;
		int mins = nsec / 60;
		int sec = nsec % 60;
		int ms = nmsec % 1000;
		timestr.Format("%02d:%02d.%03d", mins, sec, ms);
		m_pTimeEdit->SetEditText(timestr);
	}
	if (m_pFrameEdit)
	{
		CString frameString;
		frameString.Format("%03d", int(fCurrentTime * FACIAL_EDITOR_FPS));
		m_pFrameEdit->SetEditText(frameString);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSoundChanged()
{
	ReloadPhonemeCtrl();

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	if (!pSequence)
		return;

	if (m_waveCtrl)
	{
		ReloadWaveCtrlSounds();

		if (pSequence->GetFlags() & IFacialAnimSequence::FLAG_RANGE_FROM_SOUND)
		{
			Range r;
			r.Set(0, m_waveCtrl.CalculateTimeRange());
			pSequence->SetTimeRange(r);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSequenceProperties()
{
	if (!m_pContext || !m_pContext->GetSequence())
		return;
	CFacialSequencePropertiesDialog dlg;
	Range timeRange = m_pContext->GetSequence()->GetTimeRange();
	dlg.timeRange = timeRange;
	dlg.bRangeFromSound = m_pContext->GetSequence()->GetFlags() & IFacialAnimSequence::FLAG_RANGE_FROM_SOUND;
	if (dlg.DoModal() == IDOK)
	{
		CUndo undo("Set Sequence Properties");
		if (m_pContext)
			m_pContext->StoreSequenceUndo();

		if (dlg.bRangeFromSound)
		{
			m_pContext->GetSequence()->SetFlags(m_pContext->GetSequence()->GetFlags() | IFacialAnimSequence::FLAG_RANGE_FROM_SOUND);
			dlg.timeRange.Set(0, m_waveCtrl.CalculateTimeRange());
		}
		else
		{
			m_pContext->GetSequence()->SetFlags(m_pContext->GetSequence()->GetFlags() & ~IFacialAnimSequence::FLAG_RANGE_FROM_SOUND);
		}
		m_pContext->GetSequence()->SetTimeRange(dlg.timeRange);
		m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
		m_splineCtrl.RedrawWindow();
		m_timelineCtrl.RedrawWindow();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSpeedChange()
{
	m_waveCtrl.SetPlaybackSpeed(min(1.0f, max(0.0f, ReadPlaybackSpeedFromToolbar())));
	DisplayPlaybackSpeedInToolbar();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnFrameChange()
{
	if (m_pContext)
		m_pContext->SetSequenceTime(ReadFrameFromToolbar());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnZeroAll()
{
	m_splineCtrl.ZeroAll();
	if (m_pContext)
		m_pContext->StopPreviewingEffector();
	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnKeyAll()
{
	m_splineCtrl.KeyAll();
	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSkeletonAnim()
{
	if (m_pContext)
		m_pContext->SetAnimateSkeleton(!m_pContext->GetAnimateSkeleton());
	UpdateSkeletonAnimationStatus();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnCameraAnim()
{
	if (m_pContext)
		m_pContext->SetAnimateCamera(!m_pContext->GetAnimateCamera());
	UpdateCameraAnimationStatus();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnOverlapSounds()
{
	if (m_pContext)
		m_pContext->SetOverlapSounds(!m_pContext->GetOverlapSounds());
	UpdateOverlapSoundStatus();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnControlAmplitude()
{
	m_splineCtrl.SetControlAmplitude(!m_splineCtrl.GetControlAmplitude());
	UpdateControlAmplitudeStatus();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnSmoothSplineSigmaChange()
{
	m_fSmoothingSigma = min(100.0f, max(0.000001f, ReadSmoothSigmaFromToolbar()));
	UpdateSmoothSplineSigma();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnKeyCleanupErrorMaxChange()
{
	m_fKeyCleanupThreshold = min(1.0f, max(0.0f, ReadKeyCleanupErrorMaxFromToolbar()));
	UpdateKeyCleanupErrorMax();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnRemoveNoiseSigmaChange()
{
	m_fNoiseThreshold = min(100.0f, max(0.0000001f, ReadRemoveNoiseSigmaFromToolbar()));
	UpdateRemoveNoiseSigma();
}

//////////////////////////////////////////////////////////////////////////
float CFacialSequenceDialog::ReadSmoothSigmaFromToolbar()
{
	float value = m_fSmoothingSigma;
	if (m_pSmoothValueEdit)
	{
		CString text = m_pSmoothValueEdit->GetEditText();
		const char* buf = text.GetString();
		char* end;
		double sigma = strtod(buf, &end);
		if (buf != end)
			value = sigma;
	}
	return value;
}

//////////////////////////////////////////////////////////////////////////
float CFacialSequenceDialog::ReadKeyCleanupErrorMaxFromToolbar()
{
	float value = m_fKeyCleanupThreshold;
	if (m_pCleanupKeysValueEdit)
	{
		CString text = m_pCleanupKeysValueEdit->GetEditText();
		const char* buf = text.GetString();
		char* end;
		double sigma = strtod(buf, &end);
		if (buf != end)
			value = sigma;
	}
	return value;
}

//////////////////////////////////////////////////////////////////////////
float CFacialSequenceDialog::ReadRemoveNoiseSigmaFromToolbar()
{
	float value = m_fNoiseThreshold;
	if (m_pRemoveNoiseValueEdit)
	{
		CString text = m_pRemoveNoiseValueEdit->GetEditText();
		const char* buf = text.GetString();
		char* end;
		double sigma = strtod(buf, &end);
		if (buf != end)
			value = sigma;
	}
	return value;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnPhonemesBeforeChange(NMHDR* pNMHDR, LRESULT* pResult)
{
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnPhonemesChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_pContext)
		return;

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	if (!pSequence)
		return;

	m_pContext->bSequenceModfied = true;

	for (int soundEntryIndex = 0, soundEntryCount = pSequence->GetSoundEntryCount(); soundEntryIndex < soundEntryCount; ++soundEntryIndex)
	{
		IFacialAnimSoundEntry* pSoundEntry = pSequence->GetSoundEntry(soundEntryIndex);
		IFacialSentence* pSentence = (pSoundEntry ? pSoundEntry->GetSentence() : 0);
		if (!pSentence)
			continue;

		pSentence->ClearAllPhonemes();

		for (int i = 0; i < m_phonemesCtrl.GetPhonemeCount(soundEntryIndex); i++)
		{
			CPhonemesCtrl::Phoneme& ctrlPh = m_phonemesCtrl.GetPhoneme(soundEntryIndex, i);
			IFacialSentence::Phoneme ph;
			cry_strcpy(ph.phoneme, ctrlPh.sPhoneme);
			ph.time = ctrlPh.time0;
			ph.endtime = ctrlPh.time1;
			ph.intensity = ctrlPh.intensity;
			pSentence->AddPhoneme(ph);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnPhonemesPreview(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_pContext || !m_pContext->pLibrary)
		return;

	const char* phoneme = m_phonemesCtrl.GetMouseOverPhoneme();
	if (phoneme && *phoneme)
	{
		IFacialEffector* pPhonemeEffector = m_pContext->pLibrary->Find(phoneme);
		m_pContext->PreviewEffector(pPhonemeEffector);
	}
	else
	{
		m_pContext->StopPreviewingEffector();
	}

	if (m_pContext)
		m_pContext->SendEvent(EFD_EVENT_REDRAW_PREVIEW);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnPhonemesBake(NMHDR* pNMHDR, LRESULT* pResult)
{
	CUndo undo("Remove Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	// Add a channel for each phoneme.
	ISystem* pSystem = GetISystem();
	ICharacterManager* pCharManager = (pSystem ? pSystem->GetIAnimationSystem() : 0);
	IFacialAnimation* pFacialAnimation = (pCharManager ? pCharManager->GetIFacialAnimation() : 0);
	IPhonemeLibrary* pPhonemeLib = (pFacialAnimation ? pFacialAnimation->GetPhonemeLibrary() : 0);
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);

	IFacialAnimChannel* pBakedGroup = GetBakedLipSyncGroup();
	std::set<string> existingPhonemes;
	for (int i = 0; i < pSequence->GetChannelCount(); i++)
	{
		IFacialAnimChannel* pChannel = pSequence->GetChannel(i);
		if (pChannel && pChannel->GetParent() == pBakedGroup)
			existingPhonemes.insert(pChannel->GetEffectorName());
	}

	std::vector<string> phonemeNames;
	for (int phonemeIndex = 0, phonemeCount = (pPhonemeLib ? pPhonemeLib->GetPhonemeCount() : 0); phonemeIndex < phonemeCount; ++phonemeIndex)
	{
		SPhonemeInfo phonemeInfo;
		if (pPhonemeLib->GetPhonemeInfo(phonemeIndex, phonemeInfo))
			phonemeNames.push_back(phonemeInfo.ASCII);
	}

	IFacialEffectorsLibrary* pEffectorLib = (m_pContext ? m_pContext->pLibrary : 0);
	std::vector<IFacialAnimChannel*> phonemes;
	typedef std::map<string, int, stl::less_stricmp<const char*>> PhonemeChannelMap;
	PhonemeChannelMap phonemeChannelMap;
	for (int phonemeIndex = 0, phonemeCount = int(phonemeNames.size()); phonemeIndex < phonemeCount; ++phonemeIndex)
	{
		IFacialEffector* pEffector = (pEffectorLib ? pEffectorLib->Find(phonemeNames[phonemeIndex].c_str()) : 0);
		IFacialAnimChannel* pChannel = (pEffector && pSequence ? pSequence->CreateChannel() : 0);
		if (pChannel)
		{
			ISplineInterpolator* pSpline = pChannel->GetLastInterpolator();
			while (pSpline && pSpline->GetKeyCount())
				pSpline->RemoveKey(0);
			pChannel->SetEffector(pEffector);
			pChannel->SetParent(pBakedGroup);
			phonemeChannelMap.insert(std::make_pair(phonemeNames[phonemeIndex], int(phonemes.size())));
		}
		phonemes.push_back(pChannel);
	}

	// Loop through each frame, baking out a key for each phoneme.
	IFacialAnimChannel* pPhonemeStrengthChannel = 0;
	for (int i = 0; i < pSequence->GetChannelCount(); i++)
	{
		IFacialAnimChannel* pChannel = pSequence->GetChannel(i);
		if (pChannel && (pChannel->GetFlags() & IFacialAnimChannel::FLAG_PHONEME_STRENGTH))
			pPhonemeStrengthChannel = pChannel;
	}

	int frameCount = (m_pContext ? int(m_pContext->GetTimelineLength() * FACIAL_EDITOR_FPS) : 0);
	for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
	{
		float time = float(frameIndex) / FACIAL_EDITOR_FPS;

		float phonemeStrength = 0.0f;
		for (int splineIndex = 0, splineCount = (pPhonemeStrengthChannel ? pPhonemeStrengthChannel->GetInterpolatorCount() : 0); splineIndex < splineCount; ++splineIndex)
		{
			ISplineInterpolator* pSpline = (pPhonemeStrengthChannel ? pPhonemeStrengthChannel->GetInterpolator(splineIndex) : 0);
			float val = 0.0f;
			if (pSpline)
				pSpline->InterpolateFloat(time, val);
			phonemeStrength += val;
		}
		phonemeStrength = min(1.0f, max(-1.0f, phonemeStrength));
		if (!pPhonemeStrengthChannel)
			phonemeStrength = 1.0f;

		std::vector<int> lastKeyedFrames(phonemeNames.size());
		std::fill(lastKeyedFrames.begin(), lastKeyedFrames.end(), -1);
		for (int soundEntryIndex = 0, soundEntryCount = (pSequence ? pSequence->GetSoundEntryCount() : 0); soundEntryIndex < soundEntryCount; ++soundEntryIndex)
		{
			IFacialAnimSoundEntry* pSoundEntry = (pSequence ? pSequence->GetSoundEntry(soundEntryIndex) : 0);
			IFacialSentence* pSentence = (pSoundEntry ? pSoundEntry->GetSentence() : 0);
			float sentenceStart = (pSoundEntry ? pSoundEntry->GetStartTime() : -1.0f);
			float sentenceEnd = sentenceStart + m_waveCtrl.GetWaveformLength(soundEntryIndex);
			if (time >= sentenceStart && time < sentenceEnd)
			{
				IFacialSentence::ChannelSample samples[1000];
				enum {MAX_SAMPLES = sizeof(samples) / sizeof(samples[0])};
				int sampleCount = pSentence->Evaluate(time - sentenceStart, phonemeStrength, MAX_SAMPLES, samples);
				sampleCount = min(sampleCount, int(MAX_SAMPLES));

				for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
				{
					PhonemeChannelMap::iterator mapPosition = phonemeChannelMap.find(samples[sampleIndex].phoneme);
					int channelIndex = (mapPosition != phonemeChannelMap.end() ? (*mapPosition).second : -1);
					IFacialAnimChannel* pChannel = (channelIndex >= 0 ? phonemes[channelIndex] : 0);
					ISplineInterpolator* pSpline = (pChannel ? pChannel->GetLastInterpolator() : 0);
					if (pSpline)
					{
						pSpline->InsertKeyFloat(time, samples[sampleIndex].strength);
						lastKeyedFrames[channelIndex] = frameIndex;
					}
				}
			}
		}

		for (int phonemeIndex = 0, phonemeCount = int(phonemeNames.size()); phonemeIndex < phonemeCount; ++phonemeIndex)
		{
			if (lastKeyedFrames[phonemeIndex] < frameIndex)
			{
				IFacialAnimChannel* pChannel = phonemes[phonemeIndex];
				ISplineInterpolator* pSpline = (pChannel ? pChannel->GetLastInterpolator() : 0);
				if (pSpline)
					pSpline->InsertKeyFloat(time, 0.0f);
				lastKeyedFrames[phonemeIndex] = frameIndex;
			}
		}
	}

	// Send notifications.
	m_pContext->bSequenceModfied = true;
	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnPhonemesClear(NMHDR* pNMHDR, LRESULT* pResult)
{
	CUndo undo("Remove Channel");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	for (int soundEntryIndex = 0, soundEntryCount = (pSequence ? pSequence->GetSoundEntryCount() : 0); soundEntryIndex < soundEntryCount; ++soundEntryIndex)
	{
		IFacialSentence* pSentence = (pSequence ? pSequence->GetSoundEntry(soundEntryIndex)->GetSentence() : 0);
		if (pSentence)
		{
			pSentence->SetText("");
			pSentence->ClearAllWords();
			pSentence->ClearAllPhonemes();
		}
		m_waveCtrl.SetWaveformTextString(soundEntryIndex, "");
	}

	ReloadPhonemeCtrl();

	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::ReloadPhonemeCtrl()
{
	while (m_phonemesCtrl.GetSentenceCount())
		m_phonemesCtrl.DeleteSentence(0);

	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	if (!pSequence)
		return;

	for (int soundEntryIndex = 0, soundEntryCount = pSequence->GetSoundEntryCount(); soundEntryIndex < soundEntryCount; ++soundEntryIndex)
	{
		IFacialAnimSoundEntry* pSoundEntry = (pSequence ? pSequence->GetSoundEntry(soundEntryIndex) : 0);
		IFacialSentence* pSentence = (pSoundEntry ? pSoundEntry->GetSentence() : 0);
		if (!pSentence)
			continue;

		int sentenceIndex = m_phonemesCtrl.AddSentence();
		float startTime = (pSoundEntry ? pSoundEntry->GetStartTime() : 0.0f);
		float endTime = startTime + m_waveCtrl.GetWaveformLength(soundEntryIndex);
		m_phonemesCtrl.SetSentenceStartTime(sentenceIndex, startTime);
		m_phonemesCtrl.SetSentenceEndTime(sentenceIndex, endTime);

		int i;

		int nWords = pSentence->GetWordCount();
		for (i = 0; i < nWords; i++)
		{
			IFacialSentence::Word w;
			if (pSentence->GetWord(i, w))
			{
				CPhonemesCtrl::Word ctrlWord;
				ctrlWord.text = w.sWord;
				ctrlWord.time0 = w.startTime;
				ctrlWord.time1 = w.endTime;
				m_phonemesCtrl.AddWord(sentenceIndex, ctrlWord);
			}
		}

		int nPhonemes = pSentence->GetPhonemeCount();
		for (i = 0; i < nPhonemes; i++)
		{
			IFacialSentence::Phoneme ph;
			if (pSentence->GetPhoneme(i, ph))
			{
				CPhonemesCtrl::Phoneme ctrlPh;
				cry_strcpy(ctrlPh.sPhoneme, ph.phoneme);
				ctrlPh.time0 = ph.time;
				ctrlPh.time1 = ph.endtime;
				ctrlPh.intensity = ph.intensity;
				m_phonemesCtrl.AddPhoneme(sentenceIndex, ctrlPh);
			}
		}
	}

	m_phonemesCtrl.UpdatePhonemeLengths();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::LipSync(int waveformIndex)
{
	IFacialAnimSequence* pSequence = m_pContext->GetSequence();
	if (!pSequence)
		return;

	if (waveformIndex < 0 || waveformIndex > pSequence->GetSoundEntryCount())
		return;

	IFacialSentence* pSentence = (pSequence && pSequence->GetSoundEntryCount() ? pSequence->GetSoundEntry(waveformIndex)->GetSentence() : 0);
	if (!pSentence)
		return;

	CString wavFile = (pSequence && pSequence->GetSoundEntryCount() ? pSequence->GetSoundEntry(waveformIndex)->GetSoundFile() : "");
	if (wavFile.IsEmpty())
		return;
	wavFile = PathUtil::GamePathToCryPakPath(wavFile.GetString());

	CMultiLineStringDlg dlg("Enter Text for Lipsync");
	dlg.SetString(pSentence->GetText());
	if (dlg.DoModal() != IDOK)
		return;
	CString text = dlg.GetString();

	CUndo undo("LipSync");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	if (m_waveCtrl)
		m_waveCtrl.SetWaveformTextString(waveformIndex, text);

	CWaitCursor wait;

	if (DoPhonemeExtraction(wavFile, text, pSequence, waveformIndex))
		m_pContext->bSequenceModfied = true;
	ReloadPhonemeCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::RemoveSoundEntry(int waveformIndex)
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	if (m_pContext && pSequence)
	{
		{
			CUndo undo("Remove Sound");
			m_pContext->StoreSequenceUndo();

			pSequence->DeleteSoundEntry(waveformIndex);

			m_pContext->bSequenceModfied = true;
		}

		m_pContext->SendEvent(EFD_EVENT_SOUND_CHANGE, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::ClearAllSoundEntries()
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	if (m_pContext && pSequence)
	{
		{
			CUndo undo("Clear All Sounds");
			m_pContext->StoreSequenceUndo();

			while (pSequence->GetSoundEntryCount() > 0)
				pSequence->DeleteSoundEntry(0);
		}

		m_pContext->bSequenceModfied = true;

		m_pContext->SendEvent(EFD_EVENT_SOUND_CHANGE, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFacialSequenceDialog::DoPhonemeExtraction(CString wavFile, CString strText, IFacialAnimSequence* pSequence, int soundEntryIndex)
{
	IFacialSentence* pSentence = (pSequence && pSequence->GetSoundEntryCount() ? pSequence->GetSoundEntry(soundEntryIndex)->GetSentence() : 0);
	if (!pSentence)
		return false;

	ILipSyncPhonemeRecognizer::SSentance* pOuSentenece;
	CPhonemesAnalyzer analyzer;

	// First try extracting phonemes using the text that has been passed in.
	bool extractedPhonemes = false;
	CString phonemeText;
	if (analyzer.Analyze(wavFile, strText, &pOuSentenece))
	{
		extractedPhonemes = true;
		phonemeText = strText;
	}
	else
	{
		CryLogAlways("Phoneme Extraction failed for file \"%s\"", wavFile.GetString());
		CryLogAlways("  Used text was \"%s\"", strText);
		CryLogAlways("  Error message was \"%s\"", analyzer.GetLastError());
	}

	// If that failed, attempt to generate phonemes without the text.
	if (!extractedPhonemes)
	{
		if (analyzer.Analyze(wavFile, "", &pOuSentenece))
		{
			extractedPhonemes = true;
			phonemeText = "";
			CryLogAlways("  Successfully extracted phonemes using empty text");
		}
		else
		{
			CryLogAlways("  Also failed when using empty text: \"%s\"", analyzer.GetLastError());
		}
	}

	// If we still haven't been able to generate phonemes, fail.
	if (!extractedPhonemes)
		return false;

	pSentence->SetText(phonemeText);
	IPhonemeLibrary* pPhonemeLib = pSentence->GetPhonemeLib();

	pSentence->ClearAllPhonemes();
	for (int w = 0; w < (int)pOuSentenece->nWordCount; w++)
	{
		ILipSyncPhonemeRecognizer::SWord& outWord = (pOuSentenece->pWords[w]);

		IFacialSentence::Word wrd;
		wrd.startTime = outWord.startTime;
		wrd.endTime = outWord.endTime;
		wrd.sWord = outWord.sWord;

		pSentence->AddWord(wrd);
	}
	for (int p = 0; p < pOuSentenece->nPhonemeCount; p++)
	{
		ILipSyncPhonemeRecognizer::SPhoneme& outPhoneme = pOuSentenece->pPhonemes[p];

		IFacialSentence::Phoneme ph;
		ph.time = outPhoneme.startTime;
		ph.endtime = outPhoneme.endTime;
		cry_strcpy(ph.phoneme, outPhoneme.sPhoneme);
		ph.intensity = outPhoneme.intensity;
		pSentence->AddPhoneme(ph);
	}

	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
	return true;
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialSequenceDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CFacialEdContext::MoveToKeyDirection direction = (zDelta < 0 ? CFacialEdContext::MoveToKeyDirectionBackward : CFacialEdContext::MoveToKeyDirectionForward);
	if (m_pContext)
		m_pContext->MoveToFrame(direction);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* pTreeViewNM = (NMTREEVIEW*)pNMHDR;
	HTREEITEM hItem = m_channelsCtrl.HitTest(pTreeViewNM->ptDrag);
	IFacialAnimChannel* pChannel = (IFacialAnimChannel*)m_channelsCtrl.GetItemData(hItem);
	if (m_pContext && pChannel)
		m_pContext->DoChannelDragDrop(pChannel);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::OnDestroy()
{
	if (m_pDropTarget)
		delete m_pDropTarget;

	if (m_waveCtrl)
	{
		while (m_waveCtrl.GetWaveformCount())
			m_waveCtrl.DeleteWaveform(0);
		m_waveCtrl.DeleteUnusedSounds();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::UpdateSmoothSplineSigma()
{
	string text;
	text.Format("%0.4f", m_fSmoothingSigma);
	if (m_pSmoothValueEdit)
		m_pSmoothValueEdit->SetEditText(text.c_str());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::UpdateKeyCleanupErrorMax()
{
	string text;
	text.Format("%0.4f", m_fKeyCleanupThreshold);
	if (m_pCleanupKeysValueEdit)
		m_pCleanupKeysValueEdit->SetEditText(text.c_str());
}

//////////////////////////////////////////////////////////////////////////
void CFacialSequenceDialog::UpdateRemoveNoiseSigma()
{
	string text;
	text.Format("%0.4f", m_fNoiseThreshold);
	if (m_pRemoveNoiseValueEdit)
		m_pRemoveNoiseValueEdit->SetEditText(text.c_str());
}

