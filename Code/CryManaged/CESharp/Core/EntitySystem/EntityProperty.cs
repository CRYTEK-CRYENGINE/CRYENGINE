// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
		/// <summary>
		/// The property is a path to an animation file.
		/// </summary>
		Animation,
		/// <summary>
		/// The property is a path to a character file (.cdf).
		/// </summary>
		Character,
		/// <summary>
		/// The property is the name of a loaded action map.
		/// </summary>
		ActionMapName,
		/// <summary>
		/// The property is the name of an action of a loaded action map.
		/// </summary>
		ActionMapActionName
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
		/// <summary>
		/// Adding this attribute to a property of an <see cref="EntityComponent"/> exposes the property to the properties panel in the Sandbox.
		/// </summary>
		/// <param name="type">The value type of the property. This can change the way the property behaves in the Sandbox.</param>
		/// <param name="description">Mouse-over description of the property in the Sandbox.</param>
		public EntityPropertyAttribute(EntityPropertyType type = EntityPropertyType.Primitive, string description = null)
		{
			Description = description;
			Type = type;
		}
		#endregion
	}
}

