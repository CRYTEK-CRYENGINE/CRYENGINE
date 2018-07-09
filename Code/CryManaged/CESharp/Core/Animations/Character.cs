// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine.Animations
{
	//Wraps the native ICharacterInstace class

	/// <summary>
	/// Interface to character animation.
	/// This interface contains methods for manipulating and querying an animated character
	/// instance.
	/// </summary>
	public sealed class Character
	{
		[SerializeValue]
		internal ICharacterInstance NativeHandle { get; private set; }

		[SerializeValue]
		private IAttachmentManager _attachmentManager;

		public ECharRenderFlags Flags
		{
			get { return (ECharRenderFlags)NativeHandle.GetFlags(); }
			set { NativeHandle.SetFlags((int)value); }
		}

		/// <summary>
		/// The scale at which animation play on this <see cref="Character"/> .
		/// </summary>
		/// <value>The playback scale.</value>
		public float PlaybackScale
		{
			get
			{
				return NativeHandle.GetPlaybackScale();
			}
			set
			{
				NativeHandle.SetPlaybackScale(value);
			}
		}

		/// <summary>
		/// Gets a value indicating whether this <see cref="T:CryEngine.Animations.Character"/> has vertex animation.
		/// </summary>
		/// <value><c>true</c> if has vertex animation; otherwise, <c>false</c>.</value>
		public bool HasVertexAnimation
		{
			get
			{
				return NativeHandle.HasVertexAnimation();
			}
		}

		/// <summary>
		/// Get the filename of the character. This is either the model path or the CDF path.
		/// </summary>
		/// <value>The file path.</value>
		public string FilePath
		{
			get
			{
				return NativeHandle.GetFilePath();
			}
		}

		//TODO Wrap ISkeletonAnim in a managed class.
		/// <summary>
		/// Get the animation-skeleton for this instance.
		/// Returns the instance of an ISkeletonAnim derived class applicable for the model.
		/// </summary>
		/// <value>The animation skeleton.</value>
		public ISkeletonAnim AnimationSkeleton
		{
			get
			{
				return NativeHandle.GetISkeletonAnim();
			}
		}

		//TODO Wrap ISkeletonPose in a managed class.
		/// <summary>
		/// Get the pose-skeleton for this instance.
		/// Returns the instance of an ISkeletonPose derived class applicable for the model.
		/// </summary>
		/// <value>The pose skeleton.</value>
		public ISkeletonPose PoseSkeleton
		{
			get
			{
				return NativeHandle.GetISkeletonPose();
			}
		}

		//TODO Wrap IFacialInstance in a managed class.
		/// <summary>
		/// Get the Facial interface of this Character.
		/// </summary>
		/// <value>The character face.</value>
		public IFacialInstance CharacterFace
		{
			get
			{
				return NativeHandle.GetFacialInstance();
			}
		}

		internal Character(ICharacterInstance nativeCharacter)
		{
			if(nativeCharacter == null)
			{
				throw new ArgumentNullException(nameof(nativeCharacter));
			}
			NativeHandle = nativeCharacter;
		}

		/// <summary>
		/// Set the <see cref="MotionParameterId"/> to the specified <paramref name="value"/>.
		/// </summary>
		/// <param name="id">The <see cref="MotionParameterId"/> that needs to be set.</param>
		/// <param name="value">The value that will be set to the parameter.</param>
		public void SetAnimationSkeletonParameter(MotionParameterId id, float value)
		{
			var skeleton = AnimationSkeleton;
			if(skeleton == null)
			{
				throw new NullReferenceException(string.Format("The {0} of {1} is null!", nameof(AnimationSkeleton), nameof(Character)));
			}

			skeleton.SetDesiredMotionParam((EMotionParamID)id, value, 0);
		}

		/// <summary>
		/// Get the <see cref="CharacterAttachment"/> with the specified <paramref name="name"/>
		/// </summary>
		/// <returns>The <see cref="CharacterAttachment"/> or null if no attachment was found.</returns>
		/// <param name="name">Name of the required attachment.</param>
		public CharacterAttachment GetAttachment(string name)
		{
			if(_attachmentManager == null)
			{
				_attachmentManager = NativeHandle.GetIAttachmentManager();
			}

			var nativeAttachment = _attachmentManager.GetInterfaceByName(name);
			if(nativeAttachment == null)
			{
				return null;
			}
			return new CharacterAttachment(nativeAttachment);
		}

		/// <summary>
		/// Release this instance from both the managed and unmanaged side.
		/// </summary>
		public void Release()
		{
			var skeleton = NativeHandle?.GetISkeletonAnim();
			if(skeleton != null)
			{
				var values = Enum.GetValues(typeof(EMotionParamID)) as int[];
				if(values != null)
				{
					foreach(var value in values)
					{
						skeleton.SetDesiredMotionParam((EMotionParamID)value, 0, 0);
					}
				}
			}

			NativeHandle?.Dispose();
			// TODO Set self to disposed.
		}
	}
}
