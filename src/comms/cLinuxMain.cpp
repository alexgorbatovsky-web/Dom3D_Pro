#include "comms.h"

#ifdef COMMS_LINUX

static comms::cStr cLinuxMain_CmdLineArgs;
// cLinuxMain_ParseCmdLineArgs
static void cLinuxMain_ParseCmdLineArgs(int argc, char** argv) {
    if(argc > 1) {
        int i;
        for(i = 1; i < argc; i++) {
            if(!cLinuxMain_CmdLineArgs.IsEmpty()) {
                cLinuxMain_CmdLineArgs += " ";
            }
            cLinuxMain_CmdLineArgs.Append(argv[i]);
        }
    }
}

// cMain_GetCmdLineArgs
const comms::cStr comms::cMain_GetCmdLineArgs() {
    return cLinuxMain_CmdLineArgs;
}

#ifndef COMMS_CONSOLE

// cLinuxMain_Tablet
class cLinuxMain_Tablet {
public:
    static bool PenPressed, EraserUsed;
    static float CurPressure;
    static comms::cVec2 CurTilt;
    // ExtractPressure : (GdkEventButton *)
    static void ExtractPressure(GdkEventButton *Event) {
    	ExtractPressure(Event->device, Event->axes);
    }
    // ExtractPressure : (GdkEventMotion *)
    static void ExtractPressure(GdkEventMotion *Event) {
    	ExtractPressure(Event->device, Event->axes);
    }
	// ExtractPressure : (GdkDevice *, gdouble *)
	static void ExtractPressure(GdkDevice *Device, gdouble *Axes) {
		float l;
		if(ExtractAxis(Device, Axes, GDK_AXIS_PRESSURE, &l)) {
			CurPressure = l;
		}
		if(ExtractAxis(Device, Axes, GDK_AXIS_XTILT, &l)) {
			CurTilt.x = comms::cMath::Lerp(-1.0f, 1.0f, l);
		}
		if(ExtractAxis(Device, Axes, GDK_AXIS_YTILT, &l)) {
			CurTilt.y = comms::cMath::Lerp(-1.0f, 1.0f, l);
		}
	}
    // ExtractAxis : bool (GdkDevice *, gdouble *, GdkAxisUse, float *)
    static bool ExtractAxis(GdkDevice *Device, gdouble *Axes, GdkAxisUse Axis, float *Lerper) {
    	bool r = false;
    	gdouble P;
    	int i;
    	float Lo = 0.0f, Hi = 1.0f;
    	if(gdk_device_get_axis(Device, Axes, Axis, &P)) {
    		for(i = 0; i < Device->num_axes; i++) {
    			const GdkDeviceAxis &X = Device->axes[i];
    			if(X.use == Axis) {
    				Lo = (float)X.min;
    				Hi = (float)X.max;
    			}
    		}
    		*Lerper = comms::cMath::LerperClamp01(Lo, Hi, P);
    		r = true;
    	}
    	return r;
    }
};
bool cLinuxMain_Tablet::PenPressed = false;
bool cLinuxMain_Tablet::EraserUsed = false;
float cLinuxMain_Tablet::CurPressure = 0.0f;
comms::cVec2 cLinuxMain_Tablet::CurTilt = comms::cVec2::Zero;

namespace comms {

// cMain_GetTabletState
void cMain_GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastUsedTime) {
	if(CurPressure != nullptr) {
		*CurPressure = 650.0f * cLinuxMain_Tablet::CurPressure;
	}
	if(PenPressed != nullptr) {
        *PenPressed = cLinuxMain_Tablet::PenPressed;
    }
    if(EraserUsed != nullptr) {
        *EraserUsed = cLinuxMain_Tablet::EraserUsed;
    }
	if(LastUsedTime != nullptr) {
		*LastUsedTime = -1;
	}
}

} // comms

#ifdef COMMS_3DCOAT
extern bool IsInExitState;
extern bool IgnoreSystemPause;
#else // COMMS_3DCOAT
bool IsInExitState = false;
bool IgnoreSystemPause = false;
#endif // COMMS_3DCOAT

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/XKBlib.h>
#include <dirent.h>
#include <sys/stat.h>
#undef None // It is defined as macro inside "/usr/include/X11/X.h"

#ifdef COMMS_CURL
#include "curl.h"
#endif // COMMS_CURL

#ifdef COMMS_OPENGL
#include <GL/glx.h>
static GLXDrawable cLinuxMain_Drawable = 0;
static GLXContext cLinuxMain_OglContext = nullptr;
// cLinuxMain_GetDrawable
GLXDrawable cLinuxMain_GetDrawable() {
    return cLinuxMain_Drawable;
}
#endif // COMMS_OPENGL

static Display *cLinuxMain_Display = nullptr;
static GtkWidget *cLinuxMain_Window = nullptr;
static GtkWidget *cLinuxMain_DrawingArea = nullptr;
static bool cLinuxMain_FullScreen = false;
static comms::cList<int> cLinuxMain_KeySyms, cLinuxMain_Codes;
static comms::cList<int> cLinuxMain_EnterCodes, cLinuxMain_ShiftCodes, cLinuxMain_ControlCodes, cLinuxMain_AltCodes;
static comms::cList<bool> cLinuxMain_Repeated;
std::mutex cLinuxMain_GetKeysMutex; // Since "XQueryKeymap" is not thread safe
static bool cLinuxMain_Close = false;
static GdkDevice *cLinuxMain_Stylus = nullptr, *cLinuxMain_Eraser = nullptr, *cLinuxMain_Mouse = nullptr;

static int cLinuxMain_CurCursorIndex = -1;
static GdkCursor *cLinuxMain_Cursors[12] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

// cLinuxMain_GetDisplay
Display * cLinuxMain_GetDisplay() {
    return cLinuxMain_Display;
}

// cLinuxMain_GetWindow
GtkWidget * cLinuxMain_GetWindow() {
    return cLinuxMain_Window;
}

// cLinuxMain_SetWindow
void cLinuxMain_SetWindow(GtkWidget *Window) {
	cLinuxMain_Window = Window;
}

static void cLinuxMain_WhileEvents() {
    while(gtk_events_pending()) {
        gtk_main_iteration();
    }
}

namespace comms {

// cMain_GetClientWidth
int cMain_GetClientWidth() {
    int W = 0;
    if(cLinuxMain_DrawingArea != nullptr) {
        W = cLinuxMain_DrawingArea->allocation.width;
    }
    return W;
}

// cMain_GetClientHeight
int cMain_GetClientHeight() {
    int H = 0;
    if(cLinuxMain_DrawingArea != nullptr) {
        H = cLinuxMain_DrawingArea->allocation.height;
    }
    return H;
}

} // comms

static void cLinuxMain_RepeatedClear() {
    int i;
    for(i = 0; i < cLinuxMain_Repeated.Count(); i++) {
        if(!cLinuxMain_Repeated[i]) {
            continue;
        }
        cLinuxMain_Repeated[i] = false;
        comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
        E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
        E->Code = i;
        E->Pressed = false;
        comms::cInput::AddEvent(E);
        E = nullptr;
    }
}

static PangoFontDescription *cLinuxMain_FontDesc = nullptr;
static comms::cStr cLinuxMain_FontName;
static bool cLinuxMain_FontChanged;

// cLinuxMain_FontInit
static void cLinuxMain_FontInit() {
    if(cLinuxMain_FontDesc != nullptr) {
        return;
    }
    cLinuxMain_FontName = "Sans 72";
    cLinuxMain_FontDesc = pango_font_description_from_string(cLinuxMain_FontName.ToCharPtr());
    cLinuxMain_FontChanged = true;
}

namespace comms {

// cMain_GetFontChanged
bool cMain_GetFontChanged() {
    cLinuxMain_FontInit();

    bool r = cLinuxMain_FontChanged;
    cLinuxMain_FontChanged = false;
    return r;
}

// cMain_GetFontName
const cStr cMain_GetFontName() {
    cLinuxMain_FontInit();
    return cLinuxMain_FontName;
}

// cMain_ChooseFont
void cMain_ChooseFont() {
    cLinuxMain_FontInit();

    GtkWidget *d = gtk_font_selection_dialog_new("Select Font");
    if(cLinuxMain_GetWindow() != nullptr) {
        gtk_window_set_transient_for(GTK_WINDOW(d), GTK_WINDOW(cLinuxMain_GetWindow()));
    }
    gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(d), cLinuxMain_FontName.ToCharPtr());
    int r = gtk_dialog_run(GTK_DIALOG(d));
    if(r == GTK_RESPONSE_OK || r == GTK_RESPONSE_APPLY) {
        gchar *n = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(d));
        cLinuxMain_FontName = n;
        g_free(n);
        if(cLinuxMain_FontDesc != nullptr) {
            pango_font_description_free(cLinuxMain_FontDesc);
            cLinuxMain_FontDesc = nullptr;
        }
        cLinuxMain_FontDesc = pango_font_description_from_string(cLinuxMain_FontName.ToCharPtr());
        cLinuxMain_FontChanged = true;
    }
    gtk_widget_destroy(d);
    cLinuxMain_RepeatedClear();
}

//-----------------------------------------------------------------------------------------
// cMain_DrawText
//-----------------------------------------------------------------------------------------
void cMain_SetTextSize(const int Size) {
    cLinuxMain_FontInit();
    double S = PANGO_SCALE * (double)Size;
    pango_font_description_set_absolute_size(cLinuxMain_FontDesc, S);
}
int cMain_GetTextSize() {
    cLinuxMain_FontInit();
    int S = pango_font_description_get_size(cLinuxMain_FontDesc);
    S /= PANGO_SCALE;
    return S;
}
void cMain_DrawText(const char *Text, comms::cImage *To, int *TextWidth, int *TextHeight) {
    cLinuxMain_FontInit();

    int W, H, l, R, i;
    byte *Src = nullptr, *Dst = nullptr;
    PangoLayout *L = nullptr;
    GdkPixmap *PM = nullptr;
    GdkGC *gc = nullptr;
    GdkPixbuf *P = nullptr;
    // Measuring text
    L = gtk_widget_create_pango_layout(cLinuxMain_Window, Text);
    pango_layout_set_font_description(L, cLinuxMain_FontDesc);
    pango_layout_get_pixel_size(L, TextWidth, TextHeight);
    // Drawing text
    W = comms::cMath::Max(32, comms::cMath::UpperPowerOfTwo(*TextWidth));
    H = comms::cMath::Max(32, comms::cMath::UpperPowerOfTwo(*TextHeight));
    if(W > 16384) {
		W = 16384;
    }
	if(H > 4096) {
		H = 4096;
    }
    PM = gdk_pixmap_new(cLinuxMain_Window->window, W, H, -1);
    gc = gdk_gc_new(PM);
    gdk_gc_copy(gc, cLinuxMain_Window->style->black_gc);
    gdk_draw_rectangle(PM, gc, TRUE, 0, 0, W, H);
    gdk_gc_copy(gc, cLinuxMain_Window->style->white_gc);
    gdk_draw_layout(PM, gc, 0, 0, L);
    g_object_unref(gc);
    gc = nullptr;
    g_object_unref(L);
    L = nullptr;
    // Copy pixelmap
    P = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, W, H);
    gdk_pixbuf_get_from_drawable(P, PM, gdk_drawable_get_colormap(cLinuxMain_Window->window), 0, 0, 0, 0, W, H);
    g_object_unref(PM);
    PM = nullptr;
    Src = gdk_pixbuf_get_pixels(P);
    l = cFormat::BytesPerPixel(cFormat::Rgb8) * W;
    To->Create(cFormat::Rgb8, W, H, 1, 1);
    Dst = To->GetPixels();
    Dst += (H - 1) * l; // Flip
    R = gdk_pixbuf_get_rowstride(P);
    for(i = 0; i < H; i++) {
        memcpy(Dst, Src, l);
        Dst -= l;
        Src += R;
    }
    g_object_unref(P);
    P = nullptr;
    To->ToFormat(cFormat::Rgba8);
} // cMain_DrawText

// cLinuxMain_FileDialogFilter
static cList<cStr> cLinuxMain_FileDialogExtensions;
static bool cLinuxMain_ExeMode = false;
gboolean cLinuxMain_FileDialogFilter(const GtkFileFilterInfo *fi, gpointer) {
    bool r = false;
    cStr S, E;
    if(cLinuxMain_ExeMode) {
		// Field "fi->mime_type" is "nullptr" on many Linux distros, including the latest Ubuntus.
		// Therefore we should use "stat.st_mode" field. But Linux doesn't distinguish between
		// "executable" and "searchable" files. So file dialog for executable selection will
		// contain many non executable files, excluding the obvious ones, like text docs.
        struct stat st;
        if(0 == stat(fi->filename, &st)) {
            if(S_ISREG(st.st_mode) && (st.st_mode & 0111)) {
                r = true;
            }
        }
    } else {
        S = fi->filename;
        E = S.GetFileExtension();
        r = cLinuxMain_FileDialogExtensions.Contains(E, cStr::EqualsNoCase);
    }
    return r;
}

// https://docs.gtk.org/gtk3/class.FileChooserDialog.html
static bool cLinuxMain_SimpleFileOpen(const char *Title, cStr *FilePn) {
	GtkWidget *Dialog = gtk_file_chooser_dialog_new(Title, GTK_WINDOW(cLinuxMain_Window), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, nullptr);
	if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(Dialog))) {
		GtkFileChooser *Chooser = GTK_FILE_CHOOSER(Dialog);
		char *FileName = gtk_file_chooser_get_filename(Chooser);
		FilePn->Copy(FileName);
		g_free(FileName); FileName = nullptr;
	}
	gtk_widget_destroy(Dialog);	
	return !FilePn->IsEmpty();
}

// cLinuxMain_LoadFileDialog
bool cLinuxMain_LoadFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *SingleFilePn, cList<cStr> *MultiFilePn, const char *PrefKey, const char *InitialFileName) {
    cPause::SetSystemPause(true);
    GtkWidget *Dialog;
    Dialog = gtk_file_chooser_dialog_new(Title, GTK_WINDOW(cLinuxMain_Window), GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, nullptr);
    // Exe Mode
    cLinuxMain_ExeMode = false;
    int i;
    for(i = 0; i < Extensions.Count(); i++) {
        const cStr &S = Extensions[i];
        if(S.Contains("exe", true) || S.Contains("app", true)) {
            cLinuxMain_ExeMode = true;
            break;
        }
    }
    // Filter
    cLinuxMain_FileDialogExtensions = Extensions;
    GtkFileFilter *Filter = gtk_file_filter_new();
    gtk_file_filter_add_custom(Filter, GTK_FILE_FILTER_FILENAME, cLinuxMain_FileDialogFilter, nullptr, nullptr);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(Dialog), Filter);
    // Initial Folder
    cStr F = cIO::GetFileDialogInitialFolder(PrefKey);
    if(!F.IsEmpty()) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(Dialog), F.ToCharPtr());
    }
    // Initial File Name
    cStr N = InitialFileName;
    if(!N.IsEmpty()) {
        cStr P = F;
        P.AppendPath(N);
        P.BackSlashesToSlashes(); // '\' -> '/'
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(Dialog), P.ToCharPtr());
    }
    if(SingleFilePn != nullptr) {
        SingleFilePn->Clear();
    }
    if(MultiFilePn != nullptr) {
        MultiFilePn->Clear();
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(Dialog), TRUE);
    }
    bool r = (gtk_dialog_run(GTK_DIALOG(Dialog)) == GTK_RESPONSE_ACCEPT);
    if(r) {
        if(SingleFilePn != nullptr) {
            char *Filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(Dialog));
            SingleFilePn->Copy(Filename);
            g_free(Filename);
            Filename = nullptr;
            cIO::SetFileDialogInitialFolder(PrefKey, SingleFilePn->GetFilePath());
        }
        if(MultiFilePn != nullptr) {
            GSList *Filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(Dialog));
            GSList *Cur = Filenames;
            while(Cur != nullptr) {
                const char *Str = (const char *)Cur->data;
                MultiFilePn->Add(cStr(Str));
                g_free(Cur->data);
                Cur->data = nullptr;
                Cur = Cur->next;
            }
            g_slist_free(Filenames);
            Filenames = nullptr;
            cIO::SetFileDialogInitialFolder(PrefKey, MultiFilePn->GetAt(0).GetFilePath());
        }
    }
    gtk_widget_destroy(Dialog);
    cLinuxMain_RepeatedClear();
    return r;
}

