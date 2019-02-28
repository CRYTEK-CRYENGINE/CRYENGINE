// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// STypeWrapper <T, int tUniqueTag>
		//
		// - wraps an arbitrary data type and grants access to it via the STypeWrapper<>::value member variable
		// - can be used to prevent type clashes, like our beloved 'typedef unsigned int EntityId' vs. 'typedef unsigned int TVehicleObjectID'
		// - in those examples, the type ambiguity could be resolved like this:
		//
		//     typedef STypeWrapper<EntityId, 1> EntityIdWrapper;
		//     typedef STypeWrapper<TVehicleObjectID, 2> VehicleObjectIDWrapper;
		//
		// - notice about the 2nd template argument: this is a unique tag, and if you accidentally use the same tag twice with the same T, the UQS's startup consistency checker
		//   will notice the introduced duplicated type :)
		//
		//===================================================================================

		template <class T, int tUniqueTag>
		struct STypeWrapper
		{
			T value;

			STypeWrapper()
			{}

			STypeWrapper(const STypeWrapper& rhs)
				: value(rhs.value)
			{}

			explicit STypeWrapper(const T& _value)
				: value(_value)
			{}

			// allow the compiler-generated operator=
		};

	}
}
