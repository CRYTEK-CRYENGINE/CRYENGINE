// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/Editor.h"

#include "Asset.h"

#include <CrySandbox/CrySignal.h>

class QAction;
class QCommandAction;
class QToolButton;

//! Base class for editors integrated with the asset system.
//! One editor window should edit one asset at a time
//TODO : this should handle drag&drop of an asset onto it
class EDITOR_COMMON_API CAssetEditor : public CDockableEditor
{
public:
	//! Use this for your implementation of CAssetType::Edit for convenience.
	//! This should handle most if not all cases, and all asset types should try to follow this model
	//! \param szEditorClassName will use the name with which the editor was registered as a view pane to create it
	//! \param pAsset the asset instance to edit
	static CAssetEditor* OpenAssetForEdit(const char* szEditorClassName, CAsset* pAsset);

	//! Must declare which asset type can be handled by this editor
	CAssetEditor(const char* assetType, QWidget* pParent = nullptr);

	//! Use this if this editor can open several asset types
	CAssetEditor(const QStringList& assetTypes, QWidget* pParent = nullptr);

	//! Starts editing an asset. If this asset is being edited by this editor already, it is not closed and re-opened.
	//! Equivalent to asset->Edit(pAssetEditor);
	//! \return Returns whether this editor is editing the asset.
	bool OpenAsset(CAsset* pAsset);

	//! Returns true if this asset is of a type that is supported by this editor
	bool CanOpenAsset(CAsset* pAsset);

	//! Returns true if this asset type is supported by this editor
	bool CanOpenAsset(const CAssetType* pType);

	//! Closes the currently edited asset. Returns false if the user refused closing in case of unsaved changes.
	bool Close();

	//! Unconditionally discard all changes.
	//! \sa OnDiscardAssetChanges
	void DiscardAssetChanges();

	//! Returns true if this editor is editing an asset
	bool          IsAssetOpened()             { return m_assetBeingEdited != nullptr; }

	CAsset*       GetAssetBeingEdited()       { return m_assetBeingEdited; }
	const CAsset* GetAssetBeingEdited() const { return m_assetBeingEdited; }

	virtual bool  OnSave();   // #TODO Make this method non-virtual.
	virtual bool  OnSaveAs(); // #TODO Make this method non-virtual.

	bool          SaveBackup(const string& backupFolder);

	//! Creates a new instance of asset editing state and returns the IAssetEditingSession interface to manage it.
	//! \return pointer to IAssetEditingSession interface. Can return nullptr if there is no asset in the edit, or if the editor does not support sessions.
	//! \note If the editor supports sessions, it can be closed without losing unsaved changes. In this case, the editing session can be continued later, even in another instance of the editor.
	virtual std::unique_ptr<IAssetEditingSession> CreateEditingSession() { return nullptr; }

	//! Will be called for the subclass to save the asset. Always save the asset in its original file.
	//! In a "SaveAs" situation, changes will be discarded and files will be moved/renamed appropriately, so that this method should not have to care.
	//! Note that editAsset may not have all of its metadata cleared before passing. TODO:improve this!
	virtual bool OnSaveAsset(CEditableAsset& editAsset);

	const std::vector<CAssetType*>& GetSupportedAssetTypes() const { return m_supportedAssetTypes; }

	//! Called immediately after the editor has opened an asset.
	// \sa CAssetEditor::GetAssetBeingEdited()
	// \sa CAssetEditor::IsAssetOpened()
	// \sa CAssetEditor::OnOpenAsset()
	CCrySignal<void()> signalAssetOpened;

	//! Called when the editor is closed the asset.
	//! \sa CAssetEditor::OnCloseAsset
	CCrySignal<void(CAsset*)> signalAssetClosed;

protected:

	//! Called when the editor is about to open the asset.
	//! If the method returns false, it cancels the opening of the asset.
	virtual bool OnOpenAsset(CAsset* pAsset) = 0;

	//! Called when the asset changes are discarded,
	//!if the memory model of the asset still exists after closing,
	//!it needs to be reloaded from file in this method so changes are reverted
	virtual void OnDiscardAssetChanges(CEditableAsset& editAsset) {}

	//! A good place to clean up the editor's internal data. The implementation must unconditionally give up all asset changes, if any.
	virtual void OnCloseAsset() = 0;

	//! OnAboutToCloseAsset returns true if this editor can be safely closed; or false, otherwise.
	//! An editor is considered to be safely closable, if there are neither unsaved asset modifications,
	//! nor background operations that need to call back to the editor after completion.
	//! Asset modification is already handled by the editor in any case, so this method can be used
	//! to add additional checks. The default implementation returns true, which means there are no
	//! additional checks.
	//! OnAboutToCloseAsset is guaranteed to precede every call to OnCloseAsset.
	//! Note that an editor might be closed even when this
	//! \param reason Reason that prevents this editor from closing safely.
	virtual bool OnAboutToCloseAsset(string& reason) const { return true; }

	//! Called when edited asset is changed
	virtual void OnAssetChanged(CAsset& asset, int changeFlags);

	//! Returns true if the editor uses docking widgets.
	virtual bool IsDockingSystemEnabled() const { return true; }

	//! Override this method to register all possible dockable widgets for this editor.
	virtual void OnInitialize() {}

	//! Override this method and use CDockableContainer::SpawnWidget to define the default layout.
	virtual void OnCreateDefaultLayout(CDockableContainer* pSender, QWidget* pAssetBrowser) {}

	virtual bool CanQuit(std::vector<string>& unsavedChanges) final;

	virtual void closeEvent(QCloseEvent* pEvent) final;
	virtual void dragEnterEvent(QDragEnterEvent* pEvent) override;
	virtual void dropEvent(QDropEvent* pEvent) override;

	//! Tries to close this editor. If the editor cannot be safely closed, a dialog is presented that
	//! asks the user whether he wants to save or discard the asset.
	//! An editor is considered to be safely closable, if there are neither unsaved asset modifications,
	//! nor background operations that need to call back to the editor after completion.
	//! \return True, if the editor closes; or false, otherwise.
	bool TryCloseAsset();

private:
	void RegisterActions();
	void InitGenericMenu();
	void InitNewMenu();
	int  GetNewableAssetCount() const;

	void InternalNewAsset(CAssetType* pAssetType);

	void UpdateWindowTitle();
	void SetAssetBeingEdited(CAsset* pAsset);

	//! Creates copy of editing asset with given path
	bool CreateAssetCopy(const string& path);
	//! Creates and opens copy of editing asset with given path. Any changes to editing asset will be discarded
	bool CreateAssetCopyAndOpen(const string& path);

	//! OnAboutToCloseAsset returns true if this editor can be safely closed; or false, otherwise.
	//! An editor is considered to be safely closable, if there are neither unsaved asset modifications,
	//! nor background operations that need to call back to the editor after completion.
	//! This method checks for asset modification.
	//! Subclasses can add additional checks in OnAboutToCloseAsset.
	//! \param reason Reason that prevents this editor from closing safely.
	bool OnAboutToCloseAssetInternal(string& reason) const;

	//! Unconditionally closes the currently opened asset.
	//! The method calls OnCloseAsset and signalAssetClosed.
	void CloseAsset();

	bool OnNew();
	bool OnOpen();
	bool OnClose();

	//Method implemented generically are set to final on purpose
	virtual bool OnOpenFile(const QString& path) override final;
	virtual void Initialize() final;
	virtual void CreateDefaultLayout(CDockableContainer* pSender) final;

	CAsset*                  m_assetBeingEdited;
	std::vector<CAssetType*> m_supportedAssetTypes;
};
