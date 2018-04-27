// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>

namespace FbxTool
{

struct SMesh;
struct SNode;
class CScene;

} // namespace FbxTool

namespace DialogMesh
{

//! Data stored for a FBX scene.
class CSceneUserData
{
public:
	void Init(FbxTool::CScene* pFbxScene);

	bool SelectSkin(const FbxTool::SMesh* pMesh);
	bool SelectAnySkin(const FbxTool::SNode* pNode);
	void DeselectSkin();
	const FbxTool::SMesh* GetSelectedSkin() const;

	CCrySignal<void()> sigSelectedSkinChanged;
private:
	const FbxTool::SMesh* m_pSelectedSkin;  //!< There can be at most one skin selected.
};

} // namespace DialogMesh

