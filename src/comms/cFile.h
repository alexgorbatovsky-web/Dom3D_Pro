#pragma once

//*****************************************************************************
// cFileDisk
//*****************************************************************************
/**
 * \brief Class for direct disk file Input/Output operations.
 * * This class wraps standard C file operations (fopen, fread, fwrite)
 * to provide a convenient interface for reading and writing files directly to disk.
 */
class APICALL cFileDisk { /// Reads from and writes directly to disk
public:
	// cFileDisk.ctor
	cFileDisk() {
		Null();
	}
	// cFileDisk.dtor
	~cFileDisk() {
		Close();
	}

	// cFileDisk.OpenRead
	/**
	 * \brief Opens an existing file for reading.
	 * \param FilePn Path to the file.
	 * \return true if opened successfully.
	 */
	bool OpenRead(const char* FilePn) { // 0 == GetPos(), GetSizeBytes() >= GetPos(), IsEof() == (0 == GetSizeBytes())
		return Open(FilePn, Mode::Read);
	}

	// cFileDisk.CreateWrite
	/**
	 * \brief Creates a new file for writing.
	 * * If the file exists, it will be overwritten.
	 * \param FilePn Path to the file.
	 * \return true if created successfully.
	 */
	bool CreateWrite(const char* FilePn) { // 0 == GetPos(), 0 == GetSizeBytes(), true == IsEof()
		return Open(FilePn, Mode::Create);
	}

	// cFileDisk.OpenAppend
	/**
	 * \brief Opens an existing file for appending data to the end.
	 * \param FilePn Path to the file.
	 * \return true if opened successfully.
	 */
	bool OpenAppend(const char* FilePn) { // GetSizeBytes() == GetPos(), true == IsEof()
		return Open(FilePn, Mode::Append);
	}

	// cFileDisk.GetSizeBytes
	/** \brief Returns the total size of the file in bytes. */
	int64 GetSizeBytes() const {
		cAssert(m_File != nullptr);
		return m_SizeBytes;
	}

	// cFileDisk.GetPos
	/** \brief Returns the current position of the file pointer. */
	int64 GetPos() const;

	// cFileDisk.SetPos
	/** \brief Sets the file pointer to a specific absolute position. */
	void SetPos(const int64 Pos);

	// cFileDisk.SeekCur
	/** \brief Moves the file pointer relative to the current position. */
	void SeekCur(const int64 Offset);

	// cFileDisk.SeekEnd
	/** \brief Moves the file pointer to the end of the file. */
	void SeekEnd();

	// cFileDisk.IsEof
	/** \brief Checks if the file pointer is at the end of the file. */
	bool IsEof() {
		return (m_SizeBytes == GetPos());
	}

	// cFileDisk.ReadBytes
	/** * \brief Reads bytes from the file into a buffer.
	 * \param To Destination buffer.
	 * \param MaxSize Number of bytes to read.
	 * \return Number of bytes actually read (0 at EOF).
	 */
	size_t ReadBytes(void* To, const size_t MaxSize) {
		cAssert(m_File != nullptr);
		size_t BytesRead = 0;
		if (m_File != nullptr) {
			BytesRead = fread(To, 1, MaxSize, m_File);
		}
		return BytesRead;
	}

	// cFileDisk.WriteBytes
	/**
	 * \brief Writes bytes from a buffer to the file.
	 * \param From Source buffer.
	 * \param SizeBytes Number of bytes to write.
	 */
	void WriteBytes(const void* From, const size_t SizeBytes) {
		cAssert(m_File != nullptr);
		if (m_File != nullptr) {
			fwrite(From, 1, SizeBytes, m_File);
		}
	}

	// cFileDisk.Close
	/** \brief Closes the file handle. */
	void Close();

	// cFileDisk.GetFILE
	/** \brief Returns the underlying standard FILE pointer. */
	FILE* GetFILE() {
		return m_File;
	}
private:
	FILE* m_File;
	int64 m_SizeBytes;
	void Null() {
		m_File = nullptr;
		m_SizeBytes = 0;
	}
	struct Mode {
		enum Enum {
			Read, Create, Append
		};
	};
	bool Open(const char* FilePn, const Mode::Enum _Mode);
}; // cFileDisk

//*****************************************************************************
// cFile
//*****************************************************************************
/**
 * \brief Class representing an in-memory file or data buffer.
 * * Allows reading and writing various data types to a memory block similarly to a file stream.
 * * Handles endianness swapping if configured.
 */
class APICALL cFile {
public:
	cFile() : m_Pos(0) {
	}

	/** \brief Sets the internal data buffer from an existing list. */
	void Set(cList<byte>* Src, const char* FilePn = nullptr);

	/** \brief Copies data from a raw pointer into the internal buffer. */
	void Copy(const void* Src, const int Size, const char* FilePn = nullptr);

