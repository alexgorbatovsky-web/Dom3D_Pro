#pragma once

#define COAT_USER_PATH_ENV "COAT_USER_PATH"

//-------------------------------------------------------------------------
// ImageCodec
//-------------------------------------------------------------------------
/**
 * \brief Abstract base class for image decoding and encoding.
 * * Implement this interface to add support for new image formats.
 */
class APICALL cImageCodec {
public:
	cImageCodec() {}
	virtual ~cImageCodec() {}

	/**
	 * \brief Decodes an image from a source file buffer.
	 * \param Src The source file data.
	 * \param To The destination image object.
	 * \return true if decoding was successful, false otherwise.
	 */
	virtual bool Decode(const cFile& Src, cImage* To) { return false; };

	/**
	 * \brief Encodes an image into a file buffer.
	 * \param Image The source image to encode.
	 * \param To The destination file buffer.
	 * \return true if encoding was successful, false otherwise.
	 */
	virtual bool Encode(const cImage& Image, cFile* To) { return false; };

	/**
	 * \brief Checks if the file magic number matches this codec.
	 * \param Magic The magic number (usually the first 4 bytes of the file).
	 * \param ext The file extension.
	 * \return 1 if magic is correct, 0 if incorrect, -1 if this format has no magic number.
	 */
	virtual int CheckMagic(dword Magic, const char* ext) { return -1; };
};

//*************************************************************************
// Image codecs
//*************************************************************************
/**
 * \brief Structure holding information about a registered image codec.
 */
struct cImageCodecInfo {
	cStr FileExtension;     ///< The file extension supported by the codec (e.g., "png", "jpg").
	cImageCodec* Codec;     ///< Pointer to the codec instance.
	static cStr ErrorMessage; ///< Static error message buffer for codec operations.
};

//*****************************************************************************
// cIO
//*****************************************************************************
/**
 * \brief Main Input/Output utility class.
 * * Provides static methods for file manipulation, path handling, ZIP archives,
 * system dialogs, and image loading/saving.
 */
class APICALL cIO {
public:
	//-------------------------------------------------------------------------
	// Source
	//-------------------------------------------------------------------------
	/**
	 * \brief Abstract interface for a data source.
	 */
	class Source {
	public:
		Source() {}
		virtual ~Source() {}
		/**
		 * \brief Loads a file from the source.
		 * \param FilePn Path name of the file.
		 * \param To Buffer to load data into.
		 * \return true if loaded successfully.
		 */
		virtual bool Load(const char* FilePn, cFile* To) const = 0;

		/**
		 * \brief Saves data to the source.
		 * \param FilePn Path name to save to.
		 * \param Src Pointer to data source.
		 * \param Size Size of data in bytes.
		 * \param Append If true, appends to existing file.
		 * \return true if saved successfully.
		 */
		virtual bool Save(const char* FilePn, const void* Src, const size_t Size, const bool Append) const = 0;

		/**
		 * \brief Gets the root path of this source.
		 */
		virtual const cStr& GetPath() const = 0;
	};

	/** \brief Returns the list of registered sources. */
	static const cList<cIO::Source*>& GetSources();

	/** \brief Returns the mutable list of registered sources. */
	static cList<cIO::Source*>& GetSourcesMutable();


	//-------------------------------------------------------------------------
	// ZIP
	//-------------------------------------------------------------------------
	/**
	 * \brief Mounts a ZIP file as a data source.
	 * \param ZipFilePn Path to the zip file.
	 * \param DataFolderWithinZip Optional subfolder within the zip to treat as root.
	 */
	static void AddSourceZip(const char* ZipFilePn, const char* DataFolderWithinZip = nullptr);

	/**
	 * \brief Searches for ZIP files to add as sources.
	 * \param DataFolderWithinZip Optional subfolder check.
	 */
	static void SearchZipSources(const char* DataFolderWithinZip = nullptr);

#ifndef PY_PARSER
	typedef void (*ZipProgress)(const int Perc); /// Called only when the percentage has changed

