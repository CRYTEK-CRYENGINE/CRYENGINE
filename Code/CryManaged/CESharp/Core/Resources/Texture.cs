// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System.Runtime.Serialization;
using CryEngine.Common;

namespace CryEngine.Resources
{
	/// <summary>
	/// Defines how a texture is supposed draw itself. Options are draw in one piece (None), or draw equally sized slices in horizontal and/or vertical directrion.
	/// </summary>
	public enum SliceType
	{
		None,
		ThreeHorizontal,
		ThreeVertical,
		Nine
	}

	/// <summary>
	/// Wrapper for CryEngine ITexture. Holds, creates and updates an ITexture instance.
	/// </summary>
	public class Texture
	{
		private ITexture _ceTex;

		[DataMember]
		private bool _isFiltered;

		private int _texId;
		private bool _isRenderTarget;

		/// <summary>
		/// Texture Width.
		/// </summary>
		/// <value>The width.</value>
		public int Width { get; private set; }

		/// <summary>
		/// Texture Height.
		/// </summary>
		/// <value>The height.</value>
		public int Height { get; private set; }

		/// <summary>
		/// Describing name for the dexture. Set to filename if loaded from file.
		/// </summary>
		/// <value>The name.</value>
		public string Name { get { return _ceTex.GetName(); } }

		/// <summary>
		/// ID retrieved from CryEngine on texture creation.
		/// </summary>
		/// <value>The identifier.</value>
		public int ID { get { return _texId; } }

		/// <summary>
		/// Creates a texture with image data and sets some default properties.
		/// </summary>
		/// <param name="width">Intended Texture Width.</param>
		/// <param name="height">Intended Texture Height.</param>
		/// <param name="data">Image source data.</param>
		/// <param name="isFiltered">If set to <c>true</c> texture is filtered.</param>
		/// <param name="isRendertarget">If set to <c>true</c> Texture can be rendered to.</param>
		public Texture(int width, int height, byte[] data, bool isFiltered = true, bool isRendertarget = false)
		{
			_isFiltered = isFiltered;
			_isRenderTarget = isRendertarget;
			CreateTexture(width, height, data);
		}

		/// <summary>
		/// Pushes new image data into video memory for the allocated texture object. May reinstantiate the texture object, e.g. if size changed.
		/// </summary>
		/// <param name="width">Target Width.</param>
		/// <param name="height">Target Height.</param>
		/// <param name="data">Image data for the texture to be filled with.</param>
		public void UpdateData(int width, int height, byte[] data)
		{
			if (width == Width && height == Height && _texId != 0)
			{
				Global.gEnv.pRenderer.UpdateTextureInVideoMemory ((uint)_texId, data, 0, 0, Width, Height, ETEX_Format.eTF_R8G8B8A8);
			}
			else
			{
				Destroy();
				CreateTexture(width, height, data);
			}
		}

		/// <summary>
		/// Creates a new texture object and fills it with given image data.
		/// </summary>
		/// <param name="width">Target Width.</param>
		/// <param name="height">Target Height.</param>
		/// <param name="data">Image data for the texture to be filled with.</param>
		private void CreateTexture(int width, int height, byte[] data)
		{
			Width = width;
			Height = height;
			var flags = (int)ETextureFlags.FT_NOMIPS;
			if(_isRenderTarget)
			{
				flags |= (int)ETextureFlags.FT_DONT_STREAM | (int)ETextureFlags.FT_DONT_RELEASE | (int)ETextureFlags.FT_USAGE_RENDERTARGET;
			}
			_ceTex = Global.gEnv.pRenderer.CreateTexture("MF", Width, Height, 1, data, (byte)ETEX_Format.eTF_R8G8B8A8, flags);
			_ceTex.SetClamp(false);
			_ceTex.SetHighQualityFiltering(_isFiltered);
			_ceTex.AddRef();

			_texId = _ceTex.GetTextureID();
		}

		/// <summary>
		/// Destroy this texture instance.
		/// </summary>
		public void Destroy()
		{
			if(_ceTex != null && _texId != 0)
			{
				var id = _texId;
				var isRenderTarget = _isRenderTarget;
				GameFramework.AddDestroyAction(() => DestroyNativeTexture(id, isRenderTarget));
				_ceTex.Dispose();
				_ceTex = null;
				_texId = 0;
			}
		}

		private void DestroyNativeTexture(int textureId, bool isRenderTarget)
		{
			var texture = Global.gEnv.pRenderer.EF_GetTextureByID(textureId);
			if(texture == null)
			{
				return;
			}
			if(!isRenderTarget)
			{
				texture.Release();
			}
			else
			{
				texture.ReleaseForce();
			}
			Global.gEnv.pRenderer.RemoveTexture((uint)textureId);

			texture.Dispose();
		}

		protected static bool IsNull(Texture wrapper)
		{
			var isNull = ReferenceEquals(wrapper, null);
			return isNull ? isNull : (wrapper._ceTex == null);
		}

		public static bool operator ==(Texture wrapperA, Texture wrapperB)
		{
			var aIsNull = IsNull(wrapperA);
			var bIsNull = IsNull(wrapperB);

			if (aIsNull && bIsNull)
			{
				return true;
			}

			if (aIsNull != bIsNull)
			{
				return false;
			}

			return ReferenceEquals(wrapperA, wrapperB);
		}

		public static bool operator !=(Texture wrapperA, Texture wrapperB)
		{
			return !(wrapperA == wrapperB);
		}

		public override bool Equals(object obj)
		{
			if (obj == null && IsNull(this))
			{
				return true;
			}
			return ReferenceEquals(this, obj);
		}

		public bool Equals(Texture wrapper)
		{
			if (IsNull(this) && IsNull(wrapper))
			{
				return true;
			}

			return ReferenceEquals(wrapper, this);
		}

		public override int GetHashCode()
		{
			if (IsNull(this))
			{
				throw new System.NullReferenceException();
			}
			return base.GetHashCode();
		}
	}
}
