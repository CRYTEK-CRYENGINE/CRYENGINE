// Copyright 2001-2020 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntitySystem.h>

namespace LevelExplorerCommandHelper
{
void Lock(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects);
void UnLock(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects);

bool AreLocked(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects);
void ToggleLocked(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects);

bool AreVisible(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects);
void ToggleVisibility(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects);

bool AreChildrenLocked(const std::vector<CObjectLayer*>& layers);
void ToggleChildrenLocked(const std::vector<CObjectLayer*>& layers);

bool AreChildrenVisible(const std::vector<CObjectLayer*>& layers);
void ToggleChildrenVisibility(const std::vector<CObjectLayer*>& layers);

bool AreExportable(const std::vector<CObjectLayer*>& layers);
void ToggleExportable(const std::vector<CObjectLayer*>& layers);

bool AreExportableToPak(const std::vector<CObjectLayer*>& layers);
void ToggleExportableToPak(const std::vector<CObjectLayer*>& layers);

bool AreAutoLoaded(const std::vector<CObjectLayer*>& layers);
void ToggleAutoLoad(const std::vector<CObjectLayer*>& layers);

bool HavePhysics(const std::vector<CObjectLayer*>& layers);
void TogglePhysics(const std::vector<CObjectLayer*>& layers);

bool HasPlatformSpecs(const std::vector<CObjectLayer*>& layers, ESpecType type);
void TogglePlatformSpecs(const std::vector<CObjectLayer*>& layers, ESpecType type);
}
