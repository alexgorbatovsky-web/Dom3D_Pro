#pragma once

#ifdef COMMS_WINDOWS

namespace cWin32 {
	//-------------------------------------------------------------------------
	// Dib24_Material
	//-------------------------------------------------------------------------
	class Dib24_Material {
	public:
		Dib24_Material();
		Dib24_Material(const char *Name, const cColor &Ambient, const cColor &Diffuse, const cColor &Specular, const float Shininess);
		
		cStr Name;
		cColor Ambient;
		cColor Diffuse;
		cColor Specular;
		float Shininess;
		
		static const Dib24_Material Brass;
		static const Dib24_Material Bronze;
		static const Dib24_Material PolishedBronze;
		static const Dib24_Material Chrome;
		static const Dib24_Material Copper;
		static const Dib24_Material PolishedCopper;
		static const Dib24_Material Gold;
		static const Dib24_Material PolishedGold;
		static const Dib24_Material Pewter;
		static const Dib24_Material Silver;
		static const Dib24_Material PolishedSilver;
		static const Dib24_Material Emerald;
		static const Dib24_Material Jade;
		static const Dib24_Material Obsidian;
		static const Dib24_Material Pearl;
		static const Dib24_Material Ruby;
		static const Dib24_Material Turquoise;
		static const Dib24_Material BlackPlastic;
		static const Dib24_Material BlackRubber;
	}; // Dib24_Material
	
	//-------------------------------------------------------------------------
	// Dib24
	//-------------------------------------------------------------------------
	class Dib24 {
	public:
		Dib24() {
			m_pBits = nullptr;
			Free();
		}
		Dib24(LONG Lx, LONG Ly) {
			m_pBits = nullptr;
			Init(Lx, Ly);
		}
		~Dib24() {
			Free();
		}
		void Free() {
			memset(&m_bmi, 0, sizeof(BITMAPINFO));
			if(m_pBits != nullptr) {
				delete[] m_pBits;
				m_pBits = nullptr;
			}
		}
		HBITMAP Init(HDC hDC, const int Width, const int Height);
		void Init(LONG Lx, LONG Ly);
		
		struct Pixel {
			BYTE r, g, b;

			Pixel();
			Pixel(BYTE R, BYTE G, BYTE B);
			DWORD ToBgr() const;
			DWORD ToRgb() const;
			cColor ToColor() const;
			static Pixel FromBgr(DWORD);
			static Pixel FromRgb(DWORD);
			static Pixel FromColor(const cColor &);
		};

		void SetPixel(int x, int y, const Dib24::Pixel &p);
		Dib24::Pixel GetPixel(int x, int y) const;
		Dib24::Pixel GetPixel(const float s, const float t) const;
		
		void SetPixel(int x, int y, const Dib24::Pixel &p, float z, float *pZBuffer);
		void DrawLineHorFlat(int x0, int x1, int y, float z0, float z1, const Dib24::Pixel &p, float *pZBuffer);
		void DrawPolygonFlat(const cVec3 *pVerts, const int Num, const Dib24::Pixel &p, float *pZBuffer);

		static const cColor CalcColor(const Dib24_Material &Mt, const float Dot);
		void DrawLineHorGouraud(int x0, int x1, int y, float z0, float z1, float d0, float d1, const Dib24_Material &Mt, float *pZBuffer);
		void DrawPolygonGouraud(const cVec3 *pVerts, const float *pDots, const int Num, const Dib24_Material &Mt, float *pZBuffer);
		
