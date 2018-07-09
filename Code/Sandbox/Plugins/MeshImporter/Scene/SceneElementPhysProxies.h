// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SceneElementCommon.h"

struct SPhysProxies;

class CSceneElementPhysProxies : public CSceneElementCommon
{
public:
	CSceneElementPhysProxies(CScene* pScene, int id);

	void SetPhysProxies(SPhysProxies* pPhysProxies);

	SPhysProxies* GetPhysProxies();

	void Serialize(Serialization::IArchive& ar);

	virtual ESceneElementType GetType() const override;
private:
	SPhysProxies* m_pPhysProxies;
};