// cLinuxMain_SaveFileDialog
bool cLinuxMain_SaveFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *FilePn, const char *PrefKey, const char *DefaultExtension, const char *InitialFileBase) {
    cPause::SetSystemPause(true);
    GtkWidget *Dialog;
    Dialog = gtk_file_chooser_dialog_new(Title, GTK_WINDOW(cLinuxMain_Window), GTK_FILE_CHOOSER_ACTION_SAVE,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, nullptr);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(Dialog), TRUE);
    // Exe Mode
    cLinuxMain_ExeMode = false;
    int i;
    for(i = 0; i < Extensions.Count(); i++) {
        const cStr &S = Extensions[i];
        if(S.Contains("exe", true) || S.Contains("app", true)) {
            cLinuxMain_ExeMode = true;
            break;
        }
    }
    // Filter
    cLinuxMain_FileDialogExtensions = Extensions;
    GtkFileFilter *Filter = gtk_file_filter_new();
    gtk_file_filter_add_custom(Filter, GTK_FILE_FILTER_FILENAME, cLinuxMain_FileDialogFilter, nullptr, nullptr);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(Dialog), Filter);
    // Initial Folder
    cStr F = cIO::GetFileDialogInitialFolder(PrefKey);
    if(!F.IsEmpty()) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(Dialog), F.ToCharPtr());
    }
    // Default extension goes first
    cStr Def = DefaultExtension;
    if(!Def.IsEmpty()) {
        i = cLinuxMain_FileDialogExtensions.IndexOf(Def, cStr::EqualsNoCase);
        if(i != -1 && i != 0) {
            cMath::Swap(cLinuxMain_FileDialogExtensions[0], cLinuxMain_FileDialogExtensions[i]);
        }
    }
    // Initial File Base
    cStr B = InitialFileBase;
    if(!B.IsEmpty()) {
        cStr P = F;
        P.AppendPath(B);
        P.BackSlashesToSlashes(); // '\' -> '/'
        cStr Ex = P.GetFileExtension();
        if(Ex.IsEmpty() || !Extensions.Contains(Ex, comms::cStr::EqualsNoCase)) {
            if(!Def.IsEmpty()) {
                P.SetFileExtension(Def);
            } else {
                P.SetFileExtension(Extensions[0]);
            }
        }
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(Dialog), P.ToCharPtr());
    }
    // Extensions ComboBox
    GtkWidget *cb = gtk_combo_box_new_text();
    for(i = 0; i < cLinuxMain_FileDialogExtensions.Count(); i++) {
        const cStr &S = cLinuxMain_FileDialogExtensions[i];
        gtk_combo_box_append_text(GTK_COMBO_BOX(cb), S.ToCharPtr());
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(cb), 0);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(Dialog), cb);
    // Run
    bool r = (gtk_dialog_run(GTK_DIALOG(Dialog)) == GTK_RESPONSE_ACCEPT);
    cStr Sel = gtk_combo_box_get_active_text(GTK_COMBO_BOX(cb));
    if(r) {
        char *Filename;
        Filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(Dialog));
        *FilePn = Filename;
        g_free(Filename);
        comms::cIO::SetFileDialogInitialFolder(PrefKey, FilePn->GetFilePath());
        comms::cStr Ex = FilePn->GetFileExtension();
        if(Ex.IsEmpty() || !Extensions.Contains(Ex, comms::cStr::EqualsNoCase)) {
            FilePn->SetFileExtension(Sel);
        }
    }
    gtk_widget_destroy(Dialog);
    cLinuxMain_RepeatedClear();
    return r;
}

// cLinuxMain_SelectFolderDialog
bool cLinuxMain_SelectFolderDialog(const char *Title, comms::cStr *SelectedFolder, const char *InitialFolder) {
    cPause::SetSystemPause(true);
    GtkWidget *Dialog = gtk_file_chooser_dialog_new(Title, GTK_WINDOW(cLinuxMain_Window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, nullptr);
    // Initial Folder
    cStr I = cIO::EnsureAbsolutePath(InitialFolder);
    if(!I.IsEmpty()) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(Dialog), I.ToCharPtr());
    }
    bool r = (gtk_dialog_run(GTK_DIALOG(Dialog)) == GTK_RESPONSE_ACCEPT);
    if(r) {
        char *Folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(Dialog));
        *SelectedFolder = Folder;
        g_free(Folder);
    }
    gtk_widget_destroy(Dialog);
    Dialog = nullptr;
    cLinuxMain_RepeatedClear();
    return r;
}

//*****************************************************************************
// Input String
//*****************************************************************************
static bool InputString_Enter, InputString_Close;
static GtkWidget *InputString_Wnd, *InputString_Edit;
static void InputString_Ctor() {
    InputString_Enter = false;
    InputString_Close = false;
    InputString_Wnd = nullptr;
    InputString_Edit = nullptr;
}
static cStr InputString_Result;

static void InputString_GetResult() {
    InputString_Close = true;
    InputString_Enter = true;
    InputString_Result = gtk_entry_get_text(GTK_ENTRY(InputString_Edit));
}

static gboolean InputString_KeyboardHandler(GtkWidget *Widget, GdkEventKey *Event, gpointer Data) {
	const int h = (int)Event->hardware_keycode;
    if(cLinuxMain_EnterCodes.Contains(h)) { // Return, Keypad Enter
        InputString_GetResult();
        return 1;
    }
    if(cLinuxMain_Codes[cInput::Esc] == h) { // Esc
        InputString_Close = true;
        return 1;
    }
    return 0;
}

// Alt+F4 handler
static void InputString_DeleteHandler() {
    InputString_Close = true;
    // Subsequent call to "gtk_window_is_active" will return "false".
}

static void InputString_ButtonClicked(GtkWidget *, gpointer) {
    InputString_GetResult();
}

int comms::cIO::GetInputPixelsWidth() {
    return 0; // Input dialog centers under mouse
}
int comms::cIO::GetInputPixelsHeight() {
    return 0; // Input dialog centers under mouse
}

bool cLinuxMain_InputString(cStr *S) {
    InputString_Ctor();
	// Wnd
	InputString_Wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(InputString_Wnd, "delete-event", G_CALLBACK(InputString_DeleteHandler), nullptr);
	gtk_window_set_decorated(GTK_WINDOW(InputString_Wnd), false);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(InputString_Wnd), true);
	gtk_window_set_transient_for(GTK_WINDOW(InputString_Wnd), GTK_WINDOW(cLinuxMain_Window));
    // Edit
	InputString_Edit = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(InputString_Edit), S->ToCharPtr());
    gtk_container_add(GTK_CONTAINER(InputString_Wnd), InputString_Edit);
    // Show all
    gtk_widget_show_all(InputString_Wnd);
    // Wnd size
    GdkRectangle rc;
    gtk_widget_get_allocation(InputString_Wnd, &rc);
    // Mouse position
    int X, Y; // Relative to the upper left corner of window
    gdk_window_get_pointer(gtk_widget_get_window(InputString_Wnd), &X, &Y, nullptr);
    // Center under mouse
    gtk_window_move(GTK_WINDOW(InputString_Wnd), X - rc.width / 2, Y - rc.height / 2);
    // Event loop
    g_signal_connect(G_OBJECT(InputString_Wnd), "key_press_event", G_CALLBACK(InputString_KeyboardHandler), nullptr);
    while(!InputString_Close) {
        cLinuxMain_WhileEvents();
        SleepMs(20);
    }
    gtk_widget_destroy(InputString_Wnd);
	bool r = InputString_Enter;
	if(r) {
		*S = InputString_Result;
	}
    InputString_Ctor(); // For the check inside "cLinuxMain_MouseButtonHandler"
    comms::cInput::SendMouseUpEvents();
    cLinuxMain_RepeatedClear();
    return r;
}

} // comms

// cLinuxMain_CheckProcessInMemory
static bool cLinuxMain_CheckProcessInMemory_IsNumeric(const char *ccharptr_CharacterList) {
    for(; *ccharptr_CharacterList; ccharptr_CharacterList++) {
        if((*ccharptr_CharacterList < '0') || (*ccharptr_CharacterList > '9')) {
            return false;
        }
    }
    return true;
}
bool cLinuxMain_CheckProcessInMemory(const char *ProcName) {
    DIR *DirProc = opendir("/proc/");
    if(nullptr == DirProc) {
        return false;
    }
    bool r = false;
    char CommandLinePath[100];
    comms::cStr Buffer(10 * 1024);
    struct dirent *DirEntity = nullptr;
    FILE *fp = nullptr;
    char c;
    int i;
    while((DirEntity = readdir(DirProc))) {
        if(DT_DIR == DirEntity->d_type) {
            if(cLinuxMain_CheckProcessInMemory_IsNumeric(DirEntity->d_name)) {
                strcpy(CommandLinePath, "/proc/");
                strcat(CommandLinePath, DirEntity->d_name);
                strcat(CommandLinePath, "/cmdline");
                fp = fopen(CommandLinePath, "rt");
                if(fp != nullptr) {
                    Buffer.Clear();
                    while(fscanf(fp, "%c", &c) > 0) {
                        Buffer.Append(c);
                    }
                    fclose(fp);
                    fp = nullptr;
                    if(Buffer.IsEmpty()) {
                        continue;
                    }
                    i = Buffer.LastIndexOf('/');
                    if(i != -1) {
                        Buffer.Remove(0, i + 1);
                    }
                    if(comms::cStr::EqualsNoCase(ProcName, Buffer)) {
                        r = true;
                        break;
                    }
                }
            }
        }
    }
    closedir(DirProc);
    DirProc = nullptr;
    return r;
}

// cInput::SetCursor
void comms::cInput::SetCursor(const cInput::Cursor::Enum Cursor) {
    int Index = (int)Cursor;
    if(-1 == Index) {
        Index = 0;
    }
    if(Index != cLinuxMain_CurCursorIndex) {
        gdk_window_set_cursor(cLinuxMain_Window->window, cLinuxMain_Cursors[Index]);
        cLinuxMain_CurCursorIndex = Index;
    }
}

// cInput::WarpCursor
void comms::cInput::WarpCursor(const int LocalX, const int LocalY, const bool DownY) {
    int i = LocalY;
    if(!DownY) {
        i = cLinuxMain_DrawingArea->allocation.height - LocalY;
    }
    int X, Y;
    gdk_window_get_origin(cLinuxMain_DrawingArea->window, &X, &Y);
    X += LocalX;
    Y += i;
    GdkDisplay *Display = gdk_display_get_default();
    GdkScreen *Screen = gdk_display_get_default_screen(Display);
    gdk_display_warp_pointer(Display, Screen, X, Y);
}

// cInput::EnableEvents
bool comms::cInput::EnableEvents() {
	return true;
}

// cInput::IsDownAcquire
bool comms::cInput::IsDownAcquire(const int Code) {
    if(comms::cInput::IsMouseButtonCode(Code)) {
		return comms::cInput::IsDown(Code);
    }

    if(Code < Esc || Code > Decimal) {
        return false;
    }

    int i0 = cLinuxMain_Codes[Code];
    if(-1 == i0) {
        return false;
    }

    char keys[32];
    cLinuxMain_GetKeysMutex.lock();
    XQueryKeymap(cLinuxMain_Display, keys);
    cLinuxMain_GetKeysMutex.unlock();

    bool r = ((keys[i0 >> 3] & (1 << (i0 & 7))) != 0);
    // Shift
    int i, h;
    if(!r && cInput::Shift == Code) {
        for(i = 0; i < cLinuxMain_ShiftCodes.Count(); i++) {
            h = cLinuxMain_ShiftCodes[i];
            r = ((keys[h >> 3] & (1 << (h & 7))) != 0);
            if(r) {
                break;
            }
        }
    }
    // Control
    if(!r && cInput::Control == Code) {
        for(i = 0; i < cLinuxMain_ControlCodes.Count(); i++) {
            h = cLinuxMain_ControlCodes[i];
            r = ((keys[h >> 3] & (1 << (h & 7))) != 0);
            if(r) {
                break;
            }
        }
    }
    // Alt
    if(!r && cInput::Alt == Code) {
        for(i = 0; i < cLinuxMain_AltCodes.Count(); i++) {
            h = cLinuxMain_AltCodes[i];
            r = ((keys[h >> 3] & (1 << (h & 7))) != 0);
            if(r) {
                break;
            }
        }
    }
    // Enter
    if(!r && cInput::Enter == Code) {
        for(i = 0; i < cLinuxMain_EnterCodes.Count(); i++) {
            h = cLinuxMain_EnterCodes[i];
            r = ((keys[h >> 3] & (1 << (h & 7))) != 0);
            if(r) {
                break;
            }
        }
    }

    return r;
}

//-----------------------------------------------------------------------------
// cInput::AcquireKeyboard
//-----------------------------------------------------------------------------
bool comms::cInput::AcquireKeyboard(comms::cInput::KeyboardState *S) {
    if(!gtk_window_is_active(GTK_WINDOW(cLinuxMain_Window))) { // Input only to active window
        S->Clear();
        return true;
    }
    
    char keys[32];
    cLinuxMain_GetKeysMutex.lock();
    XQueryKeymap(cLinuxMain_Display, keys);
    cLinuxMain_GetKeysMutex.unlock();
    
    int i, k;
    for(i = 0; i < cLinuxMain_Codes.Count(); i++) {
        k = cLinuxMain_Codes[i];
        S->IsDown[i] = ((keys[k >> 3] & (1 << (k & 7))) != 0);
    }

    // Shift
    bool *p = &S->IsDown[cInput::Shift];
    if(!(*p)) {
        for(i = 0; i < cLinuxMain_ShiftCodes.Count(); i++) {
            k = cLinuxMain_ShiftCodes[i];
            *p = ((keys[k >> 3] & (1 << (k & 7))) != 0);
            if(*p) {
                break;
            }
        }
    }
    // Control
    p = &S->IsDown[cInput::Control];
    if(!(*p)) {
        for(i = 0; i < cLinuxMain_ControlCodes.Count(); i++) {
            k = cLinuxMain_ControlCodes[i];
            *p = ((keys[k >> 3] & (1 << (k & 7))) != 0);
            if(*p) {
                break;
            }
        }
    }
    // Alt
    p = &S->IsDown[cInput::Alt];
    if(!(*p)) {
        for(i = 0; i < cLinuxMain_AltCodes.Count(); i++) {
            k = cLinuxMain_AltCodes[i];
            *p = ((keys[k >> 3] & (1 << (k & 7))) != 0);
            if(*p) {
                break;
            }
        }
    }
    // Enter
    p = &S->IsDown[cInput::Enter];
    if(!(*p)) {
        for(i = 0; i < cLinuxMain_EnterCodes.Count(); i++) {
            k = cLinuxMain_EnterCodes[i];
            *p = ((keys[k >> 3] & (1 << (k & 7))) != 0);
            if(*p) {
                break;
            }
        }
    }

    // Chars
    int r = XkbQueryExtension(cLinuxMain_Display, nullptr, nullptr, nullptr, nullptr, nullptr);
    XkbStateRec State;
    KeySym ks;
    char c[32];
    cStr T;
    int e, l;
    unsigned int um;
    if(r != 0) {
        XkbDescPtr Desc = XkbGetMap(cLinuxMain_Display, XkbAllComponentsMask, XkbUseCoreKbd);
        if(Desc != nullptr) {
            XkbGetState(cLinuxMain_Display, XkbUseCoreKbd, &State);
            for(i = 0; i < cLinuxMain_Codes.Count(); i++) {
                if(!XkbTranslateKeyCode(Desc, cLinuxMain_Codes[i], State.mods, &um, &ks)) {
                    continue;
                }
                l = XkbTranslateKeySym(cLinuxMain_Display, &ks, 0, c, 32, &e);
                if(l != 1) {
                    continue;
                }
                S->Chars[i] = c[0];
            }
            XkbFreeKeyboard(Desc, XkbAllComponentsMask, True);
            Desc = nullptr;
        }
    }
    return true;
} // cInput::AcquireKeyboard

// cMessageBox::YesNo
bool comms::cMessageBox::YesNo(const char *Caption, const char *Text, ...) {
	va_list args;
	va_start(args, Text);
	char temp[1024];
	vsnprintf(temp, 1024, Text, args);
	va_end(args);
	
    cPause::SetSystemPause(true);
    GtkWidget *Dlg = gtk_message_dialog_new(GTK_WINDOW(cLinuxMain_Window), (GtkDialogFlags)(GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL), GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO, "%s", Caption);
    gtk_message_dialog_format_secondary_text((GtkMessageDialog *)Dlg, "%s", temp);
    int r = gtk_dialog_run(GTK_DIALOG(Dlg));
    gtk_widget_destroy(Dlg);
    cLinuxMain_RepeatedClear();
    return GTK_RESPONSE_YES == r;
}