	/**
	 * \brief Extracts a ZIP archive to a destination folder.
	 * \param ZipFilePn Path to the zip file.
	 * \param ToFolder Destination directory.
	 * \param Func Optional callback for progress reporting.
	 * \param filereport Optional callback reporting current file being extracted.
	 * \return true if extraction succeeded.
	 */
	static bool ExtractZip(const char* ZipFilePn, const char* ToFolder, ZipProgress Func = nullptr, std::function<void(const char*)> filereport = nullptr);

	/**
	 * \brief Creates a ZIP file containing a single file.
	 * \param FilePn Path to the source file.
	 * \param FilePnInZip Path/Name of the file inside the archive.
	 * \param ZipFilePn Destination ZIP file path.
	 * \param Func Optional progress callback.
	 */
	static bool CreateZip_FromFile(const char* FilePn, const char* FilePnInZip, const char* ZipFilePn, ZipProgress Func = nullptr);

	/**
	 * \brief Creates a ZIP archive from a folder.
	 * \param ParentFolder Folder to compress.
	 * \param ZipFilePn Destination ZIP file path.
	 * \param WithParent If true, includes the parent folder name in the archive structure.
	 * \param Func Optional progress callback.
	 */
	static bool CreateZip_FromFolder(const char* ParentFolder, const char* ZipFilePn, const bool WithParent, ZipProgress Func = nullptr);
#endif

	/**
	 * \brief Inflates a raw deflated chunk into memory.
	 * \param SrcCompressed Pointer to compressed data.
	 * \param CompressedSize Size of compressed data.
	 * \param ToMemory Destination file buffer.
	 * \return true if successful.
	 */
	static bool ZipInflateRaw(const void* SrcCompressed, const size_t CompressedSize, cFile* ToMemory);
	static void ZipInflateRaw_Debug();

	//-------------------------------------------------------------------------
	// File IO
	//-------------------------------------------------------------------------
	/**
	 * \brief Loads a file into memory.
	 * \param FilePn Path to the file.
	 * \param To Destination cFile object.
	 * \param ShowWarning If true, shows a warning if loading fails.
	 * \return true if successful.
	 */
	static bool LoadFile(const char* FilePn, cFile* To, const bool ShowWarning = true);

	/**
	 * \brief Saves raw memory to a file.
	 * \param FilePn Path to the file.
	 * \param Src Source memory pointer.
	 * \param Size Size to write.
	 * \param Append If true, appends to the file.
	 * \param ShowWarning If true, shows a warning on failure.
	 * \return true if successful.
	 */
	static bool SaveFile(const char* FilePn, const void* Src, const size_t Size, const bool Append = false, const bool ShowWarning = true);

	/**
	 * \brief Saves a cFile object to disk.
	 * \param FilePn Path to the file. If nullptr, uses Src.GetFilePn().
	 * \param Src Source cFile object.
	 * \param Append If true, appends to the file.
	 * \param ShowWarning If true, shows a warning on failure.
	 * \return true if successful.
	 */
	static bool SaveFile(const char* FilePn, const cFile& Src, const bool Append = false, const bool ShowWarning = true);

	//-------------------------------------------------------------------------
	// File & folder dialogs
	//-------------------------------------------------------------------------
	/**
	 * \brief Opens a system dialog to select a single file for loading.
	 * \param Title Dialog title.
	 * \param Extensions List of allowed extensions.
	 * \param SingleFilePn Output for selected file path.
	 * \param PrefKey Registry/Config key to store/retrieve the last visited folder.
	 * \param InitialFileName Optional initial filename.
	 * \return true if a file was selected.
	 */
	static bool LoadFileDialog(const char* Title, const cList<cStr>& Extensions, cStr* SingleFilePn, const char* PrefKey, const char* InitialFileName = nullptr);

	/**
	 * \brief Opens a system dialog to select multiple files for loading.
	 * \param Title Dialog title.
	 * \param Extensions List of allowed extensions.
	 * \param MultiFilePn Output list for selected file paths.
	 * \param PrefKey Registry/Config key.
	 * \param InitialFileName Optional initial filename.
	 * \return true if files were selected.
	 */
	static bool LoadFileDialog(const char* Title, const cList<cStr>& Extensions, cList<cStr>* MultiFilePn, const char* PrefKey, const char* InitialFileName = nullptr);

