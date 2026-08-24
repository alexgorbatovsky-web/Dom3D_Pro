#include "comms.h"

namespace comms {

int cHash::INVALID_INDEX[1] = {-1};

// cHash::Init
void cHash::Init(const int nInitialHashSize, const int nInitialIndexSize) {
	cAssert(cMath::IsPowerOfTwo(nInitialHashSize));

	m_nHashSize = nInitialHashSize;
	m_pHash = INVALID_INDEX;
	m_nIndexSize = nInitialIndexSize;
	m_pIndexChain = INVALID_INDEX;
	m_nGrowIndexBy = INDEX_GROW_BY;
	m_HashMask = m_nHashSize - 1;
	m_LookupMask = 0;
}

// cHash::Allocate
void cHash::Allocate(const int nNewHashSize, const int nNewIndexSize) {
	cAssert(cMath::IsPowerOfTwo(nNewHashSize));

	Free();
	m_nHashSize = nNewHashSize;
	m_pHash = new int[m_nHashSize];
	memset(m_pHash, 0xFF, m_nHashSize * sizeof(m_pHash[0]));
	m_nIndexSize = nNewIndexSize;
	m_pIndexChain = new int[m_nIndexSize];
	memset(m_pIndexChain, 0xFF, m_nIndexSize * sizeof(m_pIndexChain[0]));
	m_HashMask = m_nHashSize - 1;
	m_LookupMask = -1;
}

// cHash::Free
void cHash::Free() {
	if(m_pHash != INVALID_INDEX) {
		delete[] m_pHash;
		m_pHash = INVALID_INDEX;
	}
	if(m_pIndexChain != INVALID_INDEX) {
		delete[] m_pIndexChain;
		m_pIndexChain = INVALID_INDEX;
	}
	m_LookupMask = 0;
}

// cHash::ResizeIndex
void cHash::ResizeIndex(const int nNewIndexSize) {
	if(nNewIndexSize <= m_nIndexSize) return;

	int r = nNewIndexSize % m_nGrowIndexBy, nSize = 0;
	if(r == 0) nSize = nNewIndexSize;
	else nSize = nNewIndexSize + m_nGrowIndexBy - r;

	if(m_pIndexChain == INVALID_INDEX) {
		m_nIndexSize = nSize;
		return;
	}

	int *pOldIndexChain = m_pIndexChain;
	m_pIndexChain = new int[nSize];
	memcpy(m_pIndexChain, pOldIndexChain, m_nIndexSize * sizeof(int));
	memset(m_pIndexChain + m_nIndexSize, 0xFF, (nSize - m_nIndexSize) * sizeof(int));
	delete[] pOldIndexChain;
	m_nIndexSize = nSize;
}

// cHash::CalcSpread
int cHash::CalcSpread() const {
	if(m_pHash == INVALID_INDEX) return 100;

	int TotalItems = 0, i, index, d;
	int *pChainLengths = new int[m_nHashSize];
	memset(pChainLengths, 0, m_nHashSize * sizeof(int));
	for(i = 0; i < m_nHashSize; i++) {
		for(index = m_pHash[i]; index >= 0; index = m_pIndexChain[index]) pChainLengths[i]++;
		TotalItems += pChainLengths[i];
	}
	if(TotalItems <= 1) { // Hash is empty:
		delete[] pChainLengths;
		return 100;
	}
	int Average = TotalItems / m_nHashSize, Err = 0;
	for(i = 0; i < m_nHashSize; i++) {
		d = cMath::Abs(pChainLengths[i] - Average);
		if(d > 1) Err += d - 1;
	}
	delete[] pChainLengths;
	return 100 - (Err * 100 / TotalItems);
}

// cHash::GenerateKey(const char *, bool)
int cHash::GenerateKey(const char *Str, const bool NoCase) const {
	if(NoCase) {
		return cStr::GetHashCode(Str, true) & m_HashMask;
	} else {
		return cStr::GetHashCode(Str) & m_HashMask;
	}
}

// cHash::GenerateKey : int(const cVec3 &) const
int cHash::GenerateKey(const cVec3 &u) const {
	return (int(u.x) + int(u.y) + int(u.z)) & m_HashMask;
}

} // comms
