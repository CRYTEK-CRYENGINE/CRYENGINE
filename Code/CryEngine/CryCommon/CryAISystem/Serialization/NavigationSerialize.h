// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/INavigationSystem.h>
#include <CryAISystem/NavigationSystem/INavigationQuery.h>

#include <CrySerialization/Forward.h>

// Serialization Decorators

namespace NavigationSerialization
{
	struct NavigationQueryFilter
	{
		SNavMeshQueryFilterDefault& variable;
		NavigationQueryFilter(SNavMeshQueryFilterDefault& filter) 
			: variable(filter) 
		{}
		void Serialize(Serialization::IArchive& ar);
	};

	struct NavigationAreaCost
	{
		uint32& index;
		float&  cost;
		NavigationAreaCost(uint32& index, float& cost)
			: index(index), cost(cost)
		{}
		void Serialize(Serialization::IArchive& ar);
	};

	struct NavigationAreaFlagsMask
	{
		MNM::AreaAnnotation::value_type& value;
		NavigationAreaFlagsMask(MNM::AreaAnnotation::value_type& value) 
			: value(value) 
		{}
		void Serialize(Serialization::IArchive& ar);
	};
}

bool Serialize(Serialization::IArchive& archive, NavigationVolumeID& value, const char* szName, const char* szLabel);
bool Serialize(Serialization::IArchive& archive, NavigationAgentTypeID& value, const char* szName, const char* szLabel);
bool Serialize(Serialization::IArchive& archive, NavigationAreaTypeID& value, const char* szName, const char* szLabel);
bool Serialize(Serialization::IArchive& archive, NavigationAreaFlagID& value, const char* szName, const char* szLabel);
bool Serialize(Serialization::IArchive& archive, SNavMeshQueryFilterDefault& value, const char* szName, const char* szLabel);

#include "NavigationSerialize.inl"