		Dib24::Pixel GetBlot(int x, int y, int nSize) const;
		void Tri(const cVec2 &t0, const cVec2 &t1, const cVec2 &t2, const Dib24::Pixel &p);
		void Quad(const cVec2 &ul, const float d, const Dib24::Pixel &p);
		void BlockTri(const cVec2 &t0, const cVec2 &t1, const cVec2 &t2, const Dib24::Pixel &p);
		void ZTri(const cVec3 &t0, const cVec3 &t1, const cVec3 &t2, const Dib24::Pixel &p, int *pZBuffer);
		void Line(int x0, int y0, int x1, int y1, const Dib24::Pixel &p);
		void DrawLine(const cVec2 &p0, const cVec2 &p1, const Dib24::Pixel &p);
		void Char(int c, int x, int y, COLORREF Color);
		int GetStrWidth(const cStr &Str) const;
		int GetStrHeight() const;
		void Str(const cStr &Str, int x, int y);
		void StrCenter(const cStr &Str, int cx, int cy);
		void Circle(int cx, int cy, int R, const cColor &C);
		void Round(int xCenter, int yCenter, int Radius, const cColor &Color);
		void Sphere(int cx, int cy, int R, const cColor &C);
		void Clear(BYTE GreyLevel);
		void Clear(const Dib24::Pixel &p);
		void ToDC(HDC hDC) const {
			SetDIBitsToDevice(hDC, 0, 0, m_bmi.bmiHeader.biWidth, m_bmi.bmiHeader.biHeight,
				0, 0, 0, m_bmi.bmiHeader.biHeight, m_pBits, &m_bmi, DIB_RGB_COLORS);
		}
		void ConvertFrom32To24();
		bool LoadBMP(const char *pFileName);
		bool SaveBMP(const char *pFileName);
		void Scale(int s);
		
		int GetWidth() const;
		int GetHeight() const;
		
		Dib24 & Copy(const Dib24 &Src);
		Dib24 & Paste(int WhereX, int WhereY, const Dib24 &Src);

		void Sharp(const float Sharpness);
	private:
		BITMAPINFO m_bmi;
		DWORD m_nBytesPerScanLine;
		BYTE *m_pBits;
		HBITMAP m_hBitmap;
	}; // Dib24
	
	// Dib24::Pixel.ctor : ()
	inline Dib24::Pixel::Pixel() {
	}

	// Dib24::Pixel.ctor : (BYTE, BYTE, BYTE)
	inline Dib24::Pixel::Pixel(BYTE R, BYTE G, BYTE B)
	: r(R), g(G), b(B) {}

	// Dib24::Pixel::ToBgr : DWORD() const
	inline DWORD Dib24::Pixel::ToBgr() const {
		return b | (WORD)g << 8 | (DWORD)r << 16;
	}

	// Dib24::Pixel::ToRgb : DWORD() const
	inline DWORD Dib24::Pixel::ToRgb() const {
		return r | (WORD)g << 8 | (DWORD)b << 16;
	}

	// Dib24::Pixel::ToColor : cColor() const
	inline cColor Dib24::Pixel::ToColor() const {
		return cColor(r, g, b);
	}

	// Dib24::Pixel::FromBgr : Dib24::Pixel(DWORD)
	inline Dib24::Pixel Dib24::Pixel::FromBgr(DWORD Bgr) {
		return Pixel((BYTE)(Bgr >> 16), (BYTE)(Bgr >> 8), (BYTE)(Bgr));
	}

	// Dib24::Pixel::FromRgb : Dib24::Pixel(DWORD)
	inline Dib24::Pixel Dib24::Pixel::FromRgb(DWORD Rgb) {
		return Pixel((BYTE)Rgb, (BYTE)(Rgb >> 8), (BYTE)(Rgb >> 16));
	}

	// Dib24::Pixel::FromColor : Dib24::Pixel(const cColor &)
	inline Dib24::Pixel Dib24::Pixel::FromColor(const cColor &Color) {
		return Pixel((BYTE)cColor::FloatToDword(Color.r), (BYTE)cColor::FloatToDword(Color.g), (BYTE)cColor::FloatToDword(Color.b));
	}

	// Dib24::GetWidth : int() const
	inline int Dib24::GetWidth() const {
		return m_bmi.bmiHeader.biWidth;
	}

	// Dib24::GetHeight : int() const
	inline int Dib24::GetHeight() const {
		return m_bmi.bmiHeader.biHeight;
	}

	// Dib24::SetPixel
	inline void Dib24::SetPixel(int x, int y, const Dib24::Pixel &p) {
		if(x < 0 || x >= m_bmi.bmiHeader.biWidth || y < 0 || y >= m_bmi.bmiHeader.biHeight) return;
		long index = m_nBytesPerScanLine * (m_bmi.bmiHeader.biHeight - y - 1) + 3 * x;
		m_pBits[index] = p.b;
		m_pBits[index + 1] = p.g;
		m_pBits[index + 2] = p.r;
	}

