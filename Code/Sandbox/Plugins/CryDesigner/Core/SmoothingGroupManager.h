// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "SmoothingGroup.h"

namespace Designer
{
class Model;

class SmoothingGroupManager : public _i_reference_target_t
{
public:

	void                                              Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo, Model* pModel);

	bool                                              AddSmoothingGroup(const char* id_name, SmoothingGroupPtr pSmoothingGroup);
	bool                                              AddPolygon(const char* id_name, PolygonPtr pPolygon);
	void                                              RemoveSmoothingGroup(const char* id_name);
	SmoothingGroupPtr                                 GetSmoothingGroup(const char* id_name) const;
	SmoothingGroupPtr                                 FindSmoothingGroup(PolygonPtr pPolygon) const;
	const char*                                       GetSmoothingGroupID(PolygonPtr pPolygon) const;
	const char*                                       GetSmoothingGroupID(SmoothingGroupPtr pSmoothingGroup) const;

	void                                              InvalidateSmoothingGroup(PolygonPtr pPolygon);
	void                                              InvalidateAll();

	int                                               GetCount() { return m_SmoothingGroups.size(); }

	std::vector<std::pair<string, SmoothingGroupPtr>> GetSmoothingGroupList() const;

	void                                              Clear();
	void                                              Clean(Model* pModel);
	void                                              RemovePolygon(PolygonPtr pPolygon);

	string                                            GetEmptyGroupID() const;

	void                                              CopyFromModel(Model* pModel, const Model* pSourceModel);

private:

	typedef std::map<string, SmoothingGroupPtr> SmoothingGroupMap;
	typedef std::map<PolygonPtr, string>        InvertSmoothingGroupMap;

	SmoothingGroupMap       m_SmoothingGroups;
	InvertSmoothingGroupMap m_MapPolygon2GroupId;
};
}

