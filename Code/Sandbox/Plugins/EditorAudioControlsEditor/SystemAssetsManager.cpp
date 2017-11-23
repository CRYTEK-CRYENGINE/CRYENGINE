// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SystemAssetsManager.h"

#include "AudioControlsEditorUndo.h"
#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"

#include <IEditorImpl.h>
#include <ImplItem.h>

#include <CryString/StringUtils.h>
#include <CryString/CryPath.h>
#include <FilePathUtil.h>
#include <IEditor.h>

namespace ACE
{
CID CSystemAssetsManager::m_nextId = 1;

//////////////////////////////////////////////////////////////////////////
CSystemAssetsManager::CSystemAssetsManager()
{
	ClearDirtyFlags();
	m_scopes[Utils::GetGlobalScope()] = SScopeInfo("global", false);
	m_controls.reserve(8192);
}

//////////////////////////////////////////////////////////////////////////
CSystemAssetsManager::~CSystemAssetsManager()
{
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::signalAboutToLoad.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::signalLoaded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::Initialize()
{
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.Connect([&]()
		{
			ClearAllConnections();
		}, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.Connect([&]()
		{
			ReloadAllConnections();
		}, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::signalAboutToLoad.Connect([&]()
		{
			m_isLoading = true;
		}, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::signalLoaded.Connect([&]()
		{
			m_isLoading = false;
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CSystemControl* CSystemAssetsManager::CreateControl(string const& name, ESystemItemType const type, CSystemAsset* const pParent)
{
	if ((pParent != nullptr) && !name.empty())
	{
		ESystemItemType const parentType = pParent->GetType();

		if (((parentType == ESystemItemType::Folder) || (parentType == ESystemItemType::Library)) || ((parentType == ESystemItemType::Switch) && (type == ESystemItemType::State)))
		{
			CSystemControl* const pFoundControl = FindControl(name, type, pParent);

			if (pFoundControl != nullptr)
			{
				return pFoundControl;
			}

			signalItemAboutToBeAdded(pParent);

			CSystemControl* const pControl = new CSystemControl(name, GenerateUniqueId(), type);

			m_controls.emplace_back(pControl);

			pControl->SetParent(pParent);
			pParent->AddChild(pControl);

			signalItemAdded(pControl);
			pControl->SetModified(true);

			return pControl;
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

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::DeleteItem(CSystemAsset* const pItem)
{
	if (pItem != nullptr)
	{
		// Recursively remove all children as well
		while (pItem->ChildCount() > 0)
		{
			DeleteItem(pItem->GetChild(0));
		}

		ESystemItemType const type = pItem->GetType();

		// Inform that we're about to remove the item
		if (type == ESystemItemType::Library)
		{
			signalLibraryAboutToBeRemoved(static_cast<CSystemLibrary*>(pItem));
		}
		else
		{
			signalItemAboutToBeRemoved(pItem);
		}

		// Remove/detach item from the tree
		CSystemAsset* const pParent = pItem->GetParent();

		if (pParent != nullptr)
		{
			pParent->RemoveChild(pItem);
			pItem->SetParent(nullptr);
		}

		if (type == ESystemItemType::Library)
		{
			m_systemLibraries.erase(std::remove(m_systemLibraries.begin(), m_systemLibraries.end(), static_cast<CSystemLibrary*>(pItem)), m_systemLibraries.end());
			signalLibraryRemoved();
		}
		else
		{
			if (type != ESystemItemType::Folder)
			{
				// Must be a control
				CSystemControl* const pControl = static_cast<CSystemControl*>(pItem);

				if (pControl != nullptr)
				{
					pControl->ClearConnections();
					m_controls.erase(std::remove_if(m_controls.begin(), m_controls.end(), [&](auto pIterControl) { return pIterControl->GetId() == pControl->GetId(); }), m_controls.end());
				}
			}
			signalItemRemoved(pParent, pItem);
		}

		pItem->SetModified(true);
		delete pItem;
	}
}

//////////////////////////////////////////////////////////////////////////
CSystemControl* CSystemAssetsManager::GetControlByID(CID const id) const
{
	for (auto const pControl : m_controls)
	{
		if (pControl->GetId() == id)
		{
			return pControl;
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::ClearScopes()
{
	m_scopes.clear();

	// The global scope must always exist
	m_scopes[Utils::GetGlobalScope()] = SScopeInfo("global", false);
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::AddScope(string const& name, bool const isLocalOnly)
{
	string scopeName = name;
	m_scopes[CCrc32::Compute(scopeName.MakeLower())] = SScopeInfo(scopeName, isLocalOnly);
}

//////////////////////////////////////////////////////////////////////////
bool CSystemAssetsManager::ScopeExists(string const& name) const
{
	string scopeName = name;
	return m_scopes.find(CCrc32::Compute(scopeName.MakeLower())) != m_scopes.end();
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::GetScopeInfoList(ScopeInfoList& scopeList) const
{
	stl::map_to_vector(m_scopes, scopeList);
}

//////////////////////////////////////////////////////////////////////////
Scope CSystemAssetsManager::GetScope(string const& name) const
{
	string scopeName = name;
	scopeName.MakeLower();
	return CCrc32::Compute(scopeName);
}

//////////////////////////////////////////////////////////////////////////
SScopeInfo CSystemAssetsManager::GetScopeInfo(Scope const id) const
{
	return stl::find_in_map(m_scopes, id, SScopeInfo());
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::Clear()
{
	std::vector<CSystemLibrary*> const libraries = m_systemLibraries;

	for (auto const pLibrary : libraries)
	{
		DeleteItem(pLibrary);
	}

	CRY_ASSERT(m_controls.empty());
	CRY_ASSERT(m_systemLibraries.empty());
	ClearScopes();
	ClearDirtyFlags();
}

//////////////////////////////////////////////////////////////////////////
CSystemLibrary* CSystemAssetsManager::CreateLibrary(string const& name)
{
	if (!name.empty())
	{
		size_t const size = m_systemLibraries.size();

		for (size_t i = 0; i < size; ++i)
		{
			CSystemLibrary* const pLibrary = m_systemLibraries[i];

			if ((pLibrary != nullptr) && (name.compareNoCase(pLibrary->GetName()) == 0))
			{
				return pLibrary;
			}
		}

		signalLibraryAboutToBeAdded();
		CSystemLibrary* const pLibrary = new CSystemLibrary(name);
		m_systemLibraries.emplace_back(pLibrary);
		signalLibraryAdded(pLibrary);
		pLibrary->SetModified(true);
		return pLibrary;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
CSystemAsset* CSystemAssetsManager::CreateFolder(string const& name, CSystemAsset* const pParent)
{
	if ((pParent != nullptr) && !name.empty())
	{
		if ((pParent->GetType() == ESystemItemType::Folder) || (pParent->GetType() == ESystemItemType::Library))
		{
			size_t const size = pParent->ChildCount();

			for (size_t i = 0; i < size; ++i)
			{
				CSystemAsset* const pItem = pParent->GetChild(i);

				if ((pItem != nullptr) && (pItem->GetType() == ESystemItemType::Folder) && (name.compareNoCase(pItem->GetName()) == 0))
				{
					return pItem;
				}
			}

			CSystemFolder* const pFolder = new CSystemFolder(name);

			if (pFolder != nullptr)
			{
				signalItemAboutToBeAdded(pParent);
				pParent->AddChild(pFolder);
				pFolder->SetParent(pParent);
			}

			signalItemAdded(pFolder);
			pFolder->SetModified(true);
			return pFolder;
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

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::OnControlAboutToBeModified(CSystemControl* const pControl)
{
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::OnConnectionAdded(CSystemControl* const pControl, CImplItem* const pImplControl)
{
	signalConnectionAdded(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::OnConnectionRemoved(CSystemControl* const pControl, CImplItem* const pImplControl)
{
	signalConnectionRemoved(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::OnAssetRenamed()
{
	signalAssetRenamed();
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::OnControlModified(CSystemControl* const pControl)
{
	signalControlModified(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::SetAssetModified(CSystemAsset* const pAsset, bool const isModified)
{
	if (isModified)
	{
		UpdateLibraryConnectionStates(pAsset);
		auto const type = pAsset->GetType();

		m_modifiedTypes.emplace(type);

		if (type == ESystemItemType::Library)
		{
			m_modifiedLibraryNames.emplace(pAsset->GetName());
		}

		signalIsDirty(true);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSystemAssetsManager::IsDirty() const
{
	return !m_modifiedTypes.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CSystemAssetsManager::IsTypeDirty(ESystemItemType const type) const
{
	return m_modifiedTypes.find(type) != m_modifiedTypes.end();
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::ClearDirtyFlags()
{
	m_modifiedTypes.clear();
	m_modifiedLibraryNames.clear();
	signalIsDirty(false);
}

//////////////////////////////////////////////////////////////////////////
CSystemControl* CSystemAssetsManager::FindControl(string const& controlName, ESystemItemType const type, CSystemAsset* const pParent) const
{
	if (pParent == nullptr)
	{
		for (auto const pControl : m_controls)
		{
			if ((pControl != nullptr) && (pControl->GetName() == controlName) && (pControl->GetType() == type))
			{
				return pControl;
			}
		}
	}
	else
	{
		size_t const size = pParent->ChildCount();

		for (size_t i = 0; i < size; ++i)
		{
			CSystemControl* const pControl = static_cast<CSystemControl*>(pParent->GetChild(i));

			if ((pControl != nullptr) && (pControl->GetName() == controlName) && (pControl->GetType() == type))
			{
				return pControl;
			}
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::ClearAllConnections()
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
void CSystemAssetsManager::ReloadAllConnections()
{
	for (auto const pControl : m_controls)
	{
		if (pControl != nullptr)
		{
			pControl->ClearConnections();
			pControl->ReloadConnections();
		}
	}
	
	m_isLoading = false;
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::UpdateAllConnectionStates()
{
	for (auto const pLibrary : m_systemLibraries)
	{
		UpdateAssetConnectionStates(pLibrary);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::UpdateLibraryConnectionStates(CSystemAsset* pAsset)
{
	while ((pAsset != nullptr) && (pAsset->GetType() != ESystemItemType::Library))
	{
		pAsset = pAsset->GetParent();
	}

	if (pAsset != nullptr)
	{
		UpdateAssetConnectionStates(pAsset);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::UpdateAssetConnectionStates(CSystemAsset* const pAsset)
{
	if (pAsset != nullptr)
	{
		ESystemItemType const type = pAsset->GetType();

		if ((type == ESystemItemType::Library) || (type == ESystemItemType::Folder) || (type == ESystemItemType::Switch))
		{
			bool hasPlaceholder = false;
			bool hasNoConnection = false;
			bool hasControl = false;
			int const childCount = pAsset->ChildCount();

			for (int i = 0; i < childCount; ++i)
			{
				auto const pChild = pAsset->GetChild(i);

				UpdateAssetConnectionStates(pChild);

				if (pChild->HasPlaceholderConnection())
				{
					hasPlaceholder = true;
				}

				if (!pChild->HasConnection())
				{
					hasNoConnection = true;
				}
				
				if (pChild->HasControl())
				{
					hasControl = true;
				}
			}

			pAsset->SetHasPlaceholderConnection(hasPlaceholder);
			pAsset->SetHasConnection(!hasNoConnection);
			pAsset->SetHasControl(hasControl);
		}
		else if (type != ESystemItemType::Invalid)
		{
			CSystemControl* const pControl = static_cast<CSystemControl*>(pAsset);

			if (pControl != nullptr)
			{
				pControl->SetHasControl(true);
				bool hasPlaceholder = false;
				bool hasConnection = false;
				int const connectionCount = pControl->GetConnectionCount();

				for (int i = 0; i < connectionCount; ++i)
				{
					hasConnection = true;
					IEditorImpl const* const pEditorImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();

					if (pEditorImpl != nullptr)
					{
						CImplItem const* const pImpleControl = pEditorImpl->GetControl(pControl->GetConnectionAt(i)->GetID());

						if (pImpleControl != nullptr)
						{
							if (pImpleControl->IsPlaceholder())
							{
								hasPlaceholder = true;
							}
						}
					}
				}

				pControl->SetHasPlaceholderConnection(hasPlaceholder);
				pControl->SetHasConnection(hasConnection);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::MoveItems(CSystemAsset* const pParent, std::vector<CSystemAsset*> const& items)
{
	if (pParent != nullptr)
	{
		for (auto const pItem : items)
		{
			if (pItem != nullptr)
			{
				CSystemAsset* const pPreviousParent = pItem->GetParent();

				if (pPreviousParent != nullptr)
				{
					signalItemAboutToBeRemoved(pItem);
					pPreviousParent->RemoveChild(pItem);
					pItem->SetParent(nullptr);
					signalItemRemoved(pPreviousParent, pItem);
					pPreviousParent->SetModified(true);
				}

				signalItemAboutToBeAdded(pParent);
				pParent->AddChild(pItem);
				pItem->SetParent(pParent);
				signalItemAdded(pItem);
				pItem->SetModified(true);
			}
		}

		pParent->SetModified(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemAssetsManager::CreateAndConnectImplItems(CImplItem* const pImplItem, CSystemAsset* const pParent)
{
	signalItemAboutToBeAdded(pParent);
	CSystemAsset* pItem = CreateAndConnectImplItemsRecursively(pImplItem, pParent);
	signalItemAdded(pItem);
}

//////////////////////////////////////////////////////////////////////////
CSystemAsset* CSystemAssetsManager::CreateAndConnectImplItemsRecursively(CImplItem* const pImplItem, CSystemAsset* const pParent)
{
	CSystemAsset* pItem = nullptr;

	// Create the new control and connect it to the one dragged in externally
	IEditorImpl* const pEditorImpl = CAudioControlsEditorPlugin::GetImplEditor();

	string name = pImplItem->GetName();
	ESystemItemType const type = pEditorImpl->ImplTypeToSystemType(pImplItem);

	if (type != ESystemItemType::Invalid)
	{
		PathUtil::RemoveExtension(name);

		if (type != ESystemItemType::State)
		{
			name = Utils::GenerateUniqueControlName(name, type, *this);
		}
		else
		{
			name = Utils::GenerateUniqueName(name, type, pParent);
		}

		CSystemControl* const pControl = new CSystemControl(name, GenerateUniqueId(), type);
		pControl->SetParent(pParent);
		pParent->AddChild(pControl);
		m_controls.emplace_back(pControl);

		ConnectionPtr const pAudioConnection = pEditorImpl->CreateConnectionToControl(pControl->GetType(), pImplItem);

		if (pAudioConnection != nullptr)
		{
			pControl->AddConnection(pAudioConnection);
		}

		pItem = pControl;
	}
	else
	{
		// If the type of the control is invalid then it must be a folder or container
		name = Utils::GenerateUniqueName(name, ESystemItemType::Folder, pParent);
		CSystemFolder* const pFolder = new CSystemFolder(name);
		pParent->AddChild(pFolder);
		pFolder->SetParent(pParent);

		pItem = pFolder;
	}

	size_t const size = pImplItem->ChildCount();

	for (size_t i = 0; i < size; ++i)
	{
		CreateAndConnectImplItemsRecursively(pImplItem->GetChildAt(i), pItem);
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
namespace Utils
{
//////////////////////////////////////////////////////////////////////////
Scope GetGlobalScope()
{
	static Scope const globalScopeId = CCrc32::Compute("global");
	return globalScopeId;
}

//////////////////////////////////////////////////////////////////////////
string GenerateUniqueName(string const& name, ESystemItemType const type, CSystemAsset* const pParent)
{
	string finalName = name;

	if (pParent != nullptr)
	{
		size_t const size = pParent->ChildCount();
		std::vector<string> names;
		names.reserve(size);

		for (size_t i = 0; i < size; ++i)
		{
			CSystemAsset* const pChild = pParent->GetChild(i);

			if ((pChild != nullptr) && (pChild->GetType() == type))
			{
				names.emplace_back(pChild->GetName());
			}
		}

		finalName = PathUtil::GetUniqueName(name, names);
	}

	return finalName;
}

//////////////////////////////////////////////////////////////////////////
string GenerateUniqueLibraryName(string const& name, CSystemAssetsManager const& assetManager)
{
	size_t const size = assetManager.GetLibraryCount();
	std::vector<string> names;
	names.reserve(size);

	for (size_t i = 0; i < size; ++i)
	{
		CSystemLibrary* const pLibrary = assetManager.GetLibrary(i);

		if (pLibrary != nullptr)
		{
			names.emplace_back(pLibrary->GetName());
		}
	}

	return PathUtil::GetUniqueName(name, names);
}

//////////////////////////////////////////////////////////////////////////
string GenerateUniqueControlName(string const& name, ESystemItemType const type, CSystemAssetsManager const& assetManager)
{
	CSystemAssetsManager::Controls const& controls(assetManager.GetControls());
	std::vector<string> names;
	names.reserve(controls.size());

	for (auto const pControl : controls)
	{
		names.emplace_back(pControl->GetName());
	}

	return PathUtil::GetUniqueName(name, names);
}

//////////////////////////////////////////////////////////////////////////
CSystemAsset* GetParentLibrary(CSystemAsset* pAsset)
{
	while ((pAsset != nullptr) && (pAsset->GetType() != ESystemItemType::Library))
	{
		pAsset = pAsset->GetParent();
	}

	return pAsset;
}

//////////////////////////////////////////////////////////////////////////
void SelectTopLevelAncestors(std::vector<CSystemAsset*> const& source, std::vector<CSystemAsset*>& dest)
{
	for (auto const pItem : source)
	{
		// Check if item has ancestors that are also selected
		bool isAncestorAlsoSelected = false;

		for (auto const pOtherItem : source)
		{
			if (pItem != pOtherItem)
			{
				// Find if pOtherItem is the ancestor of pItem
				CSystemAsset const* pParent = pItem->GetParent();

				while (pParent != nullptr)
				{
					if (pParent == pOtherItem)
					{
						break;
					}

					pParent = pParent->GetParent();
				}

				if (pParent != nullptr)
				{
					isAncestorAlsoSelected = true;
					break;
				}
			}
		}

		if (!isAncestorAlsoSelected)
		{
			dest.emplace_back(pItem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string const& GetAssetFolder()
{
	static string const path = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "ace" CRY_NATIVE_PATH_SEPSTR;
	return path;
}
} // namespace Utils
} // namespace ACE