	/**
	 * \brief Opens a system dialog to select a file for saving.
	 * \param Title Dialog title.
	 * \param Extensions List of allowed extensions.
	 * \param FilePn Output for selected file path.
	 * \param PrefKey Registry/Config key.
	 * \param DefaultExtension Default extension to append if none provided.
	 * \param InitialFileBase Optional initial file name (without path).
	 * \return true if a file was selected.
	 */
	static bool SaveFileDialog(const char* Title, const cList<cStr>& Extensions, cStr* FilePn, const char* PrefKey, const char* DefaultExtension = nullptr, const char* InitialFileBase = nullptr);

	/** \brief Sets the initial folder for a specific preference key (can be absolute or relative). */
	static void SetFileDialogInitialFolder(const char* PrefKey, const char* Folder);

	/** \brief Returns the initial folder for the given key. Returns "GetDefaultSourceFolder" if key is new. */
	static const cStr GetFileDialogInitialFolder(const char* PrefKey);

	/** \brief Returns false if there is no custom initial folder stored for this PrefKey. */
	static bool GetFileDialogPrefExists(const char* PrefKey);

	/**
	 * \brief Opens a dialog to select a folder.
	 * \param Title Dialog title.
	 * \param SelectedFolder Output for selected folder path.
	 * \param InitialFolder Optional start folder (absolute or relative).
	 * \return true if a folder was selected.
	 */
	static bool SelectFolderDialog(const char* Title, cStr* SelectedFolder, const char* InitialFolder = nullptr);

	/** \brief Registers a new image codec for a specific extension. */
	static bool AddCodec(const char* FileExtension, cImageCodec* Codec);

	/** \brief Loads an image from file. */
	static bool LoadImage(const char* FilePn, cImage* To);

	/** \brief Saves an image to file. */
	static bool SaveImage(const char* FilePn, const cImage& Src);

	/** \brief Returns the list of all registered image codecs. */
	static const cList<cImageCodecInfo>& GetImageCodecs();

	/** \brief Finds a codec responsible for the given extension. */
	static cImageCodec* FindImageCodec(const char* FileExtension);

	/** \brief Checks magic bytes to determine actual file extension. Returns true if original extension matches. */
	static bool GetActualFileExtensionByMagic(const char* FilePn, cStr& ext);
	static bool GetActualFileExtensionByMagic(dword Magic, cStr& ext);

	/** \brief Returns the path of the last opened resource. */
	static cStr& LastOpenedResource();

	//*************************************************************************
	// Image file dialogs
	//*************************************************************************
	static bool LoadImageDialog(cStr* ImageFilePn);
	static bool SaveImageDialog(cStr* ImageFilePn);
	static void SetImageDialogInitialPath(const char* InitialPath);

	/** \brief Sets initial file for image dialog. Overrides initial path if not nullptr. */
	static void SetImageDialogInitialFile(const char* InitialFile);

	static const cStr& GetImageDialogInitialPath();
	static const cStr& GetImageDialogInitialFile();

	/** \brief Saves a screenshot of the given image, optionally adding a suffix. */
	static void SaveScreenShot(const cImage& I, const char* OptionalSuffix = nullptr);

	/** \brief Captures the current screen and saves it. */
	static void MakeAndSaveScreenShot();

	//*************************************************************************
	// Xml file dialogs
	//*************************************************************************
	static bool LoadXmlDialog(cStr* XmlFilePn);
	static bool SaveXmlDialog(cStr* XmlFilePn);
	static void SetXmlDialogInitialPath(const char* InitialPath);
	static void SetXmlDialogInitialFile(const char* InitialFile); // If not nullptr, it will override initial path
	static const cStr& GetXmlDialogInitialPath();

	/** \brief Returns last loaded/saved XML file or the one set via SetXmlDialogInitialFile. */
	static const cStr& GetXmlDialogInitialFile();

