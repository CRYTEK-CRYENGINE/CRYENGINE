// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "JoystickCtrl.h"
#include "JoystickUtils.h"
#include <CryInput/IJoystick.h>
#include "Controls/MemDC.h"
#include "Dialogs/QStringDialog.h"
#include "IUndoManager.h"
#include <CryMath/ISplines.h>
#include "Dialogs/CustomColorDialog.h"
#include <CryAnimation/IFacialAnimation.h> // TODO: Remove!
#include "JoystickPropertiesDlg.h"
#include "Controls/SharedFonts.h"
#include "IUndoObject.h"

static const int headerBoxHeight = 15;
static const int knobSize = 10;
static const int knobMargin = 2;

class CUndoJoystickChannelChange : public IUndoObject
{
public:
	CUndoJoystickChannelChange(IJoystickUndoContext* pContext, IJoystickSet* pJoystickSet, IJoystick* pJoystick)
		: m_pContext(pContext),
		m_pJoystickSet(pJoystickSet)
	{
		if (pJoystick)
		{
			m_entries.push_back(JoystickEntry(pJoystick));
		}
		else
		{
			for (int joystickIndex = 0; m_pJoystickSet && joystickIndex < m_pJoystickSet->GetJoystickCount(); ++joystickIndex)
			{
				IJoystick* pJoystick = (m_pJoystickSet ? m_pJoystickSet->GetJoystick(joystickIndex) : 0);
				if (pJoystick)
					m_entries.push_back(JoystickEntry(pJoystick));
			}
		}
	}

	// IUndoObject
	virtual int GetSize()
	{
		return sizeof(*this);
	}

	virtual const char* GetDescription()
	{
		return "JoystickCtrl";
	}

	virtual void Undo(bool bUndo)
	{
		for (JoystickEntryContainer::iterator joystickEntry = m_entries.begin(); joystickEntry != m_entries.end(); ++joystickEntry)
			(*joystickEntry).Undo(bUndo, m_pJoystickSet);
		if (m_pContext && bUndo)
			m_pContext->OnSplineChangesUnOrReDone();
	}

	virtual void Redo()
	{
		for (JoystickEntryContainer::iterator joystickEntry = m_entries.begin(); joystickEntry != m_entries.end(); ++joystickEntry)
			(*joystickEntry).Redo(m_pJoystickSet);
		if (m_pContext)
			m_pContext->OnSplineChangesUnOrReDone();
	}

private:
	class State
	{
	public:
		void Save(IJoystick* pJoystick)
		{
			for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
			{
				XmlNodeRef node = XmlHelpers::CreateXmlNode("Channel");
				IJoystickChannel* pChannel = pJoystick ? pJoystick->GetChannel(axis) : 0;
				if (pChannel && node)
					JoystickUtils::Serialize(pChannel, node, false);
				m_nodes[axis] = node;
			}
		}

		void Load(IJoystick* pJoystick)
		{
			for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
			{
				IJoystickChannel* pChannel = pJoystick ? pJoystick->GetChannel(axis) : 0;
				if (pChannel && m_nodes[axis])
					JoystickUtils::Serialize(pChannel, m_nodes[axis], true);
			}
		}

	private:
		XmlNodeRef m_nodes[2];
	};

	class JoystickEntry
	{
	public:
		JoystickEntry(IJoystick* pJoystick) : m_id(pJoystick->GetID())
		{
			m_undo.Save(pJoystick);
		}

		virtual void Undo(bool bUndo, IJoystickSet* pJoystickSet)
		{
			IJoystick* pJoystick = (pJoystickSet && m_id ? pJoystickSet->GetJoystickByID(m_id) : 0);
			if (bUndo && pJoystick)
				m_redo.Save(pJoystick);
			if (pJoystick)
				m_undo.Load(pJoystick);
		}

		virtual void Redo(IJoystickSet* pJoystickSet)
		{
			IJoystick* pJoystick = (pJoystickSet && m_id ? pJoystickSet->GetJoystickByID(m_id) : 0);
			if (pJoystick)
				m_redo.Load(pJoystick);
		}

	private:
		uint64 m_id;
		State  m_undo;
		State  m_redo;
	};

	typedef std::vector<JoystickEntry> JoystickEntryContainer;

	IJoystickUndoContext*  m_pContext;
	IJoystickSet*          m_pJoystickSet;
	JoystickEntryContainer m_entries;
};

class CUndoJoystickChange : public IUndoObject
{
public:
	CUndoJoystickChange(IJoystickUndoContext* pContext, IJoystickSet* pJoystickSet)
		: m_pContext(pContext),
		m_pJoystickSet(pJoystickSet)
	{
		m_undo = XmlHelpers::CreateXmlNode("undo");
		m_redo = XmlHelpers::CreateXmlNode("redo");
		if (m_pJoystickSet && m_pContext && m_undo)
			m_pContext->SerializeJoystickSet(m_pJoystickSet, m_undo, false);
	}
	// IUndoObject
	virtual int GetSize()
	{
		return sizeof(*this);
	}

	virtual const char* GetDescription()
	{
		return "JoystickCtrl";
	}

	virtual void Undo(bool bUndo)
	{
		if (bUndo && m_pJoystickSet && m_pContext && m_redo)
			m_pContext->SerializeJoystickSet(m_pJoystickSet, m_redo, false);
		if (m_pJoystickSet && m_pContext && m_undo)
			m_pContext->SerializeJoystickSet(m_pJoystickSet, m_undo, true);
		if (m_pContext && bUndo)
			m_pContext->OnJoytickChangesUnOrReDone();
	}

	virtual void Redo()
	{
		if (m_pJoystickSet && m_redo && m_pContext)
			m_pContext->SerializeJoystickSet(m_pJoystickSet, m_redo, true);
		if (m_pContext)
			m_pContext->OnJoytickChangesUnOrReDone();
	}

private:
	IJoystickUndoContext* m_pContext;
	XmlNodeRef            m_undo;
	XmlNodeRef            m_redo;
	IJoystickSet*         m_pJoystickSet;
};

class IJoystickActionMode : public _reference_target_t
{
public:
	virtual void OnClick(const Vec2& vPosition, bool controlHeld, IJoystickActionContext* pContext) {}
	virtual void OnStartDragging(const Vec2& vPosition, IJoystickActionContext* pContext)           {}
	virtual void OnDragging(const Vec2& vPosition, IJoystickActionContext* pContext)                {}
	virtual void OnEndDragging(const Vec2& vPosition, IJoystickActionContext* pContext)             {}
	virtual bool RequiresPreview() const                                                            { return false; }
	virtual bool AllowsClickDragging() const                                                        { return false; }
	virtual void OnUpdate(const Vec2& vPosition, IJoystickActionContext* pContext)                  {}
};

class CAxisJoystickActionMode : public IJoystickActionMode
{
public:
	CAxisJoystickActionMode(IJoystick* pJoystick, IJoystick::ChannelType axis, IJoystickCtrlContainer* pContainer)
		: m_pJoystick(pJoystick),
		m_axis(axis),
		m_pContainer(pContainer)
	{
	}

	void OnClick(const Vec2& vPosition, bool controlHeld, IJoystickActionContext* pContext)
	{
		float sequenceTime = (m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0.0f);
		IJoystickChannel* pChannel = (m_pJoystick ? m_pJoystick->GetChannel(m_axis) : 0);

		if (pChannel && m_pJoystick && !controlHeld && !JoystickUtils::HasKey(pChannel, sequenceTime))
		{
			pContext->BeginUndo();
			pContext->StoreSplineUndo(m_pJoystick);

			float currentValue = (pChannel ? JoystickUtils::Evaluate(pChannel, sequenceTime) : 0);
			if (pChannel && pChannel->GetFlipped())
				currentValue *= -1.0f;
			JoystickUtils::SetKey(pChannel, FacialEditorSnapTimeToFrame(sequenceTime), currentValue);

			pContext->AcceptUndo("Add Key");
			if (m_pContainer)
				m_pContainer->OnSplineChanged();
		}
		else if (pChannel && controlHeld && JoystickUtils::HasKey(pChannel, sequenceTime))
		{
			pContext->BeginUndo();
			pContext->StoreSplineUndo(m_pJoystick);
			JoystickUtils::RemoveKey(pChannel, sequenceTime);
			pContext->AcceptUndo("Remove Key");

			if (m_pContainer)
				m_pContainer->OnSplineChanged();
		}
	}

private:
	IJoystick*              m_pJoystick;
	IJoystick::ChannelType  m_axis;
	IJoystickCtrlContainer* m_pContainer;
};

