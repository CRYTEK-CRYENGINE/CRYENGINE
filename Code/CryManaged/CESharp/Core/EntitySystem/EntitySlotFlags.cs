// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace CryEngine
{
	// Mirrors EEntitySlotFlags in the native code.

	/// <summary>
	/// Flags the can be set on each of the entity object slots.
	/// </summary>
	[System.Flags]
	public enum EntitySlotFlags : uint
	{
		/// <summary>
		/// Draw this slot.
		/// </summary>
		Render =                    1 << 0,
		/// <summary>
		/// Draw this slot as nearest. [Rendered in camera space].
		/// </summary>
		RenderNearest =             1 << 1,
		/// <summary>
		/// Draw this slot using custom camera passed as a Public ShaderParameter to the entity.
		/// </summary>
		RenderWithCustomCamera =    1 << 2,
		/// <summary>
		/// This slot will ignore physics events sent to it.
		/// </summary>
		IgnorePhysics =             1 << 3,
		/// <summary>
		/// Indicates this slot is part of an entity that has broken up.
		/// </summary>
		BreakAsEntity =             1 << 4,
		/// <summary>
		/// Draw this slot after post processing.
		/// </summary>
		RenderAfterPostProcessing = 1 << 5,
		/// <summary>
		/// In MP this is an entity that shouldn't fade or participate in network breakage.
		/// </summary>
		BreakAsEntityMP =           1 << 6,
		/// <summary>
		/// Draw shadows for this slot.
		/// </summary>
		CastShadow =                1 << 7,
		/// <summary>
		/// This slot ignores vis areas.
		/// </summary>
		IgnoreVisAreas =            1 << 8,
		/// <summary>
		/// Bit one of the GI Mode.
		/// </summary>
		GIModeBit0 =                1 << 9,
		/// <summary>
		/// Bit two of the GI Mode.
		/// </summary>
		GIModeBit1 =                1 << 10,
		/// <summary>
		/// Bit three of the GI Mode.
		/// </summary>
		GIModeBit2 =                1 << 11
	}
}
