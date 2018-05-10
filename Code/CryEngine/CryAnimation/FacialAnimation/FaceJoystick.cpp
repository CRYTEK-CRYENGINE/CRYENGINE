// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FaceJoystick.h"

#include "FaceAnimSequence.h"

CFacialJoystickChannel::CFacialJoystickChannel(const string& channelName)
	: m_pChannel(0),
	m_channelName(channelName),
	m_refCount(0),
	m_bFlipped(false),
	m_videoMarkerOffset(0.0f),
	m_videoMarkerScale(1.0f)
{
}

CFacialJoystickChannel::CFacialJoystickChannel(IFacialAnimChannel* pChannel)
	: m_pChannel(pChannel),
	m_refCount(0),
	m_bFlipped(false),
	m_videoMarkerOffset(0.0f),
	m_videoMarkerScale(1.0f)
{
	IFacialAnimChannel* pAncestor = pChannel;
	string fullName;
	while (pAncestor)
	{
		const char* szChannelName = pAncestor ? pAncestor->GetIdentifier().GetString() : 0;
		szChannelName = szChannelName ? szChannelName : "";
		fullName = string(szChannelName) + "::" + fullName;
		pAncestor = pAncestor->GetParent();
	}

	m_channelName = fullName;
}

void CFacialJoystickChannel::AddRef()
{
	++m_refCount;
}

void CFacialJoystickChannel::Release()
{
	--m_refCount;
	if (m_refCount <= 0)
		delete this;
}

const char* CFacialJoystickChannel::GetName() const
{
	return (m_pChannel ? m_pChannel->GetIdentifier().GetString() : 0);
}

void* CFacialJoystickChannel::GetTarget()
{
	return m_pChannel;
}

int CFacialJoystickChannel::GetSplineCount()
{
	return (m_pChannel ? m_pChannel->GetInterpolatorCount() : 0);
}

ISplineInterpolator* CFacialJoystickChannel::GetSpline(int splineIndex)
{
	return (m_pChannel && splineIndex >= 0 && splineIndex < m_pChannel->GetInterpolatorCount() ? m_pChannel->GetInterpolator(splineIndex) : 0);
}

void CFacialJoystickChannel::SetFlipped(bool flipped)
{
	m_bFlipped = flipped;
}

bool CFacialJoystickChannel::GetFlipped() const
{
	return m_bFlipped;
}

void CFacialJoystickChannel::SetVideoMarkerOffset(float offset)
{
	m_videoMarkerOffset = offset;
}

float CFacialJoystickChannel::GetVideoMarkerOffset() const
{
	return m_videoMarkerOffset;
}

void CFacialJoystickChannel::SetVideoMarkerScale(float scale)
{
	m_videoMarkerScale = scale;
}

float CFacialJoystickChannel::GetVideoMarkerScale() const
{
	return m_videoMarkerScale;
}

void CFacialJoystickChannel::CleanupKeys(float fErrorMax)
{
	if (m_pChannel)
		m_pChannel->CleanupKeys(fErrorMax);
}

const char* CFacialJoystickChannel::GetPath() const
{
	return m_channelName.c_str();
}

IFacialAnimChannel* CFacialJoystickChannel::GetFacialChannel()
{
	return m_pChannel;
}

void CFacialJoystickChannel::Bind(IFacialJoystickSerializeContext* pContext)
{
	m_pChannel = (pContext ? pContext->FindChannel(m_channelName) : 0);
}

CFacialJoystick::CFacialJoystick(uint64 id)
	: m_id(id),
	m_refCount(0),
	m_colour(0xFF, 0xBC, 0x6A)
{
}

void CFacialJoystick::AddRef()
{
	++m_refCount;
}

void CFacialJoystick::Release()
{
	--m_refCount;
	if (m_refCount <= 0)
		delete this;
}

uint64 CFacialJoystick::GetID() const
{
	return m_id;
}

void CFacialJoystick::SetName(const char* szName)
{
	m_name = szName;
}

const char* CFacialJoystick::GetName()
{
	return m_name.c_str();
}