// cMessageBox::Ok
void comms::cMessageBox::Ok(const char *Caption, const char *Text, ...) {
	va_list args;
	va_start(args, Text);
	char temp[1024];
	vsnprintf(temp, 1024, Text, args);
	va_end(args);
	
	cPause::SetSystemPause(true);
    GtkWidget *Dlg = gtk_message_dialog_new(GTK_WINDOW(cLinuxMain_Window), (GtkDialogFlags)(GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL), GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", Caption);
    gtk_message_dialog_format_secondary_text((GtkMessageDialog *)Dlg, "%s", temp);
    gtk_dialog_run(GTK_DIALOG(Dlg));
    gtk_widget_destroy(Dlg);
    cLinuxMain_RepeatedClear();
}

// cLinuxMain_UnfullScreen
void cLinuxMain_UnfullScreen() {
    if(cLinuxMain_FullScreen) {
        gtk_window_unfullscreen(GTK_WINDOW(cLinuxMain_Window));
        cLinuxMain_FullScreen = false;
        comms::cSettings::GetInstance()->FullScreen = false;
    }
}

static float cLinuxMain_EndTabletTimeSec = -1.0f;

// cLinuxMain_EndTabletSet
static void cLinuxMain_EndTabletSet() { // On tablet button release and motion events
    cLinuxMain_EndTabletTimeSec = comms::cTimer::GetTimeSec();
}

// cLinuxMain_EndTabletClear
static void cLinuxMain_EndTabletClear() { // On tablet button press or any mouse events
    cLinuxMain_EndTabletTimeSec = -1.0f;
}

// cLinuxMain_EndTablet
static void cLinuxMain_EndTablet() {
    if(-1.0f == cLinuxMain_EndTabletTimeSec) {
        return;
    }
    float d = comms::cTimer::GetTimeSec() - cLinuxMain_EndTabletTimeSec;
    if(d < 0.2f) {
        return;
    }
    cLinuxMain_EndTabletTimeSec = comms::cTimer::GetTimeSec();
    // For some reason after events from the tablet, system doesn't send events from the mouse.
    // However, if we move mouse cursor out of the window bounds, system restores events from the mouse.
    // Moreover, this should be done during proximity out, which system doesn't send at all.
    GdkDisplay *Display = gtk_widget_get_display(cLinuxMain_Window);
    GdkScreen *Screen = gtk_widget_get_screen(cLinuxMain_Window);
    gint X = 0, Y = 0;
    gdk_display_get_pointer(Display, nullptr, &X, &Y, nullptr);
    gdk_display_warp_pointer(Display, Screen, 0, 0);
    gdk_display_warp_pointer(Display, Screen, X, Y);
}

const int cLinuxMain_Dashes = 65; // Number of dashes for tablet and 3Dconnexion log

//*****************************************************************************
// See "HowTo.txt" section "3Dconnexion under Linux using Spacenav"
//*****************************************************************************
#ifdef COMMS_3DCONNEXION
#include <spnav.h>
// cLinuxMain_3Dconnexion
class cLinuxMain_3Dconnexion {
public:
    comms::cVec3 Translation;
    comms::cVec3 Rotation;
    comms::cVec2i ButtonState;
    std::mutex Mutex;
    
    cLinuxMain_3Dconnexion() {
        Translation.SetZero();
        Rotation.SetZero();
        ButtonState.Set(0);
        m_Found = false;
    }
    void Init();
    void Free();
    void HandleEvents();
private:
    bool m_Found;
};
cLinuxMain_3Dconnexion g_3Dconnexion;

namespace comms {
bool cMain_TdxTimeProportional() {
    return false;
}
// cMain_GetTdxState
void cMain_GetTdxState(cVec3 *Translation, cVec3 *Rotation, cVec2i *ButtonState) {
    g_3Dconnexion.Mutex.lock();
    if(Translation != nullptr) {
        *Translation = g_3Dconnexion.Translation;
    }
    if(Rotation != nullptr) {
        *Rotation = g_3Dconnexion.Rotation;
    }
    if(ButtonState != nullptr) {
        *ButtonState = g_3Dconnexion.ButtonState;
    }
    g_3Dconnexion.Mutex.unlock();
}

} // comms

// cLinuxMain_3Dconnexion::Init
void cLinuxMain_3Dconnexion::Init() {
    int r = spnav_open();
    const comms::cStr Separator(cLinuxMain_Dashes, '-');
    comms::cStr L = "Space Navigator daemon for 3Dconnexion devices: ";
    m_Found = (r != -1);
    L += (m_Found ? "FOUND" : "NOT found");
    L += comms::cStr::EndLn;
    if(!m_Found) {
        L += "Install Space Navigator daemon with Terminal command:" + comms::cStr::EndLn + "sudo apt install spacenavd" + comms::cStr::EndLn;
    }
    L += Separator;
    comms::cLog::TerminalMessage(L);
}

// cLinuxMain_3Dconnexion::Free
void cLinuxMain_3Dconnexion::Free() {
    spnav_close();
}

// cLinuxMain_3Dconnexion::HandleEvents
void cLinuxMain_3Dconnexion::HandleEvents() {
    if(!m_Found) {
        return;
    }
    spnav_event SNE;
    comms::cVec3 T(0.0f), R(0.0f);
    while(spnav_poll_event(&SNE) != 0) {
        if(SPNAV_EVENT_MOTION == SNE.type) {
            T += comms::cVec3(SNE.motion.x, SNE.motion.y, SNE.motion.z);
            R += comms::cVec3(SNE.motion.rx, SNE.motion.ry, SNE.motion.rz);
		} else if(SPNAV_EVENT_BUTTON == SNE.type) {
            const int b = SNE.button.bnum;
            if(b >= 0 && b < 2) {
                g_3Dconnexion.Mutex.lock();
                ButtonState[b] = SNE.button.press;
                g_3Dconnexion.Mutex.unlock();
            }
		}
        comms::cRender::NeedUpdate();
    }
    g_3Dconnexion.Mutex.lock();
    Translation = T;
    Rotation = R;
    g_3Dconnexion.Mutex.unlock();
}
#else // !COMMS_3DCONNEXION
namespace comms {
void cMain_GetTdxState(cVec3 *Translation, cVec3 *Rotation, cVec2i *ButtonState) {
    if(Translation != nullptr) {
        Translation->SetZero();
    }
    if(Rotation != nullptr) {
        Rotation->SetZero();
    }
    if(ButtonState != nullptr) {
        ButtonState->Set(0);
    }
}
bool cMain_TdxTimeProportional() {
    return false;
}
} // comms
#endif // COMMS_3DCONNEXION

// cLinuxMain_OnRender
gboolean cLinuxMain_OnRender(gpointer) {
#ifdef COMMS_3DCONNEXION
    g_3Dconnexion.HandleEvents();
#endif // COMMS_3DCONNEXION
    comms::cSplash::DestroyLater();
    if(cLinuxMain_Close) {
        cLinuxMain_Close = false;
        if(comms::cMain_OnClose()) {
            gtk_main_quit();
        }
    }
    if(IsInExitState) {
        exit(0);
    }
    if(nullptr == comms::cRender::GetInstance()) {
        return true;
    }
    
    if(comms::cSettings::GetInstance()->FullScreen && !cLinuxMain_FullScreen) {
        gtk_window_fullscreen(GTK_WINDOW(cLinuxMain_Window));
        cLinuxMain_FullScreen = true;
    } else if(!comms::cSettings::GetInstance()->FullScreen && cLinuxMain_FullScreen) {
        gtk_window_unfullscreen(GTK_WINDOW(cLinuxMain_Window));
        cLinuxMain_FullScreen = false;
    }
    
    bool Active = gtk_window_is_active(GTK_WINDOW(cLinuxMain_Window));
    if(!Active && !comms::cPause::GetSystemPause()) {
        comms::cPause::SetSystemPause(true);
    } else if(Active && comms::cPause::GetSystemPause()) {
        comms::cTimer::Acquire();
        comms::cPause::SetSystemPause(false);
        comms::cInput::Acquire();
        comms::cInput::FreeEvents();
        cLinuxMain_RepeatedClear();
    }
    if(comms::cPause::GetSystemPause() && !IgnoreSystemPause) {
        comms::SleepMs(100);
    }

    int Width = cLinuxMain_DrawingArea->allocation.width;
    int Height = cLinuxMain_DrawingArea->allocation.height;
    if(Width < 1 || Height < 1) {
        return true;
    }

    comms::cRect Viewport;
    Viewport.SetBottomLeft(0.0f, 0.0f);
    Viewport.SetTopRight((float)Width, (float)Height);
    comms::cMain_OnRender(Viewport);

    cLinuxMain_EndTablet();

    return true;
}

namespace comms {

// cMain_SetWindowTitle
void cMain_SetWindowTitle(const char *Title) {
    static cStr CurTitle;
    if(cLinuxMain_Window != nullptr) {
        if(!cStr::Equals(Title, CurTitle)) {
            gtk_window_set_title(GTK_WINDOW(cLinuxMain_Window), Title);
            CurTitle = Title;
        }
    }
}

// cMain_Quit
void cMain_Quit() {
    gtk_main_quit();
}

} // comms

// cLinuxMain_LocalToLocal
void cLinuxMain_LocalToLocal(const int X, const int Y, comms::cVec2 *Local) {
	// Linux Ubuntu 12.10 sends tablet coords with subpixel precision.
	// We don't need that because within 3D-Coat there are a lot of places
	// where start/previous coords are cached as integers.
	Local->x = (float)X;
	Local->y = (float)(cLinuxMain_DrawingArea->allocation.height - Y);
}

comms::cList<comms::cInputEvent *> & cLinuxMain_GetEvents();

//-----------------------------------------------------------------------------------------------
// cLinuxMain_MouseButtonHandler
//-----------------------------------------------------------------------------------------------
gboolean cLinuxMain_MouseButtonHandler(GtkWidget *Widget, GdkEventButton *Event, gpointer Data) {
    if(GDK_BUTTON_PRESS == Event->type && comms::InputString_Wnd != nullptr) {
        comms::InputString_GetResult();
        return 1;
    }
    // Tablet
    bool WasPressed = cLinuxMain_Tablet::PenPressed;
    cLinuxMain_Tablet::PenPressed = false;
    cLinuxMain_Tablet::EraserUsed = false;
    if((cLinuxMain_Stylus == Event->device) || (cLinuxMain_Eraser == Event->device)) {
        cLinuxMain_Tablet::PenPressed = (GDK_BUTTON_PRESS == Event->type);
        cLinuxMain_Tablet::EraserUsed = (cLinuxMain_Tablet::PenPressed && (cLinuxMain_Eraser == Event->device));
    }
    if(WasPressed && !cLinuxMain_Tablet::PenPressed) {
        cLinuxMain_EndTabletSet();
    } else {
        cLinuxMain_EndTabletClear();
    }
    // Pressure
    cLinuxMain_Tablet::ExtractPressure(Event);
    // Mouse
    comms::cInput::OldEvent t;
    t.Type = comms::cInput::OldEvent::TYPE_BUTTON;
    // Code
    if(1 == Event->button) {
        t.Code = comms::cInput::LeftButton;
    } else if(2 == Event->button) {
        t.Code = comms::cInput::MiddleButton;
    } else if(3 == Event->button) {
        t.Code = comms::cInput::RightButton;
    } else {
        return 1;
    }
    if(GDK_2BUTTON_PRESS == Event->type) {
        if(!cLinuxMain_GetEvents().IsEmpty()) {
            comms::cInputEvent *E = cLinuxMain_GetEvents().GetLast();
            if(comms::cInputEventClass::Old == E->Class) {
            	comms::cInput::OldEvent *L = (comms::cInput::OldEvent *)E;
				if(t.Type == L->Type && t.Code == L->Code) {
					L->DoubleClick = true;
				}
            }
        }
        return 1;
    } else if(GDK_BUTTON_PRESS == Event->type) {
        t.Pressed = true;
    } else if(GDK_BUTTON_RELEASE == Event->type) {
        t.Pressed = false;
    } else {
        return 1;
    }
    // MousePos
    cLinuxMain_LocalToLocal((int)Event->x, (int)Event->y, &t.MousePos);

    comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
    *E = t;
    if(WasPressed != cLinuxMain_Tablet::PenPressed) {
		comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
		Pen->Type = cLinuxMain_Tablet::PenPressed ? comms::cInputEvent_Pen::TYPE::Down : comms::cInputEvent_Pen::TYPE::Up;
		Pen->Eraser = cLinuxMain_Tablet::EraserUsed;
		Pen->PenPos = E->MousePos;
		Pen->PenPressure = cLinuxMain_Tablet::CurPressure;
		comms::cInput::AddEvent(Pen); Pen = nullptr;
    }
    comms::cInput::AddEvent(E);
    E = nullptr;
    return 1;
} // cLinuxMain_MouseButtonHandler

//-----------------------------------------------------------------------------------------------
// cLinuxMain_MouseMotionHandler
//-----------------------------------------------------------------------------------------------
gboolean cLinuxMain_MouseMotionHandler(GtkWidget *Widget, GdkEventMotion *Event, gpointer Data) {
    if(!cLinuxMain_Tablet::PenPressed && ((cLinuxMain_Stylus == Event->device) || (cLinuxMain_Eraser == Event->device))) {
        cLinuxMain_EndTabletSet();
    } else {
        cLinuxMain_EndTabletClear();
    }
    // Pressure
    cLinuxMain_Tablet::ExtractPressure(Event);
    // Mouse
    static int PrevX = comms::cMath::IntMinValue, PrevY = comms::cMath::IntMinValue;
    const int X = (int)Event->x;
    const int Y = (int)Event->y;
    if(comms::cMath::IntMinValue == PrevX || comms::cMath::IntMinValue == PrevY) {
        PrevX = X;
        PrevY = Y;
    }
    comms::cInput::OldEvent t;
    t.Type = comms::cInput::OldEvent::TYPE_MOUSEMOVE;
    cLinuxMain_LocalToLocal(X, Y, &t.MousePos);
    t.MouseDelta.Set((float)(X - PrevX), -(float)(Y - PrevY));
    PrevX = X;
    PrevY = Y;

    comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
    *E = t;
    if(cLinuxMain_Tablet::PenPressed) {
		comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
		Pen->Type = comms::cInputEvent_Pen::TYPE::Move;
		Pen->PenPos = E->MousePos;
		Pen->PenDelta = E->MouseDelta;
		Pen->PenPressure = cLinuxMain_Tablet::CurPressure;
		comms::cInput::AddEvent(Pen); Pen = nullptr;
    }
    comms::cInput::AddEvent(E);
    E = nullptr;
    return 1;
} // cLinuxMain_MouseMotionHandler

//-----------------------------------------------------------------------------------------------
// cLinuxMain_MouseScrollHandler
//-----------------------------------------------------------------------------------------------
gboolean cLinuxMain_MouseScrollHandler(GtkWidget *Widget, GdkEventScroll *Event, gpointer Data) {
    cLinuxMain_EndTabletClear();
    float WheelDelta = 0.0f;
    if(GDK_SCROLL_DOWN == Event->direction) {
        WheelDelta = -1.0f;
    } else if(GDK_SCROLL_UP == Event->direction) {
        WheelDelta = 1.0f;
    }
    if(WheelDelta != 0.0f) {
        comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
        E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
        E->WheelDelta = WheelDelta;
        E->Code = E->WheelDelta > 0.0f ? comms::cInput::WheelUp : comms::cInput::WheelDown;
        E->Pressed = true;
        cLinuxMain_LocalToLocal((int)Event->x, (int)Event->y, &E->MousePos);
        comms::cInput::AddEvent(E);
        E = nullptr;
    }
    return 1;
} // cLinuxMain_MouseScrollHandler

//-----------------------------------------------------------------------------------------
// cLinuxMain_KeyboardHandler
//-----------------------------------------------------------------------------------------
gboolean cLinuxMain_KeyboardHandler(GtkWidget *Widget, GdkEventKey *Event, gpointer Data) {
    int h = (int)Event->hardware_keycode;
    // Shift
    if(cLinuxMain_ShiftCodes.Contains(h)) {
        h = cLinuxMain_ShiftCodes[0];
    }
    // Control
    if(cLinuxMain_ControlCodes.Contains(h)){
        h = cLinuxMain_ControlCodes[0];
    }
    // Alt
    if(cLinuxMain_AltCodes.Contains(h)) {
        h = cLinuxMain_AltCodes[0];
    }
    // Enter
    if(cLinuxMain_EnterCodes.Contains(h)) {
        h = cLinuxMain_EnterCodes[0];
    }

    int Code = cLinuxMain_Codes.IndexOf(h);
    if(-1 == Code) {
        return 1;
    }
    if(GDK_KEY_PRESS == Event->type) {
        // Ignore repeated press
        if(cLinuxMain_Repeated[Code]) {
            return 1;
        }
        cLinuxMain_Repeated[Code] = true;
    } else {
        cLinuxMain_Repeated[Code] = false;
    }

    comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
    E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
    E->Code = Code;
    E->Pressed = (GDK_KEY_PRESS == Event->type);
    comms::cInput::AddEvent(E);
    E = nullptr;
    return 1;
} // cLinuxMain_KeyboardHandler

static bool cLinuxMain_IsWacomStylusDevice(const GdkDevice *D) {
    bool p = (GDK_SOURCE_PEN == D->source);
    if(!p) {
        return false;
    }
    comms::cStr t = D->name;
    bool s = comms::cStr::EqualsNoCase(t, "stylus");
    if(s) {
        return true;
    }
    bool q = t.Contains("Wacom", true) && !t.Contains("Mouse", true) && !t.Contains("pad", true) && !t.Contains("cursor", true) && !t.Contains("eraser", true) && !t.Contains("Pointer", true) && !t.Contains("Finger", true);
    return q;
}

static bool cLinuxMain_IsGeniusStylusDevice(const GdkDevice *D) {
    comms::cStr t = D->name;
    bool u = t.Contains("UC-LOGIC Tablet");
    bool w = t.Contains("WALTOP") && t.Contains("Tablet", true) && t.Contains("Stylus", true);
    return u || w;
}

static bool cLinuxMain_IsXppenStylusDevice(const GdkDevice *D) {
    const comms::cStr n = D->name;
    const bool X = n.Contains("XPPEN") || n.Contains("XP-PEN") || n.Contains("XPPen");
    const bool P = n.Contains("Pen (0)");
    return X && P;
}

static bool cLinuxMain_IsHuionStylusDevice(const GdkDevice *D) {
    const comms::cStr n = D->name;
    const bool H = n.Contains("HUION") || n.Contains("Huion");
    const bool P = n.Contains("Pen (0)");
    return H && P;
}


static bool cLinuxMain_IsOtherStylusDevice(const GdkDevice *D) {
    const bool p = (GDK_SOURCE_PEN == D->source);
    if(!p) {
        return false;
    }
    const comms::cStr &n = D->name;
    const bool TMP = n.Contains("Tablet Monitor Pen");
    const bool HP = comms::cStr::Equals(n, "ELAN22CA:00 04F3:22CA Pen");
    const bool XP = cLinuxMain_IsXppenStylusDevice(D);
    const bool HU = cLinuxMain_IsHuionStylusDevice(D);
    const bool r = (TMP || HP || XP || HU);
    return r;
}

static bool cLinuxMain_IsStylusDevice(const GdkDevice *D) {
	const comms::cStr n = D->name;
	static const comms::cStr COAT_PEN = comms::cIO::GetEnvironmentVariable("COAT_PEN");
	if(!COAT_PEN.IsEmpty()) {
		const bool r = comms::cStr::Equals(n, COAT_PEN);
		return r;
	}
    const bool WL = n.StartsWith("xwayland-tablet stylus:"); // XX
	return WL || cLinuxMain_IsWacomStylusDevice(D) || cLinuxMain_IsGeniusStylusDevice(D) || cLinuxMain_IsOtherStylusDevice(D);
}

static bool cLinuxMain_IsMouseDevice(const GdkDevice *D) {
    const comms::cStr n = D->name;
	static const comms::cStr COAT_MOUSE = comms::cIO::GetEnvironmentVariable("COAT_MOUSE");
	if(!COAT_MOUSE.IsEmpty()) {
        const bool r = comms::cStr::Equals(n, COAT_MOUSE);
        return r;
    }
    const bool WL = n.StartsWith("xwayland-relative-pointer:"); // XX
    return WL;
}

static bool cLinuxMain_IsWacomEraserDevice(const GdkDevice *D) {
    bool p = (GDK_SOURCE_PEN == D->source);
    bool e = (GDK_SOURCE_ERASER == D->source);
    if(!p && !e) {
        return false;
    }
    comms::cStr t = D->name;
    bool r = comms::cStr::EqualsNoCase(t, "eraser");
    if(e && r) {
        return true;
    }
    bool c = t.Contains("Wacom", true) && t.Contains("eraser", true);
    return c;
}

static bool cLinuxMain_IsHuionEraserDevice(const GdkDevice *D) {
    const bool e = GDK_SOURCE_ERASER == D->source;
    if(!e) {
        return false;
    }

    const comms::cStr n = D->name;
    const bool H = n.Contains("HUION") || n.Contains("Huion");
    const bool E = n.Contains("Eraser (0)");
    return H && E;
}


static bool cLinuxMain_IsEraserDevice(const GdkDevice *D) {
    const comms::cStr n = D->name;
	const comms::cStr COAT_ERASER = comms::cIO::GetEnvironmentVariable("COAT_ERASER");
	if(!COAT_ERASER.IsEmpty()) {
		const bool r = comms::cStr::Equals(n, COAT_ERASER);
		return r;
	}
    const bool WL = n.StartsWith("xwayland-tablet eraser:"); // XX
    return WL || cLinuxMain_IsWacomEraserDevice(D) || cLinuxMain_IsHuionEraserDevice(D);
}

//-----------------------------------------------------------------------------
// cLinuxMain_InitTablet
//-----------------------------------------------------------------------------
void cLinuxMain_InitTablet() {
    GList *S = gdk_devices_list();
    if(nullptr == S) {
        return;
    }
    // Assign Devices
    GList *T = S;
    GdkDevice *Cur;
    do {
        Cur = (GdkDevice *)T->data;
        if(Cur != nullptr) {
            if(nullptr == cLinuxMain_Stylus && cLinuxMain_IsStylusDevice(Cur)) {
                cLinuxMain_Stylus = Cur;
            }
            if(nullptr == cLinuxMain_Eraser && cLinuxMain_IsEraserDevice(Cur)) {
                cLinuxMain_Eraser = Cur;
            }
            if(nullptr == cLinuxMain_Mouse && cLinuxMain_IsMouseDevice(Cur)) {
                cLinuxMain_Mouse = Cur;
            }
        }
        T = T->next;
    } while(T != nullptr);
    // Log environment variables "COAT_PEN" and "COAT_ERASER"
	comms::cStr COAT_PEN = comms::cIO::GetEnvironmentVariable("COAT_PEN");
	comms::cStr COAT_ERASER = comms::cIO::GetEnvironmentVariable("COAT_ERASER");
    comms::cStr COAT_MOUSE = comms::cIO::GetEnvironmentVariable("COAT_MOUSE");
	if(COAT_PEN.IsEmpty()) {
		COAT_PEN = "empty";
		if(nullptr == cLinuxMain_Stylus) {
			COAT_PEN += comms::cStr::EndLn + "(define the name of your stylus input device from the list below)";
		}
	}
	if(COAT_ERASER.IsEmpty()) {
		COAT_ERASER = "empty";
		if(nullptr == cLinuxMain_Eraser) {
			COAT_ERASER += comms::cStr::EndLn + "(define the name of your eraser input device from the list below)";
		}
	}
	if(COAT_MOUSE.IsEmpty()) {
		COAT_MOUSE = "empty";
		if(nullptr == cLinuxMain_Mouse) {
			COAT_MOUSE += comms::cStr::EndLn + "(define the name of your mouse input device from the list below)";
		}
	}
	comms::cLog::TerminalMessage(comms::cStr(cLinuxMain_Dashes, '-'));
	comms::cLog::TerminalMessage("Environment variable \"COAT_PEN\" = " + COAT_PEN);
	comms::cLog::TerminalMessage("Environment variable \"COAT_ERASER\" = " + COAT_ERASER);
	comms::cLog::TerminalMessage("Environment variable \"COAT_MOUSE\" = " + COAT_MOUSE);
    // Enum Devices
    comms::cLog::TerminalMessage(comms::cStr(cLinuxMain_Dashes, '-'));
    comms::cLog::TerminalMessage("Input Devices (source | \"name\" | mode)");
    comms::cLog::TerminalMessage(comms::cStr(cLinuxMain_Dashes, '-'));
    T = S;
    comms::cStr Source, Name, Mode, M;
    int i = 1;
    do {
        Cur = (GdkDevice *)T->data;
        if(Cur != nullptr) {
            Source.Clear();
            if(GDK_SOURCE_MOUSE == Cur->source) {
                Source = "Mouse";
            } else if(GDK_SOURCE_PEN == Cur->source) {
                Source = "Pen";
            } else if(GDK_SOURCE_ERASER == Cur->source) {
                Source = "Eraser";
            } else if(GDK_SOURCE_CURSOR == Cur->source) {
                Source = "Cursor";
            }
            Name = Cur->name;
            Mode.Clear();
            if(GDK_MODE_DISABLED == Cur->mode) {
                Mode = "Disabled";
            } else if(GDK_MODE_SCREEN == Cur->mode) {
                Mode = "Screen";
            } else if(GDK_MODE_WINDOW == Cur->mode) {
                Mode = "Window";
            }
            if(cLinuxMain_Stylus == Cur) {
                gdk_device_set_mode(Cur, GDK_MODE_SCREEN); // Enable "stylus"
                Mode += " --> Screen (enabled as STYLUS)";
            } else if(cLinuxMain_Eraser == Cur) {
                gdk_device_set_mode(Cur, GDK_MODE_SCREEN); // Enable "eraser"
                Mode += " --> Screen (enabled as ERASER)";
            } else if(cLinuxMain_Mouse == Cur && cLinuxMain_Stylus != nullptr) { // Enable mouse device only when there is stylus device (under WSL there is no stylus device and enabled mouse device blocks the mouse)
                gdk_device_set_mode(Cur, GDK_MODE_SCREEN); // Enable "mouse"
                Mode += " --> Screen (enabled as MOUSE)";
            }
            M = comms::cStr::ToString(i) + ". ";
            M += Source;
            M.PadRight(12);
            M += "\"" + Name + "\"";
            M.PadRight(48);
            M += Mode;
            M.PadRight(58);
            comms::cLog::TerminalMessage(M);
            i++;
        }
        T = T->next;
    } while(T != nullptr);
    comms::cLog::TerminalMessage(comms::cStr(cLinuxMain_Dashes, '-'));
    // We should also init extension events here (for the second time).
    // Otherwise Ubuntu 10.04 doesn't send events from the tablet device.
    gtk_widget_set_extension_events(cLinuxMain_DrawingArea, GDK_EXTENSION_EVENTS_CURSOR);
} // cLinuxMain_InitTablet

// cLinuxMain_DeleteHandler
gboolean cLinuxMain_DeleteHandler() {
    cLinuxMain_Close = true;
    return true;
}

#include <signal.h>
#include <netinet/in.h>

// CrashHandler
void CrashHandler(int sig) {
    comms::cMain_OnCrash();
    exit(-1);
}

// cLinuxMain_CreateIcon
void cLinuxMain_CreateIcon(const char *ImageFilePn, const char *TextFilePn) {
	comms::cImage I;
	if(!comms::cIO::LoadImage(ImageFilePn, &I)) {
		return;
	}
        if(I.GetFormat() != comms::cFormat::Rgba8) {
            return;
        }
        if(I.GetWidth() != 64 || I.GetHeight() != 64) {
            return;
        }
        I.Flip();
	int n = I.GetWidth() * I.GetHeight();
	const comms::dword *P = (const comms::dword *)I.GetPixels();
	int i, b, j;
	comms::dword d;
	comms::cStr S;
	comms::cFile F;
	bool r;
	for(i = 0; i < n; i++) {
		d = *P;
		P++;
		S.Clear();
		for(j = 0; j < 4; j++) {
			b = (d >> (24 - j * 8)) & 0xff;
			S << comms::cStr::Format("%x%x", b >> 4, b & 0xf);
		}
		S.Insert(0, "0x");
		r = (0 == (i % 8));
		if(!r) {
			S.Insert(0, ", ");
		}
		if(r && i != 0) {
			F.WriteString(",");
			F.WriteString(comms::cStr::EndLn);
		}
		F.WriteString(S.ToCharPtr());
	}
	comms::cIO::SaveFile(TextFilePn, F);
}

static const comms::dword cLinuxMain_Icon[64 * 64] = {
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x0355aa55, 0x113c785a, 0x353f9565, 0x5d47af73, 0x8543ae71, 0x9b43b271,
0x9d42af71, 0x8b46b273, 0x6547b377, 0x3d439f6d, 0x19337a52, 0x05339966, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x07246d49,
0x3149ac72, 0x794abb7a, 0xc546be7a, 0xf142bf78, 0xff3dbd74, 0xff36b56c, 0xff31af67, 0xff2fac64,
0xff2fac64, 0xff31ae66, 0xff35b46b, 0xff3cbc72, 0xf742c279, 0xd145bf7a, 0x894ac07d, 0x3d47b075,
0x0d3b8962, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x09397155, 0x534ab578, 0xb94ac57e,
0xf342c077, 0xff37b56d, 0xff2ca962, 0xff27a45c, 0xff22a057, 0xff21a058, 0xff27a55f, 0xff2bab66,
0xff2eae69, 0xff2dad68, 0xff29a862, 0xff25a35b, 0xff24a35a, 0xff2ba860, 0xff35b46b, 0xf93fbf77,
0xc74ac77f, 0x634dbc7c, 0x0f337755, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x0355aa55, 0x4348ab72, 0xc54cc881, 0xfb41c177, 0xff32b067,
0xff28a65e, 0xff22a058, 0xff23a35c, 0xff37ba77, 0xff52d69b, 0xff63ebb5, 0xff6bf6c0, 0xff6ef9c4,
0xff70fac7, 0xff6ffac6, 0xff6df7c2, 0xff68efba, 0xff5adea7, 0xff42c485, 0xff2baa64, 0xff27a55c,
0xff30af66, 0xfd3fbf76, 0xd14cca82, 0x534ab278, 0x0355aa55, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x1b429768, 0x974ec280, 0xfb47c97e, 0xff33b36a, 0xff29a75f, 0xff22a158,
0xff29ab64, 0xff48ce8f, 0xff68f5bc, 0xff6effc6, 0xff6cffc3, 0xff6affc1, 0xff69ffc0, 0xff68ffc0,
0xff68ffc0, 0xff69ffc0, 0xff6affc1, 0xff6bffc3, 0xff6effc5, 0xff71ffc9, 0xff6ffac5, 0xff59dea5,
0xff36b874, 0xff29a85f, 0xff32b268, 0xfd45c97e, 0xa54dc582, 0x1f42a46b, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x3548a36f, 0xcf4fcd85, 0xff3fc176, 0xff2ead64, 0xff25a55b, 0xff22a35a, 0xff43ca88,
0xff65f4b9, 0xff6affc1, 0xff67ffbe, 0xff65ffbc, 0xff63ffb9, 0xff61ffb8, 0xff60ffb7, 0xff60ffb7,
0xff60ffb7, 0xff60ffb7, 0xff61ffb8, 0xff62ffba, 0xff64ffbc, 0xff67ffbe, 0xff6affc1, 0xff6effc5,
0xff6ffbc5, 0xff57dda2, 0xff2fb069, 0xff2eae64, 0xff3ec276, 0xd54fcf86, 0x3948a570, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x4b47aa74, 0xe94fd287, 0xff3bbd72, 0xff2dac63, 0xff24a45b, 0xff29ad64, 0xff58e5a5, 0xff69febe,
0xff65ffbb, 0xff62ffb8, 0xff5fffb5, 0xff5dffb3, 0xff5bffb1, 0xff59ffb0, 0xff59ffaf, 0xff58ffae,
0xff58ffae, 0xff59ffaf, 0xff59ffb0, 0xff5affb1, 0xff5dffb3, 0xff5effb5, 0xff61ffb8, 0xff65ffbb,
0xff69ffbf, 0xff6effc4, 0xff50d99a, 0xff23a65b, 0xff2cad63, 0xff3bbe72, 0xe950d488, 0x4946a870,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x554bb175,
0xf151d488, 0xff3abc71, 0xff2cad63, 0xff24a55a, 0xff2fb66e, 0xff60f2b2, 0xff66ffbb, 0xff61ffb6,
0xff5dffb2, 0xff5affaf, 0xff57ffad, 0xff56ffab, 0xff54ffa9, 0xff53ffa8, 0xff51ffa7, 0xff51ffa7,
0xff51ffa7, 0xff51ffa7, 0xff52ffa8, 0xff54ffa9, 0xff55ffab, 0xff57ffad, 0xff5affaf, 0xff5dffb3,
0xff60ffb6, 0xff65ffbb, 0xff5becaa, 0xff1fa659, 0xff24a75b, 0xff2daf64, 0xff3cc073, 0xed52d689,
0x4946a873, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x4341986b, 0xed52d689,
0xff3dc175, 0xff2eb065, 0xff25a75c, 0xff2fb56c, 0xff60f5b2, 0xff63ffb7, 0xff5effb2, 0xff5affae,
0xff57ffab, 0xff53ffa8, 0xff51ffa6, 0xff4fffa4, 0xff4effa2, 0xff4cffa1, 0xff4cffa0, 0xff4bffa0,
0xff4bffa0, 0xff4cffa0, 0xff4cffa1, 0xff4dffa2, 0xff4fffa4, 0xff51ffa6, 0xff53ffa8, 0xff56ffab,
0xff5affaf, 0xff5effb3, 0xff58f1a8, 0xff1da758, 0xff20a659, 0xff26aa5e, 0xff30b367, 0xff42c779,
0xe553d78c, 0x2d3e9360, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x2537835a, 0xdf54d58b, 0xff43c87b,
0xff32b469, 0xff28aa5f, 0xff20a559, 0xff27b064, 0xff53eda1, 0xff5bffaf, 0xff57ffab, 0xff53ffa7,
0xff50ffa4, 0xff4effa1, 0xff4bff9f, 0xff4aff9d, 0xff48ff9c, 0xff47ff9b, 0xff46ff9a, 0xff46ff9a,
0xff46ff9a, 0xff46ff9a, 0xff47ff9b, 0xff48ff9c, 0xff49ff9d, 0xff4bff9f, 0xff4effa2, 0xff50ffa5,
0xff54ffa8, 0xff57ffab, 0xff50ee9f, 0xff1ba757, 0xff1ea657, 0xff23a95b, 0xff2aaf62, 0xff36bb6d,
0xff49d282, 0xcb54d089, 0x111e4b3c, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x0f225544, 0xb951c684, 0xff4bd485, 0xff37bb6f,
0xff2baf63, 0xff23a85b, 0xff1ea558, 0xff1ba556, 0xff1dab5a, 0xff41e08c, 0xff50fea3, 0xff4effa0,
0xff4bff9d, 0xff48ff9b, 0xff46ff99, 0xff45ff97, 0xff43ff96, 0xff42ff95, 0xff41ff94, 0xff41ff94,
0xff41ff94, 0xff41ff94, 0xff42ff95, 0xff43ff96, 0xff44ff98, 0xff47ff9a, 0xff48ff9c, 0xff4bff9e,
0xff4effa1, 0xff52ffa6, 0xff43e38f, 0xff1aa856, 0xff1ca757, 0xff20a95a, 0xff27ad5f, 0xff30b668,
0xff3ec576, 0xff54df8e, 0x8f4db779, 0x03000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x7144a06a, 0xfd55de8f, 0xff3fc678, 0xff31b569,
0xff27ac5f, 0xff21a85a, 0xff1da757, 0xff1ba756, 0xff18a856, 0xff19ab57, 0xff35d97e, 0xff49ff9b,
0xff47ff98, 0xff44ff96, 0xff42ff94, 0xff40ff92, 0xff3ffe91, 0xff3efd90, 0xff3dfd8f, 0xff3dfd8f,
0xff3dfc8f, 0xff3dfd8f, 0xff3efe90, 0xff3ffe91, 0xff40ff92, 0xff42ff94, 0xff44ff96, 0xff47ff99,
0xff49ff9c, 0xff4dffa0, 0xff33d27b, 0xff19a956, 0xff1ba957, 0xff1faa59, 0xff25ad5e, 0xff2db466,
0xff39c071, 0xff4bd584, 0xf158dc8f, 0x3b348256, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x211f462e, 0xeb52d087, 0xff4ad584, 0xff38bf71, 0xff2cb264,
0xff24ac5e, 0xff1fa95a, 0xff1ca958, 0xff1aa957, 0xff19aa56, 0xff17ab56, 0xff17ac57, 0xff32de7d,
0xff42fe94, 0xff40fe92, 0xff3efc8f, 0xff3cfa8d, 0xff3bf98c, 0xff3af88b, 0xff39f78a, 0xff39f78a,
0xff38f78a, 0xff39f78a, 0xff3af88b, 0xff3bf98c, 0xff3cfb8d, 0xff3efd8f, 0xff40fe91, 0xff42ff94,
0xff45ff97, 0xff47fc98, 0xff23bd65, 0xff19ab57, 0xff1bab58, 0xff1eab5a, 0xff23ae5d, 0xff2bb464,
0xff37c070, 0xff49cd7f, 0xff4aca7e, 0xb949b776, 0x07244924, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x97388c5b, 0xff58e794, 0xff42cd7c, 0xff34bc6d, 0xff29b162,
0xff22ac5c, 0xff1fa95a, 0xff1eab5a, 0xff1cab59, 0xff1aab57, 0xff18ac57, 0xff17ad57, 0xff17b058,
0xff31e17c, 0xff3df98d, 0xff3bf78b, 0xff39f589, 0xff38f488, 0xff36f287, 0xff35f186, 0xff34ef84,
0xff32eb81, 0xff31e980, 0xff31e97f, 0xff32eb81, 0xff35ee83, 0xff38f488, 0xff3cf98d, 0xff3ffd90,
0xff41ff92, 0xff3bee88, 0xff19b059, 0xff19ad57, 0xff1bad59, 0xff1ead5a, 0xff23af5d, 0xff2ab464,
0xff35c06f, 0xff4b9f6d, 0xff426550, 0xfd45c377, 0x45296b43, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x23071607, 0xed4cc17e, 0xff4ae188, 0xff2ddc70, 0xff2fbb6a, 0xff26b061,
0xff22ae5e, 0xff269b57, 0xff2d8c55, 0xff2aab60, 0xff1db15c, 0xff19ae58, 0xff17ae57, 0xff16af58,
0xff19b65c, 0xff33e881, 0xff38f287, 0xff36f186, 0xff31e97f, 0xff27d772, 0xff1fc867, 0xff19bf60,
0xff17ba5c, 0xff16b85b, 0xff15b75a, 0xff16b75a, 0xff17b85c, 0xff19bc5f, 0xff1fc565, 0xff28d371,
0xff34e880, 0xff29cf70, 0xff18af58, 0xff19af59, 0xff1caf5a, 0xff1eb05b, 0xff22b15e, 0xff28b563,
0xff2fc56c, 0xff4aa46e, 0xff585b59, 0xff398356, 0xb139a163, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x7f1a5432, 0xff57e291, 0xff25c863, 0xff1fe669, 0xff29c067, 0xff23af5e,
0xff277648, 0xff3c413f, 0xff404140, 0xff3e4642, 0xff2aa05b, 0xff1ab05a, 0xff18b058, 0xff17b058,
0xff16b158, 0xff1cbe61, 0xff2fe37c, 0xff20c968, 0xff16b75b, 0xff14b559, 0xff14b558, 0xff14b559,
0xff14b659, 0xff14b659, 0xff14b559, 0xff14b559, 0xff14b459, 0xff15b459, 0xff15b358, 0xff15b358,
0xff17b359, 0xff18b259, 0xff19b259, 0xff1ab25a, 0xff1cb15b, 0xff1fb25d, 0xff22b35f, 0xff25c064,
0xff20e56b, 0xff3db56c, 0xff565a58, 0xff48574e, 0xf135a862, 0x271a3b27, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x0f113322, 0xcf206f40, 0xff4ae186, 0xff0fb84d, 0xff17cb5a, 0xff26b964, 0xff239251,
0xff3d4541, 0xff454646, 0xff444544, 0xff424342, 0xff38674b, 0xff1eb45e, 0xff19b159, 0xff17b259,
0xff16b259, 0xff15b359, 0xff16b55a, 0xff15b459, 0xff14b559, 0xff14b659, 0xff14b659, 0xff14b659,
0xff14b75a, 0xff14b75a, 0xff15b659, 0xff15b65a, 0xff15b65a, 0xff15b65a, 0xff16b559, 0xff16b45a,
0xff17b45a, 0xff18b45a, 0xff19b45b, 0xff1bb45b, 0xff1db45d, 0xff20b55e, 0xff23b761, 0xff1ed866,
0xff1ad961, 0xff27b75e, 0xff4d5e54, 0xff515653, 0xff2f794c, 0x71297e4a, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x370e3c20, 0xf7227a47, 0xff29cc66, 0xff0fb94d, 0xff1bbd5a, 0xff25b662, 0xff316145,
0xff484a49, 0xff494949, 0xff464747, 0xff444544, 0xff3f4f45, 0xff22b35f, 0xff1ab35b, 0xff18b35a,
0xff17b45a, 0xff16b55a, 0xff16b55a, 0xff15b65a, 0xff15b65a, 0xff15b75a, 0xff15b85a, 0xff15b85b,
0xff15b85a, 0xff15b85a, 0xff15b75a, 0xff15b85a, 0xff15b75b, 0xff16b75b, 0xff16b75b, 0xff17b65b,
0xff18b65b, 0xff19b65b, 0xff1ab65c, 0xff1cb65d, 0xff1eb75f, 0xff21b860, 0xff24ba63, 0xff1ac05c,
0xff11c053, 0xff19b754, 0xff416750, 0xff4d5450, 0xff355642, 0xb11e8145, 0x03000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x71104124, 0xff156e38, 0xff13b74e, 0xff0fbc4e, 0xff26c064, 0xff23a158, 0xff434946,
0xff4c4d4c, 0xff4a4b4a, 0xff484948, 0xff464646, 0xff424745, 0xff27ad5f, 0xff1bb65c, 0xff19b65c,
0xff18b65b, 0xff17b65b, 0xff16b75b, 0xff16b75b, 0xff16b85b, 0xff16b85b, 0xff16b95b, 0xff16b95b,
0xff16b95c, 0xff16b95c, 0xff16b95b, 0xff16b95c, 0xff16b95c, 0xff17b95c, 0xff17b95c, 0xff18b85c,
0xff19b85d, 0xff1ab85d, 0xff1cb95e, 0xff1eb95f, 0xff20ba61, 0xff23bc63, 0xff27bf66, 0xff21c062,
0xff11bb52, 0xff10b54d, 0xff326e49, 0xff46514a, 0xff3d4a42, 0xdd146a34, 0x150c3118, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x03000000, 0xa9114826, 0xff0c7232, 0xff0db046, 0xff14bf53, 0xff2ac269, 0xff297e4c, 0xff4b4e4d,
0xff4e504f, 0xff4c4d4c, 0xff494a49, 0xff474847, 0xff444745, 0xff2ba75f, 0xff1cb85f, 0xff1ab85d,
0xff18b85c, 0xff18b95c, 0xff17b95c, 0xff17b95c, 0xff17b95c, 0xff17ba5c, 0xff17ba5c, 0xff17bb5d,
0xff17bb5d, 0xff17bb5d, 0xff17ba5d, 0xff17bb5d, 0xff17bb5d, 0xff18ba5d, 0xff18ba5d, 0xff19bb5e,
0xff1abb5e, 0xff1cbb5f, 0xff1ebc60, 0xff1fbd62, 0xff22be64, 0xff25c167, 0xff29c46a, 0xff23c564,
0xff15bf56, 0xff0cb248, 0xff226e3e, 0xff3e4d44, 0xff3d4540, 0xf31a6035, 0x2b12532a, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x05003333, 0xd1114c28, 0xff0c8237, 0xff0bae44, 0xff1bc15b, 0xff2bc36b, 0xff336346, 0xff4f5250,
0xff505151, 0xff4e4e4e, 0xff4b4c4b, 0xff494949, 0xff464847, 0xff2fa460, 0xff1ebb61, 0xff1bba5f,
0xff1aba5e, 0xff19ba5e, 0xff19b95d, 0xff19ba5d, 0xff19ba5d, 0xff19bb5e, 0xff18bb5e, 0xff18bc5e,
0xff17bc5e, 0xff18bc5e, 0xff17bc5e, 0xff18bc5e, 0xff18bc5e, 0xff19bc5e, 0xff19bc5f, 0xff1abd5f,
0xff1cbd60, 0xff1dbe61, 0xff1fbf63, 0xff22c165, 0xff25c367, 0xff28c66b, 0xff2dcb6f, 0xff25c867,
0xff16be57, 0xff0cad47, 0xff1b753c, 0xff384a3f, 0xff3b423e, 0xfd1f5c36, 0x450f5c2c, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x0f113322, 0xef11522b, 0xff0c8939, 0xff0cab45, 0xff24c564, 0xff2abd68, 0xff3e5347, 0xff515352,
0xff505151, 0xff4f4f4f, 0xff4d4e4d, 0xff4a4b4b, 0xff474948, 0xff32a260, 0xff20bf64, 0xff1cbe61,
0xff1bbe61, 0xff1bbd5f, 0xff1bae5a, 0xff248f50, 0xff297f4c, 0xff259152, 0xff1caf5b, 0xff1abe60,
0xff19be60, 0xff19be5f, 0xff19be5f, 0xff19be5f, 0xff19be60, 0xff1abf60, 0xff1bbf61, 0xff1cc062,
0xff1ec163, 0xff1fc264, 0xff22c366, 0xff24c669, 0xff28c96c, 0xff2ccd70, 0xff32d375, 0xff29cc6b,
0xff16bd57, 0xff0daa47, 0xff187a3b, 0xff33473b, 0xff38403b, 0xff225335, 0x5d13632f, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x27143421, 0xfd11582e, 0xff0b8b39, 0xff0dac46, 0xff2dcd6e, 0xff29b162, 0xff45504a, 0xff535554,
0xff505151, 0xff4e4f4e, 0xff4d4d4d, 0xff4b4c4b, 0xff494949, 0xff349b5f, 0xff22c467, 0xff1ec264,
0xff1dc062, 0xff1da657, 0xff375744, 0xff3e3d3e, 0xff3e3d3f, 0xff3f3e3f, 0xff3a5344, 0xff239a55,
0xff1bc162, 0xff1bc061, 0xff1ac061, 0xff1ac061, 0xff1ac161, 0xff1bc162, 0xff1cc263, 0xff1ec364,
0xff1fc465, 0xff22c667, 0xff24c86a, 0xff27cb6c, 0xff2ccf70, 0xff31d476, 0xff31ce72, 0xff25c666,
0xff14b652, 0xff0daa48, 0xff167c3b, 0xff304639, 0xff37403b, 0xff254f35, 0x69116631, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x43174126, 0xff125e30, 0xff0c8f3b, 0xff0eae48, 0xff23b55e, 0xff299f5a, 0xff4b524e, 0xff555655,
0xff515251, 0xff4d4e4e, 0xff4b4b4b, 0xff494a49, 0xff474848, 0xff36945e, 0xff25c96b, 0xff21c667,
0xff1ab35b, 0xff365f47, 0xff414241, 0xff424242, 0xff424342, 0xff424242, 0xff424242, 0xff3f4b44,
0xff1ea858, 0xff1cc263, 0xff1cc363, 0xff1bc363, 0xff1cc363, 0xff1dc464, 0xff1ec565, 0xff20c667,
0xff22c868, 0xff24ca6b, 0xff27cd6e, 0xff2bd171, 0xff2dd072, 0xff27bf66, 0xff3ade7f, 0xff29d36c,
0xff15b953, 0xff0eaa48, 0xff167d3c, 0xff2f4538, 0xff37413b, 0xff264c35, 0x6f106730, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x6d134b28, 0xff1c7740, 0xff139a44, 0xff0fb04a, 0xff34d174, 0xff1e7c45, 0xff4f5551, 0xff565857,
0xff525352, 0xff4d4e4e, 0xff4a4a4a, 0xff474747, 0xff454545, 0xff358c59, 0xff26cb6c, 0xff1bbc5f,
0xff248b4f, 0xff404241, 0xff424242, 0xff434343, 0xff434343, 0xff434343, 0xff434343, 0xff424242,
0xff376249, 0xff17b259, 0xff1dc564, 0xff1dc665, 0xff1ec666, 0xff1fc766, 0xff21c969, 0xff23cb6a,
0xff25cd6c, 0xff26cc6c, 0xff24c568, 0xff1bb35b, 0xff1aac57, 0xff37dc7c, 0xff3ce382, 0xff1ec960,
0xff18c359, 0xff0da846, 0xff1a7e3f, 0xff2f4639, 0xff39433d, 0xff274f37, 0x6b157037, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x9d10612d, 0xff1d7f41, 0xff2fa15a, 0xff10b24c, 0xff37db7a, 0xff237c48, 0xff515754, 0xff575958,
0xff535454, 0xff4f5050, 0xff4b4b4b, 0xff474848, 0xff444444, 0xff348756, 0xff16ac56, 0xff13aa53,
0xff336448, 0xff3f4040, 0xff414141, 0xff414141, 0xff414141, 0xff414141, 0xff414141, 0xff404040,
0xff3e4240, 0xff239452, 0xff14b458, 0xff1dc464, 0xff1fc867, 0xff1fc766, 0xff1dc264, 0xff1aba5e,
0xff15b057, 0xff11a550, 0xff10a14d, 0xff1eb65e, 0xff34db7a, 0xff38de7d, 0xff2ed270, 0xff16bc55,
0xff19b253, 0xff0ea948, 0xff227a42, 0xff31473a, 0xff3e4842, 0xff285539, 0x6118763a, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x03000000,
0xc714843d, 0xff18462b, 0xff326144, 0xff16b14f, 0xff35d978, 0xff2b8451, 0xff525754, 0xff575a58,
0xff555655, 0xff505150, 0xff4c4d4c, 0xff494949, 0xff454445, 0xff378759, 0xff12a651, 0xff19a956,
0xff3a4a40, 0xff3e3f3e, 0xff3f3f3f, 0xff3f3f3f, 0xff3f3f3f, 0xff3f3f3f, 0xff3f3f3f, 0xff3e3f3e,
0xff3d3d3d, 0xff326a4a, 0xff12ad54, 0xff10ac52, 0xff10ad53, 0xff10ab52, 0xff0fa850, 0xff0fa54f,
0xff11a750, 0xff1bb75d, 0xff29cd6e, 0xff30d877, 0xff32d878, 0xff32d375, 0xff20c361, 0xff1d9c4c,
0xff365f47, 0xff18a44d, 0xff24663c, 0xff344b3d, 0xff434d47, 0xff255b39, 0x5b19783b, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x0d142727,
0xe11b984b, 0xff133822, 0xff354b3d, 0xff1e9f4d, 0xff2fd170, 0xff2d8753, 0xff505753, 0xff585b59,
0xff565757, 0xff525453, 0xff4f504f, 0xff4b4c4b, 0xff474747, 0xff3b895b, 0xff18b058, 0xff23a158,
0xff3e3e3e, 0xff3f403f, 0xff3f403f, 0xff3f3f3f, 0xff3f3f3f, 0xff3f3f3f, 0xff3e3f3f, 0xff3e3e3e,
0xff3d3e3e, 0xff3b4941, 0xff1eae5b, 0xff10ac52, 0xff10ac53, 0xff11ab52, 0xff15b157, 0xff1ebf62,
0xff27ce6e, 0xff2ad271, 0xff2bd271, 0xff2dd172, 0xff2ecf71, 0xff2cc96d, 0xff18b454, 0xff346d48,
0xff3a4c42, 0xff23904c, 0xff28653e, 0xff394e41, 0xff48514c, 0xff1e6438, 0x531c7b3d, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x091c3939,
0xd7229d50, 0xff0e341c, 0xff3b4c41, 0xff2a884c, 0xff2ac969, 0xff2a8651, 0xff4c5550, 0xff565957,
0xff565857, 0xff545555, 0xff525352, 0xff4e4f4f, 0xff4b4b4b, 0xff3e885d, 0xff25c769, 0xff2a9757,
0xff424042, 0xff424342, 0xff424242, 0xff424242, 0xff414141, 0xff414141, 0xff414141, 0xff414141,
0xff404141, 0xff3f3f40, 0xff29a15b, 0xff14b257, 0xff15b459, 0xff1ec263, 0xff24cc6b, 0xff26cd6c,
0xff26cc6c, 0xff28cb6d, 0xff29cb6d, 0xff2ac96d, 0xff2bc86c, 0xff28c468, 0xff1ba850, 0xff405347,
0xff394c41, 0xff278b4e, 0xff2c7046, 0xff415248, 0xff46524b, 0xff166c38, 0x551b7b3c, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x93369c5c, 0xff19552f, 0xff435349, 0xff36744d, 0xff25c364, 0xff288a51, 0xff47514b, 0xff535754,
0xff535655, 0xff535554, 0xff525352, 0xff4f5150, 0xff4d4e4e, 0xff3f875c, 0xff27cb6d, 0xff2e8a54,
0xff454546, 0xff464746, 0xff464646, 0xff454645, 0xff454545, 0xff454545, 0xff454545, 0xff444545,
0xff444444, 0xff424242, 0xff328955, 0xff20c666, 0xff21ca69, 0xff23cb6a, 0xff24ca6a, 0xff25c96a,
0xff25c86a, 0xff26c669, 0xff27c569, 0xff28c56a, 0xff2ac66b, 0xff26c366, 0xff268f4c, 0xff404e46,
0xff374a3f, 0xff289251, 0xff30744a, 0xff4a5850, 0xff394d41, 0xff107537, 0x61127134, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x633ba562, 0xff116a31, 0xff3b4e43, 0xff426750, 0xff26be62, 0xff269454, 0xff414e47, 0xff4f5551,
0xff515452, 0xff515351, 0xff505150, 0xff4e504f, 0xff4c4d4d, 0xff3e855a, 0xff25c669, 0xff2e8050,
0xff464646, 0xff484948, 0xff484948, 0xff474848, 0xff474848, 0xff474747, 0xff474747, 0xff474747,
0xff464746, 0xff444545, 0xff387050, 0xff20c867, 0xff22ca69, 0xff23c969, 0xff23c769, 0xff24c568,
0xff24c267, 0xff24c066, 0xff25c166, 0xff27c569, 0xff2bc86c, 0xff24c364, 0xff317349, 0xff3d4d43,
0xff34473c, 0xff259150, 0xff387b51, 0xff4e5b53, 0xff22402e, 0xff10833d, 0x83127735, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x53319656, 0xff117738, 0xfb284533, 0xff4c6255, 0xff2ab961, 0xff24a359, 0xff3a4a41, 0xff4b534f,
0xff4f5350, 0xff4f5150, 0xff4e504f, 0xff4d4e4d, 0xff4b4b4b, 0xff3d8258, 0xff25c267, 0xff2c7c4d,
0xff434444, 0xff464847, 0xff474848, 0xff474847, 0xff474847, 0xff474747, 0xff474747, 0xff464747,
0xff454645, 0xff424443, 0xff3a6049, 0xff21c768, 0xff22ca69, 0xff23c869, 0xff23c567, 0xff26c368,
0xff2ccb6e, 0xff2dcc6f, 0xff24bf65, 0xff26c568, 0xff2cca6e, 0xff20bf5f, 0xff385f46, 0xff3a4b40,
0xff324439, 0xff218549, 0xff3d7e56, 0xff43554a, 0xfb113a20, 0xff19984b, 0xad13823c, 0x03005500,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x49268c49, 0xff13843f, 0xd11e5033, 0xf34f5f56, 0xff31b363, 0xff24b360, 0xff2f4438, 0xff48524c,
0xff4d524f, 0xff4d504f, 0xff4d4f4e, 0xff4b4d4c, 0xff494a4a, 0xff3b8258, 0xff25c267, 0xff2a7b4b,
0xff414241, 0xff454746, 0xff474747, 0xff474747, 0xff464747, 0xff464746, 0xff464746, 0xff454646,
0xff444645, 0xff414342, 0xff395645, 0xff22c768, 0xff23ca6a, 0xff24c769, 0xff25bd64, 0xff319b5b,
0xff3b7250, 0xff3b7e57, 0xff29b864, 0xff23c366, 0xff2bcc6e, 0xff22b55a, 0xff3b5144, 0xff37483e,
0xff2e3e35, 0xff207943, 0xff3d7c55, 0xe535493e, 0xe10e4824, 0xff1a9b4d, 0xc520994c, 0x05336633,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x43228144, 0xff169146, 0xb91e5d38, 0xaf44534a, 0xff37b568, 0xff26c268, 0xff224932, 0xff404e46,
0xff4b514d, 0xff4c4f4e, 0xff4b4e4c, 0xff4a4c4b, 0xff484948, 0xff3a8458, 0xff25c66a, 0xff27804c,
0xff3f4040, 0xff444645, 0xff464747, 0xff464747, 0xff464746, 0xff464646, 0xff454646, 0xff454645,
0xff434544, 0xff3f4240, 0xff375141, 0xff24c96a, 0xff25cd6c, 0xff26ba63, 0xff366648, 0xff444645,
0xff464947, 0xff474a49, 0xff3a7553, 0xff22be63, 0xff2bcc6e, 0xff25a957, 0xff3b4b42, 0xff35453c,
0xff2b3a31, 0xff237744, 0xff3a7d54, 0xc13d6e51, 0xab12532b, 0xf3189247, 0x8d208d4a, 0x03005555,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x3f207d41, 0xff199c4c, 0xc11d6339, 0x4534463f, 0xfb35b668, 0xff26c86a, 0xff185632, 0xff32433a,
0xff47504b, 0xff4b4f4c, 0xff4a4d4c, 0xff494b4a, 0xff464847, 0xff398658, 0xff26cb6c, 0xff258a4e,
0xff3b3c3b, 0xff454746, 0xff474847, 0xff474847, 0xff464747, 0xff464746, 0xff454646, 0xff444645,
0xff424443, 0xff3e4140, 0xff344e3e, 0xff22c567, 0xff27ce6d, 0xff33794f, 0xff424543, 0xff464947,
0xff494c4a, 0xff4a4d4b, 0xff424f48, 0xff22aa5b, 0xff29cc6c, 0xff279f55, 0xff394940, 0xff324339,
0xff313e36, 0xff267847, 0xff378d58, 0xc92c8f51, 0x17215937, 0x311f7d44, 0x0b177446, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x411f7d3f, 0xff1ba350, 0xc91c6539, 0x0f335544, 0xc72ebc66, 0xff27cc6d, 0xff17753e, 0xff26322b,
0xff3f4d45, 0xff494f4c, 0xff4a4d4b, 0xff494c4a, 0xff474848, 0xff398959, 0xff27cd6d, 0xff239352,
0xff333334, 0xff454846, 0xff494b4a, 0xff4a4b4a, 0xff484a49, 0xff474848, 0xff464847, 0xff454746,
0xff424543, 0xff3e413f, 0xff30503e, 0xff1dbc60, 0xff26c166, 0xff3d5045, 0xff454947, 0xff4a4d4b,
0xff4d504e, 0xff4c504d, 0xff3e4541, 0xff20894c, 0xff26cb6b, 0xff289e56, 0xff34483c, 0xff33433a,
0xff38453d, 0xff277a49, 0xff219d51, 0xd123954e, 0x07249249, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x451e8143, 0xff1eaa54, 0xd51d6a3b, 0x0f336644, 0xa329b561, 0xff25c769, 0xff1a8c4a, 0xff24392c,
0xff36493e, 0xff46504a, 0xff4a4f4c, 0xff4a4d4c, 0xff494a49, 0xff398e5b, 0xff28cf6f, 0xff229e57,
0xff2a2b2c, 0xff434745, 0xff4c4e4d, 0xff4d4e4d, 0xff4c4e4d, 0xff4b4c4b, 0xff494b4a, 0xff484a49,
0xff444745, 0xff404442, 0xff305841, 0xff1cbc60, 0xff26b560, 0xff434e48, 0xff4a4e4b, 0xff4e5250,
0xff505451, 0xff47504b, 0xff303a34, 0xff207945, 0xff25ca6a, 0xff29a559, 0xff34473b, 0xff3c4841,
0xff384a3f, 0xff267d4a, 0xff20a755, 0xe3239b50, 0x0b2ea25d, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x4d218b45, 0xff20af57, 0xdf1d713e, 0x0f336644, 0x5b249d54, 0xf925bd64, 0xff1f8348, 0xff2a4a36,
0xff314c3c, 0xff3d5045, 0xff46504a, 0xff484f4b, 0xff484d4a, 0xff3a945e, 0xff28d070, 0xff22ab5c,
0xff252a28, 0xff38403b, 0xff4c504d, 0xff4f5250, 0xff4f5150, 0xff4e504f, 0xff4d4f4e, 0xff4c4e4c,
0xff484b4a, 0xff454846, 0xff376449, 0xff20c466, 0xff27ba63, 0xff48544d, 0xff4e5250, 0xff505553,
0xff4b544f, 0xff39473f, 0xff26312a, 0xff207c46, 0xff24c969, 0xff2cb662, 0xff3c4c43, 0xff3c4a41,
0xeb2f4739, 0xe92b9356, 0xff1fa855, 0xf9219f50, 0x212e834d, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x5b22974c, 0xff22b45b, 0xe51d763f, 0x112d694b, 0x05336633, 0x311f8244, 0xb131523f, 0xff2e4d3a,
0xff2d4e3a, 0xff334f3e, 0xff3d5045, 0xff3f4d45, 0xff414c46, 0xff38985f, 0xff29d271, 0xff24b762,
0xff27332c, 0xff2d3631, 0xff454b47, 0xff4f5250, 0xff505251, 0xff505251, 0xff505251, 0xff4f5150,
0xff4d4f4e, 0xff4a4d4c, 0xff3c6c50, 0xff26cf6e, 0xff2ac469, 0xff495950, 0xff4c534f, 0xff49534d,
0xff404e46, 0xff2d3e34, 0xff23362b, 0xff218a4c, 0xff24c969, 0xff2ec76b, 0xff405147, 0xf531463a,
0x65263f30, 0xe52a9c59, 0xff1fac56, 0xff169445, 0x69319657, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x6b219b4c, 0xff24b85e, 0xef1e7b43, 0x19336647, 0x01000000, 0x01000000, 0x15313d31, 0xc7314a3b,
0xff2d4b39, 0xff304f3c, 0xff365040, 0xff3a4e42, 0xff394b41, 0xff349d5f, 0xff2ad572, 0xff26c368,
0xff364a3e, 0xff343b37, 0xff424845, 0xff4d4f4e, 0xff4f5150, 0xff505250, 0xff505251, 0xff4f5250,
0xff4e514f, 0xff4e504f, 0xff42634f, 0xff23c267, 0xff29b361, 0xff44544b, 0xff414e46, 0xff3e4d44,
0xff38493f, 0xff2d4135, 0xff273a2f, 0xff259d57, 0xff23c969, 0xff2cd370, 0xf932563f, 0x6f25392e,
0x393a7951, 0xf9279554, 0xff1ba753, 0xff0d863a, 0xe722954f, 0x55308a54, 0x314eac78, 0x2d5bbb88,
0x2155aa83, 0x153d7961, 0x05333333, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x8b21a14f, 0xff20b058, 0xf919763e, 0x25306745, 0x01000000, 0x01000000, 0x01000000, 0x232c4233,
0xcb2d4537, 0xff2e4838, 0xff304a3b, 0xff374c3f, 0xff3d4d44, 0xff36a763, 0xff2bd874, 0xff25ca6b,
0xff3d5a49, 0xff424644, 0xff454947, 0xff484b49, 0xff494c4a, 0xff4b4d4c, 0xff4c4e4d, 0xff4b4e4c,
0xff4b4d4c, 0xff4a4d4b, 0xff494e4a, 0xff397753, 0xff3d674e, 0xff404944, 0xff3d4942, 0xff3f4943,
0xff3b4640, 0xff35423b, 0xff36433b, 0xff29a85e, 0xff22c768, 0xff29d36f, 0x7f266c40, 0x0d3b624e,
0x9b3e8d5e, 0xff1e8b4b, 0xff1ba551, 0xff1c9b4d, 0xff3ac973, 0xfd48d182, 0xf739aa68, 0xf344b574,
0xeb4bbe7c, 0xdb51c987, 0xbb56cf8b, 0x8356c888, 0x3951b379, 0x03005555, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0xb120a550, 0xff20b058, 0xff1a7c41, 0x713f9563, 0x854bb479, 0x9546af73, 0x67409e68, 0x09395539,
0x1b26392f, 0xbb2d3f34, 0xff304137, 0xff334339, 0xff3a483f, 0xff36af67, 0xff2cdb76, 0xff26cf6d,
0xff376249, 0xff3e423f, 0xff3f4341, 0xff3f4341, 0xff404341, 0xff424443, 0xff434544, 0xff434544,
0xff434544, 0xff424543, 0xff414442, 0xff414342, 0xff3f4341, 0xff3d4240, 0xff3d433f, 0xff3d433f,
0xff3a433e, 0xff3b443e, 0xff3c4540, 0xff249754, 0xff1fc163, 0xff24c767, 0x3d1d8247, 0x974ea774,
0xfd317f51, 0xff198746, 0xff1ca352, 0xff1f8d4b, 0xff49dc85, 0xff5af79b, 0xff56e695, 0xff31a663,
0xff249050, 0xff289655, 0xff30a25f, 0xff39b06a, 0xf743be76, 0xab49bd7a, 0x17439b64, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x03005555, 0x1343865e, 0x2d4fb57d, 0x554eb478,
0xe920af54, 0xff1dab54, 0xff16773d, 0xff49b877, 0xff49bd7a, 0xff48b877, 0xf34cbd7d, 0x273b7c55,
0x01000000, 0x0d272727, 0x8b303b35, 0xf5353d38, 0xff363e39, 0xff33b065, 0xff2ddd78, 0xff26d26f,
0xff316b48, 0xff383b39, 0xff383b39, 0xff393c3a, 0xff3a3c3a, 0xff3b3d3c, 0xff3c3e3d, 0xff3c3e3c,
0xff3b3d3c, 0xff3b3d3c, 0xff3a3d3b, 0xff393c3b, 0xff383b39, 0xff363a38, 0xff353a37, 0xff333936,
0xff343a37, 0xff353b37, 0xff333936, 0xd325593b, 0xd922af5b, 0xb51fad57, 0x15246d3d, 0xa53f9464,
0xfd2b794b, 0xff1a8e49, 0xff1da654, 0xff0b652f, 0xff1e8c4a, 0xff41cd7a, 0xff52ed91, 0xff59f79c,
0xff56ec97, 0xff49d586, 0xff3fc579, 0xff3bbe73, 0xff39ba70, 0xfb2ca75e, 0x7345b171, 0x01000000,
0x01000000, 0x01000000, 0x1f429463, 0x6f50bf7e, 0xaf55cc86, 0xd957d38a, 0xf15ada8f, 0xfd4fd385,
0xff12a648, 0xff159e4a, 0xff116f36, 0xff49b375, 0xff50be7e, 0xff49b275, 0xfd48ad72, 0xbf47a36d,
0x213e835d, 0x01000000, 0x05333333, 0x472f3632, 0xcd333a37, 0xff32b466, 0xff2bdc76, 0xff26d06e,
0xff2b7347, 0xff343534, 0xff343735, 0xff343635, 0xff353736, 0xff363837, 0xff363837, 0xff363837,
0xff363837, 0xff353736, 0xff343735, 0xff333634, 0xff323533, 0xff303432, 0xff2e3330, 0xff2c312e,
0xff2b312e, 0xeb2c322d, 0x7d2d312f, 0x13282828, 0x131b5128, 0x091c551c, 0x01000000, 0x0f225533,
0x57205b38, 0xdf1ea052, 0xf720ab57, 0xe90f6733, 0xf9107338, 0xfd117f3d, 0xfd21a155, 0xfd2ab562,
0xfb2dbd67, 0xf32abb63, 0xe328b960, 0xcb24b05a, 0x9f1ea253, 0x59289b56, 0x0d3b894e, 0x01000000,
0x0d3b8962, 0x834ab377, 0xed4dc77f, 0xff4cc87f, 0xff4fcb81, 0xff55d58a, 0xff51cd85, 0xff25ad59,
0xff12a84a, 0xff1ea854, 0xff0a652e, 0xff3c9761, 0xff49a870, 0xff3d9562, 0xff398d5c, 0xff378a5a,
0x633b865a, 0x01000000, 0x01000000, 0x01000000, 0x111e3c2d, 0xc72dbc66, 0xff27d370, 0xff23c668,
0xff25653f, 0xff2c2e2d, 0xff2e302e, 0xff2e302f, 0xff2e302f, 0xff2e302f, 0xff2f3130, 0xff2f3130,
0xff2e302f, 0xff2e302e, 0xff2d2f2e, 0xff2b2e2c, 0xff2a2d2b, 0xff272b29, 0xfd262a27, 0xe7252927,
0x8f272b27, 0x21272727, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0xcb23b25d, 0xcd25bd61, 0x19145229, 0x1f195231, 0x2714552e, 0x29135d32, 0x29136332,
0x25156737, 0x19146633, 0x0d14893b, 0x07246d24, 0x03005500, 0x01000000, 0x01000000, 0x01000000,
0x6946b172, 0xfd3db26d, 0xff3db06c, 0xff3fb36e, 0xff3eb16d, 0xff38a966, 0xff289453, 0xff289e57,
0xff3ad376, 0xff2cba63, 0xff0c662f, 0xff1d673b, 0xff2d7a4d, 0xff2f7b4f, 0xff2d774c, 0xb5337c50,
0x1737644e, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x71249e56, 0xf526bf67, 0xe323af5e,
0xa7223e2c, 0xd9242524, 0xf7242524, 0xff242625, 0xff252726, 0xff252726, 0xff262726, 0xff252726,
0xff242625, 0xff232524, 0xff222322, 0xfb1f2220, 0xe51f2120, 0xb51f201f, 0x631f211f, 0x191f1f1f,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x07242424, 0xdf1eaf58, 0xdb26c163, 0x1518613d, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x1d359e61, 0xbb34ae64, 0xff3db26d, 0xff3caf6b, 0xff39a966, 0xff329e5d, 0xff2b9254, 0xff25864c,
0xff1a9f4a, 0xff18ac4b, 0xff106b31, 0xff267546, 0xff2b784b, 0xfb307e50, 0x8d33774f, 0x0f336644,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x07246d49, 0x33288c50, 0x211f7446,
0x01000000, 0x131b1b1b, 0x371c1c1c, 0x631c1c1c, 0x851b1d1d, 0xa31b1c1b, 0xad1b1c1b, 0xaf1a1c1c,
0xa7181a1a, 0x8f191b19, 0x6f171919, 0x43171b1b, 0x1d1a1a1a, 0x05000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x1d1a462c, 0xf11bae55, 0xef21be5e, 0x29258f4b, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x1f3a9463, 0xe74bc47c, 0xff3cae6a, 0xff36a563, 0xff339d5e, 0xff30975a, 0xff2e9156,
0xff139740, 0xff16a949, 0xff0f6d32, 0xff308955, 0xff3b9661, 0xe945a36c, 0x5f3e8c5e, 0x1f31734a,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x03000000, 0x03000000, 0x03000000,
0x03000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x4f176434, 0xfb16a850, 0xff1cbb5a, 0xa722a352, 0x9b27a854, 0xa743ae6c, 0xa146b170, 0x7f46b372,
0x493fa165, 0x153d9261, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x091c5539, 0xd934b165, 0xff2ea15d, 0xff309d5d, 0xff2f995b, 0xff2e9558, 0xff2d9056,
0xff13933f, 0xff0f933d, 0xff0c6e30, 0xff28874f, 0xff2c8952, 0xff29834e, 0xff257b48, 0xd121663e,
0x172c5937, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x25296745,
0xc3146835, 0xff179e4c, 0xff32da74, 0xff2ead61, 0xff38b86a, 0xff52cf85, 0xff51cd84, 0xff4ec981,
0xff4dc880, 0xe14ac27b, 0x7b46b674, 0x0d3b9d62, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x691d9b4b, 0xd523b05b, 0xf12abe66, 0xfb34c772, 0xfb34c370, 0xf931ba6a,
0xe918984b, 0xcf12843f, 0xaf117f3f, 0xa31b8646, 0xaf1f8847, 0xa516783e, 0x79136735, 0x2f104626,
0x03000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x412b6e47, 0xe11c6d3d,
0xff146e38, 0xff149945, 0xff1ac357, 0xff1e8d4a, 0xff299957, 0xff31a460, 0xff37ac68, 0xff3bb16c,
0xff3cb36e, 0xff3db56f, 0xfd3fb972, 0x9145b574, 0x03005555, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x07249249, 0x131b7943, 0x1d1a723e, 0x1f196b3a, 0x191f663d,
0x0d14763b, 0x39248648, 0xb111863b, 0xe9248a4b, 0xf539a264, 0xe932955b, 0x992a7b4b, 0x05333333,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x0f225544, 0x1b266842, 0x15317955, 0x05336633, 0xc1166235, 0xff106f36,
0xff11763a, 0xff108b40, 0xff13ae4c, 0xff11813e, 0xff109644, 0xff1da453, 0xff269c57, 0xff29a05b,
0xff2ca660, 0xff29a85e, 0xf920a555, 0x9723a054, 0x03005555, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x03005555, 0x9f25a055, 0xf926b25f, 0xff26b05e, 0xfd1b964e, 0xeb158543, 0x9b156938, 0x05333333,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x43175831, 0xd5166335, 0xef176937, 0xe71e7a44, 0x8b238849, 0x5d166e37, 0xc1169047,
0xdf179b4e, 0xed1ba955, 0xf31ebc5e, 0xf524bc63, 0xf331ce73, 0xef41e488, 0xe741e187, 0xd93fdd86,
0xbf35d679, 0x9326bf65, 0x491fa450, 0x07246d24, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x112d964b, 0x3725a258, 0x4923a457, 0x411b954e, 0x211f8b4d, 0x05336633, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x311a6d39, 0xa31b8b49, 0xc91c9a51, 0xaf22a256, 0x491c8f49, 0x01000000, 0x091c5539,
0x17218543, 0x211fa255, 0x2721a355, 0x2b1ea659, 0x291fa857, 0x231da857, 0x1d1a9e4f, 0x131b7943,
0x07244924, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x03005555, 0x0300aa55, 0x03005555, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000,
0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000, 0x01000000
};

// cLinuxMain_SetIcon
void cLinuxMain_SetIcon() {
    GdkPixbuf *P = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, 64, 64);
    void *Dst = gdk_pixbuf_get_pixels(P);
    memcpy(Dst, cLinuxMain_Icon, 64 * 64 * 4);
    gtk_window_set_icon(GTK_WINDOW(cLinuxMain_Window), P);
    gdk_pixbuf_unref(P);
    P = nullptr;
}

