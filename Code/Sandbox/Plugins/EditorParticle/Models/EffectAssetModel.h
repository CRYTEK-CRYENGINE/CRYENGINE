// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>

#include <memory>

class CAsset;

namespace CryParticleEditor {

class CEffectAsset;

class CEffectAssetModel
{
public:
	CEffectAssetModel();

	void MakeNewAsset();
	bool OpenAsset(CAsset* pAsset, bool reload = false);
	void ClearAsset();

	void ReloadFromFile(CEffectAsset* pEffectAsset);

	CEffectAsset* GetEffectAsset();

public:
	CCrySignal<void()> signalBeginEffectAssetChange;
	CCrySignal<void()> signalEndEffectAssetChange;

private:
	std::unique_ptr<CEffectAsset> m_pEffectAsset; //!< There can be at most one active effect asset.
	int m_nextUntitledAssetId;
};

}
