// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CrySystem/File/CryFile.h>
#include "PerforceSourceControl.h"
#include "PasswordDlg.h"
#include <CryCore/Platform/platform_impl.inl>
#include <CryThreading/IThreadManager.h>

// EditorCommon
#include <EditorFramework/TrayArea.h>
#include <CryIcon.h>
#include <Controls/QPopupWidget.h>

// Qt
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QtUtil.h>

extern CPerforceSourceControl* g_pPerforceControl;

REGISTER_TRAY_AREA_WIDGET(CPerforceTrayWidget, 100)

class PerforceSettings : public QWidget
{
public:
	PerforceSettings(QWidget* pParent = nullptr)
	{
		QHBoxLayout* pMainLayout = new QHBoxLayout();

		m_pLabelLayout = new QVBoxLayout();
		m_pInputLayout = new QVBoxLayout();
		pMainLayout->addLayout(m_pLabelLayout);
		pMainLayout->addLayout(m_pInputLayout);

		m_pLabelLayout->addWidget(new QLabel("Online"));
		m_pConnectionCheckBox = new QCheckBox();
		m_pConnectionCheckBox->setChecked(!g_pPerforceControl->IsWorkOffline());
		connect(m_pConnectionCheckBox, &QCheckBox::toggled, [this](bool bChecked)
		{
			g_pPerforceControl->SetWorkOffline(!bChecked);
			m_pServerInput->setEnabled(bChecked);
			m_pUserInput->setEnabled(bChecked);
			m_pWorkspaceInput->setEnabled(bChecked);
			m_pPasswordInput->setEnabled(bChecked);

		});
		m_pInputLayout->addWidget(m_pConnectionCheckBox);

		CMyClientApi& clientApi = g_pPerforceControl->GetClientApi();
		m_pServerInput = CreateSection("Server", clientApi.GetPort().Value());
		m_pUserInput = CreateSection("User", clientApi.GetUser().Value());
		m_pWorkspaceInput = CreateSection("Workspace", clientApi.GetClient().Value());
		m_pPasswordInput = CreateSection("Password", clientApi.GetPassword().Value(), true);

		g_pPerforceControl->signalWorkOfflineChanged.Connect(this, &PerforceSettings::OnConnectionStatusChanged);

		setLayout(pMainLayout);
	}

private:
	QLineEdit* CreateSection(const char* szLabel, const char* szInput, bool bIsPassword = false)
	{
		m_pLabelLayout->addWidget(new QLabel(szLabel));
		QLineEdit* pLineEdit = new QLineEdit();
		if (bIsPassword)
			pLineEdit->setEchoMode(QLineEdit::Password);
		pLineEdit->setText(szInput);
		connect(pLineEdit, &QLineEdit::textChanged, this, &PerforceSettings::OnSettingsChanged);
		m_pInputLayout->addWidget(pLineEdit);

		return pLineEdit;
	}

	void OnConnectionStatusChanged()
	{
		m_pConnectionCheckBox->setChecked(!g_pPerforceControl->IsWorkOffline());
	}

	void OnSettingsChanged() const
	{
		CMyClientApi& clientApi = g_pPerforceControl->GetClientApi();
		Error e;
		clientApi.DefinePassword(m_pPasswordInput->text().toLocal8Bit(), &e);
		clientApi.DefineClient(m_pWorkspaceInput->text().toLocal8Bit(), &e);
		clientApi.DefinePort(m_pServerInput->text().toLocal8Bit(), &e);
		clientApi.DefineUser(m_pUserInput->text().toLocal8Bit(), &e);
	}

private:
	QVBoxLayout* m_pLabelLayout;
	QVBoxLayout* m_pInputLayout;

	QCheckBox* m_pConnectionCheckBox;
	QLineEdit* m_pServerInput;
	QLineEdit* m_pUserInput;
	QLineEdit* m_pWorkspaceInput;
	QLineEdit* m_pPasswordInput;
};

CPerforceTrayWidget::CPerforceTrayWidget(QWidget* pParent /*= nullptr*/)
{
	CMyClientApi& clientApi = g_pPerforceControl->GetClientApi();
	m_pPopUpMenu = new QPopupWidget("PerforceSettingsPopUp", QtUtil::MakeScrollable(new PerforceSettings()), QSize(340, 150), true);
	m_pPopUpMenu->SetFocusShareWidget(this);
	connect(this, &QToolButton::clicked, this, &CPerforceTrayWidget::OnClicked);
	g_pPerforceControl->signalWorkOfflineChanged.Connect(this, &CPerforceTrayWidget::OnConnectionStatusChanged);
	OnConnectionStatusChanged();
}

void CPerforceTrayWidget::OnConnectionStatusChanged()
{
	CryIconColorMap colorMap;
	colorMap[QIcon::Selected] = QColor(125, 198, 83);
	if (!g_pPerforceControl->IsWorkOffline())
		setIcon(CryIcon("icons:p4.ico", colorMap).pixmap(24, 24, QIcon::Normal, QIcon::On));
	else
		setIcon(CryIcon("icons:p4.ico"));
}

void CPerforceTrayWidget::OnClicked(bool bChecked)
{
	if (m_pPopUpMenu->isVisible())
	{
		m_pPopUpMenu->hide();
		return;
	}

	m_pPopUpMenu->ShowAt(mapToGlobal(QPoint(width(), height())));
}

namespace
{
	CryCriticalSection g_cPerforceValues;
}

struct CPerforceThread : public IThread
{
	const char*             m_filename;
	CPerforceSourceControl* m_pSourceControl;

	CPerforceThread(CPerforceSourceControl* pSourceControl, const char* filename)
	{
		m_pSourceControl = pSourceControl;
		m_filename = filename;
	}

	virtual ~CPerforceThread()
	{
		gEnv->pThreadManager->JoinThread(this, eJM_Join);
	}

protected:

	// Start accepting work on thread
	virtual void ThreadEntry()
	{
		m_pSourceControl->GetFileAttributesThread(m_filename);
	}
};

void CMyClientUser::HandleError(Error* e)
{
	/*
	   StrBuf  m;
	   e->Fmt( &m );
	   if ( strstr( m.Text(), "file(s) not in client view." ) )
	   e->Clear();
	   else if ( strstr( m.Text(), "no such file(s)" ) )
	   e->Clear();
	   else if ( strstr( m.Text(), "access denied" ) )
	   e->Clear();
	   /**/
	m_e = *e;
}

void CMyClientUser::Init()
{
	m_bIsClientUnknown = false;
	m_bIsSetup = false;
	m_bIsPreCreateFile = false;
	m_output.Empty();
	m_input.Empty();
	m_e.Clear();
}

void CMyClientUser::PreCreateFileName(const char* file)
{
	m_bIsPreCreateFile = true;
	cry_strcpy(m_file, file);
	m_findedFile[0] = 0;
}