	/** \brief Clears the internal data buffer and resets position. */
	void Clear();

	const cStr& GetFilePn() const { return m_FilePn; }
	void SetFilePn(const char* FilePn) { m_FilePn = FilePn; }

	/** \brief Checks if the current position is at the end of the data buffer. */
	bool IsEof() const { return m_Pos >= m_Data.Count(); }

	/** \brief Returns the size of the data buffer. */
	size_t Size() const { return m_Data.Size(); }

	/** \brief Returns the current read/write position. */
	int GetPos() const { return m_Pos; }

	/**
	 * \brief Moves the data position pointer relative to the current location.
	 * \param Offset The offset in bytes.
	 * \return The new absolute position from the beginning of the data.
	 */
	int SeekCur(const int Offset) const;

	/**
	 * \brief Moves the data position pointer relative to the end of the data.
	 * \param Offset The offset in bytes (usually negative or zero).
	 * \return The new absolute position.
	 */
	int SeekEnd(const int Offset) const;

	/**
	 * \brief Sets the data position pointer to an absolute value.
	 * \param Pos The new position.
	 * \return The actual new position (clamped to valid range).
	 */
	int SetPos(const int Pos) const;

	/**
	 * \brief Sets the data position pointer, enlarging the file with zeros if "Pos" lies after EOF.
	 * \param Pos The new position.
	 * \return The new position.
	 */
	int SetPosMutable(const int Pos);

	/**
	 * \brief Reads raw bytes from the buffer.
	 * * Warning: only bytes reading / writing are endian independent.
	 * \param To Destination buffer.
	 * \param MaxSize Maximum bytes to read.
	 * \return The number of bytes actually read.
	 */
	int ReadBytes(void* To, const int MaxSize) const;

	/** \brief Inserts raw bytes at the current position. */
	void WriteBytes(const void* Fm, const int Size);

	/** \brief Replaces existing bytes at the current position without inserting new space. */
	int ReplaceBytes(const void* Fm, const int MaxSize);

	/** \brief Returns a constant pointer to the internal data. */
	const void* ToPtr() const { // Can be nullptr
		return m_Data.ToPtr();
	}
	/** \brief Returns a mutable pointer to the internal data. */
	void* ToPtr() {
		return m_Data.ToPtr();
	}
	const char* ToCharPtr() const { // Can be nullptr
		return (const char*)m_Data.ToPtr();
	}

	//-------------------------------------------------------------------------
	// Typed Read/Write methods
	//-------------------------------------------------------------------------
	byte ReadByte() const;
	bool ReadByte(byte* b) const;
	void WriteByte(const byte b);

	short ReadShort() const;
	bool ReadShort(short* s) const;
	void WriteShort(short s);

	word ReadWord() const;
	bool ReadWord(word* w) const;
	void WriteWord(word w);

	int  ReadInt() const;
	bool ReadInt(int* i) const;
	void WriteInt(int i);

	dword ReadDword() const;
	bool ReadDword(dword* dw) const;
	void WriteDword(dword dw);

	float ReadFloat() const;
	bool ReadFloat(float* f) const;
	void WriteFloat(const float f);

	double ReadDouble() const;
	bool ReadDouble(double* d) const;
	void WriteDouble(const double d);

	/** \brief Reads a string until a terminator is found. */
	bool ReadString(cStr*, const char* Terminators = "\r\n") const;

	/** \brief Writes a string without a trailing zero. */
	void WriteString(const char*);

	bool ReadVec2(cVec2* u) const;
	void WriteVec2(const cVec2& u);

	bool ReadVec3(cVec3* u) const;
	void WriteVec3(const cVec3& u);

	bool ReadRect(cRect* r) const;
	void WriteRect(const cRect& r);

	bool ReadVec2i(cVec2i* u) const;
	void WriteVec2i(const cVec2i& u);
private:
	cStr m_FilePn;
	cList<byte> m_Data;
	mutable int m_Pos;
};

// cFile::SeekCur
inline int cFile::SeekCur(const int Offset) const {
	m_Pos = cMath::Clamp(m_Pos + Offset, 0, m_Data.Count());
	return m_Pos;
}

// cFile::SeekEnd
inline int cFile::SeekEnd(const int Offset) const {
	m_Pos = cMath::Clamp(m_Data.Count() + Offset, 0, m_Data.Count());
	return m_Pos;
}

// cFile::SetPos
inline int cFile::SetPos(const int Pos) const {
	m_Pos = cMath::Clamp(Pos, 0, m_Data.Count());
	return m_Pos;
}

// cFile::ReadByte : (byte *)
inline bool cFile::ReadByte(byte* b) const {
	if (m_Pos >= m_Data.Count()) {
		return false;
	}
	*b = m_Data[m_Pos];
	m_Pos++;
	return true;
}

