// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// ILeafFunctionReturnValue
		//
		// - allows leaf-functions to get access to the data for returning it when being called
		// - it's a more convenient and unified way than accessing the SQueryBlackboard directly
		// - also, it bears the option to carry "pre-computed" data ready to be used (e. g. literal values will already be de-serialized)
		//
		//===================================================================================

		struct ILeafFunctionReturnValue
		{
			struct SLiteralInfo
			{
				explicit                       SLiteralInfo(const Shared::CTypeInfo& _type, const void* _pValue);

				const Shared::CTypeInfo&       type;
				const void*                    pValue;
			};

			struct SGlobalParamInfo
			{
				explicit                       SGlobalParamInfo(bool _bGlobalParamExists, const Shared::CTypeInfo* _pTypeOfGlobalParam, const void* _pValueOfGlobalParam, const char* _szNameOfGlobalParam);

				bool                           bGlobalParamExists;
				const Shared::CTypeInfo*       pTypeOfGlobalParam;    // is a nullptr if .bGlobalParamExists == false
				const void*                    pValueOfGlobalParam;   // ditto
				const char*                    szNameOfGlobalParam;   // never a nullptr, of course
			};

			struct SItemIterationInfo
			{
				explicit                       SItemIterationInfo(const IItemList* _pGeneratedItems);

				const IItemList*               pGeneratedItems;       // is a nullptr if being used by a function that is not part of the evaluation phase (the function is most likely being used by a generator in that case)
			};

			struct SShuttledItemsInfo
			{
				explicit                       SShuttledItemsInfo(const IItemList* _pShuttledItems);

				const IItemList*               pShuttledItems;        // is a nullptr if there is no previous query in the chain that could have put its result-set on the blackboard
			};

			virtual                            ~ILeafFunctionReturnValue() {}
			virtual SLiteralInfo               GetLiteral(const SQueryBlackboard& blackboard) const = 0;
			virtual SGlobalParamInfo           GetGlobalParam(const SQueryBlackboard& blackboard) const = 0;
			virtual SItemIterationInfo         GetItemIteration(const SQueryBlackboard& blackboard) const = 0;
			virtual SShuttledItemsInfo         GetShuttledItems(const SQueryBlackboard& blackboard) const = 0;
		};

		inline ILeafFunctionReturnValue::SLiteralInfo::SLiteralInfo(const Shared::CTypeInfo& _type, const void* _pValue)
			: type(_type)
			, pValue(_pValue)
		{}

		inline ILeafFunctionReturnValue::SGlobalParamInfo::SGlobalParamInfo(bool _bGlobalParamExists, const Shared::CTypeInfo* _pTypeOfGlobalParam, const void* _pValueOfGlobalParam, const char* _szNameOfGlobalParam)
			: bGlobalParamExists(_bGlobalParamExists)
			, pTypeOfGlobalParam(_pTypeOfGlobalParam)
			, pValueOfGlobalParam(_pValueOfGlobalParam)
			, szNameOfGlobalParam(_szNameOfGlobalParam)
		{}

		inline ILeafFunctionReturnValue::SItemIterationInfo::SItemIterationInfo(const IItemList* _pGeneratedItems)
			: pGeneratedItems(_pGeneratedItems)
		{}

		inline ILeafFunctionReturnValue::SShuttledItemsInfo::SShuttledItemsInfo(const IItemList* _pShuttledItems)
			: pShuttledItems(_pShuttledItems)
		{}

	}
}