void CMyClientUser::OutputStat(StrDict* varList)
{
	if (m_bIsSetup && !m_bIsPreCreateFile)
		return;

	StrRef var, val;
	*m_action = 0;
	*m_headAction = 0;
	*m_otherAction = 0;
	*m_depotFile = 0;
	*m_otherUser = 0;
	m_lockStatus = SCC_LOCK_STATUS_UNLOCKED;

	for (int i = 0; varList->GetVar(i, var, val); i++)
	{
		if (m_bIsPreCreateFile)
		{
			if (var == "clientFile")
			{
				char tmpval[MAX_PATH];
				cry_strcpy(tmpval, val.Text());
				char* ch = tmpval;
				while (ch = strchr(ch, '/'))
					*ch = '\\';

				if (!stricmp(tmpval, m_file))
				{
					cry_strcpy(m_findedFile, val.Text());
					m_bIsPreCreateFile = false;
				}
			}
		}
		else
		{
			if (var == "action")
				cry_strcpy(m_action, val.Text());
			else if (var == "headAction")
				cry_strcpy(m_headAction, val.Text());
			else if (var == "headRev")
			{
				cry_strcpy(m_clientHasLatestRev, val.Text());
				m_nFileHeadRev = val.Atoi64();
			}
			else if (!strncmp(var.Text(), "otherAction", 11) && !strcmp(val.Text(), "edit"))
				cry_strcpy(m_otherAction, val.Text());
			else if (var == "haveRev")
			{
				if (val != m_clientHasLatestRev)
					m_clientHasLatestRev[0] = 0;
				m_nFileHaveRev = val.Atoi64();
			}
			else if (var == "depotFile")
				cry_strcpy(m_depotFile, val.Text());
			else if (var == "otherOpen0")
				cry_strcpy(m_otherUser, val.Text());
			else if (!strncmp(var.Text(), "otherLock0", 10))
			{
				m_lockStatus = SCC_LOCK_STATUS_LOCKED_BY_OTHERS;
				cry_strcpy(m_lockedBy, val.Text());
			}
			else if (var == "ourLock")
				m_lockStatus = SCC_LOCK_STATUS_LOCKED_BY_US;
		}
	}
	m_bIsSetup = true;
}

void CMyClientUser::OutputInfo(char level, const char* data)
{
	m_output.Append(data);

	string strData(data);
	if ("Client unknown." == strData)
	{
		m_bIsClientUnknown = true;
		return;
	}

	int nStart = 0;
	string var = strData.Tokenize(":", nStart);
	string val;
	if (!var.IsEmpty() && nStart > 1)
		val = strData.Mid(nStart).Trim(" ");

	if ("Client root" == var)
		m_clientRoot = val;
	else if ("Client host" == var)
		m_clientHost = val;
	else if ("Client name" == var)
		m_clientName = val;
	else if ("Current directory" == var)
		m_currentDirectory = val;

	if (!m_bIsPreCreateFile)
		return;

	const char* ch = strrchr(data, '/');
	if (ch)
	{
		if (!stricmp(ch + 1, m_file))
		{
			cry_strcpy(m_findedFile, ch + 1);
			m_bIsPreCreateFile = false;
		}
	}
}

void CMyClientUser::InputData(StrBuf* buf, Error* e)
{
	if (m_bIsCreatingChangelist)
		buf->Set(m_input);
}

void CMyClientUser::Edit(FileSys* f1, Error* e)
{
	char msrc[4000];
	char mdst[4000];

	char* src = &msrc[0];
	char* dst = &mdst[0];

	f1->Open(FOM_READ, e);
	int size = f1->Read(msrc, 10240, e);
	msrc[size] = 0;
	f1->Close(e);

	while (*dst = *src)
	{
		if (!strnicmp(src, "\nDescription", 11))
		{
			src++;
			while (*src != '\n' && *src != '\0')
				src++;
			src++;
			while (*src != '\n' && *src != '\0')
				src++;
			strcpy(dst, "\nDescription:\n\t");
			dst += 15;
			strcpy(dst, m_desc);
			dst += strlen(m_desc);
			strcpy(dst, " (by Perforce Plug-in)");
			dst += 22;
		}
		else
		{
			src++;
			dst++;
		}
	}

	f1->Open(FOM_WRITE, e);
	f1->Write(mdst, strlen(mdst), e);
	f1->Close(e);

	cry_strcpy(m_desc, m_initDesc);
}

////////////////////////////////////////////////////////////

void CMyClientApi::Run(const char* func)
{
	// The "enableStreams" var has to be set prior to any Run call in order to be able to support Perforce streams
	ClientApi::SetVar("enableStreams");
	ClientApi::Run(func);
}

void CMyClientApi::Run(const char* func, ClientUser* ui)
{
	ClientApi::SetVar("enableStreams");
	ClientApi::Run(func, ui);
}

////////////////////////////////////////////////////////////

CPerforceSourceControl::CPerforceSourceControl() : m_ref(0)
{
	m_thread = 0;
	m_bIsWorkOffline = false;
	m_bIsFailConnectionLogged = false;
	m_dwLastAccessTime = 0;
	m_isSecondThread = false;
	Connect();
	InitCommonControls();
}

CPerforceSourceControl::~CPerforceSourceControl()
{
	if (m_thread)
	{
		gEnv->pThreadManager->JoinThread(m_thread, eJM_Join);
		delete m_thread;
	}
}

bool CPerforceSourceControl::Connect()
{
	m_client.Init(&m_e);
	if (m_e.Test())
	{
		SetWorkOffline(true);
		if (!m_bIsFailConnectionLogged)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "\nPerforce plugin: Failed to connect.");
			m_bIsFailConnectionLogged = true;
		}
		return false;
	}
	else
	{
		CryLog("\nPerforce plugin: Connected.");
		m_bIsFailConnectionLogged = false;
	}
	return true;
}

bool CPerforceSourceControl::Reconnect()
{
	if (m_client.Dropped())
	{
		if (!m_bIsFailConnectionLogged)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "\nPerforce connection dropped: attempting reconnect");
		}
		FreeData();
		if (!Connect())
		{
			SetWorkOffline(true);
			return false;
		}
	}
	return true;
}

void CPerforceSourceControl::FreeData()
{
	m_client.Final(&m_e);
	m_e.Clear();
}

void CPerforceSourceControl::Init()
{
	CheckConnection();
}

/*
   Note:

   ConvertFileNameCS doesn't seem to do any conversion.

   First it checks if the path is available on local HDD, then if it hasn't found
   the path locally it looks in perforce (passing in local paths to P4v's API).

   The path stored in sDst is the location which either exists and was found, or
   will exist once you get latest...

   If the file neither exists locally or in the depot, sDst will be as far as exists in Perforce...
   Why is this a problem? Well, if you called GetFileAttributes on a file that doesn't exist in P4V
   you'll be given the attributes for the deepest folder of the path you provided which exists on
   your local machine - which is probably why you're here reading this comment :)

 */
