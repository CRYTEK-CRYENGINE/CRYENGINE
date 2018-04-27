// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EffectAssetModel.h"
#include "EffectAsset.h"

#include <AssetSystem/Asset.h>

#include <CryParticleSystem/IParticlesPfx2.h>

namespace CryParticleEditor {

CEffectAssetModel::CEffectAssetModel()
	: m_nextUntitledAssetId(1)
{
}

void CEffectAssetModel::MakeNewAsset()
{
	string untitledName = "Untitled";
	untitledName += '0' + (m_nextUntitledAssetId / 10) % 10;
	untitledName += '0' + m_nextUntitledAssetId % 10;
	untitledName += "*";
	++m_nextUntitledAssetId;

	pfx2::PParticleEffect pNewEffect = GetParticleSystem()->CreateEffect();
	GetParticleSystem()->RenameEffect(pNewEffect, untitledName.c_str());

	signalBeginEffectAssetChange();

	m_pEffectAsset.reset(new CEffectAsset());
	m_pEffectAsset->SetEffect(pNewEffect); // XXX Pointer type?
	m_pEffectAsset->MakeNewComponent("%ENGINE%/EngineAssets/Particles/Default.pfxp");

	signalEndEffectAssetChange();
}

bool CEffectAssetModel::OpenAsset(CAsset* pAsset, bool reload)
{
	CRY_ASSERT(pAsset);

	if (!pAsset->GetFilesCount())
	{
		return false;
	}

	const string pfxFilePath = pAsset->GetFile(0);

	pfx2::PParticleEffect pEffect;
	if (reload)
	{
		pEffect = GetParticleSystem()->CreateEffect();
		Serialization::LoadJsonFile(*pEffect, pfxFilePath.c_str());
	}
	else
	{
		pEffect = GetParticleSystem()->FindEffect(pfxFilePath.c_str());
	}

	if (!pEffect)
	{
		return false;
	}

	signalBeginEffectAssetChange();

	m_pEffectAsset.reset(new CEffectAsset());
	m_pEffectAsset->SetAsset(pAsset);
	m_pEffectAsset->SetEffect(pEffect.get()); // XXX Pointer type?

	signalEndEffectAssetChange();

	return true;
}

void CEffectAssetModel::ClearAsset()
{
	signalBeginEffectAssetChange();
	m_pEffectAsset.reset();
	signalEndEffectAssetChange();
}

void CEffectAssetModel::ReloadFromFile(CEffectAsset* pEffectAsset)
{
	CRY_ASSERT(pEffectAsset && pEffectAsset->GetAsset() && pEffectAsset->GetAsset()->GetFilesCount());
	const string pfxFilePath = pEffectAsset->GetAsset()->GetFile(0);
	pfx2::IParticleEffectPfx2* pEffect = pEffectAsset->GetEffect();

	// Observers need to disconnect from the effect asset's model.
	signalBeginEffectAssetChange();

	pEffectAsset->SetEffect(nullptr);
	Serialization::LoadJsonFile(*pEffect, pfxFilePath.c_str());
	pEffectAsset->SetEffect(pEffect);

	signalEndEffectAssetChange();
}

CEffectAsset* CEffectAssetModel::GetEffectAsset()
{
	return m_pEffectAsset.get();
}

}

