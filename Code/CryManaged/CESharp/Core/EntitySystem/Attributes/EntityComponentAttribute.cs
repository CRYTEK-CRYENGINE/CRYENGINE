// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Attributes;

namespace CryEngine
{
	/// <summary>
	/// Used to specify specific parameters for entity components. If a parameter is not set, a default value will be set by the engine.
	/// Default name is the entity component class name.
	/// Default category is General.
	/// Default description will be empty.
	/// Default icon will be empty (entity components still receive an icon in the Sandbox by default).
	/// Default guid will be retrieved from a GUID-attribute if available. Otherwise it will be generated based on the class's fullname.
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class EntityComponentAttribute : Attribute
	{
		#region Properties
		/// <summary>
		/// Display name inside CRYENGINE's EntitySystem as well as Sandbox.
		/// </summary>
		public string Name { get; set; }
		/// <summary>
		/// Entity category inside Sandbox.
		/// </summary>
		public string Category { get; set; }
		/// <summary>
		/// User friendly description of what the component is for.
		/// </summary>
		public string Description { get; set; }
		/// <summary>
		/// Editor icon string, i.e. "icons:designer/Designer_Box.ico"
		/// </summary>
		public string Icon { get; set; }
		/// <summary>
		/// Systems reference entity components by GUID, for example when serializing to file to detect which type a component belongs to.
		/// By default we generate a GUID automatically based on the EntityComponent implementation type, however this will result in serialization breaking if you rename it.
		/// To circumvent this you can explicitly specify your desired GUID.
		/// Default GUID format is <c>"C47DF64B-E1F9-40D1-8063-2C533A1CE7D5"</c>.
		/// </summary>
		/// <value>The string value of the GUID.</value>
		public string Guid { get; set; }
		#endregion

		#region Constructors
		/// <summary>
		/// Used to specify specific parameters for entity components. If a parameter is not set, a default value will be set by the engine.
		/// Default name is the entity component class name.
		/// Default category is General.
		/// Default description will be empty.
		/// Default icon will be empty (entity components still receive an icon in the Sandbox by default).
		/// Default guid will be retrieved from a GUID-attribute if available. Otherwise it will be generated based on the class's fullname.
		/// </summary>
		/// <param name="uiName">The name of the component as it will be shown in the Sandbox.</param>
		/// <param name="category">The category in the Create Object and Add Component menu of the Sandbox that this component will be shown in.</param>
		/// <param name="description">Description of the component that will be shown in tooltips.</param>
		/// <param name="icon">Path to an icon in the icon folder.</param>
		/// <param name="guid">GUID in the format of XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX</param>
		public EntityComponentAttribute(string uiName = "", string category = "", string description = "", string icon = "", string guid = "")
		{
			Name = uiName;
			Category = category;
			Description = description;
			Icon = icon;
			Guid = guid;
		}

		/// <summary>
		/// Used to specify specific parameters for entity components. If a parameter is not set, a default value will be set by the engine.
		/// The <paramref name="icon"/> parameter is not optionally and should be set to a value of <see cref="IconType"/>.
		/// If it is set to <see cref="IconType.None"/> it will still receive an icon in the Sandbox by default.
		/// Default name is the entity component class name.
		/// Default category is General.
		/// Default description will be empty.
		/// Default guid will be retrieved from a GUID-attribute if available. Otherwise it will be generated based on the class's fullname.
		/// </summary>
		/// <param name="icon"></param>
		/// <param name="uiName">The name of the component as it will be shown in the Sandbox.</param>
		/// <param name="category">The category in the Create Object and Add Component menu of the Sandbox that this component will be shown in.</param>
		/// <param name="description">Description of the component that will be shown in tooltips.</param>
		/// <param name="guid">GUID in the format of XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX</param>
		public EntityComponentAttribute(IconType icon, string uiName = "", string category = "", string description = "", string guid = "")
		{
			Name = uiName;
			Category = category;
			Description = description;
			Guid = guid;

			// Get the icon path attribute bounded to the icon path enum (if available)
			var enumType = icon.GetType();
			var enumMemberInfo = enumType.GetMember(icon.ToString());
			var iconPathAttribute = (IconPathAttribute)enumMemberInfo[0].GetCustomAttributes(typeof(IconPathAttribute), true)[0];
			Icon = iconPathAttribute == null ? "" : (iconPathAttribute.Path ?? "");
		}
		#endregion
	}
}
