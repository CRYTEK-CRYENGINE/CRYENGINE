// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace CryEngine.Animations
{
	/// <summary>
	/// Contains data of an animation tag.
	/// </summary>
	public sealed class AnimationTag
	{
		/// <summary>
		/// The name of this AnimationTag.
		/// </summary>
		/// <value>The name.</value>
		[SerializeValue]
		public string Name { get; private set; }

		/// <summary>
		/// The ID of this AnimationTag.
		/// </summary>
		/// <value>The identifier.</value>
		[SerializeValue]
		public int Id { get; private set; }

		internal AnimationTag(string name, int id)
		{
			Name = name;
			Id = id;
		}
	}
}
