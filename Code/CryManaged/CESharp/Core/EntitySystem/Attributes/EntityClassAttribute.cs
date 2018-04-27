// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Attributes;

namespace CryEngine
{
	/// <summary>
	/// Allows creating an entity class containing a default entity component that is spawned with every instance. 
	/// Only used for types inheriting from 'EntityComponent'.
	/// </summary>
	[Obsolete("EntityClassAttribute is obsolete and will be removed. Use EntityComponentAttribute instead")]
	[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
	public sealed class EntityClassAttribute : Attribute
	{
		#region Properties
		/// <summary>
		/// Display name inside CRYENGINE's EntitySystem as well as Sandbox.
		/// </summary>
		public string Name { get; set; }
		/// <summary>
		/// Entity category inside Sandbox.
		/// </summary>
		public string EditorPath { get; set; }
		/// <summary>
		/// Geometry path for 3D helper.
		/// </summary>
		public string Helper { get; set; }
		/// <summary>
		/// Bitmap path for 2D helper.
		/// </summary>
		public string Icon { get; set; }
		/// <summary>
		/// If set to <c>true</c> the marked entity class will not be displayed inside Sandbox.
		/// </summary>
		public bool Hide { get; set; }
		#endregion

		#region Constructors

		/// <summary>
		/// Initializes a new instance of the obsolete <see cref="T:CryEngine.EntityClassAttribute"/> class.
		/// </summary>
		/// <param name="name">Display name inside CRYENGINE's EntitySystem as well as Sandbox..</param>
		/// <param name="category">Entity category inside Sandbox..</param>
		/// <param name="helper">Geometry path for 3D helper..</param>
		/// <param name="icon">Bitmap path for 2D helper..</param>
		/// <param name="hide">If set to <c>true</c> the marked entity class will not be displayed inside Sandbox.</param>
		[Obsolete("EntityClassAttribute is obsolete and will be removed. Use EntityComponentAttribute instead")]
		public EntityClassAttribute(string name = "", string category = "Game", string helper = null, string icon = "prompt.bmp", bool hide = false)
		{
			Name = name;
			EditorPath = category;
			Helper = helper;
			Icon = icon;
			Hide = hide;
		}

		/// <summary>
		/// Initializes a new instance of the obsolete <see cref="T:CryEngine.EntityClassAttribute"/> class.
		/// </summary>
		/// <param name="name"></param>
		/// <param name="category"></param>
		/// <param name="helper"></param>
		/// <param name="icon"></param>
		/// <param name="hide"></param>
		[Obsolete("EntityClassAttribute is obsolete and will be removed. Use EntityComponentAttribute instead")]
		public EntityClassAttribute(string name = "", string category = "Game", string helper = null, IconType icon = IconType.None, bool hide = false)
		{
			Name = name;
			EditorPath = category;
			Helper = helper;
			Hide = hide;
			//get icon path attribute bounded to the icon path enum (if available)
			var enumType = icon.GetType();
			var enumMemberInfo = enumType.GetMember(icon.ToString());
			var iconPathAttribute = (IconPathAttribute)enumMemberInfo[0].GetCustomAttributes(typeof(IconPathAttribute), true)[0];
			Icon = iconPathAttribute == null ? "": iconPathAttribute.Path;
		}
		#endregion
	}
}