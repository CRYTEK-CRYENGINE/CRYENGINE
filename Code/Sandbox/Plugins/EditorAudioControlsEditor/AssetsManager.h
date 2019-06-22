// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Control.h"
#include "Folder.h"
#include "Library.h"

#include <CrySandbox/CrySignal.h>

namespace ACE
{
namespace Impl
{
struct IItem;
} // namespace Impl

class CAssetsManager final
{
public:

	CAssetsManager(CAssetsManager const&) = delete;
	CAssetsManager(CAssetsManager&&) = delete;
	CAssetsManager& operator=(CAssetsManager const&) = delete;
	CAssetsManager& operator=(CAssetsManager&&) = delete;

	CAssetsManager();
	~CAssetsManager();

	void             Initialize();
	void             Clear();

	CLibrary*        CreateLibrary(string const& name);
	CLibrary*        GetLibrary(size_t const index) const { return m_libraries[index]; }
	size_t           GetLibraryCount() const              { return m_libraries.size(); }

	CAsset*          CreateFolder(string const& name, CAsset* const pParent);
	CControl*        CreateControl(string const& name, EAssetType const type, CAsset* const pParent);
	CControl*        CreateDefaultControl(string const& name, EAssetType const type, CAsset* const pParent, EAssetFlags const flags, string const& description);

	CControl*        FindControl(string const& name, EAssetType const type, CAsset* const pParent = nullptr) const;
	CControl*        FindControlById(ControlId const id) const;

	Controls const&  GetControls() const { return m_controls; }

	void             ClearAllConnections();
	void             BackupAndClearAllConnections();
	void             ReloadAllConnections();
	void             DeleteAsset(CAsset* const pAsset);
	void             MoveAssets(CAsset* const pParent, Assets const& assets);
	void             CreateAndConnectImplItems(Impl::IItem* const pIItem, CAsset* const pParent);

	bool             IsTypeDirty(EAssetType const type) const;
	bool             IsDirty() const;
	void             ClearDirtyFlags();
	bool             IsLoading() const             { return m_isLoading; }

	bool             ShouldReloadAfterSave() const { return m_reloadAfterSave; }

	void             SetAssetModified(CAsset* const pAsset, bool const isModified);

	void             UpdateAllConnectionStates();

	void             ChangeContext(CryAudio::ContextId const oldContextId, CryAudio::ContextId const newContextId);

	void             OnBeforeControlModified(CControl* const pControl);
	void             OnAfterControlModified(CControl* const pControl);
	void             OnConnectionAdded(CControl* const pControl);
	void             OnConnectionRemoved(CControl* const pControl);
	void             OnAssetRenamed(CAsset* const pAsset);

	void             UpdateConfigFolderPath();
	string const&    GetConfigFolderPath() const;

	FileNames const& GetModifiedLibraries() const { return m_modifiedLibraryNames; }

	CCrySignal<void(bool)>             SignalIsDirty;
	CCrySignal<void()>                 SignalOnBeforeLibraryAdded;
	CCrySignal<void(CLibrary*)>        SignalOnAfterLibraryAdded;
	CCrySignal<void(CLibrary*)>        SignalOnBeforeLibraryRemoved;
	CCrySignal<void()>                 SignalOnAfterLibraryRemoved;
	CCrySignal<void(CAsset*)>          SignalOnBeforeAssetAdded;
	CCrySignal<void(CAsset*)>          SignalOnAfterAssetAdded;
	CCrySignal<void(CAsset*)>          SignalOnBeforeAssetRemoved;
	CCrySignal<void(CAsset*, CAsset*)> SignalOnAfterAssetRemoved;
	CCrySignal<void(CAsset*)>          SignalAssetRenamed;
	CCrySignal<void(CControl*)>        SignalControlModified;
	CCrySignal<void(CControl*)>        SignalConnectionAdded;
	CCrySignal<void(CControl*)>        SignalConnectionRemoved;

private:

	CAsset* CreateAndConnectImplItemsRecursively(Impl::IItem* const pIItem, CAsset* const pParent);

	void    SetTypeModified(EAssetType const type, bool const isModified);

	void    UpdateLibraryConnectionStates(CAsset* const pAsset);
	void    UpdateAssetConnectionStates(CAsset* const pAsset);

	using ModifiedSystemTypes = std::set<EAssetType>;

	Libraries           m_libraries;
	Controls            m_controls;
	ModifiedSystemTypes m_modifiedTypes;
	FileNames           m_modifiedLibraryNames;
	bool                m_isLoading = false;
	bool                m_reloadAfterSave = false;

	string              m_configFolderPath;
};
} // namespace ACE
