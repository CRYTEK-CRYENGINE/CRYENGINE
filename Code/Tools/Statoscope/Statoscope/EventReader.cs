// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Reflection;

namespace Statoscope
{
	enum EventId
	{
		EI_DefineClass = 0,
		EI_BeginInterval,
		EI_EndInterval,
		EI_ModifyInterval,
		EI_ModifyIntervalBit,
	}

	class EventPropertyDesc
	{
		public EFrameElementType type;
		public FieldInfo field;

		public EventPropertyDesc(EFrameElementType type, FieldInfo field)
		{
			this.type = type;
			this.field = field;
		}
	}

	class EventClassDesc
	{
		public readonly Type propertiesType;
		public readonly EventPropertyDesc[] properties;

		public EventClassDesc(Type propertiesType, EventPropertyDesc[] properties)
		{
			this.propertiesType = propertiesType;
			this.properties = properties;
		}
	}

	class EventReader
	{
		Dictionary<UInt64, PendingInterval> m_pendingIntervals = new Dictionary<UInt64, PendingInterval>();
		byte m_nextEventSequence = 0;
		Int64 m_lastTimestamp = 0;

		Dictionary<UInt32, EventClassDesc> m_classDescs = new Dictionary<UInt32, EventClassDesc>();
		Dictionary<UInt32, IIntervalSink> m_sinks = new Dictionary<uint, IIntervalSink>();

		public EventReader(IIntervalSink[] intervalSinks)
		{
			foreach (IIntervalSink sink in intervalSinks)
				m_sinks[sink.ClassId] = sink;
		}

		public void ReadEvents(IBinaryLogDataStream stream)
		{
			UInt32 eventStreamLen = stream.ReadUInt32();
			int eventStreamPos = 0;
			for (eventStreamPos = 0; eventStreamPos < (int)eventStreamLen; )
			{
				byte eventId = stream.ReadByte();
				byte sequence = stream.ReadByte();
				UInt16 eventLengthInWords = stream.ReadUInt16();
				Int64 timeStamp = stream.ReadInt64();

				if (sequence != m_nextEventSequence)
					throw new ApplicationException("Event sequence mismatch");
				if (timeStamp < m_lastTimestamp)
					throw new ApplicationException("Timestamps go backwards");

				++m_nextEventSequence;
				m_lastTimestamp = timeStamp;

				int eventLength = eventLengthInWords * 4 - 12;

				switch ((EventId)eventId)
				{
					case EventId.EI_DefineClass: ReadDefineClass(timeStamp, eventLength, stream); break;
					case EventId.EI_BeginInterval: ReadBeginInterval(timeStamp, eventLength, stream); break;
					case EventId.EI_EndInterval: ReadEndInterval(timeStamp, eventLength, stream); break;
					case EventId.EI_ModifyInterval: ReadModifyInterval(timeStamp, eventLength, stream); break;
					case EventId.EI_ModifyIntervalBit: ReadModifyIntervalBit(timeStamp, eventLength, stream); break;
					default: throw new ApplicationException("Unknown event type");
				}

				eventStreamPos += eventLengthInWords * 4;
			}

			if (eventStreamPos != eventStreamLen)
				throw new ApplicationException("Event stream is corrupt");
		}

		private void ReadDefineClass(Int64 timeStamp, int eventLength, IBinaryLogDataStream stream)
		{
			UInt32 classId = stream.ReadUInt32();
			UInt32 numElements = stream.ReadUInt32();

			byte[] descBytes = stream.ReadBytes(eventLength - 8);

			IIntervalSink sink = m_sinks[classId];
			Type propType = sink.PropertiesType;

			EventPropertyDesc[] properties = new EventPropertyDesc[numElements];

			int p = 0;
			for (UInt32 elementIdx = 0; elementIdx < numElements; ++elementIdx)
			{
				byte typeId = descBytes[p++];

				int nullTermIdx = Array.IndexOf<byte>(descBytes, 0, p);
				string name = System.Text.Encoding.ASCII.GetString(descBytes, p, (nullTermIdx >= 0 ? nullTermIdx : descBytes.Length) - p);
				p = nullTermIdx + 1;

				// Find a field with the same name in the C# type
				FieldInfo field = propType.GetField(name, BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public);
				properties[elementIdx] = new EventPropertyDesc((EFrameElementType)typeId, field);
			}

			m_classDescs[classId] = new EventClassDesc(propType, properties);
		}

		private void ReadBeginInterval(Int64 timeStamp, int eventLength, IBinaryLogDataStream stream)
		{
			UInt64 ivId = stream.ReadUInt64();
			UInt32 classId = stream.ReadUInt32();

			EventClassDesc classDesc = m_classDescs[classId];

			var pi = new PendingInterval();

			Interval iv = (Interval)Activator.CreateInstance(classDesc.propertiesType);
			iv.StartUs = timeStamp;
			iv.EndUs = Int64.MaxValue;

			pi.classId = classId;
			pi.sink = m_sinks[classId];

			byte[] values = stream.ReadBytes(eventLength - 12);

			int p = 0;
			for (int i = 0, c = classDesc.properties.Length; i < c; ++i)
			{
				object val;

				switch (classDesc.properties[i].type)
				{
					case EFrameElementType.Float:
						val = EndianBitConverter.ToSingle(values, p, stream.Endianness);
						p += 4;
						break;

					case EFrameElementType.Int:
						val = EndianBitConverter.ToInt32(values, p, stream.Endianness);
						p += 4;
						break;

					case EFrameElementType.String:
						{
							int nullTerm = Array.IndexOf<byte>(values, 0, p);
							string s = System.Text.Encoding.ASCII.GetString(values, p, nullTerm - p);
							p = (nullTerm + 4) & ~3;
							val = s;
						}
						break;

					case EFrameElementType.Int64:
						val = EndianBitConverter.ToInt64(values, p, stream.Endianness);
						p += 8;
						break;

					default:
						throw new ApplicationException("Unhandled property type");
				}

				if (classDesc.properties[i].field != null)
					classDesc.properties[i].field.SetValue(iv, val);
			}

			pi.iv = iv;
			m_pendingIntervals[ivId] = pi;

			pi.sink.OnBegunInterval(ivId, iv);
		}

