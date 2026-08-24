#include "comms.h"

namespace comms {

#ifdef COMMS_ZIP
#include "zlib.h"
#if defined COMMS_3DCOAT && defined _DEBUG
#pragma comment (lib, "zlibd.lib")
#else // DEBUG
#pragma comment (lib, "zlib.lib")
#endif
#endif // COMMS_ZIP

#define	ZIP_DEFLATE	8
#define ZIP_VERSION_TO_EXTRACT 20
#define	DEF_WBITS	15

#pragma pack(1)

// ZipLocalFileHeader
class ZipLocalFileHeader {
public:
	static const dword SIGNATURE = 0x04034B50;
	// ZipLocalFileHeader.Init
	void Init() {
		memset(this, 0, sizeof(ZipLocalFileHeader));
		Signature = SIGNATURE;
		VersionToExtract = ZIP_VERSION_TO_EXTRACT;
		CompressionMethod = ZIP_DEFLATE;
	}
	dword Signature;
	word VersionToExtract;
	word GeneralPurposeBitFlag;
	word CompressionMethod;
	word LastModFileTime;
	word LastModFileDate;
	dword Crc32;
	dword CompressedSize;
	dword UncompressedSize;
	word FilenameLength;
	word ExtraFieldLength;
};

// ZipCentralFileHeader
class ZipCentralFileHeader {
public:
	static const dword SIGNATURE = 0x02014B50;
	// ZipCentralFileHeader.Init
	void Init() {
		memset(this, 0, sizeof(ZipCentralFileHeader));
		Signature = SIGNATURE;
		VersionToExtract = ZIP_VERSION_TO_EXTRACT;
		CompressionMethod = ZIP_DEFLATE;
	}
	dword Signature;
	word VersionMadeBy;
	word VersionToExtract;
	word GeneralPurposeBitFlag;
	word CompressionMethod;
	word LastModFileTime;
	word LastModFileDate;
	dword Crc32;
	dword CompressedSize;
	dword UncompressedSize;
	word FilenameLength;
	word ExtraFieldLength;
	word CommentLength;
	word DiskNumberStart;
	word InternalFileAttibutes;
	dword ExternalFileAttributes;
	dword LocalFileHeaderOffset;
	// ZipCentralFileHeader.IsPresentExtraField64
	bool IsPresentExtraField64() const {
		bool C = (-1 == CompressedSize);
		bool U = (-1 == UncompressedSize);
		bool L = (-1 == LocalFileHeaderOffset);
		return C || U || L;
	}
};

// ZipExtraFieldData64
class ZipExtraFieldData64 {
public:
	// ZipExtraFieldData64.Init
	void Init() {
		memset(this, 0, sizeof(ZipExtraFieldData64));
	}
	qword UncompressedSize;
	qword CompressedSize;
	qword LocalFileHeaderOffset;
	dword DiskNumber;
};

// ZipExtraFieldHeader64
class ZipExtraFieldHeader64 {
public:
	// ZipExtraFieldHeader64.Init
	void Init() {
		memset(this, 0, sizeof(ZipExtraFieldHeader64));
		HeaderID = 1;
		DataSize = sizeof(ZipExtraFieldData64);
	}
	word HeaderID;
	word DataSize;
};

class ZipCentralDirEndWithSignature {
public:
	void Init() {
		memset(this, 0, sizeof(ZipCentralDirEndWithSignature));
		Signature = SIGNATURE;
	}
	static const dword SIGNATURE = 0x06054B50;
	dword Signature;
	word DiskNo;
	word CentralDirDiskNo;
	word NumEntriesOnDisk16;
	word NumEntries16;
	dword CentralDirSize;
	dword CentralDirOffset;
	word CommentLength;
};

class ZipCentralDirEndWithout4BytesSignature {
public:
	word DiskNo;
	word CentralDirDiskNo;
	word NumEntriesOnDisk16;
	word NumEntries16;
	dword CentralDirSize;
	dword CentralDirOffset;
	word CommentLength;

