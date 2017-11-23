// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine
{
	/// <summary>
	/// The type of asset or value the EntityProperty is exposing to the Sandbox.
	/// </summary>
	public enum EntityPropertyType : uint
	{
		/// <summary>
		/// Used by default for default types such as numerics and strings
		/// </summary>
		Primitive = 0,
		/// <summary>
		/// The property is a path to a geometry asset.
		/// </summary>
		Geometry,
		/// <summary>
		/// The property is a path to a texture asset.
		/// </summary>
		Texture,
		/// <summary>
		/// The property is a path to a particle asset.
		/// </summary>
		Particle,
		/// <summary>
		/// The property is a path to a file.
		/// </summary>
		AnyFile,
		/// <summary>
		/// The property is a path to a sound file.
		/// </summary>
		Sound,
		/// <summary>
		/// The property is a path to a material file.
		/// </summary>
		Material,
		//FIXME Animation property is not working properly in the Sandbox.
		/// <summary>
		/// The property is a path to an animation file.
		/// </summary>
		Animation
	}

	/// <summary>
	/// Marked properties will be available in the Sandbox
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class EntityPropertyAttribute : Attribute
	{
		#region Properties
		/// <summary>
		/// Mouse-Over description for Sandbox.
		/// </summary>
		public string Description { get; set; }
		/// <summary>
		/// Sandbox edit type. Determines the Sandbox control for this property.
		/// </summary>
		public EntityPropertyType Type { get; set; }
		#endregion

		#region Constructors
		public EntityPropertyAttribute(EntityPropertyType type = EntityPropertyType.Primitive, string description = null)
		{
			Description = description;
			Type = type;
		}
		#endregion
	}
}

