// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROGRESSRANGE_H__
#define __PROGRESSRANGE_H__

class ProgressRange
{
public:
	template <typename T> ProgressRange(T* object, void (T::*setter)(float progress))
		: m_target(new MethodTarget<T>(object, setter)),
			m_progress(0.0f),
			m_start(0.0f),
			m_scale(1.0f)
	{
		m_target->Set(m_start);
	}

	ProgressRange(ProgressRange& parent, float scale)
		: m_target(new ParentRangeTarget(parent)),
			m_progress(0.0f),
			m_start(parent.m_progress),
			m_scale(scale)
	{
			m_target->Set(m_start);
	}

	~ProgressRange()
	{
		m_target->Set(m_start + m_scale);
		delete m_target;
	}

	void SetProgress(float progress)
	{
		assert(progress > -0.01f && progress < 1.1f);
		m_progress = progress;
		m_target->Set(m_start + m_scale * progress);
	}

private:
	struct ITarget
	{
		virtual ~ITarget() {}
		virtual void Set(float progress) = 0;
	};

	struct ParentRangeTarget : public ITarget
	{
		ParentRangeTarget(ProgressRange& range): range(range) {}
		virtual void Set(float progress) {range.SetProgress(progress);}
		ProgressRange& range;
	};

	template <typename T> struct MethodTarget : public ITarget
	{
		typedef void (T::*Setter)(float progress);
		MethodTarget(T* object, Setter setter): object(object), setter(setter) {}
		virtual void Set(float progress) {(object->*setter)(progress);}
		T* object;
		Setter setter;
	};

	ITarget* m_target;
	float m_progress;
	float m_start;
	float m_scale;
};

#endif //__PROGRESSRANGE_H__
