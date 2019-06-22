// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/Editor.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "CrySandbox/CrySignal.h"
#include "AssetSystem/AssetType.h"
#include "IAssetBrowserContext.h"

#include <QWidget>

class CAssetDropHandler;
class CAssetFolderFilterModel;
class CAssetFoldersView;
class CBreadcrumbsBar;
class CFilteredFolders;
class CLineEditDelegate;
class CSortFilterProxyModel;
class QAction;
class QAdvancedTreeView;
class QAttributeFilterProxyModel;
class QBoxLayout;
class QButtonGroup;
class QCommandAction;
class QFilteringPanel;
class QItemSelectionModel;
class QMenu;
class QSplitter;
class QThumbnailsView;
class QTimer;
class QToolButton;
struct IUIContext;

//Thumbnails should now be used, leaving for now in case it is temporarily useful
#define ASSET_BROWSER_USE_PREVIEW_WIDGET 0

//! The dockable class for the Asset Browser
class EDITOR_COMMON_API CAssetBrowser : public CDockableEditor, public IAssetBrowserContext
{
	Q_OBJECT
	Q_PROPERTY(int buttonsSpacing READ GetButtonsSpacing WRITE SetButtonsSpacing)
	Q_PROPERTY(int buttonGroupsSpacing READ GetButtonGroupsSpacing WRITE SetButtonGroupsSpacing)

public:
	static CCrySignal<void(CAbstractMenu&, const std::shared_ptr<IUIContext>&)> s_signalMenuCreated;
	static CCrySignal<void(CAbstractMenu&, const std::vector<CAsset*>&, const std::vector<string>& folders, const std::shared_ptr<IUIContext>&)> s_signalContextMenuRequested;

	CAssetBrowser(bool bHideEngineFolder = false, QWidget* pParent = nullptr);
	CAssetBrowser(const std::vector<CAssetType*>& assetTypes, bool bHideEngineFolder = false, QWidget* pParent = nullptr);
	virtual ~CAssetBrowser();

	//extract actual content from the selection
	virtual void                GetSelection(std::vector<CAsset*>& assets, std::vector<string>& folders) const override;

	virtual std::vector<string> GetSelectedFolders() const override;

	std::vector<CAsset*>        GetSelectedAssets() const;
	CAsset*                     GetLastSelectedAsset() const;

	void                        SelectAsset(const char* szPath) const;
	void                        SelectAsset(const CAsset& asset) const;

	//CEditor implementation
	virtual const char* GetEditorName() const override { return "Asset Browser"; }
	virtual void        SetLayout(const QVariantMap& state) override;
	virtual QVariantMap GetLayout() const override;

	void                GrabFocusSearchBar() { OnFind(); }
	QFilteringPanel*    GetFilterPanel();

	enum ViewMode
	{
		Details,
		Thumbnails,
		VSplit,
		HSplit,

		Max
	};

	void    SetViewMode(ViewMode viewMode);

	bool    IsRecursiveView() const;
	void    SetRecursiveView(bool recursiveView);

	bool    IsFoldersViewVisible() const;
	void    SetFoldersViewVisible(bool isVisible);

	bool    AreIrrelevantFoldersHidden() const;
	void    HideIrrelevantFolders(bool isHidden);

	CAsset* QueryNewAsset(const CAssetType& type, const CAssetType::SCreateParams* pCreateParams);

	void    NotifyContextMenuCreation(CAbstractMenu& menu, const std::vector<CAsset*>& assets, const std::vector<string>& folders);

	int     GetButtonsSpacing() const       { return m_buttonsSpacing; }
	void    SetButtonsSpacing(int val)      { m_buttonsSpacing = val; }
	int     GetButtonGroupsSpacing() const  { return m_buttonGroupsSpacing; }
	void    SetButtonGroupsSpacing(int val) { m_buttonGroupsSpacing = val; }

signals:
	//! This signal is emitted whenever the selection of folders or assets changes.
	void SelectionChanged();

protected:
	virtual void                mouseReleaseEvent(QMouseEvent* pEvent) override;

