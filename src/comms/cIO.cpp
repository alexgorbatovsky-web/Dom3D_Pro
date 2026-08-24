#include "comms.h"
#ifdef COMMS_3DCOAT
#include "tlimits.h"
bool CheckIfFileExists(const char *);
#endif // COMMS_3DCOAT

#ifdef COMMS_IOS
bool cIosMain_LoadFile(const char *FileName, comms::cFile *To);
bool cIosMain_OpenURL(const char *URL);
#endif // COMMS_IOS

#ifdef COMMS_TIZEN
bool cTizenMain_LoadFile(const char *FileName, comms::cFile *To);
#endif // COMMS_TIZEN

#ifdef COMMS_WINDOWS
#pragma comment (lib, "Netapi32.lib")
HWND cWinMain_GetWindow();
namespace comms {
bool cWinMain_LoadFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *SingleFilePn, cList<cStr> *MultiFilePn, const char *PrefKey, const char *InitialFileName);
bool cWinMain_SaveFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *FilePn, const char *PrefKey, const char *DefaultExtension, const char *InitialFileBase);
bool cWinMain_SelectFolderDialog(const char *Title, cStr *SelectedFolder, const char *InitialFolder);
};
#include <tlhelp32.h>
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
#define __OPENSCRIPTING__
#include <Carbon/Carbon.h>
void cMacMain_GetMainWindowRectGlobalSpace(comms::cRect *MainWindowRectGlobalSpace);
void cMacMain_GetRootDiskUUID(comms::cStr *UUID);
void cMacMain_RestartThisApplication();
bool cMacMain_InputString(const comms::cVec2i &Pos, comms::cStr *String);
bool cMacMain_CheckProcessInMemory(const char *ProcName);
namespace comms {
bool cMacMain_LoadFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *SingleFilePn, cList<cStr> *MultiFilePn, const char *PrefKey, const char *InitialFileName);
bool cMacMain_SaveFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *FilePn, const char *PrefKey, const char *DefaultExtension, const char *InitialFileBase);
bool cMacMain_SelectFolderDialog(const char *Title, cStr *SelectedFolder, const char *InitialFolder);
};
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
bool cLinuxMain_CheckProcessInMemory(const char *ProcName);
#include <glib.h>
#include <glib/gstdio.h>
namespace comms {
bool cLinuxMain_InputString(cStr *);
bool cLinuxMain_LoadFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *SingleFilePn, cList<cStr> *MultiFilePn, const char *PrefKey, const char *InitialFileName);
bool cLinuxMain_SaveFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *FilePn, const char *PrefKey, const char *DefaultExtension, const char *InitialFileBase);
bool cLinuxMain_SelectFolderDialog(const char *Title, cStr *SelectedFolder, const char *InitialFolder);
};
#include <sys/statfs.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <fstab.h>
#endif // COMMS_LINUX

namespace comms {


//*****************************************************************************
// SourceFolder
//*****************************************************************************
class SourceFolder : public cIO::Source {
private:
	cStr m_ReadFolder, m_WriteFolder;
	bool LoadAbsPath_NoWritePath(const char *FileAbsPn, cFile *To) const;
public:
	SourceFolder(const char *ReadFolder, const char *WriteFolder) : cIO::Source() {
		m_ReadFolder = ReadFolder;
		m_WriteFolder = WriteFolder;
	}
	virtual bool Load(const char *FilePn, cFile *To) const;
	virtual bool Save(const char *FilePn, const void *Src, const size_t Size, const bool Append) const;

	bool LoadAbsPath(const char *FileAbsPn, cFile *To) const;
	bool SaveAbsPath(const char *FileAbsPn, const void *Src, const size_t Size, const bool Append) const;

