// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialBrowser.h"
#include "IDataBaseItem.h"
#include "Material.h"
#include "MaterialManager.h"
#include "ViewManager.h"
#include "Objects\BaseObject.h"
#include "Util/Clipboard.h"
#include "Dialogs/ToolbarDialog.h"
#include "MaterialImageListCtrl.h"
#include "Dialogs/SourceControlDescDlg.h"
#include <CryCore/CryCrc32.h>

#include <ISourceControl.h>
#include "Dialogs/QStringDialog.h"
#include <CryString/StringUtils.h>
#include "MaterialPickTool.h"
#include <CryThreading/IThreadManager.h>

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QSplitter>
#include <QPainter>
#include <QStyleOption>

#include "Controls/QObjectTreeWidget.h"
#include "IResourceSelectorHost.h"
#include <CrySandbox/CrySignal.h>
#include "QtUtil.h"
#include "QT/Widgets/QPreviewWidget.h"
#include "QScrollableBox.h"
#include "Controls/QuestionDialog.h"
#include "Util/MFCUtil.h"

#include "FileDialogs/EngineFileDialog.h"
#include "FileSystem/FileSystem_Enumerator.h"
#include "FileSystem/FileSystem_FileFilter.h"
#include "FileSystem/FileSystem_File.h"
#include "FilePathUtil.h"
#include "LevelEditor/LevelFileUtils.h"

#include "Dialogs/QNumericBoxDialog.h"
#include <qevent.h>


IMPLEMENT_DYNAMIC(CMaterialBrowserRecord, CTreeItemRecord)

#define IDC_MATERIAL_TREECTRL 3

//////////////////////////////////////////////////////////////////////////
#define ITEM_IMAGE_SHARED_MATERIAL          0
#define ITEM_IMAGE_SHARED_MATERIAL_SELECTED 1
#define ITEM_IMAGE_FOLDER                   2
#define ITEM_IMAGE_FOLDER_OPEN              3
#define ITEM_IMAGE_MATERIAL                 4
#define ITEM_IMAGE_MATERIAL_SELECTED        5
#define ITEM_IMAGE_MULTI_MATERIAL           6
#define ITEM_IMAGE_MULTI_MATERIAL_SELECTED  7

#define ITEM_IMAGE_OVERLAY_CGF              8
#define ITEM_IMAGE_OVERLAY_INPAK            9
#define ITEM_IMAGE_OVERLAY_READONLY         10
#define ITEM_IMAGE_OVERLAY_ONDISK           11
#define ITEM_IMAGE_OVERLAY_LOCKED           12
#define ITEM_IMAGE_OVERLAY_CHECKEDOUT       13
#define ITEM_IMAGE_OVERLAY_NO_CHECKOUT      14
//////////////////////////////////////////////////////////////////////////

enum
{
	MENU_UNDEFINED = 0,
	MENU_CUT,
	MENU_COPY,
	MENU_COPY_NAME,
	MENU_PASTE,
	MENU_EXPLORE,
	MENU_DUPLICATE,
	MENU_EXTRACT,
	MENU_RENAME,
	MENU_DELETE,
	MENU_RESET,
	MENU_ASSIGNTOSELECTION,
	MENU_SELECTASSIGNEDOBJECTS,
	MENU_NUM_SUBMTL,
	MENU_ADDNEW,
	MENU_ADDNEW_MULTI,
	MENU_CONVERT_TO_MULTI,
	MENU_SUBMTL_MAKE,
	MENU_SUBMTL_CLEAR,
	MENU_SAVE_TO_FILE,
	MENU_SAVE_TO_FILE_MULTI,
	MENU_MERGE,

	MENU_SCM_GETPATH,
	MENU_SCM_ADD,
	MENU_SCM_CHECK_IN,
	MENU_SCM_CHECK_OUT,
	MENU_SCM_UNDO_CHECK_OUT,
	MENU_SCM_GET_LATEST,
	MENU_SCM_GET_LATEST_TEXTURES,
	MENU_SCM_HISTORY,
};

///////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Scan materials in a separate thread
//////////////////////////////////////////////////////////////////////////
class CMaterialScanThread : public IThread
{
public:
	static CMaterialScanThread* Instance()
	{
		static CMaterialScanThread s_instance;
		return &s_instance;
	}

	CMaterialScanThread()
	{
		m_bNewFiles = false;
	}

	~CMaterialScanThread()
	{
		WaitForThreadToFinish();
	}

	void StartScan()
	{
		LOADING_TIME_PROFILE_SECTION;
		WaitForThreadToFinish();

		// Prepare and start new job
		{
			CryAutoCriticalSection lock(m_lock); // lock probably not needed but let's assume multiple threads can call GetLoadedFiles()
			m_filesForUser.clear();
		}
		m_files.clear();

		if (!gEnv->pThreadManager->SpawnThread(this, "MaterialFilesScanning"))
		{
			CryFatalError("Error spawning \"MaterialFilesScanning\" thread.");
		}
	}

	// Start accepting work on thread
	virtual void ThreadEntry()
	{
		m_files.reserve(5000); // Reserve space for 5000 files.
		CFileUtil::ScanDirectory("", "*.mtl", m_files, true, false, &ScanUpdateCallback);
	}

	void WaitForThreadToFinish()
	{
		// Wait until previous request finishes
		gEnv->pThreadManager->JoinThread(this, eJM_Join);
	}

	bool GetLoadedFiles(CFileUtil::FileArray& files)
	{
		CryAutoCriticalSection lock(m_lock);
		if (m_bNewFiles)
		{
			files = m_filesForUser;
			m_filesForUser.clear();
			m_bNewFiles = false;
			return true;
		}

		return false;
	}

private:
	void AddFiles()
	{

		if (!m_files.empty())
		{
			for (size_t i = 0; i < m_files.size(); i++)
			{
				m_files[i].filename = PathUtil::ToUnixPath(m_files[i].filename.GetString()).c_str();
			}
			{
				CryAutoCriticalSection lock(m_lock);
				m_filesForUser.insert(m_filesForUser.begin(), m_files.begin(), m_files.end());
				m_bNewFiles = true;
			}
			m_files.clear();
		}
	}

	static bool ScanUpdateCallback(const string& msg)
	{
		Instance()->AddFiles();
		return true;
	}

private:
	CryCriticalSection   m_lock;
	CFileUtil::FileArray m_filesForUser;
	CFileUtil::FileArray m_files;
	bool                 m_bNewFiles;
};

//////////////////////////////////////////////////////////////////////////
// CMaterialScanner
//////////////////////////////////////////////////////////////////////////

class CMaterialScanner : public IEditorNotifyListener
{
public:
	CMaterialScanner()
	{
		GetIEditorImpl()->RegisterNotifyListener(this);

		QTimer* pTimer = new QTimer();
		QObject::connect(pTimer, &QTimer::timeout, [this]()
		{
			CFileUtil::FileArray newFiles;
			if (CMaterialScanThread::Instance()->GetLoadedFiles(newFiles))
			{
			  m_files.insert(m_files.begin(), newFiles.begin(), newFiles.end());
			  filesAddedSignal(newFiles);
			}
		});
		pTimer->start(250);
	}

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event)
	{
		switch (event)
		{
		case eNotify_OnClearLevelContents:
			Clear();
			break;
		case eNotify_OnEndSceneOpen:
			StartScan();
			break;
		}
	}

	void Clear()
	{
		m_files.clear();
		fileCacheClearedSignal();
	}

	void StartScan()
	{
		m_files.clear();
		fileCacheClearedSignal();
		CMaterialScanThread::Instance()->StartScan();
	}

	static CMaterialScanner* GetInstance()
	{
		static CMaterialScanner s_instance;
		return &s_instance;
	}

	const CFileUtil::FileArray& GetFiles() const { return m_files; }

public:
	CCrySignal<void(const CFileUtil::FileArray&)> filesAddedSignal;
	CCrySignal<void()>                            fileCacheClearedSignal;

private:
	CFileUtil::FileArray m_files;
};

//////////////////////////////////////////////////////////////////////////
// QTexturePreview
//////////////////////////////////////////////////////////////////////////

QTexturePreview::QTexturePreview(const QString& path, QWidget* pParent /*= nullptr*/)
{
	CImageEx previewImage;
	LoadTexture(path, previewImage);
	previewImage.SwapRedAndBlue();

	auto image = QImage((const uchar*)previewImage.GetData(), previewImage.GetWidth(), previewImage.GetHeight(), QImage::Format_ARGB32);
	m_originalPixmap = QPixmap::fromImage(image);

	m_maxSize = QSize(previewImage.GetWidth(), previewImage.GetHeight());
	setPixmap(m_originalPixmap);
	setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	setAlignment(Qt::AlignCenter);
}

QTexturePreview::QTexturePreview(const QString& path, QSize size, QWidget* pParent /*= nullptr*/)
{
	CImageEx previewImage;
	LoadTexture(path, previewImage);
	previewImage.SwapRedAndBlue();

	CImageEx scaledPreview;
	scaledPreview.Allocate(size.width(), size.height());
	CImageUtil::ScaleToFit(previewImage, scaledPreview);

	auto image = QImage((const uchar*)scaledPreview.GetData(), size.width(), size.height(), QImage::Format_ARGB32);
	m_originalPixmap = QPixmap::fromImage(image);

	m_maxSize = size;
	setPixmap(m_originalPixmap);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	setAlignment(Qt::AlignCenter);
}

void QTexturePreview::resizeEvent(QResizeEvent *event)
{
	QSize newSize = event->size().boundedTo(m_maxSize);
	setPixmap(m_originalPixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::FastTransformation));
}

QSize QTexturePreview::sizeHint() const
{
	return m_maxSize;
}

bool QTexturePreview::IsPower2(int n)
{
	return (((n - 1) & n) == 0);
}