	QAttributeFilterProxyModel* GetAttributeFilterProxyModel();
	QItemSelectionModel*        GetItemSelectionModel();
	const QItemSelectionModel*  GetItemSelectionModel() const;
	QAdvancedTreeView*          GetDetailsView();
	QThumbnailsView*            GetThumbnailsView();

	// The widget has to be visible.
	void         ScrollToSelected();

	bool         ValidatePath(const QString);

	virtual void OnActivated(CAsset* pAsset);
	virtual void OnActivated(const QString& folder);

	virtual void UpdatePreview(const QModelIndex& currentIndex);

	// Adaptive layouts enables editor owners to make better use of space
	bool                          SupportsAdaptiveLayout() const override { return true; }
	// Triggered on resize for editors that support adaptive layouts
	void                          OnAdaptiveLayoutChanged() override;
	// Used for determining what layout direction to use if adaptive layout is turned off
	Qt::Orientation               GetDefaultOrientation() const override { return Qt::Horizontal; }

	virtual const IEditorContext* GetContextObject() const override      { return this; };

private:
	void               SetModel(CAssetFolderFilterModel* pModel);
	void               InitActions();
	void               InitAssetTypeFilter(const QStringList assetTypeNames);
	void               InitNewNameDelegates();
	void               InitViews(bool bHideEngineFolder);
	void               InitMenus();
	void               InitAssetsView();
	void               InitDetailsView();
	void               InitThumbnailsView();
	void               WaitUntilAssetsAreReady();
	QWidget*           CreateAssetsViewSelector();

	void               FillCreateAssetMenu(CAbstractMenu* menu, bool enable);

	void               BeginCreateAsset(const CAssetType& type, const CAssetType::SCreateParams* pCreateParams);
	void               EndCreateAsset();

	QAbstractItemView* GetFocusedView() const;

	virtual bool       eventFilter(QObject* object, QEvent* event) override;

	void               OnSelectionChanged();

	void               UpdateSelectionDependantActions();
	void               UpdatePasteActionState();

	void               OnContextMenu();
	void               AppendFilterDependenciesActions(CAbstractMenu* pAbstractMenu, const CAsset* pAsset);
	void               OnFolderViewContextMenu();

	void               CreateContextMenu(bool isFolderView = false);

	void               BuildContextMenuForEmptiness(CAbstractMenu& abstractMenu);
	void               BuildContextMenuForFolders(const std::vector<string>& folders, CAbstractMenu& abstractMenu);
	void               BuildContextMenuForAssets(const std::vector<CAsset*>& assets, const std::vector<string>& folders, CAbstractMenu& abstractMenu);

	void               AddWorkFilesMenu(CAbstractMenu& abstractMenu);

	bool               HasSelectedModifiedAsset() const;

	void               OnFolderSelectionChanged(const QStringList& selectedFolders);
	void               OnActivated(const QModelIndex& index);
	void               OnCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
	void               Delete(const std::vector<CAsset*>& assets);

	void               OnRenameAsset(CAsset& asset);
	void               OnRenameFolder(const QString& folder);
	void               OnOpenInExplorer(const QString& folder);
	void               OnNavBack();
	void               OnNavForward();
	void               OnBreadcrumbClick(const QString& text, const QVariant& data);
	void               OnBreadcrumbsTextChanged(const QString& text);

	void               GenerateThumbnailsAsync(const string& folder, const std::function<void()>& finalize = std::function<void()>());

	void               OnSearch(bool isNewSearch);
	void               UpdateNavigation(bool clearHistory);
	void               UpdateBreadcrumbsBar(const QString& path);
	void               UpdateNonEmptyFolderList();

	void               EditNewAsset();

	bool               OnDiscardChanges();
	bool               OnShowInFileExplorer();
	bool               OnGenerateThumbmails();
	bool               OnSaveAll();
	bool               OnRecursiveView();
	bool               OnFolderTreeView();
	bool               OnManageWorkFiles();
	bool               OnDetailsView();
	bool               OnThumbnailsView();
	bool               OnSplitHorizontalView();
	bool               OnSplitVerticalView();
	bool               OnGenerateRepairAllMetadata();
	bool               OnReimport();
	bool               OnHideIrrelevantFolders();

	// CEditor
	virtual bool OnFind() override;
	// ~CEditor

