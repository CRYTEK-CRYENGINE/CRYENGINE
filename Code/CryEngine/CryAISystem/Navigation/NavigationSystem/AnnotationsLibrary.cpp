// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnnotationsLibrary.h"

namespace MNM
{

void CAnnotationsLibrary::Clear()
{
	m_areaTypes.clear();
	m_areaFlags.clear();

	m_areasColorMap.clear();
	m_flagsColorMap.clear();

	m_defaultColor = ColorB(Col_Azure, 0.65f);

	CreateAreaFlag(0, "Walkable");
	CreateAreaType(0, "Default", BIT(0));
}

NavigationAreaTypeID CAnnotationsLibrary::CreateAreaType(const uint32 id, const char* szName, const uint32 defaultFlags, const ColorB* pColor)
{
	CRY_ASSERT(szName);

	if (id >= AreaAnnotation::MaxAreasCount())
	{
		AIWarning("Trying to create NavigationSystem AreaType with invalid id '%u'. Id must be between 0 and %d!", id, AreaAnnotation::MaxAreasCount());
		CRY_ASSERT_MESSAGE(id < AreaAnnotation::MaxAreasCount(), "Trying to create NavigationSystem AreaType with invalid id '%u'. Id can be between 0 and %d!", id, AreaAnnotation::MaxAreasCount());
		return NavigationAreaTypeID();
	}

	for (SAreaType& areaType : m_areaTypes)
	{
		if (areaType.id == id)
		{
			if (id == 0)
			{
				if (areaType.name.CompareNoCase(szName) != 0 || areaType.defaultFlags != defaultFlags || (pColor && *pColor != m_areasColorMap[id]))
				{
					// Setting default area type with different name, flags or color
					AILogProgress("Overwriting default Navigation area type '%s' with area type '%s'.", areaType.name.c_str(), szName);

					areaType.defaultFlags = defaultFlags;
					areaType.name = szName;
					m_areasColorMap[id] = pColor ? *pColor : m_defaultColor;
				}
				return NavigationAreaTypeID(id);
			}

			AIWarning("Trying to create NavigationSystem AreaType with duplicate id '%u'. Area type %s has the same id!", id, areaType.name.c_str());
			CRY_ASSERT_MESSAGE(areaType.id != id, "Trying to create NavigationSystem AreaType with duplicate id '%u'. Area type %s has the same id!", id, areaType.name.c_str());
			return NavigationAreaTypeID();
		}
		if (!areaType.name.compareNoCase(szName))
		{
			AIWarning("Trying to create NavigationSystem AreaType with duplicate name '%s'!", szName);
			CRY_ASSERT_MESSAGE(areaType.name.compareNoCase(szName), "Trying to create NavigationSystem AreaType with duplicate name '%s'!", szName);
			return NavigationAreaTypeID();
		}
	}

	m_areaTypes.emplace_back(id, szName, defaultFlags);
	m_areasColorMap[id] = pColor ? *pColor : m_defaultColor;

	return NavigationAreaTypeID(id);
}

NavigationAreaTypeID CAnnotationsLibrary::GetAreaTypeID(const char* szName) const
{
	CRY_ASSERT(szName);

	for (size_t index = 0; index < m_areaTypes.size(); ++index)
	{
		if (!m_areaTypes[index].name.compareNoCase(szName))
			return NavigationAreaTypeID(m_areaTypes[index].id);
	}
	return NavigationAreaTypeID();
}

NavigationAreaTypeID CAnnotationsLibrary::GetAreaTypeID(const size_t index) const
{
	if (index < m_areaTypes.size())
		return NavigationAreaTypeID(m_areaTypes[index].id);

	return NavigationAreaTypeID();
}

const char* CAnnotationsLibrary::GetAreaTypeName(const NavigationAreaTypeID areaTypeID) const
{
	for (size_t index = 0; index < m_areaTypes.size(); ++index)
	{
		if (m_areaTypes[index].id == areaTypeID)
			return m_areaTypes[index].name.c_str();
	}
	return nullptr;
}

const SAreaType* CAnnotationsLibrary::GetAreaType(const NavigationAreaTypeID areaTypeID) const
{
	for (size_t index = 0; index < m_areaTypes.size(); ++index)
	{
		if (m_areaTypes[index].id == areaTypeID)
			return &m_areaTypes[index];
	}
	return nullptr;
}

const SAreaType* CAnnotationsLibrary::GetAreaType(const size_t index) const
{
	if (index < m_areaTypes.size())
		return &m_areaTypes[index];

	return nullptr;
}

const SAreaType& CAnnotationsLibrary::GetDefaultAreaType() const
{
	CRY_ASSERT(m_areaTypes.size());
	return m_areaTypes[0];
}

size_t CAnnotationsLibrary::GetAreaTypeCount() const
{
	return m_areaTypes.size();
}

//////////////////////////////////////////////////////////////////////////

NavigationAreaFlagID CAnnotationsLibrary::CreateAreaFlag(const uint32 id, const char* szName, const ColorB* pColor)
{
	CRY_ASSERT(szName);

	if (id >= AreaAnnotation::MaxFlagsCount())
	{
		AIWarning("Trying to create NavigationSystem AreaFlag with invalid id '%u'. Id must be between 0 and %d!", id, AreaAnnotation::MaxFlagsCount());
		CRY_ASSERT_MESSAGE(id < AreaAnnotation::MaxFlagsCount(), "Trying to create NavigationSystem AreaFlag with invalid id '%u'. Id can be between 0 and %d!", id, AreaAnnotation::MaxFlagsCount());
		return NavigationAreaFlagID();
	}

	for (SAreaFlag& areaFlag : m_areaFlags)
	{
		if (areaFlag.id == id)
		{
			if (id == 0)
			{
				if (areaFlag.name.CompareNoCase(szName) != 0 || (pColor && *pColor != areaFlag.color))
				{
					areaFlag.name = szName;
					if (pColor)
					{
						areaFlag.color = *pColor;
					}
					AILogProgress("Overwriting default Navigation area flag '%s' with area flag '%s'.", areaFlag.name.c_str(), szName);
				}
				return NavigationAreaFlagID(id);
			}

			AIWarning("Trying to create NavigationSystem AreaFlag with duplicate id '%u'. Area flag %s has the same id!", id, areaFlag.name.c_str());
			CRY_ASSERT_MESSAGE(areaFlag.id != id, "Trying to create NavigationSystem AreaFlag with duplicate id '%u'. Area flag %s has the same id!", id, areaFlag.name.c_str());
			return NavigationAreaFlagID();
		}
		if (!areaFlag.name.compareNoCase(szName))
		{
			AIWarning("Trying to create NavigationSystem AreaFlag with duplicate name '%s'!", szName);
			CRY_ASSERT_MESSAGE(areaFlag.name.compareNoCase(szName), "Trying to create NavigationSystem AreaFlag with duplicate name '%s'!", szName);
			return NavigationAreaFlagID();
		}
	}

	m_areaFlags.emplace_back(id, szName);
	if (pColor)
	{
		m_areaFlags.back().color = *pColor;
	}

	return NavigationAreaFlagID(id);
}

NavigationAreaFlagID CAnnotationsLibrary::GetAreaFlagID(const char* szName) const
{
	CRY_ASSERT(szName);

	for (size_t index = 0; index < m_areaFlags.size(); ++index)
	{
		if (!m_areaFlags[index].name.compareNoCase(szName))
			return NavigationAreaFlagID(m_areaFlags[index].id);
	}
	return NavigationAreaFlagID();
}

NavigationAreaFlagID CAnnotationsLibrary::GetAreaFlagID(const size_t index) const
{
	if (index < m_areaFlags.size())
		return NavigationAreaFlagID(m_areaFlags[index].id);

	return NavigationAreaFlagID();
}

const char* CAnnotationsLibrary::GetAreaFlagName(const NavigationAreaFlagID areaFlagID) const
{
	for (size_t index = 0; index < m_areaFlags.size(); ++index)
	{
		if (m_areaFlags[index].id == areaFlagID)
			return m_areaFlags[index].name.c_str();
	}
	return nullptr;
}

const SAreaFlag* CAnnotationsLibrary::GetAreaFlag(const NavigationAreaFlagID areaFlagID) const
{
	for (size_t index = 0; index < m_areaFlags.size(); ++index)
	{
		if (m_areaFlags[index].id == areaFlagID)
			return &m_areaFlags[index];
	}
	return nullptr;
}

const SAreaFlag* CAnnotationsLibrary::GetAreaFlag(const size_t index) const
{
	if (index < m_areaFlags.size())
		return &m_areaFlags[index];

	return nullptr;
}

size_t CAnnotationsLibrary::GetAreaFlagCount() const
{
	return m_areaFlags.size();
}

void CAnnotationsLibrary::GetAreaColor(const AreaAnnotation annotation, ColorB& color) const
{
	const auto findIt = m_areasColorMap.find(annotation.GetType());
	color = findIt != m_areasColorMap.end() ? findIt->second : m_defaultColor;
}

bool CAnnotationsLibrary::GetFirstFlagColor(const AreaAnnotation::value_type flags, ColorB& color) const
{
	if (flags == 0)
		return false;

	for (const SAreaFlag& areaFlag : m_areaFlags)
	{
		if (areaFlag.value & flags)
		{
			color = areaFlag.color;
			return true;
		}
	}
	return false;
}

} // namespace MNM