	virtual const cStr & GetPath() const {
		return m_ReadFolder;
	}
	void SetReadFolder(const char *ReadFolder) {
		m_ReadFolder = ReadFolder;
	}
	void SetWriteFolder(const char *WriteFolder) {
		m_WriteFolder = WriteFolder;
	}
	const cStr & GetWriteFolder() const {
		return m_WriteFolder;
	}
};

// SourceFolder::LoadAbsPath
bool SourceFolder::LoadAbsPath(const char *FileAbsPn, cFile *To) const {
	cAssert(To != nullptr);
	To->Clear();
	cStr Pn = cIO::EnsureAbsolutePath(FileAbsPn);
	// Alternative write path
	cStr W = Pn;
	if(cIO::ReplaceReadPath(&W)) {
		if(LoadAbsPath_NoWritePath(W.ToCharPtr(), To)) {
			return true;
		}
	}
	return LoadAbsPath_NoWritePath(Pn.ToCharPtr(), To);
}

//-----------------------------------------------------------------------------
// SourceFolder::LoadAbsPath_NoWritePath
//-----------------------------------------------------------------------------
bool SourceFolder::LoadAbsPath_NoWritePath(const char *FileAbsPn, cFile *To) const {
	cStr Pn = FileAbsPn;

#ifdef COMMS_IOS
	Pn.BackSlashesToSlashes(); // '\' -> '/'
    return cIosMain_LoadFile(Pn.ToCharPtr(), To);
#endif // COMMS_IOS
    
#ifdef COMMS_TIZEN
	Pn.BackSlashesToSlashes(); // '\' -> '/'
    return cTizenMain_LoadFile(Pn.ToCharPtr(), To);
#endif // COMMS_TIZEN

#if defined COMMS_WINDOWS
	cList<byte> Bits;
	HANDLE hFile;
	DWORD l, r;

	Pn.SlashesToBackSlashes(); // '/' -> '\'

	hFile = CreateFileW(comms::wchar_path(FileAbsPn), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if(INVALID_HANDLE_VALUE == hFile) {
		return false;
	}
	
	l = GetFileSize(hFile, nullptr);
	Bits.SetCount(l);
	
	if(!ReadFile(hFile, Bits.ToPtr(), l, &r, nullptr)) { // I/ error
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);
	
	if(r != l) { // I/ error
		return false;
	}
	To->Set(&Bits, Pn);
#endif // COMMS_WINDOWS

#if defined COMMS_MACOS || defined COMMS_LINUX
	cList<byte> Bits;
	FILE *fp = nullptr;
	int l, r;
	
	Pn.BackSlashesToSlashes(); // '\' -> '/'
	
	fp = fopen(Pn.ToCharPtr(), "rb");
	if(nullptr == fp) {
		return false;
	}
	
	fseek(fp, 0, SEEK_END);
	l =  (int)ftell(fp);
	if(l < 0) {
		fclose(fp);
		return false;
	}

	rewind(fp);

	Bits.SetCount(l);

	r = (int)fread(Bits.ToPtr(), 1, l, fp);
	fclose(fp);
	
	if(r != l) { // I/ error
		return false;
	}
	To->Set(&Bits, Pn);
#endif // COMMS_MACOS || COMMS_LINUX

	return true;
} // SourceFolder::LoadAbsPath

//-----------------------------------------------------------------------------
// SourceFolder::Load
//-----------------------------------------------------------------------------
bool SourceFolder::Load(const char *FilePn, cFile *To) const {
	cStr AbsPn(m_ReadFolder);
	AbsPn.AppendPath(FilePn);
	return LoadAbsPath(AbsPn, To);
} // SourceFolder::Load

//------------------------------------------------------------------------------------------------------------------
// SourceFolder::SaveAbsPath
//------------------------------------------------------------------------------------------------------------------
bool SourceFolder::SaveAbsPath(const char *FileAbsPn, const void *Src, const size_t Size, const bool Append) const {
#if defined COMMS_IOS || defined COMMS_TIZEN
	return false;
#endif // COMMS_IOS || COMMS_TIZEN

	// File pathname should not be "nullptr"
	cAssert(FileAbsPn != nullptr);
	if(nullptr == FileAbsPn) {
		return false;
	}
	// Path should be absolute (local or network)
	cAssert(cIO::PathIsAbsolute(FileAbsPn));
	if(!cIO::PathIsAbsolute(FileAbsPn)) {
		return false;
	}
	// Data block should not be "nullptr"
	cAssert(Src != nullptr);
	if(nullptr == Src) {
		return false;
	}
	// File pathname should not be empty
	cStr Pn = FileAbsPn;
	if(Pn.GetFileName().IsEmpty()) {
		return false;
	}
	// Alternative write folder
	cIO::ReplaceReadPath(&Pn);

#if defined COMMS_WINDOWS
	Pn.SlashesToBackSlashes(); // '/' -> '\'
	HANDLE hFile = CreateFileW(comms::wchar_path(Pn), GENERIC_WRITE, 0, nullptr, Append ? OPEN_ALWAYS : CREATE_ALWAYS, 0, nullptr);
	if(INVALID_HANDLE_VALUE == hFile) {
		return false;
	}
	if(Append) {
		SetFilePointer(hFile, 0, nullptr, FILE_END);
	}
	DWORD w = 0;
	WriteFile(hFile, Src, (DWORD)Size, &w, nullptr);
	CloseHandle(hFile);
	cIO::SetFilePermissions(Pn.ToCharPtr());
	if(w != Size) {
		return false;
	}
#endif // COMMS_WINDOWS

#if defined COMMS_MACOS || defined COMMS_LINUX
	Pn.BackSlashesToSlashes(); // '\' -> '/'
	cStr Flags = Append ? "a" : "w";
	Flags += "b";
	FILE *fp = fopen(Pn.ToCharPtr(), Flags.ToCharPtr());
	if(nullptr == fp) {
		return false;
	}
	size_t w = fwrite(Src, 1, Size, fp);
	fclose(fp);
	cIO::SetFilePermissions(Pn.ToCharPtr());
	if(w != Size) {
		return false;
	}
#endif // COMMS_MACOS || COMMS_LINUX
	
    return true;
} // SourceFolder::SaveAbsPath

//*****************************************************************************
// cIO
//*****************************************************************************
static cStr cIO_ThisFilePathName;
static cList<cIO::Source *> cIO_Sources;
static cList<cImageCodecInfo> cIO_ImageCodecs;

static cStr cIO_ModelAnimCameraBspDialogsInitialPath = "Models", cIO_ModelDialogInitialFile, cIO_AnimDialogInitialFile, cIO_BspDialogInitialFile, cIO_CameraDialogInitialFile;
void cIO::SetModelAnimCameraBspDialogsInitialPath(const char *InitialPath) {
	cIO_ModelAnimCameraBspDialogsInitialPath = InitialPath;
}
const cStr & cIO::GetModelAnimCameraBspDialogsInitialPath() {
	return cIO_ModelAnimCameraBspDialogsInitialPath;
}
void cIO::SetModelDialogInitialFile(const char *InitialFile) {
	cIO_ModelDialogInitialFile = InitialFile;
}
void cIO::SetAnimDialogInitialFile(const char *InitialFile) {
	cIO_AnimDialogInitialFile = InitialFile;
}
void cIO::SetBspDialogInitialFile(const char *InitialFile) {
	cIO_BspDialogInitialFile = InitialFile;
}
void cIO::SetCameraDialogInitialFile(const char *InitialFile) {
	cIO_CameraDialogInitialFile = InitialFile;
}

static cStr cIO_ImageDialogInitialPath = "data/Textures", cIO_ImageDialogInitialFile;
void cIO::SetImageDialogInitialPath(const char *InitialPath) {
	cIO_ImageDialogInitialPath = InitialPath;
}
void cIO::SetImageDialogInitialFile(const char *InitialFile) {
	cIO_ImageDialogInitialFile = InitialFile;
}
const cStr & cIO::GetImageDialogInitialPath() {
	return cIO_ImageDialogInitialPath;
}
const cStr & cIO::GetImageDialogInitialFile() {
	return cIO_ImageDialogInitialFile;
}

static cStr cIO_XmlDialogInitialPath = "Xmls", cIO_XmlDialogInitialFile;
void cIO::SetXmlDialogInitialPath(const char *InitialPath) {
	cIO_XmlDialogInitialPath = InitialPath;
}
void cIO::SetXmlDialogInitialFile(const char *InitialFile) {
	cIO_XmlDialogInitialFile = InitialFile;
}
const cStr & cIO::GetXmlDialogInitialPath() {
	return cIO_XmlDialogInitialPath;
}
const cStr & cIO::GetXmlDialogInitialFile() {
	return cIO_XmlDialogInitialFile;
}

// cIO_FillThisFilePathName
static void cIO_FillThisFilePathName() {
#ifdef COMMS_LINUX
	pid_t pid = getpid();
	char b[10];
	sprintf(b, "%d", pid);
	cStr l = "/proc/";
	l.Append(b);
	l.Append("/exe");
	char p[512];
	int c = readlink(l.ToCharPtr(), p, 512);
	if(c != -1) {
		p[c] = 0;
	}
	cStr t = p;
	cIO_ThisFilePathName = t;
#endif // COMMS_LINUX
#ifdef COMMS_WINDOWS
	wchar_t T[WCHAR_MAX_PATH];
	GetModuleFileNameW(nullptr, T, WCHAR_MAX_PATH);
	cIO_ThisFilePathName.Copy(T);
#endif // COMMS_WINDOWS
#ifdef COMMS_MACOS
	cStr T(512);
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFURLRef bundleURL = CFBundleCopyBundleURL(mainBundle);
	FSRef bundleFSRef;
	CFURLGetFSRef(bundleURL, &bundleFSRef);
	FSRefMakePath(&bundleFSRef, (unsigned char *)T.ToCharPtr(), T.Length());
	T.CalcLength();
	cIO_ThisFilePathName = T;
#endif // COMMS_MACOS
}

//-----------------------------------------------------------------------------
// cIO_Init
//-----------------------------------------------------------------------------
static void cIO_Init() {
	static bool Inited = false;
	if(Inited) {
		return;
	}
	Inited = true;

	cIO_FillThisFilePathName();
	cStr Folder = cIO_ThisFilePathName;
	Folder.RemoveFileName();

    const char *AllDataFolders[] = { "3d-coat-data-2020", "3dc-paint-2021", "3dcoatprint-2021" };
    const char *CoatData = AllDataFolders[0];
    const char *ReplaceWhatWith[] {
        // "3DCoat" executable is inside "3d-coat-data-2020" under Windows
        ".bin64", "",
    	"bin64", "",

		// "3DCoat" bundle is inside Xcode default build dirs under macOS.
		// Workaround for "3DCoat/Xcode > File > Project Settings... > Advanced... > Build Location = Legacy"
		"3dcoat/3D-CoatV4/build/Debug", CoatData,
		"3dcoat/3D-CoatV4/build/Release", CoatData,

		// VS2019 default build dirs under Windows
		"x64\\Debug", "",
		"x64\\Release", "",

        // Xcode default build dirs under macOS
        "build/Debug", "",
        "build/Release", ""
    };
	const int c = sizeof(ReplaceWhatWith) / sizeof(ReplaceWhatWith[0]);
    cAssertM(0 == (c & 1), "Parity check");
    int i, ReplaceIndex = -1;
	for(i = 0; i < c; i += 2) {
        const char *R = ReplaceWhatWith[i];
		if(Folder.EndsWith(R, true)) {
            ReplaceIndex = i;
			break;
		}
	}
    if(ReplaceIndex != -1) {
        const char *R = ReplaceWhatWith[ReplaceIndex];
		cLog::Message("Program is running inside compiler output directory \"%s\"", R);
		Folder.Remove(Folder.Length() - cStr::Length(R));
        const char *W = ReplaceWhatWith[ReplaceIndex + 1];
        Folder.Append(W);
	}
    
	cIO_Sources.Add(new SourceFolder(Folder, nullptr));
	cMain_OnInitPath(&Folder); // Override default file source after adding the prev entry
	Folder = cIO::EnsureAbsolutePath(Folder.ToCharPtr());
	((SourceFolder *)cIO_Sources[0])->SetReadFolder(Folder.ToCharPtr());
	cLog::Message("Default file source is folder \"%s\"", Folder.ToCharPtr());
	
	// Image codecs
	cIO::AddCodec("Dds", new cCodecDds);
	cIO::AddCodec("Bmp", new cCodecBmp);
#ifdef COMMS_JPEG
	cIO::AddCodec("Jpg", new cCodecJpeg);
	cIO::AddCodec("Jpeg", new cCodecJpeg);
#endif
	cIO::AddCodec("Tga", new cCodecTga);
#ifdef COMMS_PNG
	cIO::AddCodec("Png", new cCodecPng);
#endif
#ifdef COMMS_TIFF
	cIO::AddCodec("Tif", new cCodecTiff);
	cIO::AddCodec("Tiff", new cCodecTiff);
#endif
	cIO::AddCodec("Bin", new cCodecBin);
} // cIO_Init

// cIO::GetSources
const cList<cIO::Source *> & cIO::GetSources() {
	cIO_Init();
	return cIO_Sources;
}

// cIO::GetSourcesMutable
cList<cIO::Source *> & cIO::GetSourcesMutable() {
	cIO_Init();
	return cIO_Sources;
}

// cIO::CheckReadPath
bool cIO::CheckReadPath(const char *Path) {
	cStr S = Path;
	return ReplaceReadPath(&S);
}
bool cIO::ReplaceReadPathForExistingFile(cStr* Path) {
	cStr fp = EnsureAbsolutePath(*Path);
	cStr fp1 = fp;
	ReplaceReadPath(&fp1);
	FILE* F = nullptr;
#ifdef COMMS_WINDOWS
	_wfopen_s(&F, wchar_path(fp1.ToCharPtr()), wchar_path("rb"));
#else // Linux, macOS
	F = fopen(fp1.ToCharPtr(), "rb");
#endif // Windows
	if (F) {
		fclose(F);
		*Path = fp1;
		return true;
	}
#ifdef COMMS_WINDOWS
	_wfopen_s(&F, wchar_path(fp.ToCharPtr()), wchar_path("rb"));
#else // Linux, macOS
	F = fopen(fp.ToCharPtr(), "rb");
#endif // Windows
	if (F) {
		fclose(F);
		*Path = fp;
		return false;
	}
	return false;
}
// cIO::ReplaceReadPath
bool cIO::ReplaceReadPath(cStr *Path) {
	cIO_Init();

	const cStr &WriteFolder = ((SourceFolder *)cIO_Sources[0])->GetWriteFolder();
	if(WriteFolder.IsEmpty()) {
		return false;
	}
	cStr P = EnsureAbsolutePath(Path->ToCharPtr());
	const cStr &D = GetDefaultSourceFolder();
	if(!cStr::EqualsPath(P, D, D.Length()) || cStr::EqualsPath(P, WriteFolder, WriteFolder.Length())) {
		return false;
	}
	cStr T = WriteFolder;
	P.Remove(0, D.Length());
	T.AppendPath(P);
	T = EnsureAbsolutePath(T.ToCharPtr());
	*Path = T;
	return true;
}

//------------------------------------------------------------------------------------------------------------
// cIO::CreatePath
//------------------------------------------------------------------------------------------------------------
bool cIO::CreatePath(const char *PathOrFilePn, bool inUserDocuments) {
	cList<cStr> L;
	cStr CurPath;
	
	cStr Pn = EnsureAbsolutePath(PathOrFilePn);
	if(!Pn.GetFileExtension().IsEmpty()) {
		Pn.RemoveFileName();
		if (Pn.Length())Pn.EnsureTrailingPlatformSlash();
	}

	// Alternative write path
	if (inUserDocuments)ReplaceReadPath(&Pn);
	
#if defined COMMS_WINDOWS
	// We should create (check existence of) all intermediate directories.
	// Decomposing absolute path.
	Pn.Split(&L, "\\");

	// List should contain at least one element: drive letter for local path or network resource.
	// All above index 0 are intermediate directories.
	if(L.IsEmpty()) {
		return false;
	}
	CurPath = L[0];
	// If path is network we should add two backslashes at the beginning
	if('\\' == Pn[0] && '\\' == Pn[1]) {
		CurPath.Insert(0, "\\\\");
	}

	// Enumerating all intermediate directories and creating (checking existence of) them
	int i;
	for(i = 1; i < L.Count(); i++) {
		CurPath.AppendPath(L[i]);
		if(!CreateDirectoryW(comms::wchar_path(CurPath), nullptr)) {
			// If intermediate directory is created, that's ok.
			// But if not, it should be already exists.
			if(GetLastError() != ERROR_ALREADY_EXISTS) {
				return false;
			}
		}
	}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	Pn.Split(&L, "/");
	
	if(L.IsEmpty()) {
		return true; // Root path
	}
	CurPath = "/";
	FSRef Parent, Ref;
	Boolean Dir;
	OSErr E;
	CFStringRef Sr;
	UniCharCount uc;
	UniChar *U;
	CFRange Rg;
	
	// Parent
	E = FSPathMakeRef((const UInt8 *)CurPath.ToCharPtr(), &Parent, &Dir);
	if(E != noErr || !Dir) {
		return false; // Can't get ref to root
	}
	int i;
	for(i = 0; i < L.Count(); i++) {
		CurPath.AppendPath(L[i]);
		CurPath.EnsureTrailingBackslash();
		CurPath.BackSlashesToSlashes(); // '\' -> '/'
		E = FSPathMakeRef((const UInt8 *)CurPath.ToCharPtr(), &Ref, &Dir);
		if(noErr == E) {
			Parent = Ref;
			continue; // Folder already exists
		}
		// Convert to unicode
		Sr = CFStringCreateWithCString(nullptr, L[i], kCFStringEncodingUTF8);
		uc = CFStringGetLength(Sr);
		U = new UniChar[uc];
		Rg.location = 0;
		Rg.length = uc;
		CFStringGetCharacters(Sr, Rg, U);
		// We should create this folder						
		E = FSCreateDirectoryUnicode(&Parent, uc, U, kFSCatInfoNone, nullptr, &Ref, nullptr, nullptr);
		
		CFRelease(Sr);
		delete[] U;
		
		if(E != noErr) {
			return false; // Can't create folder
		}
		
		Parent = Ref;
	}
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
    Pn.Split(&L, "/");

    if(L.IsEmpty()) {
        return true; // Root path
    }
    CurPath = "/";

    gboolean b;
	int i;
    for(i = 0; i < L.Count(); i++) {
        CurPath.AppendPath(L[i]);
        CurPath.BackSlashesToSlashes(); // '\' -> '/'
        b = g_file_test(CurPath.ToCharPtr(), G_FILE_TEST_EXISTS);
        if(!b) {
			const int P = (ACCESSPERMS ^ S_IWOTH); // 775 rwxrwxr-x
            g_mkdir(CurPath.ToCharPtr(), P); // See "$ stat -c %a MyDir/" and "$ stat -c %A MyDir/"
        }
    }
#endif // COMMS_LINUX

	return true;
} // cIO::CreatePath

//-------------------------------------------------------------------------------------
// cIO::SetFilePermissions
//-------------------------------------------------------------------------------------
void cIO::SetFilePermissions(const char *FilePn) {
	cStr Pn = EnsureAbsolutePath(FilePn);

	// Alternative write path
	ReplaceReadPath(&Pn);

#if defined COMMS_WINDOWS
	SetFileAttributesW(wchar_path(Pn.ToCharPtr()), FILE_ATTRIBUTE_NORMAL);
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	FSRef Ref;
	Boolean Dir = false;
	FSCatalogInfo Info;
	FSPermissionInfo *Perm;
	if(noErr == FSPathMakeRef((const UInt8 *)Pn.ToCharPtr(), &Ref, &Dir)) {
		if(noErr == FSGetCatalogInfo(&Ref, kFSCatInfoPermissions, &Info, nullptr, nullptr, nullptr)) {
			Perm = (FSPermissionInfo *)&Info.permissions;
			Perm->mode |= (1 << 7) + (1 << 8);
			FSSetCatalogInfo(&Ref, kFSCatInfoPermissions, &Info);
		}
	}
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
	// Linux user asked to not change permissions
#endif // COMMS_LINUX
} // cIO::SetFilePermissions

//-------------------------------------------------------------------------------------
// cIO::SetFolderPermissions
//-------------------------------------------------------------------------------------
void cIO::SetFolderPermissions(const char *Folder) {
	cStr F = EnsureAbsolutePath(Folder);

	// Alternative write path
	ReplaceReadPath(&F);

#if defined COMMS_WINDOWS
	// There's nothing to do here...
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	FSRef Ref;
	Boolean Dir = false;
	FSCatalogInfo Info;
	FSPermissionInfo *Perm;
	if(noErr == FSPathMakeRef((const UInt8 *)F.ToCharPtr(), &Ref, &Dir)) {
		if(Dir) {
			if(noErr == FSGetCatalogInfo(&Ref, kFSCatInfoPermissions, &Info, nullptr, nullptr, nullptr)) {
				Perm = (FSPermissionInfo *)&Info.permissions;
				Perm->mode |= (1 << 6) + (1 << 7) + (1 << 8);
				FSSetCatalogInfo(&Ref, kFSCatInfoPermissions, &Info);
			}
		}
	}
#endif // COMMS_MACOS
	
#ifdef COMMS_LINUX
	// Linux user asked to not change permissions
#endif // COMMS_LINUX
} // cIO::SetFolderPermissions

//-------------------------------------------------------------------------------------
// SourceFolder::Save
//-------------------------------------------------------------------------------------
bool SourceFolder::Save(const char *FilePn, const void *Src, const size_t Size, const bool Append) const {
	cStr AbsPn(m_WriteFolder.IsEmpty() ? m_ReadFolder : m_WriteFolder);
	AbsPn.AppendPath(FilePn);
	return SaveAbsPath(AbsPn, Src, Size, Append);
} // SourceFolder::Save

//--------------------------------------------------------------------------------------------------------------------------
// SearchItems_AbsolutePath
//--------------------------------------------------------------------------------------------------------------------------
static void SearchItems_AbsolutePath(const char *AbsolutePath, const char *Folder, cList<cStr> *Items, const bool Folders) {
	cAssert(Items != nullptr);
	Items->Clear();

    cStr F = AbsolutePath, P;

#ifdef COMMS_WINDOWS
	cList<cStr> L;

	HANDLE h = nullptr;
	WIN32_FIND_DATAW fd;
	int i;

	wchar_t CurDir[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, CurDir);
	if(!SetCurrentDirectoryW(comms::wchar_path(F))) {
		return;
	}
	if((h = FindFirstFileW(L"*.*", &fd)) != INVALID_HANDLE_VALUE) {
		if((Folders && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) || (!Folders && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
			cStr res = fd.cFileName;
			L.Add(res);
		}
	} else {
		return;
	}
	while(FindNextFileW(h, &fd) != 0) {
		if((Folders && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) || (!Folders && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
			cStr res = fd.cFileName;
			L.Add(res);
		}
	}
	FindClose(h);

	SetCurrentDirectoryW(CurDir);

	for(i = 0; i < L.Count(); i++) {
		const cStr &S = L[i];
		if(S == "." || S == "..") {
			continue;
		}
		P = Folder;
		P.AppendPath(S);
		Items->Add(P);
	}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	FSRef Ref;
	Boolean Dir = false;
	OSErr r;
	if(FSPathMakeRef((const UInt8 *)F.ToCharPtr(), &Ref, &Dir) != noErr) {
		return;
	}
	if(!Dir) {
		return;
	}
	FSIterator It;
	if(FSOpenIterator(&Ref, kFSIterateFlat, &It) != noErr) {
		return;
	}
	const ItemCount kWantOneItem = 1;
	Boolean *kDontCareIfContainerChanged = nullptr;
	ItemCount c;
	HFSUniStr255 Name;
	FSCatalogInfo Info;
	FSRef ItemRef;
	FSSpec *kDontWantFSSpecs = nullptr;
	CFStringRef sr;
	cStr T;
	do {
		r = FSGetCatalogInfoBulk(It, kWantOneItem, &c, kDontCareIfContainerChanged,
		kFSCatInfoNodeFlags, &Info, &ItemRef, kDontWantFSSpecs, &Name);
		if(noErr == r) {
			if((Folders && (Info.nodeFlags & (1 << kFSNodeIsDirectoryBit)) != 0) || (!Folders && (Info.nodeFlags & (1 << kFSNodeIsDirectoryBit)) == 0)) {
				sr = CFStringCreateWithCharacters(kCFAllocatorDefault, Name.unicode, Name.length);
				T.SetLength(512);
				CFStringGetCString(sr, T.ToNonConstCharPtr(), T.Length(), 0);
				T.CalcLength();
				P = Folder;
				P.AppendPath(T);
				Items->Add(P);
				CFRelease(sr);
			}
		}
	} while(noErr == r);
	FSCloseIterator(It);
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
        GDir *D = g_dir_open(F.ToCharPtr(), 0, nullptr);
        if(nullptr == D) {
            return;
        }
        const gchar *t;
        gboolean b;
        while((t = g_dir_read_name(D))) {
            // Filter
            P = F;
            P.AppendPath(t);
            P.BackSlashesToSlashes();
            b = g_file_test(P.ToCharPtr(), G_FILE_TEST_IS_DIR);
            if(Folders != b) {
                continue;
            }
            // Add
            P = Folder;
            P.AppendPath(t);
            Items->Add(P);
        }
        g_dir_close(D);
#endif // COMMS_LINUX

} // SearchItems_AbsolutePath

static void SearchItems_AbsolutePath(const char* AbsolutePath, const char* Folder, cList<cStr*>* Items, const bool Folders) {
	cAssert(Items != nullptr);
	Items->FreeContents();
	Items->Clear();

	cStr F = AbsolutePath, P;

#ifdef COMMS_WINDOWS
	cList<cStr> L;

	HANDLE h = nullptr;
	WIN32_FIND_DATA fd;
	int i;

	char CurDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, CurDir);
	if (!SetCurrentDirectory(F.ToCharPtr())) {
		return;
	}
	cStr Mask = "*.*";
	if ((h = FindFirstFile(Mask, &fd)) != INVALID_HANDLE_VALUE) {
		if ((Folders && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) || (!Folders && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
			L.Add(fd.cFileName);
		}
	}
	else {
		return;
	}
	while (FindNextFile(h, &fd) != 0) {
		if ((Folders && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) || (!Folders && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
			L.Add(fd.cFileName);
		}
	}
	FindClose(h);

	SetCurrentDirectory(CurDir);

	for (i = 0; i < L.Count(); i++) {
		const cStr& S = L[i];
		if (S == "." || S == "..") {
			continue;
		}
		P = Folder;
		P.AppendPath(S);
		Items->Add(new cStr(P));
	}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	FSRef Ref;
	Boolean Dir = false;
	OSErr r;
	if (FSPathMakeRef((const UInt8*)F.ToCharPtr(), &Ref, &Dir) != noErr) {
		return;
	}
	if (!Dir) {
		return;
	}
	FSIterator It;
	if (FSOpenIterator(&Ref, kFSIterateFlat, &It) != noErr) {
		return;
	}
	const ItemCount kWantOneItem = 1;
	Boolean* kDontCareIfContainerChanged = nullptr;
	ItemCount c;
	HFSUniStr255 Name;
	FSCatalogInfo Info;
	FSRef ItemRef;
	FSSpec* kDontWantFSSpecs = nullptr;
	CFStringRef sr;
	cStr T;
	do {
		r = FSGetCatalogInfoBulk(It, kWantOneItem, &c, kDontCareIfContainerChanged,
			kFSCatInfoNodeFlags, &Info, &ItemRef, kDontWantFSSpecs, &Name);
		if (noErr == r) {
			if ((Folders && (Info.nodeFlags & (1 << kFSNodeIsDirectoryBit)) != 0) || (!Folders && (Info.nodeFlags & (1 << kFSNodeIsDirectoryBit)) == 0)) {
				sr = CFStringCreateWithCharacters(kCFAllocatorDefault, Name.unicode, Name.length);
				T.SetLength(512);
				CFStringGetCString(sr, T.ToNonConstCharPtr(), T.Length(), 0);
				T.CalcLength();
				P = Folder;
				P.AppendPath(T);
				Items->Add(new cStr(P));
				CFRelease(sr);
			}
		}
	} while (noErr == r);
	FSCloseIterator(It);
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
	GDir* D = g_dir_open(F.ToCharPtr(), 0, nullptr);
	if (nullptr == D) {
		return;
	}
	const gchar* t;
	gboolean b;
	while ((t = g_dir_read_name(D))) {
		// Filter
		P = F;
		P.AppendPath(t);
		P.BackSlashesToSlashes();
		b = g_file_test(P.ToCharPtr(), G_FILE_TEST_IS_DIR);
		if (Folders != b) {
			continue;
		}
		// Add
		P = Folder;
		P.AppendPath(t);
		Items->Add(new cStr(P));
	}
	g_dir_close(D);
#endif // COMMS_LINUX

} // SearchItems_AbsolutePath

// SearchItems
static void SearchItems(const char *Folder, cList<cStr> *Items, const bool Folders) {
	cStr F = cIO::EnsureAbsolutePath(Folder);
	SearchItems_AbsolutePath(F.ToCharPtr(), Folder, Items, Folders);
}

//-----------------------------------------------------------------------------
// cIO::PathIsAbsolute
//-----------------------------------------------------------------------------
bool cIO::PathIsAbsolute(const char *Path) {
#ifdef COMMS_WINDOWS
	if(cStr::Length(Path) < 2) {
		// Path is too short to fit even required attributes:
		// - drive letter with colon;
		// - two (back)slashes.
		return false;
	}
	
	// Is local?
	if(((Path[0] >= 'A' && Path[0] <= 'Z') || (Path[0] >= 'a' && Path[0] <= 'z')) && Path[1] == ':') {
		return true;
	}

	// Is network? (slashes will be replaced with back slashes by source)
	if((Path[0] == '\\' || Path[0] == '/') && (Path[1] == '\\' || Path[1] == '/')) {
		return true;
	}
	
	return false;
#endif // COMMS_WINDOWS

#if defined COMMS_MACOS || defined COMMS_LINUX
	if(cStr::Length(Path) < 1) {
		// Path is too short
		return false;
	}
	
	return Path[0] == '/' || Path[0] == '\\';
#endif // COMMS_MACOS || COMMS_LINUX
	
#if defined COMMS_IOS || defined COMMS_TIZEN
	return true;
#endif // COMMS_IOS || COMMS_TIZEN
	
} // cIO::PathIsAbsolute
bool cIO::MakeRelativePath(cStr* Path) {
	cIO_Init();
	if(Path) {
		if (!PathIsAbsolute(*Path))return true;
		const cStr& u = ((SourceFolder*)cIO_Sources[0])->GetWriteFolder();
		if (comms::cStr::ComparePath(u, *Path, u.Length()) == 0) {
			Path->Remove(0, u.Length());
			Path->TrimStart("\\/");
			return true;
		}
		const cStr& s = GetDefaultSourceFolder();
		if (comms::cStr::ComparePath(s, *Path, s.Length()) == 0) {
			Path->Remove(0, s.Length());
			Path->TrimStart("\\/");
			return true;
		}
	}
	return false;
}

// cIO_PathIsURL
static bool cIO_PathIsURL(const char *Path) {
    cList<cStr> Protocols;
    Protocols.Add("http://");
    Protocols.Add("https://");
    Protocols.Add("ftp://");
    int i;
    for(i = 0; i < Protocols.Count(); i++) {
        const cStr &P = Protocols[i];
        if(cStr::Length(Path) >= P.Length() && cStr::EqualsNoCase(P, Path, P.Length())) {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// cIO::EnsureAbsolutePath
//-----------------------------------------------------------------------------
const cStr cIO::EnsureAbsolutePath(const char *Path) {
    cIO_Init();
    if(cIO_PathIsURL(Path)) { // Ignore URLs
        return Path;
    }
    cStr T = Path;
    if(Path != nullptr && '~' == Path[0]) {
        T = GetUserFolder();
        T.AppendPath(&Path[1]);
    }
    cStr S;
    if(PathIsAbsolute(T)) {
        S = T;
    } else {
        S = GetDefaultSourceFolder();
        S.AppendPath(T);
    }
	S.MakePlatformSlashes();

#ifdef COMMS_LINUX
        // Since Linux has case sensitive FS, we should correct absolute path
        cList<cStr> L;
        S.Split(&L, "/");
        if(L.IsEmpty()) {
            return S; // Root path
        }
        cStr CurPath = "/";
        cStr PrevPath = CurPath;

        int i, j;
        gboolean b;
        cList<cStr> Folders, Files;
        cStr t;
        for(i = 0; i < L.Count(); i++) {
            CurPath.AppendPath(L[i]);
            CurPath.BackSlashesToSlashes(); // '\' -> '/'
            b = g_file_test(CurPath.ToCharPtr(), G_FILE_TEST_EXISTS);
            if(!b) {
                SearchItems_AbsolutePath(PrevPath, PrevPath, &Folders, true);
                Files.Clear();
                if(L.Count() - 1 == i) { // Last item can be file
                    SearchItems_AbsolutePath(PrevPath, PrevPath, &Files, false);
                }
                t.Clear();
                j = Files.IndexOf(CurPath, cStr::EqualsPath);
                if(j != -1) {
                    t = Files[j];
                } else {
                    j = Folders.IndexOf(CurPath, cStr::EqualsPath);
                    if(j != -1) {
                        t = Folders[j];
                    }
                }
                if(!t.IsEmpty()) {
                    CurPath = t;
                    CurPath.BackSlashesToSlashes(); // '\' -> '/'
                }
            }
            PrevPath = CurPath;
        }
		const bool Folder = S.EndsWith("/");
        S = CurPath;
		if(Folder) {
			S.EnsureTrailingSlash();
		}
#endif // COMMS_LINUX

	return S;
} // cIO::EnsureAbsolutePath

//-----------------------------------------------------------------------------
// cIO::LoadFile
//-----------------------------------------------------------------------------
bool cIO::LoadFile(const char *FilePn, cFile *To, const bool ShowWarning) {
	cIO_Init();

	cAssert(To != nullptr);
	if(nullptr == To) {
		return false;
	}
	
	To->Clear();
	
	cAssert(FilePn != nullptr);
	if(nullptr == FilePn) {
		return false;
	}

	// Is "FilePn" with absolute path?
	if(PathIsAbsolute(FilePn)) {
		// It seems like "FilePn" is with absolute path (local or network).
		// So load it directly by "SourceFolder".
		if(((SourceFolder *)cIO_Sources[0])->LoadAbsPath(FilePn, To)) {
			return true;
		}
	} else {
		// "FilePn" is w/o absolute path.
		// Trying to load it with all registered file sources...

		int i;
		for(i = 0; i < cIO_Sources.Count(); i++) {
			if(cIO_Sources[i]->Load(FilePn, To)) {
				To->SetPos(0); // Rewind the memory file to the beginning
				return true;
			}
		}
	}

	if(ShowWarning) {
		cLog::Warning("Can't load file \"%s\"", FilePn);
	}
	
	return false;
} // cIO::LoadFile

//---------------------------------------------------------------------------------------------------------------------
// cIO::SaveFile
//---------------------------------------------------------------------------------------------------------------------
bool cIO::SaveFile(const char *FilePn, const void *Src, const size_t Size, const bool Append, const bool ShowWarning) {
	cIO_Init();

	CreatePath(FilePn);

	cAssert(FilePn != nullptr);
	if(nullptr == FilePn) {
		return false;
	}
	
	cAssert(Src != nullptr);
	if(nullptr == Src) {
		return false;
	}

	// Is "FilePn" with absolute path?
	if(PathIsAbsolute(FilePn)) {
		// It seems like "FilePn" is with absolute path (local or network).
		// So save it directly by "SourceFolder".
		if(((SourceFolder *)cIO_Sources[0])->SaveAbsPath(FilePn, Src, Size, Append)) {
			return true;
		}
	} else {
		// "FilePn" is w/o absolute path.
		// Trying to save it with all registered file sources...
		int i;
		for(i = 0; i < cIO_Sources.Count(); i++) {
			if(cIO_Sources[i]->Save(FilePn, Src, Size, Append)) {
				return true;
			}
		}
	}
	if(ShowWarning) {
		cLog::Warning("Can't save file \"%s\"", FilePn);
	}
	return false;
} // cIO::SaveFile

// cIO::SaveFile
bool cIO::SaveFile(const char *FilePn, const cFile &Src, const bool Append, const bool ShowWarning) {
	// Selecting file pathname
	const char *Pn = FilePn != nullptr ? FilePn : Src.GetFilePn().ToCharPtr();
	if(0 == Src.Size()) {
		return false; // Nothing to save
	}
	return SaveFile(Pn, Src.ToPtr(), Src.Size(), Append, ShowWarning);
}

// cIO::LoadFileDialog : (..., cStr *, ...)
bool cIO::LoadFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *SingleFilePn, const char *PrefKey, const char *InitialFileName) {
#ifdef COMMS_WINDOWS
    return cWinMain_LoadFileDialog(Title, Extensions, SingleFilePn, nullptr, PrefKey, InitialFileName);
#endif // COMMS_WINDOWS
#ifdef COMMS_LINUX
    return cLinuxMain_LoadFileDialog(Title, Extensions, SingleFilePn, nullptr, PrefKey, InitialFileName);
#endif // COMMS_LINUX
#ifdef COMMS_MACOS
    return cMacMain_LoadFileDialog(Title, Extensions, SingleFilePn, nullptr, PrefKey, InitialFileName);
#endif // COMMS_MACOS
    return false;
}

// cIO::LoadFileDialog : (..., comms::cList<cStr> *, ...)
bool cIO::LoadFileDialog(const char *Title, const comms::cList<cStr> &Extensions, comms::cList<cStr> *MultiFilePn, const char *PrefKey, const char *InitialFileName) {
#ifdef COMMS_WINDOWS
    return cWinMain_LoadFileDialog(Title, Extensions, nullptr, MultiFilePn, PrefKey, InitialFileName);
#endif // COMMS_WINDOWS
#ifdef COMMS_LINUX
    return cLinuxMain_LoadFileDialog(Title, Extensions, nullptr, MultiFilePn, PrefKey, InitialFileName);
#endif // COMMS_LINUX
#ifdef COMMS_MACOS
    return cMacMain_LoadFileDialog(Title, Extensions, nullptr, MultiFilePn, PrefKey, InitialFileName);
#endif // COMMS_MACOS
    return false;
}

// cIO::SaveFileDialog
bool cIO::SaveFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *FilePn, const char *PrefKey, const char *DefaultExtension, const char *InitialFileBase) {
#ifdef COMMS_WINDOWS
    return cWinMain_SaveFileDialog(Title, Extensions, FilePn, PrefKey, DefaultExtension, InitialFileBase);
#endif // COMMS_WINDOWS
#ifdef COMMS_LINUX
    return cLinuxMain_SaveFileDialog(Title, Extensions, FilePn, PrefKey, DefaultExtension, InitialFileBase);
#endif // COMMS_LINUX
#ifdef COMMS_MACOS
    return cMacMain_SaveFileDialog(Title, Extensions, FilePn, PrefKey, DefaultExtension, InitialFileBase);
#endif // COMMS_MACOS
    return false;
}

// cIO::SelectFolderDialog
bool cIO::SelectFolderDialog(const char *Title, cStr *SelectedFolder, const char *InitialFolder) {
#ifdef COMMS_WINDOWS
    return cWinMain_SelectFolderDialog(Title, SelectedFolder, InitialFolder);
#endif // COMMS_WINDOWS
#ifdef COMMS_LINUX
    return cLinuxMain_SelectFolderDialog(Title, SelectedFolder, InitialFolder);
#endif // COMMS_LINUX
#ifdef COMMS_MACOS
	return cMacMain_SelectFolderDialog(Title, SelectedFolder, InitialFolder);
#endif // COMMS_MACOS
    return false;
}

//-----------------------------------------------------------------------------
// cIO::AddCodec : (..., ImageCodec *)
//-----------------------------------------------------------------------------
bool cIO::AddCodec(const char *FileExtension, cImageCodec *Codec) {
	cIO_Init();

	int i;
	for(i = cIO_ImageCodecs.Count()-1; i >= 0; i--) {
		if(cStr::EqualsNoCase(FileExtension, cIO_ImageCodecs[i].FileExtension)) {
			cIO_ImageCodecs.RemoveAt(i);
			//return false;
		}
	}

	cImageCodecInfo Info;
	Info.FileExtension = FileExtension;
	Info.Codec = Codec;
	cIO_ImageCodecs.Add(Info);

	cLog::Message("Added \"%s\" image codec.", Info.FileExtension.ToCharPtr());

	return true;
} // cIO::AddCodec : (..., ImageCodec *)

//-----------------------------------------------------------------------------
// cIO::LoadImage : (const char *, ...)
//-----------------------------------------------------------------------------
bool cIO::LoadImage(const char *FilePn, cImage *To) {
	cAssert(FilePn != nullptr);
	cAssert(To != nullptr);
	auto lo = LastOpenedResource();
	LastOpenedResource() = FilePn;
	cIO_Init();

	cStr P = EnsureAbsolutePath(FilePn);
	cStr M = P;
	M.RemoveFileAbsPath(GetDefaultSourceFolder());

	int i;
	bool Loaded = false, Decoded = false;
	cFile File;
#ifdef COMMS_IOS
    if(M.StartsWith("data/Textures", true)) {
        M.SetFileExtension(cCodecPvr::FileExtension); // Under iOS we should load "cPvr" textures
    }
#endif // COMMS_IOS
	cStr Fe = M.GetFileExtension();
	if(Fe.Contains("temp",true)) {
		cStr fn = M;
		fn.RemoveFileExtension();
		Fe = fn.GetFileExtension();		
	}
	// Load file
	Loaded = LoadFile(M, &File, false);
	if(Loaded) {
		dword D = File.ReadDword();
		File.SetPos(0);
		GetActualFileExtensionByMagic(D, Fe);
		// Look for codec
		for(i = 0; i < cIO_ImageCodecs.Count(); i++) {
			if(cStr::EqualsNoCase(cIO_ImageCodecs[i].FileExtension, Fe)) {
				// Try to decode image
				Decoded = cIO_ImageCodecs[i].Codec->Decode(File, To);
				break;
			}
		}
	}
	if(!Loaded) {
		//cLog::Warning("Can't load image file \"%s\"", M.ToCharPtr());
	} else if(!Decoded) { // File is loaded but it is not decoded
		cLog::Warning("Can't decode image file \"%s\"", M.ToCharPtr());
	} else {
		//cLog::Message("Loaded image file \"%s\"", M.ToCharPtr());
	}
	LastOpenedResource() = lo;
    return Loaded && Decoded; // Frankly speaking, decode is not possible w/o load :-)
} // cIO::LoadImage

//-----------------------------------------------------------------------------
// cIO::SaveImage
//-----------------------------------------------------------------------------
bool cIO::SaveImage(const char *FilePn, const cImage &Image) {
	cIO_Init();

	cAssert(FilePn != nullptr);

	cStr Pn, Fe;
	cImageCodec *Codec = nullptr;
	cFile File;

	Pn = FilePn;
	Fe = Pn.GetFileExtension();

	cAssert(!Fe.IsEmpty());
	if(Fe.IsEmpty()) { // There is no extension, therefore there is no criterion for codec choice
		return false;
	}

	// Look for codec
	int i;
	for(i = 0; i < cIO_ImageCodecs.Count(); i++) {
		if(cStr::EqualsNoCase(cIO_ImageCodecs[i].FileExtension, Fe)) {
			Codec = cIO_ImageCodecs[i].Codec;
			break;
		}
	}

	cAssert(Codec != nullptr);
	if(nullptr == Codec) {
		return false;
	}

	// Encode
    File.SetFilePn(FilePn);
	bool Encoded = false;
#ifdef COMMS_3DCOAT
	cImage* im2 = li_image(Image);
	if(im2) {
		Encoded = Codec->Encode(*im2, &File);
		delete(im2);
	}
	else {
#endif // COMMS_3DCOAT
		Encoded = Codec->Encode(Image, &File);
#ifdef COMMS_3DCOAT
	}
#endif // COMMS_3DCOAT
	if(!Encoded) {
		cLog::Warning("Can't encode image \"%s\"", FilePn);
		return false;
	}
	if(File.Size()==0)return true;
	return SaveFile(FilePn, File);
} // cIO::SaveImage

//-----------------------------------------------------------------------------
// cIO::LoadImageDialog
//-----------------------------------------------------------------------------
bool cIO::LoadImageDialog(cStr *ImageFilePn) {
	cAssert(ImageFilePn != nullptr);
#if defined COMMS_WINDOWS || defined COMMS_MACOS || defined COMMS_LINUX
	cList<cStr> Extensions;
	int i;
	for(i = 0; i < cIO::GetImageCodecs().Count(); i++) {
		Extensions.Add(cIO::GetImageCodecs()[i].FileExtension);
	}
	cStr InitialFileName, InitialFolder;
	if(!cIO_ImageDialogInitialFile.IsEmpty()) {
		InitialFileName = cIO_ImageDialogInitialFile.GetFileName();
		InitialFolder = cIO_ImageDialogInitialFile.GetFilePath();
	} else {
		InitialFolder = cIO_ImageDialogInitialPath;
	}
	const cStr PrefKey = "Image";
	SetFileDialogInitialFolder(PrefKey, InitialFolder);
	bool r = false;
#ifdef COMMS_WINDOWS
	cWin32::ImageFileDialog ifd(true, InitialFolder.ToCharPtr());
	if(ifd.DoModal(InitialFileName.ToCharPtr())) {
		*ImageFilePn = ifd.GetFilePathName();
		r = true;
	}
#else // !COMMS_WINDOWS = COMMS_MACOS || COMMS_LINUX
	r = LoadFileDialog("Load Image File", Extensions, ImageFilePn, PrefKey, InitialFileName);
#endif // COMMS_WINDOWS
	if(!r) {
		return false;
	}
	cIO_ImageDialogInitialPath = ImageFilePn->GetFilePath();
	cIO_ImageDialogInitialFile = *ImageFilePn;
	ImageFilePn->RemoveFileAbsPath(GetDefaultSourceFolder());
	return true;
#endif // COMMS_WINDOWS || COMMS_MACOS || COMMS_LINUX
	return false;
} // cIO::LoadImageDialog

//-----------------------------------------------------------------------------
// cIO::SaveImageDialog
//-----------------------------------------------------------------------------
bool cIO::SaveImageDialog(cStr *ImageFilePn) {
	cAssert(ImageFilePn != nullptr);
#if defined COMMS_WINDOWS || defined COMMS_MACOS || defined COMMS_LINUX
	cList<cStr> Extensions;
	int i;
	for(i = 0; i < GetImageCodecs().Count(); i++) {
		Extensions.Add(GetImageCodecs()[i].FileExtension);
	}
	cStr InitialFileName, InitialFileBase, InitialFolder, InitialExtension;
	int InitialExtensionIndex = 0;
	if(!cIO_ImageDialogInitialFile.IsEmpty()) {
		InitialFileName = cIO_ImageDialogInitialFile.GetFileName();
		InitialFileBase = cIO_ImageDialogInitialFile.GetFileBase();
		InitialFolder = cIO_ImageDialogInitialFile.GetFilePath();
		InitialExtension = cIO_ImageDialogInitialFile.GetFileExtension();
		if (!InitialExtension.IsEmpty()) {
			int i = Extensions.IndexOf(InitialExtension, cStr::EqualsNoCase);
			if (i != -1) {
				InitialExtensionIndex = i;
			}
		}
	} else {
		InitialFolder = cIO_ImageDialogInitialPath;
	}
	const cStr PrefKey = "Image";
	SetFileDialogInitialFolder(PrefKey, InitialFolder);
	bool r = false;
#ifdef COMMS_WINDOWS
	cWin32::ImageFileDialog ifd(false, InitialFolder.ToCharPtr());
	ifd.SetFilterIndex(InitialExtensionIndex);
	if (ifd.DoModal(InitialFileName.ToCharPtr())) {
		*ImageFilePn = ifd.GetFilePathName();
		r = true;
	}
#else // !COMMS_WINDOWS = COMMS_MACOS || COMMS_LINUX
	r = SaveFileDialog("Save Image File", Extensions, ImageFilePn, PrefKey, Extensions[0], InitialFileBase);
#endif // COMMS_WINDOWS
	if (!r) {
		return false;
	}
	cIO_ImageDialogInitialPath = ImageFilePn->GetFilePath();
	cIO_ImageDialogInitialFile = *ImageFilePn;
	return true;
#endif // COMMS_WINDOWS || COMMS_MACOS || COMMS_LINUX
	return false;
} // cIO::SaveImageDialog

// cIO::GetImageCodecs
const cList<cImageCodecInfo>& cIO::GetImageCodecs() {
	cIO_Init();
	return cIO_ImageCodecs;
}

// cIO::FindImageCodec
cImageCodec* cIO::FindImageCodec(const char* FileExtension) {
	cIO_Init();
	for (int i = 0; i < cIO_ImageCodecs.Count(); i++) {
		cImageCodecInfo& I = cIO_ImageCodecs[i];
		if (cStr::EqualsNoCase(I.FileExtension, FileExtension)) {
			return I.Codec;
		}
	}
	return nullptr;
}

bool cIO::CombineImages(const char* File1, const char* File2, const char* FileOut) {
	
	bool isCombined = false;

	cIO_Init();

	cAssert(File1 != nullptr);
	cAssert(File2 != nullptr);

	cStr F1 = File1, F2 = File2;
	//check if image1
	cStr ext = F1.GetFileExtension();
	if (ext.Contains("temp", true)) {
		cStr L = F1;
		L.RemoveFileExtension();
		ext = L.GetFileExtension();
	}
	comms::cImageCodec* co = comms::cIO::FindImageCodec(ext);
	if (!co) return false;

	//check if image2
	ext = F2.GetFileExtension();
	if (ext.Contains("temp", true)) {
		cStr L = F2;
		L.RemoveFileExtension();
		ext = L.GetFileExtension();
	}
	co = comms::cIO::FindImageCodec(ext);
	if (!co) return false;
	// file1 & file2 are image files

	cImage Image1;
	cImage Image2;

	if (!LoadImage(File1, &Image1)) return false;
	if (!LoadImage(File2, &Image2)) return false;

	if (Image1.GetHeight() != Image2.GetHeight() ||
		Image1.GetWidth() != Image2.GetWidth() ||
		Image1.GetDepth() != Image2.GetDepth()) return false;

	if (cFormat::IsCompressed(Image1.GetFormat())) {
		Image1.Uncompress();
	}

	if (cFormat::IsCompressed(Image2.GetFormat())) {
		Image2.Uncompress();
	}

	if (Image1.GetFormat() != cFormat::Rgba8) Image1.ToFormat(cFormat::Rgba8);
	if (Image2.GetFormat() != cFormat::Rgba8) Image2.ToFormat(cFormat::Rgba8);
	
	int W = Image1.GetWidth();
	int H = Image1.GetHeight();
	int D = Image1.GetDepth();
	int M = Image1.GetMipMapCount();

	int SpaceWidth = 32;
	int SizeOfPixel = cFormat::BytesPerPixel(Image1.GetFormat());
	int SrcRow = W * SizeOfPixel;
	int ThisRow = SrcRow;
	int ThisSize = ThisRow * (H * 2 + SpaceWidth);
	byte* pixels = new byte[ThisSize];
	const byte* From = Image1.GetPixels();
	byte* To = pixels;
	int i,j;
	for (i = 0; i < H; i++) {
		memcpy(To, From, ThisRow);
		From += SrcRow;
		To += ThisRow;
	}
	for (i = 0; i < SpaceWidth; i++) {
		for (j = 0; j < W; j++) {
			*To = 0xFF;
			*(To+1) = 0xFF;
			*(To+2) = 0xFF;
			*(To+3) = 0;
			To += 4;
		}
	}
	From = Image2.GetPixels();
	for (i = 0; i < H; i++) {
		memcpy(To, From, ThisRow);
		From += SrcRow;
		To += ThisRow;
	}
	cImage IOut;
	IOut.Set(pixels, cFormat::Rgba8, W, H * 2 + SpaceWidth, D, M);

	cStr fileOutPath = comms::cIO::EnsureAbsolutePath(FileOut);

	isCombined = cIO::SaveImage(fileOutPath.ToCharPtr(), IOut);
	return isCombined;
}

//cList<cVec2> points
bool cIO::CompareImages(const char* File1, const char* File2, const int Tolerance, cList<cVec2> *PtRes) {
	bool isEquals = true;
	
	cIO_Init();

	cAssert(File1 != nullptr);
	cAssert(File2 != nullptr);

	cStr F1 = File1, F2 = File2;
	//check if image1
	cStr ext = F1.GetFileExtension();
	if (ext.Contains("temp", true)) {
		cStr L = F1;
		L.RemoveFileExtension();
		ext = L.GetFileExtension();
	}
	comms::cImageCodec* co = comms::cIO::FindImageCodec(ext);
	if (!co) return false;

	//check if image2
	ext = F2.GetFileExtension();
	if (ext.Contains("temp", true)) {
		cStr L = F2;
		L.RemoveFileExtension();
		ext = L.GetFileExtension();
	}
	co = comms::cIO::FindImageCodec(ext);
	if (!co) return false;
	// file1 & file2 are image files

	cImage Image1;
	cImage Image2;

	if (!LoadImage(File1, &Image1)) return false;
	if (!LoadImage(File2, &Image2)) return false;

	if (Image1.GetHeight() != Image2.GetHeight() ||
		Image1.GetWidth() != Image2.GetWidth() ||
		Image1.GetDepth() != Image2.GetDepth()) return false;

	if (cFormat::IsCompressed(Image1.GetFormat())) {
		Image1.Uncompress();
	}

	if (cFormat::IsCompressed(Image2.GetFormat())) {
		Image2.Uncompress();
	}

	if (Image1.GetFormat() != cFormat::Rgba8) Image1.ToFormat(cFormat::Rgba8);
	if (Image2.GetFormat() != cFormat::Rgba8) Image2.ToFormat(cFormat::Rgba8);


	int X, Y;
	cImage::PixelRgba8 Rgba1, Rgba2;
	int H = Image1.GetHeight();
	int W = Image1.GetWidth();
	for (Y = 0; Y < H; Y++) {
		for (X = 0; X < W; X++) {
			Rgba1 = Image1.GetPixelRgba8(X, Y);
			Rgba2 = Image2.GetPixelRgba8(X, Y);
			if ((abs(Rgba1.a - Rgba2.a) > Tolerance) ||
				(abs(Rgba1.b - Rgba2.b) > Tolerance) ||
				(abs(Rgba1.g - Rgba2.g) > Tolerance) ||
				(abs(Rgba1.r - Rgba2.r) > Tolerance)) {
				isEquals = false;
				if (PtRes) {
					PtRes->Add(cVec2((float)X, (float)(H-Y-1)));
				} else break;
			}
		}
		if (!PtRes) { if (!isEquals) break; }
	}
	return isEquals;
}


cStr cImageCodecInfo::ErrorMessage;
bool cIO::GetActualFileExtensionByMagic(dword m, cStr& _ext){
	cStr ext = _ext.ToUpper();
	bool good = false;
	bool changed = false;
	for (int i = 0; i < cIO_ImageCodecs.Count(); i++) {
		cImageCodecInfo* cod = &cIO_ImageCodecs[i];
		if (ext.EqualsNoCase(ext, cod->FileExtension)){
			if (cod->Codec->CheckMagic(m, ext) == 1){
				good = true;
				break;
			}
		}
	}
	if (!good){
		for (int i = 0; i < cIO_ImageCodecs.Count(); i++) {
			cImageCodecInfo* cod = &cIO_ImageCodecs[i];
			if (cod->Codec->CheckMagic(m, cod->FileExtension) == 1){
				_ext = cod->FileExtension;
				changed = true;
				break;
			}
		}
	}
	return !changed;
}

cStr& cIO::LastOpenedResource() {
	static cStr s;
	return s;
}

bool cIO::GetActualFileExtensionByMagic(const char *FilePn, cStr& ext){
	cStr s = EnsureAbsolutePath(FilePn);
	ReplaceReadPath(&s);
	FILE *F = nullptr;
#ifdef COMMS_WINDOWS
	_wfopen_s(&F, wchar_path(s), wchar_path("rb"));
#else // Linux, macOS
	F = fopen(s, "rb");
#endif // COMMS_WINDOWS
	dword m = 0;
	ext = s.GetFileExtension();
	if (F){
		fread(&m, 1, 4, F);
		fclose(F);
		cStr ext0 = ext;
		bool r = GetActualFileExtensionByMagic(m, ext);
		if (ext0.Contains("temp") == false && !r){
			cStr fn = FilePn;
			fn.RemoveFilePath();
			cImageCodecInfo::ErrorMessage = "Incorrect extension of ";
			cImageCodecInfo::ErrorMessage += fn;
			cImageCodecInfo::ErrorMessage += ".\n The file looks like ";
			cImageCodecInfo::ErrorMessage += ext;
			cImageCodecInfo::ErrorMessage += ".";
		}
		return r;
	}
	return false;
}
// cIO::SaveScreenShot
void cIO::SaveScreenShot(const cImage &I, const char* OptionalSuffix) {
	static cStr DefPath = cIO::GetUserFolder();
	cStr T = cTimer::GetLocalTime(); T.Replace(':', '-');
	cStr Pn = DefPath;
	Pn.AppendPath(cTimer::GetLocalDate() + " " + T + cStr(OptionalSuffix) + ".jpg");
	cIO::SetImageDialogInitialFile(Pn);
	cStr S;
	if (cIO::SaveImageDialog(&S)) {
		cIO::SaveImage(S.ToCharPtr(), I);
		DefPath = S.GetFilePath();
	}
}


// cIO::MakeAndSaveScreenShot
void cIO::MakeAndSaveScreenShot() {
	cImage I;
	cRender::ScreenShot(&I);
	I.ToFormat(cFormat::Rgb8);
	SaveScreenShot(I);
}

// cIO::GetDefaultSourceFolder
const cStr & cIO::GetDefaultSourceFolder() {
	cIO_Init();
	return cIO_Sources[0]->GetPath();
}

// cIO::SetDefaultSourceWriteFolder
void cIO::SetDefaultSourceWriteFolder(const char *WriteFolder) {
	cIO_Init();
	((SourceFolder *)cIO_Sources[0])->SetWriteFolder(WriteFolder);
}

// cIO::GetThisFilePathName
const cStr & cIO::GetThisFilePathName() {
	cIO_Init();
	return cIO_ThisFilePathName;
}

#ifdef COMMS_LINUX
void cIO::GetLinuxFolder(cStr *Result, const char *FolderID) {
	Result->Clear();
	// Terminal >
	//	$ xdg-user-dir DOCUMENTS > /home/sergii/Documents
	//	$ xdg-user-dir DOWNLOAD > /home/sergii/Downloads
	cStr T(512);
	cStr P("xdg-user-dir ");
	P += FolderID;
	FILE *F = popen(P.ToCharPtr(), "r");
	if(F != nullptr) {
		if(fgets(T.ToNonConstCharPtr(), T.Length(), F) != nullptr) {
			T.CalcLength();
			T.TrimEnd("\n");
			Result->Copy(T);
		}
		pclose(F);
	}
}
#endif // Linux

#ifdef COMMS_WINDOWS
// cIO_GetWindowsFolder
static void cIO_GetWindowsFolder(cStr *Documents, cStr *Downloads, cStr *Desktop) {
	cStr* Res = nullptr;
	int ID = 0;
	KNOWNFOLDERID FolderID = GUID_NULL;
	if (Documents != nullptr) {
		Res = Documents;
		ID = CSIDL_PERSONAL;
		FolderID = FOLDERID_Documents;
	}
	else if (Downloads != nullptr) {
		Res = Downloads;
		ID = 0; // There is no "CSIDL_DOWNLOADS"
		FolderID = FOLDERID_Downloads;
	}
	else if (Desktop != nullptr) {
		Res = Desktop;
		ID = CSIDL_DESKTOP;
		FolderID = FOLDERID_Desktop;
	}
	if (Res != nullptr) {
		Res->Clear();
		WCHAR* Ptr = nullptr;
		// Trying function "SHGetKnownFolderPath" before fallback "SHGetFolderPath",
		// because japanese user has reported that "SHGetFolderPath" doesn't work for him.
		HRESULT hr = SHGetKnownFolderPath(FolderID, 0, nullptr, &Ptr);
		// Note: "SHGetKnownFolderPath" fails on some systems in which case we should not call "wcstombs_s" below.
		bool Ok = SUCCEEDED(hr) && (Ptr != nullptr);
		if (Ok) {
			Res->Copy(Ptr);
		}
		CoTaskMemFree(Ptr); Ptr = nullptr;
		if (!Ok || Res->IsEmpty()) { // "SHGetKnownFolderPath" or "wcstombs_s" failed
			// Microsoft docs remark that the old function "SHGetFolderPath" is a wrapper for new "SHGetKnownFolderPath":
			// https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
			// But "Pascal G" has a user folder with the name "greek delta". Function "SHGetKnownFolderPath"
			// (and "SHGetSpecialFolderPathW" for which Microsoft recommends to use "SHGetFolderPath" instead)
			// returned "C:\Users\'\Documents" (looks like a single quote) wide char string pointer.
			// Subsequent call to "wcstombs_s" returned an empty standard char string.
			// In the same time the function "SHGetFolderPath" returns on "Pascal G" computer
			// correct path "C:\Users\{greek delta}\Documents" and "3DCoat" is able to create user data folder inside it.
			if (ID != 0) { // Because there is no "CSIDL_DOWNLOADS" we cannot use alternative function "SHGetFolderPath" (or "SHGetSpecialFolderPathW").
				TCHAR Path[MAX_PATH] = { 0 };
				HRESULT hr = SHGetFolderPath(nullptr, ID, nullptr, 0, Path);
				if (SUCCEEDED(hr)) {
					Res->Copy(Path);
				}
			}
		} else {
			if (Documents) {
				Documents->BackSlashesToSlashes();
				int onedrive = Documents->IndexOf("/OneDrive/");
				if(onedrive >= 0) {
					Documents->Replace("/OneDrive/","/");
				}
			}
		}
	}
}
#endif // Windows

#ifdef COMMS_MACOS
void cIO::GetMacOSFolder(cStr *Documents, cStr *Downloads, cStr *Home) {
    cStr *Res = nullptr;
    OSType FolderType = 0;
    if(Documents != nullptr) {
        Res = Documents;
        FolderType = kDocumentsFolderType;
    } else if(Downloads != nullptr) {
        Res = Downloads;
        FolderType = kDownloadsFolderType;
    } else if(Home != nullptr) {
        Res = Home;
        FolderType = kCurrentUserFolderType;
    }
    if (Res != nullptr) {
        Res->Clear();
        cStr S(512);
        FSRef F;
        if(noErr == FSFindFolder(kUserDomain, FolderType, kCreateFolder, &F)) {
            FSRefMakePath(&F, (unsigned char *)S.ToCharPtr(), S.Length());
            S.CalcLength();
            Res->Copy(S);
        }
    }
}
#endif // macOS

const cStr& cIO::GetDocumentsFolder() {
	cIO_Init();
	static cStr Documents;
	if (Documents.IsEmpty()) {
#ifdef COMMS_WINDOWS
		cIO_GetWindowsFolder(&Documents, nullptr, nullptr);
		if (Documents.IsEmpty()) {
			cStr Env = cIO::GetEnvironmentVariable(COAT_USER_PATH_ENV);
			if (Env.IsEmpty()) {
				cMessageBox::Ok("ERROR: The path to your \"Documents\" folder is invalid", "To start \"3DCoat\" select where it should save your documents (for example, select your external drive). To make selected path permanent define system environment variable \"COAT_USER_PATH\" with that path as value:\nclick START > type Edit the system environment variables > Environment Variables... > System variables > New... >\nVariable name: COAT_USER_PATH\nVariable value: C:\\Users\\YOUR_USER_NAME\\Desktop\\My3DCoatFiles\n(make the path to your external drive, desktop or any other location)\n> OK > OK");
				if (!cIO::SelectFolderDialog("Select where \"3DCoat\" should save your documents for this session", &Documents)) {
					exit(0); // No sense to continue because "3DCoat" will not be able to save anything
				}
			}
		} else {
			//const int i = Documents.IndexOf("OneDrive"); // C:/Users/NAME/OneDrive/?????????/
			//if(i != -1) {
			//	Documents.Remove(i);
			//}
		}
		Documents.BackSlashesToSlashes();
#endif // Windows

#ifdef COMMS_MACOS
        GetMacOSFolder(&Documents, nullptr, nullptr);
#endif // macOS

#ifdef COMMS_LINUX
		GetLinuxFolder(&Documents, "DOCUMENTS");
#endif // Linux

        Documents.EnsureTrailingSlash();
	}
	return Documents;
}

const cStr& cIO::GetSystemDocumentsFolder() {
#ifdef COMMS_WINDOWS
	cIO_Init();
	static cStr Documents;
	if (Documents.IsEmpty()) {
		cIO_GetWindowsFolder(&Documents, nullptr, nullptr);
		Documents.BackSlashesToSlashes();
	}
	return Documents;
#else
	return GetDocumentsFolder();
#endif
}

// cIO::GetDownloadsFolder
const cStr& cIO::GetDownloadsFolder() {
	static cStr Downloads;
	if (Downloads.IsEmpty()) {
#ifdef COMMS_WINDOWS
		cIO_GetWindowsFolder(nullptr, &Downloads, nullptr);
		Downloads.BackSlashesToSlashes();
#endif // Windows

#ifdef COMMS_MACOS
        GetMacOSFolder(nullptr, &Downloads, nullptr);
#endif // macOS

#ifdef COMMS_LINUX
		GetLinuxFolder(&Downloads, "DOWNLOAD");
#endif // Linux

        Downloads.EnsureTrailingSlash();
	}
	return Downloads;
}

// cIO::GetDesktopFolder
const cStr& cIO::GetDesktopFolder() {
	static cStr Desktop;
	if (Desktop.IsEmpty()) {
#ifdef COMMS_WINDOWS
		cIO_GetWindowsFolder(nullptr, nullptr, &Desktop);
		Desktop.BackSlashesToSlashes();
#endif // Windows

#ifdef COMMS_MACOS
		cAssert(0);
#endif // macOS

#ifdef COMMS_LINUX
		cAssert(0);
#endif // Linux

		Desktop.EnsureTrailingSlash();
	}
	return Desktop;
}

// cIO::RestartThisApplication
void cIO::RestartThisApplication() {
#ifdef COMMS_WINDOWS
	ShellExecute(nullptr, "open", cIO_ThisFilePathName.ToCharPtr(), nullptr, nullptr, SW_NORMAL);
#endif // COMMS_WINDOWS
#ifdef COMMS_MACOS
    cMacMain_RestartThisApplication();
#endif // COMMS_MACOS
#ifdef COMMS_LINUX
    execl(cIO_ThisFilePathName.ToCharPtr(), "", nullptr); // Replaces the current process (which terminates itself) with a new process
#endif // COMMS_LINUX
}

//-----------------------------------------------------------------------------
// cIO::LoadXmlDialog
//-----------------------------------------------------------------------------
bool cIO::LoadXmlDialog(cStr *XmlFilePn) {
	cAssert(XmlFilePn != nullptr);
#if defined COMMS_WINDOWS || defined COMMS_MACOS || defined COMMS_LINUX
	cList<cStr> Extensions;
	Extensions.Add("Xml");
	cStr InitialFileName, InitialFolder;
	if(!cIO_XmlDialogInitialFile.IsEmpty()) {
		InitialFileName = cIO_XmlDialogInitialFile.GetFileName();
		InitialFolder = cIO_XmlDialogInitialFile.GetFilePath();
	} else {
		InitialFolder = cIO_XmlDialogInitialPath;
	}
	const cStr PrefKey = "Xml";
	SetFileDialogInitialFolder(PrefKey, InitialFolder);
	bool r = false;
#ifdef COMMS_WINDOWS
	cWin32::XmlFileDialog xfd(true, InitialFolder.ToCharPtr());
	if(xfd.DoModal(InitialFileName.ToCharPtr())) {
		*XmlFilePn = xfd.GetFilePathName();
		r = true;
	}
#else // !COMMS_WINDOWS = COMMS_MACOS || COMMS_LINUX
	r = LoadFileDialog("Load Xml File", Extensions, XmlFilePn, PrefKey, InitialFileName);
#endif // COMMS_WINDOWS
	if(!r) {
		return false;
	}
	cIO_XmlDialogInitialPath = XmlFilePn->GetFilePath();
	cIO_XmlDialogInitialFile = *XmlFilePn;
	XmlFilePn->RemoveFileAbsPath(GetDefaultSourceFolder());
	return true;
#endif // COMMS_WINDOWS || COMMS_MACOS || COMMS_LINUX
	return false;
} // cIO::LoadXmlDialog

//-----------------------------------------------------------------------------
// cIO::SaveXmlDialog
//-----------------------------------------------------------------------------
bool cIO::SaveXmlDialog(cStr *XmlFilePn) {
	cAssert(XmlFilePn != nullptr);
#if defined COMMS_WINDOWS || defined COMMS_MACOS || defined COMMS_LINUX
	cList<cStr> Extensions;
	Extensions.Add("Xml");
	cStr InitialFileName, InitialFileBase, InitialFolder;
	if(!cIO_XmlDialogInitialFile.IsEmpty()) {
		InitialFileName = cIO_XmlDialogInitialFile.GetFileName();
		InitialFileBase = cIO_XmlDialogInitialFile.GetFileBase();
		InitialFolder = cIO_XmlDialogInitialFile.GetFilePath();
	} else {
		InitialFolder = cIO_XmlDialogInitialPath;
	}
	const cStr PrefKey = "Xml";
	SetFileDialogInitialFolder(PrefKey, InitialFolder);
	bool r = false;
#ifdef COMMS_WINDOWS
	cWin32::XmlFileDialog xfd(false, InitialFolder.ToCharPtr());
	if(xfd.DoModal(InitialFileName.ToCharPtr())) {
		*XmlFilePn = xfd.GetFilePathName();
		r = true;
	}
#else // !COMMS_WINDOWS = COMMS_MACOS || COMMS_LINUX
	r = SaveFileDialog("Save Xml File", Extensions, XmlFilePn, PrefKey, Extensions[0], InitialFileBase);
#endif // COMMS_WINDOWS
	if(!r) {
		return false;
	}
	cIO_XmlDialogInitialPath = XmlFilePn->GetFilePath();
	cIO_XmlDialogInitialFile = *XmlFilePn;
	return true;
#endif // COMMS_WINDOWS || COMMS_MACOS || COMMS_LINUX
	return false;
} // cIO::SaveXmlDialog

// cIO::SearchFiles
void cIO::SearchFiles(const char *Folder, cList<cStr> *Files, const bool SeekInWritePath) {
	SearchItems(Folder, Files, false);
	// Alternative write path
	if(SeekInWritePath) {
		cStr W;
		W.Copy(Folder);
		if(ReplaceReadPath(&W)) {
			cList<cStr> T;
			SearchItems(W.ToCharPtr(), &T, false);
			int i;
			cStr S;
			for(i = 0; i < T.Count(); i++) {
				S.Copy(Folder);
				cStr &r = T[i];
				r.Remove(0, W.Length());
				S.AppendPath(r);
				if(!Files->Contains(S, cStr::EqualsPath)) {
					Files->Add(S);
				}
			}
		}
	}
}

void cIO::SearchFiles(const char* Folder, cList<cStr>* Files, const char* Extensions) {
	cAssert(Files != nullptr);
	Files->Clear();
	cAssert(Extensions != nullptr);
	cList<cStr> E;
	cStr::Split(Extensions, &E);
	if (E.IsEmpty()) {
		return;
	}
	cList<cStr> T;
	SearchFiles(Folder, &T);
	int i;
	for (i = 0; i < T.Count(); i++) {
		const cStr& S = T[i];
		if (E.Contains(S.GetFileExtension(), cStr::EqualsNoCase)) {
			Files->Add(S);
		}
	}
}

// cIO::SearchFolders
void cIO::SearchFolders(const char *Folder, cList<cStr> *Folders) {
	SearchItems(Folder, Folders, true);
	// Alternative write path
	cStr W;
	W.Copy(Folder);
	if(ReplaceReadPath(&W)) {
		cList<cStr> T;
		SearchItems(W.ToCharPtr(), &T, true);
		int i;
		cStr S;
		for(i = 0; i < T.Count(); i++) {
			S.Copy(Folder);
			cStr &r = T[i];
			r.Remove(0, W.Length());
			S.AppendPath(r);
			if(!Folders->Contains(S, cStr::EqualsPath)) {
				Folders->Add(S);
			}
		}
	}
}

// cIO_SearchFilesRecursive
void cIO_SearchFilesRecursive(const char *Folder, cList<cStr> *Files) {
	cIO::SearchFiles(Folder, Files);
	
	cList<cStr> Folders;
	cIO::SearchFolders(Folder, &Folders);
	int i;
	cList<cStr> T;
	for(i = 0; i < Folders.Count(); i++) {
		const cStr &F = Folders[i];
		cIO_SearchFilesRecursive(F, &T);
		Files->AddRange(T);
	}
}

// cIO::SearchFilesRecursive
void cIO::SearchFilesRecursive(const char *Folder, cList<cStr> *Files) {
	cAssert(Files != nullptr);
	Files->Clear();
	cIO_SearchFilesRecursive(Folder, Files);
}

// cIO::SearchFilesRecursive
void cIO::SearchFilesRecursive(const char *Folder, cList<cStr> *Files, const char *Extensions) {
	cAssert(Files != nullptr);
	Files->Clear();
	cAssert(Extensions != nullptr);
	cList<cStr> E;
	cStr::Split(Extensions, &E);
	if(E.IsEmpty()) {
		return;
	}
	cList<cStr> T;
	SearchFilesRecursive(Folder, &T);
	int i;
	for(i = 0; i < T.Count(); i++) {
		const cStr &S = T[i];
		if(E.Contains(S.GetFileExtension(), cStr::EqualsNoCase)) {
			Files->Add(S);
		}
	}
}
	
// cIO::CheckProcessInMemory
bool cIO::CheckProcessInMemory(const char *ProcName) {
#ifdef COMMS_WINDOWS
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(INVALID_HANDLE_VALUE == hProcessSnap) {
		return false;
	}
	PROCESSENTRY32 PE;
	PE.dwSize = sizeof(PE);
	if(!Process32First(hProcessSnap, &PE)) {
		CloseHandle(hProcessSnap);
		hProcessSnap = INVALID_HANDLE_VALUE;
		return false;
	}
	// There is no sense in enumerating modules, because almost all of them are dlls
	// Besides modules enumeration takes a lot of time - more than 300 ms for almost 3K module entries
	// In the same time, enumeration of processes takes only 4 ms for 70 processes
	bool r = false;
	cStr S;
	do {
		S.Copy(PE.szExeFile);
		if(S.StartsWith(ProcName, true)) {
			r = true;
			break;
		}
	} while(Process32Next(hProcessSnap, &PE));
	CloseHandle(hProcessSnap);
	hProcessSnap = INVALID_HANDLE_VALUE;
	return r;
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
    return cMacMain_CheckProcessInMemory(ProcName);
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
    return cLinuxMain_CheckProcessInMemory(ProcName);
#endif // COMMS_LINUX

	return false;
}

// cIO::Exec
bool cIO::Exec(const char *FilePn, const char *Args, bool Wait, bool Hide) {
	cStr Pn = FilePn;
#ifdef COMMS_IOS
    return cIosMain_OpenURL(FilePn);
#endif // COMMS_IOS
#ifdef COMMS_WINDOWS
	if(Wait) {
		SHELLEXECUTEINFO sei;
		ZeroMemory(&sei, sizeof(sei));
		sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		sei.lpVerb = "open";
		sei.lpFile = FilePn;
		sei.lpParameters = Args;
		if(!ShellExecuteEx(&sei)) {
			return false;
		}
		WaitForSingleObject(sei.hProcess, INFINITE);
		DWORD ExitCode;
		if(!GetExitCodeProcess(sei.hProcess, &ExitCode)) {
			return false;
		}
		return (0 == ExitCode);
	} else {
#pragma warning(disable: 4302)
#pragma warning(disable: 4311)
		std::wstring com = comms::wchar_path(Pn).path;
		std::wstring args = comms::wchar_path(Args).path;
		int r = (int)ShellExecuteW(nullptr, L"open", com.c_str(), args.c_str(), L"", Hide ? SW_SHOWMINIMIZED : SW_NORMAL);
#pragma warning(default: 4302)
#pragma warning(default: 4311)
		return r > 32;
	}
#endif // COMMS_WINDOWS
#ifdef COMMS_MACOS
	if(cStr::EqualsNoCase(Pn.GetFileExtension(), "app")) {
		LSApplicationParameters P;
		memset(&P, 0, sizeof(P));
		
		FSRef Ref, Doc;
		OSStatus E;
		
		E = FSPathMakeRef((const UInt8 *)Pn.ToCharPtr(), &Ref, nullptr);
		if(E != noErr) {
			return false;
		}
		P.application = &Ref;
		
        cStr A(Args);
        if(A.IsEmpty() || FSPathMakeRef((const UInt8 *)EnsureAbsolutePath(A).ToCharPtr(), &Doc, nullptr) != noErr) {
			E = LSOpenApplication(&P, nullptr);
			return noErr == E;
		}
		
		E = LSOpenItemsWithRole(&Doc, 1, 0, nullptr, &P, nullptr, 0);
		return noErr == E;
	} else {
		cStr S = cStr::Format("open \"%s\"", Pn.ToCharPtr());
		if(Args != nullptr) {
			S += cStr::Format(" \"%s\"", Args);
		}
		int r = system(S.ToCharPtr());
		return 0 == r;
	}
#endif // COMMS_MACOS
#ifdef COMMS_LINUX
    bool u = cIO_PathIsURL(Pn);
    cList<cStr> Docs;
    Docs.Add("pdf");
    cStr E = Pn.GetFileExtension();
    bool App = E.IsEmpty() || !Docs.Contains(E, cStr::EqualsNoCase);
    if(!u && App) {
        // Try as executable
        cStr S = Pn;
        if(Args != nullptr) {
            S += " " + cStr(Args); // Do not modify the argument because it could be a switch or a key, for example "-f"
        }
        GError *Error = nullptr;
        gboolean r = g_spawn_command_line_async(S.ToCharPtr(), &Error);
        if(r) {
            return true;
        }
    }
    // Try as document
    cStr S = "xdg-open " + Pn;
    if(Args != nullptr) {
        S += " " + cStr(Args);
    }
    gboolean r = g_spawn_command_line_async(S.ToCharPtr(), nullptr);
    return r;
#endif // COMMS_LINUX
	return false;
}

// cIO::Explore
bool cIO::Explore(const char *Folder) {
	cStr F = EnsureAbsolutePath(Folder);
#ifdef COMMS_WINDOWS
#pragma warning(disable: 4302)
#pragma warning(disable: 4311)
	cStr f = Folder;
	std::wstring ws;
	f.toWstring(ws);
	int r = (int)ShellExecuteW(nullptr, L"explore", ws.c_str(), nullptr, L"", SW_NORMAL);
#pragma warning(default: 4311)
#pragma warning(default: 4302)
	return r > 32;
#endif // COMMS_WINDOWS
#ifdef COMMS_MACOS
	return Exec(F.ToCharPtr(), nullptr);
#endif // COMMS_MACOS
#ifdef COMMS_LINUX
	cStr S = "xdg-open " + EnsureAbsolutePath(Folder);
	gboolean r = g_spawn_command_line_async(S.ToCharPtr(), nullptr);
	return r;
#endif // COMMS_LINUX
	return false;
}

#ifdef COMMS_MACOS
// CompareTimes_Less
static bool CompareTimes_Less(const UTCDateTime &l, const UTCDateTime &r) {
	if(l.highSeconds == r.highSeconds) {
		if(l.lowSeconds == r.lowSeconds) {
			return l.fraction < r.fraction;
		} else {
			return l.lowSeconds < r.lowSeconds;
		}
	} else {
		return l.highSeconds < r.highSeconds;
	}
}
#endif // COMMS_MACOS

//-----------------------------------------------------------------------------
// cIO::CompareFileChangeTimes
//-----------------------------------------------------------------------------
bool cIO::CompareFileChangeTimes(const char *FilePn0, const char *FilePn1) {
	cStr Pn0 = EnsureAbsolutePath(FilePn0);
	cStr Pn1 = EnsureAbsolutePath(FilePn1);

#ifdef COMMS_WINDOWS
	HANDLE H0, H1;
	FILETIME T0, T1;
	
	H0 = CreateFileW(comms::wchar_path(Pn0), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if(H0 != INVALID_HANDLE_VALUE) {
		H1 = CreateFileW(comms::wchar_path(Pn1), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if(H1 != INVALID_HANDLE_VALUE) {
			if(GetFileTime(H0, nullptr, nullptr, &T0)) {
				if(GetFileTime(H1, nullptr, nullptr, &T1)) {
					if(*((__int64 *)&T0) >= *((__int64*)&T1)) {
						CloseHandle(H0);
						CloseHandle(H1);
						return false;
					}
				}
			}
			CloseHandle(H1);
		}
		CloseHandle(H0);
	}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	FSRef Ref0, Ref1;
	Boolean Dir0, Dir1;
	FSCatalogInfo Info0, Info1;
	memset(&Info0, 0, sizeof(Info0));
	memset(&Info1, 0, sizeof(Info1));
	if(noErr == FSPathMakeRef((const UInt8 *)Pn0.ToCharPtr(), &Ref0, &Dir0)) {
		if(noErr == FSPathMakeRef((const UInt8 *)Pn1.ToCharPtr(), &Ref1, &Dir1)) {
			if(noErr == FSGetCatalogInfo(&Ref0, kFSCatInfoContentMod, &Info0, nullptr, nullptr, nullptr)) {
				if(noErr == FSGetCatalogInfo(&Ref1, kFSCatInfoContentMod, &Info1, nullptr, nullptr, nullptr)) {
					if(!CompareTimes_Less(Info0.contentModDate, Info1.contentModDate)) {
						return false;
					}
				}
			}
		}
	}
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
        struct stat L, R;
        if(0 == g_stat(Pn0.ToCharPtr(), &L)) {
            if(0 == g_stat(Pn1.ToCharPtr(), &R)) {
                if(L.st_mtim.tv_sec >= R.st_mtim.tv_sec) {
                    return false;
                }
            }
        }
#endif // COMMS_LINUX

	return true;
} // cIO::CompareFileChangeTimes
bool cIO::NeedUpdateFile(const char* FileToUpdate, const char* SourceFile) {
	cStr s = EnsureAbsolutePath(SourceFile);
	ReplaceReadPathForExistingFile(&s);
	cStr d = EnsureAbsolutePath(FileToUpdate);
	ReplaceReadPathForExistingFile(&d);
	return CompareFileChangeTimes(d, s);
}

void cIO::CopyToClipboard(const char *Src) {
#ifdef COMMS_WINDOWS
	if(!OpenClipboard(cWinMain_GetWindow())) {
		return;
	}
	EmptyClipboard();
	int L = cStr::Length(Src);
	
	HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, L + 1);
	if(nullptr == hBuffer) {
		CloseClipboard();
		return;
	}

	char *pBuffer = (char *)GlobalLock(hBuffer);
	if(L > 0) {
		memcpy(pBuffer, Src, L);
	}
	pBuffer[L] = 0;
	GlobalUnlock(hBuffer);
	
	SetClipboardData(CF_TEXT, hBuffer);
	CloseClipboard();
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	OSStatus E = noErr;
	PasteboardRef R;
	
	E = PasteboardCreate(kPasteboardClipboard, &R);
	if(E != noErr) {
		return;
	}
	E = PasteboardClear(R);
	if(noErr == E) {
		cStr S = Src;
		CFDataRef Data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)S.ToCharPtr(), S.Length());
		if(Data != nullptr) {
			E = PasteboardPutItemFlavor(R, (PasteboardItemID)1, CFSTR("public.utf8-plain-text"), Data, 0);
			if(noErr == E) {
			}
			CFRelease(Data);
		}
	}
	CFRelease(R);
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
	GtkClipboard *C = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	if(C != nullptr) {
		gtk_clipboard_set_text(C, Src, -1);
	}
#endif // COMMS_LINUX
}

bool cIO::CopyFromClipboard(cStr *To) {
	cAssert(To != nullptr);
	To->Clear();

#ifdef COMMS_WINDOWS
	if(!IsClipboardFormatAvailable(CF_TEXT)) {
		return false;
	}
	if(!OpenClipboard(cWinMain_GetWindow())) {
		return false;
	}
	HGLOBAL hBuffer = GetClipboardData(CF_TEXT);
	if(hBuffer != nullptr) {
		char *pBuffer = (char *)GlobalLock(hBuffer);
		if(pBuffer != nullptr) {
			To->Copy(pBuffer);
		}
		GlobalUnlock(hBuffer);
	}
	CloseClipboard();
	
	return true;
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	OSStatus E = noErr;
	PasteboardRef R;
	ItemCount c;
	int ItemIndex;
	PasteboardItemID ItemID;
	CFArrayRef FlavorTypeArray;
	CFIndex FlavorCount, FlavorIndex, FlavorDataSize;
	char FlavorTypeStr[128];
	CFStringRef FlavorType;
	CFDataRef FlavorData;
	const byte *Ptr = nullptr;
	bool r = false;

	E = PasteboardCreate(kPasteboardClipboard, &R);
	if(E != noErr) {
		return false;
	}
	E = PasteboardGetItemCount(R, &c);
	if(noErr == E) {
		for(ItemIndex = 1; ItemIndex <= c; ItemIndex++) {
			E = PasteboardGetItemIdentifier(R, ItemIndex, &ItemID); // One - based index
			if(noErr == E) {
				E = PasteboardCopyItemFlavors(R, ItemID, &FlavorTypeArray);
				if(noErr == E) {
					FlavorCount = CFArrayGetCount(FlavorTypeArray);
					for(FlavorIndex = 0; FlavorIndex < FlavorCount; FlavorIndex++) {
						FlavorType = (CFStringRef)CFArrayGetValueAtIndex(FlavorTypeArray, FlavorIndex);
						CFStringGetCString(FlavorType, FlavorTypeStr, 128, kCFStringEncodingUTF8);
						if(cStr::Equals(FlavorTypeStr, "public.utf8-plain-text")) {
							E = PasteboardCopyItemFlavorData(R, ItemID, FlavorType, &FlavorData);
							if(noErr == E) {
								FlavorDataSize = CFDataGetLength(FlavorData);
								Ptr = CFDataGetBytePtr(FlavorData);
								if(Ptr != nullptr) {
									To->SetLength((int)FlavorDataSize);
									memcpy(To->ToNonConstCharPtr(), Ptr, FlavorDataSize);
									To->CalcLength();
									r = true;
								}
							}
							CFRelease(FlavorData);
						}
						if(r) {
							break;
						}
					}
					CFRelease(FlavorTypeArray);
				}
			}
			if(r) {
				break;
			}
		}
	}
	CFRelease(R);
	return r;
#endif // COMMS_MACOS
        
#ifdef COMMS_LINUX
	GtkClipboard *C = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	if(C != nullptr) {
		char *t = gtk_clipboard_wait_for_text(C);
		// Restore OpenGL context after function "gtk_clipboard_wait_for_text"
		if(t != nullptr) {
			To->Copy(t);
			g_free(t); t = nullptr;
			return true;
		}
	}
#endif // COMMS_LINUX

	return false;
}

void cIO::CopyToClipboard(const cImage &Src) {
    cImage T;
    T.Copy(Src);
    T.Uncompress();
    T.RemoveMipMaps();

#if defined COMMS_WINDOWS || defined COMMS_MACOS
    cFile File;
    cCodecBmp Codec;
    if(!Codec.Encode(T, &File)) {
        return;
    }
#endif // Windows, macOS
	
#ifdef COMMS_WINDOWS
	size_t Size = File.Size() - sizeof(BITMAPFILEHEADER);
	const byte *Ptr = (const byte *)File.ToPtr() + sizeof(BITMAPFILEHEADER);
	
	if(!OpenClipboard(cWinMain_GetWindow())) {
		return;
	}
	EmptyClipboard();
	
	HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, Size);
	if(nullptr == hBuffer) {
		CloseClipboard();
		return;
	}
	
	char *pBuffer = (char *)GlobalLock(hBuffer);
	memcpy(pBuffer, Ptr, Size);
	GlobalUnlock(hBuffer);
	
	SetClipboardData(CF_DIB, hBuffer);
	CloseClipboard();
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	OSStatus E = noErr;
	PasteboardRef R;
	
	E = PasteboardCreate(kPasteboardClipboard, &R);
	if(E != noErr) {
		return;
	}
	E = PasteboardClear(R);
	if(noErr == E) {
		CFDataRef Data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)File.ToPtr(), File.Size());
		if(Data != nullptr) {
			E = PasteboardPutItemFlavor(R, (PasteboardItemID)1, CFSTR("com.microsoft.bmp"), Data, 0);
			if(noErr == E) {
			}
			CFRelease(Data);
		}
	}
	CFRelease(R);
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
	// Krita pastes corrupted image when its format is R8
	if(cFormat::R8 == T.GetFormat()) {
		T.ToFormat(cFormat::Rgba8);
	}
	const bool HasAlpha = (cFormat::Rgba8 == T.GetFormat());
	GdkPixbuf *P = gdk_pixbuf_new(GDK_COLORSPACE_RGB, HasAlpha, 8, T.GetWidth(), T.GetHeight());
	const int R = gdk_pixbuf_get_rowstride(P);
	byte *Dst = gdk_pixbuf_get_pixels(P);
	const int l = cFormat::BytesPerPixel(T.GetFormat()) * T.GetWidth();
	const byte *From = T.GetPixels();
	From += (T.GetHeight() - 1) * l; // Flip
	int i;
	for(i = 0; i < T.GetHeight(); i++) {
		memcpy(Dst, From, cMath::Min(R, l));
		Dst += R;
		From -= l;
	}
	GtkClipboard *C = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	if(C != nullptr) {
		gtk_clipboard_set_image(C, P);
	}
	gdk_pixbuf_unref(P); P = nullptr;
#endif // COMMS_LINUX
}

bool cIO::CopyFromClipboard(cImage *To) {
	To->Free();

#ifdef COMMS_WINDOWS
	if(!IsClipboardFormatAvailable(CF_DIB)) {
		return false;
	}
	if(!OpenClipboard(cWinMain_GetWindow())) {
		return false;
	}
	bool r = false;
	cFile File;
	cCodecBmp Codec;
	
	BITMAPFILEHEADER Hdr;
	memset(&Hdr, 0, sizeof(Hdr));
	Hdr.bfType = 'B' | 'M' << 8;

	HGLOBAL hBuffer = GetClipboardData(CF_DIB);
	if(hBuffer != nullptr) {
		byte *pBuffer = (byte *)GlobalLock(hBuffer);
		if(pBuffer != nullptr) {
			size_t Size = GlobalSize(hBuffer);
			File.WriteBytes(pBuffer, (int)Size);
			File.SetPos(0);
			File.WriteBytes(&Hdr, sizeof(Hdr));
			File.SetPos(0);
			r = Codec.Decode(File, To);
		}
		GlobalUnlock(hBuffer);
	}
	CloseClipboard();
	return r;
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	OSStatus E = noErr;
	PasteboardRef R;
	ItemCount c;
	int ItemIndex;
	PasteboardItemID ItemID;
	CFArrayRef FlavorTypeArray;
	CFIndex FlavorCount, FlavorIndex, FlavorDataSize;
	char FlavorTypeStr[128];
	CFStringRef FlavorType;
	CFDataRef FlavorData;
	const byte *Ptr = nullptr;
	cFile File;
	cCodecBmp Codec;
	bool r = false;
	
	E = PasteboardCreate(kPasteboardClipboard, &R);
	if(E != noErr) {
		return false;
	}
	E = PasteboardGetItemCount(R, &c);
	if(noErr == E) {
		for(ItemIndex = 1; ItemIndex <= c; ItemIndex++) {
			E = PasteboardGetItemIdentifier(R, ItemIndex, &ItemID); // One - based index
			if(noErr == E) {
				E = PasteboardCopyItemFlavors(R, ItemID, &FlavorTypeArray);
				if(noErr == E) {
					FlavorCount = CFArrayGetCount(FlavorTypeArray);
					for(FlavorIndex = 0; FlavorIndex < FlavorCount; FlavorIndex++) {
						FlavorType = (CFStringRef)CFArrayGetValueAtIndex(FlavorTypeArray, FlavorIndex);
						CFStringGetCString(FlavorType, FlavorTypeStr, 128, kCFStringEncodingUTF8);
						if(cStr::Equals(FlavorTypeStr, "com.microsoft.bmp")) {
							E = PasteboardCopyItemFlavorData(R, ItemID, FlavorType, &FlavorData);
							if(noErr == E) {
								FlavorDataSize = CFDataGetLength(FlavorData);
								Ptr = CFDataGetBytePtr(FlavorData);
								if(Ptr != nullptr) {
									File.WriteBytes(Ptr, (int)FlavorDataSize);
									File.SetPos(0);
									r = Codec.Decode(File, To);
								}
							}
							CFRelease(FlavorData);
						}
						if(r) {
							break;
						}
					}
					CFRelease(FlavorTypeArray);
				}
			}
			if(r) {
				break;
			}
		}
	}
	CFRelease(R);
	return r;
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
		bool r = false;
        GtkClipboard *C = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        if(C != nullptr) {
			// This function is a little faster than calling gtk_clipboard_wait_for_image()
			if(gtk_clipboard_wait_is_image_available(C)) {
				GdkPixbuf *P = gtk_clipboard_wait_for_image(C);
				if(P != nullptr) {
					GdkColorspace S = gdk_pixbuf_get_colorspace(P);
					int b = gdk_pixbuf_get_bits_per_sample(P);
					int n = gdk_pixbuf_get_n_channels(P);
					int W = gdk_pixbuf_get_width(P);
					int H = gdk_pixbuf_get_height(P);
					int R = gdk_pixbuf_get_rowstride(P);
					byte *Src = gdk_pixbuf_get_pixels(P);
					if(GDK_COLORSPACE_RGB == S && 8 == b) {
						cFormat::Enum F = (4 == n ? cFormat::Rgba8 : (3 == n ? cFormat::Rgb8 : cFormat::R8));
						int l = cFormat::BytesPerPixel(F) * W;
						To->Create(F, W, H, 1, 1);
						byte *Dst = To->GetPixels();
						Dst += (H - 1) * l; // Flip
						int i;
						for(i = 0; i < H; i++) {
							memcpy(Dst, Src, l);
							Dst -= l;
							Src += R;
						}
						r = true;
					}
					gdk_pixbuf_unref(P); P = nullptr;
				}
			}
			// Restore OpenGL context after functions "gtk_clipboard_wait_is_image_available" and "gtk_clipboard_wait_for_image"
		}
		return r;
#endif // COMMS_LINUX
	
	return false;
}

//-----------------------------------------------------------------------------
// cIO::RemoveFolder
//-----------------------------------------------------------------------------
bool cIO::RemoveFolder(const char *Folder, const bool Recursive) {
	bool r = false;
	int j;
	for(j = 0; j < 2; j++) {
		cStr F = EnsureAbsolutePath(Folder);
		if(1 == j && !ReplaceReadPath(&F)) {
			break;
		}
		cList<cStr> Files, Folders;
		int i;
		
		SetFolderPermissions(F.ToCharPtr());
		
		if(Recursive) {
			SearchFiles(F.ToCharPtr(), &Files);
			for(i = 0; i < Files.Count(); i++) {
				const cStr &t = Files[i];
				RemoveFile(t.ToCharPtr());
			}
			
			SearchFolders(F.ToCharPtr(), &Folders);
			for(i = 0; i < Folders.Count(); i++) {
				const cStr &t = Folders[i];
				RemoveFolder(t.ToCharPtr(), true);
			}
		}

#if defined COMMS_WINDOWS
		std::wstring ws;
		F.toWstring(ws);
		if(RemoveDirectoryW(ws.c_str()) != 0) {
			r = true;
		}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
		FSRef Ref;
		Boolean Dir = false;
		if(noErr == FSPathMakeRef((const UInt8 *)F.ToCharPtr(), &Ref, &Dir)) {
			if(Dir) {
				if(noErr == FSDeleteObject(&Ref)) {
					r = true;
				}
			}
		}
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
		if(0 == g_rmdir(F.ToCharPtr())) {
			r = true;
		}
#endif // COMMS_LINUX
	}
	return r;
} // cIO::RemoveFolder

//-----------------------------------------------------------------------------
// cIO::RemoveFile
//-----------------------------------------------------------------------------
bool cIO::RemoveFile(const char *FilePn) {
	bool r = false;
	int i;
	for(i = 0; i < 2; i++) {
		cStr Pn = EnsureAbsolutePath(FilePn);
		if(1 == i && !ReplaceReadPath(&Pn)) {
			break;
		}
		SetFilePermissions(Pn);
		
#if defined COMMS_WINDOWS
		if(DeleteFileW(comms::wchar_path(Pn)) != 0) {
			r = true;
		}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
		FSRef Ref;
		Boolean Dir = false;
		if(noErr == FSPathMakeRef((const UInt8 *)Pn.ToCharPtr(), &Ref, &Dir)) {
			if(!Dir) {
				if(noErr == FSDeleteObject(&Ref)) {
					r = true;
				}
			}
		}
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
		if(0 == g_remove(Pn.ToCharPtr())) {
			r = true;
		}
#endif // COMMS_LINUX
	}
	return r;
} // cIO::RemoveFile
//-----------------------------------------------------------------------------
// cIO::CheckIfPathWritable
//-----------------------------------------------------------------------------
bool cIO::CheckIfPathWritable(const char *path) {
	int r = cTimer::AcquireClockTicks();
	cStr pt = path;
	pt.EnsureTrailingBackslash();
	pt.MakePlatformSlashes();
	pt += "testwritefile.dat";
	CreatePath(pt);
	FILE *f = nullptr;
#if defined COMMS_WINDOWS
	_wfopen_s(&f, wchar_path(pt.ToCharPtr()), L"w");
#else // Linux, macOS
	f = fopen(pt.ToCharPtr(), "w");
#endif // COMMS_WINDOWS
	if(f) {
		fprintf(f, "%d", r);
		fclose(f);
		f = nullptr;
#if defined COMMS_WINDOWS
		_wfopen_s(&f, wchar_path(pt.ToCharPtr()), L"r");
#else // Linux, macOS
		f = fopen(pt.ToCharPtr(), "r");
#endif // COMMS_WINDOWS
		if(f) {
			int r1 = r + 1;
#if defined COMMS_WINDOWS
			fscanf_s(f, "%d", &r1);
#else // Linux, macOS
			fscanf(f, "%d", &r1);
#endif // COMMS_WINDOWS
			fclose(f);
			f = nullptr;
			RemoveFile(pt.ToCharPtr());
			return r == r1;
		}
	}
	return false;
}// cIO::CheckIfPathWritable
//-----------------------------------------------------------------------------
// cIO::GetDiskFreeSpace
//-----------------------------------------------------------------------------
float cIO::GetDiskFreeSpace() {
	cStr S = GetDefaultSourceFolder();
	float r = 0.0f;

#if defined COMMS_WINDOWS
	__int64 FreeBytesToCaller, TotalBytes, FreeBytes;
	if(GetDiskFreeSpaceEx(S.ToCharPtr(), (PULARGE_INTEGER)&FreeBytesToCaller, (PULARGE_INTEGER)&TotalBytes, (PULARGE_INTEGER)&FreeBytes)) {
		r = (float)(FreeBytes / 1048576);
	}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	FSRef Ref;
	Boolean Dir = false;
	FSCatalogInfo Info;
	FSVolumeInfo Vol;
	memset(&Info, 0, sizeof(Info));
	memset(&Vol, 0, sizeof(Vol));
	
	if(noErr == FSPathMakeRef((const UInt8 *)S.ToCharPtr(), &Ref, &Dir)) {
		if(Dir) {
			if(noErr == FSGetCatalogInfo(&Ref, kFSCatInfoVolume, &Info, nullptr, nullptr, nullptr)) {
				if(noErr == FSGetVolumeInfo(Info.volume, 0, nullptr, kFSVolInfoSizes, &Vol, nullptr, nullptr)) {
					r = (float)(Vol.freeBytes / 1048576);
				}
			}
		}
	}
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
        struct statfs64 B;
        int i = statfs64(S.ToCharPtr(), &B);
        if(0 == i) {
            r = (float)(B.f_bavail * B.f_bsize / 1048576);
        }
#endif // COMMS_LINUX
	
	return r;
} // cIO::GetDiskFreeSpace

bool cIO::FileExists(const char* filename) {
	cStr s = comms::cIO::EnsureAbsolutePath(filename);
	cStr s1 = s;
	ReplaceReadPath(&s1);
    FILE *F = nullptr;
#ifdef COMMS_WINDOWS
    _wfopen_s(&F, wchar_path(s1), L"rb");
#else // COMMS_LINUX, COMMS_MACOS, ...
    F = fopen(s1, "rb");
#endif // COMMS_WINDOWS
	if(F) {
		fclose(F);
		return true;
	}
#ifdef COMMS_WINDOWS
	_wfopen_s(&F, wchar_path(s), L"rb");
#else // COMMS_LINUX, COMMS_MACOS, ...
    F = fopen(s, "rb");
#endif // COMMS_WINDOWS
	if (F) {
		fclose(F);
		return true;
	}
	return false;
}

#if defined COMMS_MACOS || defined COMMS_LINUX

static bool _CopyFileU8(const char* l, const char* r, bool overwrite) {
	if (!overwrite) {
		if (cIO::FileExists(r))return true;
	}
	FILE* srcFile, * destFile;
	const int bufsize = 1024 * 512;
	static char* buffer = new char[bufsize];
	size_t bytesRead;

	srcFile = fopen(l, "rb");
	if (!srcFile) {
		return false;
	}

	destFile = nullptr;

	do {
		bytesRead = fread(buffer, 1, bufsize, srcFile);
		if (bytesRead > 0) {
			if (!destFile) {
				destFile = fopen(r, "wb");
				if (!destFile) {
					fclose(srcFile);
					return false;
				}
			}
			if (destFile)fwrite(buffer, 1, bytesRead, destFile);
		}
	} while (bytesRead > 0);

	fclose(srcFile);
	if (destFile)fclose(destFile);
	return true;
}


#endif // COMMS_MACOS || COMMS_LINUX

#ifdef COMMS_WINDOWS

static bool _CopyFileU8(const char* l, const char* r, bool overwrite) {
	if (!overwrite) {
		if (cIO::FileExists(r))return true;
	}
	std::wstring wl = comms::wchar_path(l).path;
	std::wstring wr = comms::wchar_path(r).path;
	FILE *srcFile = nullptr, *destFile = nullptr;
	const int bufsize = 1024 * 512;
	static char* buffer = new char[bufsize];
	size_t bytesRead;

	_wfopen_s(&srcFile, wl.c_str(), L"rb");
	if (!srcFile) {
		return false;
	}

	destFile = nullptr;

	do {
		bytesRead = fread(buffer, 1, bufsize, srcFile);
		if (bytesRead > 0) {
			if (!destFile) {
				_wfopen_s(&destFile, wr.c_str(), L"wb");
				if (!destFile) {
					fclose(srcFile);
					return false;
				}
			}
			if (destFile)fwrite(buffer, 1, bytesRead, destFile);
		}
	} while (bytesRead > 0);

	fclose(srcFile);
	if (destFile)fclose(destFile);
	return true;
}

#endif

#ifdef COMMS_3DCOAT
bool cIO::FileCopy(const char* l, const char* r, bool overwrite) {
	cStr rr = r;
	ReplaceReadPath(&rr);
	CreatePath(rr);
	cStr ll = l;
	ReplaceReadPath(&ll);
	cStr l0 = comms::cIO::EnsureAbsolutePath(l);
	if (CheckIfFileExists(ll.ToCharPtr())) {
		if (!cStr::Equals(ll, rr))return _CopyFileU8(ll.ToCharPtr(), rr.ToCharPtr(), overwrite);
	}
	else {
		if (!cStr::Equals(l0, rr))return _CopyFileU8(l0, rr.ToCharPtr(), overwrite);
	}
	return false;
}

bool cIO::CopyFolder(const char* src, const char* dst, bool overwrite, std::function<void(int, int)> progress) {
	cStr s = src;
	cStr d = dst;
	s.EnsureTrailingBackslash();
	d.EnsureTrailingBackslash();
	cList<cStr> FL;
	SearchFilesRecursive(s, &FL);
	int L0 = s.Length();
	bool err = false;
	cStr bad_file;
	for (int i = 0; i < FL.Count(); i++) {
		if (progress)progress(i, FL.Count());
		cStr s0 = FL[i].ToCharPtr();
		s0.Remove(0, L0);
		cStr s1 = d;
		s1 += s0;
		if (overwrite || !CheckIfFileExists(s1)) {
			err |= !FileCopy(FL[i], s1);
		}
		if(err) {
			bad_file = FL[i];
		}
	}
	return !err;
}
#endif // COMMS_3DCOAT

//-----------------------------------------------------------------------------
// cIO::GetDiskSerialNumber
//-----------------------------------------------------------------------------
const cStr cIO::GetDiskSerialNumber() {
	static cStr ID;
	if(!ID.IsEmpty()) {
		return ID;
	}
#ifdef COMMS_WINDOWS
	TCHAR Path[MAX_PATH];
	DWORD SN;
	cStr S;
	int i, b;
	if(SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_WINDOWS, nullptr, 0, Path))) {
		Path[3] = '\0'; // "C:\"
		if(GetVolumeInformation(Path, nullptr, 0, &SN, nullptr, nullptr, nullptr, 0)) {
			for(i = 0; i < 4; i++) {
				b = (SN >> 8 * (3 - i)) & 0xff;
				S << cStr::Format("%x%x", b >> 4, b & 0xf);
			}
			return S;
		}
	}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
    //************************************************************************
	// Get "/bin/ls" change time (i.e. the time when the system was installed)
	//************************************************************************
    // $ date -r tv_sec - Convert tv_sec to full date and time in OS X Terminal
    struct stat L;
	int r = stat("/bin/ls", &L);
    time_t tt = L.st_ctimespec.tv_sec; // __darwin_time_t == time_t
	cStr Fallback;
	if(0 == r) {
		Fallback.Copy(ctime(&tt));
	}
    dword CRC = cMath::Checksum(Fallback.ToCharPtr(), Fallback.Length());
    //*******************************************************************************************
    // Get UUID of disk on mount point "/" using Terminal command "$ diskutil info / | grep UUID"
    //*******************************************************************************************
    // $ diskutil info / | grep UUID
    cStr UUID;
    cMacMain_GetRootDiskUUID(&UUID);
    static const cStr Example("99B6191A-BCAC-3887-A240-E159BB27EF9F");
    if(UUID.Length() == Example.Length()) {
        CRC = cMath::Checksum(UUID.ToCharPtr(), UUID.Length());
    }
    ID = cStr::Format("%x", CRC);
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
	//************************************************************************
	// Get "/bin/ls" change time (i.e. the time when the system was installed)
	//************************************************************************
	// $ stat /bin/ls - Linux Terminal stat command
	// tv_sec - seconds since January 1, 1970
	// $ date -d @tv_sec - Convert tv_sec to full date and time in Linux Terminal
	struct stat L;
	int r = g_stat("/bin/ls", &L);
	time_t tt = L.st_ctim.tv_sec;
	cStr Fallback;
	if(0 == r) {
		Fallback.Copy(ctime(&tt));
	}
	dword CRC = cMath::Checksum(Fallback.ToCharPtr(), Fallback.Length());
	//*************************************************************************
	// Get UUID of the device on mount point "/"
	//*************************************************************************
	struct fstab *T = getfsfile("/"); // Searches the file /etc/fstab and returns an entry where fs_file field matches the mount_point argument
	cStr UUID;
	if(T != nullptr) {
		static const cStr Example("UUID=ec4d5600-6571-4fd0-8829-8d7126f201fb");
		UUID = T->fs_spec;
		if(cStr::Equals(UUID, Example, 5) && UUID.Length() == Example.Length()) {
			CRC = cMath::Checksum(UUID.ToCharPtr(), UUID.Length());
		}
	}
	ID = cStr::Format("%x", CRC);
#endif // COMMS_LINUX
	return ID;
} // cIO::GetDiskSerialNumber