//-----------------------------------------------------------------------------
// cLinuxMain_FillCodes
//-----------------------------------------------------------------------------
static void cLinuxMain_FillCodes() {
    // Convert key syms to codes
    GdkKeymapKey *K = nullptr;
    int i, n, h;
    for(i = 0; i < cLinuxMain_KeySyms.Count(); i++) {
        h = 0;
        if(gdk_keymap_get_entries_for_keyval(nullptr, cLinuxMain_KeySyms[i], &K, &n)) {
            h = K[0].keycode;
            g_free(K);
        }
        cLinuxMain_Codes.Add(h);
    }

    comms::cList<int> S;
    // Enter Codes
    S.Clear();
    S.Add(GDK_Return);     // Return
    S.Add(GDK_KP_Enter);   // Keypad Enter
    for(i = 0; i < S.Count(); i++) {
        h = 0;
        if(gdk_keymap_get_entries_for_keyval(nullptr, S[i], &K, &n)) {
            h = K[0].keycode;
            g_free(K);
        }
        cLinuxMain_EnterCodes.Add(h);
    }
    // Shift Codes
    S.Clear();
    S.Add(GDK_Shift_L);    // Left Shift
    S.Add(GDK_Shift_R);    // Right Shift
    for(i = 0; i < S.Count(); i++) {
        h = 0;
        if(gdk_keymap_get_entries_for_keyval(nullptr, S[i], &K, &n)) {
            h = K[0].keycode;
            g_free(K);
        }
        cLinuxMain_ShiftCodes.Add(h);
    }
    // Control Codes
    S.Clear();
    S.Add(GDK_Control_L);  // Left Control
    S.Add(GDK_Control_R);  // Right Control
    for(i = 0; i < S.Count(); i++) {
        h = 0;
        if(gdk_keymap_get_entries_for_keyval(nullptr, S[i], &K, &n)) {
            h = K[0].keycode;
            g_free(K);
        }
        cLinuxMain_ControlCodes.Add(h);
    }
    // Alt Codes
    S.Clear();
    S.Add(GDK_Alt_L);      // Left Alt
    S.Add(GDK_Alt_R);      // Right Alt
    S.Add(GDK_Super_L);    // Left Super
    S.Add(GDK_Super_R);    // Right Super
    for(i = 0; i < S.Count(); i++) {
        h = 0;
        if(gdk_keymap_get_entries_for_keyval(nullptr, S[i], &K, &n)) {
            h = K[0].keycode;
            g_free(K);
        }
        cLinuxMain_AltCodes.Add(h);
    }
} // cLinuxMain_FillCodes

