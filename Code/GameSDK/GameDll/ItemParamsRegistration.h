// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Macros to help construct structs that automatically implement != and < 

-------------------------------------------------------------------------
History:
- 26:08:2011   10:15 : Created by Claire Allan

*************************************************************************/
#ifndef __ITEMPARAMSREGISTRATION_H__
#define __ITEMPARAMSREGISTRATION_H__

#if _MSC_VER > 1000
# pragma once
#endif

#define INSTANCE_MEMBERS(varType, varName) varType varName;
#define INSTANCE_MEMBER_ARRAYS(varType, varNum, varName) varType varName[varNum];

#define INEQUALITY_COMPARE_MEMBERS(varType, varName) if(instanceA.varName != instanceB.varName) { return true; }

#define INEQUALITY_COMPARE_MEMBER_ARRAYS(varType, varNum, varName) \
for(int i = 0; i < varNum; i++) \
{ if(instanceA.varName[i] != instanceB.varName[i]) { return true; } }

#define INEQUALITY_COMPARE_MEMBER_VECTORS(varType, varName) \
	int numInstanceA = instanceA.varName.size(); \
	int numInstanceB = instanceB.varName.size(); \
	if(numInstanceA != numInstanceB) { return true; } \
	for(int i = 0; i < numInstanceA; i++) { if(instanceA.varName[i] != instanceB.varName[i]) { return true; } }

#define INEQUALITY_COMPARE_MEMBER_MAPS(varType, varName) \
	int numInstanceA = instanceA.varName.size(); \
	int numInstanceB = instanceB.varName.size(); \
	if(numInstanceA != numInstanceB) { return true; } \
	varType::const_iterator itA = instanceA.varName.begin(); \
	varType::const_iterator itB = instanceB.varName.begin(); \
	varType::const_iterator itAEnd = instanceA.varName.end(); \
	while(itA != itAEnd) { if(itA->first != itB->first) { return true; } if(itA->second != itB->second) { return true; } itA++; itB++; }

#define LESSTHAN_COMPARE_MEMBERS(varType, varName) if(instanceA.varName != instanceB.varName) { return (instanceA.varName < instanceB.varName); }

#define LESSTHAN_COMPARE_MEMBER_ARRAYS(varType, varNum, varName) \
	for(int i = 0; i < varNum; i++) \
	{ if(instanceA.varName[i] != instanceB.varName[i]) { return (instanceA.varName[i] < instanceB.varName[i]); } }

#define LESSTHAN_COMPARE_MEMBER_VECTORS(varType, varName) \
	int numInstanceA = instanceA.varName.size(); \
	int numInstanceB = instanceB.varName.size(); \
	if(numInstanceA != numInstanceB) { return numInstanceA < numInstanceB; } \
	for(int i = 0; i < numInstanceA; i++) { if(instanceA.varName[i] != instanceB.varName[i]) { return (instanceA.varName[i] < instanceB.varName[i]); } }

#define LESSTHAN_COMPARE_MEMBER_MAPS(varType, varName) \
	int numInstanceA = instanceA.varName.size(); \
	int numInstanceB = instanceB.varName.size(); \
	if(numInstanceA != numInstanceB) { return numInstanceA < numInstanceB; } \
	varType::const_iterator itA = instanceA.varName.begin(); \
	varType::const_iterator itB = instanceB.varName.begin(); \
	varType::const_iterator itAEnd = instanceA.varName.end(); \
	while(itA != itAEnd) { if(itA->first != itB->first) { return itA->first < itB->first; } if(itA->second != itB->second) { return itA->second < itB->second; } itA++; itB++; }

#define REGISTER_STRUCT(varList, structType) \
	varList(INSTANCE_MEMBERS) \
	friend bool operator < (const structType& instanceA, const structType& instanceB); \
	friend bool operator != (const structType& instanceA, const structType& instanceB);

#define IMPLEMENT_OPERATORS(varList, structType) \
	bool operator < (const structType& instanceA, const structType& instanceB) { varList(LESSTHAN_COMPARE_MEMBERS) return false; } \
	bool operator != (const structType& instanceA, const structType& instanceB) { varList(INEQUALITY_COMPARE_MEMBERS) return false; } 

