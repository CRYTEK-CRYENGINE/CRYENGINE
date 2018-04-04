// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

namespace CryEngine
{
	// Mirrors EEntitySlotFlags in the native code.

	public enum EntitySlotFlags
	{
		/// <summary>
		/// Draw this slot.
		/// </summary>
		Render = 1,
		/// <summary>
		/// Draw this slot as nearest. [Rendered in camera space].
		/// </summary>
		RenderNearest,
		/// <summary>
		/// Draw this slot using custom camera passed as a Public ShaderParameter to the entity.
		/// </summary>
		RenderWithCustomCamera = 4,
		/// <summary>
		/// This slot will ignore physics events sent to it.
		/// </summary>
		IgnorePhysics = 8,
		BreakAsEntity = 16,

		RenderAfterPostProcessing = 32,

		BreakAsEntityMP = 64
	}
}