// cLinuxMain_UrlDecode
static void cLinuxMain_UrlDecode(comms::cStr *S) {
    int i, c;
    for(i = 0; i < S->Length(); i++) {
        const char *r = &S->ToCharPtr()[i];
        if('%' == *r) {
            if(sscanf(r + 1, "%2x", &c)) {
                S->Remove(i, 3);
                S->Insert(i, (char)c);
            }
        }
    }
}

static comms::cFile cLinuxMain_DropFile;

#ifdef COMMS_CURL
// cLinuxMain_DropWrite
static size_t cLinuxMain_DropWrite(void *buffer, size_t size, size_t nmemb, void *userp) {
    cLinuxMain_DropFile.WriteBytes(buffer, nmemb);
    return nmemb;
}
#endif // COMMS_CURL

// cLinuxMain_DragDataReceived
void cLinuxMain_DragDataReceived(GtkWidget *, GdkDragContext *Context, gint X, gint Y, GtkSelectionData *Data, guint, guint Time, gpointer *) {
    // Drop free
    cLinuxMain_DropFile.Clear();
    comms::cMain_OnDropImage.Free();
    // Update mouse pos with event
    comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
    E->Type = comms::cInput::OldEvent::TYPE_MOUSEMOVE;
    cLinuxMain_LocalToLocal(X, Y, &E->MousePos);
    E->MouseDelta.SetZero();
    comms::cInput::AddEvent(E);
    E = nullptr;
    while(comms::cInput::GetEvent()) {
    }
    gchar *Ptr = (char*)Data->data;
    if((0 == strncmp(Ptr, "file:///", 8)) && (strlen(Ptr) < 4096)) { // Local pathname
        Ptr += (7 * sizeof(char));
        int l = strlen(Ptr) - 1;
        while((Ptr[l] == ' ') || (Ptr[l] == '\n') || (Ptr[l]=='\r')) {
            Ptr[l] = '\0';
            l--;
        }
        comms::cStr S = Ptr;
        cLinuxMain_UrlDecode(&S);
        if(g_file_test(S, G_FILE_TEST_EXISTS)) {
            if(!g_file_test(S, G_FILE_TEST_IS_DIR)) {
                // Callback
                comms::cMain_OnDrop(S);
                gtk_window_present(GTK_WINDOW(cLinuxMain_Window));
            }
        }
    } else { // Web image
        comms::cStr S = Ptr;
        int i = S.IndexOf('?'); // Cutoff '?' suffix
        if(i != -1) {
            S.Remove(i);
        }
        // File extension
        comms::cStr E = S.GetFileExtension();
	if(!E.IsEmpty()) {
            // Search codec
            const comms::cList<comms::cImageCodecInfo> &Codecs = comms::cIO::GetImageCodecs();
            comms::cImageCodec *C = nullptr;
            for(i = 0; i < Codecs.Count(); i++) {
                if(comms::cStr::EqualsNoCase(Codecs[i].FileExtension, E)) {
                    C = Codecs[i].Codec;
                    break;
                }
            }
            if(C != nullptr) {
#ifdef COMMS_CURL
                CURL *U = curl_easy_init();
                if(U != nullptr) {
                    if(CURLE_OK == curl_easy_setopt(U, CURLOPT_URL, S.ToCharPtr())) {
                        if(CURLE_OK == curl_easy_setopt(U, CURLOPT_WRITEFUNCTION, cLinuxMain_DropWrite)) {
                            if(CURLE_OK == curl_easy_perform(U)) {
                                // Decode image
								cLinuxMain_DropFile.SetPos(0);
                                if(C->Decode(cLinuxMain_DropFile, &comms::cMain_OnDropImage)) {
                                    // Callback
                                    comms::cMain_OnDrop(nullptr);
                                    gtk_window_present(GTK_WINDOW(cLinuxMain_Window));
                                }
                            }
                        }
                    }
                    curl_easy_cleanup(U);
                    U = nullptr;
                }
#endif // COMMS_CURL
			} else {
				comms::cMain_OnDropURL(S.ToCharPtr());
			}
		} else {
			comms::cMain_OnDropURL(S.ToCharPtr());
		}
    }
    gtk_drag_finish(Context, TRUE, FALSE, Time);
}

