// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleGraphModel.h"

class CAsset;

namespace CryParticleEditor {

class CEffectAsset
{
public:
	CEffectAsset();

	const char* GetName() const;
	bool IsModified() const;
	CAsset* GetAsset();

	pfx2::IParticleEffectPfx2* GetEffect();
	CryParticleEditor::CParticleGraphModel* GetModel();

	void SetAsset(CAsset* pAsset);
	void SetEffect(pfx2::IParticleEffectPfx2* pEffect);

	bool MakeNewComponent(const char* szTemplateName);

	void SetModified(bool bModified);

private:
	CAsset* m_pAsset;

	std::unique_ptr<CryParticleEditor::CParticleGraphModel> m_pModel;
	_smart_ptr<pfx2::IParticleEffectPfx2>                   m_pEffect;
	bool m_bModified;
};

}