	bool IsPresentLocator64() const {
		bool N = ((word)-1 == NumEntries16);
		bool C = (-1 == CentralDirOffset);
		return N || C;
	}
};

// ZipLocator64
class ZipLocator64 {
public:
	static const dword SIGNATURE = 0x07064b50;
	// ZipLocator64.Init
	void Init() {
		memset(this, 0, sizeof(ZipLocator64));
		Signature = SIGNATURE;
	}
	dword Signature;
	dword DiskNumber;
	qword CentralDirEndOffset64;
	dword TotalDiskCount;
};

// ZipCentralDirEnd64
class ZipCentralDirEnd64 {
public:
	static const dword SIGNATURE = 0x06064b50;
	// ZipCentralDirEnd64.Init
	void Init() {
		memset(this, 0, sizeof(ZipCentralDirEnd64));
		Signature = SIGNATURE;
		CentralDirEnd64Size = sizeof(ZipCentralDirEnd64);
		VersionToExtract = ZIP_VERSION_TO_EXTRACT;
	}
	dword Signature;
	qword CentralDirEnd64Size;
	word VersionMadeBy;
	word VersionToExtract;
	dword DiskNumber;
	dword StartDiskNumber;
	qword NumEntriesOnDisk64;
	qword NumEntries64;
	qword CentralDirSize64;
	qword CentralDirOffset64;
};

#pragma pack()

//*****************************************************************************
// SourceZip
//*****************************************************************************
class SourceZip : public cIO::Source {
public:
	SourceZip(const char *ZipFilePn, const char *DataFolderWithinZip = nullptr) : cIO::Source() {
		Ctor(ZipFilePn, DataFolderWithinZip);
	}
	virtual bool Load(const char *FilePn, cFile *To) const {
		return Load(FilePn, To, nullptr);
	}
	bool Load(const char *FilePn, cFile *ToMemory, cFileDisk *ToDisk) const;

	virtual bool Save(const char *, const void *, const size_t, const bool) const {
		return false;
	}
	virtual const cStr & GetPath() const {
		return m_ZipFilePn;
	}
	bool GetValid() const {
		return m_Valid;
	}
	bool ExtractAll(const char *ToFolder, cIO::ZipProgress Func, std::function<void(const char*)> filereport = nullptr);
	struct ProgressArgs {
		qword CurBytes;
		qword TotalBytes;
		int Perc;
		cIO::ZipProgress Func;
	};
private:
	void Ctor(const char *ZipFilePn, const char *DataFolderWithinZip);
	bool m_Valid;
	cStr m_ZipFilePn, m_DataFolderWithinZip;
	mutable cFileDisk m_ZipFile;
	
	struct FileInfo {
		cStr FileName;
		qword CompressedSize;
		qword UncompressedSize;
		qword LocalFileHeaderOffset;
		bool WasOnlyStored;
	};
	cList<FileInfo> m_FilesInfo;
	qword m_TotalCompressedSize, m_TotalUncompressedSize;