// cIO::CheckDifferentDisks
#if defined COMMS_WINDOWS || defined COMMS_LINUX || defined COMMS_MACOS
static void CheckDifferentDisks_GetStat(const char *PathName, struct stat *Stat) {
	cStr Pn = cIO::EnsureAbsolutePath(PathName);
	cList<cStr> L;
	Pn.Split(&L, "/\\");
	if(L.IsEmpty()) {
		return;
	}
	cStr CurPath;
#ifdef COMMS_WINDOWS
	CurPath = L[0]; // Drive letter
	CurPath.EnsureTrailingBackslash();
#endif // COMMS_WINDOWS
#if defined COMMS_LINUX || defined COMMS_MACOS
	CurPath = "/"; // Root
	L.Insert(0, cStr::Empty);
#endif // COMMS_LINUX || COMMS_MACOS
	struct stat S;
	int r = stat(CurPath.ToCharPtr(), &S);
	if(0 == r) {
		*Stat = S;
	}
	int i;
	for(i = 1; i < L.Count(); i++) {
		CurPath.AppendPath(L[i]);
#if defined COMMS_LINUX || defined COMMS_MACOS
		CurPath.BackSlashesToSlashes(); // '\' -> '/'
#endif // COMMS_LINUX || COMMS_MACOS
		r = stat(CurPath.ToCharPtr(), &S);
		if(0 == r) {
			*Stat = S;
		}
	}
}
#endif // COMMS_WINDOWS || COMMS_LINUX || COMMS_MACOS
bool cIO::CheckDifferentDisks(const char *PathName0, const char *PathName1) {
    bool d = false;
#if defined COMMS_WINDOWS || defined COMMS_LINUX || defined COMMS_MACOS
	struct stat S0, S1;
	memset(&S0, 0, sizeof(S0));
	memset(&S1, 0, sizeof(S1));
	CheckDifferentDisks_GetStat(PathName0, &S0);
	CheckDifferentDisks_GetStat(PathName1, &S1);
	d = (S0.st_dev != S1.st_dev);
#endif // COMMS_WINDOWS || COMMS_LINUX || COMMS_MACOS
    return d;
}

