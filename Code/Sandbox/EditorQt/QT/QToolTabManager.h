// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"
#include <Controls/SandboxWindowing.h>
#include <Util/UserDataUtil.h>
#include <QFileInfo>

class CWnd;
class QFocusEvent;

//Internal class for IPane management.
class QTabPane : public QBaseTabPane
{
	Q_OBJECT
public:

	CWnd* m_MfcWnd;

public:
	QTabPane();
	~QTabPane();

protected:
	void          showContextMenu(const QPoint& point);

	void          closeEvent(QCloseEvent* event);
	virtual QSize sizeHint() const        { return m_defaultSize; }
	// if there is an mfc widget return stored minimum size, if not let qt handle min size hint calculation
	virtual QSize minimumSizeHint() const { return m_MfcWnd ? m_minimumSize : QBaseTabPane::minimumSizeHint(); }
	void focusInEvent(QFocusEvent* pEvent);
};

class SANDBOX_API CTabPaneManager : public CUserData
{
	friend class QTabPane;
public:
	CTabPaneManager(QWidget* const pParent);
	~CTabPaneManager();

	void          SaveLayout();
	bool          LoadUserLayout();
	bool          LoadLayout(const char* filePath);
	bool          LoadDefaultLayout();
	void          SaveLayoutToFile(const char* fullFilename);

	QFileInfoList GetUserLayouts();
	QFileInfoList GetProjectLayouts();
	QFileInfoList GetAppLayouts();

	// nOverrideDockDirection is one of the IViewPanelCass::EDockingDirection
	IPane* CreatePane(const char* paneClassName, const char* title = 0, int nOverrideDockDirection = -1);

	template<typename T>
	T* CreatePaneT(const char* paneClassName, const char* title = 0, int nOverrideDockDirection = -1)
	{
		static_assert(std::is_base_of<IPane, T>::value, "CreatePaneT can only be called with IPane derived types");
		return static_cast<T*>(CreatePane(paneClassName, title, nOverrideDockDirection));
	}

	void   CloseAllPanes();

	IPane*              FindPaneByClass(const char* paneClassName);
	IPane*              FindPaneByTitle(const char* title);
	IPane*              FindPane(const std::function<bool(IPane*, const string& /*className*/)>& predicate);
	std::vector<IPane*> FindAllPanelsByClass(const char* paneClassName);

	void   BringToFront(IPane* pane);
	IPane* OpenOrCreatePane(const char* paneClassName);

	//////////////////////////////////////////////////////////////////////////
	// MFC Specific
	CWnd* OpenMFCPane(const char* paneClassName);
	CWnd* FindMFCPane(const char* paneClassName);
	//////////////////////////////////////////////////////////////////////////

	void                    OnIdle();
	void                    LayoutLoaded();

	static CTabPaneManager* GetInstance();

	void                    OnTabPaneMoved(QWidget* tabPane, bool visible);

private:
	QTabPane*                 CreateTabPane(const char* paneClassName, const char* title = 0, int nOverrideDockDirection = -1, bool bLoadLayoutPersonalization = false);
	QString                   CreateObjectName(const char* title);

	QVariant                  GetState() const;
	void                      SetState(const QVariant& state);

	void                      CreateContentInPanes();
	IPane*                    CreatePaneContents(QTabPane* pTool);

	void                      StoreHistory(QTabPane* viewDesc);

	class QToolWindowManager* GetToolManager() const;

	QTabPane*                 FindTabPane(IPane* pane);

	void                      FocusTabPane(QTabPane* pPane);
	bool                      CloseTabPane(QTabPane* pane);

	bool                      LoadLayoutFromFile(const char* fullFilename);

	QTabPane*                 FindTabPaneByName(const QString& name = QString());
	QList<QTabPane*>          FindTabPanes(const QString& name = QString());
	QTabPane*                 FindTabPaneByClass(const char* paneClassName);
	QTabPane*                 FindTabPaneByCategory(const char* sPaneCategory);
	QTabPane*                 FindTabPaneByTitle(const char* title);

	void                      PushUserEvent(const char* szEventName, const char* szTitle, const void* pAddress);

	struct SPaneHistory
	{
		QRect rect;    // Last known size of this panel.
		int   dockDir; // Last known dock style.
	};
	std::map<string, SPaneHistory> m_panesHistory;

	bool                           m_bToolsDirty;
	std::vector<QTabPane*>         m_panes;

	QWidget* const                 m_pParent;
	bool                           m_layoutLoaded;
};
