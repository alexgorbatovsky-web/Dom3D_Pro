#pragma once

//-----------------------------------------------------------------------------
// cHash
//-----------------------------------------------------------------------------
class cHash {
public:
	enum Constants {
		HASH_DEF_SIZE = 1024,
		INDEX_GROW_BY = 1024
	};
	
	// Constructors:
	cHash();
	cHash(const int nInitialHashSize, const int nInitialIndexSize);
	// Destructor:
	~cHash();
	
	// Returns total size of allocated memory.
	size_t Allocated() const;
	// Returns total size of allocated memory including size of this cHash instance.
	size_t Size() const;

	// Assignment operator:
	cHash & operator = (const cHash &src);

	void Add(const int key, const int index); // Adds a new index to the hash.
	void Remove(const int key, const int index); // Removes an index from the hash.
	int First(const int key) const; // Gets the first index from the hash. Returns -1 if empty hash entry.
	// Gets the next index from the hash. Returns -1 if at the end of the hash chain.
	int Next(const int index) const;
	void InsertIndex(const int key, const int index);
	void RemoveIndex(const int key, const int index);

	void Clear();
	void Clear(const int nNewHashSize, const int nNewIndexSize);
	int GetHashSize() const;
	int GetIndexSize() const;
	void SetGrowIndexBy(const int nNewGrowIndexBy);
	void Free();
	void ResizeIndex(const int nNewIndexSize);
	int CalcSpread() const; // Calcs the spread over the hash table in the range [0 .. 100].

	// Key generators:
	int GenerateKey(const int n) const;
	int GenerateKey(const int n1, const int n2) const;
	int GenerateKey(const char *Str, const bool NoCase) const;
	int GenerateKey(const cVec3 &u) const;
private:
	int *m_pHash, *m_pIndexChain;
	int m_nHashSize, m_nIndexSize, m_nGrowIndexBy, m_HashMask, m_LookupMask;
	static int INVALID_INDEX[1];
	void Init(const int nInitialHashSize, const int nInitialIndexSize);
	void Allocate(const int nNewHashSize, const int nNewIndexSize);
};

// cHash::cHash
inline cHash::cHash() {
	Init(HASH_DEF_SIZE, INDEX_GROW_BY);
}

// cHash::cHash(const int, const int)
inline cHash::cHash(const int nInitialHashSize, const int nInitialIndexSize) {
	Init(nInitialHashSize, nInitialIndexSize);
}

// cHash::~cHash
inline cHash::~cHash(void) {
	Free();
}

// cHash::Allocated
inline size_t cHash::Allocated() const {
	if(m_pHash == INVALID_INDEX && m_pIndexChain == INVALID_INDEX) return 0;
	return m_nHashSize * sizeof(int) + m_nIndexSize * sizeof(int);
}

// cHash::Size
inline size_t cHash::Size() const {
	return sizeof(*this) + Allocated();
}

// cHash::operator =
inline cHash &cHash::operator = (const cHash &src) {
	m_nGrowIndexBy = src.m_nGrowIndexBy;
	m_HashMask = src.m_HashMask;
	m_LookupMask = src.m_LookupMask;

	if(src.m_LookupMask == 0) {
		m_nHashSize = src.m_nHashSize;
		m_nIndexSize = src.m_nIndexSize;
		Free();
	}
	else {
		if(src.m_nHashSize != m_nHashSize || m_pHash == INVALID_INDEX) {
			if(m_pHash != INVALID_INDEX) delete[] m_pHash;
			m_nHashSize = src.m_nHashSize;
			m_pHash = new int[m_nHashSize];
		}
		if(src.m_nIndexSize != m_nIndexSize || m_pIndexChain == INVALID_INDEX) {
			if(m_pIndexChain != INVALID_INDEX) delete[] m_pIndexChain;
			m_nIndexSize = src.m_nIndexSize;
			m_pIndexChain = new int[m_nIndexSize];
		}
		memcpy(m_pHash, src.m_pHash, m_nHashSize * sizeof(m_pHash[0]));
		memcpy(m_pIndexChain, src.m_pIndexChain, m_nIndexSize * sizeof(m_pIndexChain[0]));
	}
	return *this;
}