IJoystickChannel* CFacialJoystick::GetChannel(ChannelType type)
{
	assert((uint)type < NumChannelTypes);
	if ((uint)type >= NumChannelTypes)
		return 0;
	return m_channels[type];
}

void CFacialJoystick::SetChannel(ChannelType type, IJoystickChannel* pChannel)
{
	assert((uint)type < NumChannelTypes);
	if ((uint)type >= NumChannelTypes)
		return;
	m_channels[type] = static_cast<CFacialJoystickChannel*>(pChannel);
}

const Vec2& CFacialJoystick::GetCentre() const
{
	return m_vCentre;
}

void CFacialJoystick::SetCentre(const Vec2& vCentre)
{
	m_vCentre = vCentre;
}

const Vec2& CFacialJoystick::GetDimensions() const
{
	return m_vDimensions;
}

void CFacialJoystick::SetDimensions(const Vec2& vDimensions)
{
	m_vDimensions = vDimensions;
}

CFacialJoystickChannel* CFacialJoystick::GetDerivedChannel(ChannelType type)
{
	assert((uint)type < NumChannelTypes);
	if ((uint)type >= NumChannelTypes)
		return 0;
	return m_channels[type];
}

void CFacialJoystick::Bind(IFacialJoystickSerializeContext* pContext)
{
	for (uint axis = 0; axis < NumChannelTypes; ++axis)
	{
		if (m_channels[axis])
			m_channels[axis]->Bind(pContext);
	}
}

void CFacialJoystick::SetColor(const Color& colour)
{
	m_colour = colour;
}

IJoystick::Color CFacialJoystick::GetColor() const
{
	return m_colour;
}

CFacialJoystickSet::CFacialJoystickSet()
	: m_refCount(0)
{
}

void CFacialJoystickSet::HandleRemovedChannel(IFacialAnimChannel* pRemovedChannel)
{
	for (JoystickContainer::iterator itJoystick = m_joysticks.begin(); itJoystick != m_joysticks.end(); ++itJoystick)
	{
		CFacialJoystick* pJoystick = *itJoystick;
		assert(pJoystick);
		PREFAST_ASSUME(pJoystick);
		for (size_t i = 0; i < IJoystick::NumChannelTypes; ++i)
		{
			IJoystick::ChannelType axis = IJoystick::ChannelType(i);
			_smart_ptr<CFacialJoystickChannel> pChannel = (pJoystick ? pJoystick->GetDerivedChannel(axis) : 0);
			if (pChannel && pRemovedChannel == pChannel->GetFacialChannel())
				pJoystick->SetChannel(axis, 0);
		}
	}
}

void CFacialJoystickSet::AddRef()
{
	++m_refCount;
}

void CFacialJoystickSet::Release()
{
	--m_refCount;
	if (m_refCount <= 0)
		delete this;
}

void CFacialJoystickSet::SetName(const char* name)
{
	m_name = name;
}

const char* CFacialJoystickSet::GetName() const
{
	return m_name.c_str();
}

void CFacialJoystickSet::AddJoystick(IJoystick* pJoystick)
{
	m_joysticks.push_back(static_cast<CFacialJoystick*>(pJoystick));
}

void CFacialJoystickSet::RemoveJoystick(IJoystick* pJoystick)
{
	m_joysticks.erase(std::remove(m_joysticks.begin(), m_joysticks.end(), pJoystick));
}

int CFacialJoystickSet::GetJoystickCount() const
{
	return int(m_joysticks.size());
}

IJoystick* CFacialJoystickSet::GetJoystick(int index)
{
	return m_joysticks[index];
}

IJoystick* CFacialJoystickSet::GetJoystickAtPoint(const Vec2& vPosition)
{
	for (JoystickContainer::iterator itJoystick = m_joysticks.begin(); itJoystick != m_joysticks.end(); ++itJoystick)
	{
		Vec2 vOffset((*itJoystick)->GetCentre() - vPosition);
		if (fabsf(vOffset.x) < (*itJoystick)->GetDimensions().y && fabsf(vOffset.y) < (*itJoystick)->GetDimensions().y)
			return (*itJoystick);
	}

	return 0;
}