// cLinuxMain_DragDrop
gboolean cLinuxMain_DragDrop(GtkWidget *Widget, GdkDragContext *Context, gint, gint, guint Time, gpointer *) {
    GdkAtom target_type;
    if(Context->targets) {
        target_type = GDK_POINTER_TO_ATOM(g_list_nth_data(Context->targets, 0));
        gtk_drag_get_data(Widget, Context, target_type, Time);
        return 1;
    }
    return 0;
}

// cLinuxMain_LoadCursors
static void cLinuxMain_LoadCursors() {
    GdkDisplay *D = gdk_display_get_default();
    cLinuxMain_Cursors[0] = gdk_cursor_new_for_display(D, GDK_LEFT_PTR);            // Arrow
    cLinuxMain_Cursors[1] = gdk_cursor_new_for_display(D, GDK_SB_UP_ARROW);         // UpArrow
    cLinuxMain_Cursors[2] = gdk_cursor_new_for_display(D, GDK_SB_H_DOUBLE_ARROW);   // SizeHor
    cLinuxMain_Cursors[3] = gdk_cursor_new_for_display(D, GDK_SB_V_DOUBLE_ARROW);   // SizeVert
    cLinuxMain_Cursors[4] = gdk_cursor_new_for_display(D, GDK_BOTTOM_LEFT_CORNER);  // SizeSlash
    cLinuxMain_Cursors[5] = gdk_cursor_new_for_display(D, GDK_BOTTOM_RIGHT_CORNER); // SizeBackSlash
    cLinuxMain_Cursors[6] = gdk_cursor_new_for_display(D, GDK_FLEUR);               // SizeAll
    cLinuxMain_Cursors[7] = gdk_cursor_new_for_display(D, GDK_XTERM);               // IBeam
    cLinuxMain_Cursors[8] = gdk_cursor_new_for_display(D, GDK_TCROSS);              // Cross
    cLinuxMain_Cursors[9] = gdk_cursor_new_for_display(D, GDK_WATCH);               // Wait
    cLinuxMain_Cursors[10] = gdk_cursor_new_for_display(D, GDK_CIRCLE);             // Stop
    // GDK_BLANK_CURSOR under CentOS 5.9 leads to error:
    // The program '<unknown>' received an X Window System error.
    // The error was 'BadValue (integer parameter out of range for operation)'.
    // Therefore we should create the "None" cursor from empty bitmap.
    GdkColor Black = { 0, 0, 0, 0 };
    static char Bits[] = { 0x00 };
    GdkBitmap *Empty = gdk_bitmap_create_from_data(nullptr, Bits, 1, 1);
    cLinuxMain_Cursors[11] = gdk_cursor_new_from_pixmap(Empty, Empty, &Black, &Black, 0, 0);
    gboolean a = gdk_display_supports_cursor_alpha(D);
    gboolean c = gdk_display_supports_cursor_color(D);
    if(!a || !c) {
        return;
    }
    guint w = 0, h = 0;
    gdk_display_get_maximal_cursor_size(D, &w, &h);
    if(w < 32 || h < 32) {
        return;
    }
    comms::cFile F;
    comms::cCodecBmp C;
    comms::cImage I;
    GdkPixbuf *P = nullptr;
    int W = 0, H = 0;
    guchar *Src = nullptr;
    size_t S = 0;
    if(comms::cIO::LoadFile("Cursors/Arrow.bmp", &F, false)) {
        if(C.Decode(F, &I)) {
            I.Flip();
            W = I.GetWidth();
            H = I.GetHeight();
            P = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, W, H);
            Src = gdk_pixbuf_get_pixels(P);
            S = 4 * W * H;
            memcpy(Src, I.GetPixels(), S);
            cLinuxMain_Cursors[0] = gdk_cursor_new_from_pixbuf(D, P, 0, 0);
            g_object_unref(P);
            P = nullptr;
        }
    }
}

