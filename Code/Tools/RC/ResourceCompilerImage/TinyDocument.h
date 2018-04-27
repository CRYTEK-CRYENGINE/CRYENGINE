// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TINYDOCUMENT_H__
#define __TINYDOCUMENT_H__

#include <list>
#include <algorithm>
#include <functional>

template <typename V> class TinyDocumentBase
{
public:
	typedef V Value;

protected:
	Value value = 0; // Inheritors TinyDocument::SetValue compares the value before assign. The assignment may fail if the uninitialized value is NaN and /fp:fast.
};

template <typename T> class HasLimit
{
public:
	enum {Value = false};
};

template <> class HasLimit<float>
{
public:
	enum {Value = true};
};

template <typename V, bool L> class TinyDocumentLimited : public TinyDocumentBase<V>
{
};

template <typename V> class TinyDocumentLimited<V, true> : public TinyDocumentBase<V>
{
public:
	void SetMin(const V& min)
	{
		this->min = min;
	}

	V GetMin() const
	{
		return this->min;
	}

	void SetMax(const V& max)
	{
		this->max = max;
	}

	V GetMax() const
	{
		return this->max;
	}

private:
	V max;
	V min;
};

template <typename V> class TinyDocument : public TinyDocumentLimited<V, HasLimit<V>::Value >
{
public:
	void SetValue(const Value& value)
	{
		if(this->value==value)
			return;

		this->value = value;

		using namespace std;
		for_each(this->listeners.begin(), this->listeners.end(),
			bind2nd(mem_fun<void, Listener, TinyDocument*>(&Listener::OnTinyDocumentChanged), this));
	}

	Value GetValue() const
	{
		return this->value;
	}

	class Listener
	{
	public:
		virtual void OnTinyDocumentChanged(TinyDocument* pDocument) = 0;
	};

	void AddListener(Listener* pListener)
	{
		this->listeners.push_back(pListener);
	}

	void RemoveListener(Listener* pListener)
	{
		this->listeners.remove(pListener);
	}

private:
	std::list<Listener*> listeners;
};

#endif //__TINYDOCUMENT_H__
