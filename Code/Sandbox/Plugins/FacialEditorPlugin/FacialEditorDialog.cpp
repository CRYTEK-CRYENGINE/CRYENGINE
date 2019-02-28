// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Util/Image.h"
#include "FacialEditorDialog.h"
#include "QtViewPane.h"
#include "Dialogs/QStringDialog.h"
#include "SelectAnimationDialog.h"
#include <CrySystem/File/IAVI_Reader.h>
#include <CryMovie/IMovieSystem.h>
#include "JoystickUtils.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/ICryAnimation.h>
#include <CrySystem/ILocalizationManager.h>
#include "QT/Widgets/QWaitProgress.h"
#include "FilePathUtil.h"
#include "Util/MFCUtil.h"
#include "Util/FileUtil.h"
#include "ModelViewport.h"
#include "Dialogs/QNumericBoxDialog.h"

#define FACEED_FILE_FILTER            ""

#define FACEED_PROJECT_EXT            "fpj"
#define FACEED_LIBRARY_EXT            "fxl"
#define FACEED_SEQUENCE_EXT           "fsq"
#define FACEED_JOYSTICK_EXT           "joy"
#define FACEED_PROJECT_FILTER         "Facial Editor Project Files (*.fpj)|*.fpj"
#define FACEED_LIBRARY_FILTER         "Facial Expressions Library Files (*.fxl)|*.fxl"
#define FACEED_SEQUENCE_FILTER        "Facial Sequence Files (*.fsq)|*.fsq"
#define FACEED_JOYSTICK_FILTER        "Facial Joystick Files (*.joy)|*.joy"
#define FACEED_CHARACTER_FILES_FILTER "Facial Character And Character Definition Files (*.cdf;*.chr)|*.cdf;*.chr"

#define IDW_FE_PREVIEW_PANE           AFX_IDW_CONTROLBAR_FIRST + 10
#define IDW_FE_SLIDERS_PANE           AFX_IDW_CONTROLBAR_FIRST + 11
#define IDW_FE_EXPRESSIONS_PANE       AFX_IDW_CONTROLBAR_FIRST + 12
#define IDW_FE_SEQUENCE_PANE          AFX_IDW_CONTROLBAR_FIRST + 13
#define IDW_FE_PREVIEW_OPTIONS_PANE   AFX_IDW_CONTROLBAR_FIRST + 14
#define IDW_FE_JOYSTICK_PANE          AFX_IDW_CONTROLBAR_FIRST + 15
#define IDW_FE_VIDEO_FRAME_PANE       AFX_IDW_CONTROLBAR_FIRST + 16

#define FACEED_DIALOGFRAME_CLASSNAME  "FacialEditorDialog"

#include "Controls/QuestionDialog.h"

//////////////////////////////////////////////////////////////////////////
class CFacialEditorViewClass : public IViewPaneClass
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID()	 override { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       override { return FACIAL_EDITOR_NAME; };
	virtual const char*    Category()        override { return "Animation"; };
	virtual const char*    GetMenuPath()     override { return "Animation"; }
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CFacialEditorDialog); };
	virtual const char*    GetPaneTitle()    override { return _T(FACIAL_EDITOR_NAME); };
	virtual bool           SinglePane()      override { return false; };
};

REGISTER_CLASS_DESC(CFacialEditorViewClass)

//////////////////////////////////////////////////////////////////////////
BOOL CFacialEditorView::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	if (!m_hWnd)
	{
		//////////////////////////////////////////////////////////////////////////
		// Create window.
		//////////////////////////////////////////////////////////////////////////
		CRect rcDefault(0, 0, 100, 100);
		LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
		VERIFY(CreateEx(NULL, lpClassName, "FacialEditorView", dwStyle, rcDefault, pParentWnd, nID));

		if (!m_hWnd)
			return FALSE;
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorView::OnDraw(CDC* pDC)
{
	CRect rc;
	GetClientRect(rc);
	pDC->FillRect(rc, CBrush::FromHandle((HBRUSH)GetStockObject(GRAY_BRUSH)));
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CFacialEditorDialog, CXTPFrameWnd)

BEGIN_MESSAGE_MAP(CFacialEditorDialog, CXTPFrameWnd)
ON_WM_SIZE()
ON_WM_SETFOCUS()
ON_WM_DESTROY()
ON_WM_CLOSE()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()

ON_COMMAND_EX(ID_VIEW_GRAPHS, OnToggleBar)
ON_COMMAND_EX(ID_VIEW_NODEINPUTS, OnToggleBar)
ON_COMMAND_EX(ID_VIEW_COMPONENTS, OnToggleBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_GRAPHS, OnUpdateControlBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_NODEINPUTS, OnUpdateControlBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_COMPONENTS, OnUpdateControlBar)

ON_COMMAND(ID_PROJECT_NEW, OnProjectNew)
ON_COMMAND(ID_PROJECT_OPEN, OnProjectOpen)
ON_COMMAND(ID_PROJECT_SAVE, OnProjectSave)
ON_COMMAND(ID_PROJECT_SAVEAS, OnProjectSaveAs)

ON_COMMAND(ID_EXP_LIBRARY_NEW, OnLibraryNew)
ON_COMMAND(ID_EXP_LIBRARY_OPEN, OnLibraryOpen)
ON_COMMAND(ID_EXP_LIBRARY_SAVE, OnLibrarySave)
ON_COMMAND(ID_EXP_LIBRARY_SAVEAS, OnLibrarySaveAs)
ON_COMMAND(ID_EXP_LIBRARY_EXPORT, OnLibraryExport)
ON_COMMAND(ID_EXP_LIBRARY_IMPORT, OnLibraryImport)
ON_COMMAND(ID_EXP_LIBRARY_BATCHUPDATEEXPRESSIONLIBRARIES, OnLibraryBatchUpdateLibraries)

ON_COMMAND(ID_SEQUENCE_NEW, OnSequenceNew)
ON_COMMAND(ID_SEQUENCE_OPEN, OnSequenceOpen)
ON_COMMAND(ID_SEQUENCE_SAVE, OnSequenceSave)
ON_COMMAND(ID_SEQUENCE_SAVEAS, OnSequenceSaveAs)
ON_COMMAND(ID_SEQUENCE_LOADSOUND, OnSequenceLoadSound)
ON_COMMAND(ID_SEQUENCE_LOADSKELETONANIMATION, OnSequenceLoadSkeletonAnimation)
ON_COMMAND(ID_SEQUENCE_TEXT_LIPSYNC, OnSequenceLipSync)
ON_COMMAND(ID_SEQUENCE_IMPORT_FBX_TO_SEQUENCE, OnSequenceImportFBX)

ON_COMMAND(ID_JOYSTICKS_NEW, OnJoysticksNew)
ON_COMMAND(ID_JOYSTICKS_OPEN, OnJoysticksOpen)
ON_COMMAND(ID_JOYSTICKS_SAVE, OnJoysticksSave)
ON_COMMAND(ID_JOYSTICKS_SAVEAS, OnJoysticksSaveAs)

ON_COMMAND(ID_JOYSTICKS_CREATEEXPRESSIONFROMCURRENTPOSITIONS, OnCreateExpressionFromCurrentPositions)

ON_COMMAND(ID_BATCHPROCESS_TEXTLESSEXTRACTIONOFDIRECTORY, OnBatchProcess_PhonemeExtraction)
ON_COMMAND(ID_SEQUENCE_BATCHAPPLYEXPRESSION, OnBatchProcess_ApplyExpression)
ON_COMMAND(ID_SEQUENCE_EXPORTSELECTEDEXPRESSIONS, OnSequenceExportSelectedExpressions)
ON_COMMAND(ID_SEQUENCE_IMPORTEXPRESSIONS, OnSequenceImportExpressions)
ON_COMMAND(ID_SEQUENCE_BATCHUPDATESEQUENCES, OnSequenceBatchUpdateSequences)
ON_COMMAND(ID_SEQUENCE_LOADVIDEOEXTRACTEDSEQUENCE, OnSequenceLoadVideoExtractedSequence)
ON_COMMAND(ID_SEQUENCE_LOADVIDEOIGNORESEQUENCE, OnSequenceLoadC3DFile)
ON_COMMAND(ID_SEQUENCE_LOADGROUPFILE, OnSeqenceLoadGroupFile)

ON_COMMAND(ID_CHARACTER_LOAD, OnLoadCharacter)
ON_WM_MOUSEWHEEL()

//////////////////////////////////////////////////////////////////////////
// XT Commands.
ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)

ON_COMMAND(ID_EXPRESSIONSLIBRARY_MORPHCHECK, OnMorphCheck)

