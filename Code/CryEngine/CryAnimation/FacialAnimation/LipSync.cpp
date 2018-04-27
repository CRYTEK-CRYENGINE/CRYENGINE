// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "LipSync.h"

#include <CryAnimation/ICryAnimation.h>
#include "../CharacterInstance.h"
#include "FaceAnimation.h"
#include "FacialInstance.h"
#include "CharacterManager.h"

static struct
{
	const char* name;
	const char* desc;
	int         sapi_id;
} s_english_phonemes[] =
{
	{ "-",  "syllable",                            1  },
	{ "!",  "Sentence terminator",                 2  },
	{ "&",  "Sentence terminator",                 2  },
	{ ",",  "Sentence terminator (comma)",         2  },
	{ ".",  "Sentence terminator (period)",        2  },
	{ "?",  "Sentence terminator (question mark)", 2  },
	{ "_",  "Silence (underscore)",                7  },

	{ "aa", "father",                              10 },
	{ "ae", "cat",                                 11 },
	{ "ah", "cut",                                 12 },
	{ "ao", "dog",                                 13 },
	{ "aw", "foul",                                14 },
	{ "ax", "ago",                                 15 },
	{ "ay", "bite",                                16 },
	{ "b",  "big",                                 17 },
	{ "ch", "chin",                                18 },
	{ "d",  "dig",                                 19 },
	{ "dh", "then",                                20 },
	{ "eh", "pet",                                 21 },
	{ "er", "fur",                                 22 },
	{ "ey", "ate",                                 23 },
	{ "f",  "fork",                                24 },
	{ "g",  "gut",                                 25 },
	{ "h",  "help",                                26 },
	{ "ih", "fill",                                27 },
	{ "iy", "feel",                                28 },
	{ "jh", "joy",                                 29 },
	{ "k",  "cut",                                 30 },
	{ "l",  "lid",                                 31 },
	{ "m",  "mat",                                 32 },
	{ "n",  "no",                                  33 },
	{ "ng", "sing",                                34 },
	{ "ow", "go",                                  35 },
	{ "oy", "toy",                                 36 },
	{ "p",  "put",                                 37 },
	{ "r",  "red",                                 38 },
	{ "s",  "sit",                                 39 },
	{ "sh", "she",                                 40 },
	{ "t",  "talk",                                41 },
	{ "th", "thin",                                42 },
	{ "uh", "book",                                43 },
	{ "uw", "too",                                 44 },
	{ "v",  "vat",                                 45 },
	{ "w",  "with",                                46 },
	{ "y",  "yard",                                47 },
	{ "z",  "zap",                                 48 },
	{ "zh", "pleasure",                            49 },

	/*
	   SYM Example PhoneID
	   - syllable boundary (hyphen) 1
	   ! Sentence terminator (exclamation mark) 2
	   & word boundary 3
	   , Sentence terminator (comma) 4
	   . Sentence terminator (period) 5
	   ? Sentence terminator (question mark) 6
	   _ Silence (underscore) 7
	   1 Primary stress 8
	   2 Secondary stress 9
	   aa father 10
	   ae cat 11
	   ah cut 12
	   ao dog 13
	   aw foul 14
	   ax ago 15
	   ay bite 16
	   b big 17
	   ch chin 18
	   d dig 19
	   dh then 20
	   eh pet 21
	   er fur 22
	   ey ate 23
	   f fork 24
	   g gut 25
	   h help 26
	   ih fill 27
	   iy feel 28
	   jh joy 29
	   k cut 30
	   l lid 31
	   m mat 32
	   n no 33
	   ng sing 34
	   ow go 35
	   oy toy 36
	   p put 37
	   r red 38
	   s sit 39
	   sh she 40
	   t talk 41
	   th thin 42
	   uh book 43
	   uw too 44
	   v vat 45
	   w with 46
	   y yard 47
	   z zap 48
	   zh pleasure 49
	 */
};

inline bool phoneme_less(const SPhoneme& ph1, const SPhoneme& ph2)
{
	return stricmp(ph1.ASCII, ph2.ASCII) < 0;
}

