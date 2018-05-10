// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SENSEDINCLUSIONSET_H__
#define __SENSEDINCLUSIONSET_H__

#pragma once

template<class T>
class SensedInclusionSet
{
public:
	SensedInclusionSet(bool sense) : m_sense(sense) {}

	void Set(const T& value, bool included)
	{
		if (included != m_sense)
			m_set.insert(value);
		else
			m_set.erase(value);
	}

	bool Get(const T& value) const
	{
		return (m_set.find(value) != m_set.end()) != m_sense;
	}

	void Reset(bool sense)
	{
		m_set.clear();
		m_sense = sense;
	}

	void GetMemoryStatistics(ICrySizer* s) const
	{
		s->AddObject(m_set);
	}

private:
	bool        m_sense;
	std::set<T> m_set;
};

#endif