class CSelectJoystickActionMode : public IJoystickActionMode
{
public:
	CSelectJoystickActionMode(IJoystick* pJoystick, IJoystickCtrlContainer* pContainer)
		: m_pJoystick(pJoystick),
		m_pContainer(pContainer)
	{
	}

	void OnClick(const Vec2& vPosition, bool controlHeld, IJoystickActionContext* pContext)
	{
		if (m_pContainer)
			m_pContainer->OnJoystickSelected(m_pJoystick, true);
	}

private:
	IJoystick*              m_pJoystick;
	IJoystickCtrlContainer* m_pContainer;
};

class DragJoystickActionMode : public IJoystickActionMode
{
public:
	DragJoystickActionMode(IJoystickSet* pJoystickSet, IJoystick* pJoystick, float snapMargin, IJoystickCtrlContainer* pContainer)
		: m_pJoystickSet(pJoystickSet),
		m_id(pJoystick->GetID()),
		m_snapMargin(snapMargin),
		m_pContainer(pContainer)
	{
	}

	virtual void OnStartDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		if (pContext)
			pContext->BeginUndo();

		IJoystick* pJoystick = (m_pJoystickSet ? m_pJoystickSet->GetJoystickByID(m_id) : 0);
		m_vFirstPosition = vPosition;
		m_vOriginalCentre = pJoystick ? pJoystick->GetCentre() : Vec2(0, 0);
	}

	virtual void OnDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		if (pContext)
		{
			pContext->RestoreUndo();
			pContext->StoreJoystickUndo();
		}

		IJoystick* pJoystick = (m_pJoystickSet ? m_pJoystickSet->GetJoystickByID(m_id) : 0);

		Vec2 newPosition = m_vOriginalCentre + vPosition - m_vFirstPosition;
		Vec2 dimensions(pJoystick ? pJoystick->GetDimensions() : Vec2(0, 0));
		Vec2 newTopLeft = newPosition - dimensions;

		if (newTopLeft.x < 0)
			newTopLeft.x = 0;

		if (newTopLeft.y < 0)
			newTopLeft.y = 0;

#define SNAP(v, s) (floorf((v) / (s) + 0.5f) * s)
		Vec2 vSnappedNewTopLeft(SNAP(newTopLeft.x, m_snapMargin), SNAP(newTopLeft.y, m_snapMargin));
#undef SNAP

		Vec2 vSnappedNewPosition = vSnappedNewTopLeft + dimensions;

		if (pJoystick)
			pJoystick->SetCentre(vSnappedNewPosition);

		if (m_pContainer)
			m_pContainer->OnJoysticksChanged();

		if (pContext)
			pContext->JoysticksChanged();
	}

	virtual void OnEndDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		if (pContext)
			pContext->AcceptUndo("Move joystick");
		if (m_pContainer)
			m_pContainer->OnJoysticksChanged();
	}

private:
	IJoystickSet*           m_pJoystickSet;
	uint64                  m_id;
	Vec2                    m_vFirstPosition;
	Vec2                    m_vOriginalCentre;
	float                   m_snapMargin;
	IJoystickCtrlContainer* m_pContainer;
};

class DragKnobActionMode : public IJoystickActionMode
{
public:
	DragKnobActionMode(IJoystick* pJoystick, IJoystickCtrlContainer* pContainer, bool bAutoCreateKey)
		: m_pJoystick(pJoystick),
		m_pContainer(pContainer),
		m_bAutoCreateKey(bAutoCreateKey),
		m_hasPlayed(false),
		m_timeWrappedToBeginning(false)
	{
		m_lastTime = (m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0.0f);
	}

	virtual void OnStartDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		pContext->SetJoystickBeingManipulated(m_pJoystick);

		// Must be before BeginUndo, otherwise the spline control adds its own undo (since it assumes
		// that if undo is being recorded, it should store undo, even if nothing is happening). This
		// makes for very weird problems.
		if (m_pContainer && m_pJoystick)
			m_pContainer->OnBeginDraggingJoystickKnob(m_pJoystick);

		if (pContext)
			pContext->BeginUndo();
		if (pContext && m_pJoystick)
			pContext->StoreSplineUndo(m_pJoystick);

		float sequenceTime = m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0.0f;

		// Get the knob position.
		Vec2 vChannelValues(0, 0);
		for (int channelIndex = 0; channelIndex < 2; ++channelIndex)
		{
			IJoystickChannel* pChannel = m_pJoystick ? m_pJoystick->GetChannel((IJoystick::ChannelType)channelIndex) : 0;
			vChannelValues[channelIndex] = pChannel ? JoystickUtils::Evaluate(pChannel, sequenceTime) : 0.0f;
			if (pChannel && pChannel->GetFlipped())
				vChannelValues[channelIndex] *= -1.0f;
		}

		Vec2 vClampedChannelValues = vChannelValues;
		vClampedChannelValues.x = min(max(vClampedChannelValues.x, -1.0f), 1.0f);
		vClampedChannelValues.y = min(max(vClampedChannelValues.y, -1.0f), 1.0f);

		m_vOriginalCentre = vChannelValues;

		// Convert vPosition into knob coordinates.
		Vec2 vKnobPosition = GetKnobPosition(vPosition);

		m_vFirstPosition = vKnobPosition;

		UpdateValues(m_lastValues, vPosition);
	}

	virtual void OnDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		bool playing = (m_pContainer ? m_pContainer->GetPlaying() : false);

		pContext->SetJoystickBeingManipulatedPosition(GetKnobPosition(vPosition));

		if (!playing)
		{
			Vec2 vKnobPosition = GetKnobPosition(vPosition);

			float sequenceTime = m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0.0f;

			for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
			{
				IJoystickChannel* pChannel = m_pJoystick ? m_pJoystick->GetChannel(axis) : 0;

				float fNewValue = m_vOriginalCentre[axis] + vKnobPosition[axis] - m_vFirstPosition[axis];
				if (pChannel && pChannel->GetFlipped())
					fNewValue *= -1.0f;

				JoystickUtils::SetKey(pChannel, sequenceTime, fNewValue);
			}

			if (m_pContainer)
				m_pContainer->OnSplineChanged();
		}
	}

	virtual void OnEndDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		pContext->SetJoystickBeingManipulated(0);

		if (m_hasPlayed)
		{
			for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
			{
				IJoystickChannel* pChannel = m_pJoystick ? m_pJoystick->GetChannel(axis) : 0;
				if (pChannel)
					pChannel->CleanupKeys(0.1f);
			}

			if (m_pContainer)
				m_pContainer->OnSplineChanged();
		}

		if (pContext)
			pContext->AcceptUndo("Move joystick knob");
	}

	virtual bool RequiresPreview() const
	{
		return true;
	}

	virtual void OnUpdate(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		float currentTime = (m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0.0f);
		if (currentTime < m_lastTime)
		{
			m_timeWrappedToBeginning = true;
			if (pContext)
			{
				for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
				{
					IJoystickChannel* pChannel = m_pJoystick ? m_pJoystick->GetChannel(axis) : 0;
					if (pChannel)
						pChannel->CleanupKeys(0.1f);
				}

				if (m_pContainer)
					m_pContainer->OnSplineChanged();

				pContext->TerminateActionMode();
				pContext->ReleaseMouseCapture();
				pContext->SetJoystickBeingManipulated(0);
				pContext->AcceptUndo("Record mode");
				return;
			}
		}

		bool playing = (m_pContainer ? m_pContainer->GetPlaying() : false);
		if (playing && !m_timeWrappedToBeginning)
		{
			const float interval = 1.0f / FACIAL_EDITOR_FPS;

			for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
			{
				IJoystickChannel* pChannel = m_pJoystick ? m_pJoystick->GetChannel(axis) : 0;
				JoystickUtils::RemoveKeysInRange(pChannel, m_lastTime, currentTime + interval);
			}

			float currentInterval = floorf((currentTime + interval) / interval);

			float knobValues[2];
			UpdateValues(knobValues, vPosition);

			float timeDelta = currentTime - m_lastTime;
			float timeDeltaReciprocal = (timeDelta > 0.001f ? 1.0f / timeDelta : 0.0f);

			for (float lastInterval = floorf(m_lastTime / interval); lastInterval < currentInterval; ++lastInterval)
			{
				m_hasPlayed = true;

				float newKeyTime = (lastInterval + 1) * interval;

				for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
				{
					IJoystickChannel* pChannel = m_pJoystick ? m_pJoystick->GetChannel(axis) : 0;

					float newValue = m_lastValues[axis] + (knobValues[axis] - m_lastValues[axis]) * (newKeyTime - m_lastTime) * timeDeltaReciprocal;

					JoystickUtils::SetKey(pChannel, newKeyTime, newValue);
				}
			}

			UpdateValues(m_lastValues, vPosition);

			if (m_pContainer)
				m_pContainer->OnSplineChanged();
		}

		m_lastTime = currentTime;
	}

	virtual bool AllowsClickDragging() const
	{
		return true;
	}