	inline void Dib24::SetPixel(int x, int y, const Dib24::Pixel &p, float z, float *pZBuffer) {
		if(x < 0 || x >= m_bmi.bmiHeader.biWidth || y < 0 || y >= m_bmi.bmiHeader.biHeight) {
			return;
		}
		long zindex = (long)y * m_bmi.bmiHeader.biWidth + (long)x;
		if(z <= pZBuffer[zindex]) {
			pZBuffer[zindex] = z;
			long index = m_nBytesPerScanLine * (m_bmi.bmiHeader.biHeight - y - 1) + 3 * x;
			m_pBits[index] = p.b;
			m_pBits[index + 1] = p.g;
			m_pBits[index + 2] = p.r;
		}
	}

	//-----------------------------------------------------------------------------
	// Dib24::GetPixel : DWORD(int, int) const
	//-----------------------------------------------------------------------------
	inline Dib24::Pixel Dib24::GetPixel(int x, int y) const {
		Pixel p;
		if(x < 0 || x >= m_bmi.bmiHeader.biWidth || y < 0 || y >= m_bmi.bmiHeader.biHeight) {
			p.r = p.g = p.b = 0;
			return p;
		}
		long index = m_nBytesPerScanLine * (m_bmi.bmiHeader.biHeight - y - 1) + 3 * x;
		p.b = m_pBits[index];
		p.g = m_pBits[index + 1];
		p.r = m_pBits[index + 2];
		return p;
	}

	//-----------------------------------------------------------------------------
	// Dib24::GetBlot : DWORD(int, int, int) const
	//-----------------------------------------------------------------------------
	inline Dib24::Pixel Dib24::GetBlot(int x, int y, int nSize) const {
		cColor Clr(cColor::Black);
		int X, Y, n = 0;
		for(int dy = 0; dy < nSize; dy++) {
			for(int dx = 0; dx < nSize; dx++) {
				X = x + dx;
				Y = y + dy;
				if(X < GetWidth() && Y < GetHeight()) {
					Clr += GetPixel(X, Y).ToColor();
					n++;
				}
			}
		}
		if(n) {
			Clr /= (float)n;
		}
		return Dib24::Pixel::FromColor(Clr);
	}
	
	// Dib24::Clear
	inline void Dib24::Clear(BYTE GreyLevel) {
		memset(m_pBits, GreyLevel, m_bmi.bmiHeader.biSizeImage);
	}

	void OnSizing(HWND hWnd, WPARAM wParam, LPARAM lParam, int MinClientLx = 320, int MinClientLy = 200);
	const POINT CalcPointToCenterWindow(HWND hChild, HWND hParent);
	BOOL CenterWindow(HWND hChild, HWND hParent);
	PBITMAPINFO CreateBitmapInfo(HBITMAP hBmp);
	bool SaveBitmap(const char *FileName, HBITMAP hBmp, HDC hdc);
	bool GenFont();
	
