// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#include "../Interface/ISensorTagLibrary.h"

namespace Cry
{
	namespace SensorSystem
	{
		class CSensorTagLibrary : public ISensorTagLibrary
		{
		private:

			typedef Schematyc::CStringHashWrapper<Schematyc::CFastStringHash, Schematyc::CEmptyStringConversion, string> TagName;
			typedef std::unordered_map<TagName, SensorTags>                                                              Tags;
			typedef std::vector<SensorTags>                                                                              FreeTags;

		public:

			CSensorTagLibrary();

			// ISensorTagLibrary
			virtual SensorTags CreateTag(const char* szTagName) override;
			virtual SensorTags GetTag(const char* szTagName) const override;
			virtual void GetTagNames(DynArray<const char*>& tagNames, SensorTags tags) const override;
			// ~ISensorTagLibrary

			bool SerializeTagName(Serialization::IArchive& archive, string& value, const char* szName, const char* szLabel);

		private:

			void ReleaseTag(const char* szTagName);
			const char* GetTagName(SensorTags value) const;
			void PopulateTagNames();

		private:

			Tags                      m_tags;
			FreeTags                  m_freeTags;
			Serialization::StringList m_tagNames;
		};
	}
}