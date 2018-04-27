// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FbxScene.h"

bool HasDefaultUserData(const FbxTool::CScene* pScene, const FbxTool::SNode* pNode);

FbxTool::ENodeExportSetting GetDefaultNodeExportSetting(const FbxTool::SNode* pNode);