	static UINT_PTR CALLBACK CenterHookProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam); // Hook for centering system dialogs.
	
	struct ExecArgs {
		const char *pVerb; 	// "edit", "explore", "find", "open", "print", "properties"
		const char *pFileName;
		const char *pParameters;
		const char *pDirectory;
		int nShow;
		bool fWait;
		DWORD TimeOutMs;
	};
	int Exec(const ExecArgs &Args);
	
	//*****************************************************************************
	// DlgTemplate
	//*****************************************************************************
	class DlgTemplate {
	public:
		DlgTemplate() {}
		DlgTemplate(LPCSTR Title, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, LPCSTR FontName = nullptr, WORD FontSize = 8) {
			Create(Title, Style, ExStyle, xPos, yPos, Width, Height, FontName, FontSize);
		}
		void Create(LPCSTR Title, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, LPCSTR FontName = nullptr, WORD FontSize = 8);

		void Free() { m_DlgTemplate.Free(); }
		void AddControl(LPCSTR ClassName, LPCSTR Caption, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, WORD id);
		
		// Helpers:
		void AddButton(LPCSTR Caption, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, WORD id) {
			AddControl("Button" /* 0x0080 */, Caption, Style, ExStyle, xPos, yPos, Width, Height, id);
		}
		void AddEditBox(LPCSTR Caption, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, WORD id) {
			AddControl("Edit" /* 0x0081 */, Caption, Style, ExStyle, xPos, yPos, Width, Height, id);
		}

		void AddStatic(LPCSTR Caption, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, WORD id) {
			AddControl("Static" /* 0x0082 */, Caption, Style, ExStyle, xPos, yPos, Width, Height, id);
		}

		void AddListBox(LPCSTR Caption, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, WORD id) {
			AddControl("ListBox" /* 0x0083 */, Caption, Style, ExStyle, xPos, yPos, Width, Height, id);
		}
		void AddScrollBar(LPCSTR Caption, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, WORD id) {
			AddControl("ScrollBar" /* 0x0084 */, Caption, Style, ExStyle, xPos, yPos, Width, Height, id);
		}
		void AddComboBox(LPCSTR Caption, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, WORD id) {
			AddControl("ComboBox" /* 0x0085 */, Caption, Style, ExStyle, xPos, yPos, Width, Height, id);
		}
		
		const DLGTEMPLATE * ToDlgTemplatePtr() const { return reinterpret_cast<const DLGTEMPLATE *>(m_DlgTemplate.ToPtr()); }
	private:
		cList<char> m_DlgTemplate;
		void AppendData(const void *Src, int Size);
		void AppendStr(LPCSTR Str);
		void AlignDlgTemplateToDWORD();
	}; // DlgTemplate

	struct Folders {
		enum Enum {
			SendTo = 0x0009, // CSIDL_SENDTO, Send To menu items
			Desktop = 0x0000, // CSIDL_DESKTOP, Desktop of current user
			CommonDesktop = 0x0019, // CSIDL_COMMON_DESKTOPDIRECTORY, Desktop for all users
			Startup = 0x0007, // CSIDL_STARTUP, Startup program group for current user
			CommonStartup = 0x0018, // CSIDL_COMMON_STARTUP, Startup folder for all users
			StartMenu = 0x000b, // CSIDL_STARTMENU, Start menu items for current user
			CommonStartMenu = 0x0016, // CSIDL_COMMON_STARTMENU, Start menu for all users
			ProgramFiles = 0x0026, // CSIDL_PROGRAM_FILES, The Program Files folder
			Programs = 0x0002, // CSIDL_PROGRAMS, Program groups for current user
			CommonPrograms = 0x0017, // CSIDL_COMMON_PROGRAMS, Program groups on the Start menu for all users
			Windows = 1000, // The Windows directory
			System = 1001 // The Windows System folder
		};
	};
	const cStr GetSpecialFolder(const Folders::Enum Folder);
	const cStr GetShortcutTarget(const char *Shortcut);
	bool CreateShortcut(const char *Shortcut, const char *Target);
	
	//*****************************************************************************
	// _FileDialog
	//*****************************************************************************
	class _FileDialog {
	public:
		static bool IsVisible() { // Returns "true" if one of the file dialogs is currently shown
			return s_Visible;
		}

		_FileDialog(HWND hParentWnd = nullptr, const char *InitialPath = nullptr, const char *Title = nullptr, const bool Load = true, const bool SimpleMode = false);
		void SetParent(void *Parent);
		
		// Relative path must start from '/' or '\\'.
		// If nullptr, exe - file folder will be used.
		void SetInitialPath(const char *InitialPath = nullptr);
		void SetTitle(const char *Title);
					
		const cStr & GetInitialPath() const;
		BOOL DoModal(const char *InitialFileName = nullptr); // Requires initial path
		const char * GetFilePathName() { return m_FilePathName; }
		void SetFilterIndex(const int Index);
	protected:
		virtual bool OnLoadFileForPreview(const char *FileName, cStr &FileInfo) = 0;
		virtual void OnUnloadFileForPreview() = 0;
		virtual void OnRenderPreview(Dib24 &dib, int xShift, int yShift) = 0;
		void SetFilter(const char *FileExtensions);
		bool m_LoadDialog; // Set to "false" to switch from "Load Dialog" to "Save Dialog"
		bool m_ResetShiftOnSelChange;

		static bool s_Visible;
		bool m_AnimationMode;
		bool m_NoPreviewMode;
		bool m_SimpleMode;

		static void DrawNoPreview(Dib24 &dib);
	private:
		void Init();

		struct {
			DWORD			lStructSize;
			HWND			hwndOwner;
			HINSTANCE		hInstance;
			LPCTSTR			lpstrFilter;
			LPTSTR			lpstrCustomFilter;
			DWORD			nMaxCustFilter;
			DWORD			nFilterIndex;
			LPTSTR			lpstrFile;
			DWORD			nMaxFile;
			LPTSTR			lpstrFileTitle;
			DWORD			nMaxFileTitle;
			LPCTSTR			lpstrInitialDir;
			LPCTSTR			lpstrTitle;
			DWORD			Flags;
			WORD			nFileOffset;
			WORD			nFileExtension;
			LPCTSTR			lpstrDefExt;
			LPARAM			lCustData;
			LPOFNHOOKPROC	lpfnHook;
			LPCTSTR			lpTemplateName;
			// _WIN32_WINNT >= 0x0500
			void			*pvReserved;
			DWORD			dwReserved;
			DWORD			FlagsEx;
		} m_ofn;
		cStr m_Title;
		cStr m_Filter;
		cList<cStr> m_FileExtensions;
		cStr m_FilePathName, m_InitialDir;
		char m_Buffer[MAX_PATH];
		cWin32::DlgTemplate m_PreviewDlgTemplate;
		HWND m_hDlg, m_hViewPort, m_hTitle, m_hFileNameCombo, m_hInfo;
		Dib24 m_dib;
		bool m_LoadIsClicked;
		bool m_UpdatePreview;
		int m_ShiftX, m_ShiftY;
		bool m_DrawNoPreview;
		
		enum Constants {
			WM_SETOWNER = WM_USER + 1,
			idViewPort = 1010,
			idTitle = 1011,
			idInfo = 1012
		};
		
		static UINT_PTR CALLBACK HookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
		static void AdjustLayout(HWND hDlg);
		static BOOL CALLBACK FindCtrls(HWND hWnd, LPARAM lParam);
		static bool FileExist(const char *FileName);
		bool UpdatePreview();
		static UINT_PTR CALLBACK ParentDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
		DLGPROC m_PrevDlgProc;
	};

	//*************************************************************************
	// ImageFileDialog
	//*************************************************************************
	class ImageFileDialog : public _FileDialog {
	public:
		// Set "Load" to "false" to switch from "Load Dialog" to "Save Dialog"
		ImageFileDialog(const bool Load, const char *InitialPath = nullptr);
	protected:
		virtual bool OnLoadFileForPreview(const char *FileName, cStr &FileInfo);
		virtual void OnUnloadFileForPreview();
		virtual void OnRenderPreview(Dib24 &dib, int xShift, int yShift);
		Dib24 m_Image;
	};

	//*************************************************************************
	// XmlFileDialog
	//*************************************************************************
	class XmlFileDialog : public _FileDialog {
	public:
		// Set "Load" to "false" to switch from "Load Dialog" to "Save Dialog"
		XmlFileDialog(const bool Load, const char *InitialPath = nullptr);
	protected:
		virtual bool OnLoadFileForPreview(const char *FilePn, cStr &FileInfo);
		virtual void OnUnloadFileForPreview();
		virtual void OnRenderPreview(Dib24 &dib, int xShift, int yShift);
		cList<cStr> m_Lines;
	};

	// UnknownFileDialog
	class UnknownFileDialog : public _FileDialog {
	public:
		UnknownFileDialog(const bool Load, const char *Title, const cList<cStr> &Extensions, const char *InitialPath = nullptr);
	protected:
		virtual bool OnLoadFileForPreview(const char *FileName, cStr &FileInfo);
		virtual void OnUnloadFileForPreview();
		virtual void OnRenderPreview(Dib24 &dib, int xShift, int yShift);
	};
} // cWin32

#endif // COMMS_WINDOWS
