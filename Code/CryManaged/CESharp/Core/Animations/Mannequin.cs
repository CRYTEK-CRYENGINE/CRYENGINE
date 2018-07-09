// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;

namespace CryEngine.Animations
{
	/// <summary>
	/// Handles advanced character animations in the CRYENGINE.
	/// </summary>
	public class Mannequin
	{
		#region singleton properties and fields
		/// <summary>
		/// The <see cref="Mannequin"/>-interface that manages all the mannequins.
		/// </summary>
		/// <value>The mannequin interface.</value>
		public static Mannequin MannequinInterface
		{
			get
			{
				if(_mannequinInterface == null)
				{
					_mannequinInterface = new Mannequin(Engine.GameFramework.GetMannequinInterface());
				}
				return _mannequinInterface;
			}
		}

		private static Mannequin _mannequinInterface;
		#endregion

		[SerializeValue]
		internal IMannequin NativeHandle { get; private set; }

		/// <summary>
		/// Get the <see cref="AnimationDatabaseManager"/> of this <see cref="Mannequin"/>.
		/// </summary>
		/// <value>The animation database manager.</value>
		public AnimationDatabaseManager AnimationDatabaseManager
		{
			get
			{
				var nativeManager = NativeHandle.GetAnimationDatabaseManager();
				return nativeManager != null ? new AnimationDatabaseManager(nativeManager) : null;
			}
		}

		private Mannequin(IMannequin nativeHandle)
		{
			NativeHandle = nativeHandle;
		}

		/// <summary>
		/// Create a new <see cref="ActionController"/> for this <see cref="Mannequin"/> from the specified <paramref name="entity"/> and <paramref name="context"/>.
		/// </summary>
		/// <returns>The created <see cref="ActionController"/>.</returns>
		/// <param name="entity">The <see cref="Entity"/> to which the <see cref="ActionController"/> will belong.</param>
		/// <param name="context">The <see cref="AnimationContext"/> for the <see cref="ActionController"/>.</param>
		public ActionController CreateActionController(Entity entity, AnimationContext context)
		{
			if(entity == null)
			{
				throw new ArgumentNullException(nameof(entity));
			}

			if(context == null)
			{
				throw new ArgumentNullException(nameof(context));
			}

			var nativeObj = NativeHandle.CreateActionController(entity.NativeHandle, context.NativeHandle);

			return nativeObj == null ? null : new ActionController(nativeObj);
		}
	}
}
