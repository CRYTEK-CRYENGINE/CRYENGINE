// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _IGAME_RENDERNODE_
#define _IGAME_RENDERNODE_

#pragma once

#include "Effects/RenderElements/GameRenderElement.h"
#include "Effects/GameEffectsSystem.h"

// Forward declares
struct IGameRenderNodeParams;

//==================================================================================================
// Name: IGameRenderNode
// Desc: Base interface for all game render nodes
// Author: James Chilvers
//==================================================================================================
struct IGameRenderNode : public IRenderNode, public _i_reference_target_t
{
	virtual ~IGameRenderNode() {}

	virtual bool InitialiseGameRenderNode() = 0;
	virtual void ReleaseGameRenderNode() = 0;

	virtual void SetParams(const IGameRenderNodeParams* pParams = NULL) = 0;

};//------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: IGameRenderNodeParams
// Desc: Game render node params
// Author: James Chilvers
//==================================================================================================
struct IGameRenderNodeParams
{
	virtual ~IGameRenderNodeParams() {}
};//------------------------------------------------------------------------------------------------

#endif // _IGAME_RENDERNODE_