// cFile::ReadByte : ()
inline byte cFile::ReadByte() const {
	if (m_Pos >= m_Data.Count()) {
		return 0;
	}
	byte b = m_Data[m_Pos];
	m_Pos++;
	return b;
}

// cFile::WriteByte : (byte)
inline void cFile::WriteByte(const byte b) {
	m_Data.Insert(m_Pos, b);
	m_Pos++;
}

// cFile::ReadShort : (short *)
inline bool cFile::ReadShort(short* s) const {
	if (m_Pos + 1 >= m_Data.Count()) {
		return false;
	}
	*s = *(short*)(&m_Data[m_Pos]);
	m_Pos += 2;
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap2(s);
#endif // COMMS_BIG_ENDIAN
	return true;
}

// cFile::ReadShort : ()
inline short cFile::ReadShort() const {
	if (m_Pos + 1 >= m_Data.Count()) {
		return 0;
	}
	short s = *(short*)(&m_Data[m_Pos]);
	m_Pos += 2;
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap2(&s);
#endif // COMMS_BIG_ENDIAN
	return s;
}

// cFile::WriteShort : (short)
inline void cFile::WriteShort(short s) {
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap2(&s);
#endif // COMMS_BIG_ENDIAN
	m_Data.Insert(m_Pos, 0, 2);
	*(short*)&m_Data[m_Pos] = s;
	m_Pos += 2;
}

// cFile::ReadWord : (word *)
inline bool cFile::ReadWord(word* w) const {
	if (m_Pos + 1 >= m_Data.Count()) {
		return false;
	}
	*w = *(word*)(&m_Data[m_Pos]);
	m_Pos += 2;
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap2(w);
#endif // COMMS_BIG_ENDIAN
	return true;
}

// cFile::ReadWord : ()
inline word cFile::ReadWord() const {
	if (m_Pos + 1 >= m_Data.Count()) {
		return 0;
	}
	word w = *(word*)(&m_Data[m_Pos]);
	m_Pos += 2;
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap2(&w);
#endif // COMMS_BIG_ENDIAN
	return w;
}

// cFile::WriteWord : (word)
inline void cFile::WriteWord(word w) {
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap2(&w);
#endif // COMMS_BIG_ENDIAN
	m_Data.Insert(m_Pos, 0, 2);
	*(word*)&m_Data[m_Pos] = w;
	m_Pos += 2;
}

// cFile::ReadInt : (int *)
inline bool cFile::ReadInt(int* i) const {
	if (m_Pos + 3 >= m_Data.Count()) {
		return false;
	}
	*i = *(int*)(&m_Data[m_Pos]);
	m_Pos += 4;
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap4(i);
#endif // COMMS_BIG_ENDIAN
	return true;
}

// cFile::ReadInt : ()
inline int cFile::ReadInt() const {
	if (m_Pos + 3 >= m_Data.Count()) {
		return 0;
	}
	int i = *(int*)(&m_Data[m_Pos]);
	m_Pos += 4;
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap4(&i);
#endif // COMMS_BIG_ENDIAN
	return i;
}

// cFile::WriteInt : (int)
inline void cFile::WriteInt(int i) {
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap4(&i);
#endif // COMMS_BIG_ENDIAN
	m_Data.Insert(m_Pos, 0, 4);
	*(int*)&m_Data[m_Pos] = i;
	m_Pos += 4;
}

// cFile::ReadDword : (dword *)
inline bool cFile::ReadDword(dword* dw) const {
	if (m_Pos + 3 >= m_Data.Count()) {
		return false;
	}
	*dw = *(dword*)(&m_Data[m_Pos]);
	m_Pos += 4;
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap4(dw);
#endif // COMMS_BIG_ENDIAN
	return true;
}

// cFile::ReadDword : ()
inline dword cFile::ReadDword() const {
	if (m_Pos + 3 >= m_Data.Count()) {
		return 0;
	}
	dword dw = *(dword*)(&m_Data[m_Pos]);
	m_Pos += 4;
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap4(&dw);
#endif // COMMS_BIG_ENDIAN
	return dw;
}

// cFile::WriteDword : (dword)
inline void cFile::WriteDword(dword dw) {
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap4(&dw);
#endif // COMMS_BIG_ENDIAN
	m_Data.Insert(m_Pos, 0, 4);
	*(dword*)&m_Data[m_Pos] = dw;
	m_Pos += 4;
}