int main(int argc, char** argv) {
    gtk_init(nullptr, nullptr);
	comms::cMutex::GetInstance();
    cLinuxMain_ParseCmdLineArgs(argc, argv);
    
    sigset_t signame;
    struct sigaction sa_new, sa_old;
    
    // Set up the signal mask
    sigemptyset(&signame);
    sigaddset(&signame, SIGSEGV);
    sigaddset(&signame, SIGBUS);

    // Set up the signal handler
    sa_new.sa_handler = CrashHandler;
    sa_new.sa_mask = signame;
    sa_new.sa_flags = 0;

    // Install the signal handler
    sigaction(SIGSEGV, &sa_new, &sa_old);
    sigaction(SIGBUS, &sa_new, &sa_old);

    cLinuxMain_KeySyms.Add(GDK_Escape);                 // Esc
    cLinuxMain_KeySyms.Add(GDK_F1);                     // F1
    cLinuxMain_KeySyms.Add(GDK_F2);                     // F2
    cLinuxMain_KeySyms.Add(GDK_F3);                     // F3
    cLinuxMain_KeySyms.Add(GDK_F4);                     // F4
    cLinuxMain_KeySyms.Add(GDK_F5);                     // F5
    cLinuxMain_KeySyms.Add(GDK_F6);                     // F6
    cLinuxMain_KeySyms.Add(GDK_F7);                     // F7
    cLinuxMain_KeySyms.Add(GDK_F8);                     // F8
    cLinuxMain_KeySyms.Add(GDK_F9);                     // F9
    cLinuxMain_KeySyms.Add(GDK_F10);                    // F10
    cLinuxMain_KeySyms.Add(GDK_F11);                    // F11
    cLinuxMain_KeySyms.Add(GDK_F12);                    // F12
    cLinuxMain_KeySyms.Add(GDK_0);                      // Zero, "0"
    cLinuxMain_KeySyms.Add(GDK_1);                      // One, "1"
    cLinuxMain_KeySyms.Add(GDK_2);                      // Two, "2"
    cLinuxMain_KeySyms.Add(GDK_3);                      // Three, "3"
    cLinuxMain_KeySyms.Add(GDK_4);                      // Four, "4"
    cLinuxMain_KeySyms.Add(GDK_5);                      // Five, "5"
    cLinuxMain_KeySyms.Add(GDK_6);                      // Six, "6"
    cLinuxMain_KeySyms.Add(GDK_7);                      // Seven, "7"
    cLinuxMain_KeySyms.Add(GDK_8);                      // Eight, "8"
    cLinuxMain_KeySyms.Add(GDK_9);                      // Nine, "9"
    cLinuxMain_KeySyms.Add(GDK_minus);                  // Minus, "-"
    cLinuxMain_KeySyms.Add(GDK_equal);                  // Equals, "="
    cLinuxMain_KeySyms.Add(GDK_BackSpace);              // BackSpace
    cLinuxMain_KeySyms.Add(GDK_Tab);                    // Tab
    cLinuxMain_KeySyms.Add(GDK_Caps_Lock);              // CapsLock
    cLinuxMain_KeySyms.Add(GDK_Insert);                 // Insert
    cLinuxMain_KeySyms.Add(GDK_Delete);                 // Delete
    cLinuxMain_KeySyms.Add(GDK_Home);                   // Home
    cLinuxMain_KeySyms.Add(GDK_End);                    // End
    cLinuxMain_KeySyms.Add(GDK_Page_Up);                // PageUp
    cLinuxMain_KeySyms.Add(GDK_Page_Down);              // PageDown
    cLinuxMain_KeySyms.Add(GDK_Up);                     // Up
    cLinuxMain_KeySyms.Add(GDK_Down);                   // Down
    cLinuxMain_KeySyms.Add(GDK_Left);                   // Left
    cLinuxMain_KeySyms.Add(GDK_Right);                  // Right
    cLinuxMain_KeySyms.Add(GDK_backslash);              // BackSlash, "\\"
    cLinuxMain_KeySyms.Add(GDK_Return);                 // Enter
    cLinuxMain_KeySyms.Add(GDK_bracketleft);            // LeftBracket, "["
    cLinuxMain_KeySyms.Add(GDK_bracketright);           // RightBracket, "]"
    cLinuxMain_KeySyms.Add(GDK_semicolon);              // SemiColon, ";"
    cLinuxMain_KeySyms.Add(GDK_apostrophe);             // SingleQuote, "\'"
    cLinuxMain_KeySyms.Add(GDK_comma);                  // Comma, ","
    cLinuxMain_KeySyms.Add(GDK_period);                 // Period, "."
    cLinuxMain_KeySyms.Add(GDK_slash);                  // Slash, "/"
    cLinuxMain_KeySyms.Add(GDK_Shift_L);                // Shift
    cLinuxMain_KeySyms.Add(GDK_Control_L);              // Control
    cLinuxMain_KeySyms.Add(GDK_Alt_L);                  // Alt
    cLinuxMain_KeySyms.Add(GDK_space);                  // Space
    cLinuxMain_KeySyms.Add(GDK_grave);                  // Tilda, "~"
    cLinuxMain_KeySyms.Add(GDK_A);                      // A
    cLinuxMain_KeySyms.Add(GDK_B);                      // B
    cLinuxMain_KeySyms.Add(GDK_C);                      // C
    cLinuxMain_KeySyms.Add(GDK_D);                      // D
    cLinuxMain_KeySyms.Add(GDK_E);                      // E
    cLinuxMain_KeySyms.Add(GDK_F);                      // F
    cLinuxMain_KeySyms.Add(GDK_G);                      // G
    cLinuxMain_KeySyms.Add(GDK_H);                      // H
    cLinuxMain_KeySyms.Add(GDK_I);                      // I
    cLinuxMain_KeySyms.Add(GDK_J);                      // J
    cLinuxMain_KeySyms.Add(GDK_K);                      // K
    cLinuxMain_KeySyms.Add(GDK_L);                      // L
    cLinuxMain_KeySyms.Add(GDK_M);                      // M
    cLinuxMain_KeySyms.Add(GDK_N);                      // N
    cLinuxMain_KeySyms.Add(GDK_O);                      // O
    cLinuxMain_KeySyms.Add(GDK_P);                      // P
    cLinuxMain_KeySyms.Add(GDK_Q);                      // Q
    cLinuxMain_KeySyms.Add(GDK_R);                      // R
    cLinuxMain_KeySyms.Add(GDK_S);                      // S
    cLinuxMain_KeySyms.Add(GDK_T);                      // T
    cLinuxMain_KeySyms.Add(GDK_U);                      // U
    cLinuxMain_KeySyms.Add(GDK_V);                      // V
    cLinuxMain_KeySyms.Add(GDK_W);                      // W
    cLinuxMain_KeySyms.Add(GDK_X);                      // X
    cLinuxMain_KeySyms.Add(GDK_Y);                      // Y
    cLinuxMain_KeySyms.Add(GDK_Z);                      // Z
    cLinuxMain_KeySyms.Add(GDK_KP_0);                   // NumPad0
    cLinuxMain_KeySyms.Add(GDK_KP_1);                   // NumPad1
    cLinuxMain_KeySyms.Add(GDK_KP_2);                   // NumPad2
    cLinuxMain_KeySyms.Add(GDK_KP_3);                   // NumPad3
    cLinuxMain_KeySyms.Add(GDK_KP_4);                   // NumPad4
    cLinuxMain_KeySyms.Add(GDK_KP_5);                   // NumPad5
    cLinuxMain_KeySyms.Add(GDK_KP_6);                   // NumPad6
    cLinuxMain_KeySyms.Add(GDK_KP_7);                   // NumPad7
    cLinuxMain_KeySyms.Add(GDK_KP_8);                   // NumPad8
    cLinuxMain_KeySyms.Add(GDK_KP_9);                   // NumPad9
    cLinuxMain_KeySyms.Add(GDK_KP_Add);                 // Add
    cLinuxMain_KeySyms.Add(GDK_KP_Subtract);            // Subtract
    cLinuxMain_KeySyms.Add(GDK_KP_Multiply);            // Multiply
    cLinuxMain_KeySyms.Add(GDK_KP_Divide);              // Divide
    cLinuxMain_KeySyms.Add(GDK_KP_Decimal);             // Decimal
    cLinuxMain_FillCodes();
    cLinuxMain_Repeated.SetCount(cLinuxMain_Codes.Count(), false);

    comms::cMain_OnPreInit();
    cLinuxMain_LoadCursors();

    cLinuxMain_Display = GDK_DISPLAY();
    XVisualInfo *vi = nullptr;
#ifdef COMMS_OPENGL
    int Attributes[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE,           8,
        GLX_GREEN_SIZE,         8,
        GLX_BLUE_SIZE,          8,
        GLX_ALPHA_SIZE,         8,
        GLX_DEPTH_SIZE,         24,
        GLX_STENCIL_SIZE,       8,
        GLX_SAMPLE_BUFFERS_ARB, 0,
        GLX_SAMPLES_ARB,        0,
        0
    };
    vi = glXChooseVisual(cLinuxMain_Display, DefaultScreen(cLinuxMain_Display), Attributes);
#endif // COMMS_OPENGL
    if(nullptr == vi) {
        comms::cLog::Error("Couldn't choose visual");
        return -1;
    }
    GdkVisual *v = gdkx_visual_get(vi->visualid);
    GdkColormap *cm = gdk_colormap_new(v, TRUE);
    gtk_widget_push_colormap(cm);
    cLinuxMain_Window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    cLinuxMain_SetIcon();
    gtk_widget_set_double_buffered(GTK_WIDGET(cLinuxMain_Window), FALSE);
    gtk_window_set_policy(GTK_WINDOW(cLinuxMain_Window), TRUE, TRUE, FALSE);
    gtk_window_set_title(GTK_WINDOW(cLinuxMain_Window), comms::cMain_Title.ToCharPtr());
    gtk_widget_set_usize(cLinuxMain_Window, 800, 600);
    gtk_signal_connect(GTK_OBJECT(cLinuxMain_Window), "delete_event", GTK_SIGNAL_FUNC(cLinuxMain_DeleteHandler), nullptr);
    gtk_container_set_reallocate_redraws(GTK_CONTAINER(cLinuxMain_Window), TRUE);
    gtk_container_set_resize_mode(GTK_CONTAINER(cLinuxMain_Window), GTK_RESIZE_IMMEDIATE);
    cLinuxMain_DrawingArea = gtk_drawing_area_new();
    gtk_widget_set_double_buffered(GTK_WIDGET(cLinuxMain_DrawingArea), FALSE);
    gtk_container_add(GTK_CONTAINER(cLinuxMain_Window), cLinuxMain_DrawingArea);
    gtk_widget_set_events(cLinuxMain_DrawingArea, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
    // We should init extension events before showing window.
    // Otherwise CentOS 5.5 doesn't send events from the tablet device.
    gtk_widget_set_extension_events (cLinuxMain_DrawingArea, GDK_EXTENSION_EVENTS_CURSOR);
    gtk_widget_show(cLinuxMain_DrawingArea);
    gtk_widget_show(cLinuxMain_Window);
    GdkWindowObject *p = (GdkWindowObject *)GDK_WINDOW(cLinuxMain_DrawingArea->window);
    p->bg_pixmap = GDK_NO_BG;
#ifdef COMMS_OPENGL
    cLinuxMain_OglContext = glXCreateContext(cLinuxMain_Display, vi, 0, GL_TRUE);
    if(nullptr == cLinuxMain_OglContext) {
        comms::cLog::Error("Couldn't create context");
        return -1;
    }
#endif // COMMS_OPENGL
    XFree(vi);
    vi = nullptr;
#ifdef COMMS_OPENGL
    cLinuxMain_Drawable = GDK_WINDOW_XID(cLinuxMain_DrawingArea->window);
    if(!glXMakeCurrent(cLinuxMain_Display, cLinuxMain_Drawable, cLinuxMain_OglContext)) {
        comms::cLog::Error("Couldn't make context current");
        glXDestroyContext(cLinuxMain_Display, cLinuxMain_OglContext);
        return -1;
    }
#endif // COMMS_OPENGL
    gtk_widget_pop_colormap();
    gtk_window_maximize(GTK_WINDOW(cLinuxMain_Window)); // After OpenGL init to workaround "solid white window" bug
    cLinuxMain_WhileEvents(); // This event loop is for finishing "maximize" call
    if(comms::cRender::Init()) {
        comms::cMain_OnInit();

        g_idle_add_full(G_PRIORITY_HIGH_IDLE, cLinuxMain_OnRender, nullptr, nullptr);
        g_signal_connect(G_OBJECT(cLinuxMain_DrawingArea), "button_press_event", G_CALLBACK(cLinuxMain_MouseButtonHandler), nullptr);
        g_signal_connect(G_OBJECT(cLinuxMain_DrawingArea), "button_release_event", G_CALLBACK(cLinuxMain_MouseButtonHandler), nullptr);
        g_signal_connect(G_OBJECT(cLinuxMain_DrawingArea), "motion_notify_event", G_CALLBACK(cLinuxMain_MouseMotionHandler), nullptr);
        g_signal_connect(G_OBJECT(cLinuxMain_DrawingArea), "scroll_event", G_CALLBACK(cLinuxMain_MouseScrollHandler), nullptr);
        g_signal_connect(G_OBJECT(cLinuxMain_Window), "key_press_event", G_CALLBACK(cLinuxMain_KeyboardHandler), nullptr);
        g_signal_connect(G_OBJECT(cLinuxMain_Window), "key_release_event", G_CALLBACK(cLinuxMain_KeyboardHandler), nullptr);
        cLinuxMain_InitTablet();
#ifdef COMMS_3DCONNEXION
        g_3Dconnexion.Init();
#endif // COMMS_3DCONNEXION
        
        gtk_drag_dest_set(cLinuxMain_DrawingArea, GTK_DEST_DEFAULT_ALL, nullptr, 0, GDK_ACTION_COPY);
        gtk_drag_dest_add_text_targets(cLinuxMain_DrawingArea);
        gtk_drag_dest_add_uri_targets(cLinuxMain_DrawingArea);

        g_signal_connect(cLinuxMain_DrawingArea, "drag-drop", G_CALLBACK(cLinuxMain_DragDrop), nullptr);
        g_signal_connect(cLinuxMain_DrawingArea, "drag-data-received", G_CALLBACK(cLinuxMain_DragDataReceived), nullptr);

        gtk_main();
#ifdef COMMS_3DCONNEXION
        g_3Dconnexion.Free();
#endif // COMMS_3DCONNEXION
        comms::cMain_OnFree();
        comms::cRender::Free();
    }
#ifdef COMMS_OPENGL
    glXMakeCurrent(cLinuxMain_Display, 0, nullptr);
    glXDestroyContext(cLinuxMain_Display, cLinuxMain_OglContext);
    cLinuxMain_OglContext = nullptr;
#endif // COMMS_OPENGL
    
    comms::cMain_OnPostFree();
    return 0;
}

class BaseWidget;
typedef bool fnCycleEnd(BaseWidget* W);
void ProcessRenderCycle(fnCycleEnd *WhenEnd,BaseWidget* W) {
    while(true) {
        if(nullptr == WhenEnd || WhenEnd(W)) {
            return;
        }
        cLinuxMain_WhileEvents();
        cLinuxMain_OnRender(nullptr);
    }
}

#else // COMMS_CONSOLE

// cInput::EnableEvents
bool comms::cInput::EnableEvents() {
    return true;
}

// cInput::AcquireKeyboard
bool comms::cInput::AcquireKeyboard(comms::cInput::KeyboardState *S) {
	return false;
}

// cMessageBox::YesNo
bool comms::cMessageBox::YesNo(const char *Caption, const char *Text, ...) {
    return false;
}

// cMessageBox::Ok
void comms::cMessageBox::Ok(const char *Caption, const char *Text, ...) {
}

// cInput::SetCursor
void comms::cInput::SetCursor(const cInput::Cursor::Enum Cursor) {
}

// cMain_SetWindowTitle
void comms::cMain_SetWindowTitle(const char *Title) {
}

int main(int argc, char** argv) {
	comms::cMutex::GetInstance();
    cLinuxMain_ParseCmdLineArgs(argc, argv);
    comms::cMain_OnPreInit();
    return 0;
}

#endif // COMMS_CONSOLE
#endif // COMMS_LINUX