#ifdef COMMS_MACOS
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>

// cIO_FindEthernetInterfaces
static kern_return_t cIO_FindEthernetInterfaces(io_iterator_t *matchingServices) {
	kern_return_t kernResult;
	CFMutableDictionaryRef matchingDict;
	CFMutableDictionaryRef propertyMatchDict;
	
	matchingDict = IOServiceMatching(kIOEthernetInterfaceClass);
	if(nullptr == matchingDict) {
    } else {
		propertyMatchDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if(nullptr == propertyMatchDict) {
		} else {
			CFDictionarySetValue(propertyMatchDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue);
			CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
			CFRelease(propertyMatchDict);
		}
	}
	kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, matchingServices);
	if(KERN_SUCCESS != kernResult) {
    }
	return kernResult;
}

// cIO_GetMACAddress
static kern_return_t cIO_GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress, UInt8 bufferSize) {
	io_object_t intfService;
	io_object_t controllerService;
	kern_return_t kernResult = KERN_FAILURE;
	
	if(bufferSize < kIOEthernetAddressSize) {
		return kernResult;
	}
    bzero(MACAddress, bufferSize);
	while((intfService = IOIteratorNext(intfIterator))) {
		CFTypeRef MACAddressAsCFData;
		kernResult = IORegistryEntryGetParentEntry(intfService, kIOServicePlane, &controllerService);
		if(KERN_SUCCESS != kernResult) {
        } else {
			MACAddressAsCFData = IORegistryEntryCreateCFProperty(controllerService, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
			if(MACAddressAsCFData) {
				CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), MACAddress);
				CFRelease(MACAddressAsCFData);
			}
			IOObjectRelease(controllerService);
		}
		IOObjectRelease(intfService);
	}
	return kernResult;
}
#endif // COMMS_MACOS