	//*************************************************************************
	// Model & Anim & Camera & Bsp file dialogs
	//*************************************************************************
	// Model, Animation, Camera, and Bsp dialogs have common initial path, because they are in the same folder
	static void SetModelAnimCameraBspDialogsInitialPath(const char* InitialPath);
	static const cStr& GetModelAnimCameraBspDialogsInitialPath();

	/* !!!No definitions
	static bool LoadModelDialog(cStr *ModelFilePn);
	static bool SaveModelDialog(cStr *ModelFilePn);

	static bool LoadAnimDialog(cStr *AnimFilePn);
	static bool SaveAnimDialog(cStr *AnimFilePn);

	static bool LoadCameraDialog(cStr *CameraFilePn);
	static bool SaveCameraDialog(cStr *CameraFilePn);
	*/

	// Model, Animation, Camera, and Bsp dialogs should have initial file only for
	// built-in types, such as "cModel", "cAnim", "cCamera", and "cBsp" because
	// decoding general formats for preview takes a lot of time
	static void SetModelDialogInitialFile(const char* InitialFile);
	static void SetAnimDialogInitialFile(const char* InitialFile);
	static void SetBspDialogInitialFile(const char* InitialFile);
	static void SetCameraDialogInitialFile(const char* InitialFile);

	//*************************************************************************
	// Helpers
	//*************************************************************************
	/** \brief Returns "true" if path is absolute local or network (contains drive letter or starts from two (back)slashes). */
	static bool PathIsAbsolute(const char* Path);

	/** \brief Converts the path to relative. Returns true if shortening happened successfully. */
	static bool MakeRelativePath(cStr* Path);

	/**
	 * \brief Builds an absolute path relative to the default source folder if "Path" is relative.
	 * Also ensures correct (back)slashes.
	 */
	static const cStr EnsureAbsolutePath(const char* Path);

	/**
	 * \brief Returns folder of the first (default) source.
	 * usually program folder or its parent if running in Debug/Release.
	 */
	static const cStr& GetDefaultSourceFolder();

	/** \brief Defines alternative write path for source folder. */
	static void SetDefaultSourceWriteFolder(const char* WriteFolder);

	/**
	 * \brief Updates path to point to the write folder if applicable.
	 * Returns "true" if:
	 * 1. "WriteFolder" is defined;
	 * 2. "Path" lies within default source folder;
	 * 3. The argument "Path" has been altered and contains absolute path including "WriteFolder".
	 */
	static bool ReplaceReadPath(cStr* Path);
	static bool ReplaceReadPathForExistingFile(cStr* Path);

	/**
	 * \brief Checks if path is within the default source folder structure.
	 * Returns "true" if:
	 * 1. "WriteFolder" is defined;
	 * 2. "Path" lies within default source folder.
	 */
	static bool CheckReadPath(const char* Path);

	/** \brief Exits this application and launches this executable again. */
	static void RestartThisApplication();

	/** \brief Returns absolute path to this executable file including its name. */
	static const cStr& GetThisFilePathName();

	/** \brief Returns documents folder. Linux: /home/USER/Documents/, macOS: /Users/USER/Documents/, Windows: C:/Users/USER/Documents/ */
	static const cStr& GetDocumentsFolder();

	static const cStr& GetSystemDocumentsFolder();

	/** \brief Returns downloads folder. */
	static const cStr& GetDownloadsFolder();

	/** \brief Returns desktop folder (Windows only). */
	static const cStr& GetDesktopFolder();
#ifdef COMMS_LINUX
	static void GetLinuxFolder(cStr* Result, const char* FolderID);
#endif // Linux
#ifdef COMMS_MACOS
	static void GetMacOSFolder(cStr* Documents, cStr* Downloads, cStr* Home);
#endif // macOS
	/** \brief Searches for files in a folder. */
	static void SearchFiles(const char* Folder, cList<cStr>* Files, const bool SeekInWritePath = true);

	/** \brief Searches for files with specific extensions (e.g. "jpg;png"). */
	static void SearchFiles(const char* Folder, cList<cStr>* Files, const char* Extensions);

