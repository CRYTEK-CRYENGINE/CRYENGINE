// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>

#include <vector>
#include <memory>

class CSceneElementCommon;

class CScene
{
public:
	CScene();
	~CScene();

	template<typename T>
	T* NewElement()
	{
		int id = 0;
		if (!m_freeIDs.empty())
		{
			id = m_freeIDs.back();
			m_freeIDs.pop_back();
		}
		else
		{
			id = m_elements.size();
		}

		m_elements.emplace_back(new T(this, id));
		return (T*)m_elements.back().get();
	}

	void DeleteSingleElement(CSceneElementCommon* pElement);  // Deletes single element, excluding its children.
	void DeleteSubtree(CSceneElementCommon* pElement);  // Deletes element with its entire subtree.

	int GetElementCount() const;
	CSceneElementCommon* GetElementByIndex(int index);

	void Clear();

	CCrySignal<void(CSceneElementCommon* pOldParent, CSceneElementCommon* pChild)> signalHierarchyChanged;
	CCrySignal<void(CSceneElementCommon*)> signalPropertiesChanged;
private:
	void DeleteSingleElementUnchecked(CSceneElementCommon* pElement);
	void DeleteSubtreeUnchecked(CSceneElementCommon* pElement);

	std::vector<std::unique_ptr<CSceneElementCommon>> m_elements;
	std::vector<int> m_freeIDs;
};
