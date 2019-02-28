// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine.Animations
{
	/// <summary>
	/// Defines the context of animations.
	/// </summary>
	public sealed class AnimationContext
	{
		[SerializeValue]
		internal SAnimationContext NativeHandle { get; private set; }

		internal AnimationContext(SAnimationContext nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Create a new <see cref="AnimationContext"/> based on the specified <see cref="ControllerDefinition"/>.
		/// </summary>
		/// <returns>The newly created <see cref="AnimationContext"/>.</returns>
		/// <param name="definition">The <see cref="ControllerDefinition"/> this <see cref="AnimationContext"/> will belong to.</param>
		public static AnimationContext Create(ControllerDefinition definition)
		{
			if(definition == null)
			{
				throw new ArgumentNullException(nameof(definition));
			}

			return new AnimationContext(new SAnimationContext(definition.NativeHandle));
		}

		/// <summary>
		/// Set the value of the specified <see cref="AnimationTag"/>.
		/// </summary>
		/// <param name="tag">The tag to identify which value to set.</param>
		/// <param name="value">The value that needs to be set.</param>
		public void SetTagValue(AnimationTag tag, bool value)
		{
			if(tag == null)
			{
				throw new ArgumentNullException(nameof(tag));
			}

			var id = tag.Id;
			if(id < 0)
			{
				throw new ArgumentException(string.Format("{0} with and id of {1} is an invalid AnimationTag!", nameof(tag), tag.Id), nameof(tag));
			}

			NativeHandle.state.Set(id, value);
		}

		/// <summary>
		/// Set the value of the specified id.
		/// </summary>
		/// <param name="id">The ID of the value which will be set.</param>
		/// <param name="value">The value that needs to be set.</param>
		public void SetTagValue(int id, bool value)
		{
			if(id < 0)
			{
				throw new ArgumentException(string.Format("ID with the value {0} is an invalid AnimationTag ID!", id), nameof(id));
			}

			NativeHandle.state.Set(id, value);
		}

		/// <summary>
		/// Find the <see cref="AnimationTag"/> with the specified name.
		/// </summary>
		/// <returns>The <see cref="AnimationTag"/> that was found, or null if none was found.</returns>
		/// <param name="tagName">Name of the tag.</param>
		public AnimationTag FindAnimationTag(string tagName)
		{
			if(string.IsNullOrWhiteSpace(tagName))
			{
				throw new ArgumentException(string.Format("{0} cannot be null or empty!", nameof(tagName)), nameof(tagName));
			}

			var id = NativeHandle.state.GetDef().Find(tagName);

			//In C++ there is a TAG_ID_INVALID const with a value of -1. But since we can't reach that const in C# we'll have to do id < 0 instead.
			if(id < 0)
			{
				Log.Error<AnimationContext>("Unable to find AnimationTag with name {0}!", tagName);
				return null;
			}

			return new AnimationTag(tagName, id);
		}

		/// <summary>
		/// Release this <see cref="AnimationContext"/> from the managed and unmanaged side.
		/// </summary>
		public void Release()
		{
			NativeHandle.Dispose();
			// TODO Set self to disposed.
		}
	}
}