// cHash::Add
inline void cHash::Add(const int key, const int index) {
	cAssert(index >= 0);

	if(m_pHash == INVALID_INDEX)
		Allocate(m_nHashSize, index >= m_nIndexSize ? index + 1 : m_nIndexSize);
	else if(index >= m_nIndexSize) ResizeIndex(index + 1);
	
	int h = key & m_HashMask;
	m_pIndexChain[index] = m_pHash[h];
	m_pHash[h] = index;
}

// cHash::Remove
inline void cHash::Remove(const int key, const int index) {
	if(m_pHash == INVALID_INDEX) return;

	int h = key & m_HashMask;
	if(m_pHash[h] == index) m_pHash[h] = m_pIndexChain[index];
	else {
		for(int i = m_pHash[h]; i != -1; i = m_pIndexChain[i]) {
			if(m_pIndexChain[i] == index) {
				m_pIndexChain[i] = m_pIndexChain[index];
				break;
			}
		}
	}
	m_pIndexChain[index] = -1;
}

// cHash::First
inline int cHash::First(const int key) const {
	return m_pHash[key & m_HashMask & m_LookupMask];
}

// cHash::Next
inline int cHash::Next(const int index) const {
	cAssert(index >= 0);
	cAssert(index < m_nIndexSize);
	return m_pIndexChain[index & m_LookupMask];
}

// cHash::InsertIndex
inline void cHash::InsertIndex(const int key, const int index) {
	if(m_pHash != INVALID_INDEX) {
		int max = index, i;
		for(i = 0; i < m_nHashSize; i++) {
			if(m_pHash[i] >= index) {
				m_pHash[i]++;
				if(m_pHash[i] > max) max = m_pHash[i];
			}
		}
		for(i = 0; i < m_nIndexSize; i++) {
			if(m_pIndexChain[i] >= index) {
				m_pIndexChain[i]++;
				if(m_pIndexChain[i] > max) max = m_pIndexChain[i];
			}
		}
		if(max >= m_nIndexSize) ResizeIndex(max + 1);
		for(i = max; i > index; i--) m_pIndexChain[i] = m_pIndexChain[i - 1];
		m_pIndexChain[index] = -1;
	}
	Add(key, index);
}

// cHash::RemoveIndex
inline void cHash::RemoveIndex(const int key, const int index) {
	int max, i;

	Remove(key, index);
	if(m_pHash != INVALID_INDEX) {
		max = index;
		for(i = 0; i < m_nHashSize; i++) {
			if(m_pHash[i] >= index) {
				if(m_pHash[i] > max) max = m_pHash[i];
				m_pHash[i]--;
			}
		}
		for(i = 0; i < m_nIndexSize; i++) {
			if(m_pIndexChain[i] >= index) {
				if(m_pIndexChain[i] > max) max = m_pIndexChain[i];
				m_pIndexChain[i]--;
			}
		}
		for(i = index; i < max; i++) m_pIndexChain[i] = m_pIndexChain[i + 1];
		m_pIndexChain[max] = -1;
	}
}

// cHash::Clear
inline void cHash::Clear() {
	if(m_pHash != INVALID_INDEX)
		memset(m_pHash, 0xFF, m_nHashSize * sizeof(m_pHash[0]));
}

// cHash::Clear(const int, const int)
inline void cHash::Clear(const int nNewHashSize, const int nNewIndexSize) {
	Free();
	m_nHashSize = nNewHashSize;
	m_nIndexSize = nNewIndexSize;
}

// cHash::GetHashSize
inline int cHash::GetHashSize() const {
	return m_nHashSize;
}

// cHash::GetIndexSize
inline int cHash::GetIndexSize() const {
	return m_nIndexSize;
}

// cHash::SetGrowIndexBy
inline void cHash::SetGrowIndexBy(const int nNewGrowIndexBy) {
	cAssert(nNewGrowIndexBy > 0);
	m_nGrowIndexBy = nNewGrowIndexBy;
}

// cHash::GenerateKey(const int)
inline int cHash::GenerateKey(const int n) const {
	return n & m_HashMask;
}

// cHash::GenerateKey(const int, const int)
inline int cHash::GenerateKey(const int n1, const int n2) const {
	return (n1 + n2) & m_HashMask;
}
