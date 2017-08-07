// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioAssetsManager.h"
#include <CryString/StringUtils.h>
#include "AudioControlsEditorUndo.h"
#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "IAudioSystemEditor.h"
#include "IAudioSystemItem.h"
#include <IEditor.h>
#include <CryString/CryPath.h>
#include <FilePathUtil.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
uint ItemTypeToIndex(EItemType const type)
{
	switch (type)
	{
	case EItemType::eItemType_Trigger:
		return 0;
	case EItemType::eItemType_Parameter:
		return 1;
	case EItemType::eItemType_Switch:
		return 2;
	case EItemType::eItemType_State:
		return 3;
	case EItemType::eItemType_Environment:
		return 4;
	case EItemType::eItemType_Preload:
		return 5;
	case EItemType::eItemType_Folder:
		return 6;
	case EItemType::eItemType_Library:
		return 7;
	}

	return 0;
}

CID CAudioAssetsManager::m_nextId = 1;

//////////////////////////////////////////////////////////////////////////
CAudioAssetsManager::CAudioAssetsManager()
{
	ClearDirtyFlags();
	m_scopeMap[Utils::GetGlobalScope()] = SScopeInfo("global", false);
	m_controls.reserve(8192);
}

//////////////////////////////////////////////////////////////////////////
CAudioAssetsManager::~CAudioAssetsManager()
{
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::signalAboutToLoad.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::signalLoaded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::Initialize()
{
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationAboutToChange.Connect([&]()
		{
			m_bLoading = true;
			ClearAllConnections();
	  }, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.Connect([&]()
		{
			ReloadAllConnections();
			m_bLoading = false;
	  }, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::signalAboutToLoad.Connect([&]()
		{
			m_bLoading = true;
	  });

	CAudioControlsEditorPlugin::signalLoaded.Connect([&]()
		{
			m_bLoading = false;
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CAudioControl* CAudioAssetsManager::CreateControl(string const& name, EItemType type, IAudioAsset* pParent)
{
	if (pParent != nullptr && !name.empty())
	{
		if ((pParent->GetType() == EItemType::eItemType_Folder || pParent->GetType() == EItemType::eItemType_Library) ||
		    (pParent->GetType() == EItemType::eItemType_Switch && type == EItemType::eItemType_State))
		{
			CAudioControl* const pFoundControl = FindControl(name, type, pParent);
			if (pFoundControl != nullptr)
			{
				return pFoundControl;
			}

			signalItemAboutToBeAdded(pParent);

			CAudioControl* pControl = new CAudioControl(name, GenerateUniqueId(), type);

			m_controls.push_back(pControl);

			pControl->SetParent(pParent);
			pParent->AddChild(pControl);

			signalItemAdded(pControl);
			SetAssetModified(pControl);

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
void CAudioAssetsManager::DeleteItem(IAudioAsset* pItem)
{
	if (pItem)
	{
		// Recursively remove all children as well
		while (pItem->ChildCount() > 0)
		{
			DeleteItem(pItem->GetChild(0));
		}

		EItemType const type = pItem->GetType();

		// Inform that we're about to remove the item
		if (type == EItemType::eItemType_Library)
		{
			signalLibraryAboutToBeRemoved(static_cast<CAudioLibrary*>(pItem));
		}
		else
		{
			signalItemAboutToBeRemoved(pItem);
		}

		// Remove/detach item from the tree
		IAudioAsset* pParent = pItem->GetParent();
		if (pParent)
		{
			pParent->RemoveChild(pItem);
			pItem->SetParent(nullptr);
		}

		if (type == EItemType::eItemType_Library)
		{
			m_audioLibraries.erase(std::remove(m_audioLibraries.begin(), m_audioLibraries.end(), static_cast<CAudioLibrary*>(pItem)), m_audioLibraries.end());
			signalLibraryRemoved();
		}
		else
		{
			if (type != EItemType::eItemType_Folder)
			{
				// Must be a control
				CAudioControl* pControl = static_cast<CAudioControl*>(pItem);
				if (pControl)
				{
					pControl->ClearConnections();
					m_controls.erase(std::remove_if(m_controls.begin(), m_controls.end(), [&](auto pIterControl) { return pIterControl->GetId() == pControl->GetId(); }), m_controls.end());
				}
			}
			signalItemRemoved(pParent, pItem);
		}

		SetAssetModified(pItem);
		delete pItem;
	}
}

//////////////////////////////////////////////////////////////////////////
CAudioControl* CAudioAssetsManager::GetControlByID(CID id) const
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
void CAudioAssetsManager::ClearScopes()
{
	m_scopeMap.clear();

	// The global scope must always exist
	m_scopeMap[Utils::GetGlobalScope()] = SScopeInfo("global", false);
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::AddScope(string const& name, bool bLocalOnly)
{
	string scopeName = name;
	m_scopeMap[CCrc32::Compute(scopeName.MakeLower())] = SScopeInfo(scopeName, bLocalOnly);
}

//////////////////////////////////////////////////////////////////////////
bool CAudioAssetsManager::ScopeExists(string const& name) const
{
	string scopeName = name;
	return m_scopeMap.find(CCrc32::Compute(scopeName.MakeLower())) != m_scopeMap.end();
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::GetScopeInfoList(ScopeInfoList& scopeList) const
{
	stl::map_to_vector(m_scopeMap, scopeList);
}

//////////////////////////////////////////////////////////////////////////
Scope CAudioAssetsManager::GetScope(string const& name) const
{
	string scopeName = name;
	scopeName.MakeLower();
	return CCrc32::Compute(scopeName);
}

//////////////////////////////////////////////////////////////////////////
SScopeInfo CAudioAssetsManager::GetScopeInfo(Scope id) const
{
	return stl::find_in_map(m_scopeMap, id, SScopeInfo());
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::Clear()
{
	std::vector<CAudioLibrary*> libraries = m_audioLibraries;
	for (auto pLibrary : libraries)
	{
		DeleteItem(pLibrary);
	}

	CRY_ASSERT(m_controls.empty());
	CRY_ASSERT(m_audioLibraries.empty());
	ClearScopes();
	ClearDirtyFlags();
}

//////////////////////////////////////////////////////////////////////////
CAudioLibrary* CAudioAssetsManager::CreateLibrary(string const& name)
{
	if (!name.empty())
	{
		size_t const size = m_audioLibraries.size();
		for (size_t i = 0; i < size; ++i)
		{
			CAudioLibrary* pLibrary = m_audioLibraries[i];
			if (pLibrary && (name.compareNoCase(pLibrary->GetName()) == 0))
			{
				return pLibrary;
			}
		}

		signalLibraryAboutToBeAdded();
		CAudioLibrary* pLibrary = new CAudioLibrary(name);
		m_audioLibraries.push_back(pLibrary);
		signalLibraryAdded(pLibrary);
		SetAssetModified(pLibrary);
		pLibrary->SetModified(true);
		return pLibrary;
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
ACE::IAudioAsset* CAudioAssetsManager::CreateFolder(string const& name, IAudioAsset* pParent)
{
	if (pParent != nullptr && !name.empty())
	{
		if (pParent->GetType() == EItemType::eItemType_Folder || pParent->GetType() == EItemType::eItemType_Library)
		{
			size_t const size = pParent->ChildCount();
			for (size_t i = 0; i < size; ++i)
			{
				IAudioAsset* const pItem = pParent->GetChild(i);
				if (pItem != nullptr && (pItem->GetType() == EItemType::eItemType_Folder) && (name.compareNoCase(pItem->GetName()) == 0))
				{
					return pItem;
				}
			}

			CAudioFolder* const pFolder = new CAudioFolder(name);
			if (pFolder != nullptr)
			{
				signalItemAboutToBeAdded(pParent);
				pParent->AddChild(pFolder);
				pFolder->SetParent(pParent);
			}

			signalItemAdded(pFolder);
			SetAssetModified(pFolder);
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
void CAudioAssetsManager::OnControlAboutToBeModified(CAudioControl* pControl)
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::OnConnectionAdded(CAudioControl* pControl, IAudioSystemItem* pMiddlewareControl)
{
	signalConnectionAdded(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::OnConnectionRemoved(CAudioControl* pControl, IAudioSystemItem* pMiddlewareControl)
{
	signalConnectionRemoved(pControl);
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::OnControlModified(CAudioControl* pControl)
{
	signalControlModified(pControl);
	m_bControlTypeModified[ItemTypeToIndex(pControl->GetType())] = true;
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::SetAssetModified(IAudioAsset* pAsset)
{
	m_bControlTypeModified[ItemTypeToIndex(pAsset->GetType())] = true;
}

//////////////////////////////////////////////////////////////////////////
bool CAudioAssetsManager::IsDirty()
{
	for (auto const i : m_bControlTypeModified)
	{
		if (i)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CAudioAssetsManager::IsTypeDirty(EItemType eType)
{
	return m_bControlTypeModified[ItemTypeToIndex(eType)];
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::ClearDirtyFlags()
{
	for (auto& i : m_bControlTypeModified)
	{
		i = false;
	}
}

//////////////////////////////////////////////////////////////////////////
CAudioControl* CAudioAssetsManager::FindControl(string const& controlName, EItemType const type, IAudioAsset* const pParent) const
{
	if (pParent == nullptr)
	{
		for (auto const pControl : m_controls)
		{
			if (pControl != nullptr && pControl->GetName() == controlName && pControl->GetType() == type)
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
			CAudioControl* const pControl = static_cast<CAudioControl*>(pParent->GetChild(i));

			if (pControl != nullptr && pControl->GetName() == controlName && pControl->GetType() == type)
			{
				return pControl;
			}
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::ClearAllConnections()
{
	for (auto const pControl : m_controls)
	{
		if (pControl != nullptr)
		{
			pControl->ClearConnections();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::ReloadAllConnections()
{
	for (auto const pControl : m_controls)
	{
		if (pControl != nullptr)
		{
			pControl->ClearConnections();
			pControl->ReloadConnections();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::MoveItems(IAudioAsset* pParent, std::vector<IAudioAsset*> const& items)
{
	if (pParent != nullptr)
	{
		for (auto const pItem : items)
		{
			if (pItem != nullptr)
			{
				IAudioAsset* const pPreviousParent = pItem->GetParent();

				if (pPreviousParent != nullptr)
				{
					signalItemAboutToBeRemoved(pItem);
					pPreviousParent->RemoveChild(pItem);
					pItem->SetParent(nullptr);
					signalItemRemoved(pPreviousParent, pItem);
					SetAssetModified(pPreviousParent);
				}

				signalItemAboutToBeAdded(pParent);
				pParent->AddChild(pItem);
				pItem->SetParent(pParent);
				signalItemAdded(pItem);
				SetAssetModified(pItem);
			}
		}

		SetAssetModified(pParent);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsManager::CreateAndConnectImplItems(IAudioSystemItem* pImplItem, IAudioAsset* pParent)
{
	signalItemAboutToBeAdded(pParent);
	IAudioAsset* pItem = CreateAndConnectImplItemsRecursively(pImplItem, pParent);
	signalItemAdded(pItem);
}

//////////////////////////////////////////////////////////////////////////
IAudioAsset* CAudioAssetsManager::CreateAndConnectImplItemsRecursively(IAudioSystemItem* pImplItem, IAudioAsset* pParent)
{
	IAudioAsset* pItem = nullptr;

	// Create the new control and connect it to the one dragged in externally
	IAudioSystemEditor* pImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();

	string name = pImplItem->GetName();
	EItemType const type = pImpl->ImplTypeToATLType(pImplItem->GetType());

	if (type != EItemType::eItemType_Invalid)
	{
		PathUtil::RemoveExtension(name);

		if (type != eItemType_State)
		{
			name = Utils::GenerateUniqueControlName(name, type, *this);
		}
		else
		{
			name = Utils::GenerateUniqueName(name, type, pParent);
		}

		CAudioControl* const pControl = new CAudioControl(name, GenerateUniqueId(), type);
		pControl->SetParent(pParent);
		pParent->AddChild(pControl);
		m_controls.push_back(pControl);

		ConnectionPtr pAudioConnection = pImpl->CreateConnectionToControl(pControl->GetType(), pImplItem);
		if (pAudioConnection)
		{
			pControl->AddConnection(pAudioConnection);
		}

		pItem = pControl;
	}
	else
	{
		// If the type of the control is invalid then it must be a folder or container
		name = Utils::GenerateUniqueName(name, EItemType::eItemType_Folder, pParent);
		CAudioFolder* pFolder = new CAudioFolder(name);
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
string GenerateUniqueName(string const& name, EItemType const type, IAudioAsset* const pParent)
{
	string finalName = name;

	if (pParent != nullptr)
	{
		size_t const size = pParent->ChildCount();
		std::vector<string> names;
		names.reserve(size);

		for (size_t i = 0; i < size; ++i)
		{
			IAudioAsset* const pChild = pParent->GetChild(i);

			if (pChild != nullptr && pChild->GetType() == type)
			{
				names.emplace_back(pChild->GetName());
			}
		}

		finalName = PathUtil::GetUniqueName(name, names);
	}

	return finalName;
}

//////////////////////////////////////////////////////////////////////////
string GenerateUniqueLibraryName(string const& name, CAudioAssetsManager const& assetManager)
{
	size_t const size = assetManager.GetLibraryCount();
	std::vector<string> names;
	names.reserve(size);

	for (size_t i = 0; i < size; ++i)
	{
		CAudioLibrary* const pLibrary = assetManager.GetLibrary(i);

		if (pLibrary != nullptr)
		{
			names.emplace_back(pLibrary->GetName());
		}
	}

	return PathUtil::GetUniqueName(name, names);
}

//////////////////////////////////////////////////////////////////////////
string GenerateUniqueControlName(string const& name, EItemType type, CAudioAssetsManager const& assetManager)
{
	CAudioAssetsManager::Controls const& controls(assetManager.GetControls());
	std::vector<string> names;
	names.reserve(controls.size());

	for (auto const pControl : controls)
	{
		names.emplace_back(pControl->GetName());
	}

	return PathUtil::GetUniqueName(name, names);
}

//////////////////////////////////////////////////////////////////////////
IAudioAsset* GetParentLibrary(IAudioAsset* pAsset)
{
	while (pAsset && pAsset->GetType() != EItemType::eItemType_Library)
	{
		pAsset = pAsset->GetParent();
	}

	return pAsset;
}

//////////////////////////////////////////////////////////////////////////
void SelectTopLevelAncestors(std::vector<IAudioAsset*> const& source, std::vector<IAudioAsset*>& dest)
{
	for (auto pItem : source)
	{
		// Check if item has ancestors that are also selected
		bool bAncestorAlsoSelected = false;
		for (auto pOtherItem : source)
		{
			if (pItem != pOtherItem)
			{
				// Find if pOtherItem is the ancestor of pItem
				const IAudioAsset* pParent = pItem->GetParent();
				while (pParent)
				{
					if (pParent == pOtherItem)
					{
						break;
					}
					pParent = pParent->GetParent();
				}

				if (pParent)
				{
					bAncestorAlsoSelected = true;
					break;
				}
			}
		}

		if (!bAncestorAlsoSelected)
		{
			dest.push_back(pItem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string const& GetAssetFolder()
{
	static string path = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "ace" CRY_NATIVE_PATH_SEPSTR;
	return path;
}
} // namespace Utils
} // namespace ACE