private:
	Vec2 GetKnobPosition(const Vec2& vPosition)
	{
		Vec2 vCentre(m_pJoystick ? m_pJoystick->GetCentre() : Vec2(0, 0));
		Vec2 vDimensions(m_pJoystick ? m_pJoystick->GetDimensions() : Vec2(0, 0));
		Vec2 vBoxCentre = vCentre + Vec2(0, headerBoxHeight * 0.5);
		Vec2 vBoxDimensions = vDimensions - Vec2(0, headerBoxHeight * 0.5) - Vec2(knobSize + knobMargin, knobSize + knobMargin);
		vBoxDimensions = Vec2(max(vBoxDimensions.x, 0.0f), max(vBoxDimensions.y, 0.0f));
		Vec2 vKnobPosition = vPosition - vBoxCentre;
		vKnobPosition = Vec2(vKnobPosition.x / vBoxDimensions.x, -vKnobPosition.y / vBoxDimensions.y);
		vKnobPosition = Vec2(min(max(vKnobPosition.x, -1.0f), 1.0f), min(max(vKnobPosition.y, -1.0f), 1.0f));
		return vKnobPosition;
	}

	void UpdateValues(float values[2], const Vec2& vPosition)
	{
		Vec2 vKnobPosition = GetKnobPosition(vPosition);

		for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
		{
			IJoystickChannel* pChannel = m_pJoystick ? m_pJoystick->GetChannel(axis) : 0;

			float fNewValue = m_vOriginalCentre[axis] + vKnobPosition[axis] - m_vFirstPosition[axis];
			if (pChannel && pChannel->GetFlipped())
				fNewValue *= -1.0f;
			values[axis] = fNewValue;
		}
	}

	IJoystick*              m_pJoystick;
	IJoystickCtrlContainer* m_pContainer;
	Vec2                    m_vFirstPosition;
	Vec2                    m_vOriginalCentre;
	bool                    m_bAutoCreateKey;
	float                   m_lastTime;
	bool                    m_hasPlayed;
	bool                    m_timeWrappedToBeginning;
	float                   m_lastValues[2];
};

class ResizeJoystickActionMode : public IJoystickActionMode
{
public:
	ResizeJoystickActionMode(IJoystickSet* pJoystickSet, IJoystick* pJoystick, const Vec2& vScale, float snapMargin, float minSize, IJoystickCtrlContainer* pContainer)
		: m_pJoystickSet(pJoystickSet),
		m_id(pJoystick->GetID()),
		m_vScale(vScale),
		m_snapMargin(snapMargin),
		m_minSize(minSize),
		m_pContainer(pContainer)
	{
	}

	virtual void OnStartDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		if (pContext)
			pContext->BeginUndo();

		IJoystick* pJoystick = (m_pJoystickSet ? m_pJoystickSet->GetJoystickByID(m_id) : 0);

		m_vFirstPosition = vPosition;
		m_vFirstCentre = pJoystick ? pJoystick->GetCentre() : Vec2(0, 0);
		m_vFirstDimensions = pJoystick ? pJoystick->GetDimensions() : Vec2(0, 0);
	}

	virtual void OnDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		if (pContext)
		{
			pContext->RestoreUndo();
			pContext->StoreJoystickUndo();
		}

		IJoystick* pJoystick = (m_pJoystickSet ? m_pJoystickSet->GetJoystickByID(m_id) : 0);

		Vec2 vAbsScale(fabsf(m_vScale.x), fabsf(m_vScale.y));
		Vec2 vOffset = vPosition - m_vFirstPosition;
		Vec2 vScaledOffset(vOffset.x * m_vScale.x, vOffset.y * m_vScale.y);
		Vec2 vAbsScaledOffset(vOffset.x * vAbsScale.x, vOffset.y * vAbsScale.y);
		Vec2 vNewCentre(m_vFirstCentre + 0.5f * vAbsScaledOffset);
		Vec2 vNewDimensions(m_vFirstDimensions + 0.5f * vScaledOffset);

#define SNAP(v, s) (floorf((v) / (s) + 0.5f) * s)
		float dimensionSnapMargin = m_snapMargin * 0.5;
		Vec2 vAbsNewDimensions(fabsf(vNewDimensions.x), fabsf(vNewDimensions.y));
		Vec2 vSnappedAbsNewDimensions(SNAP(vAbsNewDimensions.x, dimensionSnapMargin), SNAP(vAbsNewDimensions.y, dimensionSnapMargin));
		while (vSnappedAbsNewDimensions.x < m_minSize)
			vSnappedAbsNewDimensions.x += dimensionSnapMargin;
		while (vSnappedAbsNewDimensions.y < m_minSize)
			vSnappedAbsNewDimensions.y += dimensionSnapMargin;
		Vec2 vDimensionSnapDistance = vSnappedAbsNewDimensions - vAbsNewDimensions;
#undef SNAP

		Vec2 vCentreSnapAdjustment(m_vScale.x * vDimensionSnapDistance.x * sgn(vNewDimensions.x), m_vScale.y * vDimensionSnapDistance.y * sgn(vNewDimensions.y));
		vNewCentre += vCentreSnapAdjustment;

		float fXPos = vNewCentre.x - vSnappedAbsNewDimensions.x;
		float fYPos = vNewCentre.y - vSnappedAbsNewDimensions.y;

		if ((fXPos < 0 && vOffset.x < 0) || (fYPos < 0 && vOffset.y < 0))
		{
			if (fXPos < 0 && vOffset.x < 0)
				vSnappedAbsNewDimensions.x += fXPos;

			if (fYPos < 0 && vOffset.y < 0)
				vSnappedAbsNewDimensions.y += fYPos;

			if (m_pContainer)
				m_pContainer->OnJoysticksChanged();

			if (pContext)
				pContext->JoysticksChanged();

			return;
		}
		;

		if (pJoystick)
		{
			pJoystick->SetCentre(vNewCentre);
			pJoystick->SetDimensions(vSnappedAbsNewDimensions);
		}

		if (m_pContainer)
			m_pContainer->OnJoysticksChanged();

		if (pContext)
			pContext->JoysticksChanged();
	}

	virtual void OnEndDragging(const Vec2& vPosition, IJoystickActionContext* pContext)
	{
		if (pContext)
			pContext->AcceptUndo("Move joystick knob");

		if (m_pContainer)
			m_pContainer->OnJoysticksChanged();
	}

private:
	IJoystickSet*           m_pJoystickSet;
	uint64                  m_id;
	Vec2                    m_vScale;
	Vec2                    m_vFirstPosition;
	Vec2                    m_vFirstCentre;
	Vec2                    m_vFirstDimensions;
	float                   m_snapMargin;
	float                   m_minSize;
	IJoystickCtrlContainer* m_pContainer;
};

class JoystickMiddleMouseButtonHandler
{
public:
	JoystickMiddleMouseButtonHandler()
		: m_bTracking(false)
	{
	}

	void OnMiddleMouseDown(const Vec2& mousePosition, IJoystickMiddleMouseButtonHandlerContext* pContext)
	{
		m_lastPosition = mousePosition;
		m_bTracking = true;
	}

	void OnMiddleMouseUp(const Vec2& mousePosition, IJoystickMiddleMouseButtonHandlerContext* pContext)
	{
		m_bTracking = false;
	}

	bool OnMouseMove(const Vec2& mousePosition, IJoystickMiddleMouseButtonHandlerContext* pContext)
	{
		if (m_bTracking)
		{
			if (pContext)
				pContext->SetPosition(pContext->GetPosition() - mousePosition + m_lastPosition);
			m_lastPosition = mousePosition;
		}

		return m_bTracking;
	}

private:
	Vec2 m_lastPosition;
	bool m_bTracking;
};