void CPerforceSourceControl::ConvertFileNameCS(char* sDst, const char* sSrcFilename)
{
	*sDst = 0;
	bool bFinded = true;

	char szAdjustedFile[ICryPak::g_nMaxPath];
	cry_strcpy(szAdjustedFile, sSrcFilename);

	//_finddata_t fd;
	WIN32_FIND_DATA fd;

	char csPath[ICryPak::g_nMaxPath] = { 0 };

	char* ch, * ch1;

	ch = strrchr(szAdjustedFile, '\\');
	ch1 = strrchr(szAdjustedFile, '/');
	if (ch < ch1) ch = ch1;
	bool bIsFirstTime = true;

	bool bIsEndSlash = false;
	if (ch && ch - szAdjustedFile + 1 == strlen(szAdjustedFile))
		bIsEndSlash = true;

	while (ch)
	{
		//intptr_t handle;
		//handle = gEnv->pCryPak->FindFirst( szAdjustedFile, &fd );
		HANDLE handle = FindFirstFile(szAdjustedFile, &fd);
		//if (handle != -1)
		if (handle != INVALID_HANDLE_VALUE)
		{
			char tmp[ICryPak::g_nMaxPath];
			cry_strcpy(tmp, csPath);
			//cry_strcpy(csPath, fd.name);
			cry_strcpy(csPath, fd.cFileName);
			if (!bIsFirstTime)
				cry_strcat(csPath, "\\");
			bIsFirstTime = false;
			cry_strcat(csPath, tmp);

			//gEnv->pCryPak->FindClose( handle );
			FindClose(handle);
		}

		*ch = 0;
		ch = strrchr(szAdjustedFile, '\\');
		ch1 = strrchr(szAdjustedFile, '/');
		if (ch < ch1) ch = ch1;
	}

	if (!*csPath)
		return;

	cry_strcat(szAdjustedFile, "\\");
	cry_strcat(szAdjustedFile, csPath);
	if (bIsEndSlash || strlen(szAdjustedFile) < strlen(sSrcFilename))
		cry_strcat(szAdjustedFile, "\\");

	// if we have only folder on disk find in perforce
	if (strlen(szAdjustedFile) < strlen(sSrcFilename))
	{
		if (IsFileManageable(szAdjustedFile))
		{
			char file[MAX_PATH];
			char clienFile[MAX_PATH] = { 0 };

			bool bCont = true;
			while (bCont)
			{
				cry_strcpy(file, &sSrcFilename[strlen(szAdjustedFile)]);
				char* ch = strchr(file, '/');
				char* ch1 = strchr(file, '\\');
				if (ch < ch1) ch = ch1;

				if (ch)
				{
					*ch = 0;
					bFinded = bCont = FindDir(clienFile, szAdjustedFile, file);
				}
				else
				{
					bFinded = FindFile(clienFile, szAdjustedFile, file);
					bCont = false;
				}
				cry_strcpy(szAdjustedFile, clienFile);
				if (bCont && strlen(clienFile) >= strlen(sSrcFilename))
					bCont = false;
			}
		}
	}

	if (bFinded)
		strcpy(sDst, szAdjustedFile);
}

void CPerforceSourceControl::MakePathCS(char* sDst, const char* sSrcFilename)
{
	char szAdjustedFile[ICryPak::g_nMaxPath];
	char szCheckedPath[ICryPak::g_nMaxPath];
	cry_strcpy(szAdjustedFile, sSrcFilename);

	char* ch = &szAdjustedFile[0];

	while (ch)
	{
		char* ch1 = strchr(ch, '/');
		ch = strchr(ch, '\\');

		if (ch1 && ch > ch1) ch = ch1;

		if (ch)
		{
			cry_strcpy(szCheckedPath, szAdjustedFile, ch - szAdjustedFile + 1);

			if (strlen(szCheckedPath) == 3 && szCheckedPath[1] == ':')
			{
				ch++;
				continue;
			}

			if (IsFileManageable(szCheckedPath, false))
			{
				cry_strcpy(szAdjustedFile, szCheckedPath);
				break;
			}
			ch++;
		}
	}

	if (strlen(szAdjustedFile) < strlen(sSrcFilename))
	{
		if (IsFileManageable(szAdjustedFile))
		{
			char file[MAX_PATH];
			char clienFile[MAX_PATH] = { 0 };

			bool bCont = true;
			while (bCont)
			{
				cry_strcpy(file, &sSrcFilename[strlen(szAdjustedFile)]);
				char* ch = strchr(file, '/');
				char* ch1 = strchr(file, '\\');
				if (ch < ch1) ch = ch1;

				bool bFinded = false;

				if (ch)
				{
					*ch = 0;
					bFinded = bCont = FindDir(clienFile, szAdjustedFile, file);
				}

				if (!bFinded)
				{
					strcpy(&szAdjustedFile[strlen(szAdjustedFile)], &sSrcFilename[strlen(szAdjustedFile)]);
					break;
				}

				cry_strcpy(szAdjustedFile, clienFile);
				if (bCont && strlen(clienFile) >= strlen(sSrcFilename))
					bCont = false;
			}
		}
	}

	if (strlen(szAdjustedFile) == strlen(sSrcFilename))
		strcpy(sDst, szAdjustedFile);
	else
		strcpy(sDst, sSrcFilename);
}

void CPerforceSourceControl::RenameFolders(const char* path, const char* pathOld)
{
	const char* ch = strchr(pathOld, '\\');
	if (ch)
		ch = strchr(ch + 1, '\\');
	while (ch)
	{
		ch++;
		const char* const ch1 = strchr(ch, '\\');
		if (ch1 && strncmp(ch, &path[ch - pathOld], ch1 - ch))
		{
			char newpath[ICryPak::g_nMaxPath];
			char newpathOld[ICryPak::g_nMaxPath];
			cry_strcpy(newpath, path, (size_t)(ch1 - pathOld));
			cry_strcpy(newpathOld, pathOld, (size_t)(ch1 - pathOld));
			MoveFile(newpathOld, newpath);
		}
		ch = ch1;
	}
}

bool CPerforceSourceControl::FindDir(char* clientFile, const char* folder, const char* dir)
{
	if (!Reconnect())
		return false;

	char fl[MAX_PATH];
	cry_strcpy(fl, folder);
	cry_strcat(fl, "*");
	char* argv[] = { fl };
	m_ui.PreCreateFileName(dir);

	m_client.SetArgv(1, argv);
	m_client.Run("dirs", &m_ui);
	m_client.WaitTag();

	if (m_ui.m_e.Test())
		return false;

	strcpy(clientFile, folder);
	strcat(clientFile, m_ui.m_findedFile);
	strcat(clientFile, "\\");
	if (*m_ui.m_findedFile)
		return true;

	return false;
}

bool CPerforceSourceControl::FindFile(char* clientFile, const char* folder, const char* file)
{
	if (!Reconnect())
		return false;

	char fullPath[MAX_PATH];

	cry_strcpy(fullPath, folder);
	cry_strcat(fullPath, file);

	char fl[MAX_PATH];
	cry_strcpy(fl, folder);
	cry_strcat(fl, "*");
	char* argv[] = { fl };
	m_ui.PreCreateFileName(fullPath);

	m_client.SetArgv(1, argv);
	m_client.Run("fstat", &m_ui);
	m_client.WaitTag();

	if (m_ui.m_e.Test())
		return false;

	strcpy(clientFile, m_ui.m_findedFile);
	if (*clientFile)
		return true;

	return false;
}

