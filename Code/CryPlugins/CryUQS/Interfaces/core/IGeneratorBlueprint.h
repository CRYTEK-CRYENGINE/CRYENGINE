// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// ITextualGeneratorBlueprint
		//
		//===================================================================================

		struct ITextualGeneratorBlueprint
		{
			virtual                                       ~ITextualGeneratorBlueprint() {}
			virtual void                                  SetGeneratorName(const char* szGeneratorName) = 0;
			virtual void                                  SetGeneratorGUID(const CryGUID& generatorGUID) = 0;
			virtual ITextualInputBlueprint&               GetInputRoot() = 0;
			virtual const char*                           GetGeneratorName() const = 0;
			virtual const CryGUID&                        GetGeneratorGUID() const = 0;
			virtual const ITextualInputBlueprint&         GetInputRoot() const = 0;
			virtual void                                  SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector) = 0;
			virtual DataSource::ISyntaxErrorCollector*    GetSyntaxErrorCollector() const = 0;     // called while resolving a blueprint from its textual representation into the "in-memory" representation
		};

	}
}
