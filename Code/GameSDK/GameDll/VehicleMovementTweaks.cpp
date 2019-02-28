// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a class which handle case (group) specific 
tweaking of values of a vehicle movement

-------------------------------------------------------------------------
History:
- 13:06:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "VehicleMovementTweaks.h"
	
CVehicleMovementTweaks::CVehicleMovementTweaks()
{
	m_groups.clear();
}

//------------------------------------------------------------------------
bool CVehicleMovementTweaks::Init(const CVehicleParams& table)
{
  m_groups.clear();

	if (CVehicleParams tweakGroupsTable = table.findChild("TweakGroups"))
	{
		int i = 0;
		int c = tweakGroupsTable.getChildCount();

		m_groups.reserve(c);

		for (; i < c; i++)
		{
			CVehicleParams groupTable = tweakGroupsTable.getChild(i);
			AddGroup(groupTable);
		}
	}

	return (m_groups.size() > 0);
}

//------------------------------------------------------------------------
bool CVehicleMovementTweaks::AddGroup(const CVehicleParams& table)
{
	string groupName = table.getAttr("name");
	if (groupName.empty())
		return InvalidTweakGroupId;

	m_groups.resize(m_groups.size() + 1);
	SGroup& newGroup = m_groups.back();

	newGroup.name = groupName;
	newGroup.isEnabled = false;

	if (CVehicleParams tweaksTable = table.findChild("Tweaks"))
	{
		int i = 0;
		int c = tweaksTable.getChildCount();

		newGroup.values.reserve(c);

		for (; i < c; i++)
		{
			CVehicleParams tweakRef = tweaksTable.getChild(i);
			if (tweakRef && tweakRef.getChildCount() > 0)
			{
				if (tweakRef.haveAttr("name"))
				{
					TValueId valueId = GetValueId(tweakRef.getAttr("name"));
					if (valueId > -1)
					{
						SGroup::SValueInGroup valueInGroup;
						const SValue& v = m_values[valueId];

						valueInGroup.valueId = valueId;
						tweakRef.getAttr("value", valueInGroup.value);

						if (!tweakRef.getAttr("op", valueInGroup.op))
							valueInGroup.op = 0;

						if (!v.isRestrictedToMult || v.isRestrictedToMult && valueInGroup.op == eTVO_Multiplier)
							newGroup.values.push_back(valueInGroup);
						else
							CryLog("VehicleMovementTweaks Warning: the value <%s> can only be tweaked with a multiplyer.", 
								tweakRef.getAttr("name"));
					}
				}
			}
		}

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicleMovementTweaks::AddValue(const char* valueName, float* pValue, bool isRestrictedToMult)
{
	m_values.resize(m_values.size() + 1);
	SValue& newValue = m_values.back();

	newValue.name = valueName;
	newValue.pValue = pValue;
	newValue.defaultValue = *pValue;
	newValue.isRestrictedToMult = isRestrictedToMult;
  newValue.blocked = false;
}

//------------------------------------------------------------------------
bool CVehicleMovementTweaks::UseGroup(TTweakGroupId groupId)
{
	if (groupId < 0 || ((int)m_groups.size()) <= groupId)
		return false;

	SGroup& group = m_groups[groupId];

	if (group.isEnabled)
		return false;

	group.isEnabled = true;
	ComputeGroup(group);

	return true;
}

//------------------------------------------------------------------------
bool CVehicleMovementTweaks::RevertGroup(TTweakGroupId groupId)
{
	SGroup& group = m_groups[groupId];

	if (!group.isEnabled)
		return false;

  group.isEnabled = false;

	ComputeGroups();
	
	return true;
}

//------------------------------------------------------------------------
bool CVehicleMovementTweaks::RevertValues()
{
	TValueVector::iterator valueIte = m_values.begin();
	TValueVector::iterator valueEnd = m_values.end();

	for (; valueIte != valueEnd; ++valueIte)
	{
		SValue& valueInfo = *valueIte;
		(*valueInfo.pValue) = valueInfo.defaultValue;
	}

	TGroupVector::iterator groupIte = m_groups.begin();
	TGroupVector::iterator groupEnd = m_groups.end();

  bool reverted = false;

	for (; groupIte != groupEnd; ++groupIte)
	{
		SGroup& groupInfo = *groupIte;
		
    if (groupInfo.isEnabled)
    {
      groupInfo.isEnabled = false;
      reverted = true;
    }    
	}

	return reverted;
}

//------------------------------------------------------------------------
void CVehicleMovementTweaks::ComputeGroups()
{
	TValueVector::iterator valueIte = m_values.begin();
	TValueVector::iterator valueEnd = m_values.end();

	for (; valueIte != valueEnd; ++valueIte)
	{
		SValue& valueInfo = *valueIte;
		(*valueInfo.pValue) = valueInfo.defaultValue;
	}

	TGroupVector::iterator groupIte = m_groups.begin();
	TGroupVector::iterator groupEnd = m_groups.end();

	for (; groupIte != groupEnd; ++groupIte)
	{
		SGroup& groupInfo = *groupIte;

		if (groupInfo.isEnabled)
			ComputeGroup(groupInfo);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementTweaks::ComputeGroup(const SGroup& group)
{
	SGroup::TValueInGroupVector::const_iterator valueInGroupIte = group.values.begin();
	SGroup::TValueInGroupVector::const_iterator valueInGroupEnd = group.values.end();

	for (; valueInGroupIte != valueInGroupEnd; ++valueInGroupIte)
	{
		const SGroup::SValueInGroup& v = *valueInGroupIte;

		SValue& valueInfo = m_values[v.valueId];

    if (valueInfo.blocked)
      continue;

		if (v.op == eTVO_Replace)
			(*valueInfo.pValue) = v.value;
		else if (v.op == eTVO_Multiplier)
			(*valueInfo.pValue) *= v.value;
	}
}

//------------------------------------------------------------------------
void CVehicleMovementTweaks::BlockValue(TValueId valueId, bool block)
{ 
  assert(valueId >= 0 && valueId < (int)m_values.size());
    
  m_values[valueId].blocked = block;  
}

//------------------------------------------------------------------------
CVehicleMovementTweaks::TTweakGroupId CVehicleMovementTweaks::GetGroupId(const char* name)
{
	TTweakGroupId groupId = 0;

	TGroupVector::iterator groupIte = m_groups.begin();
	TGroupVector::iterator groupEnd = m_groups.end();

	for (; groupIte != groupEnd; ++groupIte)
	{
		const SGroup& g = *groupIte;

		if (g.name == name)
			return groupId;

		groupId++;
	}

	return -1;
}

//------------------------------------------------------------------------
CVehicleMovementTweaks::TValueId CVehicleMovementTweaks::GetValueId(const char* name)
{
	TValueId valueId = 0;

	for (TValueVector::iterator ite = m_values.begin(); ite != m_values.end(); ++ite)
	{
		SValue& valueInfo = *ite;

		if (valueInfo.name == name)
			return valueId;

		valueId++;
	}

	return -1;
}

//------------------------------------------------------------------------
void CVehicleMovementTweaks::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() == eST_SaveGame)
	{
		ser.BeginGroup("TweakGroups");
		TGroupVector::iterator groupIte = m_groups.begin();
		TGroupVector::iterator groupEnd = m_groups.end();

		for (; groupIte != groupEnd; ++groupIte)
		{
			SGroup& groupInfo = *groupIte;

			ser.BeginGroup(groupInfo.name);
			ser.Value("isEnabled", groupInfo.isEnabled);
			ser.EndGroup();
		}

		if (ser.IsReading())
			ComputeGroups();

		ser.EndGroup();
	}
}