	void ReadDirectory();
	bool ReadEntry(const int Index, cFile *ToMemory, cFileDisk *ToDisk, ProgressArgs *Args = nullptr) const;
	static void ReadEntry_Out(const void *OutPtr, const size_t OutHave, cFile *ToMemory, cFileDisk *ToDisk, ProgressArgs *Args);
}; // SourceZip

//-----------------------------------------------------------------------------
// SourceZip::Ctor
//-----------------------------------------------------------------------------
void SourceZip::Ctor(const char *ZipFilePn, const char *DataFolderWithinZip) {
	m_Valid = false;
	m_ZipFilePn = cIO::EnsureAbsolutePath(ZipFilePn);
	m_DataFolderWithinZip = DataFolderWithinZip;
	if (!m_ZipFile.OpenRead(ZipFilePn)) {
		return;
	}
	ReadDirectory();
	m_ZipFile.Close();
} // SourceZip::Ctor

//-----------------------------------------------------------------------------
// SourceZip::ReadDirectory
//-----------------------------------------------------------------------------
void SourceZip::ReadDirectory() {
	m_TotalCompressedSize = m_TotalUncompressedSize = 0;
	qword CentralDirOffset = 0, NumEntries64 = 0;
	// Verify Zip header
	dword Header = 0;
	m_ZipFile.SetPos(0);
	if(m_ZipFile.ReadBytes(&Header, 4) != 4) {
		return;
	}
	if(Header != ZipLocalFileHeader::SIGNATURE) {
		return;
	}
	// Read central end
	m_ZipFile.SeekEnd();
	ZipCentralDirEndWithout4BytesSignature CentralEnd;
	size_t SizeCentralEnd = sizeof(CentralEnd);
	dword CentralEndSignature = 0;
	// Searching "CentralEndSignature" by skipping a comment at the end of the zip-file
	m_ZipFile.SeekCur(-int64_t(SizeCentralEnd + 4));
	while(m_ZipFile.GetPos() > 0) {
		if(m_ZipFile.ReadBytes(&CentralEndSignature, 4) != 4) {
			return;
		}
		if(ZipCentralDirEndWithSignature::SIGNATURE == CentralEndSignature) {
			if(m_ZipFile.ReadBytes(&CentralEnd, SizeCentralEnd) != SizeCentralEnd) {
				return;
			}
			break;
		}
		m_ZipFile.SeekCur(-5); // sizeof(CentralEndSignature) + 1 byte
	}
	if(CentralEndSignature != ZipCentralDirEndWithSignature::SIGNATURE) {
		return;
	}
	// Read optional locator 64 before central end
	if (CentralEnd.IsPresentLocator64()) {
		ZipLocator64 Loc64;
		size_t SizeLoc64 = sizeof(Loc64);
        m_ZipFile.SeekCur(-int64(SizeCentralEnd + 4 + SizeLoc64)); // +4 because "CentralDirEndWithout4BytesSignature"
		if (m_ZipFile.ReadBytes(&Loc64, SizeLoc64) != SizeLoc64) {
			return;
		}
		if (Loc64.Signature != ZipLocator64::SIGNATURE) {
			return;
		}
		// Read central end 64 on which points the locator 64
		m_ZipFile.SetPos(Loc64.CentralDirEndOffset64);
		ZipCentralDirEnd64 CentralEnd64;
		size_t SizeCentralEnd64 = sizeof(CentralEnd64);
		if (m_ZipFile.ReadBytes(&CentralEnd64, SizeCentralEnd64) != SizeCentralEnd64) {
			return;
		}
		if (CentralEnd64.Signature != ZipCentralDirEnd64::SIGNATURE) {
			return;
		}
		CentralDirOffset = CentralEnd64.CentralDirOffset64;
		NumEntries64 = CentralEnd64.NumEntries64;
	}
	else {
		CentralDirOffset = CentralEnd.CentralDirOffset;
		NumEntries64 = CentralEnd.NumEntries16;
	}
	// Read central file headers
	m_ZipFile.SetPos(CentralDirOffset);
	ZipCentralFileHeader Hdr;
	size_t SizeHdr = sizeof(Hdr);
	FileInfo FI;
	for (int i = 0; i < NumEntries64; i++) {
		if (m_ZipFile.ReadBytes(&Hdr, SizeHdr) != SizeHdr) {
			return;
		}
		if (Hdr.Signature != ZipCentralFileHeader::SIGNATURE) {
			return;
		}
		FI.FileName.SetLength(Hdr.FilenameLength);
		if (m_ZipFile.ReadBytes(FI.FileName.ToNonConstCharPtr(), Hdr.FilenameLength) != Hdr.FilenameLength) {
			return;
		}
		// Fill file info
		FI.UncompressedSize = Hdr.UncompressedSize;
		FI.CompressedSize = Hdr.CompressedSize;
		FI.LocalFileHeaderOffset = Hdr.LocalFileHeaderOffset;
		if (Hdr.ExtraFieldLength > 0) {
			if (Hdr.IsPresentExtraField64()) {
				// Read optional extra field 64
				word ExtraFieldLeft = Hdr.ExtraFieldLength;
				ZipExtraFieldHeader64 ExtraHdr;
				size_t SizeExtraHdr = sizeof(ExtraHdr);
				if (m_ZipFile.ReadBytes(&ExtraHdr, SizeExtraHdr) != SizeExtraHdr) {
					return;
				}
				ExtraFieldLeft -= (word)SizeExtraHdr;
				// Read optional uncompressed size 64
				if (-1 == Hdr.UncompressedSize) {
					if (m_ZipFile.ReadBytes(&FI.UncompressedSize, 8) != 8) {
						return;
					}
					ExtraFieldLeft -= 8;
				}
				// Read optional compressed size 64
				if (-1 == Hdr.CompressedSize) {
					if (m_ZipFile.ReadBytes(&FI.CompressedSize, 8) != 8) {
						return;
					}
					ExtraFieldLeft -= 8;
				}
				// Read optional local header offset 64
				if (-1 == Hdr.LocalFileHeaderOffset) {
					if (m_ZipFile.ReadBytes(&FI.LocalFileHeaderOffset, 8) != 8) {
						return;
					}
					ExtraFieldLeft -= 8;
				}
				// Skip extra rest
				m_ZipFile.SeekCur(ExtraFieldLeft);
			}
			else {
				// Skip extra field
				m_ZipFile.SeekCur(Hdr.ExtraFieldLength);
			}
		}
		FI.WasOnlyStored = (Hdr.CompressionMethod != ZIP_DEFLATE);
		if(FI.FileName.EndsWith("/")) {
			// Do not add directory entry because the method "SourceZip::ExtractAll" assumes that list "m_FilesInfo" contains
			// only files. Otherwise under Linux it will create an empty file with this directory name.
		} else {
			m_FilesInfo.Add(FI);
		}
		m_TotalCompressedSize += FI.CompressedSize;
		m_TotalUncompressedSize += FI.UncompressedSize;
	}
	m_Valid = true;
} // SourceZip::ReadDirectory

//-----------------------------------------------------------------------------
// SourceZip::Load
//-----------------------------------------------------------------------------
bool SourceZip::Load(const char *FilePn, cFile *ToMemory, cFileDisk *ToDisk) const {
	if(!m_Valid) {
		return false;
	}
	cStr Pn = m_DataFolderWithinZip;
	Pn.AppendPath(FilePn);
	bool r = false;
	for (int i = 0; i < m_FilesInfo.Count(); i++) {
		const cStr &S = m_FilesInfo[i].FileName;
		if(cStr::EqualsPath(Pn, S)) {
			if (m_ZipFile.OpenRead(m_ZipFilePn)) {
				r = ReadEntry(i, ToMemory, ToDisk);
				m_ZipFile.Close();
			}
			break;
		}
	}
	return r;
} // SourceZip::Load

// SourceZip::ReadEntry_Out
void SourceZip::ReadEntry_Out(const void *OutPtr, const size_t OutHave, cFile *ToMemory, cFileDisk *ToDisk, ProgressArgs *Args) {
	if (ToMemory != nullptr) {
		ToMemory->WriteBytes(OutPtr, (int)OutHave);
	}
	else if (ToDisk != nullptr) {
		ToDisk->WriteBytes(OutPtr, OutHave);
	}
	if (Args != nullptr) {
		Args->CurBytes += OutHave;
		if (Args->Func != nullptr) {
			int p = (int)(Args->CurBytes / cMath::Max((qword)1, Args->TotalBytes / 100));
			if (p != Args->Perc) {
				Args->Perc = p;
				Args->Func(Args->Perc);
			}
		}
	}
}

// cIO::ZipInflateRaw
bool cIO::ZipInflateRaw(const void *SrcCompressed, const size_t CompressedSize, cFile *ToMemory) {
	if (0 == CompressedSize) {
		return true;
	}
#ifdef COMMS_ZIP
	static cList<byte> Out(65536);
	z_stream ZS;
	memset(&ZS, 0, sizeof(ZS));
	int Ret = inflateInit2(&ZS, -DEF_WBITS);
	cAssertM(Ret != Z_STREAM_ERROR, "Build \"zlib\" without \"Z_SOLO\" preprocessor definition");
	if (Ret != Z_OK) {
		return false;
	}
	ZS.next_in = (byte *)SrcCompressed;
	ZS.avail_in = (uInt)CompressedSize;
	cAssert(ZS.avail_in > 0);
	do {
		ZS.next_out = Out.ToPtr();
		ZS.avail_out = Out.Count();
		Ret = inflate(&ZS, Z_NO_FLUSH);
		cAssert((Ret >= 0) || (Z_BUF_ERROR == Ret));
		size_t OutHave = Out.Size() - ZS.avail_out;
		ToMemory->WriteBytes(Out.ToPtr(), (int)OutHave);
	} while (0 == ZS.avail_out);
	inflateEnd(&ZS);
#endif // COMMS_ZIP
	return true;
}

// cIO::ZipInflateRaw_Debug
void cIO::ZipInflateRaw_Debug() {
#ifdef _DEBUG
	const char *ZipFile = "C:/Users/user/Desktop/1.zip";
	const char *OutFile = "C:/Users/user/Desktop/1.jpg";
	cFile F;
	cAssert(cIO::LoadFile(ZipFile, &F));
	ZipLocalFileHeader Local;
	size_t SizeLocal = sizeof(Local);
	cAssert(F.ReadBytes(&Local, (int)SizeLocal) == SizeLocal);
	cAssert(ZipLocalFileHeader::SIGNATURE == Local.Signature);
	F.SeekCur(Local.FilenameLength + Local.ExtraFieldLength);
	cFile Out;
	bool r = cIO::ZipInflateRaw((byte *)F.ToPtr() + F.GetPos(), Local.CompressedSize, &Out);
	cAssert(r);
	cIO::SaveFile(OutFile, Out);
#endif // _DEBUG
}

//-----------------------------------------------------------------------------
// SourceZip::ReadEntry
//-----------------------------------------------------------------------------
bool SourceZip::ReadEntry(const int Index, cFile *ToMemory, cFileDisk *ToDisk, ProgressArgs *Args) const {
	const FileInfo &FI = m_FilesInfo[Index];
	ZipLocalFileHeader LocalHdr;
	size_t SizeLocalHdr = sizeof(LocalHdr);
	m_ZipFile.SetPos(FI.LocalFileHeaderOffset);
	if (m_ZipFile.ReadBytes(&LocalHdr, SizeLocalHdr) != SizeLocalHdr) {
		return false;
	}
	if (LocalHdr.Signature != ZipLocalFileHeader::SIGNATURE) {
		return false;
	}
	m_ZipFile.SeekCur(LocalHdr.FilenameLength + LocalHdr.ExtraFieldLength);
	static cList<byte> In(65536), Out(65536);
	qword InBytesLeft = FI.CompressedSize;
	if (FI.WasOnlyStored) {
		while (InBytesLeft > 0) {
			size_t InBlockSize = InBytesLeft > Out.Size() ? Out.Size() : (size_t)InBytesLeft;
			size_t OutHave = m_ZipFile.ReadBytes(Out.ToPtr(), InBlockSize);
			InBytesLeft -= InBlockSize;
			cAssert(InBytesLeft >= 0);
			ReadEntry_Out(Out.ToPtr(), OutHave, ToMemory, ToDisk, Args);
		}
	}
	else {
#ifdef COMMS_ZIP
		z_stream ZS;
		memset(&ZS, 0, sizeof(ZS));
		int Ret = inflateInit2(&ZS, -DEF_WBITS);
		cAssertM(Ret != Z_STREAM_ERROR, "Build \"zlib\" without \"Z_SOLO\" preprocessor definition");
		if (Ret != Z_OK) {
			return false;
		}
		while (InBytesLeft > 0) {
			size_t InBlockSize = InBytesLeft > In.Size() ? In.Size() : (size_t)InBytesLeft;
			ZS.next_in = In.ToPtr();
			ZS.avail_in = (uInt)m_ZipFile.ReadBytes(In.ToPtr(), InBlockSize);
			cAssert(ZS.avail_in > 0);
			InBytesLeft -= InBlockSize;
			cAssert(InBytesLeft >= 0);
			do {
				ZS.next_out = Out.ToPtr();
				ZS.avail_out = Out.Count();
				Ret = inflate(&ZS, Z_NO_FLUSH);
				cAssert((Ret >= 0) || (Z_BUF_ERROR == Ret));
				size_t OutHave = Out.Size() - ZS.avail_out;
				ReadEntry_Out(Out.ToPtr(), OutHave, ToMemory, ToDisk, Args);
			} while (0 == ZS.avail_out);
		}
		inflateEnd(&ZS);
#endif // COMMS_ZIP
	}
	return true;
} // SourceZip::ReadEntry

//-----------------------------------------------------------------------------
// cIO::AddSourceZip
//-----------------------------------------------------------------------------
void cIO::AddSourceZip(const char *ZipFilePn, const char *DataFolderWithinZip) {
	cStr Path = cIO::EnsureAbsolutePath(ZipFilePn);
	int i;
	for(i = 0; i < cIO::GetSources().Count(); i++) {
		if(cStr::EqualsPath(cIO::GetSources()[i]->GetPath(), Path)) {
			return;
		}
	}

	SourceZip *Z = new SourceZip(Path, DataFolderWithinZip);
	if(Z->GetValid()) {
		cIO::GetSourcesMutable().Add(Z);
		cLog::Message("Added source ZIP \"%s\"", Path.ToCharPtr());
		return;
	}
	delete Z; Z = nullptr;
	cLog::Warning("Can't add source ZIP \"%s\"", ZipFilePn);
} // cIO::AddSourceZip

//-----------------------------------------------------------------------------
// cIO::SearchZipSources
//-----------------------------------------------------------------------------
void cIO::SearchZipSources(const char *DataFolderWithinZip) {
	cList<cStr> Files, Zips;
	SearchFiles(nullptr, &Files);
	int i;
	for(i = 0; i < Files.Count(); i++) {
		const cStr &S = Files[i];
		if(cStr::EqualsNoCase(S.GetFileExtension(), "zip")) {
			Zips.Add(S);
		}
	}
	for(i = 0; i < Zips.Count(); i++) {
		AddSourceZip(Zips[i], DataFolderWithinZip);
	}
} // cIO::SearchZipSources

// SourceZip::ExtractAll
bool SourceZip::ExtractAll(const char *ToFolder, cIO::ZipProgress Func, std::function<void(const char*)> filereport) {
	ProgressArgs Args;
	memset(&Args, 0, sizeof(Args));
	Args.TotalBytes = m_TotalUncompressedSize;
	Args.Func = Func;
	if (m_ZipFile.OpenRead(m_ZipFilePn)) {
		cFileDisk FD;
		cStr Fn, Path;
		for (int i = 0; i < m_FilesInfo.Count(); i++) {
			const cStr &S = m_FilesInfo[i].FileName;
			Fn = ToFolder;
			Fn.AppendPath(S);
			Fn = cIO::EnsureAbsolutePath(Fn);
			cIO::ReplaceReadPath(&Fn);
			Path = Fn;
			Path.RemoveFileName();
            Path.EnsureTrailingBackslash();
			cIO::CreatePath(Path);
			if (!FD.CreateWrite(Fn)) {
				continue;
			}
			if (!ReadEntry(i, nullptr, &FD, &Args)) {
				break;
			}
			FD.Close();
		}
		m_ZipFile.Close();
	}
	for (int i = 0; i < m_FilesInfo.Count(); i++) {
		if (filereport)filereport(m_FilesInfo[i].FileName);
	}
	return (Args.CurBytes == m_TotalUncompressedSize);
}

// cIO::ExtractZip
bool cIO::ExtractZip(const char *ZipFilePn, const char *ToFolder, ZipProgress Func, std::function<void(const char*)> filereport) {
	cStr fn = ZipFilePn;
	ReplaceReadPathForExistingFile(&fn);
	SourceZip *Z = new SourceZip(fn);
	bool b = false;
	if (Z->GetValid()) {
		b = Z->ExtractAll(ToFolder, Func, filereport);
	}
	delete Z; Z = nullptr;
	return b;
}

// CreateZip_Deflate
static bool CreateZip_Deflate(cFileDisk &SrcDisk, cFileDisk &ZipDisk, dword *Crc32, SourceZip::ProgressArgs *Args) {
#ifdef COMMS_ZIP
	*Crc32 = (dword)crc32(0, Z_NULL, 0);
	static cList<byte> In(65536), Out(65536);
	int Ret = 0, Flush = 0;
	size_t OutHave = 0;
	uInt InBlockSize = 0;
	z_stream ZS;
	memset(&ZS, 0, sizeof(ZS));
	Ret = deflateInit2(&ZS, Z_BEST_COMPRESSION, Z_DEFLATED, -DEF_WBITS, 9, Z_DEFAULT_STRATEGY);
	cAssertM(Ret != Z_STREAM_ERROR, "Build \"zlib\" without \"Z_SOLO\" preprocessor definition");
	if(Ret != Z_OK) {
		return false;
	}
    qword ZippedBytes = 0;
	do {
		InBlockSize = (uInt)SrcDisk.ReadBytes(In.ToPtr(), In.Size());
		if (0 == InBlockSize) {
			deflateEnd(&ZS);
			return true;
		}
		*Crc32 = (dword)crc32(*Crc32, In.ToPtr(), InBlockSize);
		Flush = (SrcDisk.IsEof() ? Z_FINISH : Z_NO_FLUSH);
		ZS.avail_in = InBlockSize;
		ZS.next_in = In.ToPtr();
		do {
			ZS.avail_out = (uInt)Out.Size();
			ZS.next_out = Out.ToPtr();
			Ret = deflate(&ZS, Flush);
			cAssert(Ret != Z_STREAM_ERROR);
			OutHave = Out.Size() - ZS.avail_out;
			ZipDisk.WriteBytes(Out.ToPtr(), OutHave);
		} while(0 == ZS.avail_out);
		cAssert(0 == ZS.avail_in);
		ZippedBytes += InBlockSize;
        if (Args != nullptr) {
			int p = (int)((Args->CurBytes + ZippedBytes) / cMath::Max((qword)1, Args->TotalBytes / 100));
			if (p != Args->Perc) {
				Args->Perc = p;
				if (Args->Func != nullptr) {
					Args->Func(Args->Perc);
				}
			}
        }
    } while ( Flush != Z_FINISH );
	cAssert(Z_STREAM_END == Ret);
	deflateEnd(&ZS);
#endif // COMMS_ZIP
	return true;
}

// CreateZip
static bool CreateZip(const char* FilePn, const char* FilePathInZip, const char *ParentFolder, const char *ZipFilePn, const bool WithParent, cIO::ZipProgress Func) {
	const bool FromFile = (FilePn != nullptr);
	cList<cStr> Files;
	cStr Folder;
	int i;
	if (FromFile) {
		const cStr File = cIO::EnsureAbsolutePath(FilePn);
		Files.Add(File);
		Folder = File;
		Folder.RemoveFileName();
	} else {
		Folder = cIO::EnsureAbsolutePath(ParentFolder);
		// Scan for files in folder
		cIO::SearchFilesRecursive(Folder, &Files);
		if (Files.IsEmpty()) {
			return false; // No files to compress
		}
		if (WithParent) {
			i = Folder.LastIndexOfAny("/\\");
			if (i != -1) {
				Folder.Remove(i);
			}
		}
	}
	// Progress args
	SourceZip::ProgressArgs Args;
	memset(&Args, 0, sizeof(Args));
	Args.Func = Func;
	// Total bytes
	cFileDisk SrcDisk;
	for(i = 0; i < Files.Count(); i++) {
		const cStr &t = Files[i];
		if (!SrcDisk.OpenRead(t)) {
			continue;
		}
		Args.TotalBytes += SrcDisk.GetSizeBytes();
		SrcDisk.Close();
	}
	const qword SizeFor64 = (1 << 30); // 1 GiB
	const bool Mode64 = Args.TotalBytes > SizeFor64;
	// Create Zip
	cStr Zip = cIO::EnsureAbsolutePath(ZipFilePn);
	cIO::SetFilePermissions(Zip);
	cFileDisk ZipDisk;
	if (!ZipDisk.CreateWrite(Zip.ToCharPtr())) {
		return false;
	}
	// Local, central, and extra headers init
	ZipLocalFileHeader LocalHdr;
	LocalHdr.Init();
	ZipCentralFileHeader CentralHdr;
	CentralHdr.Init();
	ZipExtraFieldHeader64 ExtraHdr;
	ExtraHdr.Init();
	ZipExtraFieldData64 ExtraData;
	ExtraData.Init();
	if (Mode64) {
		LocalHdr.ExtraFieldLength = sizeof(ExtraHdr) + sizeof(ExtraData);
		CentralHdr.ExtraFieldLength = LocalHdr.ExtraFieldLength;
	}
	// Load Files & Deflate
	cFile CentralDir;
	qword Entries64 = 0;
	for(i = 0; i < Files.Count(); i++) {
		cStr &t = Files[i];
		if (!SrcDisk.OpenRead(t)) {
			continue;
		}
		t.RemoveFileAbsPath(Folder); // Convert file path to local
		t.BackSlashesToSlashes();
		// Write local header template with filename
		ExtraData.LocalFileHeaderOffset = ZipDisk.GetPos();
		CentralHdr.LocalFileHeaderOffset = (dword)ExtraData.LocalFileHeaderOffset;
		ZipDisk.WriteBytes(&LocalHdr, sizeof(LocalHdr));
		if (FilePathInZip)t = FilePathInZip;
		CentralHdr.FilenameLength = LocalHdr.FilenameLength = t.Length();
		ZipDisk.WriteBytes(t.ToCharPtr(), t.Length()); // File pathname
		if (Mode64) {
			// Write local header extra field template
			ZipDisk.WriteBytes(&ExtraHdr, sizeof(ExtraHdr));
			ZipDisk.WriteBytes(&ExtraData, sizeof(ExtraData));
		}
		// Deflate with progress
		int64 PosBeforeDeflate = ZipDisk.GetPos();
		if (!CreateZip_Deflate(SrcDisk, ZipDisk, &LocalHdr.Crc32, &Args)) {
			SrcDisk.Close();
			continue;
		}
		// Update headers after deflate
		CentralHdr.Crc32 = LocalHdr.Crc32;
		ExtraData.UncompressedSize = SrcDisk.GetSizeBytes();
		CentralHdr.UncompressedSize = LocalHdr.UncompressedSize = (dword)ExtraData.UncompressedSize;
		ExtraData.CompressedSize = ZipDisk.GetPos() - PosBeforeDeflate;
		CentralHdr.CompressedSize = LocalHdr.CompressedSize = (dword)ExtraData.CompressedSize;
		SrcDisk.Close();
		// Write updated local header before deflated data
		ZipDisk.SetPos(ExtraData.LocalFileHeaderOffset);
		if (Mode64) {
			LocalHdr.CompressedSize = -1;
			LocalHdr.UncompressedSize = -1;
		}
		ZipDisk.WriteBytes(&LocalHdr, sizeof(LocalHdr)); // Local Header
		if (Mode64) {
			// Write updated local header extra field
			ZipDisk.SeekCur(LocalHdr.FilenameLength); // Skip filename
			ZipDisk.WriteBytes(&ExtraHdr, sizeof(ExtraHdr)); // Extra field header
			ZipDisk.WriteBytes(&ExtraData, sizeof(ExtraData)); // Extra field data
		}
		ZipDisk.SeekEnd();
		// Write updated central header
		if (Mode64) {
			CentralHdr.LocalFileHeaderOffset = -1;
			CentralHdr.CompressedSize = -1;
			CentralHdr.UncompressedSize = -1;
		}
		CentralDir.WriteBytes(&CentralHdr, sizeof(CentralHdr)); // Central Header
		CentralDir.WriteString(t.ToCharPtr()); // File Pathname
		if (Mode64) {
			// Write updated central header extra field
			CentralDir.WriteBytes(&ExtraHdr, sizeof(ExtraHdr)); // Extra field header
			CentralDir.WriteBytes(&ExtraData, sizeof(ExtraData)); // Extra field data
		}
		Entries64++;
		Args.CurBytes += ExtraData.UncompressedSize;
	}
	// Central End
	ZipCentralDirEndWithSignature CentralEnd;
	CentralEnd.Init();
	ZipCentralDirEnd64 CentralEnd64;
	CentralEnd64.Init();
	CentralEnd64.NumEntries64 = CentralEnd64.NumEntriesOnDisk64 = Entries64;
	CentralEnd.NumEntries16 = CentralEnd.NumEntriesOnDisk16 = (word)Entries64;
	CentralEnd64.CentralDirSize64 = CentralDir.Size();
	CentralEnd.CentralDirSize = (dword)CentralEnd64.CentralDirSize64;
	CentralEnd64.CentralDirOffset64 = ZipDisk.GetPos();
	CentralEnd.CentralDirOffset = (dword)CentralEnd64.CentralDirOffset64;
	ZipDisk.WriteBytes(CentralDir.ToPtr(), CentralDir.Size()); // Central Dir
	if (Mode64) {
		ZipLocator64 Loc64;
		Loc64.Init();
		Loc64.CentralDirEndOffset64 = ZipDisk.GetPos();
		ZipDisk.WriteBytes(&CentralEnd64, sizeof(CentralEnd64)); // Central End 64
		ZipDisk.WriteBytes(&Loc64, sizeof(Loc64)); // Locator 64
		CentralEnd.CentralDirOffset = -1;
		CentralEnd.NumEntries16 = CentralEnd.NumEntriesOnDisk16 = -1;
	}
	ZipDisk.WriteBytes(&CentralEnd, sizeof(CentralEnd)); // Central End
	// Close Zip
	ZipDisk.Close();
	cIO::SetFilePermissions(Zip);
	return true;
}

// cIO::CreateZip_FromFile
bool cIO::CreateZip_FromFile(const char* FilePn, const char* FilePnInZip, const char* ZipFilePn, ZipProgress Func) {
	return CreateZip(FilePn, FilePnInZip, nullptr, ZipFilePn, false, Func);
}

// cIO::CreateZip_FromFolder
bool cIO::CreateZip_FromFolder(const char* ParentFolder, const char* ZipFilePn, const bool WithParent, ZipProgress Func) {
	return CreateZip(nullptr, nullptr, ParentFolder, ZipFilePn, WithParent, Func);
}

} // comms
