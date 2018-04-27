// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   TacticalPointQuery.h
   $Id$
   $DateTime$
   Description: Classes for constructing new TPS queries
   ---------------------------------------------------------------------
   History:
   - 30:07:2007 : Separated out by Matthew Jack

 *********************************************************************/

#pragma once

#ifndef __TacticalPointQuery_H__
	#define __TacticalPointQuery_H__

	#if _MSC_VER > 1000
		#pragma once
	#endif
	#include "TacticalPointQueryEnum.h"

class CCriterion
{
public:
	CCriterion();
	~CCriterion() {}

	void SetQuery(TTacticalPointQuery query)          // The Query, be it for Generation, Conditions or Weights
	{ m_eQuery = query; }
	void SetLimits(TTacticalPointQuery limits)        // The Limits flags, if any
	{ m_eLimits = limits; }
	void SetObject(TTacticalPointQuery object)
	{ m_eObject = object; }
	void SetObjectAux(TTacticalPointQuery objectAux)
	{ m_eObjectAux = objectAux; }
	void SetValue(float fVal)                       // Note that this overwrites any bool value
	{ m_fValue = fVal; }
	void SetValue(bool bVal)                          // Note that this overwrites any float value
	{ m_bValue = bVal; }
	void SetValue(ETPSRelativeValueSource nVal)     // Note that this DOES NOT overwrite any float or bool value
	{ m_nRelativeValueSource = nVal; }

	TTacticalPointQuery     GetQuery() const
	{ return m_eQuery; }
	TTacticalPointQuery     GetLimits() const
	{ return m_eLimits; }
	TTacticalPointQuery     GetObject() const
	{ return m_eObject; }
	TTacticalPointQuery     GetObjectAux() const
	{ return m_eObjectAux; }
	float                   GetValueAsFloat() const
	{ return m_fValue; }
	bool                    GetValueAsBool() const
	{ return m_bValue; }
	ETPSRelativeValueSource GetValueAsRelativeValueSource() const
	{ return m_nRelativeValueSource; }

	bool IsValid() const;                       // Checks that all settings (apart from value) make sense

private:

	TTacticalPointQuery m_eQuery;         // Just the query (unpacked for debugging purposes)
	TTacticalPointQuery m_eLimits;        // Just the limits (unpacked for debugging purposes)
	TTacticalPointQuery m_eObject;        // The Object of the Query, if any (unpacked for debugging purposes)
	TTacticalPointQuery m_eObjectAux;     // Auxiliary object used in some Generation queries (unpacked for debugging purposes)
	// E.g. Object we hide from for hidespot generation
	union
	{
		float m_fValue;
		bool  m_bValue;
	};
	ETPSRelativeValueSource m_nRelativeValueSource;
};

//---------------------------------------------------------------------------------------//

class COptionCriteria
{
public:
	struct SParameters
	{
		float  fDensity;
		float  fHeight;
		float  fHorizontalSpacing;
		int    iObjectsType;
		string sSignalToSend;
		string tagPointPostfix;
		string extenderStringParamenter;
		string sNavigationAgentType;

		SParameters()
			: fDensity(1.0f)
			, fHeight(0.0f)
			, fHorizontalSpacing(1.0f)
			, iObjectsType(-1)
		{
		}
	};

	COptionCriteria() {};
	~COptionCriteria() {};

	// Returns true iff parsed successfully
	bool AddToParameters(const char* sSpec, float fValue);
	bool AddToParameters(const char* sSpec, bool bValue);
	bool AddToParameters(const char* sSpec, const char* sValue);

	// Returns true iff parsed successfully
	bool AddToGeneration(const char* sSpec, float fValue);
	bool AddToGeneration(const char* sSpec, ETPSRelativeValueSource nSource);

	// fValue/bValue: value for comparison against result
	// Returns true iff parsed successfully
	bool AddToConditions(const char* sSpec, float fValue);
	bool AddToConditions(const char* sSpec, bool bValue);

	// fWeight: weight multiplier for the result of this query; can be negative
	// Returns true iff parsed successfully
	bool                           AddToWeights(const char* sSpec, float fWeight);

	const std::vector<CCriterion>& GetAllGeneration() const { return m_vGeneration; }
	const std::vector<CCriterion>& GetAllConditions() const { return m_vConditions; }
	const std::vector<CCriterion>& GetAllWeights() const    { return m_vWeights; }

	const SParameters*             GetParams(void) const    { return &m_params; }

	string                         GetDescription() const;

	bool                           IsValid() const;

private:
	CCriterion GetCriterion(const char* sSpec);

	SParameters             m_params;

	std::vector<CCriterion> m_vGeneration;
	std::vector<CCriterion> m_vConditions;
	std::vector<CCriterion> m_vWeights;
};

//---------------------------------------------------------------------------------------//

class CTacticalPointQuery
{
public:
	CTacticalPointQuery(const char* psName);
	~CTacticalPointQuery();

	// Adds one of the options for choosing a point
	// Each will be tried in the order they were added until a valid point is found
	void AddOption(const COptionCriteria& option);

	bool IsValid() const;

	// Get the name of this query
	const char* GetName() const { return m_sName.c_str(); }

	// NULL on option outside of range
	COptionCriteria*       GetOption(int i);
	// NULL on option outside of range
	const COptionCriteria* GetOption(int i) const;

private:
	std::vector<COptionCriteria> m_vOptions;
	string                       m_sName; // Also in map - why store twice? Storing it's ID might be better
};

#endif // __TacticalPointQuery_H__
