// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

class CTagLocations
{
private:
	struct SLocation
	{
		SLocation()
			: position(0.0f, 0.0f, 0.0f)
			, angles(0.0f, 0.0f, 0.0f)
			, segment(0, 0)
		{
		}

		SLocation(const Vec3& _position, const Ang3& _angles, const Vec2i& _segment)
			: position(_position)
			, angles(_angles)
			, segment(_segment)
		{
		}

		void Serialize(Serialization::IArchive& ar)
		{
			ar(position, "position");
			ar(angles, "angles");
			ar(segment, "segment");
		}

		Vec3  position;
		Ang3  angles;
		Vec2i segment;
	};

public:
	CTagLocations();

	void TagLocation(int locationNum);
	void GotoTagLocation(int locationNum);
	void SaveTagLocations();
	void LoadTagLocations();

	void Serialize(Serialization::IArchive& ar)
	{
		ar(m_locations, "tags", "tags");
	}

private:
	// The game code depends on this in CryAction, deprecate asap
	void SaveToTxt();

	std::vector<SLocation> m_locations;
};

