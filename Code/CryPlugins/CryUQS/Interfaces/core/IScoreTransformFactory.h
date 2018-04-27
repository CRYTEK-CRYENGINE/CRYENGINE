// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// EScoreTransformType
		//
		// - types of all currently built-in score transforms
		// - a score transform is a hard-coded function that runs after an evaluator has scored the item
		// - there are "inverse" versions of each function, which basically run in 2 steps:
		//   (1) transform the score just like the non-inverse version does
		//   (2) subtract the transformed score from 1.0
		// - i. e.: score = 1.0f - SelectedTransformFunction(score)
		//
		//===================================================================================

		enum class EScoreTransformType
		{
			Linear,
			LinearInverse,
			Sine180,
			Sine180Inverse,
			Square,
			SquareInverse
		};

		//===================================================================================
		//
		// IScoreTransformFactory
		//
		//===================================================================================

		struct IScoreTransformFactory
		{
			virtual                      ~IScoreTransformFactory() {}
			virtual const char*          GetName() const = 0;        // used for showing in the UI as well a unique key (this may change when refactoring the whole UQS system to use GUIDs for such things)
			virtual const CryGUID&       GetGUID() const = 0;
			virtual const char*          GetDescription() const = 0;
			virtual EScoreTransformType  GetScoreTransformType() const = 0;
		};

		//===================================================================================
		//
		// IScoreTransformFactoryDatabase
		//
		//===================================================================================

		typedef IFactoryDatabase<IScoreTransformFactory> IScoreTransformFactoryDatabase;

	}
}
