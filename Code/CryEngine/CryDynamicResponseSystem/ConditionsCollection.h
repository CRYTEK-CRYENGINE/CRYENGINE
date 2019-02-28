// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************

   A DynamicResponseConditionCollection is (like the name already tells) a collection of conditions, its a convenient way to group conditions and check all at once. Each ResponseSegment has exactly one of these

   /************************************************************************/

#pragma once

namespace CryDRS
{
class CResponseInstance;
class CConditionsCollection
{
public:
#if !defined(_RELEASE)
	static DRS::IResponseActor * s_pActorForEvaluation;
#endif
	struct SConditionInfo
	{
		SConditionInfo() : m_bNegated(false) {}
		bool                     m_bNegated;
		DRS::IConditionSharedPtr m_pCondition;    //todo: add OR conditions here, if needed
		void                     Serialize(Serialization::IArchive& ar);
	};

	~CConditionsCollection();
	CConditionsCollection() : m_bNegated(false) {}
	CConditionsCollection(CConditionsCollection&& other) : m_conditions(std::move(other.m_conditions)), m_bNegated(other.m_bNegated) {}
	CConditionsCollection&   operator=(CConditionsCollection&& other) { m_conditions = std::move(other.m_conditions); m_bNegated = other.m_bNegated; return *this; }
	void                     SetNegated(bool value)                   { m_bNegated = value; } //negates the whole collection.
	bool                     IsNegated()                              { return m_bNegated; }
	void                     AddCondition(DRS::IConditionSharedPtr pCondition, bool Negated);
	bool                     IsMet(CResponseInstance* pResponseInstance);
	uint32                   GetNumConditions() const      { return (uint32)(m_conditions.size()); }
	void                     Serialize(Serialization::IArchive& ar);
	DRS::IConditionSharedPtr operator[](int index)         { CRY_ASSERT(index < m_conditions.size()); return m_conditions[index].m_pCondition; }
	bool                     IsConditionNegated(int index) { CRY_ASSERT(index < m_conditions.size());  return m_conditions[index].m_bNegated; }

private:
	typedef std::vector<SConditionInfo> ConditionsList;
	ConditionsList m_conditions;
	bool           m_bNegated;
};

} // namespace CryDRS
