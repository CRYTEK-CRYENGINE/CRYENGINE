// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetsManager.h"

#include "AudioControlsEditorPlugin.h"
#include "ImplManager.h"
#include "AssetUtils.h"
#include "Common/IConnection.h"
#include "Common/IImpl.h"
#include "Common/IItem.h"

#include <PathUtils.h>

namespace ACE
{
constexpr uint16 g_controlPoolSize = 8192;
constexpr uint16 g_folderPoolSize = 1024;
constexpr uint16 g_libraryPoolSize = 256;

//////////////////////////////////////////////////////////////////////////
CAssetsManager::CAssetsManager()
{
	ClearDirtyFlags();
	m_controls.reserve(static_cast<size_t>(g_controlPoolSize));
}

//////////////////////////////////////////////////////////////////////////
CAssetsManager::~CAssetsManager()
{
	g_implManager.SignalOnBeforeImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implManager.SignalOnAfterImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::SignalOnBeforeLoad.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::SignalOnAfterLoad.DisconnectById(reinterpret_cast<uintptr_t>(this));
	Clear();

	CControl::FreeMemoryPool();
	CFolder::FreeMemoryPool();
	CLibrary::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::Initialize()
{
	CControl::CreateAllocator(g_controlPoolSize);
	CFolder::CreateAllocator(g_folderPoolSize);
	CLibrary::CreateAllocator(g_libraryPoolSize);

	g_implManager.SignalOnBeforeImplChange.Connect([this]()
		{
			ClearAllConnections();
		}, reinterpret_cast<uintptr_t>(this));

	g_implManager.SignalOnAfterImplChange.Connect([this]()
		{
			m_isLoading = false;
		}, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::SignalOnBeforeLoad.Connect([&]()
	    {
	                                                       m_isLoading = true;
			}, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::SignalOnAfterLoad.Connect([&]()
	    {
	                                                      m_isLoading = false;
			}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CControl* CAssetsManager::CreateControl(string const& name, EAssetType const type, CAsset* const pParent)
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
				SignalOnBeforeAssetAdded(pParent);

				ControlId const controlId = (type != EAssetType::State) ? AssetUtils::GenerateUniqueAssetId(name, type) : AssetUtils::GenerateUniqueStateId(pParent->GetName(), name);
				auto const pNewControl = new CControl(name, controlId, type);
				m_controls.push_back(pNewControl);

				pNewControl->SetParent(pParent);
				pParent->AddChild(pNewControl);

				SignalOnAfterAssetAdded(pNewControl);
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
CControl* CAssetsManager::CreateDefaultControl(string const& name, EAssetType const type, CAsset* const pParent, EAssetFlags const flags, string const& description)
{
	CControl* pControl = nullptr;

	if ((flags& EAssetFlags::IsInternalControl) != 0)
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
		pControl->SetContextId(CryAudio::GlobalContextId);
		pControl->SetDescription(description);
		pControl->SetFlags(pControl->GetFlags() | flags);
	}

	return pControl;
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::Clear()
{
	Libraries const libraries = m_libraries;

	for (auto const pLibrary : libraries)
	{
		DeleteAsset(pLibrary);
	}

	CRY_ASSERT_MESSAGE(m_controls.empty(), "m_controls is not empty during %s", __FUNCTION__);
	CRY_ASSERT_MESSAGE(m_libraries.empty(), "m_systemLibraries is not empty during %s", __FUNCTION__);
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
			SignalOnBeforeLibraryAdded();
			auto const pLibrary = new CLibrary(name, AssetUtils::GenerateUniqueAssetId(name, EAssetType::Library));
			m_libraries.push_back(pLibrary);
			SignalOnAfterLibraryAdded(pLibrary);
			pLibrary->SetModified(true);
			pSystemLibrary = pLibrary;
		}
	}

	return pSystemLibrary;
}

//////////////////////////////////////////////////////////////////////////
CAsset* CAssetsManager::CreateFolder(string const& name, CAsset* const pParent)
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
				auto const pFolder = new CFolder(name, AssetUtils::GenerateUniqueFolderId(name, pParent));

				if (pFolder != nullptr)
				{
					SignalOnBeforeAssetAdded(pParent);
					pParent->AddChild(pFolder);
					pFolder->SetParent(pParent);
				}

				SignalOnAfterAssetAdded(pFolder);
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
void CAssetsManager::OnBeforeControlModified(CControl* const pControl)
{
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnConnectionAdded(CControl* const pControl)
{
	SignalConnectionAdded(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnConnectionRemoved(CControl* const pControl)
{
	SignalConnectionRemoved(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnAssetRenamed(CAsset* const pAsset)
{
	SignalAssetRenamed(pAsset);
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::UpdateConfigFolderPath()
{
	if (g_pIImpl != nullptr)
	{
		m_configFolderPath = CRY_AUDIO_DATA_ROOT "/" + string(g_implInfo.folderName) + "/" + CryAudio::g_szConfigFolderName + "/";
	}
	else
	{
		m_configFolderPath = CRY_AUDIO_DATA_ROOT "/";
	}
}
//////////////////////////////////////////////////////////////////////////
string const& CAssetsManager::GetConfigFolderPath() const
{
	return m_configFolderPath;
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::OnAfterControlModified(CControl* const pControl)
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
	m_reloadAfterSave = false;
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
	if ((g_pIImpl != nullptr) && (pAsset != nullptr))
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
			SignalOnBeforeLibraryRemoved(static_cast<CLibrary*>(pAsset));
		}
		else
		{
			SignalOnBeforeAssetRemoved(pAsset);
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
			auto const pLibrary = static_cast<CLibrary*>(pAsset);
			m_libraries.erase(std::remove(m_libraries.begin(), m_libraries.end(), pLibrary), m_libraries.end());
			SignalOnAfterLibraryRemoved();

			if ((pLibrary->GetPakStatus() & EPakStatus::InPak) != 0)
			{
				m_reloadAfterSave = true;
			}
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

			SignalOnAfterAssetRemoved(pParent, pAsset);
		}

		pAsset->SetModified(true);
		delete pAsset;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::MoveAssets(CAsset* const pParent, Assets const& assets)
{
	for (auto const pAsset : assets)
	{
		CAsset* const pPreviousParent = pAsset->GetParent();

		if (pPreviousParent != nullptr)
		{
			SignalOnBeforeAssetRemoved(pAsset);
			pPreviousParent->RemoveChild(pAsset);
			pAsset->SetParent(nullptr);
			SignalOnAfterAssetRemoved(pPreviousParent, pAsset);
			pPreviousParent->SetModified(true);
		}

		SignalOnBeforeAssetAdded(pParent);

		switch (pAsset->GetType())
		{
		case EAssetType::State: // Intentional fall-through.
		case EAssetType::Folder:
			{
				// To prevent duplicated names of states and folders.
				pAsset->UpdateNameOnMove(pParent);
				break;
			}
		default:
			{
				break;
			}
		}

		pParent->AddChild(pAsset);
		pAsset->SetParent(pParent);
		SignalOnAfterAssetAdded(pAsset);
		pAsset->SetModified(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::CreateAndConnectImplItems(Impl::IItem* const pIItem, CAsset* const pParent)
{
	SignalOnBeforeAssetAdded(pParent);
	CAsset* const pAsset = CreateAndConnectImplItemsRecursively(pIItem, pParent);
	SignalOnAfterAssetAdded(pAsset);
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

		ControlId const controlId = (type != EAssetType::State) ? AssetUtils::GenerateUniqueAssetId(name, type) : AssetUtils::GenerateUniqueStateId(pParent->GetName(), name);
		auto const pControl = new CControl(name, controlId, type);
		pControl->SetParent(pParent);
		pParent->AddChild(pControl);
		m_controls.push_back(pControl);

		IConnection* const pIConnection = g_pIImpl->CreateConnectionToControl(pControl->GetType(), pIItem);

		if (pIConnection != nullptr)
		{
			pControl->AddConnection(pIConnection);
		}

		pAsset = pControl;
	}
	else
	{
		// If the type of the control is invalid then it must be a folder or container
		name = AssetUtils::GenerateUniqueName(name, EAssetType::Folder, pParent);
		auto const pFolder = new CFolder(name, AssetUtils::GenerateUniqueFolderId(name, pParent));
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

//////////////////////////////////////////////////////////////////////////
void CAssetsManager::ChangeContext(CryAudio::ContextId const oldContextId, CryAudio::ContextId const newContextId)
{
	for (auto const pControl : m_controls)
	{
		if (pControl->GetContextId() == oldContextId)
		{
			pControl->SetContextId(newContextId);
		}
	}
}
} // namespace ACE
