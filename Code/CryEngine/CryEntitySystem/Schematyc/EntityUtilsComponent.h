// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc
{

// Forward declare interfaces.
struct IEnvRegistrar;
// Forward declare structures.
struct SUpdateContext;

class CEntityUtilsComponent final : public IEntityComponent
{
public:

	ExplicitEntityId         GetEntityId() const;

	void                     SetTransform(const CryTransform::CTransform& transform);
	CryTransform::CTransform GetTransform();
	void                     SetRotation(const CryTransform::CRotation& rotation);
	CryTransform::CRotation  GetRotation();

	void                     SetVisible(bool bVisible);
	bool                     IsVisible() const;

	static void              ReflectType(CTypeDesc<CEntityUtilsComponent>& desc);
	static void              Register(IEnvRegistrar& registrar);
};

} // Schematyc
