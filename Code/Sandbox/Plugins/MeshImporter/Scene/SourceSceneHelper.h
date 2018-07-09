// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>

namespace FbxTool
{
struct SNode;
class CScene;
};

class CScene;
class CSceneElementSourceNode;

class CSceneModelCommon;
class CSceneViewCommon;

CSceneElementSourceNode* CreateSceneFromSourceScene(CScene& scene, FbxTool::CScene& sourceScene);

CSceneElementSourceNode* FindSceneElementOfNode(CScene& scene, const FbxTool::SNode* pFbxNode);

void SelectSceneElementWithNode(CSceneModelCommon* pSceneModel, CSceneViewCommon* pSceneView, const FbxTool::SNode* pNode);

