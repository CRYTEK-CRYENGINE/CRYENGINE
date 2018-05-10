// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/NavigationSystem/Annotation.h>

namespace MNM
{

class CAnnotationsLibrary : public IAnnotationsLibrary
{
public:
	CAnnotationsLibrary()
		: m_defaultColor(Col_Azure, 0.65f)
	{}

	// IAnnotationsLibrary
	virtual NavigationAreaTypeID GetAreaTypeID(const char* szName) const override;
	virtual const char*          GetAreaTypeName(const NavigationAreaTypeID areaTypeID) const override;
	virtual const MNM::SAreaType*     GetAreaType(const NavigationAreaTypeID areaTypeID) const override;
	virtual size_t               GetAreaTypeCount() const override;
	virtual NavigationAreaTypeID GetAreaTypeID(const size_t index) const override;
	virtual const MNM::SAreaType*     GetAreaType(const size_t index) const override;
	virtual const MNM::SAreaType&     GetDefaultAreaType() const override;

	virtual NavigationAreaFlagID GetAreaFlagID(const char* szName) const override;
	virtual const char*          GetAreaFlagName(const NavigationAreaFlagID areaFlagID) const override;
	virtual const SAreaFlag*     GetAreaFlag(const NavigationAreaFlagID areaFlagID) const override;
	virtual size_t               GetAreaFlagCount() const override;
	virtual NavigationAreaFlagID GetAreaFlagID(const size_t index) const override;
	virtual const SAreaFlag*     GetAreaFlag(const size_t index) const override;

	virtual void                 GetAreaColor(const MNM::AreaAnnotation annotation, ColorB& color) const override;
	// ~IAnnotationsLibrary

	void                 Clear();

	NavigationAreaTypeID CreateAreaType(const uint32 id, const char* szName, const uint32 defaultFlags, const ColorB* pColor = nullptr);
	NavigationAreaFlagID CreateAreaFlag(const uint32 id, const char* szName, const ColorB* pColor = nullptr);

	void                 SetDefaultAreaColor(const ColorB& color) { m_defaultColor = color; }
	bool                 GetFirstFlagColor(const MNM::AreaAnnotation::value_type flags, ColorB& color) const;
private:
	std::vector<MNM::SAreaType> m_areaTypes;
	std::vector<SAreaFlag> m_areaFlags;

	std::unordered_map<MNM::AreaAnnotation::value_type, ColorB> m_areasColorMap;
	std::unordered_map<MNM::AreaAnnotation::value_type, ColorB> m_flagsColorMap;
	ColorB m_defaultColor;
};

} // namespace MNM