		private void ReadModifyInterval(Int64 timeStamp, int eventLength, IBinaryLogDataStream stream)
		{
			UInt64 ivId = stream.ReadUInt64();
			UInt32 classId = stream.ReadUInt32();
			UInt32 field = stream.ReadUInt32();
			UInt32 fieldId = field & 0x7fffffff;
			UInt32 splitInterval = field & 0x80000000;

			byte[] values = stream.ReadBytes(eventLength - 16);

			if (m_pendingIntervals.ContainsKey(ivId))
			{
				EventClassDesc classDesc = m_classDescs[classId];
				PendingInterval pi = m_pendingIntervals[ivId];
				Interval iv = pi.iv;

				if (splitInterval != 0)
				{
					iv.EndUs = timeStamp;

					Interval ivClone = (Interval)iv.Clone();

					pi.sink.OnFinalisedInterval(ivId, iv, true);

					iv = ivClone;
					pi.iv = iv;
					iv.StartUs = timeStamp;
					iv.EndUs = Int64.MaxValue;
				}

				EEndian srcEndian = stream.Endianness;

				object val = null;
				switch (classDesc.properties[fieldId].type)
				{
					case EFrameElementType.Float:
						val = EndianBitConverter.ToSingle(values, 0, srcEndian);
						break;

					case EFrameElementType.Int:
						val = EndianBitConverter.ToInt32(values, 0, srcEndian);
						break;

					case EFrameElementType.String:
						{
							int nullTerm = Array.IndexOf<byte>(values, 0, 0);
							string s = System.Text.Encoding.ASCII.GetString(values, 0, nullTerm);
							val = s;
						}
						break;

					case EFrameElementType.Int64:
						val = EndianBitConverter.ToInt64(values, 0, srcEndian);
						break;

					default:
						throw new ApplicationException("Unhandled property type");
				}

				if (classDesc.properties[fieldId].field != null)
					classDesc.properties[fieldId].field.SetValue(iv, val);

				if (splitInterval != 0)
				{
					pi.sink.OnBegunInterval(ivId, iv);
				}
			}
			else
			{
				//throw new ApplicationException("Unknown interval");
			}
		}

		private void ReadModifyIntervalBit(Int64 timeStamp, int eventLength, IBinaryLogDataStream stream)
		{
			UInt64 ivId = stream.ReadUInt64();
			UInt32 classId = stream.ReadUInt32();
			UInt32 field = stream.ReadUInt32();
			UInt32 fieldId = field & 0x7fffffff;
			UInt32 splitInterval = field & 0x80000000;

			byte[] values = stream.ReadBytes(eventLength - 16);

			if (m_pendingIntervals.ContainsKey(ivId))
			{
				EventClassDesc classDesc = m_classDescs[classId];
				PendingInterval pi = m_pendingIntervals[ivId];
				Interval iv = pi.iv;

				if (splitInterval != 0)
				{
					iv.EndUs = timeStamp;

					Interval ivClone = (Interval)iv.Clone();

					pi.sink.OnFinalisedInterval(ivId, iv, true);

					iv = ivClone;
					pi.iv = iv;
					iv.StartUs = timeStamp;
					iv.EndUs = Int64.MaxValue;
				}

				EEndian srcEndian = stream.Endianness;

				object valMask = null, valOr = null;
				switch (classDesc.properties[fieldId].type)
				{
					case EFrameElementType.Int:
						valMask = EndianBitConverter.ToInt32(values, 0, srcEndian);
						valOr = EndianBitConverter.ToInt32(values, 4, srcEndian);
						break;

					case EFrameElementType.Int64:
						valMask = EndianBitConverter.ToInt64(values, 0, srcEndian);
						valOr = EndianBitConverter.ToInt64(values, 8, srcEndian);
						break;

					default:
						throw new ApplicationException("Unhandled property type");
				}

				if (classDesc.properties[fieldId].field != null)
				{
					object oldVal = classDesc.properties[fieldId].field.GetValue(iv);

					switch (classDesc.properties[fieldId].type)
					{
						case EFrameElementType.Int:
							classDesc.properties[fieldId].field.SetValue(iv, ((int)oldVal & (int)valMask) | ((int)valOr));
							break;

						case EFrameElementType.Int64:
							classDesc.properties[fieldId].field.SetValue(iv, ((long)oldVal & (long)valMask) | ((long)valOr));
							break;
					}
				}

				if (splitInterval != 0)
				{
					pi.sink.OnBegunInterval(ivId, iv);
				}
			}
			else
			{
				//throw new ApplicationException("Unknown interval");
			}
		}

		private void ReadEndInterval(Int64 timeStamp, int eventLength, IBinaryLogDataStream stream)
		{
			UInt64 ivId = stream.ReadUInt64();

			if (m_pendingIntervals.ContainsKey(ivId))
			{
				PendingInterval pi = m_pendingIntervals[ivId];
				Interval iv = pi.iv;

				iv.EndUs = timeStamp;
				pi.sink.OnFinalisedInterval(ivId, iv, false);

				m_pendingIntervals.Remove(ivId);
			}
		}

		class PendingInterval
		{
			public UInt32 classId;

			public Interval iv;
			public IIntervalSink sink;
		}
	}
}