	bool OnDelete();
	bool OnOpen();
	bool OnCopy();
	bool OnPaste();
	bool OnDuplicate();
	bool OnImport();
	bool OnNewFolder();
	bool OnRename();
	bool OnSave();

	void OnCopyName();
	void OnCopyPath();

	void Paste(bool pasteNextToOriginal);

	//ui components
	CAssetFoldersView* m_pFoldersView = nullptr;
	CBreadcrumbsBar*   m_pBreadcrumbs = nullptr;
	QCommandAction*    m_pActionCopy = nullptr;
	QCommandAction*    m_pActionCopyName = nullptr;
	QCommandAction*    m_pActionCopyPath = nullptr;
	QCommandAction*    m_pActionDelete = nullptr;
	QCommandAction*    m_pActionDiscardChanges = nullptr;
	QCommandAction*    m_pActionDuplicate = nullptr;
	QCommandAction*    m_pActionGenerateRepairMetaData = nullptr;
	QCommandAction*    m_pActionGenerateThumbnails = nullptr;
	QCommandAction*    m_pActionHideIrrelevantFolders = nullptr;
	QCommandAction*    m_pActionManageWorkFiles = nullptr;
	QCommandAction*    m_pActionPaste = nullptr;
	QCommandAction*    m_pActionRecursiveView = nullptr;
	QCommandAction*    m_pActionReimport = nullptr;
	QCommandAction*    m_pActionRename = nullptr;
	QCommandAction*    m_pActionSave = nullptr;
	QCommandAction*    m_pActionSaveAll = nullptr;
	QCommandAction*    m_pActionShowDetails = nullptr;
	QCommandAction*    m_pActionShowFoldersView = nullptr;
	QCommandAction*    m_pActionShowInFileExplorer = nullptr;
	QCommandAction*    m_pActionShowSplitHorizontally = nullptr;
	QCommandAction*    m_pActionShowSplitVertically = nullptr;
	QCommandAction*    m_pActionShowThumbnails = nullptr;
#if ASSET_BROWSER_USE_PREVIEW_WIDGET
	QCommandAction*    m_pActionShowPreview = nullptr;
#endif

	QAdvancedTreeView*                       m_pDetailsView = nullptr;
	QBoxLayout*                              m_pAssetsViewLayout = nullptr;
	QFilteringPanel*                         m_pFilterPanel = nullptr;
	QItemSelectionModel*                     m_pSelection = nullptr;
	QLabel*                                  m_pMultipleFoldersLabel = nullptr;
	QSplitter*                               m_pFoldersSplitter = nullptr;
	QSplitter*                               m_pMainViewSplitter = nullptr;
	QThumbnailsView*                         m_pThumbnailView = nullptr;
	QToolButton*                             m_pBackButton = nullptr;
	QToolButton*                             m_pForwardButton = nullptr;

	std::unique_ptr<CAbstractMenu>           m_pThumbnailSizeMenu;
	std::unique_ptr<CAssetDropHandler>       m_pAssetDropHandler;
	std::unique_ptr<CAssetFolderFilterModel> m_pFolderFilterModel;
	std::unique_ptr<CLineEditDelegate>       m_pDetailsViewNewNameDelegate;    // Note that delegates are not owned by view.
	std::unique_ptr<CLineEditDelegate>       m_pThumbnailViewNewNameDelegate;
	std::unique_ptr<CSortFilterProxyModel>   m_pAttributeFilterProxyModel;
	std::unique_ptr<CFilteredFolders>        m_pFilteredFolders;

	const std::vector<CAssetType*>           m_knownAssetTypes;

	//state variables
	QVector<QStringList> m_navigationHistory;
	ViewMode             m_viewMode = Max;
	int                  m_navigationIndex = -1;       //-1 is "all assets"
	bool                 m_dontPushNavHistory = false; //true when folder changes are triggered by back/forward buttons
	bool                 m_recursiveSearch = false;

	int                  m_buttonsSpacing = 4;
	int                  m_buttonGroupsSpacing = 24;

#if ASSET_BROWSER_USE_PREVIEW_WIDGET
	bool OnShowPreview();
	QContainer* m_previewWidget = nullptr;
#endif
};