IMPLEMENT_DYNAMIC(CJoystickCtrl, CScrollableWindow)

BEGIN_MESSAGE_MAP(CJoystickCtrl, CScrollableWindow)
ON_WM_PAINT()
ON_WM_SIZE()
ON_WM_CONTEXTMENU()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MBUTTONDOWN()
ON_WM_MBUTTONUP()
ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

CJoystickCtrl::CJoystickCtrl()
	: m_pJoystickContext(0),
	m_bFreezeLayout(true),
	m_pContainer(0),
	m_pActionMode(0),
	m_snapMargin(10),
	m_minSize(2 * m_snapMargin),
	m_bAutoCreateKey(false),
	m_pJoystickBeingManipulated(0)
{
	m_pMiddleMouseButtonHandler = new JoystickMiddleMouseButtonHandler();
}

CJoystickCtrl::~CJoystickCtrl()
{
}

BOOL CJoystickCtrl::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
	return CreateEx(0, lpClassName, "JoystickCtrl", dwStyle, rc, pParentWnd, nID);
}

void CJoystickCtrl::SetJoystickSet(IJoystickSet* pJoysticks)
{
	m_pJoysticks = pJoysticks;
	UpdateDocumentRect();
	Redraw();
}

void CJoystickCtrl::SetJoystickContext(IJoystickContext* pJoystickContext)
{
	m_pJoystickContext = pJoystickContext;
}

void CJoystickCtrl::OnPaint()
{
	CPaintDC PaintDC(this);

	Render(PaintDC);
}

void CJoystickCtrl::UpdatePreview()
{
	if (!m_pActionMode)
		Redraw();
}

void CJoystickCtrl::Redraw()
{
	CDC* dc = GetDC();
	Render(*dc);
	ReleaseDC(dc);
}

void CJoystickCtrl::KeyAll()
{
	GetIEditor()->GetIUndoManager()->Begin();
	StoreSplineUndo(0);

	float currentTime = (m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0.0f);
	for (int joystickIndex = 0; m_pJoysticks && joystickIndex < m_pJoysticks->GetJoystickCount(); ++joystickIndex)
	{
		IJoystick* pJoystick = (m_pJoysticks ? m_pJoysticks->GetJoystick(joystickIndex) : 0);
		for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
		{
			IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
			JoystickUtils::PlaceKey(pChannel, currentTime);
		}
	}

	GetIEditor()->GetIUndoManager()->Accept("Key All");
	if (m_pContainer)
		m_pContainer->OnSplineChanged();
	Redraw();
}

void CJoystickCtrl::ZeroAll()
{
	GetIEditor()->GetIUndoManager()->Begin();
	StoreSplineUndo(0);

	float currentTime = (m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0.0f);
	for (int joystickIndex = 0; m_pJoysticks && joystickIndex < m_pJoysticks->GetJoystickCount(); ++joystickIndex)
	{
		IJoystick* pJoystick = (m_pJoysticks ? m_pJoysticks->GetJoystick(joystickIndex) : 0);
		for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
		{
			IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
			JoystickUtils::SetKey(pChannel, currentTime, 0.0f, false);
		}
	}

	GetIEditor()->GetIUndoManager()->Accept("Zero All");
	if (m_pContainer)
		m_pContainer->OnSplineChanged();
	Redraw();
}

bool CJoystickCtrl::HitTestPoint(IJoystick*& pHitJoystick, IJoystick::ChannelType& hitAxis, CPoint point)
{
	Vec2 position(point.x, point.y);
	bool hit = false;

	for (int joystickIndex = 0; m_pJoysticks && !hit && joystickIndex < m_pJoysticks->GetJoystickCount(); ++joystickIndex)
	{
		IJoystick* pJoystick = m_pJoysticks ? m_pJoysticks->GetJoystick(joystickIndex) : 0;
		Vec2 vDimensions(pJoystick ? pJoystick->GetDimensions() : Vec2(0, 0));
		Vec2 vCentre(pJoystick ? pJoystick->GetCentre() : Vec2(0, 0));
		Vec2 vOffset(position - vCentre);
		Vec2 vAbsOffset(fabsf(vOffset.x), fabsf(vOffset.y));

		Vec2 vBoxCentre = vCentre + Vec2(0, headerBoxHeight * 0.5);
		Vec2 vBoxDimensions = vDimensions - Vec2(0, headerBoxHeight * 0.5) - Vec2(knobSize + knobMargin, knobSize + knobMargin);
		vBoxDimensions = Vec2(max(vBoxDimensions.x, 0.0f), max(vBoxDimensions.y, 0.0f));

		Vec2 vAxisCentres[] = { Vec2(0, 0), Vec2(0, 0) };
		Vec2 vAxisDimensions[] = { Vec2(1, 0.2f), Vec2(0.2f, 1) };
		for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); !hit && axis < 2; axis = IJoystick::ChannelType(axis + 1))
		{
			Vec2 vAxisCentre(vAxisCentres[axis].x * vBoxDimensions.x + vBoxCentre.x, vAxisCentres[axis].y * vBoxDimensions.y + vBoxCentre.y);
			Vec2 vAxisDimensions(vAxisDimensions[axis].x * vBoxDimensions.x, vAxisDimensions[axis].y * vBoxDimensions.y);
			Vec2 vAbsOffset(fabsf(position.x - vAxisCentre.x), fabsf(position.y - vAxisCentre.y));
			if (pJoystick && vAbsOffset.x < vAxisDimensions.x && vAbsOffset.y < vAxisDimensions.y)
			{
				pHitJoystick = pJoystick;
				hitAxis = axis;
				hit = new CAxisJoystickActionMode(pJoystick, axis, m_pContainer);
			}
		}
	}

	return hit;
}

void CJoystickCtrl::Update()
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	Vec2 vCameraPosition = Vec2(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
	Vec2 position = Vec2(point.x, point.y) + vCameraPosition;

	if (m_pActionMode)
	{
		m_pActionMode->OnUpdate(position, this);
	}
}