//-----------------------------------------------------------------------------
// cIO::GetMACAddress
//-----------------------------------------------------------------------------
const cStr cIO::GetMACAddress() {

#ifdef COMMS_WINDOWS
	typedef struct _ASTAT_ {
		ADAPTER_STATUS adapt;
		NAME_BUFFER NameBuff [30];
	} ASTAT, *PASTAT;
	ASTAT Adapter;
	NCB Ncb;
	UCHAR uRetCode;
	LANA_ENUM lenum;
	int i;
	memset(&Ncb, 0, sizeof(Ncb));
	Ncb.ncb_command = NCBENUM;
	Ncb.ncb_buffer = (UCHAR *)&lenum;
	Ncb.ncb_length = sizeof(lenum);
	uRetCode = Netbios(&Ncb);
	for(i = 0; i < lenum.length; i++) {
		memset(&Ncb, 0, sizeof(Ncb));
		Ncb.ncb_command = NCBRESET;
		Ncb.ncb_lana_num = lenum.lana[i];
		uRetCode = Netbios(&Ncb);
		memset(&Ncb, 0, sizeof(Ncb));
		Ncb.ncb_command = NCBASTAT;
		Ncb.ncb_lana_num = lenum.lana[i];
		// "*               "
		Ncb.ncb_callname[0] = '*';
		for(i = 1; i < 16; i++) {
			Ncb.ncb_callname[i] = ' ';
		}
		Ncb.ncb_buffer = (PUCHAR)&Adapter;
		Ncb.ncb_length = sizeof(Adapter);
		uRetCode = Netbios(&Ncb);
		if(0 == uRetCode) {
			cStr S = cStr::Format("%02x:%02x:%02x:%02x:%02x:%02x",
				Adapter.adapt.adapter_address[0],
				Adapter.adapt.adapter_address[1],
				Adapter.adapt.adapter_address[2],
				Adapter.adapt.adapter_address[3],
				Adapter.adapt.adapter_address[4],
				Adapter.adapt.adapter_address[5]);
			return S;
		}
	}
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
	cStr MAC;

	kern_return_t kernResult = KERN_SUCCESS;
	io_iterator_t intfIterator;
	UInt8 MACAddress[kIOEthernetAddressSize];
	kernResult = cIO_FindEthernetInterfaces(&intfIterator);
	
	if(KERN_SUCCESS != kernResult) {
    } else {
		kernResult = cIO_GetMACAddress(intfIterator, MACAddress, sizeof(MACAddress));
		if(KERN_SUCCESS != kernResult) {
        } else {
			MAC = cStr::Format("%02x:%02x:%02x:%02x:%02x:%02x", MACAddress[0], MACAddress[1], MACAddress[2], MACAddress[3], MACAddress[4], MACAddress[5]);
		}
    }
	IOObjectRelease(intfIterator);
	
	return MAC;
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
    struct ifreq ifr, *IFR;
    char buf[1024];
    struct ifconf ifc;
    int i;
    bool r = false;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s != -1) {
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        ioctl(s, SIOCGIFCONF, &ifc);
        IFR = ifc.ifc_req;
        for(i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; IFR++) {
            strcpy(ifr.ifr_name, IFR->ifr_name);
            if(ioctl(s, SIOCGIFFLAGS, &ifr) == 0) {
                if(!(ifr.ifr_flags & IFF_LOOPBACK)) {
                    if(ioctl(s, SIOCGIFHWADDR, &ifr) == 0) {
                        r = true;
                        break;
                    }
                }
            }
        }
        close(s);
    }
    u_char addr[6];
    cStr MAC;
    if(r) {
        bcopy(ifr.ifr_hwaddr.sa_data, addr, 6);
        MAC = cStr::Format("%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        return MAC;
    }
#endif // COMMS_LINUX

	return cStr::Empty;
} // cIO::GetMACAddress