// cFile::ReadFloat : (float *)
inline bool cFile::ReadFloat(float* f) const {
	if (m_Pos + 3 >= m_Data.Count()) {
		return false;
	}
#ifdef COMMS_BIG_ENDIAN
	((byte*)f)[3] = m_Data[m_Pos];
	((byte*)f)[2] = m_Data[m_Pos + 1];
	((byte*)f)[1] = m_Data[m_Pos + 2];
	((byte*)f)[0] = m_Data[m_Pos + 3];
#elif defined COMMS_IOS || defined COMMS_TIZEN
	memcpy(f, m_Data.ToPtr() + m_Pos, 4);
#else // !COMMS_BIG_ENDIAN && !COMMS_IOS && !COMMS_TIZEN
	* f = *(float*)(&m_Data[m_Pos]);
#endif
	m_Pos += 4;
	return true;
}

// cFile::ReadFloat : ()
inline float cFile::ReadFloat() const {
	if (m_Pos + 3 >= m_Data.Count()) {
		return 0;
	}
	float f;
#ifdef COMMS_BIG_ENDIAN
	((byte*)&f)[3] = m_Data[m_Pos];
	((byte*)&f)[2] = m_Data[m_Pos + 1];
	((byte*)&f)[1] = m_Data[m_Pos + 2];
	((byte*)&f)[0] = m_Data[m_Pos + 3];
#elif defined COMMS_IOS || defined COMMS_TIZEN
	memcpy(&f, m_Data.ToPtr() + m_Pos, 4);
#else // !COMMS_BIG_ENDIAN && !COMMS_IOS && !COMMS_TIZEN
	f = *(float*)(&m_Data[m_Pos]);
#endif
	m_Pos += 4;
	return f;
}

// cFile::WriteFloat : (float)
inline void cFile::WriteFloat(const float f) {
#ifdef COMMS_BIG_ENDIAN
	byte r[4];
	r[0] = ((const byte*)&f)[3];
	r[1] = ((const byte*)&f)[2];
	r[2] = ((const byte*)&f)[1];
	r[3] = ((const byte*)&f)[0];
	m_Data.InsertRange(m_Pos, r, 4);
#else // !COMMS_BIG_ENDIAN
	m_Data.Insert(m_Pos, 0, 4);
	*(float*)&m_Data[m_Pos] = f;
#endif // COMMS_BIG_ENDIAN
	m_Pos += 4;
}

// cFile::ReadDouble : (double *)
inline bool cFile::ReadDouble(double* d) const {
	if (m_Pos + 7 >= m_Data.Count()) {
		return 0.0;
	}
	*d = *(double*)(&m_Data[m_Pos]);
	m_Pos += 8;
	return true;
}

// cFile::ReadDouble : ()
inline double cFile::ReadDouble() const {
	if (m_Pos + 7 >= m_Data.Count()) {
		return 0.0;
	}
	double d = *(double*)(&m_Data[m_Pos]);
	m_Pos += 8;
	return d;
}

// cFile::WriteDouble : (double)
inline void cFile::WriteDouble(const double d) {
	m_Data.Insert(m_Pos, 0, 8);
	*(double*)&m_Data[m_Pos] = d;
	m_Pos += 8;
}

// cFile::ReadVec2
inline bool cFile::ReadVec2(cVec2* u) const {
	if (!ReadFloat(&u->x)) {
		return false;
	}
	if (!ReadFloat(&u->y)) {
		return false;
	}
	return true;
}

// cFile::WriteVec2
inline void cFile::WriteVec2(const cVec2& u) {
	WriteFloat(u.x);
	WriteFloat(u.y);
}

// cFile::ReadVec3
inline bool cFile::ReadVec3(cVec3* u) const {
	if (!ReadFloat(&u->x)) {
		return false;
	}
	if (!ReadFloat(&u->y)) {
		return false;
	}
	if (!ReadFloat(&u->z)) {
		return false;
	}
	return true;
}

// cFile::WriteVec3
inline void cFile::WriteVec3(const cVec3& u) {
	WriteFloat(u.x);
	WriteFloat(u.y);
	WriteFloat(u.z);
}

// cFile::ReadRect
inline bool cFile::ReadRect(cRect* r) const {
	if (!ReadVec2(&r->m_Min)) {
		return false;
	}
	if (!ReadVec2(&r->m_Max)) {
		return false;
	}
	return true;
}

// cFile::WriteRect
inline void cFile::WriteRect(const cRect& r) {
	WriteVec2(r.m_Min);
	WriteVec2(r.m_Max);
}

// cFile::ReadVec2i
inline bool cFile::ReadVec2i(cVec2i* u) const {
	if (!ReadInt(&u->ToPtr()[0])) {
		return false;
	}
	if (!ReadInt(&u->ToPtr()[1])) {
		return false;
	}
	return true;
}

// cFile::WriteVec2i
inline void cFile::WriteVec2i(const cVec2i& u) {
	WriteInt(u[0]);
	WriteInt(u[1]);
}