void QTexturePreview::LoadTexture(const QString& fileName, CImageEx& image)
{
	// Save the filename
	string relPath = PathUtil::AbsolutePathToCryPakPath(fileName.toStdString().c_str());
	if (relPath.empty())
		relPath = fileName.toStdString().c_str();

	CImageEx texture;
	CSize textureDimensions;
	CDC layerTexPrev;
	static const char* szReplaceMe = "%ENGINE%/EngineAssets/TextureMsg/ReplaceMe.tif";
	bool bQualityLoss;
	bool bError = false;

	// if file doesn't exist don't try to load it
	if (!CFileUtil::FileExists(CString(PathUtil::GamePathToCryPakPath(relPath))) ||
	    !CImageUtil::LoadImage(relPath, texture, &bQualityLoss))
	{
		// check if this a .tif file then try to load a .dds file
		if (strcmp(PathUtil::GetExt(relPath), "tif") != 0 ||
			!CImageUtil::LoadImage(PathUtil::ReplaceExtension(relPath, "dds"), texture, &bQualityLoss))
		{
			CryLog("Error loading layer texture (%s)...", (const char*)relPath);
			bError = true;
		}
	}

	if (bError && !CImageUtil::LoadImage(szReplaceMe, texture, &bQualityLoss))
	{
		texture.Allocate(4, 4);
		texture.Fill(0xff);
	}

	// Store the size
	textureDimensions = CSize(texture.GetWidth(), texture.GetHeight());

	CBitmap bmpLoad;
	CDC dcLoad;
	// Create the DC for blitting from the loaded bitmap
	VERIFY(dcLoad.CreateCompatibleDC(NULL));

	CImageEx inverted;
	inverted.Allocate(texture.GetWidth(), texture.GetHeight());
	for (int y = 0; y < texture.GetHeight(); y++)
	{
		for (int x = 0; x < texture.GetWidth(); x++)
		{
			uint32 val = texture.ValueAt(x, y);
			inverted.ValueAt(x, y) = 0xFF000000 | RGB(GetBValue(val), GetGValue(val), GetRValue(val));
		}
	}

	bmpLoad.CreateBitmap(texture.GetWidth(), texture.GetHeight(), 1, 32, inverted.GetData());

	// Select it into the DC
	dcLoad.SelectObject(&bmpLoad);

	image.Allocate(texture.GetWidth(), texture.GetHeight());
	for (int y = 0; y < texture.GetHeight(); y++)
	{
		for (int x = 0; x < texture.GetWidth(); x++)
		{
			uint32 val = texture.ValueAt(x, y);
			ColorB& colorB = *(ColorB*)&val;
			ColorF colorF(colorB.r / 255.f, colorB.g / 255.f, colorB.b / 255.f);
			colorB = colorF;

			image.ValueAt(x, y) = val;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// QMaterialPreview
//////////////////////////////////////////////////////////////////////////

QMaterialPreview::QMaterialPreview(const QString& file /* = QString()*/, QWidget* pParent /* = nullptr*/)
	: QWidget(pParent)
{
	m_pPreviewWidget = new QPreviewWidget();
	m_pPreviewWidget->SetGrid(false);
	m_pPreviewWidget->SetAxis(false);
	m_pPreviewWidget->EnableMaterialPrecaching(true);
	m_pPreviewWidget->LoadFile("%EDITOR%/Objects/mtlsphere.cgf");
	m_pPreviewWidget->SetCameraLookAt(1.6f, Vec3(0.1f, -1.0f, -0.1f));
	m_pPreviewWidget->EnableUpdate(true);
	m_pPreviewImagesLayout = new QHBoxLayout();
	m_pPreviewImagesLayout->setSpacing(2);
	m_pPreviewImagesLayout->setMargin(0);
	QWidget* pPreviewImagesContainer = new QWidget(this);
	pPreviewImagesContainer->setLayout(m_pPreviewImagesLayout);
	QScrollArea* pScrollContainer = new QScrollArea(this);
	pScrollContainer->setWidget(pPreviewImagesContainer);
	pScrollContainer->setWidgetResizable(true);

	m_pPreviewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pScrollContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	pScrollContainer->setMinimumHeight(160);

	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->addWidget(m_pPreviewWidget);
	pMainLayout->addWidget(pScrollContainer);
	pMainLayout->setAlignment(Qt::AlignTop);

	setLayout(pMainLayout);

	if (!file.isEmpty())
		PreviewItem(file);

	connect(this, &QMaterialPreview::widgetShown, this, &QMaterialPreview::validateSize, Qt::QueuedConnection);

	layout()->invalidate();
	layout()->setGeometry(layout()->geometry());
}

QMaterialPreview::~QMaterialPreview()
{

}

void QMaterialPreview::PreviewItem(const QString& path)
{
	QLayoutItem* item;
	while (item = m_pPreviewImagesLayout->takeAt(0))
	{
		item->widget()->deleteLater();
		delete item;
	}

	CMaterialManager* pManager = GetIEditorImpl()->GetMaterialManager();
	if (pManager)
	{
		CMaterial* pNewMaterial = pManager->LoadMaterial(path.toStdString().c_str());
		IStatObj* pObject = m_pPreviewWidget->GetObject();
		pObject->SetMaterial(pNewMaterial->GetMatInfo());
		CUsedResources resources;
		pNewMaterial->GatherUsedResources(resources);

		for (const string& fileName : resources.files)
		{
			auto pPreview = new QTexturePreview(fileName.GetString(), QSize(128, 128), this);
			m_pPreviewImagesLayout->addWidget(pPreview);
			m_pPreviewImagesLayout->setAlignment(pPreview, Qt::AlignLeft);
		}

		int count = pNewMaterial->GetSubMaterialCount();
		for (int i = 0; i < count; ++i)
		{
			CMaterial* pSubMaterial = pNewMaterial->GetSubMaterial(i);
			QPreviewWidget* pSubMaterialPreview = new QPreviewWidget();
			pSubMaterialPreview->SetGrid(false);
			pSubMaterialPreview->SetAxis(false);
			pSubMaterialPreview->SetObject(pObject->Clone(true, false, true));
			pSubMaterialPreview->GetObject()->SetMaterial(pSubMaterial->GetMatInfo());
			pSubMaterialPreview->setMinimumWidth(120);
			m_pPreviewImagesLayout->addWidget(pSubMaterialPreview);
			pSubMaterialPreview->SetCameraLookAt(1.6f, Vec3(0.1f, -1.0f, -0.1f));
			pSubMaterialPreview->EnableUpdate(true);
		}

		m_pPreviewImagesLayout->addStretch();
		m_pPreviewWidget->update();
	}
}

void QMaterialPreview::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void QMaterialPreview::showEvent(QShowEvent* pEvent)
{
	QWidget::showEvent(pEvent);

	// since Qt 5.6 there is a bug where the internal coordinates calculations are not correct
	// to force the correct recalculations we have to put a msg to the queue and do the recalculation there
	widgetShown();
}

void QMaterialPreview::validateSize()
{
	layout()->invalidate();
	layout()->setGeometry(layout()->geometry());
}

//////////////////////////////////////////////////////////////////////////
// CMaterialToolBar
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CMaterialToolBar::CMaterialToolBar() :
	m_pEdit(0),
	m_fIdleFilterTextTime(0.0f),
	m_curFilter(eFilter_Materials)
{
}

//////////////////////////////////////////////////////////////////////////
void CMaterialToolBar::Create(CWnd* pParentWnd)
{
	VERIFY(CreateToolBar(WS_VISIBLE | WS_CHILD | CBRS_TOOLTIPS | CBRS_GRIPPER, pParentWnd, AFX_IDW_TOOLBAR));
	VERIFY(LoadToolBar(IDR_MATERIAL_BROWSER));
	SetFlags(xtpFlagAlignTop | xtpFlagStretched);
	EnableCustomization(FALSE);

	CXTPControl* pCtrl = GetControls()->FindControl(xtpControlButton, ID_MATERIAL_BROWSER_FILTER_TEXT, TRUE, FALSE);
	if (pCtrl)
	{
		int nIndex = pCtrl->GetIndex();
		m_pEdit = (CXTPControlEdit*)GetControls()->SetControlType(nIndex, xtpControlEdit);
		if (m_pEdit)
		{
			m_pEdit->SetFlags(xtpFlagManualUpdate);
			m_pEdit->SetWidth(120);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CMaterialToolBar::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (HIWORD(wParam) == EN_CHANGE)
	{
		m_fIdleFilterTextTime = gEnv->pTimer->GetCurrTime();
	}

	return __super::OnCommand(wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
const CString& CMaterialToolBar::GetFilterText(bool* pOutIsNew)
{
	if (pOutIsNew)
		*pOutIsNew = false;

	// If user types fast do not proceed for every character. Delay is 0.3 (0.3f) seconds
	if (m_fIdleFilterTextTime > 0.0f && gEnv->pTimer->GetCurrTime() - m_fIdleFilterTextTime > 0.3f)
	{
		if (m_pEdit)
		{
			CString filterText = m_pEdit->GetEditText();
			if (m_filterText != filterText)
			{
				m_filterText = filterText;
				if (pOutIsNew)
					*pOutIsNew = true;
			}
		}
		m_fIdleFilterTextTime = 0.0f;
	}

	return m_filterText;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialToolBar::Reset()
{
	if (m_pEdit)
		m_pEdit->SetEditText("");
	m_filterText = "";
	m_fIdleFilterTextTime = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialToolBar::OnFilter()
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING | (m_curFilter == eFilter_Materials ? MF_CHECKED : 0), eFilter_Materials, "Material names");
	menu.AppendMenu(MF_STRING | (m_curFilter == eFilter_Submaterials ? MF_CHECKED : 0), eFilter_Submaterials, "Submaterial names");
	menu.AppendMenu(MF_STRING | (m_curFilter == eFilter_Textures ? MF_CHECKED : 0), eFilter_Textures, "Texture names");
	menu.AppendMenu(MF_STRING | (m_curFilter == eFilter_Materials_And_Textures ? MF_CHECKED : 0), eFilter_Materials_And_Textures, "Materials and Textures");

	CPoint point;
	CXTPControl* pCtrl = GetControls()->FindControl(xtpControlButton, ID_MATERIAL_BROWSER_FILTER, TRUE, FALSE);
	if (pCtrl)
	{
		CRect rc, rcBtn;
		rcBtn = pCtrl->GetRect();
		GetWindowRect(&rc);
		point = CPoint(rc.left + rcBtn.left, rc.top + rcBtn.bottom);
	}
	else
	{
		GetCursorPos(&point);
	}

	int cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, point.x, point.y, this);
	switch (cmd)
	{
	case eFilter_Materials:
		m_curFilter = eFilter_Materials;
		break;
	case eFilter_Submaterials:
		m_curFilter = eFilter_Submaterials;
		break;
	case eFilter_Textures:
		m_curFilter = eFilter_Textures;
		break;
	case eFilter_Materials_And_Textures:
		m_curFilter = eFilter_Materials_And_Textures;
		break;
	default:
		m_curFilter = eFilter_Materials;
		break;
	}
	;
}

//////////////////////////////////////////////////////////////////////////
// CMaterialBrowserCtrl
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMaterialBrowserCtrl, CWnd)
ON_WM_SIZE()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_COMMAND(ID_MATERIAL_BROWSER_RELOAD, OnReloadItems)
ON_COMMAND(ID_MATERIAL_BROWSER_FILTER, OnFilter)
ON_COMMAND(ID_MATERIAL_BROWSER_CHECKEDOUT, OnShowCheckedOut)
ON_UPDATE_COMMAND_UI(ID_MATERIAL_BROWSER_CHECKEDOUT, OnUpdateShowCheckedOut)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserCtrl::CMaterialBrowserCtrl()
{
	m_bIgnoreSelectionChange = false;
	m_bItemsValid = true;

	// Load cursors.
	m_hCursorDefault = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	m_hCursorNoDrop = AfxGetApp()->LoadCursor(IDC_NODROP);
	m_hCursorCreate = AfxGetApp()->LoadCursor(IDC_HIT_CURSOR);
	m_hCursorReplace = AfxGetApp()->LoadCursor(IDC_HAND_INTERNAL);

	m_pMatMan = GetIEditorImpl()->GetMaterialManager();
	m_pMatMan->AddListener(this);
	m_pListener = NULL;

	m_viewType = VIEW_LEVEL;
	m_pMaterialImageListCtrl = NULL;
	m_pLastActiveMultiMaterial = 0;

	m_bNeedReload = false;

	m_bHighlightMaterial = false;
	m_timeOfHighlight = 0;

	m_fIdleSaveMaterialTime = 0.0f;
	m_bShowOnlyCheckedOut = false;

	GetIEditorImpl()->RegisterNotifyListener(this);
	CMaterialScanner* pScanner = CMaterialScanner::GetInstance();
	LoadFromFiles(pScanner->GetFiles());

	pScanner->filesAddedSignal.Connect(this, &CMaterialBrowserCtrl::LoadFromFiles);
	pScanner->fileCacheClearedSignal.Connect(this, &CMaterialBrowserCtrl::ClearItems);
}

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserCtrl::~CMaterialBrowserCtrl()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);

	CMaterialScanner* const pScanner = CMaterialScanner::GetInstance();
	pScanner->filesAddedSignal.DisconnectObject(this);
	pScanner->fileCacheClearedSignal.DisconnectObject(this);

	CMaterial* pCurrentMtl = GetCurrentMaterial();
	if (pCurrentMtl != NULL && pCurrentMtl->IsModified())
		pCurrentMtl->Save();

	m_pMaterialImageListCtrl = NULL;
	m_pMatMan->RemoveListener(this);
	ClearItems();

	if (m_bHighlightMaterial)
	{
		m_pMatMan->SetHighlightedMaterial(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	CRect rcClient;
	GetClientRect(rcClient);
	if (!m_tree.m_hWnd)
		return;

	DWORD dwMode = LM_HORZ | LM_HORZDOCK | LM_STRETCH | LM_COMMIT;
	CSize sz = m_wndToolBar.CalcDockingLayout(32000, dwMode);

	CRect rctb = rcClient;
	rctb.bottom = rctb.top + sz.cy;
	m_wndToolBar.MoveWindow(rctb, FALSE);

	rcClient.top = rctb.bottom + 1;

	CRect rctree = rcClient;
	m_tree.MoveWindow(rctree, FALSE);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMaterialBrowserCtrl::Create(const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	LPCSTR lpzClass = AfxRegisterWndClass(CS_VREDRAW | CS_HREDRAW, ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)GetStockObject(NULL_BRUSH), NULL);
	BOOL res = CreateEx(0, lpzClass, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP, rect, pParentWnd, nID);
	VERIFY(res);

	m_wndToolBar.Create(this);

	CRect rc;
	GetClientRect(rc);

	//////////////////////////////////////////////////////////////////////////
	m_tree.SetMtlBrowser(this);
	m_tree.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 200, 300), this, IDC_MATERIAL_TREECTRL);
	m_tree.EnableAutoNameGrouping(true, ITEM_IMAGE_FOLDER);

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnReloadItems()
{
	m_wndToolBar.Reset();
	ReloadItems(m_viewType);
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialBrowserCtrl::ShowCheckedOutRecursive(CXTPReportRecords* pRecords)
{
	bool isSomeVisible = false;
	int num = pRecords->GetCount();
	for (int i = 0; i < num; i++)
	{
		CMaterialBrowserRecord* pRecord = (CMaterialBrowserRecord*)pRecords->GetAt(i);
		if (!pRecord || pRecord->nSubMtlSlot >= 0) // skip submaterials
			continue;
		bool isChildVisible = ShowCheckedOutRecursive(pRecord->GetChilds());
		bool isVisible = false;
		bool isMaterial = false;
		if (!m_bShowOnlyCheckedOut || isChildVisible)
		{
			isVisible = true;
		}
		else if (!pRecord->materialName.IsEmpty())
		{
			string filename = m_pMatMan->MaterialToFilename(pRecord->materialName.GetString(), false);
			CCryFile cryfile;
			if (cryfile.Open(filename, "rb"))
			{
				if (!cryfile.IsInPak())
				{
					uint32 scFileAttributes = CFileUtil::GetAttributes(filename);
					if ((scFileAttributes & SCC_FILE_ATTRIBUTE_MANAGED) && (scFileAttributes & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
					{
						isVisible = true;
						isMaterial = true;
					}
				}
			}
		}
		pRecord->SetVisible(isVisible);

		if (m_bShowOnlyCheckedOut && isVisible)
		{
			if (isMaterial)
			{
				for (int i = 0; i < pRecord->GetChilds()->GetCount(); i++)
					pRecord->GetChilds()->GetAt(i)->SetVisible(TRUE);
			}
			else
			{
				if (!pRecord->IsExpanded())
					pRecord->SetExpanded(TRUE);
			}
		}

		if (isVisible)
			isSomeVisible = true;
	}
	return isSomeVisible;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnShowCheckedOut()
{
	m_bShowOnlyCheckedOut = !m_bShowOnlyCheckedOut;
	CWaitCursor wait;
	m_wndToolBar.Reset();
	ShowCheckedOutRecursive(m_tree.GetRecords());
	m_tree.Populate();
	m_tree.RedrawControl();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnFilter()
{
	m_wndToolBar.OnFilter();

	const CString& filterText = m_wndToolBar.GetFilterText();
	if (!filterText.IsEmpty())
		m_tree.SetFilterText(filterText);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnUpdateShowCheckedOut(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bShowOnlyCheckedOut ? 1 : 0);
	pCmdUI->Enable(GetIEditorImpl()->IsSourceControlAvailable() ? TRUE : FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ClearItems()
{
	m_bIgnoreSelectionChange = true;
	m_items.clear();

	m_tree.SelectItem(0, 0);
	m_tree.DeleteAllItems();

	if (m_pMaterialImageListCtrl)
		m_pMaterialImageListCtrl->DeleteAllItems();
	m_pLastActiveMultiMaterial = NULL;
	m_bShowOnlyCheckedOut = false;
	GetIEditorImpl()->GetMaterialManager()->SetCurrentMaterial(nullptr);

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ReloadItems(EViewType viewType)
{
	CWaitCursor wait;

	m_bShowOnlyCheckedOut = false;

	CString prevSelection = GetSelectedMaterialID();

	m_tree.BeginUpdate();

	ClearItems();

	m_viewType = viewType;
	switch (m_viewType)
	{
	case VIEW_LEVEL:
		ReloadLevelItems();
		break;
	case VIEW_ALL:
	default:
		ReloadAllItems();
		break;
	}

	m_tree.EndUpdate();
	m_tree.Populate();
	m_bItemsValid = true;

	m_bNeedReload = false;

	if (!prevSelection.IsEmpty())
	{
		SelectItemByName(prevSelection, "");
	}
	else
	{
		SelectItem(m_pMatMan->GetSelectedItem(), m_pMatMan->GetSelectedParentItem());
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ReloadLevelItems()
{
	m_bIgnoreSelectionChange = true;

	m_pMatMan->SetCurrentMaterial(NULL);
	IMaterialManager* pMtlMan = GetIEditorImpl()->Get3DEngine()->GetMaterialManager();

	IDataBaseItemEnumerator* pEnum = m_pMatMan->GetItemEnumerator();
	for (IDataBaseItem* pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
	{
		if (pMtlMan->FindMaterial(pItem->GetName()) != NULL)
		{
			AddItemToTree(pItem, false);
		}
	}
	pEnum->Release();

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::AddItemToTree(IDataBaseItem* pItem, bool bUpdate)
{
	if (!pItem)
		return;

	m_bIgnoreSelectionChange = true;
	CMaterialBrowserRecord* pRecord = InsertItemToTree(pItem);
	if (bUpdate)
	{
		//m_tree.UpdateRecord(pRecord,TRUE); // Crashes
	}
	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserRecord* CMaterialBrowserCtrl::InsertItemToTree(IDataBaseItem* pItem, int nSubMtlSlot, CMaterialBrowserRecord* pParentRecord)
{
	CMaterial* pMtl = (CMaterial*)pItem;

	CMaterialBrowserRecord* pRecord = new CMaterialBrowserRecord;
	pRecord->materialName = pMtl->GetName();
	pRecord->materialNameCrc32 = MaterialNameToCrc32(pRecord->materialName);
	pRecord->pMaterial = pMtl;
	pRecord->nSubMtlSlot = nSubMtlSlot;

	bool alreadyInserted = false;

	if (pMtl->IsMultiSubMaterial())
	{
		for (Items::iterator it = m_items.begin(); it != m_items.end(); ++it)
		{
			CMaterialBrowserRecord* pInsertedRecord = *it;
			if (pInsertedRecord->materialNameCrc32 == pRecord->materialNameCrc32)
			{
				alreadyInserted = true;
				break;
			}
		}
	}

	if (alreadyInserted == false)
	{
		m_items.insert(pRecord);
		pRecord->CreateItems();
		m_tree.AddTreeRecord(pRecord, pParentRecord);
	}

	if (pMtl->IsMultiSubMaterial())
	{
		ReloadTreeSubMtls(pRecord, false);
	}
	return pRecord;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ReloadTreeSubMtls(CMaterialBrowserRecord* pRecord, bool bForceCtrlUpdate)
{
	if (pRecord && pRecord->HasChildren())
	{
		for (int i = pRecord->GetChildCount() - 1; i >= 0; --i)
		{
			CMaterialBrowserRecord* pChildRecord = (CMaterialBrowserRecord*)pRecord->GetChild(i);
			m_items.erase(pChildRecord);
		}
		pRecord->GetChilds()->RemoveAll();
	}

	if (pRecord)
	{
		CMaterial* pMtl = pRecord->pMaterial;
		if (pMtl && pMtl->IsMultiSubMaterial())
		{
			for (int i = 0; i < pMtl->GetSubMaterialCount(); i++)
			{
				CMaterial* pSubMtl = pMtl->GetSubMaterial(i);
				if (pSubMtl)
				{
					InsertItemToTree(pSubMtl, i, pRecord);
				}
			}
			if (bForceCtrlUpdate)
			{
				m_bItemsValid = false;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::RemoveItemFromTree(CMaterialBrowserRecord* pRecord)
{
	// Remove itself from Records
	m_tree.RemoveRecordEx(pRecord);
	m_bItemsValid = false;

	m_items.erase(pRecord);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ReloadAllItems()
{
	// First add all items already loaded in the level.
	CMaterialScanner::GetInstance()->StartScan();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::TryLoadRecordMaterial(CMaterialBrowserRecord* pRecord)
{
	if (!pRecord)
		return;

	// If material already loaded ignore.
	if (pRecord->pMaterial || pRecord->bUpdated || pRecord->IsGroup())
		return;

	m_bIgnoreSelectionChange = true;
	pRecord->bUpdated = true;
	// Try to load material for it.
	pRecord->pMaterial = m_pMatMan->LoadMaterial(pRecord->materialName, false);

	m_tree.UpdateItemState(pRecord);

	if (pRecord->pMaterial != NULL && pRecord->pMaterial->IsMultiSubMaterial())
	{
		ReloadTreeSubMtls(pRecord);
	}
	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::TickRefreshMaterials()
{
	CFileUtil::FileArray files;

	if (!m_bItemsValid)
	{
		PopulateItems();
	}

	const int nMaxUpdatesAtOnce = 1;

	bool bAnyUpdated = false;
	int nNumLoaded = 0;
	//////////////////////////////////////////////////////////////////////////
	//
	//////////////////////////////////////////////////////////////////////////
	CXTPReportRows* pRows = m_tree.GetRows();
	for (int i = 0, num = pRows->GetCount(); i < num; i++)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		CMaterialBrowserRecord* pRecord = m_tree.RowToRecord(pRow);
		if (pRecord && !pRecord->bUpdated && !pRecord->IsGroup() && pRecord->IsVisible())
		{
			if (!pRecord->pMaterial && !pRecord->materialName.IsEmpty())
			{
				TryLoadRecordMaterial(pRecord);
			}
			else
			{
				m_tree.UpdateItemState(pRecord);
				pRecord->bUpdated = true;
			}
			bAnyUpdated = true;
			if (nNumLoaded++ > nMaxUpdatesAtOnce)
				break;
		}
	}
	if (bAnyUpdated)
	{
		m_tree.RedrawControl();
	}

	if (m_bHighlightMaterial)
	{
		uint32 t = GetTickCount();
		if ((t - m_timeOfHighlight) > 300)
		{
			m_bHighlightMaterial = false;
			m_pMatMan->SetHighlightedMaterial(0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::LoadFromFiles(const CFileUtil::FileArray& files)
{
	m_bIgnoreSelectionChange = true;
	m_bShowOnlyCheckedOut = false;

	m_tree.BeginUpdate();
	CMaterial* pCurrentMaterial = GetIEditorImpl()->GetMaterialManager()->GetCurrentMaterial();

	CString itemName;
	for (int i = 0; i < files.size(); i++)
	{
		string name = m_pMatMan->FilenameToMaterial(files[i].filename.GetString());

		CMaterialBrowserRecord* pRecord = new CMaterialBrowserRecord;
		pRecord->materialName = name.GetString();
		pRecord->materialNameCrc32 = MaterialNameToCrc32(name);
		m_items.insert(pRecord);

		if (pCurrentMaterial && pCurrentMaterial->GetName() == name)
		{
			m_bIgnoreSelectionChange = false;
			SelectItem(pCurrentMaterial, nullptr);
			m_bIgnoreSelectionChange = true;
		}

		pRecord->CreateItems();
		m_tree.AddTreeRecord(pRecord, 0);
	}

	m_tree.EndUpdate();

	m_bItemsValid = false;

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		{
			TickRefreshMaterials();

			bool isNew;
			const CString& filterText = m_wndToolBar.GetFilterText(&isNew);
			if (isNew)
			{
				m_bShowOnlyCheckedOut = false;
				m_tree.SetFilterText(filterText);
			}

			// Idle save material
			// If material is changed in less than 1 second (1.0f) it is saved only one time after last change
			if (m_fIdleSaveMaterialTime > 0.0001f && gEnv->pTimer->GetCurrTime() - m_fIdleSaveMaterialTime > 1.0f)
			{
				CMaterial* pMtl = GetCurrentMaterial();
				if (pMtl)
					pMtl->Save();
				m_fIdleSaveMaterialTime = 0.0f;
			}
		}
		break;
	case eNotify_OnClearLevelContents:
		{
			SetSelectedItem(0, 0, true);
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::IdleSaveMaterial()
{
	m_fIdleSaveMaterialTime = gEnv->pTimer->GetCurrTime();
}

/*
   //////////////////////////////////////////////////////////////////////////
   void CMaterialBrowserCtrl::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
   {
   if(GetAsyncKeyState( VK_CONTROL ))
   {
    NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
 * pResult = 0;

    HTREEITEM hItem = pNMTreeView->itemNew.hItem;

    CMaterialBrowserRecord *pDraggedItem = (CMaterialBrowserRecord*)pNMTreeView->itemNew.lParam;
    if (!pDraggedItem)
      return;

    CMaterial *pMtl = pDraggedItem->pMaterial;
    if (!pMtl)
    {
      return;
    }

    m_treeCtrl.Select( hItem,TVGN_CARET );

    // Calculate the offset to the hotspot
    CPoint offsetPt(-10,-10);   // Initialize a default offset

    m_hDropItem = 0;
    m_dragImage = m_treeCtrl.CreateDragImage( hItem );
    if (m_dragImage)
    {
      m_hDraggedItem = hItem;
      m_hDropItem = hItem;

      //m_dragImage->BeginDrag(0, CPoint(-10, -10));
      m_dragImage->BeginDrag(0, offsetPt);

      POINT pt = pNMTreeView->ptDrag;
      ClientToScreen( &pt );
      m_dragImage->DragEnter(AfxGetMainWnd(), pt);
      SetCapture();

      GetIEditorImpl()->EnableUpdate( false );
    }
   }

 * pResult = 0;
   }
 */

/*
   //////////////////////////////////////////////////////////////////////////
   void CMaterialBrowserCtrl::OnMouseMove(UINT nFlags, CPoint point)
   {
   if (m_dragImage)
   {
    CPoint p;

    p = point;
    ClientToScreen( &p );
    m_treeCtrl.ScreenToClient( &p );

    SetCursor( m_hCursorDefault );
    TVHITTESTINFO hit;
    ZeroStruct(hit);
    hit.pt = p;
    HTREEITEM hHitItem = m_treeCtrl.HitTest( &hit );
    if (hHitItem)
    {
      if (m_hDropItem != hHitItem)
      {
        if (m_hDropItem)
          m_treeCtrl.SetItem( m_hDropItem,TVIF_STATE,0,0,0,0,TVIS_DROPHILITED,0 );
        // Set state of this item to drop target.
        m_treeCtrl.SetItem( hHitItem,TVIF_STATE,0,0,0,TVIS_DROPHILITED,TVIS_DROPHILITED,0 );
        m_hDropItem = hHitItem;
        //m_treeCtrl.Invalidate();
      }
    }

    // Check if this is not SubMaterial.
    CMaterialBrowserRecord *pRecord = GetItemInfo(m_hDraggedItem);
    CMaterial *pMtl = GetMaterial(m_hDraggedItem);
    if (!pMtl || pRecord->nSubMtlSlot >= 0)
    {
      SetCursor( m_hCursorNoDrop );
    }
    else
    {
      CRect rc;
      AfxGetMainWnd()->GetWindowRect( rc );
      p = point;
      ClientToScreen( &p );
      p.x -= rc.left;
      p.y -= rc.top;
      m_dragImage->DragMove( p );

      // Check if can drop here.
      {
        CPoint p;
        GetCursorPos( &p );
        CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint( p );
        if (viewport)
        {
          CPoint vp = p;
          viewport->ScreenToClient(&vp);
          HitContext hit;
          if (viewport->HitTest( vp,hit ))
          {
            if (hit.object)
            {
              SetCursor( m_hCursorReplace );
            }
          }
          else
          {
            if (viewport->CanDrop(vp,pMtl))
              SetCursor( m_hCursorReplace );
          }
        }
      }
    }
   }

   __super::OnMouseMove(nFlags, point);
   }
 */

/*
   //////////////////////////////////////////////////////////////////////////
   void CMaterialBrowserCtrl::OnLButtonUp(UINT nFlags, CPoint point)
   {
   //CXTResizeDialog::OnLButtonUp(nFlags, point);

   if (m_hDropItem)
   {
    m_treeCtrl.SetItem( m_hDropItem,TVIF_STATE,0,0,0,0,TVIS_DROPHILITED,0 );
    m_hDropItem = 0;
   }

   if (m_dragImage)
   {
    CPoint p;
    GetCursorPos( &p );

    GetIEditorImpl()->EnableUpdate( true );

    m_dragImage->DragLeave( AfxGetMainWnd() );
    m_dragImage->EndDrag();
    delete m_dragImage;
    m_dragImage = 0;
    ReleaseCapture();

    CPoint treepoint = p;
    m_treeCtrl.ScreenToClient( &treepoint );

    TVHITTESTINFO hit;
    ZeroStruct(hit);
    hit.pt = treepoint;
    HTREEITEM hHitItem = m_treeCtrl.HitTest( &hit );
    if (hHitItem)
    {
      DropToItem( hHitItem,m_hDraggedItem );
      m_hDraggedItem = 0;
      return;
    }

    // Check if this is not SubMaterial.
    CMaterialBrowserRecord *pRecord = GetItemInfo(m_hDraggedItem);
    if (pRecord->nSubMtlSlot < 0)
    {
      CWnd *wnd = WindowFromPoint( p );

      CUndo undo( "Assign Material" );

      CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint( p );
      if (viewport)
      {
        CPoint vp = p;
        viewport->ScreenToClient(&vp);
        // Drag and drop into one of views.
        // Start object creation.
        HitContext hit;
        if (viewport->HitTest( vp,hit ))
        {
          if (hit.object)
          {
            CMaterial *pMtl = GetMaterial(m_hDraggedItem);
            if (pMtl && !pMtl->IsPureChild())
            {
              hit.object->SetMaterial(pMtl);
            }
          }
        }
        else
        {
          CMaterial *pMtl = GetMaterial(m_hDraggedItem);
          viewport->Drop(vp,pMtl);
        }
      }
      else
      {
      }
    }
   }
   m_hDraggedItem = 0;

   __super::OnLButtonUp(nFlags, point);
   }
 */

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::DropToItem(CMaterialBrowserRecord* pTrgItem, CMaterialBrowserRecord* pSrcItem)
{
	if (pTrgItem && pSrcItem)
	{
		// Get multisub parent.
		CMaterialBrowserRecord* pTrgParentItem = pTrgItem->GetParent();
		CMaterialBrowserRecord* pSrcParentItem = pSrcItem->GetParent();

		_smart_ptr<CMaterial> pTrgMtl = pTrgItem->pMaterial;
		_smart_ptr<CMaterial> pSrcMtl = pSrcItem->pMaterial;
		if (pSrcMtl != NULL && !pSrcMtl->IsMultiSubMaterial())
		{
			if (pTrgParentItem == pSrcParentItem)
			{
				// We are possibly dropping within same multi sub object.
				if (pTrgItem->nSubMtlSlot >= 0 && pSrcItem->nSubMtlSlot >= 0)
				{
					if (pTrgParentItem && pTrgParentItem->pMaterial != NULL && pTrgParentItem->pMaterial->IsMultiSubMaterial())
					{
						// Swap materials.
						_smart_ptr<CMaterial> pParentMultiMaterial = pTrgParentItem->pMaterial;
						int nSrcSlot = pSrcItem->nSubMtlSlot;
						int nTrgSlot = pTrgItem->nSubMtlSlot;
						// This invalidates item infos.
						SetSubMaterial(pTrgParentItem, nSrcSlot, pTrgMtl, false);
						SetSubMaterial(pTrgParentItem, nTrgSlot, pSrcMtl, true);
						return;
					}
				}
			}
			else if (!pSrcMtl->IsPureChild())
			{
				// Not in same branch.
				if (pTrgItem->nSubMtlSlot >= 0)
				{
					if (pTrgParentItem && pTrgParentItem->pMaterial != NULL && pTrgParentItem->pMaterial->IsMultiSubMaterial())
					{
						SetSubMaterial(pTrgParentItem, pTrgItem->nSubMtlSlot, pSrcMtl, true);
						return;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SetSelectedItem(CMaterialBrowserRecord* pRecord, const TMaterialBrowserRecords* pMarkedRecords, bool bFromOnSelectOfCtrl)
{
	if (m_bIgnoreSelectionChange)
		return;

	m_bIgnoreSelectionChange = true;
	if (pMarkedRecords)
	{
		m_markedRecords = *pMarkedRecords;
	}
	else
	{
		m_markedRecords.clear();
	}

	m_pMatMan->SetCurrentFolder("");

	CMaterial* pMtl = NULL;
	if (pRecord && !pRecord->IsGroup())
	{
		TryLoadRecordMaterial(pRecord);
		pMtl = pRecord->pMaterial;
	}

	if (!bFromOnSelectOfCtrl)
	{
		m_tree.SelectItem(pRecord, pMarkedRecords);
	}

	RefreshSelected();

	if (!pMtl)
	{
		CMaterialBrowserRecord* pRec = pRecord;
		CString path;
		while (pRec)
		{
			path = '/' + CString(pRec->GetName()) + path;
			pRec = pRec->GetParent();
		}
		m_pMatMan->SetCurrentFolder((PathUtil::GetGameFolder() + path.GetBuffer()).c_str());
	}

	if (m_pListener)
		m_pListener->OnBrowserSelectItem(pMtl);

	m_timeOfHighlight = GetTickCount();
	m_pMatMan->SetHighlightedMaterial(pMtl);
	if (pMtl)
		m_bHighlightMaterial = true;

	std::vector<_smart_ptr<CMaterial>> markedMaterials;
	if (pMarkedRecords)
	{
		for (size_t i = 0; i < pMarkedRecords->size(); ++i)
		{
			markedMaterials.push_back((*pMarkedRecords)[i]->pMaterial);
		}
	}
	m_pMatMan->SetMarkedMaterials(markedMaterials);

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SelectItem(IDataBaseItem* pItem, IDataBaseItem* pParentItem)
{
	if (m_bIgnoreSelectionChange)
		return;

	if (!pItem)
		return;

	bool bFound = false;
	for (Items::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		CMaterialBrowserRecord* pRecord = *it;
		if (pRecord->pMaterial == pItem)
		{
			SetSelectedItem(pRecord, 0, false);
			bFound = true;
			break;
		}
	}

	if (!bFound && pItem && pItem->GetType() == EDB_TYPE_MATERIAL)
	{
		bool bLookForParent = false;
		CString mtlName = pItem->GetName();
		CString parentMtlName = pParentItem ? pParentItem->GetName() : "";

		CMaterial* pMtl = (CMaterial*)pItem;
		if (pMtl->GetParent())
		{
			mtlName = pMtl->GetParent()->GetName();
			bLookForParent = true;
		}

		// If material not found, try finding by name
		if (SelectItemByName(mtlName, parentMtlName))
		{
			bFound = true;
			if (bLookForParent)
			{
				// If we found parent, it must have been loaded and we can try again finding the actual record with a material.
				for (Items::iterator it = m_items.begin(); it != m_items.end(); ++it)
				{
					CMaterialBrowserRecord* pRecord = *it;
					if (pRecord->pMaterial == pItem)
					{
						SetSelectedItem(pRecord, 0, false);
						break;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnDuplicate()
{
	GetIEditorImpl()->ExecuteCommand("material.duplicate_current");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnCut()
{
	CMaterialBrowserRecord* pRecord = GetSelectedRecord();
	if (pRecord)
	{
		OnCopy();
		DeleteItem(pRecord);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnCopyName()
{
	CMaterial* pMtl = GetCurrentMaterial();
	if (pMtl)
	{
		CClipboard clipboard;
		clipboard.PutString(pMtl->GetName(), "Material Name");
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnCopy()
{
	CMaterial* pMtl = GetCurrentMaterial();
	if (pMtl)
	{
		CClipboard clipboard;
		XmlNodeRef node = XmlHelpers::CreateXmlNode("Material");
		node->setAttr("Name", pMtl->GetName());
		CBaseLibraryItem::SerializeContext ctx(node, false);
		ctx.bCopyPaste = true;
		pMtl->Serialize(ctx);
		clipboard.Put(node);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialBrowserCtrl::CanPaste()
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return false;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return false;

	if (strcmp(node->getTag(), "Material") == 0)
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnPaste()
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return;

	if (strcmp(node->getTag(), "Material") == 0)
	{
		GetIEditorImpl()->ExecuteCommand("material.create");
		CMaterial* pMtl = m_pMatMan->GetCurrentMaterial();
		if (pMtl)
		{
			// This is material node.
			CBaseLibraryItem::SerializeContext serCtx(node, true);
			serCtx.bCopyPaste = true;
			serCtx.bUniqName = true;
			pMtl->Serialize(serCtx);

			SelectItem(pMtl, NULL);
			pMtl->Save();
			pMtl->Reload();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnAddNewMaterial()
{
	GetIEditorImpl()->ExecuteCommand("material.create");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnAddNewMultiMaterial()
{
	GetIEditorImpl()->ExecuteCommand("material.create_multi");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnMergeMaterials()
{
	GetIEditorImpl()->ExecuteCommand("material.merge_selection");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnConvertToMulti()
{
	GetIEditorImpl()->ExecuteCommand("material.convert_to_multi");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::DeleteItem()
{
	CMaterialBrowserRecord* pRecord = GetSelectedRecord();
	if (pRecord)
	{
		DeleteItem(pRecord);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnResetItem()
{
	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr("Reset Material to defaults?"), QObject::tr("Reset Material")))
	{
		CMaterial* pMtl = GetCurrentMaterial();
		int index;

		pMtl->GetSubMaterialCount() > 0 ? index = pMtl->GetSubMaterialCount() : index = 1;

		if (pMtl)
		{
			for (int i = 0; i < index; i++)
			{
				pMtl->GetSubMaterialCount() > 0 ? pMtl->GetSubMaterial(i)->Reload() : pMtl->Reload();
				TickRefreshMaterials();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::DeleteItem(CMaterialBrowserRecord* pRecord)
{
	if (pRecord)
	{
		CMaterial* pMtl = pRecord->pMaterial;
		if (pMtl)
		{
			CSourceControlDescDlg dlg;
			if (!(pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_MANAGED) || dlg.DoModal() == IDOK)
			{
				if (pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_MANAGED)
				{
					if (!GetIEditorImpl()->GetSourceControl()->Delete(pMtl->GetFilename(), dlg.m_sDesc))
					{
						CQuestionDialog::SCritical(QObject::tr("Could not delete file from source control!"), QObject::tr("Error"));
					}
				}
				if (pRecord->nSubMtlSlot != -1)
					OnClearSubMtlSlot(pRecord);
				else
					GetIEditorImpl()->ExecuteCommand("material.delete_current");
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnRenameItem()
{
	CMaterial* pMtl = GetCurrentMaterial();
	if (!pMtl)
		return;

	if (pMtl->IsPureChild())
	{
		QStringDialog dlg("Enter New Material Name");
		dlg.SetString(pMtl->GetName());
		if (dlg.exec() == QDialog::Accepted)
		{
			pMtl->SetName(dlg.GetString().GetString());
			pMtl->Save();
			pMtl->Reload();
		}
	}
	else
	{
		if ((pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_MANAGED) &&
		    !(pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
		{
			if (QDialogButtonBox::StandardButton::Cancel == CQuestionDialog::SQuestion(QObject::tr("Confirm"), QObject::tr("Only checked-out files can be renamed. Check out and mark for delete before rename it?")), QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel)
			{
				return;
			}
		}

		CEngineFileDialog::RunParams runParams;
		runParams.title = QObject::tr("Save file to directory...");
		runParams.initialFile = pMtl->GetFilename();
		runParams.extensionFilters << CExtensionFilter("Material Files", "mtl");

		const QString path = CEngineFileDialog::RunGameSave(runParams, nullptr);
		if (path.isEmpty())
			return;

		CString filename(path.toStdString().c_str());
		CString itemName = m_pMatMan->FilenameToMaterial(filename.GetString());
		if (itemName.IsEmpty())
			return;

		if (m_pMatMan->FindItemByName(itemName))
		{
			Warning("Material with name %s already exist", (const char*)itemName);
			return;
		}

		if (GetIEditorImpl()->IsSourceControlAvailable() && (pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_MANAGED))
		{
			if (pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
			{
				if (QDialogButtonBox::StandardButton::Cancel == CQuestionDialog::SQuestion(QObject::tr("Confirm"), QObject::tr("The original file will be marked for delete and the new named file will be marked for integration.")), QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel)
				{
					return;
				}
			}
			else
				GetIEditorImpl()->GetSourceControl()->CheckOut(pMtl->GetFilename());

			if (!GetIEditorImpl()->GetSourceControl()->Rename(pMtl->GetFilename(), filename, "Rename"))
			{
				CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Could not rename file in Source Control."));
			}
		}

		// Delete file on disk.
		if (!pMtl->GetFilename().IsEmpty())
		{
			::DeleteFile(LevelFileUtils::ConvertGameToAbsolutePath(pMtl->GetFilename().GetString()).toStdString().c_str());
		}
		pMtl->SetName(itemName.GetString());
		pMtl->Save();

		//OnDataBaseItemEvent( pMtl,EDB_ITEM_EVENT_DELETE );
		//OnDataBaseItemEvent( pMtl,EDB_ITEM_EVENT_ADD );
	}

	/*
	   CMaterial * pMat = GetIEditorImpl()->GetMaterialManager()->GetCurrentMaterial();
	   if(pMat)
	   {
	   if(!GetIEditorImpl()->GetSourceControl()->Rename(pMat->GetFilename(), "Newname"))
	   {
	    CQuestionDialog::SCritical(QObject::tr(""),QObject::tr("Operation could not be completed."));
	   }
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnSetSubMtlCount(CMaterialBrowserRecord* pRecord)
{
	CMaterial* pMtl = pRecord->pMaterial;
	if (!pMtl)
		return;

	if (!pMtl->IsMultiSubMaterial())
		return;

	int num = pMtl->GetSubMaterialCount();
	QNumericBoxDialog dlg(QObject::tr("Number of Sub Materials"), num);
	dlg.SetRange(0.0, (float) MAX_SUB_MATERIALS);
	dlg.RestrictToInt();
	if (dlg.exec() == QDialog::Accepted)
	{
		num = dlg.GetValue();
		if (num != pMtl->GetSubMaterialCount())
		{
			CUndo undo("Set SubMtl Count");
			pMtl->SetSubMaterialCount(num);

			for (int i = 0; i < num; i++)
			{
				if (pMtl->GetSubMaterial(i) == 0)
				{
					// Allocate pure childs for all empty slots.
					string name;
					name.Format("SubMtl%d", i + 1);
					_smart_ptr<CMaterial> pChild = new CMaterial(name, MTL_FLAG_PURE_CHILD);
					pMtl->SetSubMaterial(i, pChild);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::DoSourceControlOp(CMaterialBrowserRecord* pRecord, ESourceControlOp scmOp)
{
	if (!pRecord)
		return;

	if (!GetIEditorImpl()->IsSourceControlAvailable())
		return;

	CMaterial* pMtl = pRecord->pMaterial;

	if (pMtl && pMtl->IsPureChild())
		pMtl = pMtl->GetParent();

	if (pMtl && pMtl->IsModified())
		pMtl->Save();

	string path = pMtl ? pMtl->GetFilename() : pRecord->CollectChildItems().GetString();

	if (path.IsEmpty())
		return;

	bool bRes = true;
	switch (scmOp)
	{
	case ESCM_GETPATH:
		{
			char outPath[MAX_PATH];
			bRes = GetIEditorImpl()->GetSourceControl()->GetInternalPath(path, outPath, MAX_PATH);
			if (bRes && *outPath)
			{
				CClipboard clipboard;
				clipboard.PutString(outPath);
				return; // do not need to reload the material
			}
		}
		break;
	case ESCM_IMPORT:
		if (pMtl)
		{
			CSourceControlDescDlg dlg;
			if (dlg.DoModal() == IDOK)
			{
				std::vector<string> filenames;
				int nTextures = pMtl->GetTextureFilenames(filenames);
				for (int i = 0; i < nTextures; ++i)
					GetIEditorImpl()->GetSourceControl()->Add(filenames[i], dlg.m_sDesc);

				bRes = GetIEditorImpl()->GetSourceControl()->Add(path, dlg.m_sDesc);
			}
			else
				bRes = true;
		}
		break;
	case ESCM_CHECKIN:
		if (pMtl)
		{
			CSourceControlDescDlg dlg;
			if (dlg.DoModal() == IDOK)
			{
				std::vector<string> filenames;
				int nTextures = pMtl->GetTextureFilenames(filenames);
				for (int i = 0; i < nTextures; ++i)
				{
					GetIEditorImpl()->GetSourceControl()->Add(filenames[i], dlg.m_sDesc);
					GetIEditorImpl()->GetSourceControl()->CheckIn(filenames[i], dlg.m_sDesc);
				}

				bRes = GetIEditorImpl()->GetSourceControl()->CheckIn(path, dlg.m_sDesc);
			}
			else
				bRes = true;
		}
		break;
	case ESCM_CHECKOUT:
		{
			if (pMtl && (pMtl->GetFileAttributes() & SCC_FILE_ATTRIBUTE_BYANOTHER))
			{
				char username[64];
				if (!GetIEditorImpl()->GetSourceControl()->GetOtherUser(pMtl->GetFilename(), username, 64))
					cry_strcpy(username, "another user");
				CString str;
				str.Format(_T("This file is checked out by %s. Try to continue?"), username);

				if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(str)))
				{
					return;
				}
			}
			if (bRes = GetIEditorImpl()->GetSourceControl()->GetLatestVersion(path))
			{
				bRes = GetIEditorImpl()->GetSourceControl()->CheckOut(path);
			}
		}
		break;
	case ESCM_UNDO_CHECKOUT:
		bRes = GetIEditorImpl()->GetSourceControl()->UndoCheckOut(path);
		break;
	case ESCM_GETLATEST:
		bRes = GetIEditorImpl()->GetSourceControl()->GetLatestVersion(path);
		break;
	case ESCM_GETLATESTTEXTURES:
		if (pMtl)
		{
			CString message;
			std::vector<string> filenames;
			int nTextures = pMtl->GetTextureFilenames(filenames);
			for (int i = 0; i < nTextures; ++i)
			{
				bool bRes = GetIEditorImpl()->GetSourceControl()->GetLatestVersion(filenames[i]);
				message += PathUtil::AbsolutePathToGamePath(filenames[i].GetString()) + (bRes ? " [OK]" : " [Fail]") + "\n";
			}

			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr(message.IsEmpty() ? "No files are affected." : message));
		}
		return;
	case ESCM_HISTORY:
		bRes = GetIEditorImpl()->GetSourceControl()->History(path);
		if (bRes)
			return;
		break;
	}

	if (!bRes)
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Source Control Operation Failed.\r\nCheck if Source Control Provider correctly setup and working directory is correct."));
		return;
	}

	if (pMtl)
		pMtl->Reload();
	else
		pRecord->UpdateChildItems();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnMakeSubMtlSlot(CMaterialBrowserRecord* pRecord)
{
	if (pRecord && pRecord->nSubMtlSlot >= 0)
	{
		CMaterialBrowserRecord* pParentInfo = pRecord->GetParent();
		if (pParentInfo && pParentInfo->pMaterial != NULL && pParentInfo->pMaterial->IsMultiSubMaterial())
		{
			CString str;
			str.Format(_T("Making new material will override material currently assigned to the slot %d of %s\r\nMake new sub material?"),
			           pRecord->nSubMtlSlot, pParentInfo->pMaterial->GetName());

			if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(str), QObject::tr("Confirm Override")))
			{
				string name;
				name.Format("SubMtl%d", pRecord->nSubMtlSlot + 1);
				_smart_ptr<CMaterial> pMtl = new CMaterial(name, MTL_FLAG_PURE_CHILD);
				pParentInfo->pMaterial->SetSubMaterial(pRecord->nSubMtlSlot, pMtl);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnClearSubMtlSlot(CMaterialBrowserRecord* pRecord)
{
	if (pRecord && pRecord->nSubMtlSlot >= 0 && pRecord->pMaterial != NULL)
	{
		CMaterialBrowserRecord* pParentInfo = pRecord->GetParent();
		if (pParentInfo && pParentInfo->pMaterial != NULL && pParentInfo->pMaterial->IsMultiSubMaterial())
		{
			CString str;
			str.Format(_T("Clear Sub-Material Slot %d of %s?"), pRecord->nSubMtlSlot, pParentInfo->pMaterial->GetName());

			if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(str), QObject::tr("Clear Sub-Material")))
			{
				CUndo undo("Material Change");
				SetSubMaterial(pParentInfo, pRecord->nSubMtlSlot, 0, true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SetSubMaterial(CMaterialBrowserRecord* pRecord, int nSlot, CMaterial* pSubMaterial, bool bSelectSubMtl)
{
	if (pRecord && pRecord->pMaterial != NULL && pRecord->pMaterial->IsMultiSubMaterial())
	{
		_smart_ptr<CMaterial> pMultiMat = pRecord->pMaterial;
		pMultiMat->SetSubMaterial(nSlot, pSubMaterial);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	if (m_bIgnoreSelectionChange)
		return;

	if (!pItem)
		return;

	CMaterial* pMtl = (CMaterial*)pItem;

	switch (event)
	{
	case EDB_ITEM_EVENT_ADD:
		if (!pMtl->IsPureChild())
		{
			AddItemToTree(pMtl, false);
			m_bItemsValid = false;
		}
		break;
	case EDB_ITEM_EVENT_DELETE:
		if (!pMtl->IsPureChild())
		{
			Items::iterator next;
			for (Items::iterator it = m_items.begin(); it != m_items.end(); it = next)
			{
				next = it;
				next++;
				CMaterialBrowserRecord* pRecord = *it;
				if (pRecord->pMaterial == pItem)
				{
					RemoveItemFromTree(pRecord);
					m_bItemsValid = false;
					break;
				}
			}
		}
		break;
	case EDB_ITEM_EVENT_CHANGED:
		{
			// If this is a sub material, refresh parent
			if (pMtl->IsPureChild())
			{
				pMtl = pMtl->GetParent();
				if (!pMtl)
					break;
			}

			CMaterialBrowserRecord* pRecord = 0;
			for (Items::iterator ppRecord = m_items.begin(); ppRecord != m_items.end(); ++ppRecord)
			{
				if ((*ppRecord)->pMaterial == pMtl)
				{
					pRecord = *ppRecord;
					break;
				}
			}

			if (pRecord)
			{
				pRecord->SetName(pMtl->GetShortName());
				pRecord->materialName = pMtl->GetName();
				pRecord->materialNameCrc32 = MaterialNameToCrc32(pRecord->materialName);

				if (pMtl->IsMultiSubMaterial())
				{
					ReloadTreeSubMtls(pRecord);
				}

				m_tree.UpdateItemState(pRecord);
			}

			if (pItem == GetCurrentMaterial())
			{
				if (pMtl->IsMultiSubMaterial())
					m_pLastActiveMultiMaterial = NULL;
				RefreshSelected();
			}
		}
		m_bItemsValid = false;
		break;
	case EDB_ITEM_EVENT_SELECTED:
		{
			CEditTool* pTool = GetIEditorImpl()->GetEditTool();
			if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CMaterialPickTool)))
			{
				m_wndToolBar.Reset();
				m_tree.SetFilterText("");
			}
			SelectItem(pItem, NULL);
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::SetImageListCtrl(CMaterialImageListCtrl* pCtrl)
{
	m_pMaterialImageListCtrl = pCtrl;
	if (m_pMaterialImageListCtrl)
		m_pMaterialImageListCtrl->SetSelectMaterialCallback(functor(*this, &CMaterialBrowserCtrl::OnImageListCtrlSelect));
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnImageListCtrlSelect(CImageListCtrlItem* pMtlItem)
{
	int nSlot = (INT_PTR)pMtlItem->pUserData;
	if (nSlot < 0)
	{
		return;
	}

	CMaterialBrowserRecord* pRecord = GetSelectedRecord();
	if (!pRecord)
		return;

	CMaterialBrowserRecord* pParentInfo = 0;
	if (pRecord->nSubMtlSlot < 0)
	{
		// Current selected material is not a sub slot.
		pParentInfo = pRecord;
	}
	else
	{
		// Current selected material is a sub slot.
		pParentInfo = pRecord->GetParent();
	}

	if (!pParentInfo)
		return;

	if (pParentInfo->pMaterial == NULL || !pParentInfo->pMaterial->IsMultiSubMaterial())
		return; // must be multi sub material.

	if (nSlot >= pParentInfo->pMaterial->GetSubMaterialCount())
		return;

	CMaterial* pMtl = pParentInfo->pMaterial->GetSubMaterial(nSlot);
	if (pMtl)
	{
		SelectItem(pMtl, NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::OnSaveToFile(bool bMulti)
{
	CMaterial* pCurrentMaterial = GetCurrentMaterial();
	if (pCurrentMaterial)
	{
		CString startPath = string(PathUtil::GetGameFolder()) + "/Materials";
		CString filename;
		if (!CFileUtil::SelectSaveFile("Material Files (*.mtl)|*.mtl", "mtl", startPath, filename))
		{
			return;
		}
		filename = PathUtil::AbsolutePathToCryPakPath(filename.GetString());
		CString itemName = PathUtil::RemoveExtension(filename.GetString());
		itemName = PathUtil::MakeGamePath(itemName.GetString()).c_str();

		if (m_pMatMan->FindItemByName(itemName))
		{
			Warning("Material with name %s already exist", (const char*)itemName);
			return;
		}
		int flags = pCurrentMaterial->GetFlags();
		if (bMulti)
			flags |= MTL_FLAG_MULTI_SUBMTL;

		pCurrentMaterial->SetFlags(flags);

		if (pCurrentMaterial->IsDummy())
		{
			pCurrentMaterial->ClearMatInfo();
			pCurrentMaterial->SetDummy(false);
		}
		pCurrentMaterial->SetModified(true);
		pCurrentMaterial->Save();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::RefreshSelected()
{
	CMaterialBrowserRecord* pRecord = GetSelectedRecord();
	if (!pRecord)
		return;

	CMaterial* pMtl = NULL;

	TryLoadRecordMaterial(pRecord);

	if (pRecord)
		pMtl = pRecord->pMaterial;

	if (m_pMaterialImageListCtrl)
	{
		m_pMaterialImageListCtrl->InvalidateMaterial(pMtl);
		if (pMtl)
		{
			CMaterial* pMultiMtl = 0;
			if (pMtl->IsMultiSubMaterial())
			{
				pMultiMtl = pMtl;
			}
			else if (pRecord->nSubMtlSlot >= 0)
			{
				CMaterialBrowserRecord* pParentInfo = pRecord->GetParent();
				if (pParentInfo)
				{
					pMultiMtl = pParentInfo->pMaterial;
					assert(pMultiMtl);
				}
			}

			if (m_pLastActiveMultiMaterial != pMultiMtl || pMultiMtl == 0)
			{
				// Normal material.
				m_pMaterialImageListCtrl->DeleteAllItems();
				m_pMaterialImageListCtrl->AddMaterial(pMtl, (void*)(intptr_t)pRecord->nSubMtlSlot);
			}
			else
			{
				m_pMaterialImageListCtrl->SetMaterial(0, pMtl, (void*)(intptr_t)pRecord->nSubMtlSlot);
			}
			if (m_pLastActiveMultiMaterial != pMultiMtl && pMultiMtl != 0)
			{
				for (int i = 0; i < pMultiMtl->GetSubMaterialCount(); i++)
				{
					if (pMultiMtl->GetSubMaterial(i))
						m_pMaterialImageListCtrl->AddMaterial(pMultiMtl->GetSubMaterial(i), (void*)(intptr_t)i);
				}
				m_pMaterialImageListCtrl->ResetSelection();
				if (pRecord->nSubMtlSlot >= 0)
				{
					m_pMaterialImageListCtrl->SelectItem(pRecord->nSubMtlSlot + 1);
				}
			}
			m_pMaterialImageListCtrl->SelectMaterial(pMtl);
			m_pLastActiveMultiMaterial = pMultiMtl;
		}
		else
		{
			m_pMaterialImageListCtrl->DeleteAllItems();
			m_pLastActiveMultiMaterial = NULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::ShowContextMenu(CMaterialBrowserRecord* pRecord, CPoint point)
{
	_smart_ptr<CMaterial> pMtl;
	if (pRecord)
	{
		pMtl = pRecord->pMaterial;
	}

	// Create pop up menu.
	CMenu menu;
	menu.CreatePopupMenu();

	bool bCanPaste = CanPaste();
	int pasteFlags = 0;
	if (!bCanPaste)
		pasteFlags |= MF_GRAYED;

	if (m_markedRecords.size() >= 2) // it makes sense when we have at least two items selected
	{
		int numMaterialsSelected = 0;
		for (int i = 0; i < m_markedRecords.size(); ++i)
		{
			if (m_markedRecords[i]->pMaterial)
				++numMaterialsSelected;
		}
		CString itemsSelected;
		itemsSelected.Format("  (%i Materials Selected)", numMaterialsSelected);
		menu.AppendMenu(MF_STRING | MF_GRAYED, 0, itemsSelected);
		menu.AppendMenu(MF_SEPARATOR, 0, "");

		if (numMaterialsSelected >= 2) // ... and at least two materials
			menu.AppendMenu(MF_STRING, MENU_MERGE, "Merge");
		else
			menu.AppendMenu(MF_STRING | MF_GRAYED, 0, "Merge (Select two or more)");
	}
	else if (!pRecord || pRecord->IsGroup()) // click on root, background or folder
	{
		menu.AppendMenu(MF_STRING | pasteFlags, MENU_PASTE, "Paste\tCtrl+V");
		menu.AppendMenu(MF_SEPARATOR, 0, "");
		menu.AppendMenu(MF_STRING, MENU_ADDNEW, "Add New Material");
		menu.AppendMenu(MF_STRING, MENU_ADDNEW_MULTI, "Add New Multi Material");
		if (GetIEditorImpl()->IsSourceControlAvailable())
		{
			menu.AppendMenu(MF_SEPARATOR, 0, "");
			menu.AppendMenu(MF_STRING | MF_GRAYED, 0, "  Source Control");
			menu.AppendMenu(MF_STRING, MENU_SCM_CHECK_OUT, "Check Out");
			menu.AppendMenu(MF_STRING, MENU_SCM_CHECK_IN, "Check In");
			menu.AppendMenu(MF_STRING, MENU_SCM_UNDO_CHECK_OUT, "Undo Check Out");
			menu.AppendMenu(MF_STRING, MENU_SCM_GET_LATEST, "Get Latest Version");
		}
	}
	else
	{
		if (pRecord->nSubMtlSlot >= 0)
		{
			int nFlags = 0;
			CMaterialBrowserRecord* pParentItemInfo = (CMaterialBrowserRecord*)pRecord->GetParentRecord();
			if (pParentItemInfo && pParentItemInfo->pMaterial != NULL)
			{
				uint32 nFileAttr = pParentItemInfo->pMaterial->GetFileAttributes();
				if (nFileAttr & SCC_FILE_ATTRIBUTE_READONLY)
					nFlags |= MF_GRAYED;
			}

			menu.AppendMenu(MF_STRING | nFlags, MENU_SUBMTL_MAKE, "Make Sub Material");
			menu.AppendMenu(MF_STRING | nFlags, MENU_SUBMTL_CLEAR, "Clear Sub Material");
			if (pMtl)
				menu.AppendMenu(MF_SEPARATOR, 0, "");
		}
		if (pMtl)
		{
			bool bDummMtl = false;
			if (!pMtl->IsPureChild() && (pMtl->GetFilename().IsEmpty() || pMtl->IsDummy()))
			{
				bDummMtl = true;
				menu.AppendMenu(MF_STRING, MENU_SAVE_TO_FILE, "Create Material From This");
				menu.AppendMenu(MF_STRING, MENU_SAVE_TO_FILE_MULTI, "Create Multi Material From This");
				menu.AppendMenu(MF_SEPARATOR, 0, "");
			}
			uint32 nFileAttr = pMtl->GetFileAttributes();

			if (pMtl->IsMultiSubMaterial())
			{
				menu.AppendMenu(MF_STRING, MENU_NUM_SUBMTL, "Set Number of Sub-Materials");
				menu.AppendMenu(MF_SEPARATOR, 0, "");
			}
			menu.AppendMenu(MF_STRING, MENU_CUT, "Cut\tCtrl+X");
			menu.AppendMenu(MF_STRING, MENU_COPY, "Copy\tCtrl+C");
			menu.AppendMenu(MF_STRING | pasteFlags, MENU_PASTE, "Paste\tCtrl+V");
			menu.AppendMenu(MF_STRING, MENU_COPY_NAME, "Copy Name to Clipboard");
			if (nFileAttr & SCC_FILE_ATTRIBUTE_INPAK)
			{
				menu.AppendMenu(MF_STRING, MENU_EXTRACT, "Extract");
			}
			else
			{
				menu.AppendMenu(MF_STRING, MENU_EXPLORE, "Explore");
			}
			menu.AppendMenu(MF_SEPARATOR, 0, "");
			menu.AppendMenu(MF_STRING, MENU_DUPLICATE, "Duplicate\tCtrl+D");
			menu.AppendMenu(MF_STRING, MENU_RENAME, "Rename\tF2");
			menu.AppendMenu(MF_STRING, MENU_DELETE, "Delete\tDel");
			menu.AppendMenu(MF_STRING | (nFileAttr & (SCC_FILE_ATTRIBUTE_READONLY) ? 0 : MF_GRAYED), MENU_RESET, "Reset");
			menu.AppendMenu(MF_SEPARATOR, 0, "");
			menu.AppendMenu(MF_STRING, MENU_ASSIGNTOSELECTION, "Assign to Selected Objects");
			menu.AppendMenu(MF_STRING, MENU_SELECTASSIGNEDOBJECTS, "Select Assigned Objects");
			menu.AppendMenu(MF_SEPARATOR, 0, "");

			if (!pMtl->IsMultiSubMaterial() && !pMtl->GetParent())
			{
				menu.AppendMenu(MF_STRING, MENU_CONVERT_TO_MULTI, "Convert To Multi Material");
				menu.AppendMenu(MF_SEPARATOR, 0, "");
			}

			menu.AppendMenu(MF_STRING, MENU_ADDNEW, "Add New Material");
			menu.AppendMenu(MF_STRING, MENU_ADDNEW_MULTI, "Add New Multi Material");
			menu.AppendMenu(MF_STRING | MF_GRAYED, 0, "Merge (Select two or more)");

			if (GetIEditorImpl()->IsSourceControlAvailable())
			{
				menu.AppendMenu(MF_SEPARATOR, 0, "");

				if (nFileAttr & SCC_FILE_ATTRIBUTE_INPAK)
				{
					menu.AppendMenu(MF_STRING | MF_GRAYED, 0, "  Material In Pak (Read Only)");
				}
				else
				{
					menu.AppendMenu(MF_STRING | MF_GRAYED, 0, "  Source Control");
					if (!(nFileAttr & SCC_FILE_ATTRIBUTE_MANAGED))
					{
						menu.AppendMenu(MF_STRING, MENU_SCM_ADD, "Add To Source Control");
					}
				}

				if (nFileAttr & SCC_FILE_ATTRIBUTE_MANAGED)
				{
					menu.AppendMenu(MF_STRING, MENU_SCM_GETPATH, "Copy Source Control Path To Clipboard");
					menu.AppendMenu(MF_STRING | ((nFileAttr & SCC_FILE_ATTRIBUTE_READONLY || nFileAttr & SCC_FILE_ATTRIBUTE_INPAK) ? 0 : MF_GRAYED), MENU_SCM_CHECK_OUT, "Check Out");
					menu.AppendMenu(MF_STRING | (nFileAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT ? 0 : MF_GRAYED), MENU_SCM_UNDO_CHECK_OUT, "Undo Check Out");
					menu.AppendMenu(MF_STRING | (nFileAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT ? 0 : MF_GRAYED), MENU_SCM_CHECK_IN, "Check In");
					menu.AppendMenu(MF_STRING, MENU_SCM_GET_LATEST, "Get Latest Version");
					menu.AppendMenu(MF_STRING, MENU_SCM_HISTORY, "Show History");
				}

				std::vector<string> filenames;
				int nTextures = pMtl->GetTextureFilenames(filenames);
				menu.AppendMenu(MF_STRING | (nTextures ? 0 : MF_GRAYED), MENU_SCM_GET_LATEST_TEXTURES, "Get Textures");
			}
		}
	}
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
	switch (cmd)
	{
	case MENU_UNDEFINED:
		return;                    // do nothing
	case MENU_CUT:
		OnCut();
		break;
	case MENU_COPY:
		OnCopy();
		break;
	case MENU_COPY_NAME:
		OnCopyName();
		break;
	case MENU_PASTE:
		OnPaste();
		break;
	case MENU_EXPLORE:
		{
			if (pMtl && pMtl->IsPureChild())
				pMtl = pMtl->GetParent();

			if (pMtl)
			{
				CString fullPath = pMtl->GetFilename();
				CString filename = PathUtil::GetFile(fullPath);
				if (INT_PTR(ShellExecute(0, _T("open"), _T("explorer"), CString("/select, ") + filename, PathUtil::GetPathWithoutFilename(fullPath), SW_SHOWNORMAL)) <= 32)
					ShellExecute(0, _T("explore"), PathUtil::GetPathWithoutFilename(fullPath), 0, 0, SW_SHOWNORMAL);
			}
		}
		break;
	case MENU_EXTRACT:
		{
			if (pMtl && pMtl->IsPureChild())
				pMtl = pMtl->GetParent();

			if (pMtl)
			{
				CString fullPath = pMtl->GetFilename();
				CString filename = PathUtil::GetFile(fullPath);

				if (CFileUtil::ExtractFile(fullPath, true, fullPath))
				{
					if (INT_PTR(ShellExecute(0, _T("open"), _T("explorer"), CString("/select, ") + filename, PathUtil::GetPathWithoutFilename(fullPath), SW_SHOWNORMAL)) <= 32)
						ShellExecute(0, _T("explore"), PathUtil::GetPathWithoutFilename(fullPath), 0, 0, SW_SHOWNORMAL);
				}

			}
		}
		break;
	case MENU_DUPLICATE:
		OnDuplicate();
		break;
	case MENU_RENAME:
		OnRenameItem();
		break;
	case MENU_DELETE:
		DeleteItem(pRecord);
		break;
	case MENU_RESET:
		OnResetItem();
		break;
	case MENU_ASSIGNTOSELECTION:
		GetIEditorImpl()->ExecuteCommand("material.assign_current_to_selection");
		break;
	case MENU_SELECTASSIGNEDOBJECTS:
		GetIEditorImpl()->ExecuteCommand("material.select_objects_with_current");
		break;
	case MENU_NUM_SUBMTL:
		OnSetSubMtlCount(pRecord);
		break;
	case MENU_ADDNEW:
		OnAddNewMaterial();
		break;
	case MENU_ADDNEW_MULTI:
		OnAddNewMultiMaterial();
		break;
	case MENU_CONVERT_TO_MULTI:
		OnConvertToMulti();
		break;
	case MENU_MERGE:
		OnMergeMaterials();
		break;

	case MENU_SUBMTL_MAKE:
		OnMakeSubMtlSlot(pRecord);
		break;
	case MENU_SUBMTL_CLEAR:
		OnClearSubMtlSlot(pRecord);
		break;

	case MENU_SCM_GETPATH:
		DoSourceControlOp(pRecord, ESCM_GETPATH);
		break;
	case MENU_SCM_ADD:
		DoSourceControlOp(pRecord, ESCM_IMPORT);
		break;
	case MENU_SCM_CHECK_OUT:
		DoSourceControlOp(pRecord, ESCM_CHECKOUT);
		break;
	case MENU_SCM_CHECK_IN:
		DoSourceControlOp(pRecord, ESCM_CHECKIN);
		break;
	case MENU_SCM_UNDO_CHECK_OUT:
		DoSourceControlOp(pRecord, ESCM_UNDO_CHECKOUT);
		break;
	case MENU_SCM_GET_LATEST:
		DoSourceControlOp(pRecord, ESCM_GETLATEST);
		break;
	case MENU_SCM_GET_LATEST_TEXTURES:
		DoSourceControlOp(pRecord, ESCM_GETLATESTTEXTURES);
		break;
	case MENU_SCM_HISTORY:
		DoSourceControlOp(pRecord, ESCM_HISTORY);
		break;

	case MENU_SAVE_TO_FILE:
		OnSaveToFile(false);
		break;
	case MENU_SAVE_TO_FILE_MULTI:
		OnSaveToFile(true);
		break;
		//case MENU_MAKE_SUBMTL: OnAddNewMultiMaterial(); break;
	}

	if (cmd != 0) // no need to refresh everything if we canceled menu
	{
		RefreshSelected();
		if (m_pListener)
			m_pListener->OnBrowserSelectItem(GetCurrentMaterial());
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialBrowserCtrl::SelectItemByName(const CString materialName, const CString parentMaterialName)
{
	uint32 materialNameCrc32 = MaterialNameToCrc32(materialName);
	uint32 parentNameCrc32 = MaterialNameToCrc32(parentMaterialName);

	for (Items::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		CMaterialBrowserRecord* pRecord = *it;
		if (pRecord->materialNameCrc32 == materialNameCrc32)
		{
			if (parentMaterialName.CompareNoCase("") != 0 && pRecord->GetParent() && parentNameCrc32 != pRecord->GetParent()->materialNameCrc32)
				continue;

			SetSelectedItem(pRecord, 0, false);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserCtrl::PopulateItems()
{
	if (m_bIgnoreSelectionChange)
		return;

	if (m_bItemsValid)
		return;

	m_bItemsValid = true;
	m_bIgnoreSelectionChange = true;

	IDataBaseItem* pSelection = m_pMatMan->GetSelectedItem();
	IDataBaseItem* pSelectionParent = m_pMatMan->GetSelectedParentItem();

	m_tree.Populate();

	m_bIgnoreSelectionChange = false;

	if (pSelection)
	{
		SelectItem(pSelection, pSelectionParent);
		if (m_bHighlightMaterial)
		{
			m_bHighlightMaterial = false;
			m_pMatMan->SetHighlightedMaterial(0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
uint32 CMaterialBrowserCtrl::MaterialNameToCrc32(const char* str)
{
	return CCrc32::ComputeLowercase(str);
}

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserRecord* CMaterialBrowserCtrl::GetSelectedRecord()
{
	CMaterialBrowserRecord* pFirstFoundRecord = 0;
	CMaterialBrowserRecord* pFocusedRecord = 0;
	int nCount = m_tree.GetSelectedRows()->GetCount();
	for (int i = 0; i < nCount; i++)
	{
		CXTPReportRow* pRow = m_tree.GetSelectedRows()->GetAt(i);
		CMaterialBrowserRecord* pRecord = m_tree.RowToRecord(pRow);
		if (pRecord)
		{
			if (!pFirstFoundRecord)
				pFirstFoundRecord = pRecord;
			if (pRow->IsFocused())
				return pRecord;
		}
	}

	return pFirstFoundRecord;
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialBrowserCtrl::GetCurrentMaterial()
{
	CMaterialBrowserRecord* pRecord = GetSelectedRecord();
	if (pRecord)
	{
		return pRecord->pMaterial;
	}
	else
	{
		return m_pMatMan->GetCurrentMaterial();
	}
}

//////////////////////////////////////////////////////////////////////////
CString CMaterialBrowserCtrl::GetSelectedMaterialID()
{
	CMaterialBrowserRecord* pRecord = GetSelectedRecord();
	if (pRecord)
	{
		return pRecord->materialName;
	}
	return "";
}

BOOL CMaterialBrowserCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		UINT nChar = pMsg->wParam;
		if (nChar == 'z' || nChar == 'Z') // call undo by MainFrame.
			return AfxGetMainWnd()->PreTranslateMessage(pMsg);
	}

	return __super::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
// CMaterialBrowserTreeCtrl

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMaterialBrowserTreeCtrl, CTreeCtrlReport)
ON_NOTIFY_REFLECT(NM_RCLICK, OnNMRclick)
ON_NOTIFY_REFLECT(NM_KEYDOWN, OnTreeKeyDown)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMaterialBrowserTreeCtrl::CMaterialBrowserTreeCtrl()
{
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_MATERIAL_TREE, 20, RGB(255, 0, 255));
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_FILE_STATUS, 20, RGB(255, 0, 255));

	//m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_CGF,ITEM_OVERLAY_ID_CGF );
	//m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_INPAK,ITEM_OVERLAY_ID_INPAK );
	//m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_READONLY,ITEM_OVERLAY_ID_READONLY );
	//m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_ONDISK,ITEM_OVERLAY_ID_ONDISK );
	//m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_LOCKED,ITEM_OVERLAY_ID_LOCKED );
	//m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_CHECKEDOUT,ITEM_OVERLAY_ID_CHECKEDOUT );
	//m_imageList.SetOverlayImage( ITEM_IMAGE_OVERLAY_NO_CHECKOUT,ITEM_OVERLAY_ID_NO_CHECKOUT );

	SetMultipleSelection(TRUE);
	SetImageList(&m_imageList);

	CXTPReportColumn* pCol1 = AddTreeColumn("Material");
	pCol1->SetSortable(TRUE);
	GetColumns()->SetSortColumn(pCol1, TRUE);
	SetRowsCompareFunc(RowCompareFunc);
	SetExpandOnDblClick(true);

	m_pMtlBrowser = 0;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserTreeCtrl::OnFillItems()
{
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserTreeCtrl::OnSelectionChanged()
{
	CMaterialBrowserRecord* pRecord = 0;

	TMaterialBrowserRecords markedRecords;

	int nCount = GetSelectedRows()->GetCount();
	for (int i = 0; i < nCount; i++)
	{
		CXTPReportRow* pRow = GetSelectedRows()->GetAt(i);
		CMaterialBrowserRecord* pCurrentRecord = RowToRecord(pRow);
		if (pCurrentRecord)
		{
			markedRecords.push_back(pCurrentRecord);
			if (pRow->IsFocused())
				pRecord = pCurrentRecord;
		}
	}

	if (!pRecord && !markedRecords.empty())
		pRecord = markedRecords.front();

	m_pMtlBrowser->SetSelectedItem(pRecord, &markedRecords, true);
}

void CMaterialBrowserTreeCtrl::DeleteAllItems()
{
	__super::DeleteAllItems();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserTreeCtrl::OnNMRclick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CPoint point;
	// Find node under mouse.
	GetCursorPos(&point);
	ScreenToClient(&point);
	// Select the item that is at the point myPoint.

	CMaterialBrowserRecord* pRecord = 0;

	CXTPReportRow* pRow = HitTest(point);
	if (pRow)
	{
		pRecord = RowToRecord(pRow);
	}

	if (pRecord && GetSelectedCount() <= 1)
	{
		m_pMtlBrowser->SetSelectedItem(pRecord, 0, false);
	}

	// Show helper menu.
	GetCursorPos(&point);
	m_pMtlBrowser->ShowContextMenu(pRecord, point);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserTreeCtrl::OnTreeKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	GetAsyncKeyState(VK_CONTROL);
	bool bCtrl = GetAsyncKeyState(VK_CONTROL) != 0;
	// Key press in items tree view.
	NMTVKEYDOWN* nm = (NMTVKEYDOWN*)pNMHDR;

	CPoint point;
	// Find node under mouse.
	GetCursorPos(&point);
	ScreenToClient(&point);

	CMaterialBrowserRecord* pRecord = 0;
	CXTPReportRow* pRow = HitTest(point);
	if (pRow)
	{
		pRecord = (CMaterialBrowserRecord*)pRow->GetRecord();
	}

	if (bCtrl && (nm->wVKey == 'c' || nm->wVKey == 'C'))
	{
		m_pMtlBrowser->OnCopy();  // Ctrl+C
	}
	if (bCtrl && (nm->wVKey == 'v' || nm->wVKey == 'V'))
	{
		m_pMtlBrowser->OnPaste(); // Ctrl+V
	}
	if (bCtrl && (nm->wVKey == 'x' || nm->wVKey == 'X'))
	{
		m_pMtlBrowser->OnCut(); // Ctrl+X
	}
	if (bCtrl && (nm->wVKey == 'd' || nm->wVKey == 'D'))
	{
		m_pMtlBrowser->OnDuplicate(); // Ctrl+D
	}
	if (nm->wVKey == VK_DELETE)
	{
		if (pRecord)
			m_pMtlBrowser->DeleteItem(pRecord);
	}
	if (nm->wVKey == VK_F2)
	{
		m_pMtlBrowser->OnRenameItem();
	}
	if (nm->wVKey == VK_INSERT)
	{
		m_pMtlBrowser->OnAddNewMaterial();
	}
}

//////////////////////////////////////////////////////////////////////////
CTreeItemRecord* CMaterialBrowserTreeCtrl::CreateGroupRecord(const char* name, int nGroupIcon)
{
	CMaterialBrowserRecord* pRec = new CMaterialBrowserRecord();
	pRec->CreateStdItems();
	pRec->SetName(name);
	pRec->SetGroup(true);
	pRec->SetIcon(nGroupIcon);
	return pRec;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserTreeCtrl::SelectItem(CMaterialBrowserRecord* pRecord, const TMaterialBrowserRecords* pMarkedRecords)
{
	EnsureItemVisible(pRecord);

	GetSelectedRows()->Clear();
	if (pRecord || pMarkedRecords)
	{
		CXTPReportRows* pRows = GetRows();
		for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
		{
			CXTPReportRow* pRow = pRows->GetAt(i);
			CMaterialBrowserRecord* pCurrentRecord = RowToRecord(pRow);
			if (pCurrentRecord == pRecord)
			{
				pRow->SetSelected(TRUE);
				SetFocusedRow(pRow);
			}
			else if (pMarkedRecords &&
			         std::find(pMarkedRecords->begin(), pMarkedRecords->end(), pCurrentRecord) != pMarkedRecords->end())
			{
				pRow->SetSelected(TRUE);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserTreeCtrl::UpdateItemState(CMaterialBrowserRecord* pRecord)
{
	if (!pRecord)
		return;

	uint32 attr = 0;
	bool bFromCGF = false;
	if (pRecord->pMaterial != NULL)
	{
		attr = pRecord->pMaterial->GetFileAttributes();
		bFromCGF = pRecord->pMaterial->IsFromEngine();
	}

	int nIconMtl = ITEM_IMAGE_MATERIAL;
	if (pRecord->pMaterial && pRecord->pMaterial->IsMultiSubMaterial())
	{
		nIconMtl = ITEM_IMAGE_MULTI_MATERIAL;
	}
	else if (pRecord->nSubMtlSlot >= 0 && pRecord->pMaterial && !pRecord->pMaterial->IsPureChild())
	{
		nIconMtl = ITEM_IMAGE_SHARED_MATERIAL;
	}
	pRecord->SetIcon(nIconMtl);

	int nIcon = -1;
	if (bFromCGF)
	{
		// From CGF.
		nIcon = ITEM_IMAGE_OVERLAY_CGF;
	}
	else if (attr & SCC_FILE_ATTRIBUTE_INPAK)
	{
		// Inside pak file.
		nIcon = ITEM_IMAGE_OVERLAY_INPAK;
	}
	else if (attr & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
	{
		// Checked out.
		nIcon = ITEM_IMAGE_OVERLAY_CHECKEDOUT;
	}
	else if (attr & SCC_FILE_ATTRIBUTE_MANAGED)
	{
		// Checked out.
		nIcon = ITEM_IMAGE_OVERLAY_LOCKED;
	}
	else if (attr & SCC_FILE_ATTRIBUTE_READONLY)
	{
		// Read-Only.
		nIcon = ITEM_IMAGE_OVERLAY_READONLY;
	}
	pRecord->SetIcon2(nIcon);
}

//////////////////////////////////////////////////////////////////////////
int _cdecl CMaterialBrowserTreeCtrl::RowCompareFunc(const CXTPReportRow** pRow1, const CXTPReportRow** pRow2)
{
	CMaterialBrowserRecord* pRec1 = RowToRecord(*pRow1);
	CMaterialBrowserRecord* pRec2 = RowToRecord(*pRow2);

	bool bGroup1 = false;
	int material1 = -1;
	const char* name1 = "";
	if (pRec1)
	{
		bGroup1 = pRec1->IsGroup();
		material1 = pRec1->nSubMtlSlot;
		name1 = pRec1->GetName();
	}

	bool bGroup2 = false;
	int material2 = -1;
	const char* name2 = "";
	if (pRec2)
	{
		material2 = pRec2->nSubMtlSlot;
		bGroup2 = pRec2->IsGroup();
		name2 = pRec2->GetName();
	}

	if (bGroup1 && !bGroup2)
		return -1;
	if (!bGroup1 && bGroup2)
		return 1;

	if (material1 > -1 && material2 > -1)
		return (material1 > material2) ? 1 : -1;

	return stricmp(name1, name2);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserTreeCtrl::OnItemExpanded(CXTPReportRow* pRow, bool bExpanded)
{
	CMaterialBrowserRecord* pRecord = RowToRecord(pRow);
	if (pRecord && bExpanded)
		pRecord->UpdateChildItems();
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialBrowserTreeCtrl::OnFilterTestSubmaterials(CTreeItemRecord* pRecord, bool bAddOperation)
{
	CMaterialBrowserRecord* pMatRecord = static_cast<CMaterialBrowserRecord*>(pRecord);
	if (!pMatRecord->pMaterial)
		m_pMtlBrowser->TryLoadRecordMaterial(pMatRecord);

	CMaterial* pMaterial = pMatRecord->pMaterial;
	if (!pMaterial)
		return false;

	int nSubMaterialCount = pMaterial->GetSubMaterialCount();
	for (int i = 0; i < nSubMaterialCount; ++i)
	{
		CString name = pMaterial->GetSubMaterial(i)->GetName();
		if (name.IsEmpty())
			continue;

		if (FilterTest(name, bAddOperation))
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialBrowserTreeCtrl::OnFilterTestTextures(CTreeItemRecord* pRecord, bool bAddOperation)
{
	CMaterialBrowserRecord* pMatRecord = static_cast<CMaterialBrowserRecord*>(pRecord);
	if (!pMatRecord->pMaterial)
		m_pMtlBrowser->TryLoadRecordMaterial(pMatRecord);

	CMaterial* pMaterial = pMatRecord->pMaterial;
	if (!pMaterial)
		return false;

	SInputShaderResources& shaderResources = pMaterial->GetShaderResources();
	for (int i = 0; i < EFTT_MAX; ++i)
	{
		CString name = shaderResources.m_Textures[i].m_Name;
		if (name.IsEmpty())
			continue;

		if (FilterTest(name, bAddOperation))
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialBrowserTreeCtrl::OnFilterTest(CTreeItemRecord* pRecord)
{
	if (!pRecord->IsKindOf(RUNTIME_CLASS(CMaterialBrowserRecord)))
		return false;

	if (m_pMtlBrowser->GetSearchFilter() == CMaterialToolBar::eFilter_Materials)
	{
		return FilterTest(pRecord->GetName(), true);
	}
	else if (m_pMtlBrowser->GetSearchFilter() == CMaterialToolBar::eFilter_Submaterials)
	{
		return OnFilterTestSubmaterials(pRecord, true);
	}
	else if (m_pMtlBrowser->GetSearchFilter() == CMaterialToolBar::eFilter_Textures)
	{
		return OnFilterTestTextures(pRecord, true);
	}
	else if (m_pMtlBrowser->GetSearchFilter() == CMaterialToolBar::eFilter_Materials_And_Textures)
	{
		return FilterTest(pRecord->GetName(), false) && OnFilterTestTextures(pRecord, false);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// CMaterialBrowserRecord

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserRecord::CreateItems()
{
	int nIcon = ITEM_IMAGE_MATERIAL;
	if (pMaterial && pMaterial->IsMultiSubMaterial())
	{
		nIcon = ITEM_IMAGE_MULTI_MATERIAL;
	}
	else if (nSubMtlSlot >= 0 && pMaterial && !pMaterial->IsPureChild())
	{
		nIcon = ITEM_IMAGE_SHARED_MATERIAL;
	}

	CString itemText;

	if (nSubMtlSlot >= 0)
	{
		int numSubMaterials = 0;
		if (pMaterial && pMaterial->GetParent())
			numSubMaterials = pMaterial->GetParent()->GetSubMaterialCount();
		const char* format = numSubMaterials > 9 ? "[%02d] %s" : "[%d] %s";
		itemText.Format(format, nSubMtlSlot + 1, (const char*)materialName);
	}
	else
	{
		itemText = materialName;
	}

	SetName(itemText);
	CreateStdItems();
	SetIcon(nIcon);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialBrowserRecord::UpdateChildItems()
{
	for (int i = GetChildCount() - 1; i >= 0; --i)
	{
		CTreeItemRecord* pChild = GetChild(i);
		if (!pChild || !pChild->IsKindOf(RUNTIME_CLASS(CMaterialBrowserRecord)))
			continue;

		((CMaterialBrowserRecord*)pChild)->bUpdated = false;
	}
}

//////////////////////////////////////////////////////////////////////////
CString CMaterialBrowserRecord::CollectChildItems()
{
	CString paths;
	for (int i = GetChildCount() - 1; i >= 0; --i)
	{
		CTreeItemRecord* pChild = GetChild(i);
		if (!pChild || !pChild->IsKindOf(RUNTIME_CLASS(CMaterialBrowserRecord)))
			continue;

		CMaterialBrowserRecord* pRecord = (CMaterialBrowserRecord*)pChild;
		if (pRecord && pRecord->pMaterial)
		{
			paths += pRecord->pMaterial->GetFilename();
			if (i)
				paths += ";";
		}
	}
	return paths;
}


