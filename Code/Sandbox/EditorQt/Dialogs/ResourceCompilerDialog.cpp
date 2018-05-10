// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ResourceCompilerDialog.h"
#include "Controls/QuestionDialog.h"

BEGIN_MESSAGE_MAP(CResourceCompilerDialog, CXTResizeDialog)
ON_COMMAND(IDC_CLOSE, OnClose)
ON_COMMAND(IDC_SHOWLOG, OnShowLog)
END_MESSAGE_MAP()

CResourceCompilerDialog::CResourceCompilerDialog(const CString& fileName, const CString& additionalSettings,
                                                 std::function<void(const CResourceCompilerHelper::ERcCallResult)> finishedCallback) : CXTResizeDialog(IDD_RCPROGRESS),
	m_fileName(fileName), m_additionalSettings(additionalSettings), m_bRcCanceled(false), m_bRcFinished(false),
	m_finishedCallback(finishedCallback)
{
}

CResourceCompilerDialog::~CResourceCompilerDialog()
{
	m_bRcCanceled = true;
	gEnv->pThreadManager->JoinThread(this, eJM_Join);
}

BOOL CResourceCompilerDialog::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	if (m_outputFont.CreateFont(
	      14,                       // nHeight
	      0,                        // nWidth
	      0,                        // nEscapement
	      0,                        // nOrientation
	      FW_NORMAL,                // nWeight
	      FALSE,                    // bItalic
	      FALSE,                    // bUnderline
	      0,                        // cStrikeOut
	      ANSI_CHARSET,             // nCharSet
	      OUT_DEFAULT_PRECIS,       // nOutPrecision
	      CLIP_DEFAULT_PRECIS,      // nClipPrecision
	      DEFAULT_QUALITY,          // nQuality
	      DEFAULT_PITCH | FF_SWISS, // nPitchAndFamily
	      "Consolas") != 0)
	{
		m_outputCtrl.SetFont(&m_outputFont, true);
	}
	else if (m_outputFont.CreateFont(
	           14,                       // nHeight
	           0,                        // nWidth
	           0,                        // nEscapement
	           0,                        // nOrientation
	           FW_NORMAL,                // nWeight
	           FALSE,                    // bItalic
	           FALSE,                    // bUnderline
	           0,                        // cStrikeOut
	           ANSI_CHARSET,             // nCharSet
	           OUT_DEFAULT_PRECIS,       // nOutPrecision
	           CLIP_DEFAULT_PRECIS,      // nClipPrecision
	           DEFAULT_QUALITY,          // nQuality
	           DEFAULT_PITCH | FF_SWISS, // nPitchAndFamily
	           "Courier New") != 0)
	{
		m_outputCtrl.SetFont(&m_outputFont, true);
	}

	SetResize(IDC_PROGRESS, CXTResizeRect(0, 0, 1, 0));
	SetResize(IDC_RICHEDIT, CXTResizeRect(0, 0, 1, 1));
	SetResize(IDC_SHOWLOG, CXTResizeRect(0, 1, 0, 1));
	SetResize(IDC_CLOSE, CXTResizeRect(1, 1, 1, 1));

	if (!gEnv->pThreadManager->SpawnThread(this, "ResourceCompilerDialog"))
	{
		CryFatalError("Error spawning \"ResourceCompilerDialog\" thread.");
	}

	return TRUE;
}

void CResourceCompilerDialog::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX, IDC_PROGRESS, m_progressCtrl);
	DDX_Control(pDX, IDC_RICHEDIT, m_outputCtrl);
	DDX_Control(pDX, IDC_CLOSE, m_closeButton);
}

void CResourceCompilerDialog::ThreadEntry()
{
	m_rcCallResult = m_rcHelper.CallResourceCompiler(m_fileName, m_additionalSettings, this, true, CResourceCompilerHelper::eRcExePath_editor);

	if (!m_bRcCanceled)
	{
		m_progressCtrl.SetPos(100);
		m_bRcFinished = true;

		m_finishedCallback(m_rcCallResult);
	}
}

void CResourceCompilerDialog::OnRCMessage(MessageSeverity severity, const char* pText)
{
	if (severity == IResourceCompilerListener::MessageSeverity_Info)
	{
		AppendToOutput(pText, true, RGB(0, 0, 0));

		if (strstr(pText, "%") != nullptr)
		{
			m_progressCtrl.SetPos(atoi(pText));
		}
	}
	else if (severity == IResourceCompilerListener::MessageSeverity_Warning)
	{
		AppendToOutput(pText, false, RGB(221, 72, 0));
	}
	else if (severity == IResourceCompilerListener::MessageSeverity_Error)
	{
		AppendToOutput(pText, false, RGB(150, 0, 0));
	}
}

void CResourceCompilerDialog::AppendToOutput(CString text, bool bUseDefaultColor, COLORREF color)
{
	text += "\r\n";

	CHARFORMAT charFormat;
	// Initialize character format structure
	charFormat.cbSize = sizeof(CHARFORMAT);
	charFormat.dwMask = CFM_COLOR;
	charFormat.dwEffects = bUseDefaultColor ? CFE_AUTOCOLOR : 0;
	charFormat.crTextColor = color;

	// Get the initial text length.
	const int length = m_outputCtrl.GetWindowTextLength();
	// Put the selection at the end of text.
	m_outputCtrl.SetSel(length, -1);

	//  Set the character format
	m_outputCtrl.SetSelectionCharFormat(charFormat);

	// Replace the selection.
	m_outputCtrl.ReplaceSel(text);

	const int visibleLines = GetNumVisibleLines();
	if (&m_outputCtrl != m_outputCtrl.GetFocus())
	{
		m_outputCtrl.LineScroll(INT_MAX);
		m_outputCtrl.LineScroll(1 - visibleLines);
	}
}

int CResourceCompilerDialog::GetNumVisibleLines()
{
	CRect rect;
	long firstChar, lastChar;
	long firstLine, lastLine;

	// Get client rect of rich edit control
	m_outputCtrl.GetClientRect(rect);

	// Get character index close to upper left corner
	firstChar = m_outputCtrl.CharFromPos(CPoint(0, 0));

	// Get character index close to lower right corner
	lastChar = m_outputCtrl.CharFromPos(CPoint(rect.right, rect.bottom));
	if (lastChar < 0)
	{
		lastChar = m_outputCtrl.GetTextLength();
	}

	// Convert to lines
	firstLine = m_outputCtrl.LineFromChar(firstChar);
	lastLine = m_outputCtrl.LineFromChar(lastChar);

	return (lastLine - firstLine);
}

void CResourceCompilerDialog::OnClose()
{

	if (!m_bRcFinished &&
	    QDialogButtonBox::StandardButton::Ok == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Do you want to cancel the computation?"), QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel))
	{
		m_bRcCanceled = true;
		m_rcHelper.TerminateCalledResourceCompiler();
		m_finishedCallback(CResourceCompilerHelper::eRcCallResult_crash);
		EndDialog(IDCANCEL);
	}
	else
	{
		EndDialog(IDOK);
	}
}

void CResourceCompilerDialog::OnShowLog()
{
	char workingDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, workingDirectory);

	CString path(workingDirectory);
	path.Append("/Bin64/rc/rc_log.log");

	// Explorer command lines needs windows style paths
	path.Replace('/', '\\');

	const CString parameters = "/select," + path;
	ShellExecute(NULL, "Open", "explorer.exe", parameters, "", SW_NORMAL);
}

