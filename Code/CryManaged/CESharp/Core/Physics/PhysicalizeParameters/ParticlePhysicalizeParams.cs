using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing a Particle entity.
	/// A simple entity that represents a small lightweight rigid body. 
	/// It is simulated as a point with some thickness and supports flying, sliding and rolling modes. 
	/// Recommended usage: rockets, grenades and small debris.
	/// </summary>
	public class ParticlePhysicalizeParams : EntityPhysicalizeParams
	{
		private readonly ParticleParameters _particleParameters = new ParticleParameters();

		internal override pe_type Type { get { return pe_type.PE_PARTICLE; } }

		public ParticleParameters ParticleParameters
		{
			get
			{
				return _particleParameters;
			}
		}

		internal override SEntityPhysicalizeParams ToNativeParams()
		{
			var parameters = base.ToNativeParams();
			parameters.pParticle = _particleParameters.ToNativeParameters();
			return parameters;
		}
	}
}