ON_COMMAND(ID_TOOLS_CUSTOMIZEKEYBOARD, OnCustomize)
ON_COMMAND(ID_TOOLS_EXPORT_SHORTCUTS, OnExportShortcuts)
ON_COMMAND(ID_TOOLS_IMPORT_SHORTCUTS, OnImportShortcuts)
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CFacialEditorDialog::CFacialEditorDialog()
	: m_morphCheckReportDialog("Morph Check")
{
	m_pContext = 0;

	m_pDragImage = NULL;

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, FACEED_DIALOGFRAME_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wndcls.lpfnWndProc = ::DefWindowProc;
		wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
		wndcls.hInstance = hInst;
		wndcls.hIcon = NULL;
		wndcls.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground = (HBRUSH) (COLOR_3DFACE + 1);
		wndcls.lpszMenuName = NULL;
		wndcls.lpszClassName = FACEED_DIALOGFRAME_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
	CRect rc(0, 0, 0, 0);
	BOOL bRes = Create(WS_CHILD, rc, AfxGetMainWnd());
	if (!bRes)
		return;
	ASSERT(bRes);

	OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
CFacialEditorDialog::~CFacialEditorDialog()
{
	if (m_lastProject != "")
	{
		if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Save Current Project?")))
		{
			SaveCurrentProject(false);
		}
	}

	if (m_pContext)
		delete m_pContext;
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialEditorDialog::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd)
{
	return __super::Create(FACEED_DIALOGFRAME_CLASSNAME, NULL, dwStyle, rect, pParentWnd);
}

//////////////////////////////////////////////////////////////////////////
int CFacialEditorDialog::GetNumMorphTargets() const
{
	IFacialModel* pModel = (m_pContext ? m_pContext->pModel : 0);
	return (pModel ? pModel->GetEffectorCount() : 0);
}

//////////////////////////////////////////////////////////////////////////
const char* CFacialEditorDialog::GetMorphTargetName(int index) const
{
	IFacialModel* pModel = (m_pContext ? m_pContext->pModel : 0);
	IFacialEffector* pEffector = (pModel && index >= 0 && index < pModel->GetEffectorCount() ? pModel->GetEffector(index) : 0);
	const char* name = (pEffector ? pEffector->GetName() : 0);
	return (name ? name : "");
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::PreviewEffector(int index, float value)
{
	IFacialModel* pModel = (m_pContext ? m_pContext->pModel : 0);
	IFacialEffector* pEffector = (pModel && index >= 0 && index < pModel->GetEffectorCount() ? pModel->GetEffector(index) : 0);
	m_paneSliders.SetMorphWeight(pEffector, value);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::ClearAllPreviewEffectors()
{
	m_paneSliders.ClearAllMorphs();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::SetForcedNeckRotation(const Quat& rotation)
{
	m_panePreview.SetForcedNeckRotation(rotation);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::SetForcedEyeRotation(const Quat& rotation, EyeType eye)
{
	m_panePreview.SetForcedEyeRotation(rotation, eye);
}

//////////////////////////////////////////////////////////////////////////
int CFacialEditorDialog::GetJoystickCount() const
{
	IJoystickSet* pJoysticks = (m_pContext ? m_pContext->GetJoystickSet() : 0);
	return (pJoysticks ? pJoysticks->GetJoystickCount() : 0);
}

//////////////////////////////////////////////////////////////////////////
const char* CFacialEditorDialog::GetJoystickName(int joystickIndex) const
{
	IJoystickSet* pJoysticks = (m_pContext ? m_pContext->GetJoystickSet() : 0);
	IJoystick* pJoystick = (pJoysticks && joystickIndex >= 0 && joystickIndex < pJoysticks->GetJoystickCount() ? pJoysticks->GetJoystick(joystickIndex) : 0);
	const char* name = (pJoystick ? pJoystick->GetName() : 0);
	return (name ? name : "");
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::SetJoystickPosition(int joystickIndex, float x, float y)
{
	IJoystickSet* pJoysticks = (m_pContext ? m_pContext->GetJoystickSet() : 0);
	IJoystick* pJoystick = (pJoysticks && joystickIndex >= 0 && joystickIndex < pJoysticks->GetJoystickCount() ? pJoysticks->GetJoystick(joystickIndex) : 0);
	float time = (m_pContext ? m_pContext->GetSequenceTime() : 0);
	float axisValues[2] = { x, y };
	for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
	{
		IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
		bool axisFlipped = (pChannel ? pChannel->GetFlipped() : 0);
		float value = (axisFlipped ? -axisValues[axis] : axisValues[axis]);
		JoystickUtils::SetKey(pChannel, time, value);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::GetJoystickPosition(int joystickIndex, float& x, float& y) const
{
	IJoystickSet* pJoysticks = (m_pContext ? m_pContext->GetJoystickSet() : 0);
	IJoystick* pJoystick = (pJoysticks && joystickIndex >= 0 && joystickIndex < pJoysticks->GetJoystickCount() ? pJoysticks->GetJoystick(joystickIndex) : 0);
	float time = (m_pContext ? m_pContext->GetSequenceTime() : 0);
	float* axisValues[2] = { &x, &y };
	for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
	{
		IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
		bool axisFlipped = (pChannel ? pChannel->GetFlipped() : 0);
		float value = (pChannel ? JoystickUtils::Evaluate(pChannel, time) : 0.0f);
		if (axisValues[axis])
			*axisValues[axis] = (axisFlipped ? -value : value);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::LoadJoystickFile(const char* filename)
{
	m_pContext->LoadJoystickSet(filename);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::LoadCharacter(const char* filename)
{
	m_pContext->LoadCharacter(filename);
	SetContext(m_pContext);   // Reload context.
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::LoadSequence(const char* filename)
{
	m_pContext->LoadSequence(filename);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::SetVideoFrameResolution(int width, int height, int bpp)
{
	m_paneVideoFrame.SetResolution(width, height, bpp);
}

//////////////////////////////////////////////////////////////////////////
int CFacialEditorDialog::GetVideoFramePitch()
{
	return m_paneVideoFrame.GetPitch();
}

//////////////////////////////////////////////////////////////////////////
void* CFacialEditorDialog::GetVideoFrameBits()
{
	return m_paneVideoFrame.GetBits();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::ShowVideoFramePane()
{
	GetDockingPaneManager()->ShowPane(IDW_FE_VIDEO_FRAME_PANE, FALSE);
	CRect rect;
	m_paneVideoFrame.GetWindowRect(&rect);
	m_paneVideoFrame.OnSize(0, rect.Width(), rect.Height());
}

BOOL CFacialEditorDialog::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	BOOL res = FALSE;
	if (m_view.m_hWnd)
	{
		res = m_view.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		if (TRUE == res)
			return res;
	}

	return __super::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialEditorDialog::OnInitDialog()
{
	CRect rc;
	GetClientRect(&rc);

	try
	{
		//////////////////////////////////////////////////////////////////////////
		// Initialize the command bars
		if (!InitCommandBars())
			return -1;

	}
	catch (CResourceException* e)
	{
		e->Delete();
		return -1;
	}

	// Get a pointer to the command bars object.
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if (pCommandBars == NULL)
	{
		TRACE0("Failed to create command bars object.\n");
		return -1;      // fail to create
	}

	CMFCUtils::LoadShortcuts(GetCommandBars(), IDR_FACED_MENU, "FacialEditor");

	// Add the menu bar
	CXTPCommandBar* pMenuBar = pCommandBars->SetMenu(_T("Menu Bar"), IDR_FACED_MENU);
	ASSERT(pMenuBar);
	pMenuBar->SetFlags(xtpFlagStretched);
	pMenuBar->EnableCustomization(TRUE);

	//////////////////////////////////////////////////////////////////////////
	GetDockingPaneManager()->InstallDockingPanes(this);
	GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);

	//////////////////////////////////////////////////////////////////////////

	// Create View.
	m_view.Create(WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, rc, this, AFX_IDW_PANE_FIRST);

	{
		m_paneExpressions.Create(IDD_DATABASE, this);
		CXTPDockingPane* pPane = GetDockingPaneManager()->CreatePane(IDW_FE_EXPRESSIONS_PANE, CRect(0, 0, 400, 400), xtpPaneDockLeft);
		pPane->SetTitle(_T("Expressions Explorer"));
		pPane->SetOptions(xtpPaneNoCloseable);
	}

	{
		m_paneSliders.Create(IDD_DATABASE, this);
		CXTPDockingPane* pPane = GetDockingPaneManager()->CreatePane(IDW_FE_SLIDERS_PANE, CRect(0, 0, 400, 400), xtpPaneDockLeft);
		pPane->SetTitle(_T("Effectors Sliders"));
		pPane->SetOptions(xtpPaneNoCloseable);
	}

	CXTPDockingPane* pPreviewPane = 0;
	{
		m_panePreview.Create(IDD_DATABASE, this);
		pPreviewPane = GetDockingPaneManager()->CreatePane(IDW_FE_PREVIEW_PANE, CRect(0, 0, 400, 400), xtpPaneDockLeft);
		pPreviewPane->SetTitle(_T("Preview"));
		pPreviewPane->SetOptions(xtpPaneNoCloseable);
	}

	{
		m_panePreviewOptions.SetViewport(&m_panePreview);
		m_panePreviewOptions.Create(IDD_DATABASE, this);
		CXTPDockingPane* pPane = GetDockingPaneManager()->CreatePane(IDW_FE_PREVIEW_OPTIONS_PANE, CRect(0, 0, 200, 200), xtpPaneDockBottom, pPreviewPane);
		pPane->SetTitle(_T("Preview Options"));
		pPane->SetOptions(xtpPaneNoCloseable);
	}

	CXTPDockingPane* pSequencePane = 0;
	{
		m_paneSequence.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 600, 240), this, 0);
		pSequencePane = GetDockingPaneManager()->CreatePane(IDW_FE_SEQUENCE_PANE, CRect(0, 0, 600, 240), xtpPaneDockBottom);
		pSequencePane->SetTitle(_T("Sequence"));
		pSequencePane->SetOptions(xtpPaneNoCloseable);
	}

	{
		m_paneJoysticks.Create(IDD_DATABASE, this);
		CXTPDockingPane* pPane = GetDockingPaneManager()->CreatePane(IDW_FE_JOYSTICK_PANE, CRect(0, 0, 240, 240), xtpPaneDockRight, pSequencePane);
		pPane->SetTitle(_T("Joysticks"));
		pPane->SetOptions(xtpPaneNoCloseable);
	}

	{
		m_paneVideoFrame.Create(IDD_DATABASE, this);
		CXTPDockingPane* pPane = GetDockingPaneManager()->CreatePane(IDW_FE_VIDEO_FRAME_PANE, CRect(0, 0, 240, 240), xtpPaneDockRight, pPreviewPane);
		pPane->SetTitle(_T("Video Frame"));
	}

	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	if (layout.Load(_T("FacialEditorLayout")))
	{
		if (layout.GetPaneList().GetCount() > 0)
			GetDockingPaneManager()->SetLayout(&layout);
	}

	MakeNewProject();

	CXTRegistryManager regMgr;
	m_lastCharacter = regMgr.GetProfileString(_T("Dialogs\\FaceEd"), _T("LastCharacter"), m_lastCharacter);
	m_lastSequence = regMgr.GetProfileString(_T("Dialogs\\FaceEd"), _T("LastSequence"), m_lastSequence);
	m_lastProject = regMgr.GetProfileString(_T("Dialogs\\FaceEd"), _T("LastProject"), m_lastProject);

	if (m_lastProject != "")
	{
		bool loadResult = false;
		loadResult = m_pContext->LoadProject(m_lastProject);

		if (loadResult)
		{
			CString tmpTitle("");
			SetProjectTitle(tmpTitle);
		}
	}

	SetContext(m_pContext);   // Reload context.

	//QT_TODO("TODO: Commands disabled now that Facial editor is in plugin, if necessary refactor commands into static methods that call into singleton dialog");
	/*
	CEditorCommandManager* comMan = GetIEditor()->GetCommandManager();


	if (comMan->IsRegistered("face_editor.next_key") == false)
		CommandManagerHelper::RegisterCommand(comMan, "face_editor", "next_key", CCommandDescription(""), functor(*this, &CFacialEditorDialog::OnNextKey));

	if (comMan->IsRegistered("face_editor.prev_key") == false)
		CommandManagerHelper::RegisterCommand(comMan, "face_editor", "prev_key", CCommandDescription(""), functor(*this, &CFacialEditorDialog::OnPrevKey));

	if (comMan->IsRegistered("face_editor.start_stop") == false)
		CommandManagerHelper::RegisterCommand(comMan, "face_editor", "start_stop", CCommandDescription(""), functor(*this, &CFacialEditorDialog::OnStartStop));

	if (comMan->IsRegistered("face_editor.key_all") == false)
		CommandManagerHelper::RegisterCommand(comMan, "face_editor", "key_all", CCommandDescription(""), functor(*this, &CFacialEditorDialog::OnKeyAll));

	if (comMan->IsRegistered("face_editor.select_all") == false)
		CommandManagerHelper::RegisterCommand(comMan, "face_editor", "select_all", CCommandDescription(""), functor(*this, &CFacialEditorDialog::OnSelectAll));

	if (comMan->IsRegistered("face_editor.next_frame") == false)
		CommandManagerHelper::RegisterCommand(comMan, "face_editor", "next_frame", CCommandDescription(""), functor(*this, &CFacialEditorDialog::OnNextFrame));

	if (comMan->IsRegistered("face_editor.prev_frame") == false)
		CommandManagerHelper::RegisterCommand(comMan, "face_editor", "prev_frame", CCommandDescription(""), functor(*this, &CFacialEditorDialog::OnPrevFrame));
	*/

	// Load filter settings from registry.
	{
		double v;
		if (regMgr.GetProfileDouble(_T("Dialogs\\FaceEd"), _T("SmoothingSigma"), &v))
			m_paneSequence.SetSmoothingSigma(v);
	}
	{
		double v;
		if (regMgr.GetProfileDouble(_T("Dialogs\\FaceEd"), _T("NoiseThreshold"), &v))
			m_paneSequence.SetNoiseThreshold(v);
	}
	{
		double v;
		if (regMgr.GetProfileDouble(_T("Dialogs\\FaceEd"), _T("KeyCleanupThreshold"), &v))
			m_paneSequence.SetKeyCleanupThreshold(v);
	}

	return TRUE;
}

void CFacialEditorDialog::SetProjectTitle(CString& getTitle)
{
	CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(IDW_FE_SEQUENCE_PANE);
	if (!pPane)
		return;

	CString seqName("");
	seqName = m_pContext->GetSequence()->GetName();

	if (strlen(seqName) == 0)
		seqName = "new";

	pPane->SetTitle(_T("Project: " + CString(PathUtil::GetFileName(m_lastProject).c_str()) + "  Sequence: " + seqName));
	getTitle = pPane->GetTitle();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnDestroy()
{
	/*GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.next_key");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.prev_key");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.start_stop");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.next_key");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.prev_key");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.start_stop");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.key_all");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.select_all");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.next_frame");
	GetIEditor()->GetCommandManager()->UnregisterCommand("face_editor.prev_frame");*/

	if (m_pContext)
	{
		if (m_pContext->pCharacter)
		{
			m_lastCharacter = m_pContext->pCharacter->GetFilePath();
		}
		if (m_pContext->GetSequence() && strlen(m_pContext->GetSequence()->GetName()) > 0)
		{
			m_lastSequence = m_pContext->GetSequence()->GetName();
		}
	}

	CXTRegistryManager regMgr;
	regMgr.WriteProfileString(_T("Dialogs\\FaceEd"), _T("LastCharacter"), m_lastCharacter);
	regMgr.WriteProfileString(_T("Dialogs\\FaceEd"), _T("LastSequence"), m_lastSequence);
	regMgr.WriteProfileString(_T("Dialogs\\FaceEd"), _T("LastProject"), m_lastProject);

	// Store filter settings to registry.
	{
		double v = m_paneSequence.GetSmoothingSigma();
		regMgr.WriteProfileDouble(_T("Dialogs\\FaceEd"), _T("SmoothingSigma"), &v);
	}
	{
		double v = m_paneSequence.GetNoiseThreshold();
		regMgr.WriteProfileDouble(_T("Dialogs\\FaceEd"), _T("NoiseThreshold"), &v);
	}
	{
		double v = m_paneSequence.GetKeyCleanupThreshold();
		regMgr.WriteProfileDouble(_T("Dialogs\\FaceEd"), _T("KeyCleanupThreshold"), &v);
	}

	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	GetDockingPaneManager()->GetLayout(&layout);
	layout.Save(_T("FacialEditorLayout"));

	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialEditorDialog::PreTranslateMessage(MSG* pMsg)
{
	bool bFramePreTranslate = true;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		CWnd* pWnd = CWnd::GetFocus();
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CEdit)))
			bFramePreTranslate = false;
	}

	if (bFramePreTranslate)
	{
		// allow tooltip messages to be filtered
		if (__super::PreTranslateMessage(pMsg))
			return TRUE;
	}

	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		// All key presses are translated by this frame window

		::TranslateMessage(pMsg);
		::DispatchMessage(pMsg);

		return TRUE;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
BOOL CFacialEditorDialog::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnPaint()
{
	CPaintDC PaintDC(this); // device context for painting
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEditorDialog::CloseCurrentSequence()
{
	if (m_pContext->GetSequence())
		m_lastSequence = m_pContext->GetSequence()->GetName();
	if (m_pContext->bSequenceModfied)
	{
		QDialogButtonBox::StandardButton res = QDialogButtonBox::StandardButton::Yes;

		res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Facial Sequence was modified.\r\nDo you want to save your changes?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Yes)
		{
			if (!SaveCurrentSequence())
				return false;
		}
		else if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return false;
		}

	}
	m_pContext->bSequenceModfied = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEditorDialog::CloseCurrentLibrary()
{
	if (m_pContext->GetLibrary())
		m_lastLibrary = m_pContext->GetLibrary()->GetName();
	if (m_pContext->bLibraryModfied)
	{
		// As to save old project.
		QDialogButtonBox::StandardButton res = QDialogButtonBox::StandardButton::Yes;

		res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Save Current Expressions Library?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Yes)
		{
			if (!SaveCurrentLibrary())
			{
				return false;
			}
		}
		else if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return false;
		}
	}

	m_pContext->bLibraryModfied = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEditorDialog::CloseCurrentJoysticks()
{
	if (m_pContext->bJoysticksModfied)
	{
		QDialogButtonBox::StandardButton res = QDialogButtonBox::StandardButton::Yes;

		res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Save Current Joystick Set?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return false;
		}
		if (res == QDialogButtonBox::StandardButton::Yes)
		{
			if (!SaveCurrentJoysticks())
				return false;
		}
	}
	m_pContext->bJoysticksModfied = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
typedef struct
{
	//float fScale;
	char szName[64];
}tJoystickTemp;

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::MergeVideoExtractedSequence(const char* filename, float startTime, bool loadSequence)
{
	// Load the file
	FILE* fpVideoFile = fopen(filename, "rt");
	if (!fpVideoFile)
		return;

	CUndo undo("Merge Video Extracted Sequence");
	if (m_pContext)
	{
		m_pContext->StoreSequenceUndo();
	}

	ShowVideoFramePane();

	// Add a phoneme strength channel if one does not already exist.
	IFacialAnimChannel* pPhonemeStrengthChannel = 0;
	for (int i = 0; i < m_pContext->GetSequence()->GetChannelCount(); i++)
	{
		if (m_pContext->GetSequence()->GetChannel(i)->GetFlags() & IFacialAnimChannel::FLAG_PHONEME_STRENGTH)
			pPhonemeStrengthChannel = m_pContext->GetSequence()->GetChannel(i);
	}
	if (!pPhonemeStrengthChannel)
	{
		IFacialAnimChannel* pLipSyncGroup = m_paneSequence.GetLipSyncGroup();
		if (!pLipSyncGroup)
		{
			fclose(fpVideoFile);
			fpVideoFile = NULL;
			return;
		}

		pPhonemeStrengthChannel = m_pContext->GetSequence()->CreateChannel();
		pPhonemeStrengthChannel->SetName("Phoneme Strength");
		pPhonemeStrengthChannel->SetFlags(IFacialAnimChannel::FLAG_PHONEME_STRENGTH);
		pPhonemeStrengthChannel->SetParent(pLipSyncGroup);
	}

	// read video file, number of frames, FPS and video resolution

	char szText[256];
	char szVideoFile[256];
	int nFrames;
	int nFPS;
	int w, h;

	while (1)
	{
		fgets(szText, 256, fpVideoFile);
		// at the beginning skip any line with comments
		if (szText[0] != '/')
			break;
	}

	sscanf(szText, "VideoFile=%s Frames=%d FPS=%d Width=%d Height=%d\n", szVideoFile, &nFrames, &nFPS, &w, &h);

	fgets(szText, 256, fpVideoFile);
	OutputDebugString(szText);
	sscanf(szText, "Markers=%d\n", &m_pContext->m_nMarkers);

	tJoystickTemp* lstScaleJoystick = new tJoystickTemp[256];

	for (int k = 0; k < 256; k++)
	{
		lstScaleJoystick[k].szName[0] = 0;
		//lstScaleJoystick[k].fScale=1.0f;
	}

	for (int k = 0; k < m_pContext->m_nMarkers; k++)
	{
		char szDummy[256];
		fgets(szText, 256, fpVideoFile);
		float fVal1, fVal2;
		sscanf(szText, "Joystick=%s Scale=(%f,%f)\n", szDummy, &fVal1, &fVal2);
		cry_strcpy(lstScaleJoystick[k].szName, szDummy);
		OutputDebugString(szText);

		char szDebug[256];
		cry_sprintf(szDebug, "Joystick=%s,Scale=%f,%f\n", lstScaleJoystick[k].szName, fVal1, fVal2);
		OutputDebugString(szDebug);
	}

	if (m_pContext)
	{
		char szFolder[256];
		const char* szEnd = filename + strlen(filename) - 1;
		while (*szEnd != '\\' && *szEnd != '/')
			szEnd--;
		const char* szSrc = filename;
		int k = 0;
		szEnd++;
		while (szSrc != szEnd)
			szFolder[k++] = *szSrc++;
		szFolder[k] = 0;

		char szDebug[256];
		CString szVideoFilename = PathUtil::AddBackslash(PathUtil::GetPathWithoutFilename(filename).c_str()) + szVideoFile;
		cry_sprintf(szDebug, "\nLoading %s\n", szVideoFilename.GetString());
		OutputDebugString(szDebug);

		m_pContext->GetVideoFrameReader().AddAVI(szVideoFilename.GetString(), startTime);
		//IAVI_Reader *pAVIReader=m_pContext->GetAVIReader();
		//pAVIReader->OpenFile(szFilename);
		//	OutputDebugString("Cannot open %s\n",szFilename);

		m_pContext->m_lstPosDebug.clear();
		// frame 0 is empty
		tMarkerDebugInfo tPos;
		for (int k = 0; k < 16; k++)
			tPos.fX[k] = tPos.fY[k] = 0;
		m_pContext->m_lstPosDebug.push_back(tPos);
	}

	// set video frame resolution
	SetVideoFrameResolution(w, h, 24);

	// time is in seconds
	float fVideoFPS = 1.0f / (float)(nFPS);

	int nCurrFrame = 0;

	int nCount = GetJoystickCount();
	float* fPreviousDirX = new float[nCount];
	float* fPreviousDirY = new float[nCount];
	float* fCurrDirX = new float[nCount];
	float* fCurrDirY = new float[nCount];
	int* nJoystickSet = new int[nCount];

	for (int k = 0; k < nCount; k++)
	{
		fPreviousDirX[k] = 0;
		fPreviousDirY[k] = 0;
		fCurrDirX[k] = 0;
		fCurrDirY[k] = 0;
		nJoystickSet[k] = 0;
	} //k

	int nJoystickPhonemeStrenght = -1;

	float fLenSeconds = fVideoFPS * nFrames;

	for (int nFrameIndex = 0; nFrameIndex < nFrames; ++nFrameIndex)
	{
		//float oldTime = (nFrameIndex - 1) * fVideoFPS;
		float time = nFrameIndex * fVideoFPS;

		// Play back the sequence.
		if (fpVideoFile)
		{
			float fCurrTimeDiff = fVideoFPS; //time-oldTime;

			//if (fCurrTimeDiff>1.0f || fCurrTimeDiff<0)
			// reset, has been a big delay inbetween frames,
			// otherwise looks like it accelerates and then decelerates
			//fCurrTimeDiff=1.0f;

			// check if it is time to read the next frame
			//if (fCurrTimeDiff > fVideoFPS)
			{
				int nDummy;
				char szText[256];
				char szBuffer[256];

				//fCurrTimeDiff-=fVideoFPS; // adjust it precisely

				fgets(szText, 256, fpVideoFile);

				if (strncmp(szText, "End of file", 11) == 0)
				{
					// end of sequence
					fclose(fpVideoFile);
					fpVideoFile = NULL;
					break;
				}

				sscanf(szText, "FrameStart %d/%d\n", &nCurrFrame, &nDummy);

				tMarkerDebugInfo tPos;
				m_pContext->m_lstPosDebug.push_back(tPos);
				m_pContext->m_nMarkersDebug = 0;

				while (1)
				{
					fgets(szText, 256, fpVideoFile);

					if (strncmp(szText, "FrameEnd", 8) == 0)
						break; //end of this frame

					if (strncmp(szText, "Feature", 7) == 0)
					{
						// TODO: joystick names have silly spaces in them, sscanf doesnt parse them correctly
						// sscanf(szText,"Feature %s\n",szBuffer); // feature name
						cry_strcpy(szBuffer, &szText[8]); // skip "Feature "
						int nLen = strlen(szBuffer);
						szBuffer[nLen - 1] = 0; //skip '\n'

						fgets(szText, 256, fpVideoFile);  // position
						for (int joystickIndex = 0, joystickCount = GetJoystickCount(); joystickIndex < joystickCount; ++joystickIndex)
						{
							float fDirX, fDirY;
							sscanf(szText, "PositionJoystick=(%f,%f)\n", &fDirX, &fDirY);

							//OutputDebugString(szText);

							if (stricmp(GetJoystickName(joystickIndex), szBuffer) == 0)
							{

								float fScale = 1.0f;

								for (int k = 0; k < m_pContext->m_nMarkers; k++)
								{
									if (stricmp(lstScaleJoystick[k].szName, szBuffer) == 0)
									{
										//fScale=lstScaleJoystick[k].fScale;

										if (stricmp(lstScaleJoystick[k].szName, "chin") == 0)
											nJoystickPhonemeStrenght = joystickIndex;

										break;
									}
								} //k

								IJoystickSet* pJoysticks = (m_pContext ? m_pContext->GetJoystickSet() : 0);
								IJoystick* pJoystick = (pJoysticks ? pJoysticks->GetJoystick(joystickIndex) : 0);
								float* axisValues[] = { &fDirX, &fDirY };
								for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
								{
									IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
									*axisValues[axis] *= (pChannel ? pChannel->GetVideoMarkerScale() : 1.0f);
									*axisValues[axis] += (pChannel ? pChannel->GetVideoMarkerOffset() : 0.0f);
								}

								{
									fCurrDirX[joystickIndex] = fDirX;
									fCurrDirY[joystickIndex] = fDirY;
								}

								fPreviousDirX[joystickIndex] = fCurrDirX[joystickIndex];
								fPreviousDirY[joystickIndex] = fCurrDirY[joystickIndex];

								fCurrDirX[joystickIndex] = fDirX;
								fCurrDirY[joystickIndex] = fDirY;

								// If this is the first value we have found for this joystick, clear the keys from the start of the sequence to the current time.
								if (!nJoystickSet[joystickIndex])
								{
									IJoystickSet* pJoysticks = (m_pContext ? m_pContext->GetJoystickSet() : 0);
									IJoystick* pJoystick = (pJoysticks ? pJoysticks->GetJoystick(joystickIndex) : 0);
									for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
									{
										IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
										JoystickUtils::RemoveKeysInRange(pChannel, startTime, startTime + time);
									}
								}

								nJoystickSet[joystickIndex] = 1;

								fgets(szText, 256, fpVideoFile);  // position2D
								//OutputDebugString(szText);
								tMarkerDebugInfo* pPos = &m_pContext->m_lstPosDebug[nCurrFrame];
								sscanf(szText, "Position2D=(%f,%f)\n", &pPos->fX[m_pContext->m_nMarkersDebug], &pPos->fY[m_pContext->m_nMarkersDebug]);
								m_pContext->m_nMarkersDebug++;

								break;
							}
						} // joystick index

					}
				} //1
			}

			float fLerp = fCurrTimeDiff / (fVideoFPS);
			//fLerp=0;
			if (fLerp < 0) fLerp = 0;
			else if (fLerp > 1.0f)
				fLerp = 1.0f;

			for (int joystickIndex = 0, joystickCount = GetJoystickCount(); joystickIndex < joystickCount; ++joystickIndex)
			{
				if (nJoystickSet[joystickIndex] == 0)
					continue;

				float fDirs[2];
				fDirs[0] = fPreviousDirX[joystickIndex] + fLerp * (fCurrDirX[joystickIndex] - fPreviousDirX[joystickIndex]);
				fDirs[1] = fPreviousDirY[joystickIndex] + fLerp * (fCurrDirY[joystickIndex] - fPreviousDirY[joystickIndex]);

				/*
				   if (joystickIndex==nJoystickPhonemeStrenght)
				   {
				   ISplineInterpolator* pPhonemeStrengthSpline = pPhonemeStrengthChannel->GetInterpolator();
				   if (pPhonemeStrengthSpline)
				   {
				    pPhonemeStrengthSpline->RemoveKeysInRange(time, time + fVideoFPS);
				    //pPhonemeStrengthSpline->InsertKeyFloat(time, fDirs[1]);
				    pPhonemeStrengthSpline->InsertKeyFloat(time, 1);
				   }
				   }
				 */

				IJoystickSet* pJoysticks = (m_pContext ? m_pContext->GetJoystickSet() : 0);
				IJoystick* pJoystick = (pJoysticks ? pJoysticks->GetJoystick(joystickIndex) : 0);
				for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
				{
					IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
					if (loadSequence)
					{
						JoystickUtils::RemoveKeysInRange(pChannel, startTime + time, startTime + time + fVideoFPS);
						float value = (pChannel && pChannel->GetFlipped() ? -fDirs[axis] : fDirs[axis]);
						JoystickUtils::SetKey(pChannel, startTime + time, value);
					}
				} //axis
			}
		}
	}

	if (m_pContext && m_pContext->GetSequence())
		m_pContext->ConvertSequenceToCorrectFrameRate(m_pContext->GetSequence());

	SAFE_DELETE_ARRAY(lstScaleJoystick);
	SAFE_DELETE_ARRAY(fCurrDirX);
	SAFE_DELETE_ARRAY(fCurrDirY);
	SAFE_DELETE_ARRAY(fPreviousDirX);
	SAFE_DELETE_ARRAY(fPreviousDirY);
	SAFE_DELETE_ARRAY(nJoystickSet);

	m_pContext->bSequenceModfied = true;
	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE);
	DisplayCurrentVideoFrame();
}

//////////////////////////////////////////////////////////////////////////
struct LoadGroupFileSoundEntry
{
	LoadGroupFileSoundEntry(const string& filename, float time) : filename(filename), time(time), existingPosition(-1) {}
	string filename;
	float  time;
	int    existingPosition;
};
struct LoadGroupFileSkeletonAnimationEntry
{
	LoadGroupFileSkeletonAnimationEntry(const string& animationName, float time) : animationName(animationName), time(time) {}
	string animationName;
	float  time;
};
struct LoadGroupFileSoundEntryExistingPositionOrderingPredicate : public std::binary_function<bool, LoadGroupFileSoundEntry, LoadGroupFileSoundEntry>
{
	bool operator()(const LoadGroupFileSoundEntry& left, const LoadGroupFileSoundEntry& right) const
	{
		return left.existingPosition > right.existingPosition; // We want to sort the existing positions in *reverse* order.
	}
};
void CFacialEditorDialog::LoadGroupFile(const char* filename)
{
	XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filename);

	if (!root)
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Error loading group file"));
		return;
	}

	std::vector<LoadGroupFileSoundEntry> soundsToLoad;
	std::vector<LoadGroupFileSkeletonAnimationEntry> skeletonAnimationsToLoad;
	std::vector<float> fovKeyTimes;
	std::vector<float> fovKeyValues;
	std::vector<float> cameraPositionKeyTimes;
	std::vector<Vec3> cameraPositionKeyValues;
	std::vector<float> cameraOrientationKeyTimes;
	std::vector<Quat> cameraOrientationKeyValues;
	float length = 1.0f;
	{
#pragma message("TODO: Can't access TrackView directly from here anymore")
		/*// We are going to load the objects using the engine, and then undo our changes to get rid of them
		   // (since we don't really need the objects, just some of the information, but this way we can re-use
		   // existing code and insulate ourselves from format changes).
		   std::vector<CBaseObject*> objectsLoaded;
		   std::vector<CSequenceObject*> sequenceObjects;
		   {
		   // The loading must be done in a separate scope, since the sequence objects are only fully
		   // resolved when the archive is destructed.
		   CObjectArchive ar( GetIEditor()->GetObjectManager(),root,true );

		   XmlNodeRef objectsNode = ar.node;
		   int numObjects = objectsNode->getChildCount();
		   for (int i = 0; i < numObjects; i++)
		   {
		    ar.node = objectsNode->getChild(i);
		    CBaseObject *obj = ar.LoadObject( objectsNode->getChild(i) );
		    objectsLoaded.push_back(obj);
		    if (obj && ::strcmp("SequenceObject", obj->GetTypeName()) == 0)
		      sequenceObjects.push_back(static_cast<CSequenceObject*>(obj));
		   }
		   }

		   // Check whether there was a sequence in the group file.
		   if (sequenceObjects.empty())
		   CQuestionDialog::SCritical(QObject::tr(""),QObject::tr("Group file contains no sequences."));

		   // Select the sequence to use.
		   IAnimSequence* pSequence = 0;
		   if (!sequenceObjects.empty())
		   {
		   // Just use the first sequence.
		   if (sequenceObjects.size() > 1)
		    CQuestionDialog::SWarning(QObject::tr("Warning"),QObject::tr("Group file contains multiple sequences - using the first one."));

		   pSequence = sequenceObjects[0]->GetSequence();
		   Range range = (pSequence ? pSequence->GetTimeRange() : Range(0.0f, 1.0f));
		   length = range.end - range.start;
		   }

		   // Choose the entity from the sequence.
		   IAnimNode* pNode = 0;
		   {
		   std::vector<IAnimNode*> entityNodes;
		   for (int nodeIndex = 0, nodeCount = (pSequence ? pSequence->GetNodeCount() : 0); nodeIndex < nodeCount; ++nodeIndex)
		   {
		    IAnimNode* pAnimNode = (pSequence ? pSequence->GetNode(nodeIndex) : 0);
		    EAnimNodeType type = (pAnimNode ? pAnimNode->GetType() : EAnimNodeType(0));
		    if (type == eAnimNodeType_Entity)
		      entityNodes.push_back(pAnimNode);
		   }

		   {
		    std::vector<CString> items;
		    std::vector<int> itemNodeMap;
		    for (int nodeIndex = 0, nodeCount = entityNodes.size(); nodeIndex < nodeCount	; ++nodeIndex)
		    {
		      IAnimNode* pEntityNode = entityNodes[nodeIndex];
		      const char* name = (pEntityNode ? pEntityNode->GetName() : 0);
		      if (name)
		      {
		        items.push_back(name);
		        itemNodeMap.push_back(nodeIndex);
		      }
		    }

		    if (items.empty())
		      CQuestionDialog::SWarning(QObject::tr("Warning"),QObject::tr("Sequence contains no valid entity nodes"));

		    CGenericSelectItemDialog dlg(this);
		    dlg.SetTitle("Select Entity from Group File");
		    dlg.SetItems(items);
		    if (!items.empty() && dlg.DoModal() == IDOK)
		    {
		      for (int itemIndex = 0, itemCount = items.size(); itemIndex < itemCount; ++itemIndex)
		        pNode = ((items[itemIndex] == dlg.GetSelectedItem()) ? entityNodes[itemNodeMap[itemIndex]] : pNode);
		    }
		   }
		   }

		   // Choose the camera from the sequence.
		   IAnimNode* pCameraNode = 0;
		   {
		   std::vector<IAnimNode*> cameraNodes;
		   for (int nodeIndex = 0, nodeCount = (pSequence ? pSequence->GetNodeCount() : 0); nodeIndex < nodeCount; ++nodeIndex)
		   {
		    IAnimNode* pAnimNode = (pSequence ? pSequence->GetNode(nodeIndex) : 0);
		    EAnimNodeType type = (pAnimNode ? pAnimNode->GetType() : EAnimNodeType(0));
		    if (type == eAnimNodeType_Camera)
		      cameraNodes.push_back(pAnimNode);
		   }

		   {
		    std::vector<CString> items;
		    std::vector<int> itemNodeMap;
		    for (int nodeIndex = 0, nodeCount = cameraNodes.size(); nodeIndex < nodeCount	; ++nodeIndex)
		    {
		      IAnimNode* pCameraNode = cameraNodes[nodeIndex];
		      const char* name = (pCameraNode ? pCameraNode->GetName() : 0);
		      if (name)
		      {
		        items.push_back(name);
		        itemNodeMap.push_back(nodeIndex);
		      }
		    }

		    if (items.size() == 1)
		    {
		      pCameraNode = cameraNodes[0];
		    }
		    else if (items.size() > 1)
		    {
		      CGenericSelectItemDialog dlg(this);
		      dlg.SetTitle("Select Camera from Group File");
		      dlg.SetItems(items);
		      if (!items.empty() && dlg.DoModal() == IDOK)
		      {
		        for (int itemIndex = 0, itemCount = items.size(); itemIndex < itemCount; ++itemIndex)
		          pCameraNode = ((items[itemIndex] == dlg.GetSelectedItem()) ? cameraNodes[itemNodeMap[itemIndex]] : pCameraNode);
		      }
		    }
		   }
		   }

		   if (pNode)
		   {
		   // Load the sounds.
		   {
		    IAnimTrack* pSoundTrack = (pNode ? pNode->GetTrackForParameter(eAnimParamType_Sound) : 0);
		    for (int keyIndex = 0, keyCount = (pSoundTrack ? pSoundTrack->GetNumKeys() : 0); keyIndex < keyCount; ++keyIndex)
		    {
		      const char* filename = 0;
		      float duration;
		      if (pSoundTrack)
		        pSoundTrack->GetKeyInfo(keyIndex, filename, duration);
		      if (filename)
		      {
		        CString waveFile = Path::ReplaceExtension(filename, "wav");
		        soundsToLoad.push_back(LoadGroupFileSoundEntry(waveFile.GetString(), pSoundTrack->GetKeyTime(keyIndex)));
		      }
		    }
		   }

		   // Load the animation.
		   {
		    IAnimTrack* pAnimTrack = (pNode ? pNode->GetTrackForParameter(eAnimParamType_Animation) : 0);
		    for (int keyIndex = 0, keyCount = (pAnimTrack ? pAnimTrack->GetNumKeys() : 0); keyIndex < keyCount; ++keyIndex)
		    {
		      const char* anim = 0;
		      float duration;
		      if (pAnimTrack)
		        pAnimTrack->GetKeyInfo(keyIndex, anim, duration);
		      float animationStartTime = pAnimTrack->GetKeyTime(keyIndex);
		      if (anim)
		        skeletonAnimationsToLoad.push_back(LoadGroupFileSkeletonAnimationEntry(anim, animationStartTime));
		    }
		   }
		   }

		   // Set up the fovs.
		   {
		   if (pCameraNode)
		   {
		    IAnimTrack* pFOVTrack = (pCameraNode ? pCameraNode->GetTrackForParameter(eAnimParamType_FOV) : 0);
		    int numFOVKeys = (pFOVTrack ? pFOVTrack->GetNumKeys() : 0);

		    // Loop through all the frames for the sequence.
		    if (numFOVKeys > 0)
		    {
		      int frameCount = int(length * FACIAL_EDITOR_FPS);
		      for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
		      {
		        float frameTime = float(frameIndex) / FACIAL_EDITOR_FPS;

		        float fov = 0.0f;
		        if (pFOVTrack)
		        {
		          fov = boost::get<float>(pFOVTrack->GetValue(frameTime));
		        }

		        fovKeyTimes.push_back(frameTime);
		        fovKeyValues.push_back(fov);
		      }
		    }
		   }
		   }

		   // Set up the camera tracking.
		   {
		   if (pCameraNode)
		   {
		    IAnimTrack* pCameraPosTrack = (pCameraNode ? pCameraNode->GetTrackForParameter(eAnimParamType_Position) : 0);
		    IAnimTrack* pEntityPosTrack = (pNode ? pNode->GetTrackForParameter(eAnimParamType_Position) : 0);
		    IAnimTrack* pCameraRotTrack = (pCameraNode ? pCameraNode->GetTrackForParameter(eAnimParamType_Rotation) : 0);
		    IAnimTrack* pEntityRotTrack = (pNode ? pNode->GetTrackForParameter(eAnimParamType_Rotation) : 0);

		    int numCameraPosKeys = (pCameraPosTrack ? pCameraPosTrack->GetNumKeys() : 0);
		    int numCameraRotKeys = (pCameraRotTrack ? pCameraRotTrack->GetNumKeys() : 0);
		    int numEntityPosKeys = (pEntityPosTrack ? pEntityPosTrack->GetNumKeys() : 0);
		    int numEntityRotKeys = (pEntityRotTrack ? pEntityRotTrack->GetNumKeys() : 0);

		    Vec3 cameraPosition = (pCameraNode ? pCameraNode->QueryEntityNodeInterface()->GetPos() : Vec3(ZERO));
		    Quat cameraRotation = (pCameraNode ? pCameraNode->QueryEntityNodeInterface()->GetRotate() : Quat(IDENTITY));
		    Vec3 entityPosition = (pNode ? pNode->QueryEntityNodeInterface()->GetPos() : Vec3(ZERO));
		    Quat entityRotation = (pNode ? pNode->QueryEntityNodeInterface()->GetRotate() : Quat(IDENTITY));

		    // Loop through all the frames for the sequence.
		    int frameCount = int(length * FACIAL_EDITOR_FPS);
		    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
		    {
		      float frameTime = float(frameIndex) / FACIAL_EDITOR_FPS;

		      Vec3 vSceneCameraPos(cameraPosition);
		      if (pCameraPosTrack && numCameraPosKeys > 0)
		      {
		        vSceneCameraPos = boost::get<Vec3>(pCameraPosTrack->GetValue(frameTime));
		      }

		      Vec3 vEntityPos(entityPosition);
		      if (pEntityPosTrack && numEntityPosKeys > 0)
		      {
		        vEntityPos = boost::get<Vec3>(pEntityPosTrack->GetValue(frameTime));
		      }

		      Quat vSceneCameraRot(cameraRotation);
		      if (pCameraRotTrack && numCameraRotKeys > 0)
		      {
		        vSceneCameraRot = boost::get<Quat>(pEntityPosTrack->GetValue(frameTime));
		      }

		      Quat vEntityRot(entityRotation);
		      if (pEntityRotTrack && numEntityRotKeys > 0)
		      {
		        vEntityRot = boost::get<Quat>(pEntityPosTrack->GetValue(frameTime));
		      }

		      QuatT sceneCameraTm(vSceneCameraPos, vSceneCameraRot);
		      QuatT entityTm(vEntityPos, vEntityRot);

		      QuatT relativeCameraTm = entityTm.GetInverted() * sceneCameraTm;

		      Vec3 vRelativeCameraPos = relativeCameraTm.t;
		      cameraPositionKeyTimes.push_back(frameTime);
		      cameraPositionKeyValues.push_back(vRelativeCameraPos);

		      Quat vRelativeCameraRot = relativeCameraTm.q;
		      cameraOrientationKeyTimes.push_back(frameTime);
		      cameraOrientationKeyValues.push_back(vRelativeCameraRot);
		    }
		   }
		   }

		   // Delete all the sequence objects.
		   for (int objectIndex = 0, objectCount = objectsLoaded.size(); objectIndex < objectCount; ++objectIndex)
		   GetIEditor()->GetObjectManager()->DeleteObject(objectsLoaded[objectIndex]);*/
	}

	// Create a mapping between sounds and the text that is associated with them.
	PhonemeExtractionSoundDatabase soundDB;
	CreatePhonemeExtractionSoundDatabase(soundDB);

	CUndo undo("Load Group File");
	if (m_pContext)
		m_pContext->StoreSequenceUndo();

	// Check whether any of the sounds we found in the group file already exist in the sequence - if so,
	// move the existing sound entries rather than create new ones.
	{
		// Make a list of all the sounds currently in the file, so we can update them if they are still in
		// the new file.
		IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
		typedef std::multimap<string, int> SoundFileEntryMap;
		SoundFileEntryMap soundFileEntryMap;

		for (int soundEntryIndex = 0, soundEntryCount = pSequence->GetSoundEntryCount(); soundEntryIndex < soundEntryCount; ++soundEntryIndex)
		{
			IFacialAnimSoundEntry* pSoundEntry = pSequence->GetSoundEntry(soundEntryIndex);
			const char* szSoundFile = (pSoundEntry ? pSoundEntry->GetSoundFile() : 0);
			if (szSoundFile)
				soundFileEntryMap.insert(std::make_pair(szSoundFile, soundEntryIndex));
		}

		for (int soundIndex = 0, soundCount = soundsToLoad.size(); soundIndex < soundCount; ++soundIndex)
		{
			CString gamePath = PathUtil::MakeGamePath(soundsToLoad[soundIndex].filename);
			SoundFileEntryMap::iterator itExistingSoundEntry = soundFileEntryMap.find(gamePath.GetString());
			if (itExistingSoundEntry != soundFileEntryMap.end())
			{
				// The sound is already in the sequence - remember the current index so we can update it later.
				soundsToLoad[soundIndex].existingPosition = (*itExistingSoundEntry).second;
				soundFileEntryMap.erase(itExistingSoundEntry);
			}
		}

		// Now we sort the sounds to load by their existing positions - *in reverse order*. This is so that
		// when we insert new sound entries we don't screw up existing indices.
		std::sort(soundsToLoad.begin(), soundsToLoad.end(), LoadGroupFileSoundEntryExistingPositionOrderingPredicate());
	}

	// Load all the sounds we found.
	{
		IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);

		for (int soundIndex = 0, soundCount = soundsToLoad.size(); soundIndex < soundCount; ++soundIndex)
		{
			if (pSequence && soundsToLoad[soundIndex].existingPosition != -1)
			{
				IFacialAnimSoundEntry* pSoundEntry = pSequence->GetSoundEntry(soundsToLoad[soundIndex].existingPosition);
				if (pSoundEntry)
					pSoundEntry->SetStartTime(soundsToLoad[soundIndex].time);
			}
			else if (pSequence)
			{
				// This sound has been added to the trackview - we need to add it to the sound.
				CString gamePath = PathUtil::MakeGamePath(soundsToLoad[soundIndex].filename);
				int soundEntryIndex = pSequence->GetSoundEntryCount();
				pSequence->InsertSoundEntry(soundEntryIndex);
				pSequence->GetSoundEntry(soundEntryIndex)->SetSoundFile(gamePath.GetString());
				pSequence->GetSoundEntry(soundEntryIndex)->SetStartTime(soundsToLoad[soundIndex].time);

				// Search the database for text that corresponds with this sound file.
				const char* text = "";
				{
					PhonemeExtractionSoundDatabase::iterator entry = soundDB.find(PathUtil::MakeGamePath(soundsToLoad[soundIndex].filename));
					if (entry != soundDB.end())
						text = (*entry).second.sOriginalActorLine; // TODO verify if this should be "swTranslatedActorLine" instead
				}

				// Perform phoneme extraction.
				m_paneSequence.DoPhonemeExtraction(PathUtil::GamePathToCryPakPath(gamePath.GetString()).c_str(), MakeSafeText(text), pSequence, soundEntryIndex);
			}
		}

		// Set the skeletal animations.
		while (pSequence && pSequence->GetSkeletonAnimationEntryCount())
			pSequence->DeleteSkeletonAnimationEntry(0);
		if (skeletonAnimationsToLoad.empty())
			CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("Entity contains no skeletal animations - animate skeleton will not work."));
		for (int animationToLoadIndex = 0, animationToLoadCount = int(skeletonAnimationsToLoad.size()); animationToLoadIndex < animationToLoadCount; ++animationToLoadIndex)
		{
			if (pSequence)
			{
				int skeletonAnimationIndex = pSequence->GetSkeletonAnimationEntryCount();
				pSequence->InsertSkeletonAnimationEntry(animationToLoadIndex);
				IFacialAnimSkeletonAnimationEntry* pSkeletonAnimationEntry = pSequence->GetSkeletonAnimationEntry(skeletonAnimationIndex);
				if (pSkeletonAnimationEntry)
				{
					pSkeletonAnimationEntry->SetName(skeletonAnimationsToLoad[animationToLoadIndex].animationName);
					pSkeletonAnimationEntry->SetStartTime(skeletonAnimationsToLoad[animationToLoadIndex].time);
				}
			}
		}

		// Set up the fovs.
		{
			ISplineInterpolator* pFOVSpline = (pSequence ? pSequence->GetCameraPathFOV() : 0);
			while (pFOVSpline && pFOVSpline->GetKeyCount())
				pFOVSpline->RemoveKey(0);

			// Loop through all the frames for the sequence.
			for (int keyIndex = 0, keyCount = int(min(fovKeyTimes.size(), fovKeyValues.size())); keyIndex < keyCount; ++keyIndex)
			{
				if (pFOVSpline)
					pFOVSpline->InsertKeyFloat(fovKeyTimes[keyIndex], fovKeyValues[keyIndex]);
			}
		}

		// Set up the camera tracking.
		{
			ISplineInterpolator* pPositionSpline = (pSequence ? pSequence->GetCameraPathPosition() : 0);
			while (pPositionSpline && pPositionSpline->GetKeyCount())
				pPositionSpline->RemoveKey(0);

			for (int keyIndex = 0, keyCount = int(min(cameraPositionKeyTimes.size(), cameraPositionKeyValues.size())); keyIndex < keyCount; ++keyIndex)
			{
				if (pPositionSpline)
				{
					const Vec3& key3 = cameraPositionKeyValues[keyIndex];
					float key4[4] = { key3.x, key3.y, key3.z, 0.0f };
					pPositionSpline->InsertKey(cameraPositionKeyTimes[keyIndex], key4);
				}
			}

			ISplineInterpolator* pOrientationSpline = (pSequence ? pSequence->GetCameraPathOrientation() : 0);
			while (pOrientationSpline && pOrientationSpline->GetKeyCount())
				pOrientationSpline->RemoveKey(0);

			for (int keyIndex = 0, keyCount = int(min(cameraOrientationKeyTimes.size(), cameraOrientationKeyValues.size())); keyIndex < keyCount; ++keyIndex)
			{
				if (pOrientationSpline)
					pOrientationSpline->InsertKey(cameraOrientationKeyTimes[keyIndex], (float*)&cameraOrientationKeyValues[keyIndex]);
			}
		}

		// Set the length.
		if (pSequence)
			pSequence->SetTimeRange(Range(0.0f, length));
	}

	m_pContext->bSequenceModfied = true;
	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE, 0);
}

//////////////////////////////////////////////////////////////////////////
_smart_ptr<IFacialEffectorsLibrary> CFacialEditorDialog::CreateLibraryOfSelectedEffectors()
{
	return m_paneExpressions.CreateLibraryOfSelectedEffectors();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::UpdateSkeletonAnimText()
{
	CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(IDW_FE_SEQUENCE_PANE);
	CString title("");
	SetProjectTitle(title);

	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	float sequenceTime = (m_pContext ? m_pContext->GetSequenceTime() : 0);
	if (pPane && pSequence)
	{
		int skeletonAnimationToPlayIndex = -1;
		for (int skeletonAnimationIndex = 0, skeletonAnimationCount = (pSequence ? pSequence->GetSkeletonAnimationEntryCount() : 0); skeletonAnimationIndex < skeletonAnimationCount; ++skeletonAnimationIndex)
		{
			IFacialAnimSkeletonAnimationEntry* pEntry = (pSequence ? pSequence->GetSkeletonAnimationEntry(skeletonAnimationIndex) : 0);
			float animationStartTime = (pEntry ? pEntry->GetStartTime() : 0.0f);
			if (animationStartTime <= sequenceTime)
				skeletonAnimationToPlayIndex = skeletonAnimationIndex;
		}

		IFacialAnimSkeletonAnimationEntry* pSkeletonAnimationEntry = (pSequence && skeletonAnimationToPlayIndex >= 0 ? pSequence->GetSkeletonAnimationEntry(skeletonAnimationToPlayIndex) : 0);
		if (skeletonAnimationToPlayIndex >= 0 && pSkeletonAnimationEntry)
		{
			const char* skeletonAnimationName = (pSkeletonAnimationEntry ? pSkeletonAnimationEntry->GetName() : 0);
			if (skeletonAnimationName && *skeletonAnimationName)
			{
				CString extra;
				extra.Format(" (Skeleton Animation (%d/%d): %s)", skeletonAnimationToPlayIndex + 1, (pSequence ? pSequence->GetSkeletonAnimationEntryCount() : 0), skeletonAnimationName);
				title += extra;
			}
		}
		else
		{
			CString extra;
			extra.Format(" (%d Skeleton Animations)", (pSequence ? pSequence->GetSkeletonAnimationEntryCount() : 0));
			title += extra;
		}

		pPane->SetTitle(title);

		//GetDockingPaneManager()->RecalcFramesLayout();
		//GetDockingPaneManager()->RedrawPanes();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnClose()
{
	if (m_pContext->bLibraryModfied)
	{
		QDialogButtonBox::StandardButton res = QDialogButtonBox::StandardButton::No;

		res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Facial Expression library was modified.\r\nDo you want to save your changes?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return;
		}
		else if (res == QDialogButtonBox::StandardButton::Yes)
		{
			SaveCurrentLibrary();
		}
	}
	if (!CloseCurrentSequence())
		return;

	__super::OnClose();
}

//////////////////////////////////////////////////////////////////////////
LRESULT CFacialEditorDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (!pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_FE_PREVIEW_PANE:
				pwndDockWindow->Attach(&m_panePreview);
				break;
			case IDW_FE_SLIDERS_PANE:
				pwndDockWindow->Attach(&m_paneSliders);
				m_paneSliders.RedrawWindow(0, 0, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
				break;
			case IDW_FE_EXPRESSIONS_PANE:
				pwndDockWindow->Attach(&m_paneExpressions);
				break;
			case IDW_FE_SEQUENCE_PANE:
				pwndDockWindow->Attach(&m_paneSequence);
				break;
			case IDW_FE_PREVIEW_OPTIONS_PANE:
				pwndDockWindow->Attach(&m_panePreviewOptions);
				break;
			case IDW_FE_JOYSTICK_PANE:
				pwndDockWindow->Attach(&m_paneJoysticks);
				break;
			case IDW_FE_VIDEO_FRAME_PANE:
				pwndDockWindow->Attach(&m_paneVideoFrame);
				break;
			default:
				return FALSE;
			}
		}
		return TRUE;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_pDragImage)
	{
		CRect rc;
		GetWindowRect(rc);
		CPoint p = point;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;
		m_pDragImage->DragMove(p);
		return;
	}

	__super::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_pDragImage)
	{
		CPoint p;
		GetCursorPos(&p);

		m_pDragImage->DragLeave(this);
		m_pDragImage->EndDrag();
		delete m_pDragImage;
		m_pDragImage = 0;
		ReleaseCapture();

		CWnd* wnd = CWnd::WindowFromPoint(p);
		if (wnd == &m_view)
		{
			m_view.ScreenToClient(&p);
		}

		return;
	}
	__super::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialEditorDialog::OnToggleBar(UINT nID)
{
	CXTPDockingPane* pBar = GetDockingPaneManager()->FindPane(nID);
	if (pBar)
	{
		if (pBar->IsClosed())
			GetDockingPaneManager()->ShowPane(pBar);
		else
			GetDockingPaneManager()->ClosePane(pBar);
		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnUpdateControlBar(CCmdUI* pCmdUI)
{
	UINT nID = pCmdUI->m_nID;

	CXTPCommandBars* pCommandBars = GetCommandBars();
	if (pCommandBars != NULL)
	{
		CXTPToolBar* pBar = pCommandBars->GetToolBar(nID);
		if (pBar)
		{
			pCmdUI->SetCheck((pBar->IsVisible()) ? 1 : 0);
			return;
		}
	}
	CXTPDockingPane* pBar = GetDockingPaneManager()->FindPane(nID);
	if (pBar)
	{
		pCmdUI->SetCheck((!pBar->IsClosed()) ? 1 : 0);
		return;
	}
	pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	switch (event)
	{
	case EFD_EVENT_SPLINE_CHANGE:
	case EFD_EVENT_SPLINE_CHANGE_CURRENT:
	case EFD_EVENT_REDRAW_PREVIEW:
		if (m_panePreview)
		{
			m_panePreview.RedrawPreview();
		}
		break;
	case EFD_EVENT_SELECT_EFFECTOR:
		{
			CString title;
			if (m_pContext->pSelectedEffector)
				title = CString("Preview (Expression:") + m_pContext->pSelectedEffector->GetName() + ")";
			else
				title = _T("Preview");

			CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(IDW_FE_PREVIEW_PANE);
			if (pPane)
			{
				if (title != pPane->GetTitle())
					pPane->SetTitle(title);
			}
		}
		break;
	case EFD_EVENT_SLIDERS_CHANGE:
		{
			CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(IDW_FE_PREVIEW_PANE);
			if (pPane && pPane->GetTitle() != CString("Preview (Sliders)"))
				pPane->SetTitle("Preview (Sliders)");
		}
		break;
	case EFD_EVENT_LIBRARY_CHANGE:
		{
			CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(IDW_FE_EXPRESSIONS_PANE);
			CString title = _T("Expressions Explorer");
			if (pPane && m_pContext && m_pContext->pLibrary)
			{
				title += CString(" - ") + m_pContext->pLibrary->GetName();
				pPane->SetTitle(title);

				GetDockingPaneManager()->RecalcFramesLayout();
				GetDockingPaneManager()->RedrawPanes();
			}
		}
		break;
	case EFD_EVENT_SKELETON_ANIMATION_CHANGE:
	case EFD_EVENT_SEQUENCE_CHANGE:
	case EFD_EVENT_SEQUENCE_UNDO:
		{
			UpdateSkeletonAnimText();
		}
		break;
	case EFD_EVENT_JOYSTICK_SET_CHANGED:
		{
			CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(IDW_FE_JOYSTICK_PANE);
			CString title = _T("Joysticks");
			if (pPane && m_pContext && m_pContext->GetJoystickSet())
			{
				if (strlen(m_pContext->GetJoystickSet()->GetName()) > 0)
					title += CString(" - ") + m_pContext->GetJoystickSet()->GetName();
				else
					title += CString(" - ") + " New";
				pPane->SetTitle(title);

				GetDockingPaneManager()->RecalcFramesLayout();
				GetDockingPaneManager()->RedrawPanes();
			}
		}
		break;
	case EFD_EVENT_START_EDITTING_JOYSTICKS:
		{
			m_panePreview.GetViewport()->SetPaused(true);
		}
		break;
	case EFD_EVENT_STOP_EDITTING_JOYSTICKS:
		{
			m_panePreview.GetViewport()->SetPaused(false);
		}
		break;
	case EFD_EVENT_SEQUENCE_TIME:
		{
			UpdateSkeletonAnimText();
			DisplayCurrentVideoFrame();
		}
		break;
	case EFD_EVENT_ANIMATE_CAMERA_CHANGED:
		{
			m_panePreview.SetAnimateCamera(m_pContext ? m_pContext->GetAnimateCamera() : false);
		}
		break;
	}
	;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;

	m_paneSliders.SetContext(m_pContext);
	m_panePreview.SetContext(m_pContext);
	m_paneExpressions.SetContext(m_pContext);
	m_paneSequence.SetContext(m_pContext);
	m_paneJoysticks.SetContext(m_pContext);
	m_panePreviewOptions.SetContext(m_pContext);
	m_paneVideoFrame.SetContext(m_pContext);

	m_pContext->RegisterListener(this);
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEditorDialog::SaveCurrentProject(bool bSaveAs)
{
	SaveCurrentLibrary();
	SaveCurrentSequence(false);
	SaveCurrentJoysticks(false);

	if (!m_pContext->projectFile.IsEmpty() && !bSaveAs)
	{
		return m_pContext->SaveProject(m_pContext->projectFile);
	}
	else
	{
		// Select filename
		CString filename;
		if (CFileUtil::SelectSaveFile(FACEED_PROJECT_FILTER, FACEED_PROJECT_EXT, PathUtil::GetPathWithoutFilename(m_pContext->projectFile).c_str(), filename))
		{
			filename = PathUtil::ToUnixPath(filename.GetString()).c_str();
			return m_pContext->SaveProject(filename);
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEditorDialog::SaveCurrentLibrary(bool bSaveAs)
{

	if (!m_pContext->pLibrary)
		return true;

	if (m_pContext->bLibraryModfied || bSaveAs)
	{
		if (strlen(m_pContext->pLibrary->GetName()) > 0 && !bSaveAs)
		{
			return m_pContext->SaveLibrary(m_pContext->pLibrary->GetName());
		}
		else
		{
			// Select filename
			CString filename;
			if (CFileUtil::SelectSaveFile(FACEED_LIBRARY_FILTER, FACEED_LIBRARY_EXT, PathUtil::GetPathWithoutFilename(m_pContext->pLibrary->GetName()).c_str(), filename))
			{
				filename = PathUtil::MakeGamePath(filename.GetString()).c_str();
				return m_pContext->SaveLibrary(filename);
			}
		}
	}
	else
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::MakeNewProject()
{
	if (m_pContext)
		delete m_pContext;

	//////////////////////////////////////////////////////////////////////////
	m_pContext = new CFacialEdContext();
	m_pContext->NewLibrary();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnProjectNew()
{
	CString filename;
	if (CFileUtil::SelectSaveFile(FACEED_PROJECT_FILTER, FACEED_PROJECT_EXT, "", filename))
	{
		MakeNewProject();
		m_pContext->projectFile = PathUtil::ToUnixPath(filename.GetString()).c_str();
		m_pContext->bProjectModfied = true;
		SaveCurrentProject();
		SetContext(m_pContext);   // Reload context.
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnProjectOpen()
{
	if (m_pContext->bProjectModfied)
	{
		// As to save old project.

		QDialogButtonBox::StandardButton res = QDialogButtonBox::StandardButton::Cancel;

		res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Save Current Project?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return;
		}
		else if (res == QDialogButtonBox::StandardButton::Yes && !SaveCurrentProject())
		{
			return;
		}
	}

	CString filename;
	if (CFileUtil::SelectFile(FACEED_PROJECT_FILTER, "", filename))
	{
		m_pContext->LoadProject(filename);
		SetContext(m_pContext);   // Reload context.
		m_lastProject = filename;
		CString tmpTitle("");
		SetProjectTitle(tmpTitle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnProjectSave()
{
	SaveCurrentProject();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnProjectSaveAs()
{
	SaveCurrentProject(true);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLibraryNew()
{

	CString filename;
	if (CFileUtil::SelectSaveFile(FACEED_LIBRARY_FILTER, FACEED_LIBRARY_EXT, "", filename))
	{
		m_pContext->NewLibrary();
		m_pContext->pLibrary->SetName(PathUtil::MakeGamePath(filename.GetString()).c_str());
		m_pContext->bLibraryModfied = true;
		SaveCurrentLibrary();
		SetContext(m_pContext);   // Reload context.
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLibraryOpen()
{
	if (m_pContext->bLibraryModfied)
	{
		// As to save old project.
		QDialogButtonBox::StandardButton res = QDialogButtonBox::StandardButton::Cancel;

		res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Save Current Expressions Library?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return;
		}
		else if (res == QDialogButtonBox::StandardButton::Yes && !SaveCurrentLibrary())
		{
			return;
		}
	}

	CString dir("");
	CString filename("");
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, filename, FACEED_LIBRARY_FILTER, dir))
	{
		m_pContext->LoadLibrary(filename);
		SetContext(m_pContext);   // Reload context.
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLibrarySave()
{
	SaveCurrentLibrary();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLibrarySaveAs()
{
	SaveCurrentLibrary(true);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLibraryExport()
{

	CString filename;
	if (CFileUtil::SelectSaveFile(FACEED_LIBRARY_FILTER, FACEED_LIBRARY_EXT, PathUtil::GetPathWithoutFilename(m_pContext->pLibrary->GetName()).c_str(), filename))
	{
		filename = PathUtil::MakeGamePath(filename.GetString());
		_smart_ptr<IFacialEffectorsLibrary> pLibToExport = CreateLibraryOfSelectedEffectors();
		m_pContext->SaveLibrary(filename, pLibToExport);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLibraryImport()
{

	CString dir;
	if (m_pContext->pLibrary)
	{
		dir = PathUtil::GetPathWithoutFilename(m_pContext->pLibrary->GetName());
	}

	CString filename;
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, filename, FACEED_LIBRARY_FILTER, dir))
	{
		m_pContext->ImportLibrary(filename, this);
		SetContext(m_pContext);   // Reload context.
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLibraryBatchUpdateLibraries()
{

	if (!CloseCurrentLibrary())
		return;

	std::vector<CString> files;
	if (CFileUtil::SelectFiles(FACEED_LIBRARY_FILTER, "", files))
	{
		CString filename;
		_smart_ptr<IFacialEffectorsLibrary> pLibraryToExport = m_paneExpressions.CreateLibraryOfSelectedEffectors();

		m_pContext->SupressFacialEvents(true);
		CWaitProgress progress("", true);

		int numFiles = (int)files.size();
		for (int nFile = 0; nFile < numFiles; nFile++)
		{
			CString fxlFile = PathUtil::AbsolutePathToCryPakPath(files[nFile].GetString());

			DWORD fxlFileAttr = GetFileAttributes(fxlFile);

			if (fxlFileAttr == INVALID_FILE_ATTRIBUTES)
			{
				// Make new sequence.
				continue;
			}
			else
			{
				// fxl file exist.
				if (fxlFileAttr & FILE_ATTRIBUTE_READONLY)
				{
					// Skip read only .fxl files.
					// They need to be checked out from source control first.
					CryLogAlways("Skipping conversion, file %s is read-only", (const char*)fxlFile);
					continue;
				}
			}
			// Open existing one.
			if (!m_pContext->LoadLibrary(fxlFile))
			{
				CryLogAlways("Skipping conversion, failed to load facial expression from %s", (const char*)fxlFile);
				continue;
			}
			IFacialEffectorsLibrary* pLibrary = m_pContext->GetLibrary();
			if (!pLibrary)
				continue;

			progress.SetText(fxlFile);
			if (!progress.Step((nFile * 100) / numFiles))
				break;

			m_pContext->ImportLibrary(pLibraryToExport, this);

			{
				m_pContext->bLibraryModfied = true;
				m_pContext->SaveLibrary(PathUtil::MakeGamePath(fxlFile.GetString()).c_str());
			}
		}
		m_pContext->SupressFacialEvents(false);
	}

	// Reload previous sequence.
	m_pContext->LoadLibrary(m_lastLibrary);
	SetContext(m_pContext);   // Reload context.
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnLoadCharacter()
{
	if (m_pContext->bLibraryModfied)
	{
		// As to save old project.
		QDialogButtonBox::StandardButton res = QDialogButtonBox::StandardButton::Cancel;

		res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Save Current Expressions Library?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::Cancel);

		if (res == QDialogButtonBox::StandardButton::Cancel)
		{
			return;
		}
		else if (res == QDialogButtonBox::StandardButton::Yes && !SaveCurrentLibrary())
		{
			return;
		}
	}

	CString dir("");
	CString filename("");
	CString filter("");
	filter = FACEED_CHARACTER_FILES_FILTER;

	if (CFileUtil::SelectSingleFile(EFILE_TYPE_GEOMETRY, filename, filter, dir))
	{
		m_pContext->LoadCharacter(filename);
		SetContext(m_pContext);   // Reload context.
		m_lastCharacter = filename;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceNew()
{
	if (!CloseCurrentSequence())
		return;
	CString filename;
	if (CFileUtil::SelectSaveFile(FACEED_SEQUENCE_FILTER, FACEED_SEQUENCE_EXT, "", filename))
	{
		m_pContext->NewSequence();
		m_pContext->bSequenceModfied = true;
		m_pContext->SaveSequence(PathUtil::MakeGamePath(filename.GetString()).c_str());
		m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
		m_lastCharacter = filename;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceOpen()
{
	if (!CloseCurrentSequence())
		return;
	if (!CloseCurrentJoysticks())
		return;

	CString dir("");// = Path::GetPath(m_lastSequence);
	CString filename("");
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, filename, FACEED_SEQUENCE_FILTER, dir))
	{
		m_pContext->LoadSequence(filename);
	}

	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	const char* joystickFileName = (pSequence ? pSequence->GetJoystickFile() : 0);
	if (m_pContext && joystickFileName && joystickFileName[0])
		m_pContext->LoadJoystickSet(joystickFileName);
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEditorDialog::SaveCurrentSequence(bool bSaveAs)
{
	if (!m_pContext->GetSequence())
		return true;

	bool bResult = false;

	if (strlen(m_pContext->GetSequence()->GetName()) > 0 && !bSaveAs)
	{
		bResult = m_pContext->SaveSequence(m_pContext->GetSequence()->GetName());
		m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
	}
	else
	{
		// Select filename
		CString filename;
		if (CFileUtil::SelectSaveFile(FACEED_SEQUENCE_FILTER, FACEED_SEQUENCE_EXT, PathUtil::GetPathWithoutFilename(m_pContext->GetSequence()->GetName()).c_str(), filename))
		{
			filename = PathUtil::MakeGamePath(filename.GetString()).c_str();
			bResult = m_pContext->SaveSequence(filename);
			m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE);
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialEditorDialog::SaveCurrentJoysticks(bool bSaveAs)
{
	if (!m_pContext->GetJoystickSet())
		return true;

	bool bResult = false;

	if (m_pContext->bJoysticksModfied || bSaveAs)
	{
		if (strlen(m_pContext->GetJoystickSet()->GetName()) > 0 && !bSaveAs)
		{
			bResult = m_pContext->SaveJoystickSet(m_pContext->GetJoystickSet()->GetName());
			m_pContext->SendEvent(EFD_EVENT_JOYSTICK_SET_CHANGED);
		}
		else
		{
			// Select filename
			CString filename;
			if (CFileUtil::SelectSaveFile(FACEED_JOYSTICK_FILTER, FACEED_JOYSTICK_EXT, PathUtil::GetPathWithoutFilename(m_pContext->GetJoystickSet()->GetName()).c_str(), filename))
			{
				filename = PathUtil::MakeGamePath(filename.GetString()).c_str();
				bResult = m_pContext->SaveJoystickSet(filename);
				m_pContext->SendEvent(EFD_EVENT_JOYSTICK_SET_CHANGED);
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceSave()
{
	QDialogButtonBox::StandardButton res = QDialogButtonBox::StandardButton::Cancel;

	res = CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Save Current Expressions Library?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::Cancel);

	if (res == QDialogButtonBox::StandardButton::Yes)
	{
		SaveCurrentSequence();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceSaveAs()
{
	SaveCurrentSequence(true);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceLoadSound()
{

	CString filename;
	if (CFileUtil::SelectFile("Wav Files (*.wav)|*.wav", "", filename))
	{
		filename = PathUtil::MakeGamePath(filename.GetString()).c_str();
		m_paneSequence.LoadSequenceSound(filename.GetString());
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceLoadSkeletonAnimation()
{
	ICharacterInstance* pCharacter = (m_pContext ? m_pContext->pCharacter : 0);
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	if (pCharacter && pSequence)
	{
		CSelectAnimationDialog dlg;
		dlg.SetCharacterInstance(pCharacter);
		IFacialAnimSkeletonAnimationEntry* pEntry = (pSequence && pSequence->GetSkeletonAnimationEntryCount() ? pSequence->GetSkeletonAnimationEntry(0) : 0);
		const char* currentSkeletonAnimation = (pEntry ? pEntry->GetName() : 0);
		if (currentSkeletonAnimation && *currentSkeletonAnimation)
			dlg.PreSelectItem(currentSkeletonAnimation);
		if (IDOK == dlg.DoModal())
		{
			if (m_pContext)
				m_pContext->SetSkeletonAnimation(dlg.GetSelectedItem(), m_pContext->GetSequenceTime());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceLipSync()
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	for (int soundEntryIndex = 0, soundEntryCount = (pSequence ? pSequence->GetSoundEntryCount() : 0); soundEntryIndex < soundEntryCount; ++soundEntryIndex)
		m_paneSequence.LipSync(soundEntryIndex);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceExportSelectedExpressions()
{
	CString filename;
	if (CFileUtil::SelectSaveFile(FACEED_SEQUENCE_FILTER, FACEED_SEQUENCE_EXT, PathUtil::GetPathWithoutFilename(m_pContext->GetSequence()->GetName()).c_str(), filename))
	{
		filename = PathUtil::MakeGamePath(filename.GetString()).c_str();
		_smart_ptr<IFacialAnimSequence> pSequenceToExport = m_paneSequence.CreateSequenceWithSelectedExpressions();
		m_pContext->SaveSequence(filename, pSequenceToExport);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceImportExpressions()
{
	CString dir;
	if (m_pContext->pLibrary)
	{
		dir = PathUtil::GetPathWithoutFilename(m_pContext->pLibrary->GetName());
	}

	CString filename;
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, filename, FACEED_SEQUENCE_FILTER, dir))
	{
		m_pContext->ImportSequence(filename, this);
		SetContext(m_pContext);   // Reload context.
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceBatchUpdateSequences()
{
	if (!CloseCurrentSequence())
		return;

	std::vector<CString> files;
	if (CFileUtil::SelectFiles("Facial Sequence Files (*.fsq)|*.fsq", "", files))
	{
		CString filename;
		_smart_ptr<IFacialAnimSequence> pSequenceToExport = m_paneSequence.CreateSequenceWithSelectedExpressions();

		m_pContext->SupressFacialEvents(true);
		CWaitProgress progress("", true);

		int numFiles = (int)files.size();
		for (int nFile = 0; nFile < numFiles; nFile++)
		{
			CString fsqFile = PathUtil::AbsolutePathToCryPakPath(files[nFile].GetString());

			DWORD fsqFileAttr = GetFileAttributes(fsqFile);

			if (fsqFileAttr == INVALID_FILE_ATTRIBUTES)
			{
				// Make new sequence.
				continue;
			}
			else
			{
				// fsq file exist.
				if (fsqFileAttr & FILE_ATTRIBUTE_READONLY)
				{
					// Skip read only .fsq files.
					// They need to be checked out from source control first.
					CryLogAlways("Skipping conversion, file %s is read-only", (const char*)fsqFile);
					continue;
				}
			}
			// Open existing one.
			if (!m_pContext->LoadSequence(fsqFile))
			{
				CryLogAlways("Skipping conversion, failed to load facial expression from %s", (const char*)fsqFile);
				continue;
			}
			IFacialAnimSequence* pSequence = m_pContext->GetSequence();
			if (!pSequence)
				continue;

			progress.SetText(fsqFile);
			if (!progress.Step((nFile * 100) / numFiles))
				break;

			m_pContext->ImportSequence(pSequenceToExport, this);

			{
				m_pContext->bSequenceModfied = true;
				m_pContext->SaveSequence(PathUtil::MakeGamePath(fsqFile.GetString()).c_str());
			}
		}
		m_pContext->SupressFacialEvents(false);
	}
	// Reload previous sequence.
	m_pContext->LoadSequence(m_lastSequence);
	SetContext(m_pContext);   // Reload context.
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceLoadC3DFile()
{
	CString filename;
	if (CFileUtil::SelectFile("C3D Files (*.c3d)|*.c3d", "", filename))
		LoadC3DFile(filename.GetString());
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceLoadVideoExtractedSequence()
{

	CString filename;
	if (CFileUtil::SelectFile("Video Extracted Sequence Files (*.txt)|*.txt", "", filename))
		MergeVideoExtractedSequence(filename.GetString(), (m_pContext ? m_pContext->GetSequenceTime() : 0.0f), true);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceLoadVideoIgnoreSequence()
{

	CString filename;
	if (CFileUtil::SelectFile("Video Extracted Sequence Files (*.txt)|*.txt", "", filename))
		MergeVideoExtractedSequence(filename.GetString(), (m_pContext ? m_pContext->GetSequenceTime() : 0.0f), false);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSeqenceLoadGroupFile()
{

	CString filename;
	if (CFileUtil::SelectFile("Group File (*.grp)|*.grp", "", filename))
		LoadGroupFile(filename.GetString());
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSequenceImportFBX()
{
	CString filename;
	CString filter("");
	filter = "FBX File (*.fbx)|*.fbx";
	filter += "|";
	filter += "DAE File (*.dae)|*.dae";

	if (CFileUtil::SelectFile(filter, "", filename))
		LoadFBXToSequence(filename.GetString());
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnJoysticksNew()
{

	if (!CloseCurrentJoysticks())
		return;
	CString filename;
	if (CFileUtil::SelectSaveFile(FACEED_JOYSTICK_FILTER, FACEED_JOYSTICK_EXT, "", filename))
	{
		m_pContext->NewJoystickSet();
		m_pContext->bJoysticksModfied = true;
		m_pContext->SaveJoystickSet(PathUtil::MakeGamePath(filename.GetString()).c_str());
		m_pContext->SendEvent(EFD_EVENT_JOYSTICK_SET_CHANGED);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnJoysticksOpen()
{

	CString lastJoysticks;
	if (m_pContext->GetJoystickSet())
		lastJoysticks = m_pContext->GetJoystickSet()->GetName();

	if (!CloseCurrentJoysticks())
		return;

	CString dir("");
	CString filename("");
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, filename, FACEED_JOYSTICK_FILTER, dir))
		m_pContext->LoadJoystickSet(filename);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnJoysticksSave()
{
	SaveCurrentJoysticks();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnJoysticksSaveAs()
{
	SaveCurrentJoysticks(true);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnCreateExpressionFromCurrentPositions()
{
	IFacialEffector* parentEffector = (m_pContext ? m_pContext->pSelectedEffector : 0);
	IFacialEffectorsLibrary* library = (m_pContext ? m_pContext->GetLibrary() : 0);
	if (!parentEffector)
		parentEffector = (library ? library->GetRoot() : 0);

	CString sName;
	QStringDialog dlg("Expression Name");
	if (parentEffector && dlg.exec() == QDialog::Accepted)
		sName = dlg.GetString();

	IFacialEffector* effector = 0;
	if (sName.GetLength())
	{
		IFacialEffector* existingEffector = m_pContext->pLibrary->Find(sName);
		if (existingEffector)
		{
			CString str;
			str.Format(_T("Facial Expression with name %s already exist\r\nDo you want to use the existing one?"), (const char*)sName);

			if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(str)))
			{
				effector = existingEffector;
			}
		}
		else
		{
			effector = m_pContext->pLibrary->CreateEffector(EFE_TYPE_EXPRESSION, sName);
			if (!effector)
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Creation of Effector %s Failed", sName);
		}
	}

	// Create sub-effectors for each of the channels.
	float currentTime = (m_pContext ? m_pContext->GetSequenceTime() : 0.0f);
	IFacialAnimSequence* sequence = (m_pContext ? m_pContext->GetSequence() : 0);
	for (int channelIndex = 0; sequence && channelIndex < sequence->GetChannelCount(); ++channelIndex)
	{
		IFacialAnimChannel* channel = (sequence ? sequence->GetChannel(channelIndex) : 0);
		ISplineInterpolator* spline = (channel ? channel->GetLastInterpolator() : 0);
		float currentValue = 0.0f;
		if (spline)
			spline->InterpolateFloat(currentTime, currentValue);
		IFacialEffector* channelEffector = (library && channel ? library->Find(channel->GetEffectorName()) : 0);
		IFacialEffCtrl* subCtrl = 0;
		if (effector && channelEffector && fabsf(currentValue) > 0.01f)
			subCtrl = effector->AddSubEffector(channelEffector);
		if (subCtrl)
			subCtrl->SetConstantWeight(currentValue);
	}

	bool effectorAdded = false;
	if (parentEffector && effector)
	{
		parentEffector->AddSubEffector(effector);
		effectorAdded = true;
	}

	if (effectorAdded)
	{
		m_pContext->SendEvent(EFD_EVENT_ADD, effector);
		m_pContext->SetModified(effector);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnBatchProcess_PhonemeExtraction()
{

	if (!CloseCurrentSequence())
		return;

	// Ask the user whether he wants to use localization data to provide the text for phoneme extraction.
	bool getTextFromLocalizationData = false;

	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Do you want to search localization data for the text associated with each *.wav file?")))
	{
		getTextFromLocalizationData = true;
	}

	PhonemeExtractionSoundDatabase soundDB;
	if (getTextFromLocalizationData)
		CreatePhonemeExtractionSoundDatabase(soundDB);

	CString fullPath;
	if (CFileUtil::SelectFile("Wav Files (*.wav)|*.wav", "", fullPath))
	{
		m_pContext->SupressFacialEvents(true);
		CWaitProgress progress("Scanning Directory...", true);

		string searchPath = PathUtil::GetPathWithoutFilename(fullPath);
		CFileUtil::FileArray files;
		CFileUtil::ScanDirectory(searchPath, "*.wav", files, true);
		int numFiles = (int)files.size();
		for (int i = 0; i < numFiles; i++)
		{
			CString wavFile = PathUtil::Make(searchPath, files[i].filename);
			CString fsqFile = PathUtil::ReplaceExtension(wavFile.GetString(), "fsq");

			DWORD fsqFileAttr = GetFileAttributes(fsqFile);

			if (fsqFileAttr == INVALID_FILE_ATTRIBUTES)
			{
				// Make new sequence.
				m_pContext->NewSequence();
			}
			else
			{
				// fsq file exist.
				if (fsqFileAttr & FILE_ATTRIBUTE_READONLY)
				{
					// Skip read only .fsq files.
					// They need to be checked out from source control first.
					continue;
				}

				// Open existing one.
				m_pContext->LoadSequence(fsqFile);
			}
			m_pContext->bSequenceModfied = false;
			IFacialAnimSequence* pSequence = m_pContext->GetSequence();
			if (!pSequence)
				continue;

			progress.SetText(files[i].filename);
			if (!progress.Step((i * 100) / numFiles))
				break;

			int soundEntryIndex = pSequence->GetSoundEntryCount();
			pSequence->InsertSoundEntry(soundEntryIndex);
			pSequence->GetSoundEntry(soundEntryIndex)->SetSoundFile(PathUtil::MakeGamePath(wavFile.GetString()));
			pSequence->SetFlags(pSequence->GetFlags() | IFacialAnimSequence::FLAG_RANGE_FROM_SOUND);

			// Search the database for text that corresponds with this sound file.
			const char* text = "";
			{
				PhonemeExtractionSoundDatabase::iterator entry = soundDB.find(PathUtil::AbsolutePathToGamePath(wavFile.GetString()));
				if (entry != soundDB.end())
					text = (*entry).second.sOriginalActorLine; // TODO verify if this should be "swTranslatedActorLine" instead
			}

			if (m_paneSequence.DoPhonemeExtraction(PathUtil::MakeGamePath(wavFile.GetString()).c_str(), MakeSafeText(text), pSequence, soundEntryIndex))
			{
				m_pContext->bSequenceModfied = true;
				m_pContext->SaveSequence(PathUtil::MakeGamePath(fsqFile.GetString()).c_str());
			}
		}
		m_pContext->SupressFacialEvents(false);
	}
	// Reload previous sequence.
	m_pContext->LoadSequence(m_lastSequence);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::CreatePhonemeExtractionSoundDatabase(PhonemeExtractionSoundDatabase& db)
{
	// Create an unsorted vector of all the entries.
	PhonemeExtractionSoundDatabase::container_type entries;

	// Loop through the localized strings, adding them to the map.
	ISystem* system = GetISystem();
	ILocalizationManager* localization = (system ? system->GetLocalizationManager() : 0);
	int stringCount = (localization ? localization->GetLocalizedStringCount() : 0);
	entries.reserve(stringCount);
	for (int stringIndex = 0; stringIndex < stringCount; ++stringIndex)
	{
		SLocalizedInfoEditor info;
		bool success = true;

		if (localization)
			success = success && localization->GetLocalizedInfoByIndex(stringIndex, info);

		if (success && info.sUtf8TranslatedActorLine && info.sOriginalActorLine)
		{
			PhonemeExtractionSoundEntry entry = { info.sOriginalActorLine, info.sUtf8TranslatedActorLine };
			string waveFile = string(PathUtil::GetAudioLocalizationFolder().c_str()) + info.sKey + ".wav";
			entries.push_back(std::make_pair(waveFile, entry));
		}
	}

	// Swap the items into the map, and sort them for O(log(n)) access.
	db.SwapElementsWithVector(entries);
}

//////////////////////////////////////////////////////////////////////////
CString CFacialEditorDialog::MakeSafeText(const string& text)
{
	// Allocate space for the wchar string.
	std::vector<wchar_t> bufferVector;
	bufferVector.resize(text.size() + 1);
	wchar_t* buffer = &bufferVector[0];
	std::vector<char> outputBufferVector;
	int outputBufferLength = text.size() * 3 + 1;
	outputBufferVector.resize(outputBufferLength);
	char* outputBuffer = &outputBufferVector[0];

	// Interpret any escape sequences in the text.
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer, bufferVector.size());
	WideCharToMultiByte(CP_ACP, 0, buffer, -1, outputBuffer, outputBufferLength, 0, 0);

	return outputBuffer;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnBatchProcess_ApplyExpression()
{

	if (!CloseCurrentSequence())
		return;

	std::vector<CString> files;
	if (CFileUtil::SelectFiles("Facial Sequence Files (*.fsq)|*.fsq", "", files))
	{
		m_pContext->SupressFacialEvents(true);
		CWaitProgress progress("", true);

		QStringDialog dlg("Apply Auto Facial Expression");
		if (dlg.exec() != QDialog::Accepted)
		{
			return;
		}
		CString expression = dlg.GetString();
		if (expression.IsEmpty())
			return;

		QNumericBoxDialog numDlg(QObject::tr("Enter Expression Weight"), 1);
		if (numDlg.exec() != QDialog::Accepted)
		{
			return;
		}
		float fExpressionWeight = numDlg.GetValue();

		int numFiles = (int)files.size();
		for (int nFile = 0; nFile < numFiles; nFile++)
		{
			CString fsqFile = PathUtil::AbsolutePathToCryPakPath(files[nFile].GetString());

			DWORD fsqFileAttr = GetFileAttributes(fsqFile);

			if (fsqFileAttr == INVALID_FILE_ATTRIBUTES)
			{
				// Make new sequence.
				continue;
			}
			else
			{
				// fsq file exist.
				if (fsqFileAttr & FILE_ATTRIBUTE_READONLY)
				{
					// Skip read only .fsq files.
					// They need to be checked out from source control first.
					CryLogAlways("Skipping conversion, file %s is read-only", (const char*)fsqFile);
					continue;
				}
			}
			// Open existing one.
			if (!m_pContext->LoadSequence(fsqFile))
			{
				CryLogAlways("Skipping conversion, failed to load facial expression from %s", (const char*)fsqFile);
				continue;
			}
			IFacialAnimSequence* pSequence = m_pContext->GetSequence();
			if (!pSequence)
				continue;

			progress.SetText(fsqFile);
			if (!progress.Step((nFile * 100) / numFiles))
				break;

			IFacialAnimChannel* pAutoExp = 0;
			// Find auto expression folder.
			{
				for (int i = 0; i < pSequence->GetChannelCount(); i++)
				{
					if (stricmp(pSequence->GetChannel(i)->GetName(), "AutoExpression") == 0)
					{
						pAutoExp = pSequence->GetChannel(i);
						break;
					}
				}
			}
			if (pAutoExp)
			{
				// Delete old auto expression.
				for (int i = 0; i < pSequence->GetChannelCount(); i++)
				{
					if (pSequence->GetChannel(i)->GetParent() == pAutoExp)
					{
						pSequence->RemoveChannel(pSequence->GetChannel(i));
						break;
					}
				}
			}
			else
			{
				// Make group.
				pAutoExp = pSequence->CreateChannelGroup();
				pAutoExp->SetName("AutoExpression");
			}

			// Add auto expression.
			IFacialAnimChannel* pChannel = pSequence->CreateChannel();
			pChannel->SetName(expression);
			pChannel->SetParent(pAutoExp);
			while (pChannel->GetLastInterpolator()->GetKeyCount() > 0) // remove all old keys.
				pChannel->GetLastInterpolator()->RemoveKey(0);
			pChannel->GetLastInterpolator()->InsertKeyFloat(FacialEditorSnapTimeToFrame(0), fExpressionWeight);

			{
				m_pContext->bSequenceModfied = true;
				m_pContext->SaveSequence(PathUtil::MakeGamePath(fsqFile.GetString()).c_str());
			}
		}
		m_pContext->SupressFacialEvents(false);
	}
	// Reload previous sequence.
	m_pContext->LoadSequence(m_lastSequence);
}

//////////////////////////////////////////////////////////////////////////
class MorphCheckHandler
{
public:
	MorphCheckHandler(CReport& report) : m_report(report) {}

	void operator()(const char* morphName)
	{
		CReportRecord<MorphCheckError>* record = m_report.AddRecord(MorphCheckError(morphName));
		record->AddField("Morph Name", std::mem_fun_ref(&MorphCheckError::GetMorphName));
	}

private:
	class MorphCheckError
	{
	public:
		MorphCheckError(const char* morphName) : m_morphName(morphName) {}
		string GetMorphName() const { return m_morphName; }

	private:
		string m_morphName;
	};

	CReport& m_report;
};
void CFacialEditorDialog::OnMorphCheck()
{
	m_morphCheckReport.Clear();

	if (m_pContext)
		m_pContext->CheckMorphs(MorphCheckHandler(m_morphCheckReport));

	m_morphCheckReportDialog.Open(&m_morphCheckReport);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnStartStop()
{
	m_pContext->PlaySequence(!m_pContext->IsPlayingSequence());
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnNextKey()
{
	if (m_pContext)
		m_pContext->MoveToKey(CFacialEdContext::MoveToKeyDirectionForward);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnPrevKey()
{
	if (m_pContext)
		m_pContext->MoveToKey(CFacialEdContext::MoveToKeyDirectionBackward);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnNextFrame()
{
	if (m_pContext)
		m_pContext->MoveToFrame(CFacialEdContext::MoveToKeyDirectionForward);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnPrevFrame()
{
	if (m_pContext)
		m_pContext->MoveToFrame(CFacialEdContext::MoveToKeyDirectionBackward);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnKeyAll()
{
	m_paneSequence.KeyAllSplines();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnSelectAll()
{
	m_paneSequence.SelectAllKeys();
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialEditorDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CFacialEdContext::MoveToKeyDirection direction = (zDelta < 0 ? CFacialEdContext::MoveToKeyDirectionBackward : CFacialEdContext::MoveToKeyDirectionForward);
	if (m_pContext)
		m_pContext->MoveToFrame(direction);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::DisplayCurrentVideoFrame()
{
	float fCurrTime = m_pContext->GetSequenceTime();

	int w = m_pContext->GetVideoFrameReader().GetWidth(fCurrTime);
	int h = m_pContext->GetVideoFrameReader().GetHeight(fCurrTime);
	const uint8* pImg = m_pContext->GetVideoFrameReader().GetFrame(fCurrTime);
	if (pImg)
		SetVideoFrameResolution(w, h, 24);

	if (pImg)
	{
		uint8* pMem = (uint8*)GetVideoFrameBits();
		int nBpp = 3;

		// refresh. flip and invert colors from AVI frames
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				pMem[(y * w + x) * 3 + 0] = pImg[((h - y - 1) * w + x) * nBpp + 0];
				pMem[(y * w + x) * 3 + 1] = pImg[((h - y - 1) * w + x) * nBpp + 1];
				pMem[(y * w + x) * 3 + 2] = pImg[((h - y - 1) * w + x) * nBpp + 2];
			}
		}

		int nFrame = m_pContext->GetVideoFrameReader().GetLastLoadedAVIFrameFromTime(fCurrTime);
		if (nFrame >= 0 && nFrame < m_pContext->m_lstPosDebug.size())
		{
			tMarkerDebugInfo* pPos = &m_pContext->m_lstPosDebug[nFrame];
			for (int k = 0; k < m_pContext->m_nMarkersDebug; k++)
			{
				int x = (int)(pPos->fX[k] + 0.5f);
				int y = (int)(pPos->fY[k] + 0.5f);

				for (int y1 = y - 3; y1 < y + 3; y1++)
				{
					if (y1 < 0 || y1 >= h)
						continue;
					for (int x1 = x - 3; x1 < x + 3; x1++)
					{
						if (x1 < 0 || x1 >= w)
							continue;

						pMem[(y1 * w + x1) * 3 + 0] = 0;
						pMem[(y1 * w + x1) * 3 + 1] = 255;
						pMem[(y1 * w + x1) * 3 + 2] = 0;
					}
				}
			} //k
		}
	}

	m_paneVideoFrame.RedrawWindow(0, 0, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_NOERASE);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnCustomize()
{
	CMFCUtils::ShowShortcutsCustomizeDlg(GetCommandBars(), IDR_FACED_MENU, "FacialEditor");
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnExportShortcuts()
{
	CMFCUtils::ExportShortcuts(GetCommandBars()->GetShortcutManager());
}

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::OnImportShortcuts()
{
	CMFCUtils::ImportShortcuts(GetCommandBars()->GetShortcutManager(), "FacialEditor");
}