static cVec2i InputString_Pos;
static bool _center_x = true;
static bool _center_y = true;


#ifdef COMMS_WINDOWS

std::wstring utf8to16(const char* src)
{
	std::vector<wchar_t> buffer;
	buffer.resize(MultiByteToWideChar(CP_UTF8, 0, src, -1, 0, 0));
	MultiByteToWideChar(CP_UTF8, 0, src, -1, &buffer[0], (int)buffer.size());
	return &buffer[0];
}

std::string wcharToUtf8(const wchar_t* wc)
{
	int len = WideCharToMultiByte(CP_UTF8, 0, wc, -1, nullptr, 0, nullptr, nullptr);
	std::string result = "";
	result.resize(len, 0);
	WideCharToMultiByte(CP_UTF8, 0, wc, -1, &result[0], len, nullptr, nullptr);
	return result;
}

HWND cWinMain_NonmodalDialog = nullptr;

class cIO_StringDialog {
public:
	cStr Buffer;
	struct ID {
		enum Values {
			Edit = 101
		};
	};
	static cIO_StringDialog *ModalInstance, *NonmodalInstance;
	static float PixelsToDialogUnits;
	static bool ReturnPressed;
	static int NomodalWidth;
	// Ctor
	cIO_StringDialog() {
		m_DragIgnore = false;
		m_BackBrush = nullptr;
		m_TextColor = 0;
		m_BackColor = 0;
		CreateBrush();
	}
	// Dtor
	~cIO_StringDialog() {
		FreeBrush();
	}
	// CreateBrush
	void CreateBrush() {
		m_TextColor = cIO::InputStringColors.Text.ToDword() & 0x00ffffff;
		m_BackColor = cIO::InputStringColors.Back.ToDword() & 0x00ffffff;
		cAssert(nullptr == m_BackBrush);
		m_BackBrush = CreateSolidBrush(m_BackColor);
	}
	// FreeBrush
	void FreeBrush() {
		if(m_BackBrush != nullptr) {
			BOOL r = DeleteObject(m_BackBrush);
			cAssert(r);
			m_BackBrush = nullptr;
		}
	}
	static void FromEditToBuffer(HWND hDlg, cStr *Buffer) {
		const HWND hEdit = GetDlgItem(hDlg, ID::Edit);
		const int MaxLen = 4096;
		wchar_t wc[MaxLen];
		GetWindowTextW(hEdit, wc, MaxLen); // The maximum number of characters to copy to the buffer, including the null character.
		std::string mb = wcharToUtf8(wc);
		Buffer->Copy(mb.c_str());
	}
	static void DeleteNonmodal() {
		cAssert(cWinMain_NonmodalDialog != nullptr);
		DestroyWindow(cWinMain_NonmodalDialog); cWinMain_NonmodalDialog = nullptr;
		cAssert(NonmodalInstance != nullptr);
		delete NonmodalInstance; NonmodalInstance = nullptr;
	}
	// NonmodalEditProc
	static LRESULT CALLBACK NonmodalEditProc(HWND hEdit, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
		if(WM_KEYDOWN == Msg) {
			if(VK_RETURN == wParam) {
				SendMessage(cWinMain_NonmodalDialog, WM_COMMAND, MAKEWPARAM(IDOK, 0), 0);
			}
			if (VK_ESCAPE == wParam) {
				SendMessage(cWinMain_NonmodalDialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
			}
		}
		return DefSubclassProc(hEdit, Msg, wParam, lParam);
	}
	// DialogUnitsProc
	static LRESULT CALLBACK DialogUnitsProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
		if(WM_INITDIALOG == Msg) {
			RECT rc;
			memset(&rc, 0, sizeof(rc));
			const int DialogUnits = 100;
			rc.right = DialogUnits;
			// Dialog box units <---> Pixels
			// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mapdialogrect
			MapDialogRect(hDlg, &rc);
			PixelsToDialogUnits = (float)DialogUnits / (float)rc.right;
		}
		return FALSE;
	}
	// DlgProc
	static LRESULT CALLBACK DlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
		RECT rc;
		POINT p;
		HWND hEdit = nullptr;
		if(NonmodalInstance != nullptr && nullptr == cWinMain_NonmodalDialog) {
			cWinMain_NonmodalDialog = hDlg;
		}
		const bool Nonmodal = hDlg == cWinMain_NonmodalDialog;
		cIO_StringDialog *I = Nonmodal ? NonmodalInstance : ModalInstance;
		cAssert(I != nullptr);
	
