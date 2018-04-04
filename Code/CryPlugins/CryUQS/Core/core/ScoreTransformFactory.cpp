#include "StdAfx.h"

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

		const CScoreTransformFactory* CScoreTransformFactory::s_pDefaultScoreTransformFactory;

		CScoreTransformFactory::CScoreTransformFactory(const char* szName, const CryGUID& guid, const char* szDescription, EScoreTransformType scoreTransformType)
			: CFactoryBase(szName, guid)
			, m_description(szDescription)
			, m_scoreTransformType(scoreTransformType)
		{
			// nothing
		}

		const char* CScoreTransformFactory::GetName() const
		{
			return CFactoryBase::GetName();
		}

		const CryGUID& CScoreTransformFactory::GetGUID() const
		{
			return CFactoryBase::GetGUID();
		}

		const char* CScoreTransformFactory::GetDescription() const
		{
			return m_description.c_str();
		}

		EScoreTransformType CScoreTransformFactory::GetScoreTransformType() const
		{
			return m_scoreTransformType;
		}

		void CScoreTransformFactory::InstantiateFactories()
		{
			// all currently built-in score transforms

			const char* szDescription_linear = "Simply preserves the original score.";
			const char* szDescription_linearInverse = "Subtracts the original score from 1.0";
			const char* szDescription_sine180 = "Maps the score onto a sine wave between [0 .. 180] degrees.\nThe lowest scores will be at the beginning and end, and the peak in the middle of the curve.";
			const char* szDescription_sine180inverse = "Similar to the Sine 180 transform, except that the result is subtracted from 1.0\nThat means that the peaks will be at the beginning and end, while the middle yields a score of 0.0";
			const char* szDescription_square = "Squares the original score, thus giving an increasing gradient with increasing scores.";
			const char* szDescription_squareInverse = "Subtracts the squared score from 1.0, thus giving a negative gradient which decreases even further with increasing scores.";

			static const CScoreTransformFactory scoreTransformFactory_linear("Linear", "a4782c62-101b-4039-a68d-c115b65493b1"_cry_guid, szDescription_linear, EScoreTransformType::Linear);
			static const CScoreTransformFactory scoreTransformFactory_linearInverse("LinearInverse", "92bb5c1d-f6a3-4477-a24d-adb04b4b6828"_cry_guid, szDescription_linearInverse, EScoreTransformType::LinearInverse);
			static const CScoreTransformFactory scoreTransformFactory_sine180("Sine180", "c1593038-fc89-43cc-ac1f-6d2ef563fe14"_cry_guid, szDescription_sine180, EScoreTransformType::Sine180);
			static const CScoreTransformFactory scoreTransformFactory_sine180inverse("Sine180Inverse", "8b81091c-6de7-45b6-a58c-12d03cbf2dad"_cry_guid, szDescription_sine180inverse, EScoreTransformType::Sine180Inverse);
			static const CScoreTransformFactory scoreTransformFactory_square("Square", "b52a94b5-3322-4b02-a9d8-c388d6b6cf31"_cry_guid, szDescription_square, EScoreTransformType::Square);
			static const CScoreTransformFactory scoreTransformFactory_squareInverse("SquareInverse", "752ad11d-8cf4-42e8-b6ae-8d8fb6ccc7a8"_cry_guid, szDescription_squareInverse, EScoreTransformType::SquareInverse);

			s_pDefaultScoreTransformFactory = &scoreTransformFactory_linear;
		}

		const CScoreTransformFactory& CScoreTransformFactory::GetDefaultScoreTransformFactory()
		{
			assert(s_pDefaultScoreTransformFactory);
			return *s_pDefaultScoreTransformFactory;
		}

	}
}
