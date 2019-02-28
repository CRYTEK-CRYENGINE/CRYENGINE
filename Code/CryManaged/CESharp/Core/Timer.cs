using System;

namespace CryEngine
{
	/// <summary>
	/// Creates a Timer that will callback after expiring.
	/// </summary>
	public class Timer : IGameUpdateReceiver
	{
		private readonly float _timeToLive;
		private Action _onTimerExpired;
		private float _startTime;
		private float _time;

		public float Time { get { return _time; } }

		[SerializeValue]
		public bool Looping { get; private set; }

		public Timer(float time, bool looping, Action onTimerExpired = null)
		{
			GameFramework.RegisterForUpdate(this);

			_timeToLive = time;
			_onTimerExpired = onTimerExpired;
			Looping = looping;

			_startTime = Engine.Timer.GetCurrTime();
		}

		public Timer(float time, Action onTimerExpired = null) : this(time, false, onTimerExpired)
		{

		}

		public void Destroy()
		{
			GameFramework.UnregisterFromUpdate(this);
			_onTimerExpired = null;
		}

		void IGameUpdateReceiver.OnUpdate()
		{
			if (Engine.Timer.GetCurrTime() > _startTime + _timeToLive)
			{
				_onTimerExpired?.Invoke();

				if (Looping)
				{
					_startTime = Engine.Timer.GetCurrTime();
				}
				else
				{
					Destroy();
				}
			}
			else
			{
				_time += Engine.Timer.GetCurrTime();
			}
		}
	}
}

