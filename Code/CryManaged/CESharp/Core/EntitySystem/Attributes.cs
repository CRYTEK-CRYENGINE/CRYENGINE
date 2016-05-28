// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Attributes;

namespace CryEngine.EntitySystem
{
	/// <summary>
	/// Allows you to provide (Sandbox-relevant) metadata for CESharp entity classes. Only used for types inheriting from 'BaseEntity'
	/// </summary>
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
		/// If set, the marked entity class will not be displayed inside Sandbox.
		/// </summary>
		public bool Hide { get; set; }
		/// <summary>
		/// Helper function to determine if any custom Sandbox info has been set.
		/// </summary>
		public bool HasEditorInfo { get { return !(String.IsNullOrEmpty(EditorPath) && String.IsNullOrEmpty(Helper) && String.IsNullOrEmpty(Icon)); }}
		#endregion

		#region Constructors
		public EntityClassAttribute(string name, string category = "Game", string helper = null, string icon = "prompt.bmp", bool hide = false)
		{
			Name = name;
			EditorPath = category;
			Helper = helper;
			Icon = icon;
			Hide = hide;
		}
		#endregion
	}

	public enum EEditTypes
	{
		Default,
		[StringValue("n")]
		Number,
		[StringValue("i")]
		Integer,
		[StringValue("b")]
		Bool,
		[StringValue("f")]
		Float,
		[StringValue("s")]
		String,
		[StringValue("ei")]
		Integer_Enum,
		[StringValue("es")]
		String_Enum,
		[StringValue("shader")]
		Shader,
		[StringValue("clr")]
		Color,
		[StringValue("vector")]
		Vector,
		[StringValue("tex")]
		Texture,
		[StringValue("obj")]
		Geometry,
		[StringValue("file")]
		File,
		[StringValue("aibehavior")]
		AiBehavior,
		[StringValue("aipfpropertieslist")]
		AiPfPropertiesList,
		[StringValue("aientityclasses")]
		AiEntityClasses,
		[StringValue("aiterritory")]
		AiTerritory,
		[StringValue("aiwave")]
		AiWave,
		[StringValue("text")]
		Text,
		[StringValue("equip")]
		Equip,
		[StringValue("reverbpreset")]
		ReverbPreset,
		[StringValue("eaxpreset")]
		EaxPreset,
		[StringValue("aianchor")]
		AiAnchor,
		[StringValue("soclass")]
		SmarObjectClass,
		[StringValue("soclasses")]
		SmartObjectClasses,
		[StringValue("sostate")]
		SmartObjectState,
		[StringValue("sostates")]
		SmartObjectStates,
		[StringValue("sopattern")]
		SmartObjectPattern,
		[StringValue("soaction")]
		SmartObjectAction,
		[StringValue("sohelper")]
		SmartObjectHelper,
		[StringValue("sonavhelper")]
		SmartObjectNavHelper,
		[StringValue("soanimhelper")]
		SmartObjectAnimHelper,
		[StringValue("soevent")]
		SmartObjectEevent,
		[StringValue("sotemplate")]
		SmartObjectTemplate,
		[StringValue("customaction")]
		CustomAction,
		[StringValue("gametoken")]
		GameToken,
		[StringValue("seq_")]
		Sequence,
		[StringValue("seqid_")]
		SequenceId,
		[StringValue("mission")]
		Mission,
		[StringValue("lightanimation_")]
		LightAnimation,
		[StringValue("flare_")]
		Flare,
		[StringValue("ParticleEffect")]
		ParticleEffect,
		[StringValue("geomcache")]
		GeomCache,
		[StringValue("material")]
		Material,
		[StringValue("audioTrigger")]
		AudioTrigger,
		[StringValue("audioSwitch")]
		AudioSwitch,
		[StringValue("audioSwitchState")]
		AudioSwitchState,
		[StringValue("audioRTPC")]
		AudioRTPC,
		[StringValue("audioEnvironment")]
		AudioEnvironment,
		[StringValue("audioPreloadRequest")]
		AudioPreloadRequest
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
		public EEditTypes EditType { get; set; }
		/// <summary>
		/// Minimum valid value inside Sandbox.
		/// </summary>
		public float Min { get; set; }
		/// <summary>
		/// Maximum valid value inside Sandbox.
		/// </summary>
		public float Max { get; set; }
		/// <summary>
		/// Default value, when creating the entity.
		/// </summary>
		public string Default { get; set; }
		#endregion

		#region Constructors
		public EntityPropertyAttribute(string description = null, EEditTypes editType = EEditTypes.Default, float min = Int16.MinValue, float max = Int16.MaxValue, string defaultValue = null)
		{
			Description = description;
			EditType = editType;
			Min = min;
			Max = max;
			Default = defaultValue;
		}
		#endregion
	}
}