// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace CryEngine
{
	/// <summary>
	/// Primitive shapes that can be loaded from the engine assets.
	/// </summary>
	public enum Primitives
	{
		/// <summary>
		/// A 2x2x0.125 box.
		/// </summary>
		Box,
		/// <summary>
		/// A 2x2x2 cube.
		/// </summary>
		Cube,
		/// <summary>
		/// A 0.25x0.25x0.25 cube.
		/// </summary>
		CubeSmall,
		/// <summary>
		/// A cylinder with a diameter of 2 units and a height of 2 units.
		/// </summary>
		Cylinder,
		/// <summary>
		/// A 1x1 plane.
		/// </summary>
		PlaneSmall,
		/// <summary>
		/// A pyramid shape of 2x2x2.
		/// </summary>
		Pyramid,
		/// <summary>
		/// A sphere with a diameter of 2 units.
		/// </summary>
		Sphere,
		/// <summary>
		/// A sphere with a diameter of 0.25 units.
		/// </summary>
		SphereSmall
	}
}
