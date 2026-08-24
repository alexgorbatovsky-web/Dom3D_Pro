#include "comms.h"

#ifdef COMMS_WINDOWS

void cWinMain_SetOverrideParent(HWND Dialog);
HWND cWinMain_GetWindow();
#ifdef _WIN32
#pragma warning(disable: 4244)
#endif // _WIN32

namespace comms {
namespace cWin32 {

//-----------------------------------------------------------------------------
// Dib24::Init
//-----------------------------------------------------------------------------
void Dib24::Init(LONG Lx, LONG Ly) {
	Free();
	if(Lx <= 0 || Ly <= 0) {
		return;
	}

	// m_nBytesPerScanLine:
	m_nBytesPerScanLine = 3 * Lx;
	int mod = m_nBytesPerScanLine % 4;
	if(mod) m_nBytesPerScanLine += 4 - mod;
	// m_bmi:
	m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_bmi.bmiHeader.biWidth = Lx;
	m_bmi.bmiHeader.biHeight = Ly;
	m_bmi.bmiHeader.biPlanes = 1;
	m_bmi.bmiHeader.biBitCount = 24;
	m_bmi.bmiHeader.biCompression = BI_RGB;
	m_bmi.bmiHeader.biSizeImage = m_nBytesPerScanLine * Ly;
	// m_pBits:
	m_pBits = new BYTE[m_bmi.bmiHeader.biSizeImage];
	cAssert(m_pBits);
} // Dib24::Init

//----------------------------------------------------------------------------------------
// OnSizing
//----------------------------------------------------------------------------------------
void OnSizing(HWND hWnd, WPARAM wParam, LPARAM lParam, int MinClientLx, int MinClientLy) {
	DWORD Style = GetWindowLong(hWnd, GWL_STYLE);
	DWORD ExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
	RECT rc;
	SetRect(&rc, 0, 0, MinClientLx, MinClientLy);
	AdjustWindowRectEx(&rc, Style, false, ExStyle);
	int MinLx = rc.right - rc.left, MinLy = rc.bottom - rc.top;

	LPRECT lprc = (LPRECT)lParam;
	if(!lprc) return;
	int Lx = lprc->right - lprc->left;
	int Ly = lprc->bottom - lprc->top;
	bool fTop = false, fBottom = false, fRight = false, fLeft = false;
	switch(wParam) {
		case WMSZ_TOP: fTop = true;
			break;
		case WMSZ_BOTTOM: fBottom = true;
			break;
		case WMSZ_RIGHT: fRight = true;
			break;
		case WMSZ_LEFT: fLeft = true;
			break;
		case WMSZ_BOTTOMLEFT: fBottom = fLeft = true;
			break;
		case WMSZ_BOTTOMRIGHT: fBottom = fRight = true;
			break;
		case WMSZ_TOPLEFT: fTop = fLeft = true;
			break;
		case WMSZ_TOPRIGHT: fTop = fRight = true;
			break;
		default:
			break;
	}
	if(Lx < MinLx) {
		if(fRight) lprc->right = lprc->left + MinLx;
		else if(fLeft) lprc->left = lprc->right - MinLx;
	}
	if(Ly < MinLy) {
		if(fTop) lprc->top = lprc->bottom - MinLy;
		else if(fBottom) lprc->bottom = lprc->top + MinLy;
	}
} // OnSizing

//-----------------------------------------------------------------------------
// cWin32::CalcPointToCenterWindow
//-----------------------------------------------------------------------------
const POINT cWin32::CalcPointToCenterWindow(HWND hChild, HWND hParent) {
	// Screen dimensions excluding task bar:
	RECT rcDesktop;
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDesktop, 0);

	// Child dimensions:
	RECT rcChild;
	::GetWindowRect(hChild, &rcChild);
	
	LONG cxChild = rcChild.right - rcChild.left, cyChild = rcChild.bottom - rcChild.top;

	// Parent window dimensions (could be desktop):
	RECT rcParent;
	if(hParent != nullptr) {
		::GetWindowRect(hParent, &rcParent);
	} else {
		rcParent = rcDesktop;
	}

	// Calc point to center the child on the parent window:
	POINT P;
	P.x = rcParent.left + ((rcParent.right - rcParent.left) - cxChild) / 2;
	P.y = rcParent.top + ((rcParent.bottom - rcParent.top) - cyChild) / 2;
	
	// Keep the child wholly on the desktop:
	P.x = cMath::Clamp(P.x, (LONG)0, rcDesktop.right - cxChild);
	P.y = cMath::Clamp(P.y, (LONG)0, rcDesktop.bottom - cyChild);

	return P;
} // cWin32::CalcPointToCenterWindow

//-----------------------------------------------------------------------------
// CenterWindow
//-----------------------------------------------------------------------------
BOOL CenterWindow(HWND hChild, HWND hParent) {
	const POINT C = CalcPointToCenterWindow(hChild, hParent);
	return SetWindowPos(hChild, nullptr, C.x, C.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
} // CenterWindow

//-----------------------------------------------------------------------------
// Exec
//-----------------------------------------------------------------------------
int Exec(const ExecArgs &Args) {
	SHELLEXECUTEINFO sei;
	memset(&sei, 0, sizeof(sei));
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = Args.pVerb;
	sei.lpFile = Args.pFileName;
	sei.lpParameters = Args.pParameters;
	sei.lpDirectory = Args.pDirectory;
	sei.nShow = Args.nShow;

	if(ShellExecuteEx(&sei)) {
		if(Args.fWait) {
			while(WaitForSingleObject(sei.hProcess, Args.TimeOutMs) == WAIT_TIMEOUT);
			DWORD ExitCode;
			if(GetExitCodeProcess(sei.hProcess, &ExitCode)) return 0;
			else return -1;
		} else return 0;
	} else return -1;
} // Exec

//-----------------------------------------------------------------------------
// cWin32::CreateBitmapInfo
//-----------------------------------------------------------------------------
PBITMAPINFO cWin32::CreateBitmapInfo(HBITMAP hBmp) {
	BITMAP bmp;
	PBITMAPINFO pbmi;
	WORD cClrBits;
	
	GetObjectA(hBmp, sizeof(BITMAP), (LPSTR)&bmp);
	cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
	if(cClrBits == 1) {
		cClrBits = 1;
	} else if(cClrBits <= 4) {
		cClrBits = 4;
	} else if(cClrBits <= 8) {
		cClrBits = 8;
	} else if(cClrBits <= 16) {
		cClrBits = 16;
	} else if(cClrBits <= 24) {
		cClrBits = 24;
	} else {
		cClrBits = 32;
	}
	
	if(cClrBits < 24) {
		dword One = (1 << cClrBits);
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * One);
	} else {
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR, sizeof(BITMAPINFOHEADER));
	}
	
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bmp.bmWidth;
	pbmi->bmiHeader.biHeight = bmp.bmHeight;
	pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
	pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
	if(cClrBits < 24) {
		pbmi->bmiHeader.biClrUsed = (1 << cClrBits);
	}
	
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits + 31) & ~31) / 8 * pbmi->bmiHeader.biHeight;
	pbmi->bmiHeader.biClrImportant = 0;
	return pbmi;
} // cWin32::CreateBitmapInfo

//-----------------------------------------------------------------------------
// cWin32::SaveBitmap
//-----------------------------------------------------------------------------
bool cWin32::SaveBitmap(const char *FilePn, HBITMAP hBmp, HDC hdc) {
	BITMAPFILEHEADER hdr;
	PBITMAPINFOHEADER pbih;
	LPBYTE lpBits;
	DWORD dwTotal;
	DWORD cb;
	BYTE *hp;
	
	PBITMAPINFO pbmi = CreateBitmapInfo(hBmp);
	pbih = (PBITMAPINFOHEADER) pbmi;
	lpBits = (LPBYTE)GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);
	
	GetDIBits(hdc, hBmp, 0, (WORD)pbih->biHeight, lpBits, pbmi, DIB_RGB_COLORS);
	
	hdr.bfType = 0x4d42;
	hdr.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed * sizeof(RGBQUAD) + pbih->biSizeImage);
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	
	hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed * sizeof (RGBQUAD);

	cFile File;
	File.WriteBytes(&hdr, sizeof(BITMAPFILEHEADER));
	File.WriteBytes(pbih, sizeof(BITMAPINFOHEADER) + pbih->biClrUsed * sizeof(RGBQUAD));

	dwTotal = cb = pbih->biSizeImage;
	
	hp = lpBits;
	File.WriteBytes(hp, cb);
	
	const bool Success = cIO::SaveFile(FilePn, File);
	GlobalFree((HGLOBAL)lpBits);
	return Success;
} // cWin32::SaveBitmap

//---------------------------------------------------------------------------------------------
// cWin32::CenterHookProc
//---------------------------------------------------------------------------------------------
UINT_PTR CALLBACK cWin32::CenterHookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
	if(Msg == WM_INITDIALOG) {
		HWND hParent = GetParent(hDlg);
		CenterWindow(hDlg, hParent);
		return TRUE;
	}
	return FALSE;
} // cWin32::CenterHookProc

