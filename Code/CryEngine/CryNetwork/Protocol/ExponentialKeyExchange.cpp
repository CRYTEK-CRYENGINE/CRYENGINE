// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExponentialKeyExchange.h"
#include <BigDigits/bigd.h>
#include <BigDigits/bigdigits.h>
#include <BigDigits/bigdRand.h>
// big digits defines uint32_t as unsigned long, which breaks compilation
#undef uint32_t
#include <CrySystem/ITimer.h>

#if ALLOW_ENCRYPTION

namespace
{
struct BIGD_COPY
{
private:
	BIGD_COPY() { copyright_notice(); }
	static BIGD_COPY s_singleton;
};

BIGD_COPY BIGD_COPY::s_singleton;
}

class CAutoBIGD
{
public:
	CAutoBIGD() : m_b(bdNew()) {}
	~CAutoBIGD() { bdFree(&m_b); }

	BIGD Get() { return m_b; }

private:
	BIGD m_b;
};

CExponentialKeyExchange::CExponentialKeyExchange()
{
}

CExponentialKeyExchange::~CExponentialKeyExchange()
{
}

int CExponentialKeyExchange::RandFunc(unsigned char* bytes, size_t nbytes, const unsigned char* seed, size_t slen)
{
	NET_ASSERT(slen % sizeof(uint32) == 0);
	CMTRand_int32 r((const uint32*)seed, slen / sizeof(uint32));
	while (nbytes >= sizeof(uint32))
	{
		*(uint32*)bytes = r.GenerateUint32();
		bytes += sizeof(uint32);
		nbytes -= sizeof(uint32);
	}
	if (nbytes)
	{
		uint32 last = r.GenerateUint32();
		unsigned char* p = (unsigned char*)&last;
		while (nbytes--)
			*bytes++ = *p++;
	}
	return 0;
}

