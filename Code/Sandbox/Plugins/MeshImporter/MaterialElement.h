// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "FbxScene.h"
#include "FbxMetaData.h"
#include "QtUtil.h"
#include <CrySerialization/Forward.h>
#include <vector>

class CMaterialModel;

class CMaterialElement
{
public:
	CMaterialElement(
	  const QString& name,
	  CMaterialModel* pModel,
	  const FbxTool::SMaterial* pMaterial,
	  bool bDummyMaterial);

	const QString&                   GetName() const;
	const FbxTool::SMaterial*        GetMaterial() const;
	int                              GetSubMaterialIndex() const;
	FbxMetaData::EPhysicalizeSetting GetPhysicalizeSetting() const;
	const ColorF& GetColor() const;

	bool          IsMarkedForIndexAutoAssignment() const;
	bool          IsMarkedForDeletion() const;

	bool          IsDummyMaterial() const;

	void          MarkForIndexAutoAssignment(bool bEnabled);
	void          MarkForDeletion();

	void          SetSubMaterialIndex(int index);
	void          SetPhysicalizeSetting(FbxMetaData::EPhysicalizeSetting physicalizeSetting);
	void          SetColor(const ColorF& color);

	void          Serialize(Serialization::IArchive& ar);
private:
	CMaterialModel* const     m_pModel;
	FbxTool::CScene* const    m_pScene;
	const FbxTool::SMaterial* m_pMaterial;
	const QString             m_name;
	const bool                m_bDummyMaterial;
};

FbxTool::EMaterialPhysicalizeSetting Convert(FbxMetaData::EPhysicalizeSetting setting);
FbxMetaData::EPhysicalizeSetting     Convert(FbxTool::EMaterialPhysicalizeSetting setting);