//-----------------------------------------------------------------------------
// cWin32::GenFont
//-----------------------------------------------------------------------------
bool cWin32::GenFont() {
	// Required Unicode chars
	cBitMap Required;
	Required.SetBitCount(0xffff + 1);
	Required.ClearAll();
	Required.Set(0, 128); // ASCII
	cFile XmlFile;
	cStr XmlText;
	cList<word> UniChars;
	int i, c, f;
	cList<comms::cStr> Filenames;
	cList<comms::cStr> Ext;
	Ext.Add("XML");
	if(cIO::LoadFileDialog("Select one or several files with required characters", Ext, &Filenames, "GenFont")) {
		for(f = 0; f < Filenames.Count(); f++) {
			const cStr &Pn = Filenames[f];
			if(cIO::LoadFile(Pn.ToCharPtr(), &XmlFile)) {
				XmlText.SetLength((int)XmlFile.Size());
				XmlFile.ReadBytes(XmlText.ToNonConstCharPtr(), XmlText.Length());
				if(XmlText.DecodeUTF8(&UniChars)) {
					for(i = 0; i < UniChars.Count(); i++) {
						c = (int)UniChars[i];
						Required.Set(c);
					}
				}
			}
		}
	}
	// Choose font dialog
	LOGFONT lf;
	CHOOSEFONT cf;
	memset(&cf, 0, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.Flags = CF_SCREENFONTS | CF_FORCEFONTEXIST | CF_TTONLY | CF_ENABLEHOOK;
	cf.lpLogFont = &lf;
	cf.lpfnHook = (LPCFHOOKPROC)CenterHookProc;
	cf.hwndOwner = cWinMain_GetWindow();
	if(!ChooseFont(&cf)) {
		return false;
	}
	// Size of the texture will be selected from this list.
	// Generator supports non square textures, but they were omitted
	// from the list because iPad 2 supports only square textures.
	const int TexSize[][2] = {
		{ 64, 64 },
		{ 128, 64 },
		{ 128, 128 },
		{ 256, 128 },
		{ 256, 256 },
		{ 512, 256 },
		{ 512, 512 },
		{ 1024, 512 },
		{ 1024, 1024 },
		{ 2048, 1024 },
		{ 2048, 2048 },
		{ 0, 0 }
	};

	enum MODE {
		MODE_SELECTING_SIZE = 0,
		MODE_RENDERING_TEXTURE = 1
	};
	MODE Mode = MODE_SELECTING_SIZE;
	int CurSizeIndex = 0;

	const int TexOffsetX = 2; // X - offset for glyph black box from texture border or neighboring glyph black boxes.
	const int TexOffsetY = 2; // Y - offset for row of glyph black boxes from texture border of neighboring rows.
	const int Border = 1; // Border around each glyph. Will be added to glyph size, subtracted from glyph origin as well as on texture glyph position.

	// Create offscreen device context compatible with "DISPLAY"
	HDC hScreenDC = CreateDC("DISPLAY", nullptr, nullptr, nullptr);
	HDC hdc = CreateCompatibleDC(hScreenDC);
	
	// Create font object from selected attributes
	HFONT hf = CreateFontIndirect(&lf);
	if(nullptr == hf) {
		DeleteDC(hdc);
		DeleteDC(hScreenDC);
		return false;
	}
	hf = (HFONT)SelectObject(hdc, hf);

	// Get text metrics
	OUTLINETEXTMETRIC otm;
	GetOutlineTextMetrics(hdc, sizeof(otm), &otm);
	// Preparing identity transform matrix for glyph
	_FIXED One, Zero;
	One.value = 1, One.fract = 0;
	Zero.value = 0, Zero.fract = 0;
	MAT2 T;
	T.eM11 = T.eM22 = One;
	T.eM12 = T.eM21 = Zero;

	GLYPHMETRICS gm;
	DWORD cb;
	byte *Glyph = nullptr;
	int GlyphWidthBytes;
	int ix, iy;
	int j, n;
	int TexSizeX, TexSizeY;
	cVec2i TexPos;

	// Char ranges
	cList<byte> Temp;
	Temp.SetCount(GetFontUnicodeRanges(hdc, nullptr));
	GLYPHSET *GS = (GLYPHSET *)Temp.ToPtr();
	GetFontUnicodeRanges(hdc, GS);
	cBitMap Visible;
	Visible.SetBitCount(Required.GetBitCount());
	Visible.ClearAll();
	for(i = 0; i < (int)GS->cRanges; i++) {
		j = (int)GS->ranges[i].wcLow;
		n = (int)GS->ranges[i].cGlyphs;
		if(j >= Visible.GetBitCount()) {
			continue;
		}
		c = cMath::Min(n, Visible.GetBitCount() - j);
		Visible.Set(j, c);
	}

	// Zero font format
	cFont Font;
	cFont::Glyph t;
	for(i = 0; i < Visible.GetBitCount(); i++) {
		if(Visible[i] && Required[i]) {
			t.Char = i;
			Font.Glyphs.Add(t);
		}
	}
	Font.CellAscent = otm.otmTextMetrics.tmAscent;
	Font.CellDescent = otm.otmTextMetrics.tmDescent;
	Font.LineSpacing = Font.CellAscent + Font.CellDescent;
	Font.GlyphBorder = Border;

	// Grayscale bitmap
	BITMAPFILEHEADER FileHdr;
	BITMAPINFOHEADER InfoHdr;
	RGBQUAD Palette[256];
	cList<byte> Bits;
	int BitmapWidthBytes;
	
	bool Success = false;
	while(true) {
		TexSizeX = TexSize[CurSizeIndex][0];
		TexSizeY = TexSize[CurSizeIndex][1];
		
		cVec2i CurPos(0, TexOffsetY);
		int CurRowMaxGlyphSizeY = 0;
		
		// MODE_SELECTING_SIZE - only considering glyph size
		// MODE_RENDERING_TEXTURE - putting glyph to texture with enough size
		for(i = 0; i < Font.Glyphs.Count(); i++) {
			c = Font.Glyphs[i].Char;
			// Query glyph size
			cb = GetGlyphOutlineW(hdc, c, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &T);
			if(MODE_RENDERING_TEXTURE == Mode) {
				// Allocate buffer for glyph and get it
				Glyph = new byte[cb];
				GetGlyphOutlineW(hdc, c, GGO_GRAY8_BITMAP, &gm, cb, Glyph, &T);
			}

			// Put glyph info to font format
			if(MODE_RENDERING_TEXTURE == Mode) {
				cFont::Glyph &g = Font.Glyphs[i];
				g.SizeX = gm.gmBlackBoxX + 2 * Border;
				g.SizeY = gm.gmBlackBoxY + 2 * Border;
				g.Origin.Set(gm.gmptGlyphOrigin.x - Border, - gm.gmptGlyphOrigin.y - Border + Font.CellAscent);
				g.CellInc.Set(gm.gmCellIncX, gm.gmCellIncY);
			}

			// Is there space on texture with current size from current position for this glyph?
			if(CurPos[Xelt] + TexOffsetX + int(gm.gmBlackBoxX) + TexOffsetX >= TexSizeX) {
				// There is no space.
				// If that was the first char in row, we don't have enough space even for one this glyph!
				if(CurPos[Xelt] == 0) {
					break;
				}
				// Current row is out of space. We should go to the next row.
				CurPos[Xelt] = 0;
				CurPos[Yelt] += CurRowMaxGlyphSizeY + TexOffsetY;
				CurRowMaxGlyphSizeY = 0;
				// If we reach texture bottom, we should go with bigger texture.
				if(CurPos[Yelt] >= TexSizeY) {
					break;
				}
			}

			// Offset from texture border or previous glyph
			CurPos[Xelt] += TexOffsetX;
			
			// Copy glyph to texture
			if(MODE_RENDERING_TEXTURE == Mode) {
				// TexCoord
				TexPos = CurPos - cVec2i(Border);
				cFont::Glyph &g = Font.Glyphs[i];
				g.TexCoord.SetLeft(float(TexPos[Xelt]) / float(TexSizeX));
				g.TexCoord.SetTop(float(TexSizeY - TexPos[Yelt]) / float(TexSizeY));
				g.TexCoord.SetRight(float(TexPos[Xelt] + g.SizeX) / float(TexSizeX));
				g.TexCoord.SetBottom(float(TexSizeY - TexPos[Yelt] - g.SizeY) / float(TexSizeY));
				// Glyph bits
				GlyphWidthBytes = cMath::AlignToDword(gm.gmBlackBoxX);
				for(iy = 0; iy < int(gm.gmBlackBoxY); iy++) {
					for(ix = 0; ix < int(gm.gmBlackBoxX); ix++) {
						const int l = Glyph[iy * GlyphWidthBytes + ix];
						if(l > 0) {
							// Queried glyph contains 65 levels of gray (values range from 0 to 64)
							byte ls = byte(4 * l - 1);
							const int Index = (TexSizeY - 1 - (CurPos[Yelt] + iy)) * BitmapWidthBytes + CurPos[Xelt] + ix;
							Bits[Index] = (byte)ls;
						}
					}
				}
			}
			
			CurPos[Xelt] += gm.gmBlackBoxX;
			CurRowMaxGlyphSizeY = cMath::Max(CurRowMaxGlyphSizeY, int(gm.gmBlackBoxY));

			// Free glyph buffer
			if(MODE_RENDERING_TEXTURE == Mode) {
				delete Glyph;
				Glyph = nullptr;
			}
		}

		if(MODE_SELECTING_SIZE == Mode) {
			// If all glyphs are enumerated and there's space at texture bottom, we fit to current texture.
			if(i >= Font.Glyphs.Count() && CurPos[Yelt] + CurRowMaxGlyphSizeY + TexOffsetY < TexSizeY) {
				// Creating texture gray scale bitmap.
				// Bitmap info header
				memset(&InfoHdr, 0, sizeof(InfoHdr));
				InfoHdr.biSize = sizeof(InfoHdr);
				InfoHdr.biWidth = TexSizeX;
				InfoHdr.biHeight = TexSizeY;
				InfoHdr.biPlanes = 1;
				InfoHdr.biBitCount = 8;
				InfoHdr.biClrUsed = 256;
				InfoHdr.biCompression = BI_RGB;
				BitmapWidthBytes = cMath::AlignToDword(InfoHdr.biWidth);
				InfoHdr.biSizeImage = BitmapWidthBytes * InfoHdr.biHeight;
				
				// Gray scale palette
				memset(&Palette, 0, sizeof(Palette));
				for(i = 0; i < 256; i++) {
					Palette[i].rgbRed = Palette[i].rgbGreen = Palette[i].rgbBlue = (BYTE)i;
				}

				// Bitmap file header
				memset(&FileHdr, 0, sizeof(FileHdr));
				FileHdr.bfType = 'B' | 'M' << 8;
				FileHdr.bfOffBits = sizeof(FileHdr) + InfoHdr.biSize + sizeof(Palette);
				FileHdr.bfSize = FileHdr.bfOffBits + InfoHdr.biSizeImage;
				
				// Bitmap bits
				Bits.SetCount(InfoHdr.biSizeImage, 0);
				
				Mode = MODE_RENDERING_TEXTURE;
			} else { // We are out of space on this texture with this size.
				// Go to bigger texture size
				CurSizeIndex++;
				if(TexSize[CurSizeIndex][0] == 0) { // Size list is enumerated, but no one texture can fit glyphs.
					cLog::Warning("Can't generate font \"%s\" with such big size as \"%d\" pixels", lf.lfFaceName, cf.iPointSize / 10);
					break;
				}
			}
		} else {
			// Glyphs are rendered
			Success = true;

			// Forming font name
			cStr fn = cStr::Format("%s ", lf.lfFaceName);
			if(FW_BOLD == lf.lfWeight) {
				fn << "B";
			}
			if(lf.lfItalic) {
				fn << "I";
			}
			fn << cf.iPointSize / 10;

			// Save texture directly into "bmp" file
			cFile File;
			File.WriteBytes(&FileHdr, sizeof(FileHdr));
			File.WriteBytes(&InfoHdr, sizeof(InfoHdr));
			File.WriteBytes(Palette, sizeof(Palette));
			File.WriteBytes(Bits.ToPtr(), InfoHdr.biSizeImage);
			File.SetFilePn(cStr::Format("data/Fonts/%s.bmp", fn.ToCharPtr()));
			// Convert from "bmp" file into image.
			// Then from image into "png" file.
			cImageCodec *BMP = cIO::FindImageCodec("bmp"), *PNG = cIO::FindImageCodec("png");
			if ((BMP != nullptr) && (PNG != nullptr)) {
				cImage I;
				File.SetPos(0);
				if (BMP->Decode(File, &I)) {
					File.Clear();
					I.ToFormat(cFormat::R8); // Make 8-bit per pixel inside "png"
					if (PNG->Encode(I, &File)) {
						File.SetFilePn(cStr::Format("data/Fonts/%s.png", fn.ToCharPtr()));
					}
				}
			}
			// Save texture file
			cIO::CreatePath(File.GetFilePn());
			if(!cIO::SaveFile(nullptr, File)) {
				Success = false;
			}

			// Saving font format
			File.Clear();
			Font.Write(&File);
			if(!cIO::SaveFile(cStr::Format("data/Fonts/%s.cFont", fn.ToCharPtr()), File)) {
				Success = false;
			}

			if(Success) {
				cLog::Message("Generated font \"%s\"", fn.ToCharPtr());
			}
			
			break;
		}
	}

	// Free font object
	hf = (HFONT)SelectObject(hdc, hf);
	DeleteObject(hf);
	// Free DCs
	DeleteDC(hdc);
	DeleteDC(hScreenDC);

	return Success;
} // cWin32::GenFont

//*****************************************************************************
// DlgTemplate
//*****************************************************************************

//---------------------------------------------------------------------------------------------------------------------------------------------
// DlgTemplate::Create
//---------------------------------------------------------------------------------------------------------------------------------------------
void DlgTemplate::Create(LPCSTR Title, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, LPCSTR FontName, WORD FontSize) {
	Free();
	
	// Dialog template structure:
	DLGTEMPLATE dt;
	memset(&dt, 0, sizeof(dt));
	dt.style = Style;
	if(FontName != nullptr) {
		dt.style |= DS_SETFONT;
	}
	dt.dwExtendedStyle = ExStyle;
	dt.x = xPos;
	dt.y = yPos;
	dt.cx = Width;
	dt.cy = Height;
	AppendData(&dt, sizeof(dt));
	
	// Menu array:
	AppendData("\0", 2); // 0x0000 - no menu.

	// Class array:
	AppendData("\0", 2); // 0x0000 - predefined dialog box class.

	// Title array:
	AppendStr(Title); // Null - terminated Unicode title string.

	if(FontName != nullptr) {
		// 16 - bit point size value:
		AppendData(&FontSize, sizeof(WORD));
		// Typeface array:
		AppendStr(FontName); // Null - terminated Unicode string with the name of the typeface for the font.
	}
} // DlgTemplate::Create

//----------------------------------------------------------------------------------------------------------------------------------------------
// DlgTemplate::AddControl
//----------------------------------------------------------------------------------------------------------------------------------------------
void DlgTemplate::AddControl(LPCSTR ClassName, LPCSTR Caption, DWORD Style, DWORD ExStyle, int xPos, int yPos, int Width, int Height, WORD id) {
	AlignDlgTemplateToDWORD(); // Align DLGITEMTEMPLATE on DWORD boundary.

	// Dialog item template structure:
	DLGITEMTEMPLATE dit;
	memset(&dit, 0, sizeof(dit));
	dit.style = Style | WS_CHILD;
	dit.dwExtendedStyle = ExStyle;
	dit.x = xPos;
	dit.y = yPos;
	dit.cx = Width;
	dit.cy = Height;
	dit.id = id;
	AppendData(&dit, sizeof(dit));
	
	// Class array:
	AppendStr(ClassName); // Null - terminated Unicode string with the name of a registered window class.

	// Title array:
	AppendStr(Caption); // Null - terminated Unicode caption string.

	// Creation data array:
	AppendData("\0", 2); // 0x0000 - no creation data.

	((DLGTEMPLATE *)m_DlgTemplate.ToPtr())->cdit++;
} // DlgTemplate::AddControl

//-----------------------------------------------------------------------------
// DlgTemplate::AppendData
//-----------------------------------------------------------------------------
void DlgTemplate::AppendData(const void *Src, int Size) {
	int index = m_DlgTemplate.Count();
	m_DlgTemplate.Add(0, Size);
	memcpy(&m_DlgTemplate[index], Src, Size);
} // DlgTemplate::AppendData

//-----------------------------------------------------------------------------
// DlgTemplate::AppendStr
//-----------------------------------------------------------------------------
void DlgTemplate::AppendStr(LPCSTR Str) {
	int Len = MultiByteToWideChar(CP_ACP, 0, Str, -1, nullptr, 0);
	if(Len > 0) {
		WCHAR *WideStr = new WCHAR[Len];
		MultiByteToWideChar(CP_ACP, 0, Str, -1, WideStr, Len);
		AppendData(WideStr, Len * sizeof(WCHAR));
		delete WideStr;
	}
} // DlgTemplate::AppendStr

//-----------------------------------------------------------------------------
// DlgTemplate::AlignDlgTemplateToDWORD
//-----------------------------------------------------------------------------
void DlgTemplate::AlignDlgTemplateToDWORD() {
	const int PaddingSize = m_DlgTemplate.Count() % sizeof(DWORD);
	if(PaddingSize) {
		m_DlgTemplate.Add(0, PaddingSize);
	}
} // DlgTemplate::AlignDlgTemplateToDWORD

//-----------------------------------------------------------------------------
// GetSpecialFolder
//-----------------------------------------------------------------------------
const cStr GetSpecialFolder(const Folders::Enum Folder) {
	cStr Path;

	// Windows
	if(Folders::Windows == Folder) {
		Path.SetLength(MAX_PATH);
		GetWindowsDirectory(Path.ToNonConstCharPtr(), Path.Length());
		Path.CalcLength();
		return Path;
	}

	// System
	if(Folders::System == Folder) {
		Path.SetLength(MAX_PATH);
		GetSystemDirectory(Path.ToNonConstCharPtr(), Path.Length());
		Path.CalcLength();
		return Path;
	}

	HRESULT hr;
	LPITEMIDLIST pidl;
	
	// Pointer to an item ID list that represents the path of a special folder
	hr = SHGetSpecialFolderLocation(nullptr, (int)Folder, &pidl);
	if(SUCCEEDED(hr)) {
		// Convert the item ID list's binary representation into a file system path
		Path.SetLength(MAX_PATH);
		Path.Fill('\0');
		if(SHGetPathFromIDList(pidl, Path.ToNonConstCharPtr())) {
			Path.CalcLength();
			// We should free pointer to the item ID list with the Shell's IMalloc interface
			LPMALLOC pm;
			hr = SHGetMalloc(&pm);
			if(SUCCEEDED(hr)) {
				pm->Free(pidl);
				pm->Release();
			}
		} else {
			Path.Clear();
		}
	}
	return Path;
} // GetSpecialFolder

//-----------------------------------------------------------------------------
// CreateShortcut
//-----------------------------------------------------------------------------
bool CreateShortcut(const char *Shortcut, const char *Target) {
	HRESULT hr;
	IShellLink *sl;
	IPersistFile *pf;
	bool Success = false;

	// Init OLE
	hr = CoInitialize(nullptr);
	if(SUCCEEDED(hr)) {
		// Instance of IShellLink
		hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&sl);
		if(SUCCEEDED(hr)) {
			// Instance of IPersistFile
			hr = sl->QueryInterface(IID_IPersistFile, (void **)&pf);
			if(SUCCEEDED(hr)) {
				// Creating shortcut
				cStr L(Shortcut);
				L.SetFileDefaultExtension("lnk");
				hr = sl->SetPath(Target);
				if(SUCCEEDED(hr)) {
					WCHAR wc[MAX_PATH];
					MultiByteToWideChar(CP_ACP, 0, L.ToCharPtr(), -1, wc, MAX_PATH);
					hr = pf->Save(wc, TRUE);
					if(SUCCEEDED(hr)) {
						Success = true;
					}
				}
				pf->Release();
			}
			sl->Release();
		}
	}
	return Success;
} // CreateShortcut

