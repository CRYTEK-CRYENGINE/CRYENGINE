// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// primary header to be included by game code intending to use the UQS's standard library

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// - some types that the UQS StdLib provides, wrapped by the type-disambiguation mechanism
		// - not all types in the StdLib need this disambiguation (typically only those whose underlying type is used to typedef various other types)
		// - on the other hand, the same underlying type can be used to typedef new types for use in more specific contexts
		//
		//===================================================================================

		typedef Client::STypeWrapper<EntityId, 1> EntityIdWrapper;

		typedef Client::STypeWrapper<Vec3, 1>     Pos3;              // global position in the 3D world
		typedef Client::STypeWrapper<Vec3, 2>     Ofs3;              // offset on top of a Pos3; can also act as "local position"
		typedef Client::STypeWrapper<Vec3, 3>     Dir3;              // normalized 3D vector (aka "direction")

		//===================================================================================
		//
		// CStdLibRegistration
		//
		// - provides automatic registration of some default item types, functions, generators and evaluators
		// - in order to make use of these, CStdLibRegistration::InstantiateAllFactoriesForRegistration() must be called once
		// - the call should happen early so that by the time the UQS Hub does its consistency checks all modules can have their dependencies resolved
		// - typically, it's the one who instantiates the UQS Hub that should also do the call
		//
		//===================================================================================

		class CStdLibRegistration
		{
		public:
			static void    InstantiateAllFactoriesForRegistration();                 // call this function once to automatically make all standard stuff available

		private:
			static void    InstantiateItemFactoriesForRegistration();                // stdlib/Items.cpp
			static void    InstantiateFunctionFactoriesForRegistration();            // stdlib/Functions.cpp
			static void    InstantiateGeneratorFactoriesForRegistration();           // stdlib/Generators.cpp
			static void    InstantiateInstantEvaluatorFactoriesForRegistration();    // stdlib/InstantEvaluators.cpp
			static void    InstantiateDeferredEvaluatorFactoriesForRegistration();   // stdlib/DeferredEvaluators.cpp
		};

	}
}
