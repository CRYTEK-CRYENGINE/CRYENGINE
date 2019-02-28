// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Serialization/Resources/IGameResourceList.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_StringHashWrapper.h>

namespace Schematyc2
{
	class CGameResourceList : public IGameResourceList
	{
	private:
		typedef TemplateUtils::CStringHashWrapper<TemplateUtils::CStringHashCRC32, TemplateUtils::CEmptyStringConversion, string> ResourceName;

		struct SResource
		{
			SResource(ResourceName _name, IGameResourceList::EType _type)
				: name(_name)
				, type(_type)
			{

			}

			inline bool operator==(const SResource& rhs) const { return (name == rhs.name) && (type == rhs.type); }
			inline bool operator< (const SResource& rhs) const { return (type < rhs.type) || ((type == rhs.type) && (name < rhs.name)); }

			ResourceName             name;
			IGameResourceList::EType type;
		};

	protected:

		// IGameResourceList
		virtual void   AddResource(const char* szResource, EType type) override;
		virtual size_t GetResourceCount() const override;
		virtual SItem  GetResourceItemAt(size_t idx) const override;
		virtual void   Sort() override;
		//~IGameResourceList

	private:
		std::vector<SResource> m_resources;
	};
}
