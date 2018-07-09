// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// ITextualGlobalConstantParamsBlueprint
		//
		//===================================================================================

		struct ITextualGlobalConstantParamsBlueprint
		{
			struct SParameterInfo
			{
				explicit                              SParameterInfo(const char* _szName, const char* _szTypeName, const CryGUID& _typeGUID, const char* _szValue, bool _bAddToDebugRenderWorld, DataSource::ISyntaxErrorCollector* _pSyntaxErrorCollector);

				const char*                           szName;
				const char*                           szTypeName;
				CryGUID                               typeGUID;
				const char*                           szValue;
				bool                                  bAddToDebugRenderWorld;
				DataSource::ISyntaxErrorCollector*    pSyntaxErrorCollector;   // might be nullptr
			};

			virtual                                   ~ITextualGlobalConstantParamsBlueprint() {}
			virtual void                              AddParameter(const char* szName, const char* szTypeName, const CryGUID& typeGUID, const char* szValue, bool bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector) = 0;   // adding a parameter with the same name twice is NOT an error in this class
			virtual size_t                            GetParameterCount() const = 0;
			virtual SParameterInfo                    GetParameter(size_t index) const = 0;
		};

		inline ITextualGlobalConstantParamsBlueprint::SParameterInfo::SParameterInfo(const char* _szName, const char* _szTypeName, const CryGUID& _typeGUID, const char* _szValue, bool _bAddToDebugRenderWorld, DataSource::ISyntaxErrorCollector* _pSyntaxErrorCollector)
			: szName(_szName)
			, szTypeName(_szTypeName)
			, typeGUID(_typeGUID)
			, szValue(_szValue)
			, bAddToDebugRenderWorld(_bAddToDebugRenderWorld)
			, pSyntaxErrorCollector(_pSyntaxErrorCollector)
		{}

	}
}