		switch(Msg) {
			case WM_TIMER: {
				GetCursorPos(&p);
				GetWindowRect(hDlg, &rc);
				const bool LeftButton = GetKeyState(VK_LBUTTON) < 0;
				const bool RightButton = GetKeyState(VK_RBUTTON) < 0;
				const bool MiddleButton = GetKeyState(VK_MBUTTON) < 0;
				const bool AnyButton = LeftButton || RightButton || MiddleButton;
				if(AnyButton) {
					if(PtInRect(&rc, p)) {
						I->m_DragIgnore = true;
					} else if(!I->m_DragIgnore) {
						SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDOK, 0), 0);
					}
				} else {
					I->m_DragIgnore = false;
				}
				break;
			}
			case WM_INITDIALOG:
			{
				// Set text "Enter" on the button
				HWND hButton = GetDlgItem(hDlg, IDOK);
				wchar_t Enter[2] = { 0x23CE, 0 };
				SetWindowTextW(hButton, Enter);
				// Text edit initial text
				std::wstring ws = utf8to16(I->Buffer.ToCharPtr());
				hEdit = GetDlgItem(hDlg, ID::Edit);
				SetWindowTextW(hEdit, ws.c_str());
				SetFocus(hEdit);
				SendMessage(hEdit, EM_SETSEL, (WPARAM)0, (LPARAM)0xffffff);
				if(Nonmodal) {
					SetWindowSubclass(hEdit, NonmodalEditProc, 0, 0);
				}
				// Center the dialog
				GetWindowRect(hDlg, &rc);
				const int W = rc.right - rc.left;
				const int H = rc.bottom - rc.top;
				p.x = InputString_Pos[0];
				if(_center_x) p.x -= W / 2;
				p.y = InputString_Pos[1];
				if(_center_y) p.y -= H / 2;
				// Keep inside client
				const int CW = cMain_GetClientWidth();
				const int CH = cMain_GetClientHeight();
				p.x = cMath::Clamp((int)p.x, 0, CW - W);
				p.y = cMath::Clamp((int)p.y, 0, CH - H);
				// Move window
				ClientToScreen(cWinMain_GetWindow(), &p);
				MoveWindow(hDlg, p.x, p.y, W, H, true);
				// Timer
				SetTimer(hDlg, 1, 20, nullptr);
				break;
			}
			case WM_CTLCOLORBTN:
			case WM_CTLCOLORDLG:
			case WM_CTLCOLOREDIT:
				if(!cIO::InputStringColors.Override) {
					return 0;
				}
				SetTextColor((HDC)wParam, I->m_TextColor);
				SetBkColor((HDC)wParam, I->m_BackColor);
				return (LRESULT)I->m_BackBrush;
			case WM_COMMAND:
			{
				const WORD id = LOWORD(wParam);
				const WORD nc = HIWORD(wParam);
				if(ID::Edit == id) {
					if(EN_CHANGE == nc) {
						FromEditToBuffer(hDlg, &I->Buffer);
					}
				}
				if(IDOK == id) {
					if(Nonmodal) {
						ReturnPressed = true;
						DeleteNonmodal();
					} else {
						EndDialog(hDlg, IDOK);
					}
				}
				if(IDCANCEL == id) {
					ReturnPressed = false;					
					if (Nonmodal) {
						DeleteNonmodal();
					} else {
						EndDialog(hDlg, IDCANCEL);
					}
				}
				break;
			}
			case WM_CLOSE:
				EndDialog(hDlg, IDCANCEL);
				break;
			case WM_ACTIVATE: {
				BOOL Inactive = WA_INACTIVE == LOWORD(wParam);
				if(Inactive) {
					cPause::SetSystemPause(true);
				}
				break;
				}
		}
		return FALSE;
	}
	static void RunNonmodal(const int EditWidthPixels, const cStr *OptionalInit) {
		FlipY();
		cWin32::DlgTemplate Dlg;
		CreateTemplate(EditWidthPixels, &Dlg);
		NomodalWidth = EditWidthPixels;

		cAssert(nullptr == NonmodalInstance);
		NonmodalInstance = new cIO_StringDialog;
		if(OptionalInit != nullptr) {
			NonmodalInstance->Buffer = *OptionalInit;
		}
		const HWND hDlg = CreateDialogIndirectW(GetModuleHandle(nullptr), Dlg.ToDlgTemplatePtr(), cWinMain_GetWindow(), (DLGPROC)cIO_StringDialog::DlgProc);
		cAssert(hDlg == cWinMain_NonmodalDialog);
	}
	static bool RunModal(cStr *S, const int EditWidthPixels) {
		FlipY();
		cWin32::DlgTemplate Dlg;
		CreateTemplate(EditWidthPixels, &Dlg);

		cAssert(nullptr == ModalInstance);
		ModalInstance = new cIO_StringDialog;
		ModalInstance->Buffer = *S;
		const int ID = (int)DialogBoxIndirectW(GetModuleHandle(nullptr), Dlg.ToDlgTemplatePtr(), cWinMain_GetWindow(), (DLGPROC)cIO_StringDialog::DlgProc);
		const bool Ok = IDOK == ID;
		if(Ok) {
			*S = ModalInstance->Buffer;
		}
		delete ModalInstance; ModalInstance = nullptr;
		cInput::FreeEvents();
		return Ok;
	}
	static int GetInputPixelsHeight() {
		if (PixelsToDialogUnits == 0.0f) {
			FillPixelsToDialogUnits();
		}
		if (PixelsToDialogUnits > 0) {
			return 14 / PixelsToDialogUnits;
		}
		return 20;
	}
	static int GetInputPixelsWidth() {
		if (PixelsToDialogUnits == 0.0f) {
			FillPixelsToDialogUnits();
		}
		return NomodalWidth;
	}