bool CPerforceSourceControl::IsFileManageable(const char* sFilename, bool bCheckFatal)
{
	if (!Reconnect())
		return false;

	bool bRet = false;
	bool fatal = false;

	if (m_bIsWorkOffline)
		return false;

	char fl[MAX_PATH];
	cry_strcpy(fl, sFilename);
	//ConvertFileNameCS(fl, sFilename);

	char* argv[] = { fl };
	Run("fstat", 1, argv, true);

	m_ui.Init();
	m_client.SetArgv(1, argv);
	m_client.Run("fstat", &m_ui);
	m_client.WaitTag();

	if (bCheckFatal && m_ui.m_e.IsFatal())
	{
		fatal = true;
	}

	if (!fatal && !m_ui.m_e.IsError())
	{
		bRet = true;
	}

	m_ui.m_e.Clear();
	return bRet;
}

bool CPerforceSourceControl::IsFileExistsInDatabase(const char* sFilename)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char fl[MAX_PATH];
	cry_strcpy(fl, sFilename);
	char* argv[] = { fl };
	m_ui.Init();
	m_client.SetArgv(1, argv);
	m_client.Run("fstat", &m_ui);
	m_client.WaitTag();

	if (!m_ui.m_e.Test())
	{
		if (strcmp(m_ui.m_headAction, "delete"))
			bRet = true;
	}
	else
	{
		//  StrBuf errorMsg;
		//  m_ui.m_e.Fmt(&errorMsg);
	}
	m_ui.m_e.Clear();
	return bRet;
}

bool CPerforceSourceControl::IsFileCheckedOutByUser(const char* sFilename, bool* pIsByAnotherUser)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char fl[MAX_PATH];
	cry_strcpy(fl, sFilename);
	char* argv[] = { fl };
	m_ui.Init();
	m_client.SetArgv(1, argv);
	m_client.Run("fstat", &m_ui);
	m_client.WaitTag();

	if ((!strcmp(m_ui.m_action, "edit") || !strcmp(m_ui.m_action, "add")) && !m_ui.m_e.Test())
		bRet = true;

	if (pIsByAnotherUser)
	{
		*pIsByAnotherUser = false;
		if (!strcmp(m_ui.m_otherAction, "edit") && !m_ui.m_e.Test())
			*pIsByAnotherUser = true;
	}

	m_ui.m_e.Clear();
	return bRet;
}

bool CPerforceSourceControl::IsFileLatestVersion(const char* sFilename)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char fl[MAX_PATH];
	cry_strcpy(fl, sFilename);
	char* argv[] = { fl };
	m_ui.Init();
	m_client.SetArgv(1, argv);
	m_client.Run("fstat", &m_ui);
	m_client.WaitTag();

	if (m_ui.m_clientHasLatestRev[0] != 0)
		bRet = true;
	m_ui.m_e.Clear();
	return bRet;
}

uint32 CPerforceSourceControl::GetFileAttributesAndFileName(const char* filename, char* FullFileName)
{
	//	g_pSystem->GetILog()->Log("\n checking connection");
	if (!Reconnect())
		return false;

	if (FullFileName)
		FullFileName[0] = 0;

	CCryFile file;
	bool bCryFile = file.Open(filename, "rb");

	uint32 attributes = 0;

	char sFullFilenameLC[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, sFullFilenameLC);
	cry_strcat(sFullFilenameLC, "\\");
	if (bCryFile)
	{
		if (*sFullFilenameLC && !strnicmp(sFullFilenameLC, filename, strlen(sFullFilenameLC)))
			cry_strcpy(sFullFilenameLC, file.GetAdjustedFilename());
		else
			cry_strcat(sFullFilenameLC, file.GetAdjustedFilename());
	}
	else
		cry_strcat(sFullFilenameLC, filename);

	char sFullFilename[ICryPak::g_nMaxPath];
	ConvertFileNameCS(sFullFilename, sFullFilenameLC);

	if (FullFileName)
		strcpy(FullFileName, sFullFilename);

	if (bCryFile && file.IsInPak())
	{
		//		g_pSystem->GetILog()->Log("\n file is in pak");
		attributes = SCC_FILE_ATTRIBUTE_READONLY | SCC_FILE_ATTRIBUTE_INPAK;

		if (IsFileManageable(sFullFilename) && IsFileExistsInDatabase(sFullFilename))
		{
			//			g_pSystem->GetILog()->Log("\n file is managable and also exists in the database");
			attributes |= SCC_FILE_ATTRIBUTE_MANAGED;
			bool isByAnotherUser;
			if (IsFileCheckedOutByUser(sFullFilename, &isByAnotherUser))
				attributes |= SCC_FILE_ATTRIBUTE_CHECKEDOUT;
			if (isByAnotherUser)
				attributes |= SCC_FILE_ATTRIBUTE_BYANOTHER;
		}
	}
	else
	{
		DWORD dwAttrib = ::GetFileAttributes(sFullFilename);
		if (dwAttrib != INVALID_FILE_ATTRIBUTES)
		{
			//		g_pSystem->GetILog()->Log("\n we have valid file attributes");
			attributes = SCC_FILE_ATTRIBUTE_NORMAL;
			if (dwAttrib & FILE_ATTRIBUTE_READONLY)
				attributes |= SCC_FILE_ATTRIBUTE_READONLY;

			if (IsFileManageable(sFullFilename))
			{
				//			g_pSystem->GetILog()->Log("\n file is manageable");
				if (IsFileExistsInDatabase(sFullFilename))
				{
					attributes |= SCC_FILE_ATTRIBUTE_MANAGED;
					//				g_pSystem->GetILog()->Log("\n file exists in database");
					bool isByAnotherUser;
					if (IsFileCheckedOutByUser(sFullFilename, &isByAnotherUser))
					{
						//					g_pSystem->GetILog()->Log("\n file is checked out");
						attributes |= SCC_FILE_ATTRIBUTE_CHECKEDOUT;
					}
					if (isByAnotherUser)
					{
						//					g_pSystem->GetILog()->Log("\n by another user");
						attributes |= SCC_FILE_ATTRIBUTE_BYANOTHER;
					}
				}
				else
				{
					if (*sFullFilename && sFullFilename[strlen(sFullFilename) - 1] == '\\')
					{
						attributes |= SCC_FILE_ATTRIBUTE_MANAGED | SCC_FILE_ATTRIBUTE_FOLDER;
					}
				}
			}
		}
	}

	//	g_pSystem->GetILog()->Log("\n file has invalid file attributes");
	if (!attributes && !bCryFile)
	{
		if (IsFileManageable(sFullFilename))
		{
			if (IsFileExistsInDatabase(sFullFilename))
			{
				//				g_pSystem->GetILog()->Log("\n file is managable and exists in the database");
				attributes = SCC_FILE_ATTRIBUTE_MANAGED | SCC_FILE_ATTRIBUTE_NORMAL | SCC_FILE_ATTRIBUTE_READONLY;
				bool isByAnotherUser;
				if (IsFileCheckedOutByUser(sFullFilename, &isByAnotherUser))
					attributes |= SCC_FILE_ATTRIBUTE_CHECKEDOUT;
				if (isByAnotherUser)
					attributes |= SCC_FILE_ATTRIBUTE_BYANOTHER;
			}
			else
			{
				if (*sFullFilename && sFullFilename[strlen(sFullFilename) - 1] == '\\')
				{
					attributes |= SCC_FILE_ATTRIBUTE_MANAGED | SCC_FILE_ATTRIBUTE_FOLDER;
				}
			}
		}
	}

	if (!attributes && IsFolder(filename, FullFileName))
		attributes = SCC_FILE_ATTRIBUTE_FOLDER;

	if ((attributes & SCC_FILE_ATTRIBUTE_FOLDER) && FullFileName)
		strcat(FullFileName, "...");

	return attributes ? attributes : SCC_FILE_ATTRIBUTE_INVALID;
}

