// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc {

static constexpr CryGUID g_stdModuleGUID = "a929e72e-d3a0-46a0-949b-17d22162ec33"_cry_guid;
static constexpr CryGUID g_logModuleGUID = "a2cbae18-2114-4c0f-8fc0-58988affca7e"_cry_guid;
static constexpr CryGUID g_mathModuleGUID = "f2800127-ed71-4d1e-a178-87aac8ee74ed"_cry_guid;
static constexpr CryGUID g_resourceModuleGUID = "4dbeaab8-e902-4db9-b930-09228a172d89"_cry_guid;

static constexpr CryGUID g_coreEnvPackageGuid = "a67cd89b-a62c-417e-851c-85bc2ffafdc9"_cry_guid;

struct IEnvRegistrar;

void RegisterCoreEnvPackage(IEnvRegistrar& registrar);

} // ~Schematyc namespace