void CJoystickCtrl::Render(CDC& dc)
{
	//OutputDebugString("Joystick Render\n");

	Vec2 vCameraPosition = Vec2(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
	float sequenceTime = m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0;

	CRect clientRect;
	GetClientRect(&clientRect);

	if (!m_buffer.GetSafeHandle())
		m_buffer.CreateCompatibleBitmap(&dc, clientRect.Width(), clientRect.Height());

	CMemoryDC bufferDC(&dc, &m_buffer);
	CBrush backgroundBrush(RGB(0xA0, 0xA0, 0xA0));
	bufferDC.FillRect(&clientRect, &backgroundBrush);

	for (int joystickIndex = 0; m_pJoysticks && joystickIndex < m_pJoysticks->GetJoystickCount(); ++joystickIndex)
	{
		IJoystick* pJoystick = m_pJoysticks ? m_pJoysticks->GetJoystick(joystickIndex) : 0;

		// Calculate the overall bounds of the joystick.
		Vec2 vCentre((pJoystick ? pJoystick->GetCentre() : Vec2(0, 0)) - vCameraPosition);
		Vec2 vDimensions(pJoystick ? pJoystick->GetDimensions() : Vec2(0, 0));
		Vec2 vTopLeft(vCentre - vDimensions);
		Vec2 vBottomRight(vCentre + vDimensions);
		CRect rect(vTopLeft.x, vTopLeft.y, vBottomRight.x, vBottomRight.y);

		// Render the frame.
		CBrush fillBrush(RGB(0x80, 0x80, 0x80));
		bufferDC.FillRect(&rect, &fillBrush);

		CBrush frameBrush(RGB(101, 74, 50));
		bufferDC.FrameRect(&rect, &frameBrush);

		// Render the heading.
		IJoystick::Color joystickColour = (pJoystick ? pJoystick->GetColor() : ZERO);
		CBrush headerBoxFillBrush(RGB(joystickColour.x, joystickColour.y, joystickColour.z));
		CRect headerBoxRect(vTopLeft.x, vTopLeft.y, vBottomRight.x, vTopLeft.y + headerBoxHeight);
		bufferDC.FillRect(&headerBoxRect, &headerBoxFillBrush);
		bufferDC.FrameRect(&headerBoxRect, &frameBrush);

		{
			// Choose whether to display the heading text in black or white.
			unsigned luminance = (joystickColour.x + joystickColour.y + joystickColour.z) / 3;
			unsigned headingColour = (luminance > 128 ? RGB(0, 0, 0) : RGB(255, 255, 255));

			// Display the heading text.
			RenderString(bufferDC, (pJoystick ? pJoystick->GetName() : ""), vCentre - vDimensions, vDimensions.x * 2, headingColour);
		}

		// Render some shaded boxes to show the range of the joystick.
		Vec2 vBoxCentre = vCentre + Vec2(0, headerBoxHeight * 0.5);
		Vec2 vBoxDimensions = vDimensions - Vec2(0, headerBoxHeight * 0.5) - Vec2(knobSize + knobMargin, knobSize + knobMargin);
		vBoxDimensions = Vec2(max(vBoxDimensions.x, 0.0f), max(vBoxDimensions.y, 0.0f));

		enum {NUM_REGIONS = 2};
		float regionSizes[NUM_REGIONS] = { 1.0, 0.5 };
		COLORREF regionColours[NUM_REGIONS] = { RGB(0xA0, 0xA0, 0xA0), RGB(0xB0, 0xB0, 0xB0) };
		for (int regionIndex = 0; regionIndex < NUM_REGIONS; ++regionIndex)
		{
			CBrush brush(regionColours[regionIndex]);
			Vec2 topLeft = vBoxCentre - regionSizes[regionIndex] * vBoxDimensions;
			Vec2 bottomRight = vBoxCentre + regionSizes[regionIndex] * vBoxDimensions;
			CRect rect(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
			bufferDC.FillRect(&rect, &brush);
		}

		// Render a crosshair. Use colour coding to indicate when we are positioned on a key.
		class LineEntry
		{
		public:
			LineEntry(const Vec2& vStart, const Vec2& vEnd, unsigned axisFlags) : vStart(vStart), vEnd(vEnd), axisFlags(axisFlags) {}
			const Vec2 vStart;
			const Vec2 vEnd;
			unsigned   axisFlags;
		};
		const LineEntry lineDefs[] = {
			LineEntry(Vec2(-1,    0),     Vec2(1,     0),     1),
			LineEntry(Vec2(0,     -1),    Vec2(0,     1),     2),
			LineEntry(Vec2(-0.2f, 1),     Vec2(0.2f,  1),     2),
			LineEntry(Vec2(-0.2f, -1),    Vec2(0.2f,  -1),    2),
			LineEntry(Vec2(1,     -0.2f), Vec2(1,     0.2f),  1),
			LineEntry(Vec2(-1,    -0.2f), Vec2(-1,    0.2f),  1),
			LineEntry(Vec2(-0.1f, 0.5f),  Vec2(0.1f,  0.5f),  2),
			LineEntry(Vec2(-0.1f, -0.5f), Vec2(0.1f,  -0.5f), 2),
			LineEntry(Vec2(0.5f,  -0.1f), Vec2(0.5f,  0.1f),  1),
			LineEntry(Vec2(-0.5f, -0.1f), Vec2(-0.5f, 0.1f),  1),
			LineEntry(Vec2(-1,    1),     Vec2(-0.8f, 1),     0),
			LineEntry(Vec2(-1,    -1),    Vec2(-0.8f, -1),    0),
			LineEntry(Vec2(1,     1),     Vec2(0.8f,  1),     0),
			LineEntry(Vec2(1,     -1),    Vec2(0.8f,  -1),    0),
			LineEntry(Vec2(-1,    1),     Vec2(-1,    0.8f),  0),
			LineEntry(Vec2(1,     1),     Vec2(1,     0.8f),  0),
			LineEntry(Vec2(-1,    -1),    Vec2(-1,    -0.8f), 0),
			LineEntry(Vec2(1,     -1),    Vec2(1,     -0.8f), 0)
		};
		unsigned keyFoundFlags = 0;
		for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = (IJoystick::ChannelType)(axis + 1))
		{
			IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
			keyFoundFlags |= (pChannel && JoystickUtils::HasKey(pChannel, sequenceTime) ? (1 << axis) : 0);
		}
		enum {NUM_LINE_DEFS = sizeof(lineDefs) / sizeof(lineDefs[0])};
		CPen crosshairPenNormal(PS_SOLID, 0, RGB(0x00, 0x00, 0x00));
		CPen crosshairPenWithKey(PS_SOLID, 0, RGB(0xFF, 0x00, 0x00));
		CPen* crosshairPens[2] = { &crosshairPenNormal, &crosshairPenWithKey };
		for (int pass = 0; pass < 2; ++pass)
		{
			bufferDC.SelectObject(crosshairPens[pass]);
			for (int lineDefIndex = 0; lineDefIndex < NUM_LINE_DEFS; ++lineDefIndex)
			{
				if ((lineDefs[lineDefIndex].axisFlags & keyFoundFlags) && pass == 1 || !(lineDefs[lineDefIndex].axisFlags & keyFoundFlags) && pass == 0)
				{
					Vec2 vBegin(vBoxCentre + Vec2(lineDefs[lineDefIndex].vStart.x * vBoxDimensions.x, lineDefs[lineDefIndex].vStart.y * vBoxDimensions.y));
					Vec2 vEnd(vBoxCentre + Vec2(lineDefs[lineDefIndex].vEnd.x * vBoxDimensions.x, lineDefs[lineDefIndex].vEnd.y * vBoxDimensions.y));
					bufferDC.MoveTo(CPoint(vBegin.x, vBegin.y));
					bufferDC.LineTo(CPoint(vEnd.x, vEnd.y));
				}
			}
		}

		// Evaluate the channels and render the knob.
		Vec2 vChannelValues(m_vJoystickBeingManipulatedPosition);
		if (pJoystick != m_pJoystickBeingManipulated)
		{
			for (int channelIndex = 0; channelIndex < 2; ++channelIndex)
			{
				IJoystickChannel* pChannel = pJoystick ? pJoystick->GetChannel((IJoystick::ChannelType)channelIndex) : 0;
				vChannelValues[channelIndex] = pChannel ? JoystickUtils::Evaluate(pChannel, sequenceTime) : 0.0f;
				if (pChannel && pChannel->GetFlipped())
					vChannelValues[channelIndex] *= -1.0f;
			}
		}

		Vec2 vClampedChannelValues = vChannelValues;
		vClampedChannelValues.x = min(max(vClampedChannelValues.x, -1.0f), 1.0f);
		vClampedChannelValues.y = min(max(vClampedChannelValues.y, -1.0f), 1.0f);
		Vec2 vKnobPosition = vBoxCentre + Vec2(vBoxDimensions.x * vClampedChannelValues.x, vBoxDimensions.y * -vClampedChannelValues.y);

		CPen knobFramePen(PS_SOLID, 0, RGB(0x00, 0x00, 0x00));
		bufferDC.SelectObject(&knobFramePen);
		CBrush knobFrameBrush(RGB(0xFF, 0xFF, 0xFF));
		bufferDC.SelectObject(&knobFrameBrush);
		bufferDC.Ellipse(vKnobPosition.x - knobSize, vKnobPosition.y - knobSize, vKnobPosition.x + knobSize, vKnobPosition.y + knobSize);
	}
}

void CJoystickCtrl::OnCreateJoystick(const Vec2& position)
{
	GetIEditor()->GetIUndoManager()->Begin();
	StoreJoystickUndo();

	const Vec2 defaultDimensions(30, 30);

	IJoystick* pJoystick = m_pJoystickContext ? m_pJoystickContext->CreateJoystick(GenerateJoystickID()) : 0;
	if (pJoystick)
	{
		pJoystick->SetCentre(position);
		pJoystick->SetDimensions(defaultDimensions);
	}

	if (m_pJoystickContext && m_pJoysticks)
		m_pJoysticks->AddJoystick(pJoystick);

	if (pJoystick)
		pJoystick->SetName(GenerateDefaultNameForJoystick(pJoystick).c_str());

	GetIEditor()->GetIUndoManager()->Accept("Create Joystick");

	if (m_pContainer)
		m_pContainer->OnJoysticksChanged();

	SetFreezeLayout(false);

	UpdateDocumentRect();

	Redraw();
}

void CJoystickCtrl::OnEditJoystickName(IJoystick* pJoystick)
{
	QStringDialog dlg("Enter Joystick Name");
	dlg.SetString(pJoystick ? pJoystick->GetName() : "");
	if (pJoystick && dlg.exec() == QDialog::Accepted)
	{
		GetIEditor()->GetIUndoManager()->Begin();
		StoreJoystickUndo();

		pJoystick->SetName(dlg.GetString());

		GetIEditor()->GetIUndoManager()->Accept("Edit Joystick Name");

		if (m_pContainer)
			m_pContainer->OnJoysticksChanged();
	}

	Redraw();
}

void CJoystickCtrl::OnDeleteJoystick(IJoystick* pJoystick)
{
	GetIEditor()->GetIUndoManager()->Begin();
	StoreJoystickUndo();

	m_pJoysticks->RemoveJoystick(pJoystick);

	GetIEditor()->GetIUndoManager()->Accept("Delete Joystick");

	if (m_pContainer)
		m_pContainer->OnJoysticksChanged();

	Redraw();
}

void CJoystickCtrl::OnJoystickProperties(IJoystick* pJoystick)
{
	CJoystickPropertiesDlg dlg;
	for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); pJoystick && axis < 2; axis = IJoystick::ChannelType(axis + 1))
	{
		IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
		dlg.SetChannelName(axis, (pChannel ? pChannel->GetName() : 0));
		dlg.SetChannelFlipped(axis, (pChannel ? pChannel->GetFlipped() : false));
		dlg.SetVideoScale(axis, (pChannel ? pChannel->GetVideoMarkerScale() : 1.0f));
		dlg.SetVideoOffset(axis, (pChannel ? pChannel->GetVideoMarkerOffset() : 0.0f));
		dlg.SetChannelEnabled(axis, pChannel != 0);
	}

	if (dlg.DoModal() == IDOK)
	{
		GetIEditor()->GetIUndoManager()->Begin();
		StoreJoystickUndo();

		for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); pJoystick && axis < 2; axis = IJoystick::ChannelType(axis + 1))
		{
			IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
			if (pChannel)
			{
				pChannel->SetFlipped(dlg.GetChannelFlipped(axis));
				pChannel->SetVideoMarkerScale(dlg.GetVideoScale(axis));
				pChannel->SetVideoMarkerOffset(dlg.GetVideoOffset(axis));
			}
		}

		GetIEditor()->GetIUndoManager()->Accept("Set Joystick Properties");

		if (m_pContainer)
			m_pContainer->OnJoysticksChanged();

		Redraw();
	}
}