void CPerforceSourceControl::GetFileAttributesThread(const char* filename)
{
	uint32 unRetValue = GetFileAttributesAndFileName(filename, 0);

	AUTO_LOCK(g_cPerforceValues);
	m_unRetValue = unRetValue;
	m_isSetuped = true;
}

uint32 CPerforceSourceControl::GetFileAttributes(const char* filename)
{
	//return GetFileAttributesAndFileName(filename, 0);

	DWORD dwTime = GetTickCount();

	if (m_bIsWorkOffline || dwTime - m_dwLastAccessTime < 1000)
	{
		m_dwLastAccessTime = dwTime;
		return GetFileAttributesAndFileName(filename, 0);
	}

	m_isSkipThread = false;
	m_isSetuped = false;
	uint32 unRetValue = SCC_FILE_ATTRIBUTE_INVALID;

	if (m_thread)
	{
		gEnv->pThreadManager->JoinThread(m_thread, eJM_Join);
		delete m_thread;
	}

	m_isSecondThread = true;

	m_thread = new CPerforceThread(this, filename);
	if (!gEnv->pThreadManager->SpawnThread(m_thread, "PerforcePlugin"))
	{
		CryFatalError("Error spawning \"PerforcePlugin\" thread.");
	}

	DWORD dwWaitTime = 10000; // 10 sec
	DWORD dwCurTime = dwTime;

	while (1)
	{
		if (!m_isSkipThread && GetTickCount() - dwCurTime > dwWaitTime)
		{
			if (CryMessageBox(_T("Connection to Perforce is not responding. Do you want to switch to the offline mode?"), _T("Perforce Plug-in Error"), eMB_YesCancel) == eQR_Yes)
			{
				SetWorkOffline(true);
				CheckConnection();
				break;
			}
			dwCurTime = GetTickCount();
			dwWaitTime = 10000; // 10 sec
		}

		CrySleep(50);

		if (m_isSetuped)
		{
			AUTO_LOCK(g_cPerforceValues);
			unRetValue = m_unRetValue;
			break;
		}
	}

	m_isSecondThread = false;
	m_dwLastAccessTime = dwTime;

	return unRetValue;
}

bool CPerforceSourceControl::IsFolder(const char* filename, char* FullFileName)
{
	bool bFolder = false;

	char sFullFilename[ICryPak::g_nMaxPath];
	ConvertFileNameCS(sFullFilename, filename);

	uint32 attr = ::GetFileAttributes(sFullFilename);
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		if (*sFullFilename && sFullFilename[strlen(sFullFilename) - 1] == '\\')
			bFolder = true;
		else
			return false;
	}
	else if ((attr & FILE_ATTRIBUTE_DIRECTORY))
		bFolder = true;

	if (bFolder && FullFileName)
		strcpy(FullFileName, sFullFilename);

	return bFolder;
}

bool CPerforceSourceControl::DoesChangeListExist(const char* pDesc, char* changeid, int nLen)
{
	if (!Reconnect())
		return false;

	bool ret = false;

	char* argv[] = { "-c", m_client.GetClient().Text(), "-s", "pending", "-l" };
	m_ui.Init();
	m_client.SetArgv(5, argv);
	m_client.Run("changes", &m_ui);

	string id("");
	bool foundChange = false;
	string changes(m_ui.m_output);
	string token("\r\n");
	int nStart = 0;
	string item = changes.Tokenize(token, nStart);
	while (!item.IsEmpty())
	{
		int idx = item.Find("Change ");
		if (idx != -1)
		{
			int last = item.Find(" on ");
			int first = item.Find(" ");
			id = item.Left(last);
			id = id.Right(id.GetLength() - (first + 1));
		}
		idx = item.Find(pDesc);
		if (idx != -1)
		{
			foundChange = true;
			break;
		}

		item = changes.Tokenize(token, nStart);
	}

	m_ui.m_output.Empty();
	m_ui.m_input.Empty();

	if (!m_ui.m_e.Test() && !id.IsEmpty() && foundChange)
	{
		cry_sprintf(changeid, nLen, "%s", id.GetBuffer());
		ret = true;
	}

	return ret;
}

bool CPerforceSourceControl::CreateChangeList(const char* pDesc, char* changeid, int nLen)
{
	if (!Reconnect())
		return false;

	bool ret = false;

	char* argv[] = { "-o" };
	m_ui.Init();
	m_client.SetArgv(1, argv);
	m_client.Run("change", &m_ui);

	string change(m_ui.m_output);
	string description;
	description.Format("%s", pDesc);
	change.Replace("<enter description here>", description);

	string files("Files:");
	int end = change.Find(files.GetBuffer()) + files.GetLength();
	int iFiles = change.Find(files, end);
	if (iFiles >= 0)
	{
		end = iFiles;
		if (change.GetLength() > end)
			change = change.Left(end);
	}

	m_ui.Init();
	m_ui.m_input = change;
	m_ui.m_bIsCreatingChangelist = true;
	char* argv2[] = { "-i" };
	m_client.SetArgv(1, argv2);
	m_client.Run("change", &m_ui);
	m_ui.m_bIsCreatingChangelist = false;

	string changeId(m_ui.m_output);
	int lastIdx = changeId.ReverseFind(' ');
	int firstIdx = changeId.Find(' ');
	string left = changeId.Left(lastIdx);
	string id(left.Right(left.GetLength() - (firstIdx + 1)));
	cry_sprintf(changeid, nLen, "%s", id.GetBuffer());

	if (!m_ui.m_e.Test())
		ret = true;

	return ret;
}

bool CPerforceSourceControl::SubmitChangeList(char* changeid)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char* argv[] = { "-c", changeid };
	m_ui.Init();
	m_client.SetArgv(2, argv);
	m_client.Run("submit", &m_ui);

	if (!m_ui.m_e.Test())
		bRet = true;

	return bRet;
}

bool CPerforceSourceControl::DeleteChangeList(char* changeid)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char* argv[] = { "-d", changeid };
	m_ui.Init();
	m_client.SetArgv(2, argv);
	m_client.Run("change", &m_ui);

	if (!m_ui.m_e.Test())
		bRet = true;

	return bRet;
}

