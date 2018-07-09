// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2014.
// -------------------------------------------------------------------------
//  File name: QtMain.h
//  Created:   26/09/2014 by timur
//  Description: QT Main entry point
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include <processenv.h>
#include "QMfcApp/qmfcapp.h"

#include "CryEdit.h"

//////////////////////////////////////////////////////////////////////////
#include <QWidget>
#include <QStyleFactory>
#include <QFile>
#include <QDir>
#include <QMainWindow>
#include <QLabel>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QMenuBar>
//////////////////////////////////////////////////////////////////////////

#include "Util/BoostPythonHelpers.h"
#include "Serialization/PropertyTree/PropertyTreeStyle.h"
#include "Serialization/PropertyTree/PropertyTree.h"
#include "QtMainFrame.h"
#include "SplashScreen.h"
#include "EditorStyleHelper.h"

#include "CryIcon.h"

#include "EditorStyleHelper.h"

#include "Controls/QuestionTimeoutDialog.h"
#include "Material/StandaloneMaterialEditor.h"
#include "SessionData.h"

#include <CryCore/Platform/CryLibrary.h>

static const char* styleSheetPath = "Editor/Styles/stylesheet.qss";

// Advise notebook graphics drivers to prefer discrete GPU when no explicit application profile exists
extern "C"
{
	// nVidia
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	// AMD
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

extern CCryEditApp theApp;

CWinApp mfcApp;

static void QtLogToDebug(QtMsgType Type, const QMessageLogContext& Context, const QString& message)
{
	OutputDebugStringW(L"Qt: ");
	OutputDebugStringW(reinterpret_cast<const wchar_t*>(message.utf16()));
	OutputDebugStringW(L"\n");
}

void EnableXTDarkSkin()
{
	// Enable dark theme in XT Toolikit
	{
		XTPOffice2007Images()->SetHandle(PathUtil::Make(PathUtil::GetEnginePath(), "Editor\\Styles\\Office2007Black.dll").c_str());

		//CXTPPaintManager::SetCustomTheme(new CCrytekTheme());
		BOOL bLoaded = XTPSkinManager()->LoadSkin(PathUtil::Make(PathUtil::GetEnginePath(), "Editor\\Styles\\CryDark.cjstyles").c_str());
		// Apply skin, including metrics, coloring and frame
		XTPSkinManager()->SetApplyOptions(xtpSkinApplyMetrics | xtpSkinApplyColors | xtpSkinApplyMenus);

		// Apply skin to all new windows and threads
		XTPSkinManager()->SetAutoApplyNewWindows(TRUE);
		XTPSkinManager()->SetAutoApplyNewThreads(TRUE);

		// Apply skin to current thread and window
		XTPSkinManager()->EnableCurrentThread();
		//XTPSkinManager()->ApplyWindow(this->GetSafeHwnd());

		//bool test = XTPSkinManager()->IsColorFilterExists();
		// Redraw everything

		//m_bSkinLoaded = true;
	}
}

void LoadStyleSheet()
{
	QFile styleSheetFile(PathUtil::Make(PathUtil::GetEnginePath(), styleSheetPath).c_str());

	if (styleSheetFile.open(QFile::ReadOnly))
	{
		qApp->setStyleSheet(styleSheetFile.readAll());
		qApp->setPalette(GetStyleHelper()->palette());
	}
}

void UpdateStyleSheet(const QString& path)
{
	if (path != PathUtil::Make(PathUtil::GetEnginePath(), styleSheetPath).c_str())
		return;

	LoadStyleSheet();
}

void SetVisualStyle()
{
	//On windows, available styles (QStyleFactory::keys()) are :
	/*
	   Windows
	   WindowsXP
	   WindowsVista
	   Fusion
	 */
	qApp->setStyle(QStyleFactory::create("Fusion"));

	LoadStyleSheet();

	PropertyTreeStyle treeStyle;
	treeStyle.packCheckboxes = false;
	treeStyle.horizontalLines = false;
	treeStyle.firstLevelIndent = 0.6f;
	treeStyle.scrollbarIndent = 0.0f;
	treeStyle.propertySplitter = true;
	treeStyle.doNotIndentSecondLevel = true;
	treeStyle.sliderSaturation = -1.85f;
	treeStyle.alignLabelsToRight = false;
	treeStyle.selectionRectangle = false;
	treeStyle.showSliderCursor = false;
	treeStyle.groupRectangle = true;

	PropertyTree::setDefaultTreeStyle(treeStyle);

	EnableXTDarkSkin();

	CryIcon::SetDefaultTint(QIcon::Normal, GetStyleHelper()->iconTint());
	CryIcon::SetDefaultTint(QIcon::Disabled, GetStyleHelper()->disabledIconTint());
	CryIcon::SetDefaultTint(QIcon::Active, GetStyleHelper()->activeIconTint());
	CryIcon::SetDefaultTint(QIcon::Selected, GetStyleHelper()->iconTint());
}

class CWaitingForDebuggerDialog : public CQuestionTimeoutDialog
{
public:
	CWaitingForDebuggerDialog()
	{
		setWindowIcon(CryIcon("icons:editor_icon.ico"));
		SetupWarning(QObject::tr("Waiting for debugger"), QCoreApplication::applicationName());
	}
};

int main(int argc, char* argv[])
{
#ifdef _DEBUG
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif

	QCoreApplication::setOrganizationName("Crytek");
	QCoreApplication::setOrganizationDomain("crytek.com");
	QCoreApplication::setApplicationName("CRYENGINE");//Not using Sandbox as this will be the name of the folder in AppData
	QSettings::setDefaultFormat(QSettings::IniFormat);

	// initialize MFC
	if (!AfxWinInit(CryGetCurrentModule(), NULL, GetCommandLineA(), 0))
	{
		return 1;
	}

	bool bDevMode = false;
	int waitForSeconds = 0; // Time in seconds that editor waits on startup.
	// Qt initialization
	{
		QStringList arglist;
		arglist.reserve(argc);
		for (int i = 0; i < argc; i++)
			arglist.append(QString::fromLocal8Bit(argv[i]));

		//Handles sleeping at the beginning of execution to wait for debugger, parameter is integer in seconds
		int waitIndex = arglist.indexOf("-waitfordebugger");
		if (waitIndex >= 0 && arglist.size() > ++waitIndex)
		{
			waitForSeconds = max(0, arglist[waitIndex].toInt());
		}

		int dpiIndex = arglist.indexOf("-nodpiscaling");
		if (dpiIndex == -1)
		{
			QMfcApp::setAttribute(Qt::AA_EnableHighDpiScaling);
		}
	}

	QDir::setSearchPaths("icons", QStringList(":/icons"));

	QMfcApp::mfc_argc = argc;
	QMfcApp::mfc_argv = argv;
	QMfcApp qMfcApp(&mfcApp, QMfcApp::mfc_argc, QMfcApp::mfc_argv);

	// Make sure to load stylesheets before creating the mainframe. If not, any Qt widgets created before the mainframe
	// might be created with erroneous style
	SetVisualStyle();

	if (waitForSeconds > 0)
	{
		CWaitingForDebuggerDialog timeoutDialog;
		timeoutDialog.Execute(waitForSeconds);
	}

	QPixmap pixmap(":/splash.bmp");
	SplashScreen splash(pixmap);
	splash.show();
	qApp->processEvents();

	qInstallMessageHandler(QtLogToDebug);

	// MFC initialization
	mfcApp.InitApplication();
	mfcApp.InitInstance();

	if (!theApp.InitInstance())
	{
		theApp.ExitInstance();
		AfxWinTerm();
		return 0;
	}

	LOADING_TIME_PROFILE_SECTION_NAMED("Sanbox::main() after initting app instance");

	WriteSessionData();

	CEditorMainFrame* mainFrame = new CEditorMainFrame(nullptr);

	// We must to [re]load style sheet after creating the main form to display the logo on the title bar.
	// Please see: #1350577. !XB (EditorQt) Restore logo by creating main frame before loading stylesheet.
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("SetVisualStyle()");
		SetVisualStyle();
	}

	QFileSystemWatcher* watcher = new QFileSystemWatcher();
	if (!watcher->addPath(PathUtil::Make(PathUtil::GetEnginePath(), styleSheetPath).c_str()))
	{
		delete watcher;
		watcher = nullptr;
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to add qss stylesheet to watcher");
	}
	else
	{
		QObject::connect(watcher, &QFileSystemWatcher::fileChanged, UpdateStyleSheet);
	}

	GetIEditorImpl()->EnableDevMode(bDevMode);

	mainFrame->PostLoad();

	splash.close();
	
	if (GetIEditorImpl()->IsInMatEditMode())//special mode where we only show a material editor. Currently handled here for code clarity
	{
		StandaloneMaterialEditor* standaloneMatEdit = new StandaloneMaterialEditor();
		standaloneMatEdit->Execute();

		mainFrame->hide();
	}

	theApp.PostInit();

	QMfcApp::run(&mfcApp);

	theApp.ExitInstance();

	if (watcher)
		delete watcher;

	if (CEditorMainFrame::m_pInstance)
		delete mainFrame;

	AfxWinTerm();

	return 0;
}