void CExponentialKeyExchange::Generate(KEY_TYPE& gg, KEY_TYPE& pp, KEY_TYPE& AA)
{
	char s[256];

regenerate:         // LH : Should be safe to simply regenerate a new key
	uint64 seed = gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64();
	//	seed = 0x53275A-13-13;		// LH : This is a key that generates a 0 in e - which reproduces the infinite loop problem - if memory conditions are right

	// generate private key
	CAutoBIGD a;
	bdRandomSeeded(a.Get(), KEY_SIZE_IN_BITS, (unsigned char*)&seed, sizeof(seed), RandFunc);
	bdConvToDecimal(a.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: a=%s", s);
	bdConvToOctets(a.Get(), m_x.key, KEY_SIZE);

	// generate p
	CAutoBIGD p;
	seed += 13;
	bdGeneratePrime(p.Get(), KEY_SIZE_IN_BITS, 5, (unsigned char*)&seed, sizeof(seed), RandFunc);
	bdConvToDecimal(p.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: p=%s", s);
	bdConvToOctets(p.Get(), m_p.key, KEY_SIZE);

	// generate g
	CAutoBIGD q;
	seed += 13;
	bdGeneratePrime(q.Get(), KEY_SIZE_IN_BITS, 5, (unsigned char*)&seed, sizeof(seed), RandFunc);
	bdConvToDecimal(q.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: q=%s", s);
	CAutoBIGD p1;
	bdShortSub(p1.Get(), p.Get(), 1);
	CAutoBIGD t, r;
	bdDivide(t.Get(), r.Get(), p1.Get(), q.Get());
	bdConvToDecimal(t.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: t=%s", s);
	CAutoBIGD h;
	bdSetShort(h.Get(), 2);
	CAutoBIGD g;
	bdSetZero(g.Get());
	for (; bdCompare(h.Get(), p1.Get()) < 0; bdIncrement(h.Get()))
	{
		bdConvToDecimal(h.Get(), s, sizeof(s));
		//NetLog("CExponentialKeyExchange: h=%s", s);
		if (bdModExp(g.Get(), h.Get(), t.Get(), p.Get()) == -1)
		{
			goto regenerate;
		}
		bdConvToDecimal(g.Get(), s, sizeof(s));
		//NetLog("CExponentialKeyExchange: g=%s", s);
		if (bdShortCmp(g.Get(), 1) > 0)
			break;
	}

	NET_ASSERT(bdShortCmp(g.Get(), 1) > 0);
	bdConvToOctets(g.Get(), m_g.key, KEY_SIZE);

	// generate A
	CAutoBIGD A;
	bdModExp(A.Get(), g.Get(), a.Get(), p.Get());
	bdConvToDecimal(A.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: A=%s", s);
	bdConvToOctets(A.Get(), m_A.key, KEY_SIZE);

	memcpy(gg.key, m_g.key, KEY_SIZE);
	memcpy(pp.key, m_p.key, KEY_SIZE);
	memcpy(AA.key, m_A.key, KEY_SIZE);
}

void CExponentialKeyExchange::Generate(KEY_TYPE& BB, const KEY_TYPE& gg, const KEY_TYPE& pp, const KEY_TYPE& AA)
{
	memcpy(m_g.key, gg.key, KEY_SIZE);
	memcpy(m_p.key, pp.key, KEY_SIZE);
	memcpy(m_A.key, AA.key, KEY_SIZE);

	char s[256];

regenerate:     // LH : Should be safe to simply regenerate a new key
	uint64 seed = gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64();

	// generate private key
	CAutoBIGD b;
	bdRandomSeeded(b.Get(), KEY_SIZE_IN_BITS, (unsigned char*)&seed, sizeof(seed), RandFunc);
	bdConvToDecimal(b.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: b=%s", s);
	bdConvToOctets(b.Get(), m_x.key, KEY_SIZE);

	// calculate B
	CAutoBIGD g;
	bdConvFromOctets(g.Get(), m_g.key, KEY_SIZE);
	bdConvToDecimal(g.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: g=%s", s);
	CAutoBIGD p;
	bdConvFromOctets(p.Get(), m_p.key, KEY_SIZE);
	bdConvToDecimal(p.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: p=%s", s);
	CAutoBIGD B;
	if (bdModExp(B.Get(), g.Get(), b.Get(), p.Get()) == -1)
		goto regenerate;

	bdConvToDecimal(B.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: B=%s", s);
	bdConvToOctets(B.Get(), m_B.key, KEY_SIZE);

	// calculate key
	CAutoBIGD A;
	bdConvFromOctets(A.Get(), m_A.key, KEY_SIZE);
	bdConvToDecimal(A.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: A=%s", s);
	CAutoBIGD k;
	if (bdModExp(k.Get(), A.Get(), b.Get(), p.Get()))
		goto regenerate;

	bdConvToDecimal(k.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: k=%s", s);
	bdConvToOctets(k.Get(), m_k.key, KEY_SIZE);

	memcpy(BB.key, m_B.key, KEY_SIZE);
}

void CExponentialKeyExchange::Calculate(const KEY_TYPE& BB)
{
	memcpy(m_B.key, BB.key, KEY_SIZE);

	char s[256];

	CAutoBIGD B;
	bdConvFromOctets(B.Get(), m_B.key, KEY_SIZE);
	bdConvToDecimal(B.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: B=%s", s);
	CAutoBIGD a;
	bdConvFromOctets(a.Get(), m_x.key, KEY_SIZE);
	bdConvToDecimal(a.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: a=%s", s);
	CAutoBIGD p;
	bdConvFromOctets(p.Get(), m_p.key, KEY_SIZE);
	bdConvToDecimal(p.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: p=%s", s);
	CAutoBIGD k;
	bdModExp(k.Get(), B.Get(), a.Get(), p.Get());
	bdConvToDecimal(k.Get(), s, sizeof(s));
	//NetLog("CExponentialKeyExchange: k=%s", s);
	bdConvToOctets(k.Get(), m_k.key, KEY_SIZE);
}

const CExponentialKeyExchange::KEY_TYPE& CExponentialKeyExchange::GetKey() const
{
	return m_k;
}

void CExponentialKeyExchange::Test(IConsoleCmdArgs* pArgs)
{
	size_t rounds = 0;
	if (pArgs->GetArgCount() >= 2)
		rounds = atoi(pArgs->GetArg(1));
	if (0 == rounds)
		rounds = 1;

	for (size_t i = 0; i < rounds; ++i)
	{
		CExponentialKeyExchange alice;
		CExponentialKeyExchange::KEY_TYPE g, p, A;
		alice.Generate(g, p, A);

		CExponentialKeyExchange bob;
		CExponentialKeyExchange::KEY_TYPE B;
		bob.Generate(B, g, p, A);

		alice.Calculate(B);

		if (0 != memcmp(alice.GetKey().key, bob.GetKey().key, CExponentialKeyExchange::KEY_SIZE))
			return NetError("CExponentialKeyExchange: Test Failed");
	}

	NetLog("CExponentialKeyExchange: test was successful");
}

#endif
