// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include "AudioAssets.h"
#include <IAudioConnection.h>
#include "ACETypes.h"
#include <CrySandbox/CrySignal.h>
#include <array>
#include <QVariant>

namespace ACE
{
class IAudioSystemItem;

static QVariant GetHeaderData(int section, Qt::Orientation orientation, int role)
{
	if (orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
	{
		return "name";
	}

	return QVariant();
}

class CAudioAssetsManager
{
	friend class IUndoControlOperation;
	friend class CUndoControlModified;
	friend class CAudioControl;

public:

	CAudioAssetsManager();
	~CAudioAssetsManager();

	void Initialize();
	void Clear();

	// Libraries
	CAudioLibrary* CreateLibrary(string const& name);
	CAudioLibrary* GetLibrary(size_t const index) const { return m_audioLibraries[index]; }
	size_t         GetLibraryCount() const              { return m_audioLibraries.size(); }

	//
	IAudioAsset*   CreateFolder(string const& name, IAudioAsset* pParent = nullptr);
	CAudioControl* CreateControl(string const& controlName, EItemType type, IAudioAsset* pParent = nullptr);
	void           DeleteItem(IAudioAsset* pItem);

	CAudioControl* GetControlByID(CID id) const;
	CAudioControl* FindControl(string const& controlName, EItemType const type, IAudioAsset* const pParent = nullptr) const;

	using Controls = std::vector<CAudioControl*>;
	Controls const& GetControls() const { return m_controls; }

	// Scope
	void       ClearScopes();
	void       AddScope(string const& name, bool bLocalOnly = false);
	bool       ScopeExists(string const& name) const;
	Scope      GetScope(string const& name) const;
	SScopeInfo GetScopeInfo(Scope id) const;
	void       GetScopeInfoList(ScopeInfoList& scopeList) const;

	// Helper functions
	void ClearAllConnections();
	void ReloadAllConnections();
	void MoveItems(IAudioAsset* pParent, std::vector<IAudioAsset*> const& items);
	void CreateAndConnectImplItems(IAudioSystemItem* pImplItem, IAudioAsset* pParent);

	bool IsTypeDirty(EItemType eType);
	bool IsDirty();
	void ClearDirtyFlags();
	bool IsLoading() const { return m_bLoading; }

	void SetAssetModified(IAudioAsset* pAsset);

	// Library signals
	CCrySignal<void()>               signalLibraryAboutToBeAdded;
	CCrySignal<void(CAudioLibrary*)> signalLibraryAdded;
	CCrySignal<void(CAudioLibrary*)> signalLibraryAboutToBeRemoved;
	CCrySignal<void()>               signalLibraryRemoved;

	// Items signals
	CCrySignal<void(IAudioAsset*)>               signalItemAboutToBeAdded;
	CCrySignal<void(IAudioAsset*)>               signalItemAdded;
	CCrySignal<void(IAudioAsset*)>               signalItemAboutToBeRemoved;
	CCrySignal<void(IAudioAsset*, IAudioAsset*)> signalItemRemoved;

	CCrySignal<void(CAudioControl*)>             signalControlModified;
	CCrySignal<void(CAudioControl*)>             signalConnectionAdded;
	CCrySignal<void(CAudioControl*)>             signalConnectionRemoved;

private:

	IAudioAsset* CreateAndConnectImplItemsRecursively(IAudioSystemItem* pImplItem, IAudioAsset* pParent);
	void         OnControlAboutToBeModified(CAudioControl* pControl);
	void         OnControlModified(CAudioControl* pControl);
	void         OnConnectionAdded(CAudioControl* pControl, IAudioSystemItem* pMiddlewareControl);
	void         OnConnectionRemoved(CAudioControl* pControl, IAudioSystemItem* pMiddlewareControl);

	CID          GenerateUniqueId() { return m_nextId++; }

	std::vector<CAudioLibrary*> m_audioLibraries;

	static CID                  m_nextId;
	Controls                    m_controls;
	std::map<Scope, SScopeInfo> m_scopeMap;
	std::array<bool, 8>         m_bControlTypeModified { false, false, false, false, false, false ,false, false };
	bool                        m_bLoading = false;
};

namespace Utils
{
Scope         GetGlobalScope();
string        GenerateUniqueName(string const& name, EItemType const type, IAudioAsset* const pParent);
string        GenerateUniqueLibraryName(string const& name, CAudioAssetsManager const& assetManager);
string        GenerateUniqueControlName(string const& name, EItemType type, CAudioAssetsManager const& assetManager);
IAudioAsset*  GetParentLibrary(IAudioAsset* pAsset);
void          SelectTopLevelAncestors(std::vector<IAudioAsset*> const& source, std::vector<IAudioAsset*>& dest);
string const& GetAssetFolder();
} // namespace Utils
} // namespace ACE
