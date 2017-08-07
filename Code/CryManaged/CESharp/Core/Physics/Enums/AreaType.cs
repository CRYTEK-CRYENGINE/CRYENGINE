namespace CryEngine
{
	/// <summary>
	/// Used when physicalizing an area with AreaPhysicalizeParams.
	/// </summary>
	public enum AreaType
	{
		/// <summary>
		/// Physical area will be a sphere.
		/// </summary>
		Sphere,
		/// <summary>
		/// Physical area will be a box.
		/// </summary>
		Box,
		/// <summary>
		/// Physical area will use the geometry from the specified slot.
		/// </summary>
		Geometry,
		/// <summary>
		/// Physical area will use points to specify a 2D shape.
		/// </summary>
		Shape,
		/// <summary>
		/// Physical area will be a cylinder.
		/// </summary>
		Cylinder,
		/// <summary>
		/// Physical area will be a spline-tube.
		/// </summary>
		Spline
	}
}
