// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client { struct IFunctionFactory; }
	namespace Client { struct IInputParameterRegistry; }

	namespace Core
	{

		class CInputBlueprint;

		//===================================================================================
		//
		// CTextualInputBlueprint
		//
		// - used by a loader to build a preliminary input hierarchy for a blueprint from an abstract data source (e. g. XML, JSON, UI)
		// - that input hierarchy cannot be used directly, it needs to get validated and resolved into a CInputBlueprint
		//
		//===================================================================================

		class CTextualInputBlueprint final : public ITextualInputBlueprint
		{
		public:
			explicit                                    CTextualInputBlueprint();
			                                            ~CTextualInputBlueprint();

			virtual const char*                         GetParamName() const override;
			virtual const Client::CInputParameterID&    GetParamID() const override;
			virtual const char*                         GetFuncName() const override;
			virtual const CryGUID&                      GetFuncGUID() const override;
			virtual const char*                         GetFuncReturnValueLiteral() const override;
			virtual bool                                GetAddReturnValueToDebugRenderWorldUponExecution() const override;

			virtual void                                SetParamName(const char* szParamName) override;
			virtual void                                SetParamID(const Client::CInputParameterID& paramID) override;
			virtual void                                SetFuncName(const char* szFuncName) override;
			virtual void                                SetFuncGUID(const CryGUID& funcGUID) override;
			virtual void                                SetFuncReturnValueLiteral(const char* szValue) override;
			virtual void                                SetAddReturnValueToDebugRenderWorldUponExecution(bool bAddReturnValueToDebugRenderWorldUponExecution) override;

			virtual ITextualInputBlueprint&             AddChild(const char* szParamName, const Client::CInputParameterID& paramID, const char* szFuncName, const CryGUID& funcGUID, const char* szFuncReturnValueLiteral, bool bAddReturnValueToDebugRenderWorldUponExecution) override;
			virtual size_t                              GetChildCount() const override;
			virtual const ITextualInputBlueprint&       GetChild(size_t index) const override;
			virtual const ITextualInputBlueprint*       FindChildByParamName(const char* szParamName) const override;
			virtual const ITextualInputBlueprint*       FindChildByParamID(const Client::CInputParameterID& paramID) const override;

			virtual void                                SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector) override;
			virtual DataSource::ISyntaxErrorCollector*  GetSyntaxErrorCollector() const override;

		private:
			explicit                                    CTextualInputBlueprint(const char* szParamName, const Client::CInputParameterID& paramID, const char* szFuncName, const CryGUID& funcGUID, const char* szFuncReturnValueLiteral, bool bAddReturnValueToDebugRenderWorldUponExecution);

			                                            UQS_NON_COPYABLE(CTextualInputBlueprint);

		private:
			string                                      m_paramName;
			Client::CInputParameterID                   m_paramID;
			string                                      m_funcName;
			CryGUID                                     m_funcGUID;
			string                                      m_funcReturnValueLiteral;
			bool                                        m_bAddReturnValueToDebugRenderWorldUponExecution;
			std::vector<CTextualInputBlueprint*>        m_children;
			DataSource::SyntaxErrorCollectorUniquePtr   m_pSyntaxErrorCollector;
		};

		class CQueryBlueprint;

		//===================================================================================
		//
		// CInputBlueprint
		//
		// - result after resolving the preliminary input hierarchy that was loaded from an abstract data source (e. g. XML, JSON, UI)
		// - allows traversing the hierarchy and grabbing the function factories for building the actual function blueprints
		//
		//===================================================================================

		class CInputBlueprint
		{
		public:
			explicit                            CInputBlueprint();
			                                    ~CInputBlueprint();

			bool                                Resolve(const ITextualInputBlueprint& sourceParent, const Client::IInputParameterRegistry& inputParamsReg, const CQueryBlueprint& queryBlueprintForGlobalParamChecking, bool bResolvingForAGenerator);

			size_t                              GetChildCount() const;
			const CInputBlueprint&              GetChild(size_t index) const;
			Client::IFunctionFactory*           GetFunctionFactory() const;
			const CLeafFunctionReturnValue&     GetLeafFunctionReturnValue() const;
			bool                                GetAddReturnValueToDebugRenderWorldUponExecution() const;

		private:
			                                    UQS_NON_COPYABLE(CInputBlueprint);

		private:
			Client::IFunctionFactory*           m_pFunctionFactory;
			CLeafFunctionReturnValue            m_leafFunctionReturnValue;
			bool                                m_bAddReturnValueToDebugRenderWorldUponExecution;
			std::vector<CInputBlueprint*>       m_children;	// in order of how the input parameters need to appear at runtime
		};

	}
}