//////////////////////////////////////////////////////////////////////////
CPhonemesLibrary::CPhonemesLibrary()
{
	int nPhonemes = CRY_ARRAY_COUNT(s_english_phonemes);
	m_phonemes.reserve(nPhonemes);
	for (int i = 0; i < nPhonemes; i++)
	{
		SPhoneme ph;
		memset(ph.ASCII, 0, sizeof(ph.ASCII));
		cry_strcpy(ph.ASCII, s_english_phonemes[i].name);
		ph.description = s_english_phonemes[i].desc;
		m_phonemes.push_back(ph);
	}
	std::sort(m_phonemes.begin(), m_phonemes.end(), phoneme_less);
}

//////////////////////////////////////////////////////////////////////////
int CPhonemesLibrary::GetPhonemeCount() const
{
	return m_phonemes.size();
}

/////////////////////////////////////////////////////////////////////////
bool CPhonemesLibrary::GetPhonemeInfo(int nIndex, SPhonemeInfo& phoneme)
{
	if (nIndex >= 0 && nIndex < (int)m_phonemes.size())
	{
		memcpy(phoneme.ASCII, m_phonemes[nIndex].ASCII, sizeof(phoneme.ASCII));
		phoneme.codeIPA = m_phonemes[nIndex].codeIPA;
		phoneme.description = m_phonemes[nIndex].description.c_str();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
SPhoneme& CPhonemesLibrary::GetPhoneme(int nIndex)
{
	assert(nIndex >= 0 && nIndex < (int)m_phonemes.size());
	return m_phonemes[nIndex];
}

//////////////////////////////////////////////////////////////////////////
void CPhonemesLibrary::LoadPhonemes(const char* filename)
{
}

//////////////////////////////////////////////////////////////////////////
int CPhonemesLibrary::FindPhonemeByName(const char* sPhonemeName)
{
	SPhoneme findPhoneme;
	cry_strcpy(findPhoneme.ASCII, sPhonemeName);
	std::vector<SPhoneme>::iterator it = std::lower_bound(m_phonemes.begin(), m_phonemes.end(), findPhoneme, phoneme_less);
	if (it != m_phonemes.end())
		return (int)(it - m_phonemes.begin());

	return -1;
}

//////////////////////////////////////////////////////////////////////////
// CFacialSentence
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CFacialSentence::CFacialSentence()
	: m_nValidateID(1)
{
}

//////////////////////////////////////////////////////////////////////////
IPhonemeLibrary* CFacialSentence::GetPhonemeLib()
{
	return gEnv->pCharacterManager->GetIFacialAnimation()->GetPhonemeLibrary();
}

//////////////////////////////////////////////////////////////////////////
bool CFacialSentence::GetPhoneme(int index, Phoneme& ph)
{
	if (index < 0 || index >= (int)m_phonemes.size())
		return false;

	ph = m_phonemes[index];
	return true;
}

//////////////////////////////////////////////////////////////////////////
int CFacialSentence::AddPhoneme(const Phoneme& ph)
{
	m_phonemes.push_back(ph);
	++m_nValidateID;
	return (int)m_phonemes.size() - 1;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialSentence::GetWord(int index, Word& wrd)
{
	if (index < 0 || index >= (int)m_words.size())
		return false;

	wrd.startTime = m_words[index].startTime;
	wrd.endTime = m_words[index].endTime;
	wrd.sWord = m_words[index].text;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSentence::AddWord(const Word& wrd)
{
	WordRec w;
	w.startTime = wrd.startTime;
	w.endTime = wrd.endTime;
	w.text = wrd.sWord;
	m_words.push_back(w);
	++m_nValidateID;
}

//////////////////////////////////////////////////////////////////////////
int CFacialSentence::GetPhonemeFromTime(int timeMs, int nFirst)
{
	if (nFirst > (int)m_phonemes.size() - 1)
		nFirst = 0;
	if (nFirst < 0)
		nFirst = 0;
	if (nFirst > 0)
	{
		if (m_phonemes[nFirst].time > timeMs)
			nFirst = 0;
	}
	for (int i = nFirst; i < (int)m_phonemes.size(); i++)
	{
		if (timeMs < m_phonemes[i].time)
			break;
		if (timeMs >= m_phonemes[i].time && timeMs <= m_phonemes[i].endtime)
		{
			return i;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialSentence::GetPhonemeInfo(int phonemeId, SPhonemeInfo& phonemeInfo) const
{
	if (!gEnv->pCharacterManager->GetIFacialAnimation()->GetPhonemeLibrary()->GetPhonemeInfo(phonemeId, phonemeInfo))
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSentence::Serialize(XmlNodeRef& node, bool bLoading)
{
	CPhonemesLibrary* pPhonemeLib = (CPhonemesLibrary*)gEnv->pCharacterManager->GetIFacialAnimation()->GetPhonemeLibrary();
	if (bLoading)
	{
		m_phonemes.clear();

		m_text = node->getAttr("Text");

		m_phonemes.clear();
		m_words.clear();

		XmlNodeRef wordsNode = node->findChild("Words");
		XmlNodeRef phonemesNode = node->findChild("Phonemes");

		if (wordsNode)
		{
			// Loading.
			int num = wordsNode->getChildCount();
			for (int i = 0; i < num; i++)
			{
				XmlNodeRef wordNode = wordsNode->getChild(i);

				WordRec wrd;
				wrd.text = wordNode->getAttr("Word");
				wordNode->getAttr("Start", wrd.startTime);
				wordNode->getAttr("End", wrd.endTime);

				m_words.push_back(wrd);
			}
		}
		if (phonemesNode)
		{
			// Loading.
			string strall = phonemesNode->getContent();
			int curPos = 0;
			string key = strall.Tokenize(",", curPos);
			while (!key.empty())
			{
				char sPhoneme[128];
				int start = 0, end = 0;
				PREFAST_SUPPRESS_WARNING(6031) sscanf(key, "%d:%d:%127s", &start, &end, sPhoneme);
				key = strall.Tokenize(",", curPos);

				Phoneme ph;
				cry_strcpy(ph.phoneme, sPhoneme);
				ph.intensity = 1;
				ph.time = start;
				ph.endtime = end;
				m_phonemes.push_back(ph);
			}
		}

		++m_nValidateID;
	}
	else
	{
		// Saving.

		int i;
		node->setAttr("Text", m_text);

		XmlNodeRef wordsNode = node->newChild("Words");

		int numWords = (int)m_words.size();
		for (i = 0; i < numWords; i++)
		{
			XmlNodeRef wordNode = wordsNode->newChild("Word");
			wordNode->setAttr("Word", m_words[i].text);
			wordNode->setAttr("Start", m_words[i].startTime);
			wordNode->setAttr("End", m_words[i].endTime);
		}

		// Saving.
		int numPhonemes = (int)m_phonemes.size();
		if (numPhonemes > 0)
		{
			XmlNodeRef phonemesNode = node->newChild("Phonemes");
			string strall;
			string skey;
			for (i = 0; i < numPhonemes; i++)
			{
				skey.Format("%d:%d:%s", m_phonemes[i].time, m_phonemes[i].endtime, m_phonemes[i].phoneme);
				strall += skey;
				if (i < numPhonemes - 1)
					strall += ",";
			}
			phonemesNode->setContent(strall);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//#define USE_LIPSYNC_V2
#if defined(USE_LIPSYNC_V2)
inline float EvalGaussian(float x)
{
	const float scale = 1.0f / sqrtf(2 * 3.14159f);
	return scale * expf(-x * x / 2);
}
#endif //defined(USE_LIPSYNC_V2)
int CFacialSentence::Evaluate(float fTime, float fInputPhonemeStrength, int maxSamples, ChannelSample* samples)
{
	int numSamples = 0;

#if defined(USE_LIPSYNC_V2)
	const float timeInMilliseconds = fTime * 1000.0f;
	const float phonemeGlobalScale = max(0.0f, min(1.0f, fInputPhonemeStrength));
	const float minSpread = 150.0f;

	string debugText;

	// Declare an array of structures to hold our calculations about the phonemes to play. To simplify references
	// to the previous and next phonemes, add a dummy entry to the beginning and end.
	struct Entry
	{
		const char* name;
		float       intensity;
		float       times[2];

		float       modeTime;
		float       scales[2];
		float       spreads[2];
	};

	enum {MAX_ENTRIES = 3};
	Entry entryBuffer[MAX_ENTRIES + 2];
	static const Entry dummyEntry = { 0, 0.0f, 0.0f, 0.0f };
	Entry* const entries = &entryBuffer[1];
	entries[-1] = dummyEntry;
	int entryCount = 0;

	for (int phonemeIndex = 0, phonemeCount = GetPhonemeCount(); phonemeIndex < phonemeCount; ++phonemeIndex)
	{
		const CFacialSentence::Phoneme& currentPhoneme = GetPhoneme(phonemeIndex);
		if (currentPhoneme.time < timeInMilliseconds && currentPhoneme.endtime > timeInMilliseconds)
		{
			for (int phonemeOffset = -1; phonemeOffset <= 1; ++phonemeOffset)
			{
				if (phonemeIndex + phonemeOffset >= 0 && phonemeIndex + phonemeOffset < phonemeCount)
				{
					const CFacialSentence::Phoneme& phoneme = GetPhoneme(phonemeIndex + phonemeOffset);
					entries[entryCount].name = phoneme.phoneme;
					entries[entryCount].times[0] = float(phoneme.time);
					entries[entryCount].times[1] = float(phoneme.endtime);
					entries[entryCount].intensity = phoneme.intensity;
					++entryCount;
				}
			}
		}
	}

	entries[entryCount] = dummyEntry;
	entries[-1].times[1] = entries[0].times[0];
	entries[entryCount].times[0] = entries[entryCount - 1].times[1];

	// Now that we have a nice list of entries to process, go through them and calculate the curves for them.
	for (int entryIndex = 0; entryIndex < entryCount; ++entryIndex)
	{
		Entry& entry = entries[entryIndex];

		entry.modeTime = (entry.times[0] + entry.times[1]) * 0.5f;

		for (int direction = 0; direction < 2; ++direction)
		{
			float tailLength = fabs(entry.times[direction] - entry.modeTime);
			entry.spreads[direction] = max(tailLength / 0.8f, 0.01f);
			;
			entry.scales[direction] = 1.0f / (EvalGaussian(0.0f) / entry.spreads[direction]);
		}
	}

	// Go through them and apply the phonemes using the values we have calculated.
	for (int entryIndex = 0; entryIndex < entryCount; ++entryIndex)
	{
		const Entry& entry = entries[entryIndex];

		// Evaluate the phoneme strength at this point.
		int direction = (timeInMilliseconds > entry.modeTime ? 1 : 0);
		float phonemeStrength = EvalGaussian(fabs(timeInMilliseconds - entry.modeTime) / entry.spreads[direction]) * entry.scales[direction] / entry.spreads[direction];

		if (numSamples < maxSamples)
		{
			samples[numSamples].phoneme = entry.name;

			samples[numSamples].strength = phonemeStrength * phonemeGlobalScale;
			if (samples[numSamples].strength > 1.0f)
				samples[numSamples].strength = 1.0f;
		}

		++numSamples;
	}
#else //defined(USE_LIPSYNC_V2)

	float fPhonemeStrength = fInputPhonemeStrength;
	if (fPhonemeStrength > 1.0f)
		fPhonemeStrength = 1.0f;
	if (fPhonemeStrength < 0.0f)
		fPhonemeStrength = 0.0f;

	string debugText;

	float time = fTime * 1000.0f + Console::GetInst().ca_lipsync_phoneme_offset;
	float fFadeTime = (float) Console::GetInst().ca_lipsync_phoneme_crossfade;

	int nPhonemes = GetPhonemeCount();
	for (int i = 0; i < nPhonemes; i++)
	{
		CFacialSentence::Phoneme& ph = GetPhoneme(i);

		float fPossibleFadeTime = fFadeTime;

		if (i > 0)
		{
			// make sure this is not time dependent as before, as this
			// leads to discontinuities in the sequence
			CFacialSentence::Phoneme& ph0 = GetPhoneme(i - 1);
			float fToPrevPhonemeLength = (float) (ph.time - ph0.time);
			float fThisPhonemeLength = (float) (ph.endtime - ph.time);
			fPossibleFadeTime = min(fToPrevPhonemeLength, fThisPhonemeLength);
		}

		// do fading with at least fFadeTime, when fPossibleFadeTime is smaller
		// than fFadeTime, dont go up to maximum value
		// but scale maximum strength by fPossibleFadeTime / fFadeTime

		float ft = max(fFadeTime, fPossibleFadeTime);

		float t1 = (ph.time - time) / ft;
		float t2 = (ph.endtime - time) / ft;

		if (t1 < 1.0f && t2 > 0.0f)
		{
			if (t2 > 1.0f) t2 = 1.0f;
			if (t1 < 0.0f) t1 = 0.0f;
			float fFadeStrength = (t2 - t1);

			if (fPhonemeStrength > 1.0f)
				fPhonemeStrength = 1.0f;

			if (numSamples < maxSamples)
			{
				samples[numSamples].phoneme = ph.phoneme;
				float fScale = 1.0f;
				if (fPossibleFadeTime < fFadeTime)
				{
					fScale = fPossibleFadeTime / fFadeTime;
				}
				samples[numSamples].strength = fPhonemeStrength * fFadeStrength * fScale;
				if (samples[numSamples].strength > 1.0f)
					samples[numSamples].strength = 1.0f;
			}

			++numSamples;
		}

		// Must be at the end of loop.
		if (ph.time > time)
			break;
	}
#endif //defined(USE_LIPSYNC_V2)

	return numSamples;
}

void CFacialSentence::Animate(const QuatTS& rAnimLocationNext, CFacialAnimationContext* pAnimContext, float fTime, float fInputPhonemeStrength, const VectorSet<CFaceIdentifierHandle, SFaceIdentifierHandleLess>& overriddenPhonemes)
{
	ChannelSample samples[1000];
	enum {MAX_SAMPLES = CRY_ARRAY_COUNT(samples)};
	int sampleCount = Evaluate(fTime, fInputPhonemeStrength, MAX_SAMPLES, samples);
	sampleCount = min(sampleCount, int(MAX_SAMPLES));

	CFacialAnimation* pFaceAnim = g_pCharacterManager->GetFacialAnimation();
	if (!pFaceAnim)
		return;

	string debugText;

	for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
	{
		const ChannelSample& sample = samples[sampleIndex];
		IFacialEffector* pPhonemeEffector = 0;

		CFaceIdentifierHandle phonemeIdent = pFaceAnim->CreateIdentifierHandle(sample.phoneme);
		if (!pPhonemeEffector)
			pPhonemeEffector = pAnimContext->GetInstance()->FindEffector(phonemeIdent);

		// It is possible for the animator to override the lipsync with a manual channel. These channels are stored in the
		// 'BakedLipSync' folder. We should check whether this effector is referenced in that folder, and if so do nothing.
		bool manualOverride = overriddenPhonemes.find(phonemeIdent) != overriddenPhonemes.end();

		if (pPhonemeEffector && !manualOverride)
		{
			// Make a channel in animation context.
			SFacialEffectorChannel effch;
			effch.status = SFacialEffectorChannel::STATUS_ONE;
			effch.pEffector = (CFacialEffector*)pPhonemeEffector;
			effch.fWeight = sample.strength;
			effch.bTemporary = true;
			effch.bLipSync = true;
			pAnimContext->StartChannel(effch);

			if (Console::GetInst().ca_lipsync_debug)
			{
				string str;
				str.Format("%s:%.2f", sample.phoneme, effch.fWeight);
				debugText += str;
			}
		}
	}

	if (Console::GetInst().ca_lipsync_debug)
	{
		CCharInstance* pChar = pAnimContext->GetInstance()->GetMasterCharacter();
		Vec3 pos = rAnimLocationNext.t; //pChar->m_AnimCharLocation.t;
		int16 id = pChar->GetIDefaultSkeleton().GetJointIDByName("Bip01 Head");
		if (id >= 0)
			pos += pChar->GetISkeletonPose()->GetAbsJointByID(id).t;
		float color[4] = { 1, 1, 1, 1 };
		IRenderAuxText::DrawLabelEx(pos, 1.2f, color, true, true, debugText.c_str());
	}
}
