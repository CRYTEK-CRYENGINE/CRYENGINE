// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Control.h"
#include "Folder.h"
#include "Library.h"
#include "ScopeInfo.h"

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

	CAssetsManager();
	~CAssetsManager();

	void             Initialize();
	void             Clear();

	CLibrary*        CreateLibrary(string const& name);
	CLibrary*        GetLibrary(size_t const index) const { return m_libraries[index]; }
	size_t           GetLibraryCount() const              { return m_libraries.size(); }

	CAsset*          CreateFolder(string const& name, CAsset* const pParent = nullptr);
	CControl*        CreateControl(string const& name, EAssetType const type, CAsset* const pParent = nullptr);
	CControl*        CreateDefaultControl(string const& name, EAssetType const type, CAsset* const pParent, bool const isInternal, string const& description);

	CControl*        FindControl(string const& name, EAssetType const type, CAsset* const pParent = nullptr) const;
	CControl*        FindControlById(ControlId const id) const;

	Controls const&  GetControls() const { return m_controls; }

	void             ClearScopes();
	void             AddScope(string const& name, bool const isLocalOnly = false);
	bool             ScopeExists(string const& name) const;
	Scope            GetScope(string const& name) const;
	SScopeInfo       GetScopeInfo(Scope const id) const;
	void             GetScopeInfos(ScopeInfos& scopeInfos) const;

	void             ClearAllConnections();
	void             BackupAndClearAllConnections();
	void             ReloadAllConnections();
	void             DeleteAsset(CAsset* const pAsset);
	void             MoveAssets(CAsset* const pParent, Assets const& assets);
	void             CreateAndConnectImplItems(Impl::IItem* const pIItem, CAsset* const pParent);

	bool             IsTypeDirty(EAssetType const type) const;
	bool             IsDirty() const;
	void             ClearDirtyFlags();
	bool             IsLoading() const { return m_isLoading; }

	void             SetAssetModified(CAsset* const pAsset, bool const isModified);

	void             UpdateAllConnectionStates();

	void             OnControlAboutToBeModified(CControl* const pControl);
	void             OnControlModified(CControl* const pControl);
	void             OnConnectionAdded(CControl* const pControl, Impl::IItem* const pIItem);
	void             OnConnectionRemoved(CControl* const pControl, Impl::IItem* const pIItem);
	void             OnAssetRenamed(CAsset* const pAsset);

	void             UpdateFolderPaths();
	string const&    GetConfigFolderPath() const;
	string const&    GetAssetFolderPath() const;

	FileNames const& GetModifiedLibraries() const { return m_modifiedLibraryNames; }

	CCrySignal<void(bool)>             SignalIsDirty;
	CCrySignal<void()>                 SignalLibraryAboutToBeAdded;
	CCrySignal<void(CLibrary*)>        SignalLibraryAdded;
	CCrySignal<void(CLibrary*)>        SignalLibraryAboutToBeRemoved;
	CCrySignal<void()>                 SignalLibraryRemoved;
	CCrySignal<void(CAsset*)>          SignalAssetAboutToBeAdded;
	CCrySignal<void(CAsset*)>          SignalAssetAdded;
	CCrySignal<void(CAsset*)>          SignalAssetAboutToBeRemoved;
	CCrySignal<void(CAsset*, CAsset*)> SignalAssetRemoved;
	CCrySignal<void(CAsset*)>          SignalAssetRenamed;
	CCrySignal<void(CControl*)>        SignalControlModified;
	CCrySignal<void(CControl*)>        SignalConnectionAdded;
	CCrySignal<void(CControl*)>        SignalConnectionRemoved;

private:

	CAsset*   CreateAndConnectImplItemsRecursively(Impl::IItem* const pIItem, CAsset* const pParent);
	ControlId GenerateUniqueId() { return m_nextId++; }

	void      SetTypeModified(EAssetType const type, bool const isModified);

	void      UpdateLibraryConnectionStates(CAsset* const pAsset);
	void      UpdateAssetConnectionStates(CAsset* const pAsset);

	using ScopesInfo = std::map<Scope, SScopeInfo>;
	using ModifiedSystemTypes = std::set<EAssetType>;

	Libraries           m_libraries;
	static ControlId    m_nextId;
	Controls            m_controls;
	ScopesInfo          m_scopes;
	ModifiedSystemTypes m_modifiedTypes;
	FileNames           m_modifiedLibraryNames;
	bool                m_isLoading = false;

	string              m_configFolderPath;
	string              m_assetFolderPath;
};
} // namespace ACE