void CJoystickCtrl::OnEditJoystickColour(IJoystick* pJoystick)
{
	IJoystick::Color joystickColour = (pJoystick ? pJoystick->GetColor() : ZERO);
	COLORREF colourRef = RGB(joystickColour.x, joystickColour.y, joystickColour.z);
	CCustomColorDialog colourDlg(colourRef, CC_FULLOPEN, this);
	if (colourDlg.DoModal() == IDOK)
	{
		CUndo undo("Change Joystick Color");
		StoreJoystickUndo();

		if (pJoystick)
			pJoystick->SetColor(IJoystick::Color(GetRValue(colourDlg.GetColor()), GetGValue(colourDlg.GetColor()), GetBValue(colourDlg.GetColor())));

		if (m_pContainer)
			m_pContainer->OnJoysticksChanged();

		Redraw();
	}
}

void CJoystickCtrl::OnSetJoystickChannel(IJoystick* pSelectedJoystick, IJoystick::ChannelType channelType, IJoystickChannel* pChannel)
{
	bool replaceName = HasDefaultName(pSelectedJoystick);

	if (pSelectedJoystick && pSelectedJoystick->GetChannel(channelType) != pChannel)
		pSelectedJoystick->SetChannel(channelType, pChannel);

	if (replaceName)
		pSelectedJoystick->SetName(GenerateDefaultNameForJoystick(pSelectedJoystick).c_str());

	if (m_pContainer)
		m_pContainer->OnJoysticksChanged();

	Redraw();
}

void CJoystickCtrl::OnFlipJoystickChannel(IJoystick* pJoystick, IJoystick::ChannelType channelType)
{
	IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(channelType) : 0);
	if (pChannel)
		pChannel->SetFlipped(!pChannel->GetFlipped());

	Redraw();
}

void CJoystickCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_buffer.GetSafeHandle())
		m_buffer.DeleteObject();

	Redraw();
}

void CJoystickCtrl::OnContextMenu(CWnd* window, CPoint position)
{
	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	enum MenuCommand
	{
		MENU_COMMAND_CREATE_JOYSTICK = 1,
		MENU_COMMAND_EDIT_JOYSTICK_NAME,
		MENU_COMMAND_SET_JOYSTICK_CHANNEL_VERTICAL,
		MENU_COMMAND_SET_JOYSTICK_CHANNEL_HORIZONTAL,
		MENU_COMMAND_FLIP_CHANNEL_VERTICAL,
		MENU_COMMAND_FLIP_CHANNEL_HORIZONTAL,
		MENU_COMMAND_DELETE_JOYSTICK,
		MENU_COMMAND_EDIT_JOYSTICK_COLOUR,
		MENU_COMMAND_JOYSTICK_PROPERTIES
	};

	CPoint clientPosition = position;
	ScreenToClient(&clientPosition);
	IJoystick* pSelectedJoystick = HitTestJoysticks(Vec2(clientPosition.x, clientPosition.y));

	_smart_ptr<IJoystickChannel> pChannel = m_pContainer ? m_pContainer->GetPotentialJoystickChannel() : 0;

	if (pSelectedJoystick)
	{
		menu.AppendMenu(MF_STRING, MENU_COMMAND_EDIT_JOYSTICK_NAME, _T("Edit Joystick Name"));
		menu.AppendMenu(MF_STRING, MENU_COMMAND_EDIT_JOYSTICK_COLOUR, _T("Set Joystick Color"));
	}
	if (pSelectedJoystick && pChannel)
	{
		menu.AppendMenu(MF_STRING, MENU_COMMAND_SET_JOYSTICK_CHANNEL_HORIZONTAL, _T("Set Horizontal Joystick Channel"));
		menu.AppendMenu(MF_STRING, MENU_COMMAND_SET_JOYSTICK_CHANNEL_VERTICAL, _T("Set Vertical Joystick Channel"));
		menu.AppendMenu(MF_STRING, MENU_COMMAND_FLIP_CHANNEL_VERTICAL, _T("Flip Vertical Channel"));
		menu.AppendMenu(MF_STRING, MENU_COMMAND_FLIP_CHANNEL_HORIZONTAL, _T("Flip Horizontal Channel"));
	}
	if (!pSelectedJoystick)
		menu.AppendMenu(MF_STRING, MENU_COMMAND_CREATE_JOYSTICK, _T("Create Joystick"));
	if (pSelectedJoystick)
		menu.AppendMenu(MF_STRING, MENU_COMMAND_DELETE_JOYSTICK, _T("Delete Joystick"));
	if (pSelectedJoystick)
		menu.AppendMenu(MF_STRING, MENU_COMMAND_JOYSTICK_PROPERTIES, _T("Joystick Properties"));

	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, position.x, position.y, this);
	ScreenToClient(&position);
	Vec2 vCameraPosition = Vec2(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
	Vec2 vPosition = Vec2(position.x, position.y) + vCameraPosition;
	switch (cmd)
	{
	case MENU_COMMAND_CREATE_JOYSTICK:
		OnCreateJoystick(vPosition);
		break;
	case MENU_COMMAND_SET_JOYSTICK_CHANNEL_VERTICAL:
		OnSetJoystickChannel(pSelectedJoystick, IJoystick::ChannelTypeVertical, pChannel);
		break;
	case MENU_COMMAND_SET_JOYSTICK_CHANNEL_HORIZONTAL:
		OnSetJoystickChannel(pSelectedJoystick, IJoystick::ChannelTypeHorizontal, pChannel);
		break;
	case MENU_COMMAND_FLIP_CHANNEL_VERTICAL:
		OnFlipJoystickChannel(pSelectedJoystick, IJoystick::ChannelTypeVertical);
		break;
	case MENU_COMMAND_FLIP_CHANNEL_HORIZONTAL:
		OnFlipJoystickChannel(pSelectedJoystick, IJoystick::ChannelTypeHorizontal);
		break;
	case MENU_COMMAND_EDIT_JOYSTICK_NAME:
		OnEditJoystickName(pSelectedJoystick);
		break;
	case MENU_COMMAND_DELETE_JOYSTICK:
		OnDeleteJoystick(pSelectedJoystick);
		break;
	case MENU_COMMAND_JOYSTICK_PROPERTIES:
		OnJoystickProperties(pSelectedJoystick);
		break;
	case MENU_COMMAND_EDIT_JOYSTICK_COLOUR:
		OnEditJoystickColour(pSelectedJoystick);
		break;
	}
}

