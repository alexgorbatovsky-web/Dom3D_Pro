#include "comms.h"

namespace comms {

// cFileDisk.Open
bool cFileDisk::Open(const char *FilePn, const Mode::Enum _Mode) {
	cAssert(nullptr == m_File);
	if (m_File != nullptr) {
		Close();
	}
	const cStr Abs = cIO::EnsureAbsolutePath(FilePn);
	const char *Mode = (Mode::Read == _Mode ? "rb" : (Mode::Append == _Mode ? "ab" : "wb"));
#ifdef COMMS_WINDOWS
	_wfopen_s(&m_File, wchar_path(Abs), wchar_path(Mode));
#else // COMMS_LINUX, COMMS_MACOS, ...
	m_File = fopen(Abs, Mode);
#endif // COMMS_WINDOWS
	if (nullptr == m_File) {
		return false;
	}
	if ((Mode::Read == _Mode) || (Mode::Append == _Mode)) {
		SeekEnd();
		m_SizeBytes = GetPos();
		if (Mode::Read == _Mode) {
			SetPos(0);
		}
	}
	return true;
}

// cFileDisk.SeekEnd
void cFileDisk::SeekEnd() {
	cAssert(m_File != nullptr);
	if (m_File != nullptr) {
#ifdef COMMS_WINDOWS
		_fseeki64(m_File, 0, SEEK_END);
#else // macOS
        fseek(m_File, 0, SEEK_END);
#endif // COMMS_WINDOWS
	}
}

// cFileDisk::GetPos
int64 cFileDisk::GetPos() const {
	cAssert(m_File != nullptr);
	int64 Pos = 0;
	if (m_File != nullptr) {
#ifdef COMMS_WINDOWS
		Pos = _ftelli64(m_File);
#else // macOS
        Pos = ftell(m_File);
#endif // COMMS_WINDOWS
	}
	return Pos;
}

// cFileDisk.SetPos
void cFileDisk::SetPos(const int64 Pos) {
	cAssert(m_File != nullptr);
	if (m_File != nullptr) {
#ifdef COMMS_WINDOWS
		_fseeki64(m_File, Pos, SEEK_SET);
#else // macOS
        fseek(m_File, Pos, SEEK_SET);
#endif // COMMS_WINDOWS
	}
}

// cFileDisk.SeekCur
void cFileDisk::SeekCur(const int64 Offset) {
	cAssert(m_File != nullptr);
	if (m_File != nullptr) {
#ifdef COMMS_WINDOWS
		_fseeki64(m_File, Offset, SEEK_CUR);
#else // macOS
        fseek(m_File, Offset, SEEK_CUR);
#endif // COMMS_WINDOWS
	}
}

// cFileDisk.Close
void cFileDisk::Close() {
	if (m_File != nullptr) {
		fclose(m_File);
		Null();
	}
}

// cFile::Set
void cFile::Set(cList<byte> *Src, const char *FilePn) {
	m_Data.Set(Src);
	m_Pos = 0;
	m_FilePn = FilePn;
}

// cFile::Copy
void cFile::Copy(const void *Src, const int Size, const char *FilePn) {
	if(Size > 0 && Src != nullptr) {
        m_Data.Copy( ( const byte * )Src, Size );
	} else {
		m_Data.Clear();
	}
	m_Pos = 0;
	m_FilePn = FilePn;
}

// cFile::Clear
void cFile::Clear() {
	m_Data.Clear();
	m_Pos = 0;
	m_FilePn.Clear();
}

// cFile::ReadBytes
int cFile::ReadBytes(void *To, const int MaxSize) const {
	if(m_Pos >= m_Data.Count()) {
		return -1;
	}
	const int Size = cMath::Min(MaxSize, m_Data.Count() - m_Pos);
	memcpy(To, &m_Data[m_Pos], Size);
	m_Pos += Size;
	return Size;
}

// cFile::WriteBytes
void cFile::WriteBytes(const void *Fm, const int Size) {
	m_Data.InsertRange(m_Pos, (const byte *)Fm, Size);
	m_Pos += Size;
}

// cFile::ReplaceBytes
int cFile::ReplaceBytes(const void *Fm, const int MaxSize) {
	if(m_Pos >= m_Data.Count()) {
		return -1;
	}
	const int Size = cMath::Min(MaxSize, m_Data.Count() - m_Pos);
	memcpy(m_Data.ToPtr() + m_Pos, Fm, Size);
	m_Pos += Size;
	return Size;
}

// cFile::SetPos
int cFile::SetPosMutable(const int Pos) {
	int Diff = Pos - m_Data.Count();
	SetPos(Pos);
	if(Diff > 0) {
		byte *Zero = new byte[Diff];
		memset(Zero, 0, Diff);
		WriteBytes(Zero, Diff);
		delete[] Zero;
	}
	return m_Pos;
}

//-----------------------------------------------------------------------------
// cFile::ReadString
//-----------------------------------------------------------------------------
bool cFile::ReadString(cStr *S, const char *Terminators) const {
	cAssert(S != nullptr);
	cAssert(Terminators != nullptr);
	
	cStr T;

	S->Clear();
	
	if(m_Pos >= m_Data.Count()) {
		return false;
	}

	T = Terminators;

	// Skip leading terminators
	while(m_Pos < m_Data.Count() && T.Contains((char)m_Data[m_Pos])) {
		m_Pos++;
	}
	
	// Read until terminator
	while(m_Pos < m_Data.Count() && !T.Contains((char)m_Data[m_Pos])) {
		S->Append((char)m_Data[m_Pos]);
		m_Pos++;
	}

	// Skip trailing terminators
	if(m_Pos < m_Data.Count() && T.Contains((char)m_Data[m_Pos])) {
		m_Pos++;
	}
	
	return true;
} // cFile::ReadString

// cFile::WriteString
void cFile::WriteString(const char *Str) {
	const int l = cStr::Length(Str);
	m_Data.InsertRange(m_Pos, (const byte *)Str, l); // w/o trailing zero
	m_Pos += l;
}

} // comms