private:
	static void FlipY() {
		const int CH = cMain_GetClientHeight();
		InputString_Pos[1] = CH - InputString_Pos[1];
	}
	static void FillPixelsToDialogUnits() {
		cWin32::DlgTemplate DialogUnitsTemplate;
		DialogUnitsTemplate.Create("", 0, 0, 0, 0, 0, 0, "MS Shell Dlg", 8);
		HWND hDialogUnits = CreateDialogIndirectW(GetModuleHandle(nullptr), DialogUnitsTemplate.ToDlgTemplatePtr(), cWinMain_GetWindow(), (DLGPROC)cIO_StringDialog::DialogUnitsProc);
		DestroyWindow(hDialogUnits); hDialogUnits = nullptr;
	}
	static void CreateTemplate(const int EditWidthPixels, cWin32::DlgTemplate *Dlg) {
		FillPixelsToDialogUnits();
		const int EditWidthDialogUnits = (int)((float)EditWidthPixels * PixelsToDialogUnits);
		const int Border = 1;
		const int EditWidth = EditWidthDialogUnits;
		const int ButtonWidth = 0, EditButtonHeight = 10;
		const int DlgWidth = Border + EditWidth + Border + ButtonWidth + Border;
		const int DlgHeight = Border + EditButtonHeight + Border;
		Dlg->Create("", WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS | WS_BORDER, WS_EX_TOOLWINDOW, 0, 0, DlgWidth, DlgHeight, "MS Shell Dlg", 8);
		//Dlg->AddButton("", WS_VISIBLE | WS_TABSTOP, 0, Border + EditWidth + Border, Border, ButtonWidth, EditButtonHeight, IDOK);
		Dlg->AddEditBox("", WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_CENTER, 0, Border, Border, EditWidth, EditButtonHeight, cIO_StringDialog::ID::Edit);
	}	
	bool m_DragIgnore;
	HBRUSH m_BackBrush;
	DWORD m_TextColor, m_BackColor;
};

bool cIO_StringDialog::ReturnPressed = false;
cIO_StringDialog *cIO_StringDialog::ModalInstance = nullptr;
cIO_StringDialog *cIO_StringDialog::NonmodalInstance = nullptr;
float cIO_StringDialog::PixelsToDialogUnits = 0.0f;
int cIO_StringDialog::NomodalWidth = 0;

#ifndef DOM3D_COMMS_STANDALONE
__postprocess(colors) {
	comms::cIO::InputStringColors.Back = comms::cColor::FromDword(AppOpt.InterfaceColor);
	comms::cIO::InputStringColors.Text = comms::cColor::FromDword(AppOpt.FontColor);
	comms::cIO::InputStringColors.Override = true;
}
#endif

int cIO::GetInputPixelsHeight() {
	return cIO_StringDialog::GetInputPixelsHeight();
}

int cIO::GetInputPixelsWidth() {
	return cIO_StringDialog::GetInputPixelsWidth();
}
#endif // COMMS_WINDOWS

cIO::_InputStringColors cIO::InputStringColors;

bool cIO::InputString(const int X, const int Y, cStr *S, const int EditWidthPixels) {
	_center_x = true;
	_center_y = true;
	bool r = false;
	InputString_Pos.Set(X, Y);
	
#ifdef COMMS_MACOS
	r = cMacMain_InputString(InputString_Pos, S);
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
	r = cLinuxMain_InputString(S);
#endif // COMMS_LINUX

#ifdef COMMS_WINDOWS
	r = cIO_StringDialog::RunModal(S, EditWidthPixels);
#endif // COMMS_WINDOWS

	return r;
}

void cIO::ShowString(const cStr *OptionalInit, int dx, int dy, bool center_x, bool center_y) {
#ifdef COMMS_WINDOWS
	if(cIO_StringDialog::NonmodalInstance != nullptr) {
		return;
	}
	const int X = (int)cInput::GetMousePosition().x + dx;
	const int Y = (int)cInput::GetMousePosition().y + dy;
	_center_x = center_x;
	_center_y = center_y;
	InputString_Pos.Set(X, Y);
	cIO_StringDialog::ReturnPressed = false;
	const int input_width =
#ifdef DOM3D_COMMS_STANDALONE
		200;
#else
		_ui_scale(200);
#endif
	cIO_StringDialog::RunNonmodal(input_width, OptionalInit);
	cAssert(cIO_StringDialog::NonmodalInstance != nullptr);
#endif // COMMS_WINDOWS
}

bool cIO::ReturnPressed() {
#ifdef COMMS_WINDOWS
	bool r = cIO_StringDialog::ReturnPressed;
	cIO_StringDialog::ReturnPressed = false;
	return r;
#else
	return false;
#endif // COMMS_WINDOWS
}


void comms::cIO::StopStringInput() {
#ifdef COMMS_WINDOWS
	cIO_StringDialog::DeleteNonmodal();
#endif
}

bool cIO::GetString(cStr *S) {
#ifdef COMMS_WINDOWS
	if(nullptr == cIO_StringDialog::NonmodalInstance) {
		return false;
	}
	if(S != nullptr) {
		S->Copy(cIO_StringDialog::NonmodalInstance->Buffer);
	}
	return true;
#endif // COMMS_WINDOWS
	return false;
}

void StopStringInput() {
#ifdef COMMS_WINDOWS
	cIO_StringDialog::DeleteNonmodal();
#endif // COMMS_WINDOWS
}
int GetStringInputWidth() {
#ifdef COMMS_WINDOWS
	if(nullptr == cIO_StringDialog::NonmodalInstance) {
		return 0;
	}
	HWND hDlg = cWinMain_NonmodalDialog;
	cAssert(hDlg != nullptr);
	RECT rc;
	GetWindowRect(hDlg, &rc);
	return rc.right - rc.left;
#endif // COMMS_WINDOWS
}

struct cIO_FileDialogPref {
    cStr PrefKey;
    cStr InitialFolder;
};
static cList<cIO_FileDialogPref> cIO_FileDialogPrefs;

// cIO::GetFileDialogInitialFolder
const cStr cIO::GetFileDialogInitialFolder(const char *PrefKey) {
	cStr P = PrefKey;
	cStr F = GetDefaultSourceFolder();
	ReplaceReadPath(&F);
	
	int i;
	for(i = 0; i < cIO_FileDialogPrefs.Count(); i++) {
		const cIO_FileDialogPref &r = cIO_FileDialogPrefs[i];
		if(r.PrefKey == P) {
			F = r.InitialFolder;
			break;
		}
	}
	
	return F;
}

// cIO::SetFileDialogInitialFolder
void cIO::SetFileDialogInitialFolder(const char *PrefKey, const char *Folder) {
	cStr P = PrefKey;
	cStr F = EnsureAbsolutePath(Folder);
	
	int i;
	for(i = 0; i < cIO_FileDialogPrefs.Count(); i++) {
		cIO_FileDialogPref &r = cIO_FileDialogPrefs[i];
		if(r.PrefKey == P) {
			r.InitialFolder = F;
			return;
		}
	}
	
	cIO_FileDialogPref T;
	T.PrefKey = P;
	T.InitialFolder = F;
	cIO_FileDialogPrefs.Add(T);
}

// cIO::GetFileDialogPrefExists
bool cIO::GetFileDialogPrefExists(const char *PrefKey) {
	cStr P = PrefKey;
	
	int i;
	for(i = 0; i < cIO_FileDialogPrefs.Count(); i++) {
		const cIO_FileDialogPref &r = cIO_FileDialogPrefs[i];
		if(r.PrefKey == P) {
			return true;
		}
	}
	return false;
}

// cIO::GetUserFolder
const cStr cIO::GetUserFolder() {
    static cStr r;
	if (r.IsEmpty()) {
		r = GetDocumentsFolder();
		r.TrimEnd("/");
	}
	return r;
}

// cIO::GetCacheFolder
const cStr cIO::GetCacheFolder() {
    cStr r;
#ifdef COMMS_LINUX
    r = g_get_user_cache_dir();
#endif // COMMS_LINUX
	return r;
}

// cIO::GetEnvironmentVariable
const cStr cIO::GetEnvironmentVariable(const char *VarName) {
	if(nullptr == VarName) {
		return cStr::Empty;
	}
	cStr Value;
#ifdef COMMS_WINDOWS
	char *Buffer = nullptr;
	size_t Size = 0;
	if(_dupenv_s(&Buffer, &Size, VarName) == 0) {
		Value = Buffer;
		free(Buffer);
	}
#else // !COMMS_WINDOWS
	Value = getenv(VarName);
#endif // COMMS_WINDOWS
	return Value;
}

#ifdef COMMS_WINDOWS
wchar_path::wchar_path(const char *Src) {
	path[0] = 0;
	if(Src) {
		const int L = (int)strlen(Src);
		path[MultiByteToWideChar(CP_UTF8, 0, Src, L, path, WCHAR_MAX_PATH)] = 0;
	}
}
wchar_path::wchar_path(const wchar_t* _path) {
	if(_path)wcscpy_s(path, WCHAR_MAX_PATH, _path);
}
wchar_path::wchar_path(const ::std::wstring& _path) {
	wcscpy_s(path, WCHAR_MAX_PATH, _path.c_str());
}
wchar_path::wchar_path() {
	path[0] = 0;
}
wchar_path::operator const wchar_t*() const {
	return path;
}
wchar_path::operator wchar_t*() {
	return path;
}
cStr wchar_path::toStr() {
	cStr s;
	s.Copy(path);
	return s;
}
int wchar_path::maxlen() {
	return WCHAR_MAX_PATH;
}
int wchar_path::length() {
	return (int)wcslen(path);
}
#endif // Windows

// cMain_SetTitle
void cMain_SetTitle(const char *Title) {
    cStr TitleSuffix;
#ifdef COMMS_ASSERT
    TitleSuffix = "(Debug)";
#endif // COMMS_ASSERT
#ifdef COMMS_64
    if(TitleSuffix.IsEmpty()) {
        TitleSuffix = "(64)";
    } else {
        TitleSuffix.TrimEnd(")");
        TitleSuffix += " 64)";
    }
#endif // COMMS_64
    TitleSuffix += " Build: " __DATE__ " " __TIME__;
    cMain_Title = Title + cStr(" | ") + TitleSuffix;
    cMain_SetWindowTitle(cMain_Title);
}

} // comms