IJoystick* CFacialJoystickSet::GetJoystickByID(uint64 id)
{
	IJoystick* pSelectedJoystick = 0;
	for (JoystickContainer::iterator itJoystick = m_joysticks.begin(); itJoystick != m_joysticks.end(); ++itJoystick)
	{
		IJoystick* pJoystick = *itJoystick;
		if (pJoystick && pJoystick->GetID() == id)
			pSelectedJoystick = pJoystick;
	}

	return pSelectedJoystick;
}

void CFacialJoystickSet::Serialize(XmlNodeRef& node, bool bLoading)
{
	typedef const Vec2& (CFacialJoystick::* VecGetter)() const;
	typedef void (CFacialJoystick::*        VecSetter)(const Vec2&);
	class VecProperty
	{
	public:
		VecProperty(VecGetter getter, VecSetter setter, const char* name) : getter(getter), setter(setter), name(name) {}

		VecGetter   getter;
		VecSetter   setter;
		const char* name;
	};

	VecProperty vecProperties[] = {
		VecProperty(&CFacialJoystick::GetCentre,     &CFacialJoystick::SetCentre,     "Centre"),
		VecProperty(&CFacialJoystick::GetDimensions, &CFacialJoystick::SetDimensions, "Dimensions")
	};
	enum {NUM_VEC_PROPERTIES = CRY_ARRAY_COUNT(vecProperties)};

	if (bLoading)
	{
		m_joysticks.clear();

		for (int childIndex = 0; node && childIndex < node->getChildCount(); ++childIndex)
		{
			XmlNodeRef childNode = (node ? node->getChild(childIndex) : (XmlNodeRef)0);
			if (childNode && stricmp(childNode->getTag(), "Joystick") == 0)
			{
				XmlNodeRef joystickNode = childNode;

				uint64 id = 0;
				if (joystickNode && joystickNode->haveAttr("ID"))
					joystickNode->getAttr("ID", id);

				_smart_ptr<CFacialJoystick> pJoystick = (joystickNode && id ? new CFacialJoystick(id) : 0);

				if (pJoystick && joystickNode && joystickNode->haveAttr("Name"))
				{
					const char* szName = joystickNode->getAttr("Name");
					pJoystick->SetName(szName ? szName : "");
				}

				if (pJoystick && joystickNode && joystickNode->haveAttr("Color"))
				{
					unsigned colourValue;
					joystickNode->getAttr("Color", colourValue);
					IJoystick::Color colour(((colourValue >> 16) & 0xFF), ((colourValue >> 8) & 0xFF), (colourValue & 0xFF));
					pJoystick->SetColor(colour);
				}

				for (int joystickChildIndex = 0; joystickNode && joystickChildIndex < joystickNode->getChildCount(); ++joystickChildIndex)
				{
					XmlNodeRef joystickChildNode = (joystickNode ? joystickNode->getChild(joystickChildIndex) : (XmlNodeRef)0);

					if (joystickChildNode && stricmp(joystickChildNode->getTag(), "Channel") == 0)
					{
						int axis = -1;
						if (joystickChildNode && joystickChildNode->haveAttr("Axis"))
						{
							if (stricmp(joystickChildNode->getAttr("Axis"), "Horizontal") == 0)
								axis = IJoystick::ChannelTypeHorizontal;
							else if (stricmp(joystickChildNode->getAttr("Axis"), "Vertical") == 0)
								axis = IJoystick::ChannelTypeVertical;
						}

						const char* szChannelName = 0;
						if (joystickChildNode && joystickChildNode->haveAttr("Channel"))
							szChannelName = joystickChildNode->getAttr("Channel");

						if (pJoystick && szChannelName && axis != -1 && joystickNode)
						{
							CFacialJoystickChannel* pNewJoystickChannel = new CFacialJoystickChannel(szChannelName);
							if (joystickChildNode && joystickChildNode->haveAttr("Flipped"))
							{
								bool flipped;
								joystickChildNode->getAttr("Flipped", flipped);
								pNewJoystickChannel->SetFlipped(flipped);
							}
							if (joystickChildNode && joystickChildNode->haveAttr("VideoScale"))
							{
								float scale;
								joystickChildNode->getAttr("VideoScale", scale);
								pNewJoystickChannel->SetVideoMarkerScale(scale);
							}
							if (joystickChildNode && joystickChildNode->haveAttr("VideoOffset"))
							{
								float offset;
								joystickChildNode->getAttr("VideoOffset", offset);
								pNewJoystickChannel->SetVideoMarkerOffset(offset);
							}
							pJoystick->SetChannel(IJoystick::ChannelType(axis), pNewJoystickChannel);
						}
					}
					else
					{
						for (int vecIndex = 0; vecIndex < NUM_VEC_PROPERTIES; ++vecIndex)
						{
							if (joystickChildNode && vecProperties[vecIndex].name && stricmp(vecProperties[vecIndex].name, joystickChildNode->getTag()) == 0)
							{
								Vec2 v(10, 10);
								if (joystickChildNode && joystickChildNode->haveAttr("x"))
									joystickChildNode->getAttr("x", v.x);
								if (joystickChildNode && joystickChildNode->haveAttr("y"))
									joystickChildNode->getAttr("y", v.y);
								if (pJoystick.get() && vecProperties[vecIndex].setter)
									(pJoystick.get()->*(vecProperties[vecIndex].setter))(v);
							}
						}
					}
				}

				if (pJoystick)
					m_joysticks.push_back(pJoystick);
			}
		}
	}
	else
	{
		for (int joystickIndex = 0, end = int(m_joysticks.size()); joystickIndex < end; ++joystickIndex)
		{
			CFacialJoystick* pJoystick = m_joysticks[joystickIndex];
			XmlNodeRef joystickNode = (node ? node->newChild("Joystick") : (XmlNodeRef)0);

			if (joystickNode)
			{
				joystickNode->setAttr("ID", (pJoystick ? pJoystick->GetID() : 0));
				joystickNode->setAttr("Name", (pJoystick ? pJoystick->GetName() : ""));
				IJoystick::Color colour = (pJoystick ? pJoystick->GetColor() : ZERO);
				joystickNode->setAttr("Color", ((colour.x << 16) | (colour.y << 8) | colour.z));
			}

			for (int vecIndex = 0; vecIndex < NUM_VEC_PROPERTIES; ++vecIndex)
			{
				Vec2 v = (pJoystick && vecProperties[vecIndex].getter ? (pJoystick->*(vecProperties[vecIndex].getter))() : Vec2(10, 10));
				XmlNodeRef vecNode = (joystickNode && vecProperties[vecIndex].name ? joystickNode->newChild(vecProperties[vecIndex].name) : XmlNodeRef(0));
				if (vecNode)
				{
					vecNode->setAttr("x", v.x);
					vecNode->setAttr("y", v.y);
				}
			}

			const char* axisNames[] = { "Horizontal", "Vertical" };
			enum {NUM_AXES = CRY_ARRAY_COUNT(axisNames)};
			for (int axis = 0; axis < NUM_AXES; ++axis)
			{
				CFacialJoystickChannel* pChannel = (pJoystick ? pJoystick->GetDerivedChannel(IJoystick::ChannelType(axis)) : 0);
				XmlNodeRef channelNode = (joystickNode && pChannel ? joystickNode->newChild("Channel") : XmlNodeRef(0));
				if (channelNode)
				{
					channelNode->setAttr("Axis", (axisNames[axis] ? axisNames[axis] : ""));

					const char* name = (pChannel ? pChannel->GetPath() : 0);
					channelNode->setAttr("Channel", name ? name : "");
					if (pChannel)
					{
						channelNode->setAttr("Flipped", pChannel->GetFlipped());
						channelNode->setAttr("VideoScale", pChannel->GetVideoMarkerScale());
						channelNode->setAttr("VideoOffset", pChannel->GetVideoMarkerOffset());
					}
				}
			}
		}
	}
}

void CFacialJoystickSet::Bind(IFacialJoystickSerializeContext* pContext)
{
	for (int joystickIndex = 0, end = int(m_joysticks.size()); joystickIndex < end; ++joystickIndex)
	{
		if (m_joysticks[joystickIndex])
			m_joysticks[joystickIndex]->Bind(pContext);
	}
}
