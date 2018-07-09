// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetsManager.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "AssetUtils.h"

#include <IItem.h>

#include <FilePathUtil.h>

namespace ACE
{
ControlId CAssetsManager::m_nextId = 1;

//////////////////////////////////////////////////////////////////////////
CAssetsManager::CAssetsManager()
{
	ClearDirtyFlags();
	m_scopes[GlobalScopeId] = SScopeInfo(s_szGlobalScopeName, false);
	m_controls.reserve(8192);
}

//////////////////////////////////////////////////////////////////////////
CAssetsManager::~CAssetsManager()
{
	g_implementationManager.SignalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::SignalAboutToLoad.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::SignalLoaded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::Initialize()
{
	g_implementationManager.SignalImplementationAboutToChange.Connect([this]()
		{
			ClearAllConnections();
	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationChanged.Connect([this]()
		{
			m_isLoading = false;
	  }, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::SignalAboutToLoad.Connect([&]()
		{
			m_isLoading = true;
	  }, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::SignalLoaded.Connect([&]()
		{
			m_isLoading = false;
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CControl* CAssetsManager::CreateControl(string const& name, EAssetType const type, CAsset* const pParent /*= nullptr*/)
{
	CControl* pControl = nullptr;

	if ((pParent != nullptr) && !name.empty())
	{
		EAssetType const parentType = pParent->GetType();

		if (((parentType == EAssetType::Folder) || (parentType == EAssetType::Library)) || ((parentType == EAssetType::Switch) && (type == EAssetType::State)))
		{
			CControl* const pFoundControl = FindControl(name, type, pParent);

			if (pFoundControl != nullptr)
			{
				pControl = pFoundControl;
			}
			else
			{
				SignalAssetAboutToBeAdded(pParent);

				auto const pNewControl = new CControl(name, GenerateUniqueId(), type);

				m_controls.push_back(pNewControl);

				pNewControl->SetParent(pParent);
				pParent->AddChild(pNewControl);

				SignalAssetAdded(pNewControl);
				pNewControl->SetModified(true);

				pControl = pNewControl;
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] Controls must be created on either Libraries or Folders!");
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] No valid parent or name to create control!");
	}

	return pControl;
}

//////////////////////////////////////////////////////////////////////////
CControl* CAssetsManager::CreateDefaultControl(string const& name, EAssetType const type, CAsset* const pParent, bool const isInternal, string const& description)
{
	CControl* pControl = nullptr;

	if (isInternal)
	{
		pControl = CreateControl(name, type, pParent);
	}
	else
	{
		if (type != EAssetType::State)
		{
			pControl = FindControl(name, type);
		}
		else
		{
			pControl = FindControl(name, type, pParent);
		}

		if (pControl == nullptr)
		{
			pControl = CreateControl(name, type, pParent);

			if (pControl != nullptr)
			{
				pControl->SetModified(true, true);
			}
		}
		else
		{
			CAsset* const pPreviousParent = pControl->GetParent();

			if (pPreviousParent != pParent)
			{
				pControl->GetParent()->RemoveChild(pControl);
				pParent->AddChild(pControl);
				pControl->SetParent(pParent);

				pPreviousParent->SetModified(true, true);
				pControl->SetModified(true, true);
			}
		}
	}

	if (pControl != nullptr)
	{
		pControl->SetScope(GlobalScopeId);
		pControl->SetDescription(description);

		if (isInternal)
		{
			pControl->SetFlags(pControl->GetFlags() | (EAssetFlags::IsDefaultControl | EAssetFlags::IsInternalControl));
		}
		else
		{
			pControl->SetFlags(pControl->GetFlags() | EAssetFlags::IsDefaultControl);
		}
	}

	return pControl;
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::ClearScopes()
{
	m_scopes.clear();

	// The global scope must always exist
	m_scopes[GlobalScopeId] = SScopeInfo(s_szGlobalScopeName, false);
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::AddScope(string const& name, bool const isLocalOnly /*= false*/)
{
	m_scopes[CryAudio::StringToId(name.c_str())] = SScopeInfo(name, isLocalOnly);
}

//////////////////////////////////////////////////////////////////////////
bool CAssetsManager::ScopeExists(string const& name) const
{
	return m_scopes.find(CryAudio::StringToId(name.c_str())) != m_scopes.end();
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::GetScopeInfos(ScopeInfos& scopeInfos) const
{
	stl::map_to_vector(m_scopes, scopeInfos);
}

//////////////////////////////////////////////////////////////////////////
Scope CAssetsManager::GetScope(string const& name) const
{
	return CryAudio::StringToId(name.c_str());
}

//////////////////////////////////////////////////////////////////////////
SScopeInfo CAssetsManager::GetScopeInfo(Scope const id) const
{
	return stl::find_in_map(m_scopes, id, SScopeInfo());
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::Clear()
{
	Libraries const libraries = m_libraries;

	for (auto const pLibrary : libraries)
	{
		DeleteAsset(pLibrary);
	}

	CRY_ASSERT_MESSAGE(m_controls.empty(), "m_controls is not empty.");
	CRY_ASSERT_MESSAGE(m_libraries.empty(), "m_systemLibraries is not empty.");
	ClearScopes();
	ClearDirtyFlags();
}

//////////////////////////////////////////////////////////////////////////
CLibrary* CAssetsManager::CreateLibrary(string const& name)
{
	CLibrary* pSystemLibrary = nullptr;
	bool foundLibrary = false;

	if (!name.empty())
	{
		size_t const numLibraries = m_libraries.size();

		for (size_t i = 0; i < numLibraries; ++i)
		{
			CLibrary* const pLibrary = m_libraries[i];

			if ((pLibrary != nullptr) && (name.compareNoCase(pLibrary->GetName()) == 0))
			{
				pSystemLibrary = pLibrary;
				foundLibrary = true;
				break;
			}
		}

		if (!foundLibrary)
		{
			SignalLibraryAboutToBeAdded();
			auto const pLibrary = new CLibrary(name);
			m_libraries.push_back(pLibrary);
			SignalLibraryAdded(pLibrary);
			pLibrary->SetModified(true);
			pSystemLibrary = pLibrary;
		}
	}

	return pSystemLibrary;
}

//////////////////////////////////////////////////////////////////////////
CAsset* CAssetsManager::CreateFolder(string const& name, CAsset* const pParent /*= nullptr*/)
{
	CAsset* pAsset = nullptr;
	bool foundFolder = false;

	if ((pParent != nullptr) && !name.empty())
	{
		if ((pParent->GetType() == EAssetType::Folder) || (pParent->GetType() == EAssetType::Library))
		{
			size_t const numChildren = pParent->ChildCount();

			for (size_t i = 0; i < numChildren; ++i)
			{
				CAsset* const pChild = pParent->GetChild(i);

				if ((pChild != nullptr) && (pChild->GetType() == EAssetType::Folder) && (name.compareNoCase(pChild->GetName()) == 0))
				{
					pAsset = pChild;
					foundFolder = true;
					break;
				}
			}

			if (!foundFolder)
			{
				auto const pFolder = new CFolder(name);

				if (pFolder != nullptr)
				{
					SignalAssetAboutToBeAdded(pParent);
					pParent->AddChild(pFolder);
					pFolder->SetParent(pParent);
				}

				SignalAssetAdded(pFolder);
				pFolder->SetModified(true);
				pAsset = pFolder;
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] Folders must be created on either Libraries or Folders!");
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] No valid parent or name to create folder!");
	}

	return pAsset;
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnControlAboutToBeModified(CControl* const pControl)
{
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnConnectionAdded(CControl* const pControl, Impl::IItem* const pIItem)
{
	SignalConnectionAdded(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnConnectionRemoved(CControl* const pControl, Impl::IItem* const pIItem)
{
	SignalConnectionRemoved(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnAssetRenamed(CAsset* const pAsset)
{
	SignalAssetRenamed(pAsset);
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::UpdateFolderPaths()
{
	string const rootPath = AUDIO_SYSTEM_DATA_ROOT "/";

	if (g_pIImpl != nullptr)
	{
		string const& implFolderPath = rootPath + g_pIImpl->GetFolderName() + "/";
		m_configFolderPath = implFolderPath + CryAudio::s_szConfigFolderName + "/";
		m_assetFolderPath = implFolderPath + CryAudio::s_szAssetsFolderName + "/";
	}
	else
	{
		m_configFolderPath = rootPath;
		m_assetFolderPath = rootPath;
	}
}
//////////////////////////////////////////////////////////////////////////
string const& CAssetsManager::GetConfigFolderPath() const
{
	return m_configFolderPath;
}

//////////////////////////////////////////////////////////////////////////
string const& CAssetsManager::GetAssetFolderPath() const
{
	return m_assetFolderPath;
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnControlModified(CControl* const pControl)
{
	SignalControlModified(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::SetAssetModified(CAsset* const pAsset, bool const isModified)
{
	if (isModified)
	{
		auto const type = pAsset->GetType();
		m_modifiedTypes.emplace(type);

		if (type == EAssetType::Library)
		{
			m_modifiedLibraryNames.emplace(pAsset->GetName());
		}

		SignalIsDirty(true);
		UpdateLibraryConnectionStates(pAsset);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::SetTypeModified(EAssetType const type, bool const isModified)
{
	if (isModified)
	{
		m_modifiedTypes.emplace(type);
	}
}
//////////////////////////////////////////////////////////////////////////
bool CAssetsManager::IsDirty() const
{
	return !m_modifiedTypes.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CAssetsManager::IsTypeDirty(EAssetType const type) const
{
	return m_modifiedTypes.find(type) != m_modifiedTypes.end();
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::ClearDirtyFlags()
{
	m_modifiedTypes.clear();
	m_modifiedLibraryNames.clear();
	SignalIsDirty(false);
}

//////////////////////////////////////////////////////////////////////////
CControl* CAssetsManager::FindControl(string const& name, EAssetType const type, CAsset* const pParent /*= nullptr*/) const
{
	CControl* pSearchedControl = nullptr;

	if (pParent == nullptr)
	{
		for (auto const pControl : m_controls)
		{
			if ((pControl != nullptr) && (pControl->GetName().compareNoCase(name) == 0) && (pControl->GetType() == type))
			{
				pSearchedControl = pControl;
				break;
			}
		}
	}
	else
	{
		size_t const numChildren = pParent->ChildCount();

		for (size_t i = 0; i < numChildren; ++i)
		{
			auto const pControl = static_cast<CControl*>(pParent->GetChild(i));

			if ((pControl != nullptr) && (pControl->GetName().compareNoCase(name) == 0) && (pControl->GetType() == type))
			{
				pSearchedControl = pControl;
				break;
			}
		}
	}

	return pSearchedControl;
}

//////////////////////////////////////////////////////////////////////////
CControl* CAssetsManager::FindControlById(ControlId const id) const
{
	CControl* pSearchedControl = nullptr;

	for (auto const pControl : m_controls)
	{
		if (pControl->GetId() == id)
		{
			pSearchedControl = pControl;
			break;
		}
	}

	return pSearchedControl;
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::ClearAllConnections()
{
	m_isLoading = true;

	for (auto const pControl : m_controls)
	{
		if (pControl != nullptr)
		{
			pControl->ClearConnections();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::BackupAndClearAllConnections()
{
	m_isLoading = true;

	for (auto const pControl : m_controls)
	{
		if (pControl != nullptr)
		{
			pControl->BackupAndClearConnections();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::ReloadAllConnections()
{
	for (auto const pControl : m_controls)
	{
		if (pControl != nullptr)
		{
			pControl->ReloadConnections();
		}
	}

	m_isLoading = false;

	UpdateAllConnectionStates();
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::UpdateAllConnectionStates()
{
	for (auto const pLibrary : m_libraries)
	{
		UpdateAssetConnectionStates(pLibrary);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::UpdateLibraryConnectionStates(CAsset* const pAsset)
{
	CAsset* const pLibrary = AssetUtils::GetParentLibrary(pAsset);

	if (pLibrary != nullptr)
	{
		UpdateAssetConnectionStates(pLibrary);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::UpdateAssetConnectionStates(CAsset* const pAsset)
{
	if (pAsset != nullptr)
	{
		EAssetType const assetType = pAsset->GetType();

		if ((assetType == EAssetType::Library) || (assetType == EAssetType::Folder) || (assetType == EAssetType::Switch))
		{
			bool hasPlaceholder = false;
			bool hasNoConnection = false;
			bool hasControl = false;
			int const childCount = pAsset->ChildCount();

			for (int i = 0; i < childCount; ++i)
			{
				auto const pChild = pAsset->GetChild(i);

				UpdateAssetConnectionStates(pChild);

				EAssetFlags const childFlags = pChild->GetFlags();

				if ((childFlags& EAssetFlags::HasPlaceholderConnection) != 0)
				{
					hasPlaceholder = true;
				}

				if ((childFlags& EAssetFlags::HasConnection) == 0)
				{
					hasNoConnection = true;
				}

				if ((childFlags& EAssetFlags::HasControl) != 0)
				{
					hasControl = true;
				}
			}

			pAsset->SetFlags(hasPlaceholder ? (pAsset->GetFlags() | EAssetFlags::HasPlaceholderConnection) : (pAsset->GetFlags() & ~EAssetFlags::HasPlaceholderConnection));
			pAsset->SetFlags(!hasNoConnection ? (pAsset->GetFlags() | EAssetFlags::HasConnection) : (pAsset->GetFlags() & ~EAssetFlags::HasConnection));
			pAsset->SetFlags(hasControl ? (pAsset->GetFlags() | EAssetFlags::HasControl) : (pAsset->GetFlags() & ~EAssetFlags::HasControl));
		}
		else if (assetType != EAssetType::None)
		{
			auto const pControl = static_cast<CControl*>(pAsset);

			if (pControl != nullptr)
			{
				pControl->SetFlags(pControl->GetFlags() | EAssetFlags::HasControl);

				bool hasPlaceholder = false;
				bool hasConnection = (pControl->GetFlags() & EAssetFlags::IsDefaultControl) != 0;
				size_t const connectionCount = pControl->GetConnectionCount();

				for (size_t i = 0; i < connectionCount; ++i)
				{
					hasConnection = true;
					Impl::IItem const* const pIItem = g_pIImpl->GetItem(pControl->GetConnectionAt(i)->GetID());

					if (pIItem != nullptr)
					{
						if ((pIItem->GetFlags() & EItemFlags::IsPlaceHolder) != 0)
						{
							hasPlaceholder = true;
							break;
						}
					}
				}

				pControl->SetFlags(hasPlaceholder ? (pControl->GetFlags() | EAssetFlags::HasPlaceholderConnection) : (pControl->GetFlags() & ~EAssetFlags::HasPlaceholderConnection));
				pControl->SetFlags(hasConnection ? (pControl->GetFlags() | EAssetFlags::HasConnection) : (pControl->GetFlags() & ~EAssetFlags::HasConnection));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::DeleteAsset(CAsset* const pAsset)
{
	if (pAsset != nullptr)
	{
		// Recursively remove all children as well
		while (pAsset->ChildCount() > 0)
		{
			DeleteAsset(pAsset->GetChild(0));
		}

		EAssetType const type = pAsset->GetType();

		// Inform that we're about to remove the asset
		if (type == EAssetType::Library)
		{
			SignalLibraryAboutToBeRemoved(static_cast<CLibrary*>(pAsset));
		}
		else
		{
			SignalAssetAboutToBeRemoved(pAsset);
		}

		// Remove/detach asset from the tree
		CAsset* const pParent = pAsset->GetParent();

		if (pParent != nullptr)
		{
			pParent->RemoveChild(pAsset);
			pAsset->SetParent(nullptr);
		}

		if (type == EAssetType::Library)
		{
			m_libraries.erase(std::remove(m_libraries.begin(), m_libraries.end(), static_cast<CLibrary*>(pAsset)), m_libraries.end());
			SignalLibraryRemoved();
		}
		else
		{
			if (type != EAssetType::Folder)
			{
				// Must be a control
				auto const pControl = static_cast<CControl*>(pAsset);

				if (pControl != nullptr)
				{
					pControl->ClearConnections();
					m_controls.erase(std::remove_if(m_controls.begin(), m_controls.end(), [&](auto pIterControl) { return pIterControl->GetId() == pControl->GetId(); }), m_controls.end());
					SetTypeModified(type, true);
				}
			}

			SignalAssetRemoved(pParent, pAsset);
		}

		pAsset->SetModified(true);
		delete pAsset;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::MoveAssets(CAsset* const pParent, Assets const& assets)
{
	if (pParent != nullptr)
	{
		for (auto const pAsset : assets)
		{
			if (pAsset != nullptr)
			{
				CAsset* const pPreviousParent = pAsset->GetParent();

				if (pPreviousParent != nullptr)
				{
					SignalAssetAboutToBeRemoved(pAsset);
					pPreviousParent->RemoveChild(pAsset);
					pAsset->SetParent(nullptr);
					SignalAssetRemoved(pPreviousParent, pAsset);
					pPreviousParent->SetModified(true);
				}

				SignalAssetAboutToBeAdded(pParent);
				EAssetType const type = pAsset->GetType();

				if ((type == EAssetType::State) || (type == EAssetType::Folder))
				{
					// To prevent duplicated names of states and folders.
					pAsset->UpdateNameOnMove(pParent);
				}

				pParent->AddChild(pAsset);
				pAsset->SetParent(pParent);
				SignalAssetAdded(pAsset);
				pAsset->SetModified(true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::CreateAndConnectImplItems(Impl::IItem* const pIItem, CAsset* const pParent)
{
	SignalAssetAboutToBeAdded(pParent);
	CAsset* pAsset = CreateAndConnectImplItemsRecursively(pIItem, pParent);
	SignalAssetAdded(pAsset);
}

//////////////////////////////////////////////////////////////////////////
CAsset* CAssetsManager::CreateAndConnectImplItemsRecursively(Impl::IItem* const pIItem, CAsset* const pParent)
{
	CAsset* pAsset = nullptr;

	// Create the new control and connect it to the one dragged in externally
	string name = pIItem->GetName();
	EAssetType type = g_pIImpl->ImplTypeToAssetType(pIItem);

	if (type != EAssetType::None)
	{
		PathUtil::RemoveExtension(name);

		if ((type == EAssetType::Parameter) && (pParent->GetType() == EAssetType::Switch))
		{
			// Create a state instead of a parameter if the parent is a switch.
			type = EAssetType::State;
		}

		if (type != EAssetType::State)
		{
			name = AssetUtils::GenerateUniqueControlName(name, type);
		}
		else
		{
			name = AssetUtils::GenerateUniqueName(name, type, pParent);
		}

		auto const pControl = new CControl(name, GenerateUniqueId(), type);
		pControl->SetParent(pParent);
		pParent->AddChild(pControl);
		m_controls.push_back(pControl);

		ConnectionPtr const pAudioConnection = g_pIImpl->CreateConnectionToControl(pControl->GetType(), pIItem);

		if (pAudioConnection != nullptr)
		{
			pControl->AddConnection(pAudioConnection);
		}

		pAsset = pControl;
	}
	else
	{
		// If the type of the control is invalid then it must be a folder or container
		name = AssetUtils::GenerateUniqueName(name, EAssetType::Folder, pParent);
		auto const pFolder = new CFolder(name);
		pParent->AddChild(pFolder);
		pFolder->SetParent(pParent);

		pAsset = pFolder;
	}

	size_t const numChildren = pIItem->GetNumChildren();

	for (size_t i = 0; i < numChildren; ++i)
	{
		CreateAndConnectImplItemsRecursively(pIItem->GetChildAt(i), pAsset);
	}

	return pAsset;
}
} // namespace ACE

