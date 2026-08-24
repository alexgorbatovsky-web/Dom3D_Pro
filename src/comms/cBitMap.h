#pragma once

//-----------------------------------------------------------------------------
// cBitMap
//-----------------------------------------------------------------------------
class cBitMap {
public:
	cBitMap();
	explicit cBitMap(const int BitCount);
	~cBitMap();
	
	void Copy(const cBitMap &Src);
	
	void Set(const int BitIndex, const int Count = 1);
	void Clear(const int BitIndex, const int Count = 1);
	void SetAll();
	void ClearAll();
	
	const bool operator [] (const int BitIndex) const;
	
	void SetBitCount(const int BitCount);
	int GetBitCount() const;
private:
	dword *m_Bits;
	int m_BitCount, m_DwordCount, m_DwordCapacity;
	
	void Null();
	void Free();

	void EnsureCapacity(const int DwordCapacity);
}; // cBitMap

// cBitMap::SetBitCount
inline void cBitMap::SetBitCount(const int BitCount) {
	cAssert(BitCount >= 0);
	int DwordCount = BitCount / 32 + (BitCount % 32 ? 1 : 0);
	EnsureCapacity(DwordCount);
	m_BitCount = BitCount;
	m_DwordCount = DwordCount;
}

// cBitMap::GetBitCount
inline int cBitMap::GetBitCount() const {
	return m_BitCount;
}

//-----------------------------------------------------------------------------
// cBitMap::EnsureCapacity
//-----------------------------------------------------------------------------
inline void cBitMap::EnsureCapacity(const int DwordCapacity) {
	if(DwordCapacity <= m_DwordCapacity) {
		return;
	}
	
	int i;
	dword *B = m_Bits;
	
	m_DwordCapacity = DwordCapacity;
	if(m_DwordCapacity < m_DwordCount) {
		m_DwordCount = m_DwordCapacity;
		m_BitCount = 32 * m_DwordCount;
	}
	
	m_Bits = new dword[m_DwordCapacity];
	for(i = 0; i < m_DwordCount; i++) {
		m_Bits[i] = B[i];
	}
	
	if(B != nullptr) {
		delete[] B;
	}
} // cBitMap::EnsureCapacity

// cBitMap.ctor : ()
inline cBitMap::cBitMap() {
	Null();
}

// cBitMap.ctor : (const int)
inline cBitMap::cBitMap(const int BitCount) {
	Null();
	SetBitCount(BitCount);
}

// cBitMap.dtor
inline cBitMap::~cBitMap() {
	Free();
}

// cBitMap::Null
inline void cBitMap::Null() {
	m_Bits = nullptr;
	m_BitCount = 0;
	m_DwordCount = 0;
	m_DwordCapacity = 0;
}

// cBitMap::Free
inline void cBitMap::Free() {
	if(m_Bits != nullptr) {
		delete[] m_Bits;
	}
	Null();
}

// cBitMap::Copy
inline void cBitMap::Copy(const cBitMap &Src) {
	Free();

	m_BitCount = Src.m_BitCount;
	m_DwordCount = Src.m_DwordCount;
	m_DwordCapacity = Src.m_DwordCapacity;
	
	if(m_DwordCount > 0) {
		m_Bits = new dword[m_DwordCount];
		memcpy(m_Bits, Src.m_Bits, sizeof(dword) * m_DwordCount);
	}
}

// cBitMap::Set
inline void cBitMap::Set(const int BitIndex, const int Count) {
	cAssert(BitIndex >= 0);
	cAssert(BitIndex < m_BitCount);
	cAssert(BitIndex + Count <= m_BitCount);
	int i;
	for(i = 0; i < Count; i++) {
		m_Bits[(BitIndex + i) >> 5] |= 1 << ((BitIndex + i) & 0x1f);
	}
}

// cBitMap::Clear
inline void cBitMap::Clear(const int BitIndex, const int Count) {
	cAssert(BitIndex >= 0);
	cAssert(BitIndex < m_BitCount);
	cAssert(BitIndex + Count <= m_BitCount);
	int i;
	for(i = 0; i < Count; i++) {
		m_Bits[(BitIndex + i) >> 5] &= ~(1 << ((BitIndex + i) & 0x1f));
	}
}

// cBitMap::SetAll
inline void cBitMap::SetAll() {
	if(m_DwordCount > 0) {
		memset(m_Bits, -1, m_DwordCount * sizeof(dword));
	}
}

// cBitMap::ClearAll
inline void cBitMap::ClearAll() {
	if(m_DwordCount > 0) {
		memset(m_Bits, 0, m_DwordCount * sizeof(dword));
	}
}

// cBitMap::operator []
inline const bool cBitMap::operator [] (const int BitIndex) const {
	cAssert(BitIndex >= 0);
	cAssert(BitIndex < m_BitCount);
	return m_Bits[BitIndex >> 5] & 1 << (BitIndex & 0x1f) ? true : false;
}
