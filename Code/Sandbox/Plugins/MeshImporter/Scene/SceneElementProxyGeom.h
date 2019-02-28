// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SceneElementCommon.h"

struct phys_geometry;

class CSceneElementProxyGeom : public CSceneElementCommon
{
public:
	CSceneElementProxyGeom(CScene* pScene, int id);

	void SetPhysGeom(phys_geometry* pPhysGeom);

	phys_geometry* GetPhysGeom();

	void Serialize(Serialization::IArchive& ar);

	virtual ESceneElementType GetType() const override;
private:
	phys_geometry* m_pPhysGeom;
};