//-----------------------------------------------------------------------------
// GetShortcutTarget
//-----------------------------------------------------------------------------
const cStr GetShortcutTarget(const char *Shortcut) {
	HRESULT hr;
	IShellLink *sl;
	IPersistFile *pf;
	cStr Target;

	// Init OLE
	hr = CoInitialize(nullptr);
	if(SUCCEEDED(hr)) {
		// Instance of IShellLink
		hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&sl);
		if(SUCCEEDED(hr)) {
			// Instance of IPersistFile
			hr = sl->QueryInterface(IID_IPersistFile, (void **)&pf);
			if(SUCCEEDED(hr)) {
				// Load shortcut
				WCHAR wc[MAX_PATH];
				MultiByteToWideChar(CP_ACP, 0, Shortcut, -1, wc, MAX_PATH);
				hr = pf->Load(wc, STGM_READ);
				if(SUCCEEDED(hr)) {
					// Resolve shortcut
					hr = sl->Resolve(nullptr, SLR_NO_UI | SLR_ANY_MATCH | SLR_NOUPDATE | 0x10 | 0x20 | 0x40); // SLR_NOSEARCH | SLR_NOTRACK | SLR_NOLINKINFO
					if(SUCCEEDED(hr)) {
						Target.SetLength(MAX_PATH);
						Target.Fill(' ');
						hr = sl->GetPath(Target.ToNonConstCharPtr(), Target.Length(), nullptr, SLGP_RAWPATH);
						if(SUCCEEDED(hr)) {
							Target.CalcLength();
						} else {
							Target.Clear();
						}
					}
				}
				pf->Release();
			}
			sl->Release();
		}
	}

	return Target;
} // GetShortcutTarget

//*****************************************************************************
// _FileDialog
//*****************************************************************************

//---------------------------------------------------------------------------------------------------------------------------
// _FileDialog.ctor
//---------------------------------------------------------------------------------------------------------------------------
_FileDialog::_FileDialog(HWND hParentWnd, const char *InitialPath, const char *Title, const bool Load, const bool SimpleMode) {
	m_SimpleMode = SimpleMode;
	m_LoadDialog = Load;
	Init();
	SetParent(hParentWnd);
	SetInitialPath(InitialPath);
	m_ofn.lpstrTitle = Title;
	m_AnimationMode = false;
	m_NoPreviewMode = false;
} // FileDialog.ctor

// FileDialog::SetTitle
void _FileDialog::SetTitle(const char *Title) {
	m_Title = Title;
	m_ofn.lpstrTitle = m_Title.ToCharPtr();
}

//-----------------------------------------------------------------------------
// FileDialog::Init
//-----------------------------------------------------------------------------
void _FileDialog::Init() {
	cAssert(sizeof(m_ofn) == sizeof(OPENFILENAME));
	memset(&m_ofn, 0, sizeof(m_ofn));
	m_ofn.lStructSize = sizeof(m_ofn);
	if(!m_SimpleMode) {
		m_ofn.FlagsEx = 0x00000001; // OFN_EX_NOPLACESBAR
	}

	SetParent(nullptr);

	m_Buffer[0] = '\0';
	m_ofn.lpstrFile = m_Buffer;
	m_ofn.nMaxFile = MAX_PATH;

	SetInitialPath(nullptr);
	
	m_ofn.lpstrTitle = nullptr;
	m_ofn.lpfnHook = (LPOFNHOOKPROC)HookProc;
	m_ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLEHOOK | OFN_EXPLORER | OFN_ENABLESIZING;
	if(m_LoadDialog) {
		m_ofn.Flags |= OFN_FILEMUSTEXIST;
	} else {
		m_ofn.Flags |= OFN_OVERWRITEPROMPT;
	}
	
	if(!m_SimpleMode) {
		// Creating preview dialog template
		m_PreviewDlgTemplate.Create("", DS_3DLOOK | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | DS_SETFONT | DS_FIXEDSYS, WS_EX_CONTROLPARENT, 0, 0, 215, 130, "MS Shell Dlg", 8);
		m_PreviewDlgTemplate.AddStatic("", WS_VISIBLE | WS_CHILDWINDOW | SS_CENTER, 0, 0, 5, 116, 10, idTitle);
		m_PreviewDlgTemplate.AddStatic("", SS_OWNERDRAW | WS_VISIBLE, 0, 4, 18, 110, 100, idViewPort);
		m_PreviewDlgTemplate.AddStatic("", WS_VISIBLE | WS_CHILDWINDOW | SS_CENTER, 0, 4, 122, 110, 46, idInfo);
		m_PreviewDlgTemplate.AddStatic("", WS_VISIBLE, 0, 116, 0, 99, 130, stc32);
		
		m_ofn.Flags |= OFN_ENABLETEMPLATEHANDLE;
		m_ofn.hInstance = (HINSTANCE)m_PreviewDlgTemplate.ToDlgTemplatePtr();
	}

	m_hDlg = nullptr;
	m_hViewPort = nullptr;
	m_hTitle = nullptr;
	m_hFileNameCombo = nullptr;
	m_ResetShiftOnSelChange = 0;
	m_ShiftX = m_ShiftY = 0;
} // FileDialog::Init

// FileDialog::SetParent
void _FileDialog::SetParent(void *Parent) {
	m_ofn.hwndOwner = (HWND)Parent;
}

//-----------------------------------------------------------------------------
// FileDialog::SetFilter
//-----------------------------------------------------------------------------
void _FileDialog::SetFilter(const char *FileExtensions) {
	cStr::Split(FileExtensions, &m_FileExtensions);

	cStr Filter, Label, Mask;
	int i;
	m_Filter.Clear();
	for(i = 0; i < m_FileExtensions.Count(); i++) {
		const cStr &r = m_FileExtensions[i];
		if(m_LoadDialog) {
			if(!Label.IsEmpty()) {
				Label += " ";
			}
			Label += r;
			Mask += cStr::Format("*.%s;", r.ToCharPtr());
		} else {
			m_Filter += cStr::Format("%s|*.%s|", r.ToCharPtr(), r.ToCharPtr());
		}
	}
	if(m_LoadDialog) {
		m_Filter = Label + "|" + Mask + "|";
	}
	m_Filter.Append('|');
	m_Filter.Replace('|', '\0');
	m_Filter.CalcLength();
	m_ofn.lpstrFilter = m_Filter.ToCharPtr();
	m_ofn.nFilterIndex = 1;
} // FileDialog::SetFilter

// FileDialog::SetFilterIndex
void _FileDialog::SetFilterIndex(const int Index) {
	m_ofn.nFilterIndex = Index + 1;
}

//-----------------------------------------------------------------------------
// FileDialog::SetInitialPath
//-----------------------------------------------------------------------------
void _FileDialog::SetInitialPath(const char *InitialPath) {
	m_InitialDir = cIO::EnsureAbsolutePath(InitialPath);
	m_ofn.lpstrInitialDir = m_InitialDir.ToCharPtr();
} // FileDialog::SetInitialPath

// FileDialog::GetInitialPath
const cStr & _FileDialog::GetInitialPath() const {
	return m_InitialDir;
}

bool _FileDialog::s_Visible = false;

//-----------------------------------------------------------------------------
// FileDialog::DoModal
//-----------------------------------------------------------------------------
BOOL _FileDialog::DoModal(const char *InitialFileName) {
	s_Visible = true;
	
	// During modal file dialog pause the timer.
	// We are using "user pause" because "OnUnloadFileForPreview" for audio dialog
	// should be called before unpause. System unpause is fired immediately
	// after GetOpenFileName/GetSaveFileName calls which is not what we want.
	bool WasPaused = cPause::GetUserPause();
	if(!WasPaused) {
		cPause::SetUserPause(true);
	}

	// Storing CurDir:
	cStr CurDir(MAX_PATH);
	GetCurrentDirectory(MAX_PATH, CurDir.ToNonConstCharPtr());
	CurDir.CalcLength();
	
	// Open File Dialog:
	m_FilePathName.Clear();
	HookProc(nullptr, WM_SETOWNER, (WPARAM)this, (LPARAM)0);
	m_PrevDlgProc = nullptr;
	m_LoadIsClicked = false;
	m_UpdatePreview = false;
	m_Buffer[0] = '\0';
	cStr S;
	if(cStr::Length(InitialFileName) > 0) {
		cStr F = m_InitialDir;
		F.AppendPath(InitialFileName);
		S = cIO::EnsureAbsolutePath(F);
		strcpy_s(m_Buffer, S.ToCharPtr());
	}
	BOOL Load = m_LoadDialog ? GetOpenFileName(reinterpret_cast<OPENFILENAME *>(&m_ofn)) : GetSaveFileName(reinterpret_cast<OPENFILENAME *>(&m_ofn));
	cWinMain_SetOverrideParent(nullptr);
	OnUnloadFileForPreview(); // To free allocated data.
	HookProc(nullptr, WM_SETOWNER, (WPARAM)nullptr, (LPARAM)0);
	if(Load) {
		if(m_LoadDialog) { // During load we should check existance
			cAssertM(FileExist(m_FilePathName), cStr::Format("File \"%s\" does not exist", m_FilePathName));
		}
		SetInitialPath(m_FilePathName.GetFilePath()); // Storing last loaded from path in initial dir.
	} else {
		m_FilePathName.Clear();
	}
	
	// Restoring CurDir:
	SetCurrentDirectory(CurDir);

	if(!WasPaused) {
		cPause::SetUserPause(false);
	}
	s_Visible = false;

	if(m_FilePathName.IsEmpty()) {
		return 0;
	}
	// Check extension
	comms::cStr Ex = m_FilePathName.GetFileExtension();
	if(Ex.IsEmpty() || !m_FileExtensions.Contains(Ex, comms::cStr::EqualsNoCase)) {
		if(m_LoadDialog) {
			return 0;
		} else {
			if(m_ofn.nFilterIndex >= 1 && (int)m_ofn.nFilterIndex <= m_FileExtensions.Count()) {
				m_FilePathName.SetFileExtension(m_FileExtensions[m_ofn.nFilterIndex - 1]);
			}
		}
	}
	
	return 1;
} // FileDialog::DoModal

//-----------------------------------------------------------------------------
// FileDialog::FindCtrls
//-----------------------------------------------------------------------------
BOOL CALLBACK _FileDialog::FindCtrls(HWND hWnd, LPARAM lParam) {
	_FileDialog *pOwner = (_FileDialog *)lParam;
	if(pOwner && !pOwner->m_SimpleMode) {
		// Does this child contains ctrls?
		HWND hViewPort = GetDlgItem(hWnd, idViewPort);
		HWND hTitle = GetDlgItem(hWnd, idTitle);
		HWND hInfo = GetDlgItem(hWnd, idInfo);
		if(hViewPort) {
			pOwner->m_hViewPort = hViewPort;
		}
		if(hTitle) {
			pOwner->m_hTitle = hTitle;
		}
		if(hInfo) {
			pOwner->m_hInfo = hInfo;
		}
	}
	return TRUE;
} // FileDialog::FindCtrls

//-----------------------------------------------------------------------------
// FileDialog::DrawNoPreview
//-----------------------------------------------------------------------------
void _FileDialog::DrawNoPreview(Dib24 &dib) {
	const float d = 20.0f;
	const float t = 7.0f;
	const float w = (float)dib.GetWidth();
	const float h = (float)dib.GetHeight();
	const float dx = w <= h ? d : d + (w - h) / 2.0f;
	const float dy = h <= w ? d : d + (h - w) / 2.0f;

	const cVec2 lt(dx, dy);
	const cVec2 rt(w - dx, dy);
	const cVec2 lb(dx, h - dy);
	const cVec2 rb(w - dx, h - dy);
	const cVec2 xs(t, 0);
	const cVec2 ys(0, t);

	const cVec2 vb[] = { /* First strip: */ lt, lt + ys, rb - xs, rb, rb - ys, lt + xs,
		/* Second strip: */ lb, lb + xs, rt + ys, rt, rt - xs, lb - ys };
	const int ib[] = { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 6, 7, 8, 6, 8, 9, 6, 9, 10, 6, 10, 11 };
	Dib24::Pixel p = Dib24::Pixel::FromRgb(GetSysColor(COLOR_3DSHADOW));
	for(int i = 0; i < sizeof(ib) / sizeof(ib[0]); i += 3) {
		dib.Tri(vb[ib[i]], vb[ib[i + 1]], vb[ib[i + 2]], p);
	}
} // FileDialog::DrawNoPreview

