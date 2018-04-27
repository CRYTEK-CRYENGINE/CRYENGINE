// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>

struct CoverID;

struct IEntityCoverUserComponent : public IEntityComponent
{
	typedef Functor1<const CoverID&> CoverCallback;
	typedef Functor1<DynArray<Vec3>&> RefreshEyesCustomFunction;

	struct SCoverCallbacks
	{
		CoverCallback coverEnteredCallback;
		CoverCallback coverLeftCallback;
		CoverCallback coverCompromisedCallback;
		CoverCallback moveToCoverFailedCallback;
	};

	virtual void MoveToCover(const CoverID& coverId) = 0;
	virtual void SetCallbacks(const SCoverCallbacks& callbacks) = 0;
	virtual void SetRefreshEyesCustomFunction(RefreshEyesCustomFunction functor) = 0;

	virtual bool IsInCover() const = 0;
	virtual bool IsMovingToCover() const = 0;
	virtual bool IsCoverCompromised() const = 0;

	virtual bool GetCurrentCoverPosition(Vec3& position) const = 0;
	virtual bool GetCurrentCoverNormal(Vec3& normal) const = 0;

	static void ReflectType(Schematyc::CTypeDesc<IEntityCoverUserComponent>& desc)
	{
		desc.SetGUID("D3C6A814-DBEE-4CB1-9322-9AC6028A3EB8"_cry_guid);
	}
};