bool CPerforceSourceControl::Reopen(const char* filename, char* changeid)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char FullFileName[MAX_PATH];
	string files = filename;
	int curPos = 0;
	string file = files.Tokenize(";", curPos);
	for (; !file.IsEmpty(); file = files.Tokenize(";", curPos))
	{
		if (file.Trim().IsEmpty())
			continue;
		uint32 attrib = GetFileAttributesAndFileName(file, FullFileName);

		if ((attrib != SCC_FILE_ATTRIBUTE_INVALID) && (attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
		{
			char* argv[] = { "-c", changeid, FullFileName };
			m_ui.Init();
			m_client.SetArgv(3, argv);
			m_client.Run("reopen", &m_ui);

			if (!m_ui.m_e.Test())
				bRet = true;
		}
	}

	return bRet;
}

bool CPerforceSourceControl::Add(const char* filename, const char* desc, int nFlags, char* changelistId)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char FullFileName[MAX_PATH];
	string files = filename;
	int curPos = 0;
	string file = files.Tokenize(";", curPos);
	for (; !file.IsEmpty(); file = files.Tokenize(";", curPos))
	{
		if (file.Trim().IsEmpty())
			continue;
		uint32 attrib = GetFileAttributesAndFileName(file, FullFileName);
		char sFullFilename[ICryPak::g_nMaxPath];
		if (attrib & SCC_FILE_ATTRIBUTE_FOLDER)
		{
			cry_strcpy(sFullFilename, filename);
			cry_strcat(sFullFilename, "*");
		}
		else
		{
			MakePathCS(sFullFilename, FullFileName);
			if (strcmp(sFullFilename, FullFileName))
				RenameFolders(sFullFilename, FullFileName);
		}

		if ((attrib & SCC_FILE_ATTRIBUTE_FOLDER) || ((attrib != SCC_FILE_ATTRIBUTE_INVALID) && !(attrib & SCC_FILE_ATTRIBUTE_MANAGED) && IsFileManageable(sFullFilename)))
		{
			if (nFlags & ADD_CHANGELIST && changelistId)
			{
				char* argv[] = { "-c", changelistId, sFullFilename };
				m_ui.Init();
				m_client.SetArgv(3, argv);
				m_client.Run("add", &m_ui);

				if (desc && !(nFlags & ADD_WITHOUT_SUBMIT))
				{
					cry_strcpy(m_ui.m_desc, desc);
					m_ui.Init();
					m_client.SetArgv(2, argv);
					m_client.Run("submit", &m_ui);
				}
			}
			else
			{
				char* argv[] = { sFullFilename };
				m_ui.Init();
				m_client.SetArgv(1, argv);
				m_client.Run("add", &m_ui);

				if (desc && !(nFlags & ADD_WITHOUT_SUBMIT))
				{
					cry_strcpy(m_ui.m_desc, desc);
					m_ui.Init();
					m_client.SetArgv(1, argv);
					m_client.Run("submit", &m_ui);
				}
			}

			if (!m_ui.m_e.Test())
				bRet = true;
		}
	}

	return bRet;
}

bool CPerforceSourceControl::CheckIn(const char* filename, const char* desc, int nFlags)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char FullFileName[MAX_PATH];

	string files = filename;
	int curPos = 0;
	string file = files.Tokenize(";", curPos);
	for (; !file.IsEmpty(); file = files.Tokenize(";", curPos))
	{
		if (file.Trim().IsEmpty())
			continue;
		uint32 attrib = GetFileAttributesAndFileName(file, FullFileName);

		if ((attrib & SCC_FILE_ATTRIBUTE_FOLDER) || ((attrib != SCC_FILE_ATTRIBUTE_INVALID) && (attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)))
		{
			{
				char* argv[] = { "-c", "default", FullFileName };
				m_client.SetArgv(3, argv);
				m_client.Run("reopen", &m_ui);
			}

			if (desc)
				cry_strcpy(m_ui.m_desc, desc);
			{
				char* argv[] = { FullFileName };
				m_client.SetArgv(1, argv);
				m_client.Run("submit", &m_ui);
			}

			if (!m_ui.m_e.Test())
				bRet = true;
		}
	}

	return bRet;
}

bool CPerforceSourceControl::CheckOut(const char* filename, int nFlags, char* changelistId)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char FullFileName[MAX_PATH];

	string files = filename;
	int curPos = 0;
	string file = files.Tokenize(";", curPos);
	for (; !file.IsEmpty(); file = files.Tokenize(";", curPos))
	{
		if (file.Trim().IsEmpty())
			continue;
		uint32 attrib = GetFileAttributesAndFileName(file, FullFileName);

		if ((attrib & SCC_FILE_ATTRIBUTE_FOLDER) || ((attrib & SCC_FILE_ATTRIBUTE_MANAGED) && !(attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)))
		{
			if (nFlags & ADD_CHANGELIST && changelistId)
			{
				char* argv[] = { "-c", changelistId, FullFileName };
				m_client.SetArgv(3, argv);
				m_client.Run("edit", &m_ui);
			}
			else
			{
				char* argv[] = { FullFileName };
				m_client.SetArgv(1, argv);
				m_client.Run("edit", &m_ui);
			}

			if (!m_ui.m_e.Test())
				bRet = true;
		}
	}
	return bRet;
}

bool CPerforceSourceControl::UndoCheckOut(const char* filename, int nFlags)
{
	if (!Reconnect())
		return false;

	bool bRet = false;

	char FullFileName[MAX_PATH];

	string files = filename;
	int curPos = 0;
	string file = files.Tokenize(";", curPos);
	for (; !file.IsEmpty(); file = files.Tokenize(";", curPos))
	{
		if (file.Trim().IsEmpty())
			continue;
		uint32 attrib = GetFileAttributesAndFileName(file, FullFileName);

		if ((attrib & SCC_FILE_ATTRIBUTE_FOLDER) || ((attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)))
		{
			char* argv[] = { FullFileName };
			m_client.SetArgv(1, argv);
			m_client.Run("revert", &m_ui);
			if (!m_ui.m_e.Test())
				bRet = true;
		}
	}
	return true; // always return true to avoid error message box on folder operation
}

