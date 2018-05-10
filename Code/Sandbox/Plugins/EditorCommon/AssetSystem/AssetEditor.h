// Copyright 2001-2016 Crytek GmbH. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorFramework/Editor.h"

#include "Asset.h"

#include <CrySandbox/CrySignal.h>

//! Base class for editors integrated with the asset system.
//! One editor window should edit one asset at a time
//TODO : this should handle drag&drop of an asset onto it
class EDITOR_COMMON_API CAssetEditor : public CDockableEditor
{
public:
	//! Use this for your implementation of CAssetType::Edit for convenience.
	//! This should handle most if not all cases, and all asset types should try to follow this model
	//! editorClassName will use the name with which the editor was registered as a view pane to create it
	static CAssetEditor* OpenAssetForEdit(const char* editorClassName, CAsset* asset);

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

	//! Closes the currently edited asset. Returns false if the user refused closing in case of unsaved changess
	bool Close();

	//! Saves the currently edited asset.
	bool Save();

	//! Returns true if this editor is editing an asset
	bool IsAssetOpened() { return m_assetBeingEdited != nullptr; }

	CAsset* GetAssetBeingEdited() { return m_assetBeingEdited; }
	const CAsset* GetAssetBeingEdited() const { return m_assetBeingEdited; }

	virtual bool OnSave() override;    // #TODO Make this method final.
	virtual bool OnSaveAs() override;  // #TODO Make this method final.

	bool SaveBackup(const string& backupFolder);

	//! Returns true if the asset being edited is read-only
	bool IsReadOnly() const;

	CCrySignal<void(CAsset*)> signalAssetClosed;

protected:

	virtual bool OnOpenAsset(CAsset* pAsset) = 0;

	//! Will be called for the subclass to save the asset. Always save the asset in its original file.
	//! In a "SaveAs" situation, changes will be discarded and files will be moved/renamed appropriately, so that this method should not have to care.
	//! Note that editAsset may not have all of its metadata cleared before passing. TODO:improve this!
	virtual bool OnSaveAsset(CEditableAsset& editAsset) = 0;

	//! Called when the asset changes are discarded, 
	//!if the memory model of the asset still exists after closing, 
	//!it needs to be reloaded from file in this method so changes are reverted
	virtual void OnDiscardAssetChanges() {}

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

	//! Tries to close this editor. If the editor cannot be safely closed, a dialog is presented that
	//! asks the user whether he wants to save or discard the asset.
	//! An editor is considered to be safely closable, if there are neither unsaved asset modifications,
	//! nor background operations that need to call back to the editor after completion.
	//! \return True, if the editor closes; or false, otherwise.
	bool TryCloseAsset();

	//! Called when edited asset is changed
	virtual void OnAssetChanged(CAsset& asset, int changeFlags);

	//! Called when read-only status of asset has changed, all editors must implement this as it is critical to prevent modifications of read-only assets
	virtual void OnReadOnlyChanged() {}

	virtual bool CanQuit(std::vector<string>& unsavedChanges) final;

	virtual void closeEvent(QCloseEvent* pEvent) final;
	virtual void dragEnterEvent(QDragEnterEvent* pEvent) override;
	virtual void dropEvent(QDropEvent* pEvent) override;

private:
	void Init();
	void InitGenericMenu();
	void InitNewMenu();
	int GetNewableAssetCount() const;

	void InternalNewAsset(CAssetType* pAssetType);
	bool InternalSaveAsset(CAsset* pAsset);
	bool InternalSaveAs(const string& newAssetPath);

	void UpdateWindowTitle();
	void SetAssetBeingEdited(CAsset* pAsset);

	//! OnAboutToCloseAsset returns true if this editor can be safely closed; or false, otherwise.
	//! An editor is considered to be safely closable, if there are neither unsaved asset modifications,
	//! nor background operations that need to call back to the editor after completion.
	//! This method checks for asset modification.
	//! Subclasses can add additional checks in OnAboutToCloseAsset.
	//! \param reason Reason that prevents this editor from closing safely.
	bool OnAboutToCloseAssetInternal(string& reason) const;

	//Methods implemented generically are set to final on purpose
	virtual bool OnNew() override final;
	virtual bool OnOpen() override final;
	virtual bool OnOpenFile(const QString& path) override final;
	virtual bool OnClose() override final;

	CAsset* m_assetBeingEdited;
	std::vector<CAssetType*> m_supportedAssetTypes;
};