	/** \brief Searches for subfolders. */
	static void SearchFolders(const char* Folder, cList<cStr>* Folders);

	static void SearchFilesRecursive(const char* Folder, cList<cStr>* Files);
	static void SearchFilesRecursive(const char* Folder, cList<cStr>* Files, const char* Extensions); // i.e. "dds;bmp;jpg"

	/** \brief Corrects file attributes to ensure read/write rights. */
	static void SetFilePermissions(const char* FilePn);
	static void SetFolderPermissions(const char* Folder);

	/**
	 * \brief Checks existence of all intermediate directories and creates them if necessary.
	 * * "PathOrFilePn" can be absolute / relative path or file name without path.
	 * Examples:
	 * - `cIO::CreatePath("1/2/3");`
	 * - Windows: `cIO::CreatePath("c:/A/B\\C\\File.txt")`
	 * - MacOS: `cIO::CreatePath("/A/B\\C")`
	 * \param PathOrFilePn The path to create.
	 * \param inUserDocuments If true, tries to create in user documents folder.
	 * \return "true" if all intermediate directories exist or have been created.
	 */
	static bool CreatePath(const char* PathOrFilePn, bool inUserDocuments = true);

	/**
	 * \brief Executes an external file or program.
	 * * Usage examples:
	 * 1. Document pathname:
	 * - `cIO::Exec(cIO::EnsureAbsolutePath("~/Scripts/Scripting.pdf"))`
	 * 2. Program with absolute path and optional arguments:
	 * - Windows: `cIO::Exec("C:\\...\\maya.exe", "D:\\...\\Logo.mb")`
	 * - Linux: `cIO::Exec("/usr/bin/env python", "-V")`
	 * 3. URL with protocol:
	 * - `cIO::Exec("http://3d-coat.com/")`
	 * * \param FilePn Path to file, program, or URL.
	 * \param Args Optional arguments.
	 * \param Wait If true, waits for process to finish.
	 * \param Hide If true, hides the window.
	 * \return true if execution started successfully.
	 */
	static bool Exec(const char* FilePn, const char* Args = nullptr, bool Wait = false, bool Hide = false);

	/**
	 * \brief Checks if a process with the given name is running.
	 * \param ProcName Process name (case-insensitive partial match).
	 * \return true if process exists.
	 */
	static bool CheckProcessInMemory(const char* ProcName);

	/**
	 * \brief Opens a folder in the system file explorer.
	 * Examples: `cIO::Explore("Shaders")`
	 * \param Folder Absolute or relative path.
	 * \return true on success.
	 */
	static bool Explore(const char* Folder);

	/**
	 * \brief Compares file modification times.
	 * \param FilePn0 First file path.
	 * \param FilePn1 Second file path.
	 * \return "true" when "FilePn0" is older than "FilePn1".
	 */
	static bool CompareFileChangeTimes(const char* FilePn0, const char* FilePn1);

	/** \brief Returns true if FileToUpdate does not exist or is older than SourceFile. */
	static bool NeedUpdateFile(const char* FileToUpdate, const char* SourceFile);

	/**
	 * \brief Combines two images into an output image with the same format.
	 * \return true if successful.
	 */
	static bool CombineImages(const char* File1, const char* File2, const char* FileOut);

	/**
	 * \brief Compares two images for equality.
	 * \param File1 First image path.
	 * \param File2 Second image path.
	 * \param Tolerance Threshold for pixel value difference.
	 * \param PtRes Optional list to store coordinates of different pixels.
	 * \return "true" if images are considered equal.
	 */
	static bool CompareImages(const char* File1, const char* File2, const int Tolerance = 1, cList<comms::cVec2>* PtRes = nullptr);

	/**
	 * \brief Returns "true" when files/folders "PathName0" and "PathName1" reside on different disks.
	 * Note: paths could be non-existent.
	 */
	static bool CheckDifferentDisks(const char* PathName0, const char* PathName1);

	static void CopyToClipboard(const char* Src);
	static bool CopyFromClipboard(cStr* To);