bool CPerforceSourceControl::Rename(const char* filename, const char* newname, const char* desc, int nFlags)
{
	if (!Reconnect())
		return false;

	bool bRet = false;
	char FullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);

	if (!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
		return true;

	if (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
		UndoCheckOut(filename, 0);

	//if(m_pIntegrator->FileCheckedOutByAnotherUser(FullFileName))
	//return false;

	char FullNewFileName[MAX_PATH];
	cry_strcpy(FullNewFileName, newname);

	{
		char* argv[] = { FullFileName, FullNewFileName };
		m_client.SetArgv(2, argv);
		m_client.Run("integrate", &m_ui);
	}

	{
		char* argv[] = { FullFileName };
		m_client.SetArgv(1, argv);
		m_client.Run("delete", &m_ui);
	}
	if (desc)
		cry_strcpy(m_ui.m_desc, desc);
	{
		char* argv[] = { FullFileName };
		m_client.SetArgv(1, argv);
		m_client.Run("submit", &m_ui);
	}

	if (desc)
		cry_strcpy(m_ui.m_desc, desc);
	{
		char* argv[] = { FullNewFileName };
		m_client.SetArgv(1, argv);
		m_client.Run("submit", &m_ui);
	}

	if (!m_ui.m_e.Test())
		bRet = true;

	//p4 integrate source_file target_file
	//p4 delete source_file
	//p4 submit

	return bRet;
}

bool CPerforceSourceControl::Delete(const char* filename, const char* desc, int nFlags, char* changelistId /*= NULL*/)
{
	if (!Reconnect())
		return false;

	bool bRet = false;
	char FullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, FullFileName);

	if (!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
		return true;

	if ((attrib & SCC_FILE_ATTRIBUTE_MANAGED) && (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
	{
		char* argv[] = { FullFileName };
		m_client.SetArgv(1, argv);
		m_client.Run("revert", &m_ui);
	}

	if ((nFlags & ADD_CHANGELIST) && (changelistId != NULL))
	{
		char* argv[] = { "-c", changelistId, FullFileName };
		m_client.SetArgv(3, argv);
		m_client.Run("delete", &m_ui);

		if (desc && !(nFlags & DELETE_WITHOUT_SUBMIT))
		{
			cry_strcpy(m_ui.m_desc, desc);
			m_client.SetArgv(2, argv);
			m_client.Run("submit", &m_ui);
		}
	}
	else
	{
		char* argv[] = { FullFileName };
		m_client.SetArgv(1, argv);
		m_client.Run("delete", &m_ui);

		if (desc && !(nFlags & DELETE_WITHOUT_SUBMIT))
		{
			cry_strcpy(m_ui.m_desc, desc);
			m_client.SetArgv(1, argv);
			m_client.Run("submit", &m_ui);
		}
	}

	if (!m_ui.m_e.Test())
		bRet = true;

	return bRet;
}

void CPerforceSourceControl::SetWorkOffline(bool bWorkOffline)
{
	m_bIsWorkOffline = bWorkOffline;
	signalWorkOfflineChanged();
}

void CPerforceSourceControl::ShowSettings()
{
	m_isSkipThread = true;

	string strPassword(m_client.GetPassword().Value());
	string strClient(m_client.GetClient().Value());
	string strUser(m_client.GetUser().Value());
	string strPort(m_client.GetPort().Value());

	if (PerforceConnection::OpenPasswordDlg(strPort, strUser, strClient, strPassword, m_bIsWorkOffline))
	{
		Error e;
		m_client.DefinePassword(strPassword, &e);
		m_client.DefineClient(strClient, &e);
		m_client.DefinePort(strPort, &e);
		m_client.DefineUser(strUser, &e);
		CheckConnection();
	}
}

const char* CPerforceSourceControl::GetErrorByGenericCode(int nGeneric)
{
	switch (nGeneric)
	{
	case EV_USAGE:
		return "Request is not consistent with documentation or cannot support a server version";
	case EV_UNKNOWN:
		return "Using unknown entity";
	case EV_CONTEXT:
		return "Using entity in wrong context";
	case EV_ILLEGAL:
		return "Trying to do something you can't";
	case EV_NOTYET:
		return "Something must be corrected first";
	case EV_PROTECT:
		return "Operation was prevented by protection level";

	// No fault at all
	case EV_EMPTY:
		return "Action returned empty results";

	// not the fault of the user
	case EV_FAULT:
		return "Inexplicable program fault";
	case EV_CLIENT:
		return "Client side program errors";
	case EV_ADMIN:
		return "Server administrative action required";
	case EV_CONFIG:
		return "Client configuration is inadequate";
	case EV_UPGRADE:
		return "Client or server is too old to interact";
	case EV_COMM:
		return "Communication error";
	case EV_TOOBIG:
		return "File is too big";
	}

	return "Undefined";
}

bool CPerforceSourceControl::Run(const char* func, int nArgs, char* argv[], bool bOnlyFatal)
{
	for (int x = 0; x < nArgs; ++x)
	{
		if (argv[x][0] == '\0')
			return false;
	}

	if (!Reconnect())
		return false;

	bool bRet = false;

	m_ui.Init();
	m_client.SetArgv(nArgs, argv);
	m_client.Run(func, &m_ui);
	m_client.WaitTag();

#if 0 // connection debug
	for (int argid = 0; argid < nArgs; argid++)
	{
		CryLog("\n arg %d : %s", argid, argv[argid]);
	}
#endif // connection debug

	if (bOnlyFatal)
	{
		if (!m_ui.m_e.IsFatal())
			bRet = true;
	}
	else
	{
		if (!m_ui.m_e.Test())
			bRet = true;
	}

	if (m_ui.m_e.GetSeverity() == E_FAILED || m_ui.m_e.GetSeverity() == E_FATAL)
	{
		static int nGenericPrev = 0;
		const int nGeneric = m_ui.m_e.GetGeneric();

		if (IsSomeTimePassed())
			nGenericPrev = 0;

		if (nGenericPrev != nGeneric)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Perforce plugin: %s", GetErrorByGenericCode(nGeneric));

			StrBuf m;
			m_ui.m_e.Fmt(&m);
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Perforce plugin: %s", m.Text());
			nGenericPrev = nGeneric;
		}
	}

	m_ui.m_e.Clear();
	return bRet;
}

bool CPerforceSourceControl::CheckConnection()
{
	m_ui.m_bIsClientUnknown = false;
	bool bRet = false;

	if (!m_bIsWorkOffline)
	{
		bRet = Run("info", 0, 0);
	}

	if (m_bIsWorkOffline)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Perforce plugin: Perforce is offline");
		return false;
	}

	if (m_ui.m_bIsClientUnknown || m_ui.m_clientRoot.IsEmpty())
	{
		string errorMsg;
		errorMsg.Format("Workspace %s for host %s is unknown. Check Perforce settings.", m_ui.m_clientName, m_ui.m_clientHost);
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, string("Perforce plugin: ") + errorMsg);
		bRet = false;
	}
	else if (m_ui.m_clientRoot.CompareNoCase(m_ui.m_currentDirectory.Left(m_ui.m_clientRoot.GetLength())))
	{
		string errorMsg;
		errorMsg.Format("Working folder (%s) is out of Perforce root (%s). Change Perforce settings or start Editor from %s...", m_ui.m_currentDirectory, m_ui.m_clientRoot, m_ui.m_clientRoot);
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, string("Perforce plugin: ") + errorMsg);
		bRet = false;
	}
	return bRet;
}