//-----------------------------------------------------------------------------
// FileDialog::UpdatePreview
//-----------------------------------------------------------------------------
bool _FileDialog::UpdatePreview() {
	if(m_ResetShiftOnSelChange) {
		m_ShiftX = m_ShiftY = 0;
	}
	// Get file path (from dialog):
	cStr FilePath;
	int l = SendMessage(GetParent(m_hDlg), CDM_GETFILEPATH, 0, 0);
	if(l > 0) {
		FilePath.SetLength(l);
		SendMessage(GetParent(m_hDlg), CDM_GETFILEPATH, (WPARAM)l, (LPARAM)(LPSTR)FilePath.ToCharPtr());
		FilePath.CalcLength();
		FilePath.RemoveFileName();
	}
	
	// Get file name (from combo box):
	cStr FileName(MAX_PATH);
	GetWindowText(m_hFileNameCombo, FileName.ToNonConstCharPtr(), FileName.Length());
	FileName.CalcLength();
	FileName.RemoveFilePath();

	// Create file path name:
	cStr FilePathName = FilePath;
	FilePathName.AppendPath(FileName);

	// Get current directory:
	cStr CurDir(MAX_PATH, '\0');
	GetCurrentDirectory(MAX_PATH, CurDir.ToNonConstCharPtr());
	CurDir.CalcLength();

	// If created file path name is without path, we should use current.
	// This occurs when location is unidentified.
	// For example, when after selecting file we go to network navigation.
	FilePathName.SetFileDefaultPath(CurDir);

	// File is already accepted
	if(!m_FilePathName.IsEmpty() && cStr::EqualsNoCase(m_FilePathName, FilePathName)) {
		return true;
	}

	cStr LoadedFileTitle, LoadedFileInfo;
	
	// Clean - up:
	OnUnloadFileForPreview();
	m_FilePathName.Clear();
	LoadedFileTitle = "No Preview";

	cStr FileExtension = FileName.GetFileExtension();

	// We should resolve shortcuts:
	if(cStr::EqualsNoCase(FileExtension.ToCharPtr(), "lnk")) {
		FilePathName = cWin32::GetShortcutTarget(FilePathName);
		FileExtension = FilePathName.GetFileExtension();
	}

	// Checking file extension for support:
	bool IsSupported = m_FileExtensions.IndexOf(FileExtension, cStr::EqualsNoCase) != -1;

	bool IsAccepted = false;
	
	if(IsSupported) { // File type is supprted.
		bool Exist = FileExist(FilePathName);
		if(Exist) { // File is supported and exists.
			IsAccepted = OnLoadFileForPreview(FilePathName, LoadedFileInfo);
			if(IsAccepted) { // File is loaded and accepted.
				// Creating title and caping length for long names:
				LoadedFileTitle = FilePathName.GetFileBase();
				const int MaxLength = 30;
				if(LoadedFileTitle.Length() > MaxLength) {
					LoadedFileTitle.Remove(MaxLength);
					LoadedFileTitle.Append("...");
				}
				m_FilePathName = FilePathName;
			}
		}
	}
	if(!IsAccepted && !m_LoadDialog) {
		m_FilePathName = FilePathName;
	}
	
	if(m_LoadDialog) {
		EnableWindow(GetDlgItem(GetParent(m_hDlg), IDOK), IsAccepted);
	}

	m_DrawNoPreview = !IsAccepted || m_NoPreviewMode;
	if(!m_SimpleMode) {
		SetWindowText(m_hTitle, LoadedFileTitle);
		SetWindowText(m_hInfo, LoadedFileInfo);
		InvalidateRect(m_hViewPort, nullptr, FALSE);
	}

	return IsAccepted;
} // FileDialog::UpdatePreview

//----------------------------------------------------------------------------------------------
// FileDialog::ParentDlgProc
//----------------------------------------------------------------------------------------------
UINT_PTR CALLBACK _FileDialog::ParentDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
	static _FileDialog *pOwner = nullptr;
	static DLGPROC pPrevDlgProc = nullptr;
	switch(Msg) {
		case WM_SETOWNER:
			pOwner = (_FileDialog *)wParam;
			return FALSE;
		case WM_COMMAND:
			if(LOWORD(wParam) == IDOK) {
				if(pOwner->m_LoadDialog) {
					pOwner->m_LoadIsClicked = true;
				}
			} else if(!pOwner->m_LoadIsClicked && LOWORD(wParam) == cmb13 && HIWORD(wParam) == CBN_EDITCHANGE) {
				// Here we should not call "pOwner->UpdatePreview()" directly, because this handler is in another thread
				pOwner->m_UpdatePreview = true;
			}
			break;
		default:
			break;
	}
	return CallWindowProc((WNDPROC)pOwner->m_PrevDlgProc, hDlg, Msg, wParam, lParam);
} // FileDialog::ParentDlgProc

