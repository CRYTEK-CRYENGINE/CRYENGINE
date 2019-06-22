// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntitySystem.h>

namespace LevelExplorerCommandHelper
{
bool AreLocked(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects);
void ToggleLocked(std::vector<CObjectLayer*>& layers, std::vector<CBaseObject*>& objects);

bool AreVisible(const std::vector<CObjectLayer*>& layers, const std::vector<CBaseObject*>& objects);
void ToggleVisibility(std::vector<CObjectLayer*>& layers, std::vector<CBaseObject*>& objects);

bool AreExportable(const std::vector<CObjectLayer*>& layers);
void ToggleExportable(std::vector<CObjectLayer*>& layers);

bool AreExportableToPak(const std::vector<CObjectLayer*>& layers);
void ToggleExportableToPak(std::vector<CObjectLayer*>& layers);

bool AreAutoLoaded(const std::vector<CObjectLayer*>& layers);
void ToggleAutoLoad(std::vector<CObjectLayer*>& layers);

bool HavePhysics(const std::vector<CObjectLayer*>& layers);
void TogglePhysics(std::vector<CObjectLayer*>& layers);

bool HasPlatformSpecs(const std::vector<CObjectLayer*>& layers, ESpecType type);
void TogglePlatformSpecs(std::vector<CObjectLayer*>& layers, ESpecType type);
}
