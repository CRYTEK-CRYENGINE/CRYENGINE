// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>

struct IEntityObserverComponent : public IEntityComponent
{
	typedef std::function<bool(const VisionID& observerId, const ObserverParams& observerParams, const VisionID& observableId, const ObservableParams& observableParams)> UserConditionCallback;
	
	virtual bool CanSee(const EntityId entityId) const = 0;

	virtual void SetUserConditionCallback(const UserConditionCallback callback) = 0;
	
	virtual void SetTypeMask(const uint32 typeMask) = 0;
	virtual void SetTypesToObserveMask(const uint32 typesToObserverMask) = 0;
	virtual void SetFactionsToObserveMask(const uint32 factionsToObserveMask) = 0;
	virtual void SetFOV(const float fovInRad) = 0;
	virtual void SetRange(const float range) = 0;
	virtual void SetPivotOffset(const Vec3& offsetFromPivot) = 0;
	virtual void SetBoneOffset(const Vec3& offsetFromBone, const char* szBoneName) = 0;

	virtual uint32 GetTypeMask() const = 0;
	virtual uint32 GetTypesToObserveMask() const = 0;
	virtual uint32 GetFactionsToObserveMask() const = 0;
	virtual float GetFOV() const = 0;
	virtual float GetRange() const = 0;
	
	static void ReflectType(Schematyc::CTypeDesc<IEntityObserverComponent>& desc)
	{
		desc.SetGUID("EC4E135E-BB29-48DB-B143-CB9206F4B2E6"_cry_guid);
	}
};