#define REGISTER_STRUCT_WITH_ARRAYS(varList, varArrayList, structType) \
	varList(INSTANCE_MEMBERS) \
	varArrayList(INSTANCE_MEMBER_ARRAYS) \
	friend bool operator < (const structType& instanceA, const structType& instanceB); \
	friend bool operator != (const structType& instanceA, const structType& instanceB);

#define IMPLEMENT_OPERATORS_WITH_ARRAYS(varList, varArrayList, structType) \
	bool operator < (const structType& instanceA, const structType& instanceB) \
	{ varList(LESSTHAN_COMPARE_MEMBERS) \
		varArrayList(LESSTHAN_COMPARE_MEMBER_ARRAYS) \
		return false; } \
	bool operator != (const structType& instanceA, const structType& instanceB) \
	{ varList(INEQUALITY_COMPARE_MEMBERS) \
		varArrayList(INEQUALITY_COMPARE_MEMBER_ARRAYS) \
		return false; }

#define REGISTER_STRUCT_WITH_TWO_LISTS(varList, varList2, structType) \
	varList(INSTANCE_MEMBERS) \
	varList2(INSTANCE_MEMBERS) \
	friend bool operator < (const structType& instanceA, const structType& instanceB); \
	friend bool operator != (const structType& instanceA, const structType& instanceB);

#define IMPLEMENT_OPERATORS_WITH_VECTORS(varList, varVectorList, structType) \
	bool operator < (const structType& instanceA, const structType& instanceB) \
	{ varList(LESSTHAN_COMPARE_MEMBERS) \
		varVectorList(LESSTHAN_COMPARE_MEMBER_VECTORS) \
		return false; } \
	bool operator != (const structType& instanceA, const structType& instanceB) \
	{ varList(INEQUALITY_COMPARE_MEMBERS) \
		varVectorList(INEQUALITY_COMPARE_MEMBER_VECTORS) \
		return false; }

#define IMPLEMENT_OPERATORS_WITH_MAPS(varList, varMapList, structType) \
	bool operator < (const structType& instanceA, const structType& instanceB) \
	{ varList(LESSTHAN_COMPARE_MEMBERS) \
		varMapList(LESSTHAN_COMPARE_MEMBER_MAPS) \
		return false; } \
	bool operator != (const structType& instanceA, const structType& instanceB) \
	{ varList(INEQUALITY_COMPARE_MEMBERS) \
		varMapList(INEQUALITY_COMPARE_MEMBER_MAPS) \
		return false; }

#define FORWARD_DECLARE_STRUCTS(structType) struct structType;
#define INSTANCE_SETS(structType) std::set<structType> m_##structType;
#define DECLARE_SET_FUNCS(structType) const structType&	Store##structType(const structType& p);
#define CLEAR_SETS(structType) m_##structType.clear();

#define IMPLEMENT_SET_FUNCS(structType) \
const structType&	CGameSharedParametersStorage::Store##structType(const structType& p) \
{ return *(m_##structType.insert(p).first); } 

#define ITEM_PARAM_STRUCT_TYPES(f) \
	f(SAimLookParameters) \
	f(SFireParams) \
	f(STracerParams) \
	f(SEffectParams) \
	f(SRecoilParams) \
	f(SRecoilHints) \
	f(SProceduralRecoilParams) \
	f(SSpreadParams) \
	f(SShotgunParams) \
	f(SBurstParams) \
	f(SThrowParams) \
	f(SChargeParams) \
	f(SRapidParams) \
	f(SLTagGrenades) \
	f(SPlantParams) \
	f(SThermalVisionParams) \
	f(SFireModeAnimationParams) \
	f(SSpammerParams) \
	f(SBowParams) \
	f(SWeaponStatsData) \
	f(SAIDescriptor) \
	f(SHazardDescriptor) \

#endif //__ITEMPARAMSREGISTRATION_H__
