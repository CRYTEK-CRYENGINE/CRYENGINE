// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>

#include <CryAISystem/NavigationSystem/NavigationIdTypes.h>
#include <CryAISystem/NavigationSystem/INavigationQuery.h>

struct IEntityNavigationComponent : public IEntityComponent
{
	typedef std::function<void(const Vec3&)> StateUpdatedCallback;
	typedef std::function<void(const bool)> NavigationCompletedCallback;

	struct SMovementProperties
	{
		bool operator==(const SMovementProperties& other) const
		{
			return normalSpeed == other.normalSpeed
				&& minSpeed == other.minSpeed
				&& maxSpeed == other.maxSpeed
				&& maxAcceleration == other.maxAcceleration
				&& maxDeceleration == other.maxDeceleration
				&& lookAheadDistance == other.lookAheadDistance
				&& bStopAtEnd == other.bStopAtEnd;
		}

		float normalSpeed = 4.0f;
		float minSpeed = 0.0f;
		float maxSpeed = 6.0f;
		float maxAcceleration = 4.0f;
		float maxDeceleration = 6.0f;
		float lookAheadDistance = 0.5f;
		bool  bStopAtEnd = true;
	};

	struct SCollisionAvoidanceProperties
	{
		enum class EType
		{
			None,
			Passive,
			Active,
		};

		bool operator==(const SCollisionAvoidanceProperties& other) const
		{
			return radius == other.radius && type == other.type;
		}

		float radius = 0.3;
		EType type = EType::Active;
	};
	
	virtual void SetNavigationAgentType(const char* szTypeName) = 0;
	virtual void SetMovementProperties(const SMovementProperties& properties) = 0;
	virtual void SetCollisionAvoidanceProperties(const SCollisionAvoidanceProperties& properties) = 0;
	virtual void SetNavigationQueryFilter(const SNavMeshQueryFilterDefault& filter) = 0;
	virtual bool TestRaycastHit(const Vec3& toPositon, Vec3& hitPos, Vec3& hitNorm) const = 0;
	virtual bool IsRayObstructed(const Vec3& toPosition) const = 0;
	virtual bool IsDestinationReachable(const Vec3& destination) const = 0;
	virtual void NavigateTo(const Vec3& destination) = 0;
	virtual void StopMovement() = 0;

	virtual const SMovementProperties& GetMovementProperties() const = 0;
	virtual const SCollisionAvoidanceProperties& GetCollisionAvoidanceProperties() const = 0;

	virtual Vec3 GetRequestedVelocity() const = 0;
	virtual void SetRequestedVelocity(const Vec3& velocity) = 0;
	virtual void SetStateUpdatedCallback(const StateUpdatedCallback& callback) = 0;
	virtual void SetNavigationCompletedCallback(const NavigationCompletedCallback& callback) = 0;

	virtual NavigationAgentTypeID      GetNavigationTypeId() const = 0;
	virtual const INavMeshQueryFilter* GetNavigationQueryFilter() const = 0;

	static void ReflectType(Schematyc::CTypeDesc<IEntityNavigationComponent>& desc)
	{
		desc.SetGUID("F2D45F94-8558-4C4F-8FFF-697783BEEB7C"_cry_guid);
	}
};