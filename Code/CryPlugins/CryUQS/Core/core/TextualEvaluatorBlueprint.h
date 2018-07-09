// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTextualEvaluatorBlueprint
		//
		//===================================================================================

		class CTextualEvaluatorBlueprint final : public ITextualEvaluatorBlueprint
		{
		public:

			explicit                                      CTextualEvaluatorBlueprint();

			// ITextualEvaluatorBlueprint
			virtual const char*                           GetEvaluatorName() const override;
			virtual void                                  SetEvaluatorName(const char* szEvaluatorName) override;
			virtual const CryGUID&                        GetEvaluatorGUID() const override;
			virtual void                                  SetEvaluatorGUID(const CryGUID& evaluatorGUID) override;
			virtual float                                 GetWeight() const override;
			virtual void                                  SetWeight(float weight) override;
			virtual const char*                           GetScoreTransformName() const override;
			virtual void                                  SetScoreTransformName(const char* szScoreTransformName) override;
			virtual const CryGUID&                        GetScoreTransformGUID() const override;
			virtual void                                  SetScoreTransformGUID(const CryGUID& scoreTransformGUID) override;
			virtual bool                                  GetNegateDiscard() const override;
			virtual void                                  SetNegateDiscard(bool bNegateDiscard) override;
			virtual const ITextualInputBlueprint&         GetInputRoot() const override;
			virtual ITextualInputBlueprint&               GetInputRoot() override;
			virtual DataSource::ISyntaxErrorCollector*    GetSyntaxErrorCollector() const override;
			virtual void                                  SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector) override;
			// ~ITextualEvaluatorBlueprint

		private:

			                                              UQS_NON_COPYABLE(CTextualEvaluatorBlueprint);

		private:

			string                                        m_evaluatorName;
			CryGUID                                       m_evaluatorGUID;
			float                                         m_weight;
			CTextualInputBlueprint                        m_rootInput;
			string                                        m_scoreTransformName;
			CryGUID                                       m_scoreTransformGUID;
			bool                                          m_bNegateDiscard;
			DataSource::SyntaxErrorCollectorUniquePtr     m_pSyntaxErrorCollector;
		};

		inline CTextualEvaluatorBlueprint::CTextualEvaluatorBlueprint()
			: m_evaluatorName()
			, m_evaluatorGUID(CryGUID::Null())
			, m_weight(1.0f)
			, m_rootInput()
			, m_scoreTransformName(CScoreTransformFactory::GetDefaultScoreTransformFactory().GetName())
			, m_scoreTransformGUID(CScoreTransformFactory::GetDefaultScoreTransformFactory().GetGUID())
			, m_bNegateDiscard(false)
			, m_pSyntaxErrorCollector()
		{
			// nothing
		}

		inline const char* CTextualEvaluatorBlueprint::GetEvaluatorName() const
		{
			return m_evaluatorName.c_str();
		}

		inline void CTextualEvaluatorBlueprint::SetEvaluatorName(const char* szEvaluatorName)
		{
			m_evaluatorName = szEvaluatorName;
		}

		inline const CryGUID& CTextualEvaluatorBlueprint::GetEvaluatorGUID() const
		{
			return m_evaluatorGUID;
		}

		inline void CTextualEvaluatorBlueprint::SetEvaluatorGUID(const CryGUID& evaluatorGUID)
		{
			m_evaluatorGUID = evaluatorGUID;
		}

		inline float CTextualEvaluatorBlueprint::GetWeight() const
		{
			return m_weight;
		}

		inline void CTextualEvaluatorBlueprint::SetWeight(float weight)
		{
			m_weight = weight;
		}

		inline const char* CTextualEvaluatorBlueprint::GetScoreTransformName() const
		{
			return m_scoreTransformName.c_str();
		}

		inline void CTextualEvaluatorBlueprint::SetScoreTransformName(const char* szScoreTransformName)
		{
			m_scoreTransformName = szScoreTransformName;
		}

		inline const CryGUID& CTextualEvaluatorBlueprint::GetScoreTransformGUID() const
		{
			return m_scoreTransformGUID;
		}

		inline void CTextualEvaluatorBlueprint::SetScoreTransformGUID(const CryGUID& scoreTransformGUID)
		{
			m_scoreTransformGUID = scoreTransformGUID;
		}

		inline bool CTextualEvaluatorBlueprint::GetNegateDiscard() const
		{
			return m_bNegateDiscard;
		}

		inline void CTextualEvaluatorBlueprint::SetNegateDiscard(bool bNegateDiscard)
		{
			m_bNegateDiscard = bNegateDiscard;
		}

		inline const ITextualInputBlueprint& CTextualEvaluatorBlueprint::GetInputRoot() const
		{
			return m_rootInput;
		}

		inline ITextualInputBlueprint& CTextualEvaluatorBlueprint::GetInputRoot()
		{
			return m_rootInput;
		}

		inline DataSource::ISyntaxErrorCollector* CTextualEvaluatorBlueprint::GetSyntaxErrorCollector() const
		{
			return m_pSyntaxErrorCollector.get();
		}

		inline void CTextualEvaluatorBlueprint::SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector)
		{
			m_pSyntaxErrorCollector = std::move(pSyntaxErrorCollector);
		}

	}
}
