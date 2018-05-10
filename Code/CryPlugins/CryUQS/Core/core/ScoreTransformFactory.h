// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CScoreTransformFactory
		//
		//===================================================================================
		
		class CScoreTransformFactory final : public IScoreTransformFactory, public Shared::CFactoryBase<CScoreTransformFactory>
		{
		public:

			explicit                              CScoreTransformFactory(const char* szName, const CryGUID& guid, const char* szDescription, EScoreTransformType scoreTransformType);

			// IScoreTransformFactory
			virtual const char*                   GetName() const override;
			virtual const CryGUID&                GetGUID() const override;
			virtual const char*                   GetDescription() const override;
			virtual EScoreTransformType           GetScoreTransformType() const override;
			// ~IScoreTransformFactory

			static void                           InstantiateFactories();
			static const CScoreTransformFactory&  GetDefaultScoreTransformFactory();   // this may only be called after InstantiateFactories(); will assert() and crash otherwise

		private:

			string                                m_description;
			EScoreTransformType                   m_scoreTransformType;
			static const CScoreTransformFactory*  s_pDefaultScoreTransformFactory;     // will be set by InstantiateFactories()
		};

		//===================================================================================
		//
		// ScoreTransformFactoryDatabase
		//
		//===================================================================================

		typedef CFactoryDatabase<IScoreTransformFactory> ScoreTransformFactoryDatabase;

	}
}
