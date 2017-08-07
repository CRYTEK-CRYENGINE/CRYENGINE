// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{

			//===================================================================================
			//
			// CInputParameterRegistry
			//
			// - used by various factories to store the kind of parameter they expect
			// - used by various blueprints to check for type consistency at load time
			//
			//===================================================================================

			class CInputParameterRegistry final : public IInputParameterRegistry
			{
			private:
				struct SStoredParameterInfo
				{
					explicit                        SStoredParameterInfo(const char* _szName, const CInputParameterID& _id, const Shared::CTypeInfo& _type, size_t _offset, const char* _szDescription);

					string                          name;
					CInputParameterID               id;
					const Shared::CTypeInfo&        type;
					size_t                          offset;
					string                          description;
				};

			public:
				// IInputParameterRegistry
				virtual size_t                      GetParameterCount() const override;
				virtual SParameterInfo              GetParameter(size_t index) const override;
				// ~IInputParameterRegistry

				void                                RegisterParameterType(const char* szParamName, const char (&idAsFourCharacterString)[5], const Shared::CTypeInfo& typeInfo, size_t offset, const char* szDescription);

			private:
				std::vector<SStoredParameterInfo>   m_parametersInOrder;
			};

		}
	}
}