bool CPerforceSourceControl::GetLatestVersion(const char* filename, int nFlags)
{
	if (!Reconnect())
		return false;

	bool bRet = false;
	char FullFileName[MAX_PATH];

	string files = filename;
	int curPos = 0;
	string file = files.Tokenize(";", curPos);
	for (; !file.IsEmpty(); file = files.Tokenize(";", curPos))
	{
		if (file.Trim().IsEmpty())
			continue;
		uint32 attrib = GetFileAttributesAndFileName(file, FullFileName);

		if (!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
			continue;

		if ((attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT) && ((nFlags & GETLATEST_REVERT) == 0))
			continue;

		if (nFlags & GETLATEST_REVERT)
		{
			char* argv[] = { FullFileName };
			m_client.SetArgv(1, argv);
			m_client.Run("revert", &m_ui);
		}

		if (nFlags & GETLATEST_ONLY_CHECK)
		{
			if (attrib & SCC_FILE_ATTRIBUTE_FOLDER)
				bRet = true;
			else
				bRet = IsFileLatestVersion(FullFileName);
		}
		else
		{
			char* argv[] = { "-f", FullFileName };
			m_client.SetArgv(2, argv);
			m_client.Run("sync", &m_ui);
			bRet = true;
		}
	}

	return bRet;
}

bool CPerforceSourceControl::GetInternalPath(const char* filename, char* outPath, int nOutPathSize)
{
	if (!filename || !outPath)
		return false;

	uint32 attrib = GetFileAttributesAndFileName(filename, 0);

	if (attrib & SCC_FILE_ATTRIBUTE_MANAGED && *m_ui.m_depotFile)
	{
		cry_strcpy(outPath, nOutPathSize, m_ui.m_depotFile);
		return true;
	}
	return false;
}

bool CPerforceSourceControl::GetOtherUser(const char* filename, char* outUser, int nOutUserSize)
{
	if (!filename || !outUser)
		return false;

	uint32 attrib = GetFileAttributesAndFileName(filename, 0);

	if (attrib & SCC_FILE_ATTRIBUTE_MANAGED && *m_ui.m_otherUser)
	{
		cry_strcpy(outUser, nOutUserSize, m_ui.m_otherUser);
		return true;
	}
	return false;
}

bool CPerforceSourceControl::History(const char* filename)
{
	if (!filename)
		return false;

	uint32 attrib = GetFileAttributesAndFileName(filename, 0);

	if (attrib & SCC_FILE_ATTRIBUTE_MANAGED && *m_ui.m_depotFile)
	{
		string commandLine = string("-cmd \"history ") + string(m_ui.m_depotFile) + "\"";
		ShellExecute(NULL, NULL, "p4v.exe", commandLine, NULL, SW_SHOW);
		return true;
	}
	return false;
}

bool CPerforceSourceControl::IsSomeTimePassed()
{
	const DWORD dwSomeTime = 10000; // 10 sec
	static DWORD dwLastTime = 0;
	DWORD dwCurTime = GetTickCount();

	if (dwCurTime - dwLastTime > dwSomeTime)
	{
		dwLastTime = dwCurTime;
		return true;
	}

	return false;
}

bool CPerforceSourceControl::GetOtherLockOwner(const char* filename, char* outUser, int nOutUserSize)
{
	if (NULL == filename || NULL == outUser)
		return false;

	uint32 attrib = GetFileAttributesAndFileName(filename, 0);
	if (attrib & SCC_FILE_ATTRIBUTE_LOCKEDBYANOTHER && *m_ui.m_lockedBy)
	{
		cry_strcpy(outUser, nOutUserSize, m_ui.m_lockedBy);
		return true;
	}

	return false;
}

ESccLockStatus CPerforceSourceControl::GetLockStatus(const char* filename)
{
	Reconnect();
	ESccLockStatus nRet = SCC_LOCK_STATUS_UNLOCKED;

	m_ui.m_e.Clear();

	char fl[MAX_PATH];
	cry_strcpy(fl, filename);
	char* argv[] = { fl };
	m_ui.Init();
	m_client.SetArgv(1, argv);
	m_client.Run("fstat", &m_ui);
	m_client.WaitTag();

	if (!m_ui.m_e.Test())
		nRet = SCC_LOCK_STATUS_UNLOCKED; // default to not locked

	if (nRet >= 0                                                                    // if no error
	    && (0 == strcmp(m_ui.m_action, "edit") || 0 == strcmp(m_ui.m_action, "add")) // checked out
	    )
	{
		nRet = m_ui.m_lockStatus;
	}

	m_ui.m_e.Clear();
	return nRet;
}

bool CPerforceSourceControl::Lock(const char* filename, int nFlags)
{
	Reconnect();
	char fullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, fullFileName);

	if (!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
		return false;

	if (!(attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
	{
		if (false == CheckOut(filename, 0))
			return false;
	}

	if (attrib & SCC_FILE_ATTRIBUTE_LOCKEDBYANOTHER)
		return false;

	char* argv[] = { fullFileName };
	m_client.SetArgv(1, argv);
	m_client.Run("lock", &m_ui);
	return !m_ui.m_e.Test();
}

bool CPerforceSourceControl::Unlock(const char* filename, int nFlags)
{
	Reconnect();
	char fullFileName[MAX_PATH];
	uint32 attrib = GetFileAttributesAndFileName(filename, fullFileName);

	if (!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
		return false;

	if (attrib & SCC_FILE_ATTRIBUTE_LOCKEDBYANOTHER)
		return false;

	if (!(attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
		return true;

	char* argv[] = { fullFileName };
	m_client.SetArgv(1, argv);
	m_client.Run("unlock", &m_ui);
	return !m_ui.m_e.Test();
}

bool CPerforceSourceControl::GetUserName(char* outUser, int nOutUserSize)
{
	cry_strcpy(outUser, nOutUserSize, m_client.GetUser().Text());
	return true;
}

bool CPerforceSourceControl::GetFileRev(const char* sFilename, int64* pHaveRev, int64* pHeadRev)
{
	Reconnect();
	bool bRet = false;

	m_ui.Init();
	m_ui.m_nFileHaveRev = -1;
	m_ui.m_nFileHeadRev = -1;

	char fl[MAX_PATH];
	cry_strcpy(fl, sFilename);
	char* argv[] = { fl };
	m_client.SetArgv(1, argv);
	m_client.Run("fstat", &m_ui);
	m_client.WaitTag();

	if (m_ui.m_nFileHaveRev >= 0 || m_ui.m_nFileHeadRev >= 0)
		bRet = true;

	if (pHaveRev)
		*pHaveRev = m_ui.m_nFileHaveRev;
	if (pHeadRev)
		*pHeadRev = m_ui.m_nFileHeadRev;

	m_ui.m_e.Clear();
	return bRet;
}

bool CPerforceSourceControl::GetRevision(const char* filename, int64 nRev, int nFlags)
{
	bool bRet = false;
	char FullFileName[MAX_PATH];

	string files = filename;
	int curPos = 0;
	string file = files.Tokenize(";", curPos);
	for (; !file.IsEmpty(); file = files.Tokenize(";", curPos))
	{
		if (file.Trim().IsEmpty())
			continue;
		uint32 attrib = GetFileAttributesAndFileName(file, FullFileName);

		cry_sprintf(&FullFileName[strlen(FullFileName)], sizeof(FullFileName) - strlen(FullFileName), "#%I64d", nRev);

		if (!(attrib & SCC_FILE_ATTRIBUTE_MANAGED))
			continue;

		if (attrib & SCC_FILE_ATTRIBUTE_CHECKEDOUT && (nFlags & GETLATEST_REVERT))
		{
			char* argv[] = { FullFileName };
			m_client.SetArgv(1, argv);
			m_client.Run("revert", &m_ui);
		}
		if (nFlags & GETLATEST_ONLY_CHECK)
		{
			if (attrib & SCC_FILE_ATTRIBUTE_FOLDER)
				bRet = true;
			else
				bRet = IsFileLatestVersion(FullFileName);
		}
		else
		{
			char* argv[] = { "-f", FullFileName };
			m_client.SetArgv(2, argv);
			m_client.Run("sync", &m_ui);
			bRet = true;
		}
	}

	return bRet;
}