void CJoystickCtrl::SetFreezeLayout(bool bFreezeLayout)
{
	if (m_bFreezeLayout != bFreezeLayout)
	{
		m_bFreezeLayout = bFreezeLayout;
		if (m_pContainer)
			m_pContainer->OnFreezeLayoutChanged();
	}
}

bool CJoystickCtrl::GetFreezeLayout() const
{
	return m_bFreezeLayout;
}

void CJoystickCtrl::SetContainer(IJoystickCtrlContainer* pContainer)
{
	m_pContainer = pContainer;
}

_smart_ptr<IJoystickActionMode> CJoystickCtrl::FindActionModeForMouseDown(const Vec2& position)
{
	float sequenceTime = m_pContainer ? m_pContainer->GetCurrentEvaluationTime() : 0;

	_smart_ptr<IJoystickActionMode> pActionMode = 0;

	for (int joystickIndex = 0; m_pJoysticks && !pActionMode && joystickIndex < m_pJoysticks->GetJoystickCount(); ++joystickIndex)
	{
		IJoystick* pJoystick = m_pJoysticks ? m_pJoysticks->GetJoystick(joystickIndex) : 0;
		Vec2 vDimensions(pJoystick ? pJoystick->GetDimensions() : Vec2(0, 0));
		Vec2 vCentre(pJoystick ? pJoystick->GetCentre() : Vec2(0, 0));
		Vec2 vOffset(position - vCentre);
		Vec2 vAbsOffset(fabsf(vOffset.x), fabsf(vOffset.y));

		{
			Vec2 vBoxCentre = vCentre + Vec2(0, headerBoxHeight * 0.5);
			Vec2 vBoxDimensions = vDimensions - Vec2(0, headerBoxHeight * 0.5) - Vec2(knobSize + knobMargin, knobSize + knobMargin);
			vBoxDimensions = Vec2(max(vBoxDimensions.x, 0.0f), max(vBoxDimensions.y, 0.0f));

			if (!pActionMode && m_bFreezeLayout)
			{
				// Evaluate the channels and render the knob.
				Vec2 vChannelValues(0, 0);
				for (int channelIndex = 0; channelIndex < 2; ++channelIndex)
				{
					IJoystickChannel* pChannel = pJoystick ? pJoystick->GetChannel((IJoystick::ChannelType)channelIndex) : 0;
					vChannelValues[channelIndex] = pChannel ? JoystickUtils::Evaluate(pChannel, sequenceTime) : 0.0f;
					if (pChannel && pChannel->GetFlipped())
						vChannelValues[channelIndex] *= -1.0f;
				}

				Vec2 vClampedChannelValues = vChannelValues;
				vClampedChannelValues.x = min(max(vClampedChannelValues.x, -1.0f), 1.0f);
				vClampedChannelValues.y = min(max(vClampedChannelValues.y, -1.0f), 1.0f);
				Vec2 vKnobPosition = vBoxCentre + Vec2(vBoxDimensions.x * vClampedChannelValues.x, -vBoxDimensions.y * vClampedChannelValues.y);

				Vec2 vDistanceFromKnob = position - vKnobPosition;
				vDistanceFromKnob = Vec2(fabsf(vDistanceFromKnob.x), fabsf(vDistanceFromKnob.y));
				if (vDistanceFromKnob.x < knobSize && vDistanceFromKnob.y < knobSize)
					pActionMode = new DragKnobActionMode(pJoystick, m_pContainer, m_bAutoCreateKey);
			}

			if (!pActionMode && m_bFreezeLayout)
			{
				Vec2 vAxisCentres[] = { Vec2(0, 0), Vec2(0, 0) };
				Vec2 vAxisDimensions[] = { Vec2(1, 0.2f), Vec2(0.2f, 1) };
				for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); !pActionMode && axis < 2; axis = IJoystick::ChannelType(axis + 1))
				{
					Vec2 vAxisCentre(vAxisCentres[axis].x * vBoxDimensions.x + vBoxCentre.x, vAxisCentres[axis].y * vBoxDimensions.y + vBoxCentre.y);
					Vec2 vAxisDimensions(vAxisDimensions[axis].x * vBoxDimensions.x, vAxisDimensions[axis].y * vBoxDimensions.y);
					Vec2 vAbsOffset(fabsf(position.x - vAxisCentre.x), fabsf(position.y - vAxisCentre.y));
					if (pJoystick && vAbsOffset.x < vAxisDimensions.x && vAbsOffset.y < vAxisDimensions.y)
						pActionMode = new CAxisJoystickActionMode(pJoystick, axis, m_pContainer);
				}

				if (!pActionMode && vAbsOffset.x < vDimensions.x && vAbsOffset.y < vDimensions.y && pJoystick && m_pJoysticks)
					pActionMode = new CSelectJoystickActionMode(pJoystick, m_pContainer);
			}
		}

		if (!pActionMode && !m_bFreezeLayout)
		{
			enum AbsEdges
			{
				EDGE_X    = 1,
				EDGE_Y    = 2,
				EDGE_BOTH = EDGE_X | EDGE_Y
			};
			int absEdges = 0;
			const float tolerance = 5.0f;
			if (fabsf(vAbsOffset.x - vDimensions.x) < tolerance && vAbsOffset.y < vDimensions.y + tolerance)
				absEdges |= EDGE_X;
			if (fabsf(vAbsOffset.y - vDimensions.y) < tolerance && vAbsOffset.x < vDimensions.x + tolerance)
				absEdges |= EDGE_Y;

			bool shouldDragEdge = true;
			Vec2 vScale;
			switch (absEdges)
			{
			case EDGE_X:
				vScale = Vec2(sgn(vOffset.x), 0);
				break;
			case EDGE_Y:
				vScale = Vec2(0, sgn(vOffset.y));
				break;
			case EDGE_BOTH:
				vScale = Vec2(sgn(vOffset.x), sgn(vOffset.y));
				break;
			default:
				shouldDragEdge = false;
				break;
			}

			if (shouldDragEdge && pJoystick && m_pJoysticks)
				pActionMode = new ResizeJoystickActionMode(m_pJoysticks, pJoystick, vScale, m_snapMargin, m_minSize, m_pContainer);
		}

		if (!pActionMode && !m_bFreezeLayout)
		{
			if (vAbsOffset.x < vDimensions.x && vAbsOffset.y < vDimensions.y && pJoystick && m_pJoysticks)
				pActionMode = new DragJoystickActionMode(m_pJoysticks, pJoystick, m_snapMargin, m_pContainer);
		}
	}

	if (!pActionMode && m_bFreezeLayout)
		pActionMode = new CSelectJoystickActionMode(0, m_pContainer);

	return pActionMode;
}

