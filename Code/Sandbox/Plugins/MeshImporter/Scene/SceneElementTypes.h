// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

enum class ESceneElementType : int
{
	SourceNode,  // Element is of type CSceneElementSourceNode.

	// Created by proxy generation.
	PhysProxy,  // Element is of type CSceneElementPhysProxy.
	ProxyGeom,  // Element is of type CSceneElementProxyGeom.

	Skin, // Each node of type SourceNode that has a mesh with a skin has a child node of type Skin.
};