//-----------------------------------------------------------------------------
// FileDialog::AdjustLayout
//-----------------------------------------------------------------------------
void _FileDialog::AdjustLayout(HWND hDlg) {
	RECT FmRc, ToRc;
	GetWindowRect(hDlg, &FmRc);
	SetWindowPos(hDlg, nullptr, 0, 0, FmRc.right - FmRc.left, 415, SWP_NOZORDER | SWP_NOMOVE);
	GetWindowRect(hDlg, &ToRc);

	const int Delta = (ToRc.bottom - ToRc.top) - (FmRc.bottom - FmRc.top);

	const int Controls[] = {
		stc3, // "File name:"
		cmb13, // Name combo
		IDOK,
		stc2, // "Files of type:"
		cmb1, // Type combo
		IDCANCEL,
		0
	};
	
	// Shifting bottom controls:
	RECT rc;
	POINT Pos;
	for(const int *It = Controls; *It != 0; ++It) {
		HWND hCtrl = GetDlgItem(hDlg, *It);
		if(nullptr == hCtrl) {
			continue;
		}
		GetWindowRect(hCtrl, &rc);
		Pos.x = rc.left;
		Pos.y = rc.top;
		ScreenToClient(hDlg, &Pos);
		SetWindowPos(hCtrl, nullptr, Pos.x, Pos.y + Delta, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}

	// Changing height of list with files:
	HWND hList = GetDlgItem(hDlg, lst1);
	if(hList != nullptr) {
		GetWindowRect(hList, &rc);
		SetWindowPos(hList, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top + Delta, SWP_NOZORDER | SWP_NOMOVE);
	}
} // FileDialog::AdjustLayout

//-----------------------------------------------------------------------------------------
// FileDialog::HookProc
//-----------------------------------------------------------------------------------------
UINT_PTR CALLBACK _FileDialog::HookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
	static _FileDialog *pOwner = nullptr;
	const OFNOTIFY *pNotify;
	LPDRAWITEMSTRUCT lpdis;
	int Width, Height;
	static bool IsDragging;
	static POINT PrevMousePos;
	POINT CurMousePos;
	int dx, dy;
	RECT rc;
	Dib24::Pixel BackGround;
	int Len;
	cStr SelSpec;
	int FilterIndex;
	cStr FilePn;

	switch(Msg) {
		case WM_TIMER:
			if(pOwner != nullptr && pOwner->m_UpdatePreview) {
				pOwner->m_UpdatePreview = false;
				pOwner->UpdatePreview();
			}
			if(pOwner != nullptr && pOwner->m_AnimationMode && !pOwner->m_SimpleMode) {
				InvalidateRect(pOwner->m_hViewPort, nullptr, FALSE);
			}
			break;
		case WM_LBUTTONDOWN:
			cAssert(pOwner);
			if(pOwner->m_SimpleMode) {
				break;
			}
			GetCursorPos(&PrevMousePos);
			GetWindowRect(pOwner->m_hViewPort, &rc);
			if(pOwner->m_FilePathName.Length() && PtInRect(&rc, PrevMousePos)) { // File exist and click within viewport.
				SetCapture(hDlg);
				IsDragging = true;
			}
			break;
		case WM_MOUSEMOVE:
			if(IsDragging) {
				GetCursorPos(&CurMousePos);
				dx = CurMousePos.x - PrevMousePos.x;
				dy = CurMousePos.y - PrevMousePos.y;
				pOwner->m_ShiftX += dx;
				pOwner->m_ShiftY += dy;
				PrevMousePos = CurMousePos;
				if(dx || dy) {
					cAssert(pOwner);
					InvalidateRect(pOwner->m_hViewPort, nullptr, FALSE);
				}
			}
			break;
		case WM_LBUTTONUP:
			if(IsDragging) {
				ReleaseCapture();
				IsDragging = false;
			}
			break;
		case WM_DRAWITEM:
			if(pOwner != nullptr && !pOwner->m_SimpleMode && idViewPort == (int)wParam) {
				lpdis = (LPDRAWITEMSTRUCT)lParam;
				Width = lpdis->rcItem.right - lpdis->rcItem.left;
				Height = lpdis->rcItem.bottom - lpdis->rcItem.top;
				if(Width != pOwner->m_dib.GetWidth() || Height != pOwner->m_dib.GetHeight()) {
					pOwner->m_dib.Init(Width, Height);
				}
				BackGround = Dib24::Pixel::FromRgb(GetSysColor(COLOR_3DFACE));
				pOwner->m_dib.Clear(BackGround);
				if(pOwner->m_DrawNoPreview) {
					DrawNoPreview(pOwner->m_dib);
				} else {
					pOwner->OnRenderPreview(pOwner->m_dib, pOwner->m_ShiftX, pOwner->m_ShiftY);
				}
				pOwner->m_dib.ToDC(lpdis->hDC);
				return TRUE;
			}
			break;
		case WM_SETOWNER:
			pOwner = (_FileDialog *)wParam;
			IsDragging = false;
			break;
		case WM_NOTIFY:
			pNotify = (const OFNOTIFY *)lParam;
			switch(pNotify->hdr.code) {
				case CDN_TYPECHANGE:
					FilterIndex = (int)pNotify->lpOFN->nFilterIndex - 1;
					if(FilterIndex >= 0 && FilterIndex < pOwner->m_FileExtensions.Count()) {
						Len = SendMessage(GetParent(hDlg), CDM_GETSPEC, 0, 0);
						if(Len > 0) {
							FilePn.SetLength(Len - 1);
							SendMessage(GetParent(hDlg), CDM_GETSPEC, (WPARAM)Len, (LPARAM)FilePn.ToCharPtr());
							FilePn.SetFileExtension(pOwner->m_FileExtensions[FilterIndex].ToLower());
							SendMessage(GetParent(hDlg), CDM_SETCONTROLTEXT, (WPARAM)cmb13, (LPARAM)FilePn.ToCharPtr());
						}
					}
					break;
				case CDN_INITDONE:
					AdjustLayout(GetParent(hDlg));
					cWin32::CenterWindow(GetParent(hDlg), nullptr);
					cAssert(pOwner);
					pOwner->m_hDlg = hDlg;
					cWinMain_SetOverrideParent(hDlg);
					pOwner->m_hFileNameCombo = GetDlgItem(GetParent(hDlg), cmb13);
					if(pOwner->m_LoadDialog) {
						if(!pOwner->m_SimpleMode) {
							SendMessage(GetParent(hDlg), CDM_SETCONTROLTEXT, (WPARAM)IDOK, (LPARAM)"Load");
						}
					}
					if(pOwner->m_LoadDialog) {
						EnableWindow(GetDlgItem(GetParent(hDlg), IDOK), FALSE); // "Load" is initially disabled.
					}
					EnumChildWindows(GetParent(hDlg), FindCtrls, (LPARAM)pOwner); // Find handlers to ViewPort && Title ctrls:
					// Subclassing parent dlg:
					ParentDlgProc(nullptr, WM_SETOWNER, (WPARAM)pOwner, 0);
					pOwner->m_PrevDlgProc = (DLGPROC)SetWindowLongPtr(GetParent(hDlg), GWLP_WNDPROC, (LONG_PTR)&ParentDlgProc);
					SetTimer(hDlg, 1, 0x0000000A, nullptr);
					break;
				case CDN_SELCHANGE:
				case CDN_FILEOK:
					if(pOwner->m_LoadIsClicked) {
						break;
					}
					Len = SendMessage(GetParent(hDlg), CDM_GETSPEC, 0, 0);
					SelSpec.Clear();
					if(Len > 0) {
						SelSpec.SetLength(Len);
						SendMessage(GetParent(hDlg), CDM_GETSPEC, (WPARAM)Len, (LPARAM)SelSpec.ToCharPtr());
					}
					SendMessage(pOwner->m_hFileNameCombo, WM_SETTEXT, 0, (LPARAM)SelSpec.ToCharPtr());
				case CDN_FOLDERCHANGE:
					// "HookProc" and "ParentDlgProc" are working in different threads, therefore
					// we should call "pOwner->UpdatePreview()" centrally
					pOwner->m_UpdatePreview = true;
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
	return FALSE;
} // FileDialog::HookProc

//-----------------------------------------------------------------------------
// FileDialog::FileExist
//-----------------------------------------------------------------------------
bool _FileDialog::FileExist(const char *FileName) {
	struct _stat Buffer;
	int r = _stat(FileName, &Buffer);
	return r != -1 && (Buffer.st_mode & _S_IFREG);
} // FileDialog::FileExist

//*****************************************************************************
// ImageFileDialog
//*****************************************************************************

//---------------------------------------------------------------------------------------------
// ImageFileDialog.ctor
//---------------------------------------------------------------------------------------------
ImageFileDialog::ImageFileDialog(const bool Load, const char *InitialPath) : _FileDialog(cWinMain_GetWindow(), InitialPath, nullptr, Load) {
	SetTitle(Load ? "Load Image File" : "Save Image File");
	// Gathering available image codecs:
	cStr Filter;
	for(int i = 0; i < cIO::GetImageCodecs().Count(); i++) {
		Filter << " " << cIO::GetImageCodecs()[i].FileExtension;
	}
	SetFilter(Filter);
} // ImageFileDialog.ctor

//--------------------------------------------------------------------------------
// ImageFileDialog::OnLoadFileForPreview
//--------------------------------------------------------------------------------
bool ImageFileDialog::OnLoadFileForPreview(const char *FileName, cStr &FileInfo) {
	cImage Img;
	if(!cIO::LoadImage(FileName, &Img)) { // Error during file operations
		return false;
	}

	cDimension::Enum Dim = Img.GetDimension();
	cFormat::Enum Format = Img.GetFormat();

	cStr DS = "Type: ";
	DS += cDimension::ToString(Dim);

	cStr FS = "Format: ";
	FS += cFormat::ToString(Format);

	cStr MS = "Mipmaps: ";
	int c = Img.GetMipMapCount();
	MS += (1 == c ? cStr("no") : cStr::ToString(c));

	if(cFormat::IsCompressed(Img.GetFormat())) {
		Img.Uncompress();
	}

	if(cDimension::Cube == Dim || cDimension::ThreeD == Dim) {
		Img.Flip();
	}
	Img.ToFormat(cFormat::Rgb8);
	
	m_Image.Init(Img.GetWidth(), Img.GetHeight());
	int X, Y;
	cImage::PixelRgb8 Rgb;

	for(Y = 0; Y < m_Image.GetHeight(); Y++) {
		for(X = 0; X < m_Image.GetWidth(); X++) {
			Rgb = Img.GetPixelRgb8(X, Y);
			m_Image.SetPixel(X, Img.GetHeight() - Y - 1, Dib24::Pixel(Rgb.r, Rgb.g, Rgb.b));
		}
	}
	
	// Info:
	FileInfo << "Size: " << Img.GetWidth();
	if(Dim != cDimension::OneD) {
		FileInfo << " x " << Img.GetHeight();
		if(cDimension::ThreeD == Dim) {
			FileInfo << " x " << Img.GetDepth();
		}
	}
	FileInfo << "\n" << FS << "\n" << DS << "\n" << MS;

	return true;
} // ImageFileDialog::OnLoadFileForPreview

//-----------------------------------------------------------------------------
// ImageFileDialog::OnUnloadFileForPreview
//-----------------------------------------------------------------------------
void ImageFileDialog::OnUnloadFileForPreview() {
	m_Image.Free();
} // ImageFileDialog::OnUnloadFileForPreview

//-----------------------------------------------------------------------------
// ImageFileDialog::OnRenderPreview
//-----------------------------------------------------------------------------
void ImageFileDialog::OnRenderPreview(Dib24 &dib, int xShift, int yShift) {
	if(m_Image.GetWidth() == 0 || m_Image.GetHeight() == 0) {
		return;
	}
	const float wFm = (float)m_Image.GetWidth(), hFm = (float)m_Image.GetHeight();
	const float wTo = (float)dib.GetWidth(), hTo = (float)dib.GetHeight();
	const float Scale = cMath::Min(wTo / wFm, hTo / hFm);
	const int dx = (int)((wTo - Scale * wFm) / 2.0f), dy = (int)((hTo - Scale * hFm) / 2.0f);
	const int W = (int)cMath::Max(Scale * wFm, 1.0f), H = (int)cMath::Max(Scale * hFm, 1.0f);

	for(int y = dy; y < H + dy; y++) {
		for(int x = dx; x < W + dx; x++) {
			const float s = cMath::Round((float)(x - dx)) / (float)W;
			const float t = cMath::Round((float)(y - dy)) / (float)H;
			dib.SetPixel(x, y, m_Image.GetPixel(s, t));
		}
	}
} // ImageFileDialog::OnRenderPreview

// XmlFileDialog.ctor
XmlFileDialog::XmlFileDialog(const bool Load, const char *InitialPath) : _FileDialog(cWinMain_GetWindow(), InitialPath, nullptr, Load) {
	SetTitle(Load ? "Load Xml File" : "Save Xml File");
	SetFilter("Xml");
	m_ResetShiftOnSelChange = true;
}

// XmlFileDialog::OnLoadFileForPreview
bool XmlFileDialog::OnLoadFileForPreview(const char *FilePn, cStr &FileInfo) {
	cFile Xml;
	if(!cIO::LoadFile(FilePn, &Xml)) {
		return false; // Error during I/
	}
	cStr S;
	while(Xml.ReadString(&S)) {
		m_Lines.Add(S);
	}
	FileInfo = "Lines: " + cStr::ToString(m_Lines.Count());
	return true;
}

// XmlFileDialog::OnUnloadFileForPreview
void XmlFileDialog::OnUnloadFileForPreview() {
	// Nothing special to do...
	m_Lines.Clear();
}

// XmlFileDialog::OnRenderPreview
void XmlFileDialog::OnRenderPreview(Dib24 &Dib, int xShift, int yShift) {
	int l, Y = yShift - 16;
	for(l = 0; l < m_Lines.Count(); l++) {
		Y += 16;
		if(Y < 0) {
			continue; // Skip lines drawing above top
		}
		const cStr &S = m_Lines[l];
		Dib.Str(S, xShift, Y);
		if(Y > Dib.GetHeight()) {
			break; // Skip lines drawing below bottom
		}
	}
}

//*****************************************************************************
// UnknownFileDialog
//*****************************************************************************

//---------------------------------------------------------------------------------------------
// UnknownFileDialog.ctor
//---------------------------------------------------------------------------------------------
UnknownFileDialog::UnknownFileDialog(const bool Load, const char *Title, const cList<cStr> &Extensions, const char *InitialPath) :
_FileDialog(cWinMain_GetWindow(), InitialPath, nullptr, Load, true) {
	SetTitle(Title);

	int i;
	cStr Types, Filter;
	for(i = 0; i < Extensions.Count(); i++) {
		if(!Types.IsEmpty()) {
			Types += "; ";
		}
		Types << "*." << Extensions[i];
		if(!Filter.IsEmpty()) {
			Filter += " ";
		}
		Filter += Extensions[i];
	}
	SetFilter(Filter);
} // UnknownFileDialog.ctor

// UnknownFileDialog::OnLoadFileForPreview
bool UnknownFileDialog::OnLoadFileForPreview(const char *, cStr &) {
	return true;
}

// UnknownFileDialog::OnUnloadFileForPreview
void UnknownFileDialog::OnUnloadFileForPreview() {
}

// UnknownFileDialog::OnRenderPreview
void UnknownFileDialog::OnRenderPreview(Dib24 &, int, int) {
}

// Dib24_Material.ctor
Dib24_Material::Dib24_Material() {
}

//---------------------------------------------------------------------------------------------------------------------------------
// Dib24_Material.ctor
//---------------------------------------------------------------------------------------------------------------------------------
Dib24_Material::Dib24_Material(const char *Name, const cColor &Ambient, const cColor &Diffuse, const cColor &Specular, const float Shininess)
: Name(Name), Ambient(Ambient), Diffuse(Diffuse), Specular(Specular), Shininess(Shininess) {
} // Dib24_Material.ctor

const Dib24_Material Dib24_Material::Brass("Brass",
								 cColor(0.329412f, 0.223529f, 0.027451f, 1.0f),
								 cColor(0.780392f, 0.568627f, 0.113725f, 1.0f),
								 cColor(0.992157f, 0.941176f, 0.807843f, 1.0f), 27.8974f);

const Dib24_Material Dib24_Material::Bronze("Bronze",
								  cColor(0.2125f, 0.1275f, 0.054f, 1.0f),
								  cColor(0.714f, 0.4284f, 0.18144f, 1.0f),
								  cColor(0.393548f, 0.271906f, 0.166721f, 1.0f), 25.6f);

const Dib24_Material Dib24_Material::PolishedBronze("Polished Bronze",
										  cColor(0.25f, 0.148f, 0.06475f, 1.0f),
										  cColor(0.4f, 0.2368f, 0.1036f, 1.0f),
										  cColor(0.774597f, 0.458561f, 0.200621f, 1.0f), 76.8f);

const Dib24_Material Dib24_Material::Chrome("Chrome",
								  cColor(0.25f, 0.25f, 0.25f, 1.0f),
								  cColor(0.4f, 0.4f, 0.4f, 1.0f),
								  cColor(0.774597f, 0.774597f, 0.774597f, 1.0f), 76.8f);

const Dib24_Material Dib24_Material::Copper("Copper",
								  cColor(0.19125f, 0.0735f, 0.0225f, 1.0f),
								  cColor(0.7038f, 0.27048f, 0.0828f, 1.0f),
								  cColor(0.256777f, 0.137622f, 0.086014f, 1.0f), 12.8f);

const Dib24_Material Dib24_Material::PolishedCopper("Polished Copper",
										  cColor(0.2295f, 0.08825f, 0.0275f, 1.0f),
										  cColor(0.5508f, 0.2118f, 0.066f, 1.0f),
										  cColor(0.580594f, 0.223257f, 0.0695701f, 1.0f), 51.2f);

const Dib24_Material Dib24_Material::Gold("Gold",
								cColor(0.24725f, 0.1995f, 0.0745f, 1.0f),
								cColor(0.75164f, 0.60648f, 0.22648f, 1.0f),
								cColor(0.628281f, 0.555802f, 0.366065f, 1.0f), 51.2f);

const Dib24_Material Dib24_Material::PolishedGold("Polished Gold",
										cColor(0.24725f, 0.2245f, 0.0645f, 1.0f),
										cColor(0.34615f, 0.3143f, 0.0903f, 1.0f),
										cColor(0.797357f, 0.723991f, 0.208006f, 1.0f), 83.2f);

const Dib24_Material Dib24_Material::Pewter("Pewter",
								  cColor(0.105882f, 0.058824f, 0.113725f, 1.0f),
								  cColor(0.427451f, 0.470588f, 0.541176f, 1.0f),
								  cColor(0.333333f, 0.333333f, 0.521569f, 1.0f), 9.84615f);

const Dib24_Material Dib24_Material::Silver("Silver",
								  cColor(0.19225f, 0.19225f, 0.19225f, 1.0f),
								  cColor(0.50754f, 0.50754f, 0.50754f, 1.0f),
								  cColor(0.508273f, 0.508273f, 0.508273f, 1.0f), 51.2f);

const Dib24_Material Dib24_Material::PolishedSilver("Polished Silver",
										  cColor(0.23125f, 0.23125f, 0.23125f, 1.0f),
										  cColor(0.2775f, 0.2775f, 0.2775f, 1.0f),
										  cColor(0.773911f, 0.773911f, 0.773911f, 1.0f), 89.6f);

const Dib24_Material Dib24_Material::Emerald("Emerald",
								   cColor(0.0215f, 0.1745f, 0.0215f, 0.55f),
								   cColor(0.07568f, 0.61424f, 0.07568f, 0.55f),
								   cColor(0.633f, 0.727811f, 0.633f, 0.55f), 76.8f);

const Dib24_Material Dib24_Material::Jade("Jade",
								cColor(0.135f, 0.2225f, 0.1575f, 0.95f),
								cColor(0.54f, 0.89f, 0.63f, 0.95f),
								cColor(0.316228f, 0.316228f, 0.316228f, 0.95f), 12.8f);

const Dib24_Material Dib24_Material::Obsidian("Obsidian",
									cColor(0.05375f, 0.05f, 0.06625f, 0.82f),
									cColor(0.18275f, 0.17f, 0.22525f, 0.82f),
									cColor(0.332741f, 0.328634f, 0.346435f, 0.82f), 38.4f);

const Dib24_Material Dib24_Material::Pearl("Pearl",
								 cColor(0.25f, 0.20725f, 0.20725f, 0.922f),
								 cColor(1.0f, 0.829f, 0.829f, 0.922f),
								 cColor(0.296648f, 0.296648f, 0.296648f, 0.922f), 11.264f);

const Dib24_Material Dib24_Material::Ruby("Ruby",
								cColor(0.1745f, 0.01175f, 0.01175f, 0.55f),
								cColor(0.61424f, 0.04136f, 0.04136f, 0.55f),
								cColor(0.727811f, 0.626959f, 0.626959f, 0.55f), 76.8f);

const Dib24_Material Dib24_Material::Turquoise("Turquoise",
									 cColor(0.1f, 0.18725f, 0.1745f, 0.8f),
									 cColor(0.396f, 0.74151f, 0.69102f, 0.8f),
									 cColor(0.297254f, 0.30829f, 0.306678f, 0.8f), 12.8f);

const Dib24_Material Dib24_Material::BlackPlastic("Black Plastic",
										cColor(0.0f, 0.0f, 0.0f, 1.0f),
										cColor(0.01f, 0.01f, 0.01f, 1.0f),
										cColor(0.50f, 0.50f, 0.50f, 1.0f), 32.0f);

const Dib24_Material Dib24_Material::BlackRubber("Black Rubber", 
									   cColor(0.02f, 0.02f, 0.02f, 1.0f),
									   cColor(0.01f, 0.01f, 0.01f, 1.0f),
									   cColor(0.4f, 0.4f, 0.4f, 1.0f), 10.0f);

HBITMAP Dib24::Init(HDC hDC, const int Width, const int Height) {
	Free();
	if(Width <= 0 || Height <= 0) {
		return nullptr;
	}
	
	// m_nBytesPerScanLine:
	m_nBytesPerScanLine = 3 * Width;
	int mod = m_nBytesPerScanLine % 4;
	if(mod) m_nBytesPerScanLine += 4 - mod;
	// m_bmi:
	m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_bmi.bmiHeader.biWidth = Width;
	m_bmi.bmiHeader.biHeight = Height;
	m_bmi.bmiHeader.biPlanes = 1;
	m_bmi.bmiHeader.biBitCount = 24;
	m_bmi.bmiHeader.biCompression = BI_RGB;
	m_bmi.bmiHeader.biSizeImage = m_nBytesPerScanLine * Height;
	// m_pBits:
	m_hBitmap = CreateDIBSection(hDC, &m_bmi, DIB_RGB_COLORS, (void **)&m_pBits, nullptr, 0);
	cAssert(m_pBits);
	return m_hBitmap;
}

void Dib24::Clear(const Dib24::Pixel &p) {
	BYTE *Ptr = m_pBits;
	for(int y = 0; y < m_bmi.bmiHeader.biHeight; y++) {
		for(int x = 0; x < m_bmi.bmiHeader.biWidth; x++) {
			*Ptr++ = p.b;
			*Ptr++ = p.g;
			*Ptr++ = p.r;
		}
		Ptr += m_nBytesPerScanLine - 3 * m_bmi.bmiHeader.biWidth;
	}
}

//-----------------------------------------------------------------------------
// Dib24::GetPixel : Dib24::Pixel(const float, const float) const
//-----------------------------------------------------------------------------
Dib24::Pixel Dib24::GetPixel(const float s, const float t) const {
	float x = (float)GetWidth() * s - 0.5f;
	float y = (float)GetHeight() * t - 0.5f;
	int x0 = cMath::Clamp((int)cMath::Floor(x), 0, GetWidth() - 1);
	int y0 = cMath::Clamp((int)cMath::Floor(y), 0, GetHeight() - 1);
	int x1 = cMath::Clamp((int)cMath::Ceil(x), 0, GetWidth() - 1);
	int y1 = cMath::Clamp((int)cMath::Ceil(y), 0, GetHeight() - 1);
	Pixel p00 = GetPixel(x0, y0);
	Pixel p10 = GetPixel(x1, y0);
	Pixel p01 = GetPixel(x0, y1);
	Pixel p11 = GetPixel(x1, y1);
	float k00 = (1.0f - (x - x0)) * (1.0f - (y - y0));
	float k10 = (x - x0) * (1.0f - (y - y0));
	float k01 = (1.0f - (x - x0)) * (y - y0);
	float k11 = (x - x0) * (y - y0);
	Pixel p;
	p.r = (unsigned char)((float)p00.r * k00 + (float)p01.r * k01 + (float)p10.r * k10 + (float)p11.r * k11);
	p.g = (unsigned char)((float)p00.g * k00 + (float)p01.g * k01 + (float)p10.g * k10 + (float)p11.g * k11);
	p.b = (unsigned char)((float)p00.b * k00 + (float)p01.b * k01 + (float)p10.b * k10 + (float)p11.b * k11);
	return p;
}

void Dib24::Circle(int cx, int cy, int R, const cColor &C) {
	long r2 = (long)R * (long)R;
	long dst = 4 * r2;
	int dxt = int((double)R / 1.414213562373);
	long t = 0;
	long s = - 4 * r2 * (long)R;
	long e = (- s / 2) - 3 * r2;
	long ca = - 6 * r2;
	long cd = - 10 * r2;
	int x = 0;
	int y = R;
	Dib24::Pixel Px = Dib24::Pixel::FromColor(C);
	SetPixel(cx, cy + R, Px);
	SetPixel(cx, cy - R, Px);
	SetPixel(cx + R, cy, Px);
	SetPixel(cx - R, cy, Px);
	for(long indx = 1; indx <= dxt; indx++) {
		x++;
		if(e >= 0) e += t + ca;
		else {
			y--;
			e += t - s + cd;
			s += dst;
		}
		t -= dst;
		SetPixel(cx + x, cy + y, Px);
		SetPixel(cx + y, cy + x, Px);
		SetPixel(cx + y, cy - x, Px);
		SetPixel(cx + x, cy - y, Px);
		SetPixel(cx - x, cy - y, Px);
		SetPixel(cx - y, cy - x, Px);
		SetPixel(cx - y, cy + x, Px);
		SetPixel(cx - x, cy + y, Px);
	}
}

//-----------------------------------------------------------------------------
// Dib24::Round : void(int, int, int, const cColor &)
//-----------------------------------------------------------------------------
void Dib24::Round(int xCenter, int yCenter, int Radius, const cColor &Color) {
	int R2 = cMath::Square(Radius);
	Pixel Px = Pixel::FromColor(Color);
	for(int x = 0; x <= Radius; x++) {
		int r2 = cMath::Square(x);
		int dr2 = 0;
		for(int y = 0; y <= x; y++) {
			r2 += dr2;
			dr2 += 2;
			if(r2 >= R2) break;
			
			SetPixel(xCenter + x, yCenter + y, Px);
			SetPixel(xCenter - x, yCenter - y, Px);
			SetPixel(xCenter + x, yCenter - y, Px);
			SetPixel(xCenter - x, yCenter + y, Px);
			SetPixel(xCenter + y, yCenter + x, Px);
			SetPixel(xCenter - y, yCenter - x, Px);
			SetPixel(xCenter - y, yCenter + x, Px);
			SetPixel(xCenter + y, yCenter - x, Px);
		}
	}
}

void Dib24::Sphere(int cx, int cy, int R, const cColor &Clr) {
	int R2 = R * R;
	for(int x = 0; x <= R; x++) {
		int r2 = x * x;
		int dr2 = 0;
		for(int y = 0; y <= x; y++) {
			r2 += dr2;
			dr2 += 2;
			if(r2 >= R2) break;
			float k = 1.0f - float((r2 << 8) / R2) / 512.0f;
			
			cColor C = k * Clr;
			Dib24::Pixel p = Dib24::Pixel::FromColor(C);
			SetPixel(cx + x, cy + y, p);
			SetPixel(cx - x, cy - y, p);
			SetPixel(cx + x, cy - y, p);
			SetPixel(cx - x, cy + y, p);
			SetPixel(cx + y, cy + x, p);
			SetPixel(cx - y, cy - x, p);
			SetPixel(cx - y, cy + x, p);
			SetPixel(cx + y, cy - x, p);
		}
	}
}

// Dib24::Quad
void Dib24::Quad(const cVec2 &ul, const float d, const Dib24::Pixel &p) {
	Tri(ul, ul + cVec2(1.0f, 0.0f) * d, ul + cVec2(1.0f, 1.0f) * d, p);
	Tri(ul, ul + cVec2(1.0f, 1.0f) * d, ul + cVec2(0.0f, 1.0f) * d, p);
}

// Dib24::Tri
void Dib24::Tri(const cVec2 &t0, const cVec2 &t1, const cVec2 &t2, const Dib24::Pixel &p) {
	// 28.4
	const int x1 = int(16. * t0.x), x2 = int(16. * t1.x), x3 = int(16. * t2.x);
	const int y1 = int(16. * t0.y), y2 = int(16. * t1.y), y3 = int(16. * t2.y);

	// Deltas:
	const int Dx12 = x1 - x2, Dx23 = x2 - x3, Dx31 = x3 - x1;
	const int Dy12 = y1 - y2, Dy23 = y2 - y3, Dy31 = y3 - y1;
	// int fixed - point:
	const int fDx12 = Dx12 << 4, fDx23 = Dx23 << 4, fDx31 = Dx31 << 4;
	const int fDy12 = Dy12 << 4, fDy23 = Dy23 << 4, fDy31 = Dy31 << 4;

	// Bounding rect:
	int xMin = (cMath::Min(x1, x2, x3) + 0xF) >> 4;
	int xMax = (cMath::Max(x1, x2, x3) + 0xF) >> 4;
	int yMin = (cMath::Min(y1, y2, y3) + 0xF) >> 4;
	int yMax = (cMath::Max(y1, y2, y3) + 0xF) >> 4;

	// Constant parts of half - space functions:
	int C1 = Dy12 * x1 - Dx12 * y1;
	int C2 = Dy23 * x2 - Dx23 * y2;
	int C3 = Dy31 * x3 - Dx31 * y3;

	// Fill convention:
	if(Dy12 < 0 || (Dy12 == 0 && Dx12 > 0)) C1++;
	if(Dy23 < 0 || (Dy23 == 0 && Dx23 > 0)) C2++;
	if(Dy31 < 0 || (Dy31 == 0 && Dx31 > 0)) C3++;

	// Starting values of the half - space functions at the top of the bounding rectangle:
	int Cy1 = C1 + Dx12 * (yMin << 4) - Dy12 * (xMin << 4);
	int Cy2 = C2 + Dx23 * (yMin << 4) - Dy23 * (xMin << 4);
	int Cy3 = C3 + Dx31 * (yMin << 4) - Dy31 * (xMin << 4);

	// Scanning:
	for(int y = yMin; y < yMax; y++) {
		// Starting values for horizontal scan:
		int Cx1 = Cy1, Cx2 = Cy2, Cx3 = Cy3;
		for(int x = xMin; x < xMax; x++) {
			if(Cx1 > 0 && Cx2 > 0 && Cx3 > 0) {
				SetPixel(x, y, p);
			}
			Cx1 -= fDy12;
			Cx2 -= fDy23;
			Cx3 -= fDy31;
		}
		Cy1 += fDx12;
		Cy2 += fDx23;
		Cy3 += fDx31;
	}
} // Dib24::Tri

// Dib24::BlockTri
void Dib24::BlockTri(const cVec2 &t0, const cVec2 &t1, const cVec2 &t2, const Dib24::Pixel &p) {
	// 28.4
	const int x1 = int(16. * t0.x), x2 = int(16. * t1.x), x3 = int(16. * t2.x);
	const int y1 = int(16. * t0.y), y2 = int(16. * t1.y), y3 = int(16. * t2.y);

	// Deltas:
	const int Dx12 = x1 - x2, Dx23 = x2 - x3, Dx31 = x3 - x1;
	const int Dy12 = y1 - y2, Dy23 = y2 - y3, Dy31 = y3 - y1;
	// int fixed - point:
	const int fDx12 = Dx12 << 4, fDx23 = Dx23 << 4, fDx31 = Dx31 << 4;
	const int fDy12 = Dy12 << 4, fDy23 = Dy23 << 4, fDy31 = Dy31 << 4;

	// Bounding rect:
	int xMin = (cMath::Min(x1, x2, x3) + 0xF) >> 4;
	int xMax = (cMath::Max(x1, x2, x3) + 0xF) >> 4;
	int yMin = (cMath::Min(y1, y2, y3) + 0xF) >> 4;
	int yMax = (cMath::Max(y1, y2, y3) + 0xF) >> 4;

	// Block size (must be power of 2):
	const int q = 8; // 8x8

	// Start in corner of block:
	xMin &= ~(q - 1);
	yMin &= ~(q - 1);

	// Constant parts of half - space functions:
	int C1 = Dy12 * x1 - Dx12 * y1;
	int C2 = Dy23 * x2 - Dx23 * y2;
	int C3 = Dy31 * x3 - Dx31 * y3;

	// Fill convention:
	if(Dy12 < 0 || (Dy12 == 0 && Dx12 > 0)) C1++;
	if(Dy23 < 0 || (Dy23 == 0 && Dx23 > 0)) C2++;
	if(Dy31 < 0 || (Dy31 == 0 && Dx31 > 0)) C3++;

	// Loop through blocks:
	for(int y = yMin; y < yMax; y += q) {
		for(int x = xMin; x < xMax; x += q) {
			// Corners of block:
			int x0 = x << 4, x1 = (x + q - 1) << 4;
			int y0 = y << 4, y1 = (y + q - 1) << 4;
			// Evaluate half - space functions:
			bool a00 = C1 + Dx12 * y0 - Dy12 * x0 > 0;
			bool a10 = C1 + Dx12 * y0 - Dy12 * x1 > 0;
			bool a01 = C1 + Dx12 * y1 - Dy12 * x0 > 0;
			bool a11 = C1 + Dx12 * y1 - Dy12 * x1 > 0;
			int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

			bool b00 = C2 + Dx23 * y0 - Dy23 * x0 > 0;
			bool b10 = C2 + Dx23 * y0 - Dy23 * x1 > 0;
			bool b01 = C2 + Dx23 * y1 - Dy23 * x0 > 0;
			bool b11 = C2 + Dx23 * y1 - Dy23 * x1 > 0;
			int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

			bool c00 = C3 + Dx31 * y0 - Dy31 * x0 > 0;
			bool c10 = C3 + Dx31 * y0 - Dy31 * x1 > 0;
			bool c01 = C3 + Dx31 * y1 - Dy31 * x0 > 0;
			bool c11 = C3 + Dx31 * y1 - Dy31 * x1 > 0;
			int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

			// Skip this block if it's outside at least one edge:
			if(a == 0x0 || b == 0x0 || c == 0x0) continue;

			if(a == 0xF && b == 0xF && c == 0xF) { // block is totally covered
				for(int iy = y; iy < y + q; iy++)
					for(int ix = x; ix < x + q; ix++) {
						SetPixel(ix, iy, p);
					}
			} else { // block is partically covered
				// Starting values of the half - space functions at the top of the bounding rectangle:
				int Cy1 = C1 + Dx12 * y0 - Dy12 * x0;
				int Cy2 = C2 + Dx23 * y0 - Dy23 * x0;
				int Cy3 = C3 + Dx31 * y0 - Dy31 * x0;

				// Scanning:
				for(int iy = y; iy < y + q; iy++) {
					// Starting values for horizontal scan:
					int Cx1 = Cy1, Cx2 = Cy2, Cx3 = Cy3;
					for(int ix = x; ix < x + q; ix++) {
						if(Cx1 > 0 && Cx2 > 0 && Cx3 > 0) {
							SetPixel(ix, iy, p);
						}
						Cx1 -= fDy12;
						Cx2 -= fDy23;
						Cx3 -= fDy31;
					}
					Cy1 += fDx12;
					Cy2 += fDx23;
					Cy3 += fDx31;
				}
			}
		}
	}
} // Dib24::BlockTri

// Dib24::ZTri
void Dib24::ZTri(const cVec3 &t0, const cVec3 &t1, const cVec3 &t2, const Dib24::Pixel &p, int *pZBuffer) {
	int x0 = (int)t0.x, y0 = (int)t0.y, z0 = (int)t0.z;
	int x1 = (int)t1.x, y1 = (int)t1.y, z1 = (int)t1.z;
	int x2 = (int)t2.x, y2 = (int)t2.y, z2 = (int)t2.z;

	// Sorting ascending along y axis:
	if(y0 > y1) { cMath::Swap(x0, x1); cMath::Swap(y0, y1); cMath::Swap(z0, z1); }
	if(y0 > y2) { cMath::Swap(x0, x2); cMath::Swap(y0, y2); cMath::Swap(z0, z2); }
	if(y1 > y2) { cMath::Swap(x1, x2); cMath::Swap(y1, y2); cMath::Swap(z1, z2); }

	if(y2 - y1 == 0) return;

	for(int y = y0; y <= y2; y++) {
		// Crossing with longest section ((x0, y0) --- (x2, y2)):
		int _x1 = x0 + (y - y0) * (x2 - x0) / (y2 - y0);
		int _z1 = z0 + (y - y0) * (z2 - z0) / (y2 - y0);

		// Crossing with short section ((x0, y0) --- (x1, y1) or (x1, y1) --- (x2, y2)):
		int _x2, _z2;
		if(y < y1) {
			_x2 = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
			_z2 = z0 + (y - y0) * (z1 - z0) / (y1 - y0);
		} else {
			if(y2 == y1) {
				_x2 = x1;
				_z2 = z1;
			}
			else {
				_x2 = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
				_z2 = z1 + (y - y1) * (z2 - z1) / (y2 - y1);
			}
		}

		if(_x1 > _x2) {
			cMath::Swap(_x1, _x2);
			cMath::Swap(_z1, _z2);
		}

		// Rasterizing horizontal line (_x1 - _x2):
		for(int x = _x1; x < _x2; x++) {
			int z = _z1 + (x - _x1) * (_z2 - _z1) / (_x2 - _x1);
			if(x >= 0 && x < m_bmi.bmiHeader.biWidth && y >= 0 && y < m_bmi.bmiHeader.biHeight) {
				int index = y * m_bmi.bmiHeader.biWidth + x;
				if(pZBuffer[index] < z) {
					SetPixel(x, y, p);
					pZBuffer[index] = z;
				}
			}
		}
	}
} // Dib24::ZTri

void Dib24::DrawLine(const cVec2 &p0, const cVec2 &p1, const Dib24::Pixel &p) {
	Line((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, p);
}

void Dib24::Line(int x0, int y0, int x1, int y1, const Dib24::Pixel &p) {
	const int dx = cMath::Abs(x1 - x0), dy = cMath::Abs(y1 - y0);

	const int sx = x1 >= x0 ? 1 : -1;
	const int sy = y1 >= y0 ? 1 : -1;

	if(dy <= dx) {
		int d1 = dy << 1;
		int d = d1 - dx;
		int d2 = (dy - dx) << 1;
		SetPixel(x0, y0, p);
		for(int x = x0 + sx, y = y0, i = 1; i <= dx; i++, x += sx) {
			if(d > 0) {
				d += d2;
				y += sy;
			} else d += d1;
			SetPixel(x, y, p);
		}
	} else {
		int d1 = dx << 1;
		int d = d1 - dy;
		int d2 = (dx - dy) << 1;
		SetPixel(x0, y0, p);
		for(int x = x0, y = y0 + sy, i = 1; i <= dy; i++, y += sy) {
			if(d > 0) {
				d += d2;
				x += sx;
			} else d += d1;
			SetPixel(x, y, p);
		}
	}
}

BYTE Chars8x16[95][16] = {
	{   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}, //   //
	{   0,   0,   0,   0,  24,  24,   0,  24,  24,  24,  60,  60,  60,  24,   0,   0}, // ! //
	{   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  36, 102, 102, 102,   0}, // " //
	{   0,   0,   0,   0,  54,  54, 127,  54,  54,  54, 127,  54,  54,   0,   0,   0}, // # //
	{   0,   0,   0,  24,  24,  62,  99,  97,  96,  62,   3,  67,  99,  62,  24,  24}, // $ //
	{   0,   0,   0,   0,  97,  99,   6,  12,  24,  48,  99,  67,   0,   0,   0,   0}, // % //
	{   0,   0,   0,   0, 110,  51,  51,  51,  59, 110,  28,  54,  54,  28,   0,   0}, // & //
	{   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   6,  12,  12,  12,   0}, // ' //
	{   0,   0,   0,   0,  48,  24,  12,  12,  12,  12,  12,  12,  24,  48,   0,   0}, // ( //
	{   0,   0,   0,   0,  12,  24,  48,  48,  48,  48,  48,  48,  24,  12,   0,   0}, // ) //
	{   0,   0,   0,   0,   0,   0, 102,  60, 255,  60, 102,   0,   0,   0,   0,   0}, // * //
	{   0,   0,   0,   0,   0,   0,  24,  24, 126,  24,  24,   0,   0,   0,   0,   0}, // + //
	{   0,   0,   0,  12,  24,  24,  24,   0,   0,   0,   0,   0,   0,   0,   0,   0}, // , //
	{   0,   0,   0,   0,   0,   0,   0,   0, 127,   0,   0,   0,   0,   0,   0,   0}, // - //
	{   0,   0,   0,   0,  24,  24,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}, // . //
	{   0,   0,   0,   0,   1,   3,   6,  12,  24,  48,  96,  64,   0,   0,   0,   0}, // / //
	{   0,   0,   0,   0,  62,  99,  99, 103, 107, 107, 115,  99,  99,  62,   0,   0}, // 0 //
	{   0,   0,   0,   0, 126,  24,  24,  24,  24,  24,  24,  30,  28,  24,   0,   0}, // 1 //
	{   0,   0,   0,   0, 127,  99,   3,   6,  12,  24,  48,  96,  99,  62,   0,   0}, // 2 //
	{   0,   0,   0,   0,  62,  99,  96,  96,  96,  60,  96,  96,  99,  62,   0,   0}, // 3 //
	{   0,   0,   0,   0, 120,  48,  48,  48, 127,  51,  54,  60,  56,  48,   0,   0}, // 4 //
	{   0,   0,   0,   0,  62,  99,  96,  96, 112,  63,   3,   3,   3, 127,   0,   0}, // 5 //
	{   0,   0,   0,   0,  62,  99,  99,  99,  99,  63,   3,   3,   6,  28,   0,   0}, // 6 //
	{   0,   0,   0,   0,  12,  12,  12,  12,  24,  48,  96,  96,  99, 127,   0,   0}, // 7 //
	{   0,   0,   0,   0,  62,  99,  99,  99,  99,  62,  99,  99,  99,  62,   0,   0}, // 8 //
	{   0,   0,   0,   0,  30,  48,  96,  96,  96, 126,  99,  99,  99,  62,   0,   0}, // 9 //
	{   0,   0,   0,   0,   0,  24,  24,   0,   0,   0,  24,  24,   0,   0,   0,   0}, // : //
	{   0,   0,   0,   0,  12,  24,  24,   0,   0,   0,  24,  24,   0,   0,   0,   0}, // ; //
	{   0,   0,   0,   0,  96,  48,  24,  12,   6,  12,  24,  48,  96,   0,   0,   0}, // < //
	{   0,   0,   0,   0,   0,   0, 127,   0,   0, 127,   0,   0,   0,   0,   0,   0}, // = //
	{   0,   0,   0,   0,   6,  12,  24,  48,  96,  48,  24,  12,   6,   0,   0,   0}, // > //
	{   0,   0,   0,   0,  24,  24,   0,  24,  24,  24,  48,  99,  99,  62,   0,   0}, // ? //
	{   0,   0,   0,   0,  62,   3,  59, 123, 123, 123,  99,  99,  62,   0,   0,   0}, // @ //
	{   0,   0,   0,   0,  99,  99,  99,  99, 127,  99,  99,  54,  28,   8,   0,   0}, // A //
	{   0,   0,   0,   0,  63, 102, 102, 102, 102,  62, 102, 102, 102,  63,   0,   0}, // B //
	{   0,   0,   0,   0,  60, 102,  67,   3,   3,   3,   3,  67, 102,  60,   0,   0}, // C //
	{   0,   0,   0,   0,  31,  54, 102, 102, 102, 102, 102, 102,  54,  31,   0,   0}, // D //
	{   0,   0,   0,   0, 127, 102,  70,   6,  22,  30,  22,  70, 102, 127,   0,   0}, // E //
	{   0,   0,   0,   0,  15,   6,   6,   6,  22,  30,  22,  70, 102, 127,   0,   0}, // F //
	{   0,   0,   0,   0,  92, 102,  99,  99, 123,   3,   3,  67, 102,  60,   0,   0}, // G //
	{   0,   0,   0,   0,  99,  99,  99,  99,  99, 127,  99,  99,  99,  99,   0,   0}, // H //
	{   0,   0,   0,   0,  60,  24,  24,  24,  24,  24,  24,  24,  24,  60,   0,   0}, // I //
	{   0,   0,   0,   0,  30,  51,  51,  51,  48,  48,  48,  48,  48, 120,   0,   0}, // J //
	{   0,   0,   0,   0, 103, 102, 102,  54,  30,  30,  54,  54, 102, 103,   0,   0}, // K //
	{   0,   0,   0,   0, 127, 102,  70,   6,   6,   6,   6,   6,   6,  15,   0,   0}, // L //
	{   0,   0,   0,   0,  99,  99,  99,  99,  99, 107, 127, 127, 119,  99,   0,   0}, // M //
	{   0,   0,   0,   0,  99,  99,  99,  99, 115, 123, 127, 111, 103,  99,   0,   0}, // N //
	{   0,   0,   0,   0,  28,  54,  99,  99,  99,  99,  99,  99,  54,  28,   0,   0}, // O //
	{   0,   0,   0,   0,  15,   6,   6,   6,   6,  62, 102, 102, 102,  63,   0,   0}, // P //
	{   0,   0, 112,  48,  62, 123, 107,  99,  99,  99,  99,  99,  99,  62,   0,   0}, // Q //
	{   0,   0,   0,   0, 103, 102, 102, 102,  54,  62, 102, 102, 102,  63,   0,   0}, // R //
	{   0,   0,   0,   0,  62,  99,  99,  96,  48,  28,   6,  99,  99,  62,   0,   0}, // S //
	{   0,   0,   0,   0,  60,  24,  24,  24,  24,  24,  24,  90, 126, 126,   0,   0}, // T //
	{   0,   0,   0,   0,  62,  99,  99,  99,  99,  99,  99,  99,  99,  99,   0,   0}, // U //
	{   0,   0,   0,   0,   8,  28,  54,  99,  99,  99,  99,  99,  99,  99,   0,   0}, // V //
	{   0,   0,   0,   0,  54,  54, 127, 107, 107,  99,  99,  99,  99,  99,   0,   0}, // W //
	{   0,   0,   0,   0,  99,  99,  54,  54,  28,  28,  54,  54,  99,  99,   0,   0}, // X //
	{   0,   0,   0,   0,  60,  24,  24,  24,  24,  60, 102, 102, 102, 102,   0,   0}, // Y //
	{   0,   0,   0,   0, 127,  99,  67,   6,  12,  24,  48,  97,  99, 127,   0,   0}, // Z //
	{   0,   0,   0,   0,  60,  12,  12,  12,  12,  12,  12,  12,  12,  60,   0,   0}, // [ //
	{   0,   0,   0,   0,  64,  96, 112,  56,  28,  14,   7,   3,   1,   0,   0,   0}, // \ //
	{   0,   0,   0,   0,  60,  48,  48,  48,  48,  48,  48,  48,  48,  60,   0,   0}, // ] //
	{   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  99,  54,  28,   8}, // ^ //
	{   0,   0, 255,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}, // _ //
	{   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  24,  12,  12}, // ` //
	{   0,   0,   0,   0, 110,  51,  51,  51,  62,  48,  30,   0,   0,   0,   0,   0}, // a //
	{   0,   0,   0,   0,  59, 102, 102, 102, 102,  54,  30,   6,   6,   7,   0,   0}, // b //
	{   0,   0,   0,   0,  62,  99,   3,   3,   3,  99,  62,   0,   0,   0,   0,   0}, // c //
	{   0,   0,   0,   0, 110,  51,  51,  51,  51,  54,  60,  48,  48,  56,   0,   0}, // d //
	{   0,   0,   0,   0,  62,  99,   3,   3, 127,  99,  62,   0,   0,   0,   0,   0}, // e //
	{   0,   0,   0,   0,  15,   6,   6,   6,   6,  15,   6,  38,  54,  28,   0,   0}, // f //
	{   0,  30,  51,  48,  62,  51,  51,  51,  51,  51, 110,   0,   0,   0,   0,   0}, // g //
	{   0,   0,   0,   0, 103, 102, 102, 102, 102, 110,  54,   6,   6,   7,   0,   0}, // h //
	{   0,   0,   0,   0,  60,  24,  24,  24,  24,  24,  28,   0,  24,  24,   0,   0}, // i //
	{   0,  60, 102, 102,  96,  96,  96,  96,  96,  96, 112,   0,  96,  96,   0,   0}, // j //
	{   0,   0,   0,   0, 103, 102,  54,  30,  30,  54, 102,   6,   6,   7,   0,   0}, // k //
	{   0,   0,   0,   0,  60,  24,  24,  24,  24,  24,  24,  24,  24,  28,   0,   0}, // l //
	{   0,   0,   0,   0, 107, 107, 107, 107, 107, 127,  55,   0,   0,   0,   0,   0}, // m //
	{   0,   0,   0,   0, 102, 102, 102, 102, 102, 102,  59,   0,   0,   0,   0,   0}, // n //
	{   0,   0,   0,   0,  62,  99,  99,  99,  99,  99,  62,   0,   0,   0,   0,   0}, // o //
	{   0,  15,   6,   6,  62, 102, 102, 102, 102, 102,  59,   0,   0,   0,   0,   0}, // p //
	{   0, 120,  48,  48,  62,  51,  51,  51,  51,  51, 110,   0,   0,   0,   0,   0}, // q //
	{   0,   0,   0,   0,  15,   6,   6,   6,  70, 110,  59,   0,   0,   0,   0,   0}, // r //
	{   0,   0,   0,   0,  62,  99,  48,  28,   6,  99,  62,   0,   0,   0,   0,   0}, // s //
	{   0,   0,   0,   0,  56, 108,  12,  12,  12,  12,  63,  12,  12,   8,   0,   0}, // t //
	{   0,   0,   0,   0, 110,  51,  51,  51,  51,  51,  51,   0,   0,   0,   0,   0}, // u //
	{   0,   0,   0,   0,  24,  60, 102, 102, 102, 102, 102,   0,   0,   0,   0,   0}, // v //
	{   0,   0,   0,   0,  54, 127, 107, 107,  99,  99,  99,   0,   0,   0,   0,   0}, // w //
	{   0,   0,   0,   0,  99,  54,  28,  28,  28,  54,  99,   0,   0,   0,   0,   0}, // x //
	{   0,  31,  48,  96, 126,  99,  99,  99,  99,  99,  99,   0,   0,   0,   0,   0}, // y //
	{   0,   0,   0,   0, 127,  99,   6,  12,  24,  51, 127,   0,   0,   0,   0,   0}, // z //
	{   0,   0,   0,   0, 112,  24,  24,  24,  24,  14,  24,  24,  24, 112,   0,   0}, // { //
	{   0,   0,   0,   0,  24,  24,  24,  24,  24,   0,  24,  24,  24,  24,   0,   0}, // | //
	{   0,   0,   0,   0,  14,  24,  24,  24,  24, 112,  24,  24,  24,  14,   0,   0}, // } //
	{   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  59, 110,   0,   0}, // ~ //
};

int Dib24::GetStrWidth(const cStr &Str) const {
	return 8 * Str.Length();
}

int Dib24::GetStrHeight() const {
	return 16;
}

// Dib24::Char
void Dib24::Char(int c, int x, int y, COLORREF Color) {
	if(c < 32 || c > 126) return;
	if(!cMath::IsInRange(x, 0, int(m_bmi.bmiHeader.biWidth) - 8) ||
		!cMath::IsInRange(y, 0, int(m_bmi.bmiHeader.biHeight) - 16)) return;

	BYTE r = GetRValue(Color), g = GetGValue(Color), b = GetBValue(Color);
	BYTE *pRow = Chars8x16[c - 32] + 15;
	long index = m_nBytesPerScanLine * (m_bmi.bmiHeader.biHeight - y - 1) + 3 * x;
	BYTE *pBitsRow = &m_pBits[index];
	for(int iy = 0; iy < 16; iy++) {
		BYTE *pCurPixel = pBitsRow;
		for(int ix = 0; ix < 8; ix++) {
			if(*pRow & (1 << ix)) {
				pCurPixel[0] = b, pCurPixel[1] = g, pCurPixel[2] = r;
			}
			pCurPixel += 3;
		}
		pRow--;
		pBitsRow -= m_nBytesPerScanLine;
	}
} // Dib24::Char

// Dib24::Str
void Dib24::Str(const cStr &Str, int x, int y) {
	cColor Color = cColor::Black;
	int xCur = x;
	const char *pStr = Str.ToCharPtr();
	do {
		int c = *pStr++;
		if(!c) break;
		Char(c, xCur, y, Color.ToDword());
		xCur += 8;
	} while(true);
} // Dib24::Str

// Dib24::StrCenter
void Dib24::StrCenter(const cStr &Str, int cx, int cy) {
	int Lx = 8 * Str.Length(), Ly = 16;
	Dib24::Str(Str, cx - Lx / 2, cy - Ly / 2);
} // Dib24::StrCenter

//-----------------------------------------------------------------------------
// Dib24::SaveBMP : bool(const char *)
//-----------------------------------------------------------------------------
bool Dib24::SaveBMP(const char *pFileName) {
	cAssert(m_pBits);

	HANDLE hf = CreateFileW(comms::wchar_path(pFileName), GENERIC_READ | GENERIC_WRITE,
		(DWORD)0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, (HANDLE)nullptr);

	if(INVALID_HANDLE_VALUE == hf) {
		return false;
	}
	
	BITMAPFILEHEADER hdr;
	memset(&hdr, 0, sizeof(BITMAPFILEHEADER));
	hdr.bfType = 0x4d42;
	hdr.bfSize = sizeof(BITMAPFILEHEADER) + m_bmi.bmiHeader.biSize + m_bmi.bmiHeader.biSizeImage;
	hdr.bfOffBits = sizeof(BITMAPFILEHEADER) + m_bmi.bmiHeader.biSize;
	
	DWORD nBytesWritten;
	WriteFile(hf, (LPVOID)&hdr, sizeof(BITMAPFILEHEADER), (LPDWORD)&nBytesWritten, nullptr);
	WriteFile(hf, (LPVOID)&m_bmi.bmiHeader, sizeof(BITMAPINFOHEADER), (LPDWORD)&nBytesWritten, nullptr);
	WriteFile(hf, (LPSTR)m_pBits, m_bmi.bmiHeader.biSizeImage, (LPDWORD)&nBytesWritten, nullptr);

	CloseHandle(hf);

	return true;
}

//-----------------------------------------------------------------------------
// ConvertFrom32To24
//-----------------------------------------------------------------------------
void Dib24::ConvertFrom32To24() {
	if(m_bmi.bmiHeader.biBitCount != 32) return;

	m_bmi.bmiHeader.biBitCount = 24;
	int nOldBytesPerScanLine = m_nBytesPerScanLine;
	m_nBytesPerScanLine = 3 * m_bmi.bmiHeader.biWidth;
	int mod = m_nBytesPerScanLine % 4;
	if(mod) m_nBytesPerScanLine += 4 - mod;
	m_bmi.bmiHeader.biSizeImage = m_nBytesPerScanLine * m_bmi.bmiHeader.biHeight;
	
	BYTE *p32Bits = m_pBits;
	
	m_pBits = new BYTE[m_bmi.bmiHeader.biSizeImage];
	cAssert(m_pBits);
	
	COLORREF Clr;
	for(int y = 0; y < m_bmi.bmiHeader.biHeight; y++) {
		for(int x = 0; x < m_bmi.bmiHeader.biWidth; x++) {
			long index = nOldBytesPerScanLine * (m_bmi.bmiHeader.biHeight - y - 1) + 4 * x;
			Clr = RGB(p32Bits[index + 2], p32Bits[index + 1], p32Bits[index]);
			SetPixel(x, y, Dib24::Pixel::FromBgr(Clr));
		}
	}
	delete[] p32Bits;
	p32Bits = nullptr;
}

//-----------------------------------------------------------------------------
// Dib24::LoadBMP : bool(const char *)
//-----------------------------------------------------------------------------
bool Dib24::LoadBMP(const char *pFileName) {
	Free();
	HANDLE hf = CreateFileW(comms::wchar_path(pFileName), GENERIC_READ, (DWORD)0, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)nullptr);
	
	if(INVALID_HANDLE_VALUE == hf) {
		return false;
	}

	bool fSuccess = false;

	BITMAPFILEHEADER hdr;
	memset(&hdr, 0, sizeof(BITMAPFILEHEADER));
	DWORD nBytesRead;
	ReadFile(hf, (LPVOID)&hdr, sizeof(BITMAPFILEHEADER), (LPDWORD)&nBytesRead, nullptr);
	if(nBytesRead == sizeof(BITMAPFILEHEADER)) {
		if(0x4d42 == hdr.bfType) {
			ReadFile(hf, (LPVOID)&m_bmi.bmiHeader, sizeof(BITMAPINFOHEADER), (LPDWORD)&nBytesRead, nullptr);
			if(nBytesRead == sizeof(BITMAPINFOHEADER)) {
				// Only 24 or 32 bits per pixel!
				if(m_bmi.bmiHeader.biBitCount == 24 || m_bmi.bmiHeader.biBitCount == 32) {
					m_nBytesPerScanLine = m_bmi.bmiHeader.biBitCount / 8 * m_bmi.bmiHeader.biWidth;
					int mod = m_nBytesPerScanLine % 4;
					if(mod) m_nBytesPerScanLine += 4 - mod;
					m_bmi.bmiHeader.biSizeImage = m_nBytesPerScanLine * m_bmi.bmiHeader.biHeight;

					m_pBits = new BYTE[m_bmi.bmiHeader.biSizeImage];
					cAssert(m_pBits);
					
					ReadFile(hf, (LPSTR)m_pBits, m_bmi.bmiHeader.biSizeImage, (LPDWORD)&nBytesRead, nullptr);
					if(m_bmi.bmiHeader.biSizeImage == nBytesRead) {
						// We don't need alpha:
						if(m_bmi.bmiHeader.biBitCount == 32) {
							ConvertFrom32To24();
						}
						fSuccess = true;
					}
				}
			}
		}
	}
	CloseHandle(hf);

	if(!fSuccess) {
		Free();
	}

	return fSuccess;
}

//-----------------------------------------------------------------------------
// Dib24::Copy : Dib24 & (const Dib24 &)
//-----------------------------------------------------------------------------
Dib24 & Dib24::Copy(const Dib24 &Src) {
	Free();
	memcpy(&m_bmi, &Src.m_bmi, sizeof(m_bmi));
	m_nBytesPerScanLine = Src.m_nBytesPerScanLine;
	if(Src.m_pBits) {
		m_pBits = new BYTE[Src.m_bmi.bmiHeader.biSizeImage];
		cAssert(m_pBits);
		memcpy(m_pBits, Src.m_pBits, Src.m_bmi.bmiHeader.biSizeImage);
	}
	return *this;
}

//-----------------------------------------------------------------------------
// Dib24::Scale
//-----------------------------------------------------------------------------
void Dib24::Scale(int s) {
	Dib24 dib(GetWidth() / s, GetHeight() / s);
	for(int y = 0; y < dib.GetHeight(); y++) {
		for(int x = 0; x < dib.GetWidth(); x++) {
			Dib24::Pixel C = GetBlot(s * x, s * y, s);
			dib.SetPixel(x, y, C);
		}
	}
	Copy(dib);
}

//-----------------------------------------------------------------------------
// Dib24::Paste
//-----------------------------------------------------------------------------
Dib24 & Dib24::Paste(int WhereX, int WhereY, const Dib24 &Src) {
	for(int y = WhereY; y < cMath::Min(GetHeight(), WhereY + Src.GetHeight()); y++) {
		for(int x = WhereX; x < cMath::Min(GetWidth(), WhereX + Src.GetWidth()); x++) {
			SetPixel(x, y, Src.GetPixel(x - WhereX, y - WhereY));
		}
	}
	return *this;
}

//-----------------------------------------------------------------------------
// Dib24::Sharp : void(const float)
//-----------------------------------------------------------------------------
void Dib24::Sharp(const float Sharpness) {
	Dib24 Buffer;
	Buffer.Copy(*this);
	for(int y = 1; y < GetHeight(); y++) {
		for(int x = 1; x < GetWidth(); x++) {
			cColor c = GetPixel(x, y).ToColor();
			cColor c00 = GetPixel(x - 1, y - 1).ToColor();
			cColor c01 = GetPixel(x + 1, y - 1).ToColor();
			cColor c10 = GetPixel(x - 1, y + 1).ToColor();
			cColor c11 = GetPixel(x + 1, y + 1).ToColor();
			cColor cc = (c00 + c01 + c10 + c11) / 4.0f;
			cColor r = (c - cc * Sharpness) / (1.0f - Sharpness);
			Buffer.SetPixel(x, y, Dib24::Pixel::FromColor(r));
		}
	}
	Copy(Buffer);
}

void Dib24::DrawLineHorFlat(int x0, int x1, int y, float z0, float z1, const Dib24::Pixel &p, float *pZBuffer) {
	if(y < 0 || y >= m_bmi.bmiHeader.biHeight) {
		return;
	}
	if(x0 == x1) {
		SetPixel(x0, y, p, z0, pZBuffer);
		return;
	}
	float k = (z1 - z0) / (float)(x1 - x0);
	if(x0 < x1) {
		for(int x = x0 + 1; x < x1; x++) {
			float z = z0 + (float)(x - x0) * k;
			SetPixel(x, y, p, z, pZBuffer);
		}
	}
	if(x0 > x1) {
		for(int x = x1 + 1; x < x0; x++) {
			float z = z0 + (float)(x - x0) * k;
			SetPixel(x, y, p, z, pZBuffer);
		}
	}
	SetPixel(x0, y, p, z0, pZBuffer);
	SetPixel(x1, y, p, z1, pZBuffer);
}

void Dib24::DrawPolygonFlat(const cVec3 *pVerts, const int Num, const Dib24::Pixel &p, float *pZBuffer) {
	int yMin = (int)pVerts[0].y;
	int yMax = (int)pVerts[0].y;

	int i;
	for(i = 1; i < Num; i++) {
		yMin = cMath::Min(yMin, (int)pVerts[i].y);
		yMax = cMath::Max(yMax, (int)pVerts[i].y);
	}
	float x[4], z[4];
	int y, nhor, st, en;
	for(y = yMin; y <= yMax; y++) {
		nhor = 0;
		for(i = 0; i < Num; i++) {
			st = i;
			en = i + 1;
			if(en >= Num) {
				en = 0;
			}
			float x1 = pVerts[st].x;
			float y1 = pVerts[st].y;
			float z1 = pVerts[st].z;
			float x2 = pVerts[en].x;
			float y2 = pVerts[en].y;
			float z2 = pVerts[en].z;
			if(y >= y1 && y < y2 || y <= y1 && y > y2) {
				if(y1 != y2) {
					x[nhor] = x1 + (x2 - x1) * (float)(y - y1) / (float)(y2  - y1);
					z[nhor] = z1 + (z2 - z1) * (float)(y - y1) / (float)(y2 - y1);
					nhor++;
				}
			}
			if(nhor == 2) {
				DrawLineHorFlat((int)x[0], (int)x[1], y, z[0], z[1], p, pZBuffer);
			}
		}
	}
}

const cColor Dib24::CalcColor(const Dib24_Material &Mt, const float Dot) {
	return cColor::Saturate(Mt.Ambient + Dot * Mt.Diffuse + Mt.Specular * cMath::Pow(Dot, Mt.Shininess));
}

//------------------------------------------------------------------------------------------------------------------------------------
// Dib24::DrawLineHorGouraud
//------------------------------------------------------------------------------------------------------------------------------------
void Dib24::DrawLineHorGouraud(int x0, int x1, int y, float z0, float z1, float d0, float d1, const Dib24_Material &Mt, float *pZBuffer) {
	if(y < 0 || y >= m_bmi.bmiHeader.biHeight) {
		return;
	}
	if(x0 == x1) {
		SetPixel(x0, y, Dib24::Pixel::FromColor(CalcColor(Mt, d0)), z0, pZBuffer);
		return;
	}
	float kz = (z1 - z0) / (float)(x1 - x0);
	float kd = (d1 - d0) / (float)(x1 - x0);
	if(x0 < x1) {
		for(int x = x0; x < x1; x++) {
			float z = z0 + (float)(x - x0) * kz;
			float d = d0 + (float)(x - x0) * kd;
			SetPixel(x, y, Dib24::Pixel::FromColor(CalcColor(Mt, d)), z, pZBuffer);
		}
	}
	if(x0 > x1) {
		for(int x = x1; x < x0; x++) {
			float z = z0 + (float)(x - x0) * kz;
			float d = d0 + (float)(x - x0) * kd;
			SetPixel(x, y, Dib24::Pixel::FromColor(CalcColor(Mt, d)), z, pZBuffer);
		}
	}
} // Dib24::DrawLineHorGouraud

//-----------------------------------------------------------------------------------------------------------------------------
// Dib24::DrawPolygonGouraud
//-----------------------------------------------------------------------------------------------------------------------------
void Dib24::DrawPolygonGouraud(const cVec3 *pVerts, const float *pDots, const int Num, const Dib24_Material &Mt, float *pZBuffer) {
	int yMin = (int)pVerts[0].y;
	int yMax = (int)pVerts[0].y;

	int i;

	for(i = 1; i < Num; i++) {
		yMin = cMath::Min(yMin, (int)pVerts[i].y);
		yMax = cMath::Max(yMax, (int)pVerts[i].y);
	}

	float x[4], z[4], d[4];
	int y, nhor, st, en;

	for(y = yMin; y <= yMax; y++) {
		nhor = 0;
		for(i = 0; i < Num; i++) {
			st = i;
			en = i + 1;
			if(en >= Num) {
				en = 0;
			}
			float y0 = pVerts[st].y;
			float y1 = pVerts[en].y;
			if(y >= y0 && y < y1 || y <= y0 && y > y1) {
				if(y0 != y1) {
					float x0 = pVerts[st].x;
					float z0 = pVerts[st].z;
					float d0 = pDots[st];
					float x1 = pVerts[en].x;
					float z1 = pVerts[en].z;
					float d1 = pDots[en];
					x[nhor] = x0 + (x1 - x0) * ((float)y - y0) / (y1 - y0);
					z[nhor] = z0 + (z1 - z0) * ((float)y - y0) / (y1 - y0);
					d[nhor] = d0 + (d1 - d0) * ((float)y - y0) / (y1 - y0);
					nhor++;
				}
			}
			if(nhor == 2) {
				DrawLineHorGouraud((int)x[0], (int)x[1], y, z[0], z[1], d[0], d[1], Mt, pZBuffer);
			}
		}
	}
} // Dib24::DrawPolygonGouraud

} // cWin32
} // comms

#endif // COMMS_WINDWS