IJoystick* CJoystickCtrl::HitTestJoysticks(const Vec2& position)
{
	Vec2 vCameraPosition = Vec2(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
	IJoystick* pSelectedJoystick = 0;

	for (int joystickIndex = 0; m_pJoysticks && !pSelectedJoystick && joystickIndex < m_pJoysticks->GetJoystickCount(); ++joystickIndex)
	{
		IJoystick* pJoystick = m_pJoysticks->GetJoystick(joystickIndex);
		Vec2 vDimensions(pJoystick->GetDimensions());
		Vec2 vOffset(position - pJoystick->GetCentre() + vCameraPosition);
		Vec2 vAbsOffset(fabsf(vOffset.x), fabsf(vOffset.y));

		if (!pSelectedJoystick && vAbsOffset.x < vDimensions.x && vAbsOffset.y < vDimensions.y)
			pSelectedJoystick = pJoystick;
	}

	return pSelectedJoystick;
}

void CJoystickCtrl::OnLButtonDown(UINT flags, CPoint point)
{
	Vec2 vCameraPosition = Vec2(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
	Vec2 position = Vec2(point.x, point.y) + vCameraPosition;

	if (m_pActionMode)
	{
		m_pActionMode->OnEndDragging(position, this);
		m_pActionMode = 0;
		ReleaseCapture();
	}
	else
	{
		_smart_ptr<IJoystickActionMode> pActionMode = FindActionModeForMouseDown(position);

		if (pActionMode)
		{
			m_vActionStart = position;
			m_pActionMode = pActionMode;
			m_bStartedDragging = false;

			if (m_pContainer && !pActionMode->RequiresPreview())
				m_pContainer->OnAction(IJoystickCtrlContainer::JOYSTICK_ACTION_START);

			SetCapture();
		}
	}
}

void CJoystickCtrl::OnLButtonUp(UINT flags, CPoint point)
{
	Vec2 vCameraPosition = Vec2(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
	Vec2 position = Vec2(point.x, point.y) + vCameraPosition;

	if (m_pActionMode)
	{
		bool bClick = false;
		if (m_bStartedDragging)
			m_pActionMode->OnEndDragging(position, this);
		else
			bClick = true;

		if (bClick && m_pActionMode->AllowsClickDragging())
		{
			m_bClickDragging = true;
			m_bStartedDragging = true;
			m_pActionMode->OnStartDragging(m_vActionStart, this);
		}
		else
		{
			if (bClick)
				m_pActionMode->OnClick(position, (flags & MK_CONTROL) != 0, this);

			if (m_pContainer && !m_pActionMode->RequiresPreview())
				m_pContainer->OnAction(IJoystickCtrlContainer::JOYSTICK_ACTION_END);

			m_pActionMode = 0;

			ReleaseCapture();
		}
	}
}

void CJoystickCtrl::OnMButtonDown(UINT flags, CPoint point)
{
	Vec2 position = Vec2(point.x, point.y);

	m_pMiddleMouseButtonHandler->OnMiddleMouseDown(position, this);

	SetCapture();
}

void CJoystickCtrl::OnMButtonUp(UINT flags, CPoint point)
{
	Vec2 position = Vec2(point.x, point.y);

	ReleaseCapture();

	m_pMiddleMouseButtonHandler->OnMiddleMouseUp(position, this);
}

void CJoystickCtrl::OnMouseMove(UINT flags, CPoint point)
{
	Vec2 vCameraPosition = Vec2(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
	Vec2 position = Vec2(point.x, point.y) + vCameraPosition;

	if (m_pActionMode)
	{
		if (!m_bStartedDragging)
		{
			m_bStartedDragging = true;

			m_pActionMode->OnStartDragging(m_vActionStart, this);
		}

		m_pActionMode->OnDragging(position, this);
	}

	m_pMiddleMouseButtonHandler->OnMouseMove(Vec2(point.x, point.y), this);
}

void CJoystickCtrl::SetSnapMargin(float snapMargin)
{
	m_snapMargin = snapMargin;
}

float CJoystickCtrl::GetSnapMargin() const
{
	return m_snapMargin;
}

void CJoystickCtrl::SetAutoCreateKey(bool bAutoCreateKey)
{
	m_bAutoCreateKey = bAutoCreateKey;
}

bool CJoystickCtrl::GetAutoCreateKey() const
{
	return m_bAutoCreateKey;
}

void CJoystickCtrl::RenderString(CDC& dc, const char* szString, const Vec2& position, int maxWidth, COLORREF colour)
{
	enum {MAX_STRING = 256};
	WCHAR wszString[MAX_STRING];

	int length = min((szString ? int(strlen(szString)) : 0), MAX_STRING - 1);
	Unicode::Convert(wszString, szString ? szString : "");

	if (wcslen(wszString) == 0)
		wcscpy(wszString, L"{}");

	// Create a Gdiplus graphics object from the DC.
	CFont* pFont = CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont);
	dc.SelectObject(pFont);
	Gdiplus::Graphics graphics(dc.GetSafeHdc());

	const int layoutHeight = 15;

	Gdiplus::StringFormat format;
	format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
	format.SetAlignment(Gdiplus::StringAlignmentCenter);

	Gdiplus::Font font(dc.GetSafeHdc());

	Gdiplus::RectF layoutBox(position.x, position.y, maxWidth, layoutHeight);
	Gdiplus::SolidBrush brush(Gdiplus::Color(GetRValue(colour), GetGValue(colour), GetBValue(colour)));
	graphics.DrawString(wszString, -1, &font, layoutBox, &format, &brush);
}

uint64 CJoystickCtrl::GenerateJoystickID()
{
	CryGUID guid = CryGUID::Create();
	return guid.hipart ^ guid.lopart;
}

void CJoystickCtrl::StoreSplineUndo(IJoystick* pJoystick)
{
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoJoystickChannelChange(this, m_pJoysticks, pJoystick));
}

void CJoystickCtrl::StoreJoystickUndo()
{
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoJoystickChange(this, m_pJoysticks));
}

void CJoystickCtrl::JoysticksChanged()
{
	UpdateDocumentRect();
	RedrawWindow();
}

void CJoystickCtrl::TerminateActionMode()
{
	m_pActionMode = 0;
}

void CJoystickCtrl::ReleaseMouseCapture()
{
	ReleaseCapture();
}

void CJoystickCtrl::SetJoystickBeingManipulated(IJoystick* pJoystickBeingManipulated)
{
	m_pJoystickBeingManipulated = pJoystickBeingManipulated;
}

void CJoystickCtrl::SetJoystickBeingManipulatedPosition(const Vec2& vJoystickBeingManipulatedPosition)
{
	m_vJoystickBeingManipulatedPosition = vJoystickBeingManipulatedPosition;
}

void CJoystickCtrl::BeginUndo()
{
	GetIEditor()->GetIUndoManager()->Begin();
}

void CJoystickCtrl::AcceptUndo(const char* name)
{
	GetIEditor()->GetIUndoManager()->Accept(name);
}

void CJoystickCtrl::RestoreUndo()
{
	GetIEditor()->GetIUndoManager()->Restore();
}

void CJoystickCtrl::OnSplineChangesUnOrReDone()
{
	Redraw();
	if (m_pContainer)
		m_pContainer->OnSplineChanged();
}

void CJoystickCtrl::OnJoytickChangesUnOrReDone()
{
	Redraw();
	if (m_pContainer)
		m_pContainer->OnJoysticksChanged();
}

void CJoystickCtrl::SerializeJoystickSet(IJoystickSet* pJoystickSet, XmlNodeRef node, bool bLoading)
{
	if (pJoystickSet && node)
		pJoystickSet->Serialize(node, bLoading);
}

Vec2 CJoystickCtrl::GetPosition() const
{
	return Vec2(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
}

void CJoystickCtrl::SetPosition(const Vec2& position)
{
	SetScrollPos(SB_HORZ, int(position.x));
	SetScrollPos(SB_VERT, int(position.y));
	Redraw();
}

string CJoystickCtrl::GenerateDefaultNameForJoystick(IJoystick* pJoystick)
{
	IJoystickChannel* pHorizontalChannel = (pJoystick ? pJoystick->GetChannel(IJoystick::ChannelTypeHorizontal) : 0);
	string name(pHorizontalChannel ? pHorizontalChannel->GetName() : "");
	IJoystickChannel* pVerticalChannel = (pJoystick ? pJoystick->GetChannel(IJoystick::ChannelTypeVertical) : 0);
	string name2(pVerticalChannel ? pVerticalChannel->GetName() : "");
	name += (name[0] && name2[0] ? " v " : "");
	name += name2;
	name = (name[0] ? name : "Empty");
	return name;
}

bool CJoystickCtrl::HasDefaultName(IJoystick* pJoystick)
{
	string defaultName = GenerateDefaultNameForJoystick(pJoystick);
	return defaultName == pJoystick->GetName();
}

void CJoystickCtrl::UpdateDocumentRect()
{
	CRect boundingRect(INT_MAX, INT_MAX, INT_MIN, INT_MIN);

	for (int joystickIndex = 0; m_pJoysticks && joystickIndex < m_pJoysticks->GetJoystickCount(); ++joystickIndex)
	{
		IJoystick* pJoystick = m_pJoysticks ? m_pJoysticks->GetJoystick(joystickIndex) : 0;

		Vec2 vCentre((pJoystick ? pJoystick->GetCentre() : Vec2(0, 0)));
		Vec2 vDimensions(pJoystick ? pJoystick->GetDimensions() : Vec2(0, 0));
		Vec2 vTopLeft(vCentre - vDimensions);
		Vec2 vBottomRight(vCentre + vDimensions);
		boundingRect.left = 0;
		boundingRect.top = 0;
		boundingRect.right = max(int(boundingRect.right), int(vBottomRight.x));
		boundingRect.bottom = max(int(boundingRect.bottom), int(vBottomRight.y));
	}

	m_stDesiredClientSize = boundingRect;

	OnClientSizeUpdated();
}