	static void CopyToClipboard(const cImage& Src);
	static bool CopyFromClipboard(cImage* To);

	/**
	 * \brief Removes a directory.
	 * \param Folder Directory path.
	 * \param Recursive If true, deletes content recursively.
	 */
	static bool RemoveFolder(const char* Folder, const bool Recursive = true);

	/** \brief Deletes a file. */
	static bool RemoveFile(const char* FilePn);

	/** \brief Checks if the path is writable. */
	static bool CheckIfPathWritable(const char* path);

	/** \brief Returns number of free megabytes on the disk containing this program. */
	static float GetDiskFreeSpace();

	/**
	 * \brief Returns true if file exists.
	 * \param filename The filename. Relative and absolute paths are supported. Documents folder placement taken into account. Checked in both install and documents folders.
	 * \return true if file exists.
	 */
	static bool FileExists(const char* filename);

	/**
	 * \brief Copy file from Src to Dst.
	 * \param Src The source file.
	 * \param Dst The destination file. If file is in install folder, it will be copied to the documents folder.
	 * \param overwrite If true, overwrites existing file.
	 * \return true if file copied successfully.
	 */
	static bool FileCopy(const char* Src, const char* Dst, bool overwrite = true);

	/**
	 * \brief Copy folder from Src to Dst.
	 * \param Src The source folder.
	 * \param Dst The destination folder.
	 * \param overwrite If true, existing files will be overwritten.
	 * \param progress The progress callback (current file number, total files).
	 * \return true if folder copied successfully.
	 */
	static bool CopyFolder(const char* Src, const char* Dst, bool overwrite = true, std::function<void(int, int)> progress = nullptr);

	/**
	 * \brief Returns serial number of bootable hard disk.
	 * Examples: MacOS "6RY57LG3", Windows "1490395c", Linux "49cb4cde".
	 * \return Serial number or empty string on failure.
	 */
	static const cStr GetDiskSerialNumber();

	/**
	 * \brief Returns primary MAC address.
	 * Format: "xx:xx:xx:xx:xx:xx".
	 * \return MAC address or empty string on failure.
	 */
	static const cStr GetMACAddress();

	static void ShowString(const cStr* OptionalInit = nullptr, int dx = 0, int dy = 0, bool center_x = true, bool center_y = true);
	/** \brief Returns "true" when the string is visible. */
	static bool GetString(cStr* S);
	static bool InputString(const int X, const int Y, cStr* S, const int EditWidthPixels = 100);
	static bool InputString(cStr* S) {
		const int X = (int)cInput::GetMousePosition().x;
		const int Y = (int)cInput::GetMousePosition().y;
		return InputString(X, Y, S);
	}
	static void StopStringInput();
	static int GetInputPixelsHeight();
	static int GetInputPixelsWidth();
	static bool ReturnPressed();

#ifndef PY_PARSER
	struct _InputStringColors {
		bool Override;
		cColor Text;
		cColor Back;
		_InputStringColors() {
			Override = false;
			Text = cColor::White;
			Back = cColor::Black;
		}
	};
	static _InputStringColors InputStringColors;
#endif
	
	/** \brief Deprecated. Use "cIO::GetDocumentsFolder". */
	static const cStr GetUserFolder();

	/** \brief Returns cache folder path. Could be empty. */
	static const cStr GetCacheFolder();

	/** \brief Returns value of environment variable or empty string if the variable is not defined. */
	static const cStr GetEnvironmentVariable(const char* VarName);
};

#ifdef COMMS_WINDOWS
#define WCHAR_MAX_PATH (MAX_PATH*2)
/**
 * \brief Helper class to speed up converting files paths to wchar_t* (Windows specific).
 */
class APICALL wchar_path {
public:
	wchar_t path[WCHAR_MAX_PATH];
	wchar_path(const char* path);
	wchar_path(const wchar_t* path);
	wchar_path(const ::std::wstring& path);
	wchar_path();
	operator const wchar_t* () const;
	operator wchar_t* ();
	cStr toStr();
	int maxlen();
	int length();
};
#endif // Windows


