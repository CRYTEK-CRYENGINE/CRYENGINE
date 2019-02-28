// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySerialization/BlackBox.h"

#include <CrySchematyc2/Script/IScriptFile.h>
#include <CrySchematyc2/Script/IScriptGraph.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

namespace Schematyc2
{
	namespace DocSerializationUtils
	{
		DECLARE_SHARED_POINTERS(IScriptElement)

		class CDocGraphFactory // #SchematycTODO : Combine with CScriptElementFactory?
		{
		public:

			IDocGraphPtr CreateGraph(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, EScriptGraphType type, const SGUID& contextGUID);
		};

		// #SchematycTODO : Move to separate file .cpp and .h files and/or to IFramework? 
		// #SchematycTODO : Resolve disconnect between factory method used to construct elements during serialization and in place methods used when editing?
		class CScriptElementFactory
		{
		public:

			CScriptElementFactory();

			void SetDefaultElementType(EScriptElementType defaultElementType);
			IScriptElementPtr CreateElement(IScriptFile& file, EScriptElementType elementType);
			IScriptElementPtr CreateElement(Serialization::IArchive& archive, const char* szName);

		private:

			EScriptElementType m_defaultElementType; // N.B. This is a hack to work around the fact that elements are located in discreet sections of each file. In future we should inline element type.
		};

		class CInputBlock
		{
		public:

			typedef std::vector<size_t> DependencyIndices;

			struct SElement
			{
				SElement();

				void Serialize(Serialization::IArchive& archive);

				IScriptElementPtr        ptr;
				Serialization::SBlackBox blackBox;
				ESerializationPass       serializationPass;
				DependencyIndices        dependencyIndices;
				size_t                   sortPriority;
			};

			typedef std::vector<SElement> Elements;

		public:

			CInputBlock(IScriptFile& file);

			void Serialize(Serialization::IArchive& archive);
			void BuildElementHierarchy(IScriptElement& root);
			void SortElementsByDependency();
			Elements& GetElements();

		private:

			void AppendElements(Elements& dst, const Elements& src) const;
			void EraseEmptyElements(Elements &elements) const;
			void SortElementsByDependency(Elements &elements);
			void VerifyElementDependencies(const Elements &elements);

		private:

			IScriptFile& m_file;
			Elements     m_elements;
		};

		extern CDocGraphFactory      g_docGraphFactory;
		extern CScriptElementFactory g_scriptElementFactory;
	}
}
