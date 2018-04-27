// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CAsset;
class CAssetType;

class CModel
{
public:
	CModel();
	virtual ~CModel();
	void                            OpenAsset(CAsset* pAsset);
	CAsset*                         GetAsset() const { return m_pAsset; }
	const std::vector<CAssetType*>& GetSupportedTypes() const;
	const CAssetType*               FindAssetTypeByFile(const char* szFile) const;
	const std::vector<string>       GetSourceFileExtensions(const CAssetType* assetType) const;

	CCrySignal<void()> signalBeginChange;
	CCrySignal<void()> signalEndChange;

	//! Depth-first enumeration of the asset dependency tree.
	//! \param asset Pointer to asset instance to enumerate dependencies.
	//! \param f function object to be applied to every dependency; The signature of the function should be equivalent to: void f(CAsset* asset, const char* path, size_t depth);
	template<typename F>
	void ForAllDependencies(CAsset* asset, F&& f)
	{
		const static size_t rootDepth = 0;
		f(asset, asset->GetMetadataFile(), rootDepth);

		CAssetManager* pAssetManager = GetIEditor()->GetAssetManager();
		std::stack<std::vector<SAssetDependencyInfo>> stack;
		stack.push(asset->GetDependencies());
		while (!stack.empty())
		{
			auto& dependencies = stack.top();

			if (dependencies.empty()) // Reached the end of the list.
			{
				stack.pop();
				continue;
			}
			const string& path = dependencies.back().path;
			CAsset* pAsset = pAssetManager->FindAssetForFile(path);
			f(pAsset, path, stack.size());
			dependencies.pop_back();

			if (!pAsset)
			{
				// Unresolved dependency.
				continue;
			}
			stack.push(pAsset->GetDependencies());
		}
	}

protected:
	CAsset* m_pAsset;
};

