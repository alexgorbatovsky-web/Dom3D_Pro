#include "comms.h"

#ifdef COMMS_MACOS

#import <Cocoa/Cocoa.h>
#import <OpenGL/glu.h>

#define SET_DELEGATE_CAST id // Mac OS 10.5
//#define SET_DELEGATE_CAST id<NSWindowDelegate> // Mac OS 10.6

#ifdef COMMS_3DCOAT
extern bool IsInExitState;
extern bool IgnoreSystemPause;
#else // !COMMS_3DCOAT
bool IsInExitState = false;
bool IgnoreSystemPause = false;
#endif // COMMS_3DCOAT

// cMacMain_Tablet
class cMacMain_Tablet {
public:
    bool PenPressed, Eraser, Proximity;
    float CurPressure;
    comms::cVec2 CurPos, CurTilt;
    cMacMain_Tablet() {
        memset(this, 0, sizeof(*this));
    }
};
static cMacMain_Tablet g_Tablet;

// cMain_GetTabletState
void comms::cMain_GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastUsedTime) {
    if(CurPressure != nullptr) {
        *CurPressure = 650.0f * g_Tablet.CurPressure;
    }
    if(PenPressed != nullptr) {
        *PenPressed = g_Tablet.PenPressed;
    }
    if(EraserUsed != nullptr) {
        *EraserUsed = g_Tablet.PenPressed && g_Tablet.Eraser;
    }
    if(LastUsedTime != nullptr) {
		*LastUsedTime = -1;
	}
}

// BorderlessWindow
@interface BorderlessWindow : NSWindow {
}
@end
@implementation BorderlessWindow
-(BOOL)canBecomeKeyWindow {
	// Cocoa window created with "NSBorderlessWindowMask" will never become key window by default
	return YES;
}
@end

static comms::cStr cMacMain_CmdLineArgs;
static NSWindow *cMacMain_WindowedWindow = nullptr;
static NSRect cMacMain_WindowedWindowContentRectBeforeFullScreen, cMacMain_WindowedWindowFrameBeforeFullScreen;
static BorderlessWindow *cMacMain_FullScreenWindow = nil;
static comms::cStr cMacMain_CurTitle;
static NSOpenGLContext *cMacMain_OglContext = nullptr;
static bool cMacMain_FullScreen = false;
static comms::cList<int> cMacMain_Codes;
static comms::cInput::KeyboardState cMacMain_KeyboardState;
static bool cMacMain_PostQuitMessage = false;
static int cMacMain_WindowWidth = 0, cMacMain_WindowHeight = 0;
static NSView *cMacMain_View = nil;

// cMain_GetClientWidth
int comms::cMain_GetClientWidth() {
    return cMacMain_WindowWidth;
}

// cMain_GetClientHeight
int comms::cMain_GetClientHeight() {
    return cMacMain_WindowHeight;
}

// cMacMain_GetWindow
NSWindow * cMacMain_GetWindow() {
	return cMacMain_FullScreen ? cMacMain_FullScreenWindow : cMacMain_WindowedWindow;
}

// cMacMain_SetWindowView
void cMacMain_SetWindowView(NSWindow *Window, NSView *View) {
    cMacMain_WindowedWindow = Window;
    cMacMain_View = View;
}

// cMacMain_ActivateApp
static void cMacMain_ActivateApp() {
    [NSApp activateIgnoringOtherApps:YES];
}

// cMacMain_ActivateMainWindow
static void cMacMain_ActivateMainWindow() {
    [cMacMain_GetWindow() makeKeyAndOrderFront:nil];
}

// MainView
@interface MainView : NSView {
}
-(const comms::cVec2)GetMousePositionFrom:(NSEvent *)Event;
-(const comms::cVec2)GetMouseDeltaFrom:(NSEvent *)Event;
-(void)AddMouseButton:(NSEvent *)Event :(int)Code :(bool)Pressed;
-(void)AddMouseMove:(NSEvent *)Event;
-(void)AddMouseWheel:(NSEvent *)Event;
-(void)OnMenuQuit:(id)sender;
-(void)HandleTabletEvent:(NSEvent *)Event;
-(void)HandleProximityEvent:(NSEvent *)Event;
-(void)HandlePointEvent:(NSEvent *)Event;
@end

// cMacMain_IsActive
static bool cMacMain_IsActive() {
	bool App = (YES == [[NSApplication sharedApplication] isActive]);
	bool Wnd = (NO == [cMacMain_GetWindow() isMiniaturized]);
	return App && Wnd;
}

// cMacMain_ResumeEvents
static void cMacMain_ResumeEvents() {
	// We should fire these two events after file dialogs
	// Otherwise the application doesn't receive input events
	// And "location" should not be zero
	NSPoint p = [NSEvent mouseLocation];
	NSPoint l = [[cMacMain_View window] convertScreenToBase:p];
    // In Mac OS X 10.8 Mountain Lion location should be equal to the current
    // mouse location. Otherwise send event method hangs forever.
	NSEvent *Down = [NSEvent mouseEventWithType:NSLeftMouseDown location:l modifierFlags:NSDeviceIndependentModifierFlagsMask timestamp:0 windowNumber:[[cMacMain_View window] windowNumber] context:nil eventNumber:0 clickCount:1 pressure:0.0];
    [NSApp sendEvent:Down];
	NSEvent *Up = [NSEvent mouseEventWithType:NSLeftMouseUp location:l modifierFlags:NSDeviceIndependentModifierFlagsMask timestamp:0 windowNumber:[[cMacMain_View window] windowNumber] context:nil eventNumber:0 clickCount:1 pressure:0.0];
	[NSApp sendEvent:Up];
}

// cMacMain_SendMouseMoveEvent
static void cMacMain_SendMouseMoveEvent() {
	// To update mouse position after dialogs we should fire mouse move event with new coords
	NSPoint p = [NSEvent mouseLocation];
	NSPoint l = [[cMacMain_View window] convertScreenToBase:p];
	NSEvent *Move = [NSEvent mouseEventWithType:NSMouseMoved location:l modifierFlags:NSDeviceIndependentModifierFlagsMask timestamp:0 windowNumber:[[cMacMain_View window] windowNumber] context:nil eventNumber:0 clickCount:0 pressure:0.0];
	[NSApp sendEvent:Move];
}

// cMacMain_RestoreContext
static void cMacMain_RestoreContext() {
    // This fixes 3D-Coat flickering bug. Under Mac OS X 10.7 Lion file dialogs use OpenGL for view modes "icons" and "CoverFlow".
    // In those modes the file dialogs replace the current OpenGL context with their own.
    // Because OpenGL context is defined per thread and the file dialogs are executed in the same thread as 3D-Coat,
    // the screen flickers and becomes currupted after closing any file dialog.
    if(cMacMain_OglContext != nil) {
        if([NSOpenGLContext currentContext] != cMacMain_OglContext) {
            [cMacMain_OglContext makeCurrentContext];
        }
    }
}

// cMacMain_GetRootDiskUUID
void cMacMain_GetRootDiskUUID(comms::cStr *UUID) {
    @autoreleasepool {
        // $ diskutil info -plist /
        NSArray *Args = [NSArray arrayWithObjects:@"info", @"-plist", @"/", nil];
        NSTask *Task = [NSTask new];
        [Task setLaunchPath:@"/usr/sbin/diskutil"];
        [Task setArguments:Args];
        NSPipe *Pipe = [NSPipe new];
        [Task setStandardOutput:Pipe];
        [Task launch];
        NSFileHandle *FileHandle = [Pipe fileHandleForReading];
        NSData *Data = [FileHandle readDataToEndOfFile];
        NSString *ErrorDesc = nil;
        NSPropertyListFormat Format;
        NSMutableDictionary *Dict = (NSMutableDictionary *)[NSPropertyListSerialization propertyListFromData:Data mutabilityOption:NSPropertyListMutableContainersAndLeaves format:&Format errorDescription:&ErrorDesc];
        NSString *Str = [Dict objectForKey:@"VolumeUUID"];
        UUID->Copy([Str UTF8String]);
    }
}

// cMacMain_RestartThisApplication
void cMacMain_RestartThisApplication() {
    @autoreleasepool {
        NSString *DaemonPath = [[NSBundle mainBundle] pathForResource:@"relaunch" ofType:nil];
        NSString *BundlePath = [[NSBundle mainBundle] bundlePath];
        NSString *PID = [NSString stringWithFormat:@"%d", [[NSProcessInfo processInfo] processIdentifier]];
        NSArray *Args = @[BundlePath, PID];
        [NSTask launchedTaskWithLaunchPath:DaemonPath arguments:Args];
        [NSApp terminate:nil];
    }
}

//*****************************************************************************
// Font
//*****************************************************************************

static comms::cStr cMacMain_FontPostScriptName;
static comms::cStr cMacMain_FontDisplayName;
static int cMacMain_FontSize = -1;
static bool cMacMain_FontChanged = false;
static void cMacMain_UpdateFontNamesAndSize(NSFont *F) {
    @autoreleasepool {
        cMacMain_FontPostScriptName = [[F fontName] UTF8String];
        cMacMain_FontDisplayName = [[F displayName] UTF8String];
        cMacMain_FontSize = static_cast<int>([F pointSize]);
    }
}

static void cMacMain_FontInit() {
    if(cMacMain_FontSize != -1) {
        return;
    }
    @autoreleasepool {
        NSFontPanel *FontPanel = [NSFontPanel sharedFontPanel];
        NSFont *F = [NSFont systemFontOfSize:72];
        [FontPanel setPanelFont:F isMultiple:NO];
        cMacMain_UpdateFontNamesAndSize(F);
    }
}

bool comms::cMain_GetFontChanged() {
    const bool r = cMacMain_FontChanged;
    cMacMain_FontChanged = false;
    return r;
}

const comms::cStr comms::cMain_GetFontName() {
	cMacMain_FontInit();
    return cMacMain_FontDisplayName;
}

void comms::cMain_ChooseFont() {
	cMacMain_FontInit();
    @autoreleasepool {
        NSFontPanel *FontPanel = [NSFontPanel sharedFontPanel];
        if([FontPanel isVisible]) {
            [FontPanel orderOut:nil];
        } else {
            [FontPanel orderFront:cMacMain_GetWindow()];
        }
    }
}

void comms::cMain_SetTextSize(const int Size) {
    cMacMain_FontInit();
    if(Size == cMacMain_FontSize) {
        return;
    }
    cMacMain_FontSize = Size;
}

int comms::cMain_GetTextSize() {
	cMacMain_FontInit();
    return cMacMain_FontSize;
}

void comms::cMain_DrawText(const char *Text, comms::cImage *To, int *TextWidth, int *TextHeight) {
	cMacMain_FontInit();
    @autoreleasepool {
        // Create "Core Text" font object
        CTFontRef Font = CTFontCreateWithName((CFStringRef)[NSString stringWithUTF8String:cMacMain_FontPostScriptName.ToCharPtr()], cMacMain_FontSize, nullptr);
        // Metrics of the font
        CGFloat FontAscent = CTFontGetAscent(Font);
        CGFloat FontDescent = CTFontGetDescent(Font);
        const int Offset = 1; // Make offset around the text (above, below, left, and right)
        int X = Offset, Y = (int)FontAscent + Offset;
        // Text height directly from the metrics
        *TextHeight = Offset + (int)(FontAscent + FontDescent) + Offset;
        // Create "Core Text" line object
        CFStringRef String = CFStringCreateWithCString(nullptr, Text, kCFStringEncodingUTF8);
        CFStringRef Keys[] = {
            kCTFontAttributeName, kCTForegroundColorAttributeName
        };
        CGColorRef Color = CGColorCreateGenericGray(1.0f, 1.0f);
        CFTypeRef Values[] = {
            Font, Color
        };
        CFDictionaryRef Attributes = CFDictionaryCreate(kCFAllocatorDefault, (const void **)&Keys, (const void **)&Values, sizeof(Keys) / sizeof(Keys[0]),
                                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef AttrString = CFAttributedStringCreate(kCFAllocatorDefault, (CFStringRef)String, Attributes);
        CFRelease(String);
        String = nullptr;
        CFRelease(Attributes);
        Attributes = nullptr;
        CTLineRef Line = CTLineCreateWithAttributedString(AttrString);
        CFRelease(AttrString);
        AttrString = nullptr;
        // Measure text width
        int Width = 32, Height = 32;
        size_t BytesPerRow = 4 * Width;
        size_t TotalSize = BytesPerRow * Height;
        To->Create(comms::cFormat::Rgba8, Width, Height, 1, 1);
        memset(To->GetPixels(), 0, TotalSize);
        // Create context for the first time
        CGColorSpaceRef ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
        CGContextRef Context = CGBitmapContextCreate(To->GetPixels(), Width, Height, 8, BytesPerRow, ColorSpace, kCGImageAlphaPremultipliedLast);
        CGContextSetTextPosition(Context, X, Height - Y);
        CTLineDraw(Line, Context);
        *TextWidth = (int)CGContextGetTextPosition(Context).x + Offset;
        CGContextRelease(Context);
        Context = nullptr;
        // Draw text
        Width = comms::cMath::Max(32, comms::cMath::UpperPowerOfTwo(*TextWidth));
        Height = comms::cMath::Max(32, comms::cMath::UpperPowerOfTwo(*TextHeight));
        if(Width > 4096) {
            Width = 4096;
        }
        if(Height > 4096) {
            Height = 4096;
        }
        BytesPerRow = 4 * Width;
        TotalSize = BytesPerRow * Height;
        To->Create(comms::cFormat::Rgba8, Width, Height, 1, 1);
        memset(To->GetPixels(), 0, TotalSize);
        // Create context for the second time
        Context = CGBitmapContextCreate(To->GetPixels(), Width, Height, 8, BytesPerRow, ColorSpace, kCGImageAlphaPremultipliedLast);
        CGContextSetTextPosition(Context, X, Height - Y);
        CTLineDraw(Line, Context);
        CGContextRelease(Context);
        Context = nullptr;
        // Free
        CGColorRelease(Color);
        Color = nullptr;
        CGColorSpaceRelease(ColorSpace);
        ColorSpace = nullptr;
        CFRelease(Line);
        Line = nullptr;
        CFRelease(Font);
        Font = nullptr;
    }
    To->Flip();
}

// cMacMain_AllocWindowedWindow
static void cMacMain_AllocWindowedWindow(NSRect ContentRect) {
	if(cMacMain_WindowedWindow != nil) {
		return;
	}
	NSUInteger Style = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;
	cMacMain_WindowedWindow = [[NSWindow alloc] initWithContentRect:ContentRect styleMask:Style backing:NSBackingStoreBuffered defer:NO];
	[cMacMain_WindowedWindow setAcceptsMouseMovedEvents:YES];
    [cMacMain_WindowedWindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    // The window that has been created after full screen mode for some reason
    // has disabled minimize button, so we should enable it manually
    NSButton *MiniaturizeButton = [cMacMain_WindowedWindow standardWindowButton:NSWindowMiniaturizeButton];
    [MiniaturizeButton setEnabled:YES];
}

// cMacMain_SetFullScreen
static void cMacMain_SetFullScreen() {
	if(cMacMain_FullScreen) {
		return;
	}
	cMacMain_FullScreen = true;
	NSRect rc = [[cMacMain_WindowedWindow screen] frame];
	bool WindowIsOnPrimaryDisplay = [[cMacMain_WindowedWindow screen] isEqual:[[NSScreen screens] objectAtIndex:0]];
	if(WindowIsOnPrimaryDisplay) {
		[NSMenu setMenuBarVisible:NO];
	}
	[cMacMain_WindowedWindow setContentView:nil];
	[cMacMain_WindowedWindow setDelegate:nil];
	[cMacMain_WindowedWindow makeFirstResponder:nil];
	// We should close the windowed window because when we hide it using "orderOut:nil", the windowed window
	// becomes visible after switching with "Cmd + Tab" combination back and forth couple of times
	cMacMain_WindowedWindowFrameBeforeFullScreen = [cMacMain_WindowedWindow frame];
	cMacMain_WindowedWindowContentRectBeforeFullScreen = [cMacMain_WindowedWindow contentRectForFrameRect:cMacMain_WindowedWindowFrameBeforeFullScreen];
	[cMacMain_WindowedWindow close];
	cMacMain_WindowedWindow = nil;
	// Create fullscreen window
    NSUInteger Style = NSBorderlessWindowMask;
	cMacMain_FullScreenWindow = [[BorderlessWindow alloc] initWithContentRect:rc styleMask:Style backing:NSBackingStoreBuffered defer:NO];
	[cMacMain_FullScreenWindow setAcceptsMouseMovedEvents:YES];
	[cMacMain_FullScreenWindow setTitle:[NSString stringWithUTF8String:cMacMain_CurTitle.ToCharPtr()]];
	[cMacMain_FullScreenWindow setContentView:cMacMain_View];
	[cMacMain_FullScreenWindow setDelegate:(SET_DELEGATE_CAST)cMacMain_View];
    [cMacMain_FullScreenWindow makeFirstResponder:cMacMain_View];
	if(WindowIsOnPrimaryDisplay) {
		[cMacMain_FullScreenWindow setHidesOnDeactivate:YES];
		[cMacMain_FullScreenWindow setLevel:NSFloatingWindowLevel];
	}
	[cMacMain_FullScreenWindow makeKeyAndOrderFront:nil];
}

static NSMenuItem *cMacMain_MinimizeMenuItem = nil;

// cMacMain_SetWindowed
static void cMacMain_SetWindowed() {
	if(!cMacMain_FullScreen) {
		return;
	}
	cMacMain_FullScreen = false;
	bool WindowIsOnPrimaryDisplay = [[cMacMain_FullScreenWindow screen] isEqual:[[NSScreen screens] objectAtIndex:0]];
	[cMacMain_FullScreenWindow setContentView:nil];
	[cMacMain_FullScreenWindow setDelegate:nil];
	[cMacMain_FullScreenWindow makeFirstResponder:nil];
	[cMacMain_FullScreenWindow close];
	cMacMain_FullScreenWindow = nil;
	cMacMain_AllocWindowedWindow(cMacMain_WindowedWindowContentRectBeforeFullScreen);
	[cMacMain_WindowedWindow setTitle:[NSString stringWithUTF8String:cMacMain_CurTitle.ToCharPtr()]];
	[cMacMain_WindowedWindow setContentView:cMacMain_View];
	[cMacMain_WindowedWindow setDelegate:(SET_DELEGATE_CAST)cMacMain_View];
    [cMacMain_WindowedWindow makeFirstResponder:cMacMain_View];
	[cMacMain_WindowedWindow setFrame:cMacMain_WindowedWindowFrameBeforeFullScreen display:NO animate:NO];
	[cMacMain_WindowedWindow makeKeyAndOrderFront:nil];
	if(WindowIsOnPrimaryDisplay) {
		[NSMenu setMenuBarVisible:YES];
	}
}

//*****************************************************************************
// Message Box
//*****************************************************************************

@interface AlertSync: NSObject {
    int Button;
}
-(int)Run:(NSAlert *)Alert ParentWindow:(NSWindow *)Window;
@end

@implementation AlertSync
-(int)Run:(NSAlert *)Alert ParentWindow:(NSWindow *)Window {
    Button = -1;
    [Alert beginSheetModalForWindow:Window modalDelegate:self didEndSelector:@selector(alertDidEnd:returnCode:) contextInfo:nil];
    NSModalSession Session = [NSApp beginModalSessionForWindow:[Alert window]];
    for(;;) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]; // w/o this line CPU load will be 100%
        if([NSApp runModalSession:Session] != NSRunContinuesResponse) {
            break;
        }
    }
    [NSApp endModalSession:Session];
    [NSApp endSheet:[Alert window]];
    return Button;
}

-(void)alertDidEnd:(NSAlert *)Alert returnCode:(NSInteger)ReturnCode {
    Button = (int)(ReturnCode - NSAlertFirstButtonReturn);
    [NSApp stopModal];
}
@end

static void cMacMain_SendMouseUpEvents() {
    if(comms::cInput::IsDown(comms::cInput::Codes::LeftButton)) {
        comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
        E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
        E->Code = comms::cInput::Codes::LeftButton;
        comms::cInput::AddEvent(E);
        E = nullptr;
    }
}

// cMessageBox::YesNo
bool comms::cMessageBox::YesNo(const char *Caption, const char *Text, ...) {
    va_list args;
    va_start(args, Text);
    char temp[1024];
    vsnprintf(temp, 1024, Text, args);
    va_end(args);
	comms::cPause::SetSystemPause(true);
    bool Result = false;
    @autoreleasepool {
        NSAlert *Alert = [[NSAlert alloc] init];
        [Alert addButtonWithTitle:@"No"];
        [Alert addButtonWithTitle:@"Yes"];
        [Alert setMessageText:[NSString stringWithUTF8String:Caption]];
        [Alert setInformativeText:[NSString stringWithUTF8String:temp]];
        [Alert setAlertStyle:NSWarningAlertStyle];
        NSWindow *Window = cMacMain_GetWindow();
        if(Window != nil) {
            AlertSync *Sync = [[AlertSync alloc] init];
            int Button = [Sync Run:Alert ParentWindow:Window];
            [Sync release];
            Sync = nil;
            Result = (1 == Button);
        } else {
            Result = ([Alert runModal] == NSAlertSecondButtonReturn);
        }
        [Alert release];
        cMacMain_ResumeEvents();
        cMacMain_SendMouseMoveEvent();
    }
	return Result;
}

// cMessageBox::Ok
void comms::cMessageBox::Ok(const char *Caption, const char *Text, ...) {
    va_list args;
    va_start(args, Text);
    char temp[1024];
    vsnprintf(temp, 1024, Text, args);
    va_end(args);
	comms::cPause::SetSystemPause(true);
    @autoreleasepool {
        NSAlert *Alert = [[NSAlert alloc] init];
        [Alert addButtonWithTitle:@"OK"];
        [Alert setMessageText:[NSString stringWithUTF8String:Caption]];
        [Alert setInformativeText:[NSString stringWithUTF8String:temp]];
        [Alert setAlertStyle:NSWarningAlertStyle];
        NSWindow *Window = cMacMain_GetWindow();
        if(Window != nil) {
            AlertSync *Sync = [[AlertSync alloc] init];
            [Sync Run:Alert ParentWindow:Window];
            [Sync release];
            Sync = nil;
        } else {
            [Alert runModal];
        }
        [Alert release];
        cMacMain_ResumeEvents();
        cMacMain_SendMouseMoveEvent();
    }
}

//*****************************************************************************
// Input
//*****************************************************************************

static comms::cInput::Cursor::Enum cMacMain_CurCursor = comms::cInput::Cursor::Arrow;
static void cMacMain_SetCursor(const comms::cInput::Cursor::Enum Cursor) {
    @autoreleasepool {
        comms::cInput::Cursor::Enum T = Cursor;
        if(comms::cInput::Cursor::Default == T) {
            T = comms::cInput::Cursor::Arrow;
        }
        if(cMacMain_CurCursor != T) {
            if(comms::cInput::Cursor::None == cMacMain_CurCursor) {
                [NSCursor unhide];
            }
            if(comms::cInput::Cursor::None == Cursor) {
                [NSCursor hide];
            }
            cMacMain_CurCursor = T;
            [cMacMain_GetWindow() invalidateCursorRectsForView:cMacMain_View];
        }
    }
}

static bool cMacMain_FontPanelIsVisible() {
    @autoreleasepool {
        NSFontPanel *FontPanel = [NSFontPanel sharedFontPanel];
        return [FontPanel isVisible];
    }
}

void comms::cInput::SetCursor(const cInput::Cursor::Enum Cursor) {
    cInput::Cursor::Enum C = Cursor;
    if(cInput::Cursor::None == Cursor) {
        const bool F = cMacMain_FontPanelIsVisible();
        if(F) {
            C = cInput::Cursor::Arrow;
        }
    }
    cMacMain_SetCursor(C);
}

static float macOS_Global_Scale = 1.0f;

// cInput::WarpCursor
void comms::cInput::WarpCursor(const int LocalX, const int LocalY, const bool DownY) {
    @autoreleasepool {
        NSPoint l = NSMakePoint(LocalX / macOS_Global_Scale, LocalY / macOS_Global_Scale);
        if(DownY) {
            CGFloat h = [cMacMain_View frame].size.height;
            l.y = h - l.y;
        }
        NSPoint g = [cMacMain_GetWindow() convertBaseToScreen:l];
        CGFloat H = [[cMacMain_GetWindow() screen] frame].size.height;
        g.y = H - g.y;
        CGWarpMouseCursorPosition(*((CGPoint *)&g));
        CGAssociateMouseAndMouseCursorPosition(true);
    }
}

// cInput::EnableEvents
bool comms::cInput::EnableEvents() {
	return true;
}

//-----------------------------------------------------------------------------
// cInput::IsDownAcquire
//-----------------------------------------------------------------------------
bool comms::cInput::IsDownAcquire(const int Code) {
	bool r = false;
	int k;
	if(comms::cInput::IsKeyboardCode(Code)) {
		k = cMacMain_Codes[Code];
		r = CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, k);
        if(!r && cInput::Control == Code) {
            CGEventFlags f = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
            if(f & kCGEventFlagMaskControl) {
                r = true;
            }
        }
	}
    if(comms::cInput::IsMouseButtonCode(Code)) {
        const CGMouseButton Button = cInput::LeftButton == Code ? kCGMouseButtonLeft : (cInput::RightButton == Code ? kCGMouseButtonRight : kCGMouseButtonCenter);
        r = CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, Button);
    }
	return r;
} // cInput::IsDownAcquire

//-----------------------------------------------------------------------------
// cInput::AcquireKeyboard
//-----------------------------------------------------------------------------
bool comms::cInput::AcquireKeyboard(comms::cInput::KeyboardState *S) {
	*S = cMacMain_KeyboardState;
	return true;
} // cInput::AcquireKeyboard

// cInput::SetCapture
void comms::cInput::SetCapture() {
}

// InputManager::ReleaseCapture
void comms::cInput::ReleaseCapture() {
}

static void cMacMain_SendUpEvents();

//-----------------------------------------------------------------------------
// cMacMain_OnRender
//-----------------------------------------------------------------------------
static void cMacMain_OnRender() {
    comms::cSplash::DestroyLater();
	if(nullptr == comms::cRender::GetInstance()) {
		return;
	}
    if(nullptr == comms::cRender::GetViewer()) {
        return;
    }
	
    if(comms::cSettings::GetInstance()->FullScreen && !cMacMain_FullScreen) {
		cMacMain_SetFullScreen();
	} else if(!comms::cSettings::GetInstance()->FullScreen && cMacMain_FullScreen) {
		cMacMain_SetWindowed();
	}
	
	bool Active = cMacMain_IsActive();
    if(!Active && !comms::cPause::GetSystemPause()) {
		comms::cPause::SetSystemPause(true);
    } else if(Active && comms::cPause::GetSystemPause()) {
		cMacMain_SendUpEvents();
		comms::cPause::SetSystemPause(false);
	}
	if(comms::cPause::GetSystemPause() && !IgnoreSystemPause) {
		comms::SleepMs(100);
		return;
	}
	
	NSSize Size = [[cMacMain_GetWindow() contentView] frame].size;
    macOS_Global_Scale = [cMacMain_GetWindow() backingScaleFactor];
    const int ScaledHeight = (int)(Size.height * macOS_Global_Scale);
    const bool UpscaleInterfaceOnHiDPI = ScaledHeight > 2000;
    if(UpscaleInterfaceOnHiDPI) {
        comms::cRender::PixelsPerPoint = (int)macOS_Global_Scale;
        macOS_Global_Scale = 1.0f;
    } else {
        comms::cRender::PixelsPerPoint = 1;
    }
	cMacMain_WindowWidth = (int)(Size.width * macOS_Global_Scale);
	cMacMain_WindowHeight = (int)(Size.height * macOS_Global_Scale);
    if(cMacMain_WindowWidth < 1 || cMacMain_WindowHeight < 1) {
		return;
	}

	comms::cRect Viewport;
	Viewport.SetBottomLeft(0.0f, 0.0f);
	Viewport.SetTopRight((float)cMacMain_WindowWidth, (float)cMacMain_WindowHeight);

	[cMacMain_OglContext update];
    comms::cTimer T; T.Begin();
	comms::cMain_OnRender(Viewport);
    T.End();
    int RenderTimeMs = 15 - (int)T.MeanTimeMs;
    if(comms::cSettings::GetInstance()->VSync && (RenderTimeMs > 0)) {
        comms::SleepMs(RenderTimeMs);
    }
    [cMacMain_OglContext flushBuffer];
} // cMacMain_OnRender

namespace comms {

//*****************************************************************************
// Window & Application
//*****************************************************************************

void cMain_SetWindowTitle(const char *Title) {
    @autoreleasepool {
        if(cMacMain_GetWindow() != nullptr) {
            if(!cStr::Equals(Title, cMacMain_CurTitle)) {
                cMacMain_CurTitle = Title;
                [cMacMain_GetWindow() setTitle:[NSString stringWithUTF8String:cMacMain_CurTitle.ToCharPtr()]];
            }
        }
    }
}

// cMain_Quit
void cMain_Quit() {
    cMacMain_PostQuitMessage = true;
}

// cMain_GetCmdLineArgs
const cStr cMain_GetCmdLineArgs() {
	return cMacMain_CmdLineArgs;
}

} // comms

// SaveView
@interface SaveView : NSView {
@public
    NSPopUpButton *PopUp;
}
-(void)CreateControls;
-(void)OnChangeSelection:(id)Sender;
@end

@implementation SaveView
-(void)CreateControls {
	// PopUp
    PopUp = [[NSPopUpButton alloc] initWithFrame:[self frame]];
	[PopUp setTarget:self];
	[PopUp setAction:@selector(OnChangeSelection:)];
	[self addSubview:PopUp];
	[PopUp release];
}

-(void)OnChangeSelection:(id)Sender {
}

@end // SaveView

namespace comms {

//*****************************************************************************
// File Dialogs
//*****************************************************************************

// cMacMain_SelectFolderDialog
bool cMacMain_SelectFolderDialog(const char *Title, cStr *SelectedFolder, const char *InitialFolder) {
    comms::cPause::SetSystemPause(true);
    cStr R;
    @autoreleasepool {
        // Get instance of open panel
        NSOpenPanel *Dlg = [NSOpenPanel openPanel];
        [Dlg setCanChooseFiles:NO];
        [Dlg setCanChooseDirectories:YES];
        // Title
        [Dlg setTitle:[NSString stringWithUTF8String:Title]];
        // Initial Folder
        if(InitialFolder != nullptr) {
            cStr I = cIO::EnsureAbsolutePath(InitialFolder);
            NSString *Str = [NSString stringWithUTF8String:I.ToCharPtr()];
            NSURL *Url = [NSURL fileURLWithPath:Str];
            [Dlg setDirectoryURL:Url];
        }
        // Run
        if(NSOKButton == [Dlg runModal]) {
            R = [[[Dlg URL] path] UTF8String];
        }
        if(!R.IsEmpty()) {
            *SelectedFolder = R;
        }
        cMacMain_ResumeEvents();
        cMacMain_SendMouseMoveEvent();
    }
    cMacMain_RestoreContext(); // We should restore OpenGL context outside "@autoreleasepool"
	return !R.IsEmpty();
}

// cMacMain_LoadFileDialog
bool cMacMain_LoadFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *SingleFilePn, cList<cStr> *MultiFilePn, const char *PrefKey, const char *InitialFileName) {
	cPause::SetSystemPause(true);
    bool r = false;
    @autoreleasepool {
        // Get instance of open panel
        NSOpenPanel *Dlg = [NSOpenPanel openPanel];
        [Dlg setCanChooseFiles:YES];
        [Dlg setCanChooseDirectories:NO];
        if(SingleFilePn != nullptr) {
            SingleFilePn->Clear();
        }
        if(MultiFilePn != nullptr) {
            MultiFilePn->Clear();
            [Dlg setAllowsMultipleSelection:YES];
        }
        // Title
        [Dlg setTitle:[NSString stringWithUTF8String:Title]];
        // Filter
        NSMutableArray *Types = nil;
        int i;
        if(!Extensions.IsEmpty()) {
            Types = [NSMutableArray array];
            for(i = 0; i < Extensions.Count(); i++) {
                const cStr &S = Extensions[i];
                [Types addObject:[NSString stringWithUTF8String:S.ToCharPtr()]];
            }
        }
        // Initial Folder
        NSString *Folder = nil;
        cStr F = cIO::GetFileDialogInitialFolder(PrefKey);
        if(!F.IsEmpty()) {
            Folder = [NSString stringWithUTF8String:F.ToCharPtr()];
        }
        // Initial File Name
        NSString *File = nil;
        cStr N = InitialFileName;
        if(!N.IsEmpty()) {
            File = [NSString stringWithUTF8String:N.ToCharPtr()];
        }
        // Run
        r = (NSOKButton == [Dlg runModalForDirectory:Folder file:File types:Types]);
        if(r) {
            NSArray *Files = [Dlg URLs];
            if(SingleFilePn != nullptr) {
                *SingleFilePn = [[[Files objectAtIndex:0] path] UTF8String];
                cIO::SetFileDialogInitialFolder(PrefKey, SingleFilePn->GetFilePath());
            }
            if(MultiFilePn != nullptr) {
                for(i = 0; i < [Files count]; i++) {
                    MultiFilePn->Add(cStr([[[Files objectAtIndex:i] path] UTF8String]));
                }
                cIO::SetFileDialogInitialFolder(PrefKey, MultiFilePn->GetAt(0).GetFilePath());
            }
        }
        cMacMain_SendMouseUpEvents();
        cMacMain_ResumeEvents();
        cMacMain_SendMouseMoveEvent();
    }
    cMacMain_RestoreContext(); // We should restore OpenGL context outside "@autoreleasepool"
    return r;
}

// cMacMain_SaveFileDialog
bool cMacMain_SaveFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *FilePn, const char *PrefKey, const char *DefaultExtension, const char *InitialFileBase) {
	comms::cPause::SetSystemPause(true);
    cStr R;
    @autoreleasepool {
        // Get instance of open panel
        NSSavePanel *Dlg = [NSSavePanel savePanel];
        // Title
        [Dlg setTitle:[NSString stringWithUTF8String:Title]];
        // Accessory view
        const int DlgWidth = 160, DlgHeight = 26;
        NSRect rc = NSMakeRect(0, 0, DlgWidth, DlgHeight);
        SaveView *View = [[[SaveView alloc] initWithFrame:rc] autorelease];
        [View CreateControls];
        // Filter
        int i;
        cList<cStr> E = Extensions;
        cStr D = DefaultExtension;
        if(!D.IsEmpty()) {
            E.AddUnique(D, cStr::EqualsNoCase);
        }
        if(!E.IsEmpty()) {
            // Default extension goes first
            if(!D.IsEmpty()) {
                i = E.IndexOf(D, cStr::EqualsNoCase);
                if(i != -1 && i != 0) {
                    cMath::Swap(E[0], E[i]);
                }
            }
            for(i = 0; i < E.Count(); i++) {
                const cStr &S = E[i];
                NSString *T = [NSString stringWithUTF8String:S.ToCharPtr()];
                [View->PopUp addItemWithTitle:T];
            }
        }
        // Initial Folder
        NSString *Folder = nil;
        cStr F = cIO::GetFileDialogInitialFolder(PrefKey);
        if(!F.IsEmpty()) {
            Folder = [NSString stringWithUTF8String:F.ToCharPtr()];
        }
        // Initial File Base
        NSString *File = nil;
        cStr B = InitialFileBase;
        if(!B.IsEmpty()) {
            cStr X = B.GetFileExtension();
            if(!E.IsEmpty() && (X.IsEmpty() || !E.Contains(X, comms::cStr::EqualsNoCase))) {
                B.SetFileExtension(E[0]);
            }
            File = [NSString stringWithUTF8String:B.ToCharPtr()];
        }
        [Dlg setAccessoryView:View];
        // Run
        if(NSOKButton == [Dlg runModalForDirectory:Folder file:File]) {
            R = [[[Dlg URL] path] UTF8String];
        }
        if(!R.IsEmpty()) {
            comms::cIO::SetFileDialogInitialFolder(PrefKey, R.GetFilePath());
            // Check extension
            comms::cStr X = R.GetFileExtension();
            if(!E.IsEmpty() && (X.IsEmpty() || !E.Contains(X, comms::cStr::EqualsNoCase))) {
                int n = (int)[View->PopUp indexOfSelectedItem];
                R.SetFileExtension(E[n]);
            }
            *FilePn = R;
        }
        cMacMain_SendMouseUpEvents();
        cMacMain_ResumeEvents();
        cMacMain_SendMouseMoveEvent();
    }
    cMacMain_RestoreContext(); // We should restore OpenGL context outside "@autoreleasepool"
	return !R.IsEmpty();
}

} // comms

// cMacMain_CheckProcessInMemory
bool cMacMain_CheckProcessInMemory(const char *ProcName) {
    bool r = false;
    comms::cStr N;
    @autoreleasepool {
        NSWorkspace *WS = [NSWorkspace sharedWorkspace];
        NSArray *Apps = [WS runningApplications];
        NSRunningApplication *App;
        for(App in Apps) {
            if(App.activationPolicy == NSApplicationActivationPolicyRegular) {
                N = [App.localizedName UTF8String];
                if(comms::cStr::EqualsNoCase(ProcName, N)) {
                    r = true;
                    break;
                }
            }
        }

    }
    return r;
}

static NSWindow *cMacMain_InputDialog = nil;
static comms::cStr cMacMain_InputBuffer;
static bool cMacMain_InputClose = false;
static bool cMacMain_InputEnter = false;
static void cMacMain_InputTrue() {
    cMacMain_InputClose = true;
    cMacMain_InputEnter = true;
}

@interface InputEdit : NSTextField {
}
@end

@implementation InputEdit
-(BOOL)textView:(NSTextView *)TextView doCommandBySelector:(SEL)Command {
	// Useful function for debugging "NSStringFromSelector"
	if(Command == @selector(cancelOperation:)) {
		cMacMain_InputClose = true;
	}
	return NO;
}
@end

@interface InputParentView : NSView {
}
@end

@implementation InputParentView

// The view is transparent when the method "drawRect" is not overridden

-(void)mouseDown:(NSEvent *)Event {
    cMacMain_InputTrue();
}

-(void)rightMouseDown:(NSEvent *)Event {
    cMacMain_InputTrue();
}

-(void)otherMouseDown:(NSEvent *)Event {
    cMacMain_InputTrue();
}

-(BOOL)windowShouldClose:(id)sender {
	cMacMain_InputClose = true;
	return NO;
}

@end

// InputView
@interface InputView : NSView {
	NSButton *Button;
	NSTextField *TextField;
}
-(void)CreateControls;
-(void)OnButtonClick:(id)Sender;
-(void)OnTextFieldEnter:(id)Sender;
-(void)Enter;
@end

@implementation InputView
-(void)CreateControls {
	// Button
	Button = [[NSButton alloc] initWithFrame:NSMakeRect(125, 2, 33, 23)];
	[Button setBezelStyle:NSShadowlessSquareBezelStyle];
    const int UTF32 = 0x000023CE;
    [Button setTitle:[[NSString alloc] initWithBytes:&UTF32 length:4 encoding:NSUTF32LittleEndianStringEncoding]];
	[Button setTarget:self];
	[Button setAction:@selector(OnButtonClick:)];
	[self addSubview:Button];
	[Button release];
	
	// Edit
	TextField = [[InputEdit alloc] initWithFrame:NSMakeRect(2, 2, 120, 23)];
	[TextField setStringValue:[NSString stringWithUTF8String:cMacMain_InputBuffer.ToCharPtr()]];
	[TextField setTarget:self];
	[TextField setAction:@selector(OnTextFieldEnter:)];
	[self addSubview:TextField];
	[TextField release];
}

-(void)OnButtonClick:(id)Sender {
	cMacMain_InputEnter = true;
	[NSApp stopModal];
}

-(void)OnTextFieldEnter:(id)Sender {
	cMacMain_InputEnter = true;
	[NSApp stopModal];
}

-(void)Enter {
	cMacMain_InputBuffer = [[TextField stringValue] UTF8String];
}

-(void)drawRect:(NSRect)dirtyRect {
    // Under macOS 12 Monterey "dirtyRect == [0, 0, 160, 26]"
    // Under macOS 14 Sonoma "dirtyRect == [-536.5, -373, 1240, 695]"
    // Both macOS 12 & 14 expand the content view to the whole window.
    // But only macOS 14 Sonoma expands the subview to the whole parent view.
    
    // To fix "Transparent controls under macOS 13":
    [[NSColor windowBackgroundColor] set];
    NSRectFill([Button frame]);
    NSRectFill([TextField frame]);
}

-(void)mouseDown:(NSEvent *)Event {
    cMacMain_InputTrue();
}

-(void)rightMouseDown:(NSEvent *)Event {
    cMacMain_InputTrue();
}

-(void)otherMouseDown:(NSEvent *)Event {
    cMacMain_InputTrue();
}

@end // InputView

static const int cMacMain_DlgWidth = 160, cMacMain_DlgHeight = 27;
int comms::cIO::GetInputPixelsWidth() {
    return cMacMain_DlgWidth;
}
int comms::cIO::GetInputPixelsHeight() {
    return cMacMain_DlgHeight;
}

bool cMacMain_InputString(const comms::cVec2i &Pos, comms::cStr *String) {
	comms::cPause::SetSystemPause(true);

	cMacMain_InputBuffer = *String;
	cMacMain_InputClose = false;
	cMacMain_InputEnter = false;
	
	// Create the autorelease pool
	NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
	
	// Input dialog rect
	NSRect WholeRc = [cMacMain_GetWindow() frame];
	
	NSRect ParentRc = [cMacMain_View frame];
	NSRect rc = NSMakeRect(ParentRc.origin.x + Pos[0] / macOS_Global_Scale - cMacMain_DlgWidth / 2, ParentRc.origin.y + Pos[1] / macOS_Global_Scale - cMacMain_DlgHeight / 2, cMacMain_DlgWidth, cMacMain_DlgHeight);
	// Align rect within parent content
	if(rc.origin.x < ParentRc.origin.x) {
		rc.origin.x = ParentRc.origin.x;
	}
	if(rc.origin.y < ParentRc.origin.y) {
		rc.origin.y = ParentRc.origin.y;
	}
	if(rc.origin.x + rc.size.width > ParentRc.origin.x + ParentRc.size.width) {
		rc.origin.x = ParentRc.origin.x + ParentRc.size.width - rc.size.width;
	}
	if(rc.origin.y + rc.size.height > ParentRc.origin.y + ParentRc.size.height) {
		rc.origin.y = ParentRc.origin.y + ParentRc.size.height - rc.size.height;
	}
	// Alloc dialog
	cMacMain_InputDialog = [[BorderlessWindow alloc] initWithContentRect:WholeRc styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
	[cMacMain_InputDialog setIgnoresMouseEvents:NO];
	[cMacMain_InputDialog setBackgroundColor:[NSColor clearColor]];
	[cMacMain_InputDialog setOpaque:NO];
	// Create parent view
    // Passing zero rect because after "setContentView" below the "[ParentView frame]" will equal "WholeRc"
	NSView *ParentView = [[[InputParentView alloc] initWithFrame:NSZeroRect] autorelease];
	[cMacMain_InputDialog setContentView:ParentView];
	[cMacMain_InputDialog setDelegate:(SET_DELEGATE_CAST)ParentView];
    // Create child view
	InputView *View = [[[InputView alloc] initWithFrame:rc] autorelease];
	[View CreateControls];
	[ParentView addSubview:View];
	[cMacMain_InputDialog makeFirstResponder:View];
	// Show the dialog
	[cMacMain_InputDialog makeKeyAndOrderFront:nil];
	// Start message loop
	NSModalSession Session = [NSApp beginModalSessionForWindow:cMacMain_InputDialog];
	NSInteger r;
	for(;;) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]; // w/o this line CPU load will be 100%
		r = [NSApp runModalSession:Session];
		if((r != NSModalResponseContinue) || cMacMain_InputClose) {
			break;
		}
	}
	[NSApp endModalSession:Session];
	if(cMacMain_InputEnter) {
		[View Enter];
	}
	[cMacMain_InputDialog close];
	cMacMain_InputDialog = nil;
    cMacMain_ActivateMainWindow();
	cMacMain_SendMouseMoveEvent();
	[Pool release];	// Release the pool

	if(cMacMain_InputEnter) {
		*String = cMacMain_InputBuffer;
	}
	return cMacMain_InputEnter;
}

// CenterWindow
static void CenterWindow(NSWindow *Window) {
    // Center window manually, because "center" method shifts the window too high
    NSRect WndRc = [Window frame];
    NSRect ScreenRc = [[Window screen] frame];
    NSPoint c;
    c.x = ScreenRc.origin.x + 0.5f * (ScreenRc.size.width - WndRc.size.width);
    c.y = ScreenRc.origin.y + 0.5f * (ScreenRc.size.height - WndRc.size.height);
    [Window setFrameOrigin:c];
}

static NSWindow *SplashDialog = nil;

// cMacMain_CreateApp
void cMacMain_CreateApp() {
    if(nil == NSApp) {
        // Create the application object
        NSApp = [NSApplication sharedApplication];
    }
}

// cMacMain_ShowSplash
void cMacMain_ShowSplash(const comms::cImage *Image, const int CornerRadius) {
    @autoreleasepool {
        int W = Image->GetWidth();
        int H = Image->GetHeight();
        int Row = 3 * W;
        // Image representation
        NSBitmapImageRep *ImageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr pixelsWide:W pixelsHigh:H bitsPerSample:8 samplesPerPixel:3 hasAlpha:NO isPlanar:NO colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:Row bitsPerPixel:24];
        comms::byte *Data = [ImageRep bitmapData];
        size_t S = Row * H;
        memcpy(Data, Image->GetPixels(), S);
        // Image object
        NSImage *Image = [[NSImage alloc] initWithSize:[ImageRep size]];
        [Image addRepresentation:ImageRep];
        [ImageRep release];
        ImageRep = nil;
        // Image view
        NSRect Rc = NSMakeRect(0, 0, W, H);
        NSImageView *View = [[NSImageView alloc] initWithFrame:Rc];
        [View setImage:Image];
        [Image release];
        Image = nil;
        // Splash window
        NSUInteger Style = NSBorderlessWindowMask;
        SplashDialog = [[NSWindow alloc] initWithContentRect:Rc styleMask:Style backing:NSBackingStoreBuffered defer:YES];
        View.wantsLayer = true;
        View.layer.cornerRadius = CornerRadius;
        [SplashDialog setOpaque:NO];
        [SplashDialog setBackgroundColor:NSColor.clearColor];
        [SplashDialog setContentView:View];
        [View release];
        View = nil;
        CenterWindow(SplashDialog);
        [SplashDialog setLevel:NSNormalWindowLevel];
        cMacMain_CreateApp(); // Without "NSApp" console outputs "[default] 0 is not a valid connection ID"
        [SplashDialog makeKeyAndOrderFront:nil];
        [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
    }
}

// cMacMain_CloseSplash
void cMacMain_CloseSplash() {
    @autoreleasepool {
        [SplashDialog orderOut:nil];
        [SplashDialog release];
        SplashDialog = nil;
    }
}

static NSWindow *cMacMain_LogDialog = nil;
static int cMacMain_LogResult = -1;
static comms::cLog::Mode::Enum cMacMain_LogMode = comms::cLog::Mode::Message;
static const comms::cStr *cMacMain_LogText = nil;
static bool cMacMain_CloseLog = false;

// LogView
@interface LogView : NSView {
	NSButton *Buttons[(comms::cLog::ID::Exit - comms::cLog::ID::Debug) + 1]; // Debug, Ignore, Ignore Always, Copy, Exit
	NSScrollView *ScrollView;
	NSTextView *TextView;
}
-(void)CreateControls;
-(void)EnableControls;
-(void)OnButtonClick:(id)Sender;
-(BOOL)acceptsFirstResponder;
-(void)keyDown:(NSEvent *)event;
@end

@implementation LogView
-(BOOL)acceptsFirstResponder {
    return YES;
}
-(void)keyDown:(NSEvent *)event {
    switch([event keyCode]) {
        case 0x35:
            cMacMain_CloseLog = true;
            break;
        default:
            [super keyDown:event];
    }
}

-(void)CreateControls {
	const int ButtonWidth = 100;
	const int ButtonHeight = 26;
	const int Space = 4;
    const int DlgWidth = (int)([self frame].size.width);
    const int DlgHeight = (int)([self frame].size.height);
	int X = Space, Y = Space;
	const char *Labels[(comms::cLog::ID::Exit - comms::cLog::ID::Debug) + 1] = {
		"Debug", "Ignore", "Ignore Always", "Copy", "Exit"
	};
	int i, c = sizeof(Labels) / sizeof(Labels[0]);
	for(i = 0; i < c; i++) {
		Buttons[i] = [[NSButton alloc] initWithFrame:NSMakeRect(X, Y, ButtonWidth, ButtonHeight)];
		[Buttons[i] setBezelStyle:NSTexturedRoundedBezelStyle];
		[Buttons[i] setTitle:[NSString stringWithUTF8String:Labels[i]]];
		[Buttons[i] setTarget:self];
		[Buttons[i] setAction:@selector(OnButtonClick:)];
		[self addSubview:Buttons[i]];
		[Buttons[i] release];
		X += ButtonWidth + Space;
	}
	X = Space;
	Y = Space + ButtonHeight + Space;
    int W = DlgWidth - 2 * Space;
    int H = DlgHeight - 3 * Space - ButtonHeight;
    // Set up the "Scroll View"
	ScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(X, Y, W, H)];
	[ScrollView setHasVerticalScroller:YES];
	[ScrollView setHasHorizontalScroller:YES];
	[ScrollView setBorderType:NSBezelBorder];
    [ScrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable]; // NSScrollView will automatically match the parent window's dimensions
    NSSize S = [ScrollView contentSize];
    // Set up the "Text View"
	TextView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, S.width, S.height)];
	[TextView setEditable:NO];
	[TextView setVerticallyResizable:YES];
	[TextView setHorizontallyResizable:YES];
	[TextView setBackgroundColor:[NSColor blackColor]];
	[TextView setTextColor:[NSColor whiteColor]];
    // Assemble the scroll and text views
	[ScrollView setDocumentView:TextView];
	[TextView release];
	[self addSubview:ScrollView];
	[ScrollView release];
    // Set the text to the "Text View"
	NSString *L = [NSString stringWithUTF8String:cMacMain_LogText->ToCharPtr()];
	[TextView setString:L];
	[TextView scrollRangeToVisible:NSMakeRange([L length], 0)]; // Scroll to the bottom
}

-(void)EnableControls {
	// Enable controls according to the mode
	const BOOL Enabled[(comms::cLog::ID::Exit - comms::cLog::ID::Debug) + 1] = {
		comms::cLog::Mode::Assert == cMacMain_LogMode, // Debug
		comms::cLog::Mode::Assert == cMacMain_LogMode || comms::cLog::Mode::Warning == cMacMain_LogMode, // Ignore
		comms::cLog::Mode::Assert == cMacMain_LogMode, // IgnoreAlways
		YES, // Copy
		comms::cLog::Mode::IsModal(cMacMain_LogMode) // Exit
	};
	int i, c = sizeof(Enabled) / sizeof(Enabled[0]);
	for(i = 0; i < c; i++) {
		[Buttons[i] setEnabled:Enabled[i]];
	}
	// Set title according to the required mode
	comms::cStr Title = comms::cMain_Title;
	if(comms::cLog::Mode::Message == cMacMain_LogMode) {
		Title << " Log";
	} else if(comms::cLog::Mode::Warning == cMacMain_LogMode) {
		Title << " Warning";
	} else if(comms::cLog::Mode::Error == cMacMain_LogMode) {
		Title << " Error";
	} else if(comms::cLog::Mode::Assert == cMacMain_LogMode) {
		Title << " Assert";
	}
	[cMacMain_LogDialog setTitle:[NSString stringWithUTF8String:Title.ToCharPtr()]];
}

-(void)OnButtonClick:(id)Sender {
	int i, c = sizeof(Buttons) / sizeof(Buttons[0]);
	for(i = 0; i < c; i++) {
		if(Buttons[i] == Sender) {
			cMacMain_LogResult = comms::cLog::ID::Debug + i;
			break;
		}
	}
	if(comms::cLog::ID::Copy == cMacMain_LogResult) {
		comms::cIO::CopyToClipboard(cMacMain_LogText->ToCharPtr());
	} else {
		[NSApp stopModal];
	}
}

-(BOOL)windowShouldClose:(id)sender {
	cMacMain_CloseLog = true;
	return NO;
}
@end // LogView

static void cMacMain_OneIterationOfMessageLoop();

// cMacMain_ShowLog
int cMacMain_ShowLog(const comms::cLog::Mode::Enum Mode, const comms::cStr *Text) {
	comms::cPause::SetSystemPause(true);
	cMacMain_LogMode = Mode;
    cMacMain_LogText = Text;
	cMacMain_LogResult = -1;
	cMacMain_CloseLog = false;
	
	// Create the autorelease pool
	NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
    
	// Alloc dialog
	NSRect rc = NSMakeRect(0.0, 0.0, 610.0, 460.0);
	NSUInteger Style = NSTitledWindowMask | NSResizableWindowMask;
	if(!comms::cLog::Mode::IsModal(Mode)) {
		Style |= NSClosableWindowMask;
	}
	cMacMain_LogDialog = [[NSWindow alloc] initWithContentRect:rc styleMask:Style backing:NSBackingStoreBuffered defer:YES];
    CenterWindow(cMacMain_LogDialog);
	// Minimum size
	NSRect DlgRc = [cMacMain_LogDialog frame];
	[cMacMain_LogDialog setMinSize:NSMakeSize(DlgRc.size.width - 50.0f, DlgRc.size.height - 50.0f)];
	// Create the view
	LogView *View = [[[LogView alloc] initWithFrame:rc] autorelease];
	[View CreateControls];
	[View EnableControls];
	// Set the window's view & delegate
	[cMacMain_LogDialog setContentView:View];
	[cMacMain_LogDialog setDelegate:(SET_DELEGATE_CAST)View];
    [cMacMain_LogDialog makeFirstResponder:View];
	// Show the dialog
	[cMacMain_LogDialog makeKeyAndOrderFront:nil];
	// Start message loop
	NSModalSession Session = [NSApp beginModalSessionForWindow:cMacMain_LogDialog];
	NSInteger r;
	for(;;) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]; // w/o this line CPU load will be 100%
		r = [NSApp runModalSession:Session];
		if((r != NSRunContinuesResponse) || cMacMain_CloseLog) {
			break;
		}
	}
	[NSApp endModalSession:Session];
	[cMacMain_LogDialog close];
	cMacMain_LogDialog = nil;
    cMacMain_OneIterationOfMessageLoop(); // With this line click on [Debug] button closes log dialog before returning to Xcode
    cMacMain_ActivateMainWindow();
    cMacMain_SendMouseMoveEvent();
	[Pool release]; // Release the pool
	if(comms::cLog::ID::Exit == cMacMain_LogResult && comms::cLog::Mode::IsAutoExit(cMacMain_LogMode)) {
		exit(0);
	}
	return cMacMain_LogResult;
}

// cMacMain_GetMainWindowRectGlobalSpace
void cMacMain_GetMainWindowRectGlobalSpace(comms::cRect *MainWindowRectGlobalSpace) {
    @autoreleasepool {
        NSRect InWindow = [cMacMain_View frame];
        NSRect OnScreen = [cMacMain_GetWindow() convertRectToScreen:InWindow];
        MainWindowRectGlobalSpace->SetSize(OnScreen.origin.x, OnScreen.origin.y, OnScreen.size.width, OnScreen.size.height);
    }
}

// cMacMain_ConvertToNSCursor
static NSCursor * cMacMain_ConvertToNSCursor(const comms::cInput::Cursor::Enum Type) {
    switch(Type) {
        case comms::cInput::Cursor::UpArrow:
            return [NSCursor resizeUpCursor];
        case comms::cInput::Cursor::SizeHor:
            return [NSCursor resizeLeftRightCursor];
        case comms::cInput::Cursor::SizeVert:
            return [NSCursor resizeUpDownCursor];
        case comms::cInput::Cursor::SizeSlash:
        case comms::cInput::Cursor::SizeBackSlash:
            return [NSCursor closedHandCursor];
        case comms::cInput::Cursor::SizeAll:
            return [NSCursor openHandCursor];
        case comms::cInput::Cursor::IBeam:
            return [NSCursor IBeamCursor];
        case comms::cInput::Cursor::Cross:
            return [NSCursor crosshairCursor];
        case comms::cInput::Cursor::Wait:
        case comms::cInput::Cursor::Stop:
            return [NSCursor operationNotAllowedCursor];
        default:
            return [NSCursor arrowCursor];
    }
}

static comms::cFile cMacMain_DropFile;
static NSURLConnection *cMacMain_DropConnection = nil;
static NSMutableData *cMacMain_DropData = nil;
static comms::cStr cMacMain_DropURL;

// cMacMain_OnDrop
static void cMacMain_OnDrop() {
    int l = (int)[cMacMain_DropData length];
    cMacMain_DropFile.Copy([cMacMain_DropData bytes], l);
    // Cutoff '?' suffix
    comms::cStr S = cMacMain_DropURL;
    int i = S.IndexOf('?');
    if(i != -1) {
        S.Remove(i);
    }
    // File extension
    comms::cStr T = S.GetFileExtension();
    if(!T.IsEmpty()) {
        // Search codec
        const comms::cList<comms::cImageCodecInfo> &Codecs = comms::cIO::GetImageCodecs();
        comms::cImageCodec *C = nullptr;
        for(i = 0; i < Codecs.Count(); i++) {
            if(comms::cStr::EqualsNoCase(Codecs[i].FileExtension, T)) {
                C = Codecs[i].Codec;
                break;
            }
        }
        if(C != nullptr) {
            if(C->Decode(cMacMain_DropFile, &comms::cMain_OnDropImage)) {
                cMacMain_ActivateApp();
                comms::cMain_OnDrop(nullptr); // Web image callback
                return;
            }
        }
    }
    cMacMain_ActivateApp();
    comms::cMain_OnDropURL(cMacMain_DropURL);
}

static void AddMouseEventFromPenEvent(const comms::cInputEvent_Pen *Pen) {
    comms::cInput::OldEvent *Mouse = new comms::cInput::OldEvent;
    switch(Pen->Type) {
        case comms::cInputEvent_Pen::TYPE::Down:
        case comms::cInputEvent_Pen::TYPE::Up:
            Mouse->Type = comms::cInput::OldEvent::TYPE_BUTTON;
            break;
        case comms::cInputEvent_Pen::TYPE::Move:
            Mouse->Type = comms::cInput::OldEvent::TYPE_MOUSEMOVE;
            break;
        default:
            cAssert(0);
    }
    Mouse->Code = comms::cInput::LeftButton;
    Mouse->Pressed = (comms::cInputEvent_Pen::TYPE::Down == Pen->Type);
    Mouse->MousePos = Pen->PenPos;
    Mouse->MouseDelta = Pen->PenDelta;
    comms::cInput::AddEvent(Mouse);
    Mouse = nullptr;
}

// MainView
@implementation MainView

-(id)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSURLPboardType, nil]];
    return self;
}

-(void)resetCursorRects {
    [self addCursorRect:[self bounds] cursor:cMacMain_ConvertToNSCursor(cMacMain_CurCursor)];
}

// drawRect
-(void)drawRect:(NSRect)rect {
}

-(void)keyDown:(NSEvent *)theEvent {
}

-(void)keyUp:(NSEvent *)theEvent {
}
 
-(void)flagsChanged:(NSEvent *)theEvent {
}

// GetMousePositionFrom
-(const comms::cVec2)GetMousePositionFrom:(NSEvent *)Event {
	NSPoint P = [Event locationInWindow];
	P = [self convertPoint:P fromView:nil];
	// Recent Mac OS X sends tablet coords with subpixel precision.
	// We don't need that because within 3D-Coat there are a lot of places
	// where start/previous coords are cached as integers.
    int X = P.x * macOS_Global_Scale;
    int Y = P.y * macOS_Global_Scale;
    return comms::cVec2(X, Y);
}

// GetMouseDeltaFrom
-(const comms::cVec2)GetMouseDeltaFrom:(NSEvent *)Event {
    int X = [Event deltaX];
    int Y = -[Event deltaY];
    return comms::cVec2(X, Y);
}

// AddMouseButton
-(void)AddMouseButton:(NSEvent *)Event :(int)Code :(bool)Pressed {
	comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
	E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
	E->Code = Code;
	E->DoubleClick = (2 == [Event clickCount]);
	E->Pressed = Pressed;
    E->MousePos = [self GetMousePositionFrom:Event];
    comms::cInput::AddEvent(E);
	E = nullptr;
}

// mouseDown
-(void)mouseDown:(NSEvent *)Event {
    if(g_Tablet.Proximity) {
        // For some unknown reason we should generate mouse event from pen event inside "HandlePointEvent" when in proximity.
        // Otherwise "3DCoat" very rarely misses strokes.
        [self HandleTabletEvent:Event];
    } else {
        [self AddMouseButton:Event :comms::cInput::LeftButton :true];
    }

}

// mouseUp
-(void)mouseUp:(NSEvent *)Event {
    if(g_Tablet.Proximity) {
        [self HandleTabletEvent:Event];
    } else {
        [self AddMouseButton:Event :comms::cInput::LeftButton :false];
    }
}

// rightMouseDown
-(void)rightMouseDown:(NSEvent *)Event {
	[self AddMouseButton:Event :comms::cInput::RightButton :true];
}

// rightMouseUp
-(void)rightMouseUp:(NSEvent *)Event {
	[self AddMouseButton:Event :comms::cInput::RightButton :false];
}

// otherMouseDown
-(void)otherMouseDown:(NSEvent *)Event {
	[self AddMouseButton:Event :comms::cInput::MiddleButton :true];
}

// otherMouseUp
-(void)otherMouseUp:(NSEvent *)Event {
	[self AddMouseButton:Event :comms::cInput::MiddleButton :false];
}

// AddMouseMove
-(void)AddMouseMove:(NSEvent *)Event {
	comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
	E->Type = comms::cInput::OldEvent::TYPE_MOUSEMOVE;
	E->MousePos = [self GetMousePositionFrom:Event];
    E->MouseDelta = [self GetMouseDeltaFrom:Event];
    comms::cInput::AddEvent(E);
    E = nullptr;
}

// mouseMoved
-(void)mouseMoved:(NSEvent *)Event {
	[self AddMouseMove:Event];
}

// mouseDragged
-(void)mouseDragged:(NSEvent *)Event {
	[self AddMouseMove:Event];
    [self HandleTabletEvent:Event];
}

// rightMouseDragged
-(void)rightMouseDragged:(NSEvent *)Event {
	[self AddMouseMove:Event];
}

// otherMouseDragged
-(void)otherMouseDragged:(NSEvent *)Event {
	[self AddMouseMove:Event];
}

// AddMouseWheel
-(void)AddMouseWheel:(NSEvent *)Event {
	float D = [Event deltaY];
	if(comms::cMath::IsZero(D)) {
		return; // A lot of messages have zero delta
	}
	comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
	E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
	E->WheelDelta = comms::cMath::Sign(D);
	E->Code = E->WheelDelta > 0.0f ? comms::cInput::WheelUp : comms::cInput::WheelDown;
	E->Pressed = true;
	E->MousePos = [self GetMousePositionFrom:Event];
    comms::cInput::AddEvent(E);
	E = nullptr;
}

// scrollWheel
-(void)scrollWheel:(NSEvent *)Event {
	[self AddMouseWheel:Event];
}

// HandleTabletEvent
-(void)HandleTabletEvent:(NSEvent *)Event {
    const NSEventSubtype ST = [Event subtype];
    if(NSEventSubtypeTabletProximity == ST) {
        [self HandleProximityEvent:Event]; // The proximity event is rarely fired here
    } else if(NSEventSubtypeTabletPoint == ST) {
        [self HandlePointEvent:Event]; // The point event handler is regularly called here
    }
}

// HandleProximityEvent
-(void)HandleProximityEvent:(NSEvent *)Event {
    // Eraser type is available only in proximity event handler
    const bool Eraser = (NSPointingDeviceTypeEraser == [Event pointingDeviceType]);
    g_Tablet.Eraser = Eraser;
    // The proximity event is very rare and it is fired only when the pen is entering or leaving the surface
    g_Tablet.Proximity = [Event isEnteringProximity];
}

// HandlePointEvent
-(void)HandlePointEvent:(NSEvent *)Event {
    g_Tablet.CurPressure = [Event pressure];
    NSPoint T = [Event tilt];
    g_Tablet.CurTilt.Set(T.x, T.y);
    const bool Zero = (0.0f == g_Tablet.CurPressure);
    comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
    const comms::cVec2 Pos = [self GetMousePositionFrom:Event];
    Pen->PenDelta = Pos - g_Tablet.CurPos;
    Pen->PenPos = g_Tablet.CurPos = Pos;
    Pen->PenPressure = g_Tablet.CurPressure;
    Pen->Eraser = g_Tablet.Eraser;
    if(g_Tablet.PenPressed) {
        if(Zero) {
            Pen->Type = comms::cInputEvent_Pen::TYPE::Up;
            g_Tablet.PenPressed = false;
        } else {
            Pen->Type = comms::cInputEvent_Pen::TYPE::Move;
        }
    } else {
        cAssert(!g_Tablet.PenPressed);
        // Can be "Zero"
        Pen->Type = comms::cInputEvent_Pen::TYPE::Down;
        g_Tablet.PenPressed = true;
    }
    comms::cInput::AddEvent(Pen);
    AddMouseEventFromPenEvent(Pen);
    Pen = nullptr;
}

-(void)tabletProximity:(NSEvent *)Event {
    [self HandleProximityEvent:Event]; // The proximity event is fired regularly here
}

-(void)tabletPoint:(NSEvent *)Event {
    [self HandlePointEvent:Event]; // This event handler is called very rare from here
}

// windowWillClose
-(void)windowWillClose:(NSNotification *)notify {
    cMacMain_PostQuitMessage = true;
}

// windowShouldClose
-(BOOL)windowShouldClose:(id)sender {
    return (BOOL)comms::cMain_OnClose();
}

// OnMenuQuit
-(void)OnMenuQuit:(id)sender {
    if(comms::cMain_OnClose()) {
        cMacMain_PostQuitMessage = true;
	}
}

-(void)changeFont:(id)sender {
    NSFontManager *FontManager = [NSFontManager sharedFontManager];
    NSFont *F = [NSFont systemFontOfSize:12];
    F = [FontManager convertFont:F];
    cMacMain_UpdateFontNamesAndSize(F);
    cMacMain_FontChanged = true;
}

// draggingEntered
-(NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    if((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric) {
        return NSDragOperationGeneric;
    } else {
        return NSDragOperationNone;
    }
}

// draggingExited
-(void)draggingExited:(id<NSDraggingInfo>)sender {
}

// draggingUpdated
-(NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    if((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric) {
        return NSDragOperationGeneric;
    } else {
        return NSDragOperationNone;
    }
}

// draggingEnded
-(void)draggingEnded:(id<NSDraggingInfo>)sender {
}

// prepareForDragOperation
-(BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    return YES;
}

// performDragOperation
-(BOOL)performDragOperation:(id <NSDraggingInfo>)sender {
    if(nil == cMacMain_DropConnection) { // Start new download only when the previous has been finished
        comms::cMain_OnDropImage.Free();
        NSPasteboard *Paste = [sender draggingPasteboard];
        if([[Paste types] containsObject:NSURLPboardType]) {
            NSURL *URL = [NSURL URLFromPasteboard:Paste];
            // Does the URL point to local file?
            if([[NSFileManager defaultManager] fileExistsAtPath:[URL path]]) {
                cMacMain_ActivateApp();
                comms::cStr LocalPn = [[URL path] UTF8String];
                comms::cMain_OnDrop(LocalPn);
            } else {
                // The URL points to web resource
                cMacMain_DropURL = [[URL absoluteString] UTF8String];
                NSURLRequest *Request = [NSURLRequest requestWithURL:URL cachePolicy:NSURLRequestReloadIgnoringCacheData timeoutInterval:10.0];
                cMacMain_DropConnection = [[NSURLConnection alloc] initWithRequest:Request delegate:self];
                if(cMacMain_DropConnection != nil) {
                    cMacMain_DropData = [[NSMutableData data] retain];
                }
            }
        }
    }
    return YES;
}

// connection:didReceiveData
-(void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
    if(connection == cMacMain_DropConnection) {
        [cMacMain_DropData appendData:data];
    }
}

// connectionDidFinishLoading
-(void)connectionDidFinishLoading:(NSURLConnection *)connection {
    if(connection == cMacMain_DropConnection) {
        cMacMain_OnDrop();
        [cMacMain_DropData release];
        cMacMain_DropData = nil;
        [cMacMain_DropConnection release];
        cMacMain_DropConnection = nil;
    }
}

@end // MainView

// cMacMain_AddKeyboardCode
static void cMacMain_AddKeyboardCode(int Code, bool Down, int Char) {
	cMacMain_KeyboardState.IsDown[Code] = Down;
	cMacMain_KeyboardState.Chars[Code] = Char;
	comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
	E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
	E->Code = Code;
	E->Pressed = Down;
	comms::cInput::AddEvent(E);
	E = nullptr;
}

// cMacMain_SendKeyUpEvents
static void cMacMain_SendKeyUpEvents() {
	int i;
	for(i = 0; i < cMacMain_KeyboardState.IsDown.Count(); i++) {
		if(cMacMain_KeyboardState.IsDown[i]) {
			if(!comms::cInput::IsDownAcquire(i)) {
				cMacMain_KeyboardState.IsDown[i] = false;
				cMacMain_AddKeyboardCode(i, false, -1);
			}
		}
	}
}

// cMacMain_SendUpEvents
static void cMacMain_SendUpEvents() {
	// If "Down" events were sent, but OS didn't send "Up" events due to the window deactivation,
	// then we should manually generate "Up" events
	cMacMain_SendKeyUpEvents();
}

// cMacMain_RemapKeyCode
static void cMacMain_RemapKeyCode(int *KeyCode) {
    if(0x4C == *KeyCode) { // Fn + Enter = Insert
        *KeyCode = cMacMain_Codes[comms::cInput::Codes::Insert];
    }
}

// cMacMain_AddKeyboardEvent
static void cMacMain_AddKeyboardEvent(NSEvent *Event, bool Down) {
	if([Event isARepeat]) { // Ignore repeats
		return;
	}
	int k = [Event keyCode];
	cMacMain_RemapKeyCode(&k);
    int Code = cMacMain_Codes.IndexOf(k);
    if(-1 == Code) {
        return;
    }
	// Extract char
	int c = -1;
	NSString *S = [Event characters];
	if([S length] > 0) {
		c = [S characterAtIndex:0];
	}
	cMacMain_AddKeyboardCode(Code, Down, c);
}

// cMacMain_FlagsChanged
static void cMacMain_FlagsChanged(NSEvent *Event) {
	const comms::dword Masks[] = {
		NSAlphaShiftKeyMask, NSShiftKeyMask, NSAlternateKeyMask, NSCommandKeyMask
	};
    const comms::dword AltMasks[] = {
        0, 0, 0, NSControlKeyMask
    };
	const int Codes[] = {
		comms::cInput::CapsLock, comms::cInput::Shift, comms::cInput::Alt, comms::cInput::Control
	};
	int n = sizeof(Codes) / sizeof(Codes[0]), i;
	comms::dword Modifers = (comms::dword)[Event modifierFlags];
	bool r;
	for(i = 0; i < n; i++) {
		r = ((Modifers & Masks[i]) != 0);
        if(!r) {
            r = ((Modifers & AltMasks[i]) != 0);
        }
		if(cMacMain_KeyboardState.IsDown[Codes[i]] != r) {
			cMacMain_AddKeyboardCode(Codes[i], r, -1);
		}
	}
}

// cMacMain_HandleEvent
static void cMacMain_HandleEvent(NSEvent *Event) {
	NSEventType Type = [Event type];
	if(NSKeyDown == Type) {
		cMacMain_AddKeyboardEvent(Event, true);
	} else if(NSKeyUp == Type) {
		cMacMain_AddKeyboardEvent(Event, false);
	} else if(NSFlagsChanged == Type) {
		cMacMain_FlagsChanged(Event);
	}
}

// For some reason, Apple has removed "setAppleMenu" from the headers in 10.4,
// but the method still is there and works. To avoid warnings, we declare it ourselves here.
@interface NSApplication(Missing_Methods)
-(void)setAppleMenu:(NSMenu *)Menu;
@end

// cMacMain_CreateApplicationMenustatic
static void cMacMain_CreateApplicationMenu() {
	NSString *AppName = [NSString stringWithUTF8String:comms::cMain_Title.ToCharPtr()];
	NSMenu *AppleMenu = [[NSMenu alloc] initWithTitle:@""];
	// Hide
	NSString *Title = [@"Hide " stringByAppendingString:AppName];
	NSMenuItem *MenuItem = [[NSMenuItem alloc] initWithTitle:Title action:@selector(hide:) keyEquivalent:@"h"];
	[AppleMenu addItem:MenuItem];
	[MenuItem release];
	// Hide Others
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[MenuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask | NSCommandKeyMask)];
	[AppleMenu addItem:MenuItem];
	[MenuItem release];
	// Show All
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
	[AppleMenu addItem:MenuItem];
	[MenuItem release];
	// -
	[AppleMenu addItem:[NSMenuItem separatorItem]];
	// Quit
	Title = [@"Quit " stringByAppendingString:AppName];
	MenuItem = [[NSMenuItem alloc] initWithTitle:Title action:@selector(OnMenuQuit:) keyEquivalent:@"q"];
	[AppleMenu addItem:MenuItem];
	[MenuItem release];
	// Tell the application object that this is now the application menu
	[NSApp setAppleMenu:AppleMenu];
	// Put menu into the menubar
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[MenuItem setSubmenu:AppleMenu];
	[[NSApp mainMenu] addItem:MenuItem];
	[MenuItem release];
	// Finally give up our references to the objects
	[AppleMenu release];
}

// cMacMain_CreateWindowMenu
static void cMacMain_CreateWindowMenu() {
	NSMenu *WindowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
	// Minimize
	NSMenuItem *MenuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
	[WindowMenu addItem:MenuItem];
    cMacMain_MinimizeMenuItem = MenuItem;
	[MenuItem release];
	// Tell the application object that this is now the window menu
	[NSApp setWindowsMenu:WindowMenu];
	// Put menu into the menubar
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
	[MenuItem setSubmenu:WindowMenu];
	[[NSApp mainMenu] addItem:MenuItem];
	[MenuItem release];
	// Finally give up our references to the objects
	[WindowMenu release];
}

// cMacMain_CreateEditMenu
static void cMacMain_CreateEditMenu() {
	NSMenu *EditMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    NSMenuItem *MenuItem = nil;
	// Undo
    // http://www.cocoabuilder.com/archive/cocoa/307917-validatemenuitem-not-called-for-undo-menu-item.html
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
	[EditMenu addItem:MenuItem];
	[MenuItem release];
	// ---
	[EditMenu addItem:[NSMenuItem separatorItem]];
	// Cut
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
	[EditMenu addItem:MenuItem];
	[MenuItem release];
	// Copy
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
	[EditMenu addItem:MenuItem];
	[MenuItem release];
	// Paste
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
	[EditMenu addItem:MenuItem];
	[MenuItem release];
	// Select All
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
	[EditMenu addItem:MenuItem];
	[MenuItem release];
	// Put menu into the menubar
	MenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
	[MenuItem setSubmenu:EditMenu];
	[[NSApp mainMenu] addItem:MenuItem];
	[MenuItem release];
	// Finally give up our references to the objects
	[EditMenu release];
}

// cMacMain_CreateMainMenu
void cMacMain_CreateMainMenu() {
    // Create Menu
    NSMenu *MainMenu = [[NSMenu alloc] initWithTitle:@""];
    [NSApp setMainMenu:MainMenu];
    cMacMain_CreateApplicationMenu();
    cMacMain_CreateWindowMenu();
    cMacMain_CreateEditMenu();
    [NSApp finishLaunching]; // Menu isn't visible w/o this call
}

// cMacMain_OneIterationOfMessageLoop
static void cMacMain_OneIterationOfMessageLoop() {
    @autoreleasepool {
        NSEvent *Event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
        // OS X closes NSOpen(Save)Panel not immediately after returning from method [runModal] but later in the subsequent event handling.
        // In OS X 10.10 Yosemite open/save file dialogs on closing can replace current OpenGL context.
        cMacMain_RestoreContext(); // Without this line we will get several "GL_INVALID_OPERATION" fails in cRenderGL.cpp/HandleGLError()
        // followed by "EXC_BAD_ACCESS" in subsequent call to "glDrawArrays" function.
        if(Event) {
            // We should handle keyboard events separately because "Cocoa" doesn't call "NSView::keyUp" for a key that was pressed with "Cmd" modifier
            // But we should override "NSView::keyDown/keyUp" methods to notify "Cocoa" that we are handling them
            cMacMain_HandleEvent(Event);
            [NSApp sendEvent:Event];
            [NSApp updateWindows];
        } else {
            cMacMain_OnRender();
        }
    }
}

//*********************************************************************************************
// 3Dconnexion SpaveNavigator
//*********************************************************************************************
namespace comms {
bool cMain_TdxTimeProportional() {
    return false;
}
} // comms
#ifdef COMMS_3DCONNEXION
#import <3DconnexionClient/ConnexionClientAPI.h>
extern int16_t SetConnexionHandlers(ConnexionMessageHandlerProc messageHandler, ConnexionAddedHandlerProc addedHandler, ConnexionRemovedHandlerProc removedHandler, bool useSeparateThread) __attribute__((weak_import));
extern void CleanupConnexionHandlers(void) __attribute__((weak_import));
extern uint16_t RegisterConnexionClient(uint32_t signature, uint8_t *name, uint16_t mode, uint32_t mask) __attribute__((weak_import));
extern void UnregisterConnexionClient(uint16_t clientID)  __attribute__((weak_import));


static std::mutex cMacMain_TdxMutex;
static UInt16 cMacMain_TdxClientID(0);
static comms::cVec3 cMacMain_TdxTranslation(0.0f, 0.0f, 0.0f);
static comms::cVec3 cMacMain_TdxRotation(0.0f, 0.0f, 0.0f);
static comms::cVec2i cMacMain_TdxButtonState(0, 0); // { Left button, Right button }, 1 - Down, 0 - Up

namespace comms {

// cMain_GetTdxState
void cMain_GetTdxState(cVec3 *Translation, cVec3 *Rotation, cVec2i *ButtonState) {
	cMacMain_TdxMutex.lock();
	if(Translation != nullptr) {
		*Translation = cMacMain_TdxTranslation;
        comms::cMath::Swap(Translation->y, Translation->z);
        (*Translation) *= cVec3(1.0f, -1.0f, -1.0f);
	}
	if(Rotation != nullptr) {
		*Rotation = cMacMain_TdxRotation;
        comms::cMath::Swap(Rotation->y, Rotation->z);
        (*Rotation) *= cVec3(1.0f, -1.0f, -1.0f);
	}
	if(ButtonState != nullptr) {
		*ButtonState = cMacMain_TdxButtonState;
	}
	cMacMain_TdxMutex.unlock();
}

} // comms

typedef SInt16 cMacMain_TdxDeviceAxes[6];

// cMacMain_TdxComputeButtons
static void cMacMain_TdxComputeButtons(UInt16 btnPressed, SInt16 btnState) {
	int Index = btnPressed - 1;
	if(Index >= 0 && Index < 2) {
		cMacMain_TdxMutex.lock();
		cMacMain_TdxButtonState[Index] = btnState;
		cMacMain_TdxMutex.unlock();
	}
}

// cMacMain_TdxComputeAxes
static void cMacMain_TdxComputeAxes(const cMacMain_TdxDeviceAxes inArrayPtr) {
	cMacMain_TdxMutex.lock();
	cMacMain_TdxTranslation.Set((float)inArrayPtr[0], (float)inArrayPtr[1], (float)inArrayPtr[2]);
	cMacMain_TdxRotation.Set((float)inArrayPtr[3], (float)inArrayPtr[4], (float)inArrayPtr[5]);
	cMacMain_TdxMutex.unlock();
}

// cMacMain_TdxComputeEventZero
static void cMacMain_TdxComputeEventZero() {
	static SInt16 zero[6];
	memset(zero, 0, sizeof(zero));
	cMacMain_TdxComputeAxes(zero);
}

//------------------------------------------------------------------------------------------------------
// cMacMain_TdxHandler
//------------------------------------------------------------------------------------------------------
static void cMacMain_TdxHandler(io_connect_t connection, natural_t messageType, void *messageArgument) {
	ConnexionDeviceStatePtr msg = (ConnexionDeviceStatePtr)messageArgument;
	static UInt16 lastBtnPressed = 0;
	
	switch(messageType) {
		case kConnexionMsgDeviceState:
			if(msg->client == cMacMain_TdxClientID) {
				switch(msg->command) {
					case kConnexionCmdHandleAxis: {
						static SInt16 zerodata[] = { 0, 0, 0, 0, 0, 0 };
						static Boolean isZero = FALSE, wasZero;
						
						wasZero = isZero;
						if((isZero = (memcmp(msg->axis, zerodata, sizeof(zerodata)) == 0))) {
							if(wasZero == FALSE) {
								cMacMain_TdxComputeEventZero();
							}
						} else {
							cMacMain_TdxComputeAxes(msg->axis);
						}
						break;
					}
					case kConnexionCmdHandleButtons: {
						SInt16 buttonState;
						if(msg->value == 0) {
							buttonState = 0;
						} else {
							lastBtnPressed = msg->buttons;
							buttonState = 1;
						}
						cMacMain_TdxComputeButtons(lastBtnPressed, buttonState);
						break;
					}
					default:
						break;
				}
			}
			break;
		default:
			break;
	}
} // cMacMain_TdxHandler

// cMacMain_TdxInitDevice
static void cMacMain_TdxInitDevice() {
    const comms::cStr Prefix(" 3Dconnexion | "), Separator(60, '-');
    comms::cLog::Message(Separator);
	if(nullptr == SetConnexionHandlers) {
		comms::cLog::Message(Prefix + "framework not found");
    } else {
        comms::cLog::Message(Prefix + "framework found");
        int16_t r = SetConnexionHandlers(cMacMain_TdxHandler, nullptr, nullptr, YES);
        comms::cLog::Message(Prefix + "SetConnexionHandlers() returns %d", r);
        if(noErr == r) {
            cMacMain_TdxClientID = RegisterConnexionClient('Coat', nullptr, kConnexionClientModeTakeOver, kConnexionMaskAll);
            comms::cLog::Message(Prefix + "RegisterConnexionClient() returns %d", cMacMain_TdxClientID);
        }
    }
    comms::cLog::Message(Separator);
}

// cMacMain_TdxTerminateDevice
static void cMacMain_TdxTerminateDevice() {
	if(nullptr == SetConnexionHandlers) {
		return;
	}
	if(cMacMain_TdxClientID != 0) {
        UnregisterConnexionClient(cMacMain_TdxClientID);
        cMacMain_TdxClientID = 0;
	}
	CleanupConnexionHandlers();
}

#else // !COMMS_3DCONNEXION

namespace comms {

// cMain_GetTdxState
void cMain_GetTdxState(cVec3 *Translation, cVec3 *Rotation, cVec2i *ButtonState) {
    if(Translation != nullptr) {
        *Translation = cVec3::Zero;
    }
    if(Rotation != nullptr) {
        *Rotation = cVec3::Zero;
    }
    if(ButtonState != nullptr) {
        ButtonState->Set(0);
    }
}
    
} // comms

#endif // COMMS_3DCONNEXION

// To resolve the problem "3D-Coat freezes the whole Mac" we should install our crash handler with simple "signal(SIGSEGV, CrashHandler)" and
// "signal(SIGBUS, CrashHandler)". On crash, after saving the scene, we should revert default signal handlers with "signal(SIGSEGV, SIG_DFL)" and
// "signal(SIGBUS, SIG_DFL)". Finally we should exit the application with "kill(getpid(), 0)".

// CrashHandler
static void CrashHandler(int sig) {
    comms::cMain_OnCrash();
#ifdef COMMS_3DCOAT
    comms::cLog::Flush();
#endif // COMMS_3DCOAT
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    kill(getpid(), 0);
}

// CrashInstall
static void CrashInstall() {
    signal(SIGSEGV, CrashHandler);
    signal(SIGBUS, CrashHandler);
}

// cMacMain_GenTempFilePn
//static void cMacMain_GenTempFilePn(comms::cStr *TempFilePn) {
//    @autoreleasepool {
//        CFUUIDRef uuid = CFUUIDCreate(nullptr);
//        CFStringRef uuidStr = CFUUIDCreateString(nullptr, uuid);
//        NSString *prefix = [NSString stringWithUTF8String:comms::cMain_Title.ToCharPtr()];
//        NSString *result = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@-%@", prefix, uuidStr]];
//        *TempFilePn = [result UTF8String];
//        CFRelease(uuidStr);
//        CFRelease(uuid);
//    }
//}

// main
int main(int argc, char *argv[]) {
	comms::cMutex::GetInstance();
	if(argc > 1) {
		int i;
		for(i = 1; i < argc; i++) {
            if(!cMacMain_CmdLineArgs.IsEmpty()) {
                cMacMain_CmdLineArgs += " ";
            }
            cMacMain_CmdLineArgs.Append(argv[i]);
        }
    }
	
    CrashInstall();
	
	setpriority(PRIO_PROCESS, 0, 20);
	
	cMacMain_Codes.Add(0x35);			// Esc
	cMacMain_Codes.Add(0x7A);			// F1
	cMacMain_Codes.Add(0x78);			// F2
	cMacMain_Codes.Add(0x63);			// F3
	cMacMain_Codes.Add(0x76);			// F4
	cMacMain_Codes.Add(0x60);			// F5
	cMacMain_Codes.Add(0x61);			// F6
	cMacMain_Codes.Add(0x62);			// F7
	cMacMain_Codes.Add(0x64);			// F8
	cMacMain_Codes.Add(0x65);			// F9
	cMacMain_Codes.Add(0x6D);			// F10
	cMacMain_Codes.Add(0x67);			// F11
	cMacMain_Codes.Add(0x6F);			// F12
	cMacMain_Codes.Add(0x1D);			// Zero, "0"
	cMacMain_Codes.Add(0x12);			// One, "1"
	cMacMain_Codes.Add(0x13);			// Two, "2"
	cMacMain_Codes.Add(0x14);			// Three, "3"
	cMacMain_Codes.Add(0x15);			// Four, "4"
	cMacMain_Codes.Add(0x17);			// Five, "5"
	cMacMain_Codes.Add(0x16);			// Six, "6"
	cMacMain_Codes.Add(0x1A);			// Seven, "7"
	cMacMain_Codes.Add(0x1C);			// Eight, "8"
	cMacMain_Codes.Add(0x19);			// Nine, "9"
	cMacMain_Codes.Add(0x1B);			// Minus, "-"
	cMacMain_Codes.Add(0x18);			// Equals, "="
	cMacMain_Codes.Add(0x33);			// BackSpace
	cMacMain_Codes.Add(0x30);        	// Tab
	cMacMain_Codes.Add(0x39);			// CapsLock
	cMacMain_Codes.Add(0x72);			// Insert (Fn + Enter)
	cMacMain_Codes.Add(0x75);			// Delete (Fn + BackSpace)
	cMacMain_Codes.Add(0x73);			// Home (Fn + Left)
	cMacMain_Codes.Add(0x77);			// End (Fn + Right)
	cMacMain_Codes.Add(0x74);			// PageUp (Fn + Up)
	cMacMain_Codes.Add(0x79);			// PageDown (Fn + Down)
	cMacMain_Codes.Add(0x7E);			// Up
	cMacMain_Codes.Add(0x7D);			// Down
	cMacMain_Codes.Add(0x7B);			// Left
	cMacMain_Codes.Add(0x7C);			// Right
	cMacMain_Codes.Add(0x2A);			// BackSlash, "\\"
	cMacMain_Codes.Add(0x24);           // Enter
	cMacMain_Codes.Add(0x21);			// LeftBracket, "["
	cMacMain_Codes.Add(0x1E);			// RightBracket, "]"
	cMacMain_Codes.Add(0x29);			// SemiColon, ";"
	cMacMain_Codes.Add(0x27);			// SingleQuote, "\'"
	cMacMain_Codes.Add(0x2B);			// Comma, ","
	cMacMain_Codes.Add(0x2F);			// Period, "."
	cMacMain_Codes.Add(0x2C);			// Slash, "/"
	cMacMain_Codes.Add(0x38); // 0x3C	// Shift
	cMacMain_Codes.Add(0x37); // 0x36	// Control (Command)
	cMacMain_Codes.Add(0x3A); // 0x3D	// Alt (Option)
	cMacMain_Codes.Add(0x31);			// Space
	cMacMain_Codes.Add(0x32);			// Tilda, "~"
	cMacMain_Codes.Add(0x00);			// A
	cMacMain_Codes.Add(0x0B);			// B
	cMacMain_Codes.Add(0x08);			// C
	cMacMain_Codes.Add(0x02);			// D
	cMacMain_Codes.Add(0x0E);			// E
	cMacMain_Codes.Add(0x03);			// F
	cMacMain_Codes.Add(0x05);			// G
	cMacMain_Codes.Add(0x04);			// H
	cMacMain_Codes.Add(0x22);			// I
	cMacMain_Codes.Add(0x26);			// J
	cMacMain_Codes.Add(0x28);			// K
	cMacMain_Codes.Add(0x25);			// L
	cMacMain_Codes.Add(0x2E);			// M
	cMacMain_Codes.Add(0x2D);			// N
	cMacMain_Codes.Add(0x1F);			// O
	cMacMain_Codes.Add(0x23);			// P
	cMacMain_Codes.Add(0x0C);			// Q
	cMacMain_Codes.Add(0x0F);			// R
	cMacMain_Codes.Add(0x01);			// S
	cMacMain_Codes.Add(0x11);			// T
	cMacMain_Codes.Add(0x20);			// U
	cMacMain_Codes.Add(0x09);			// V
	cMacMain_Codes.Add(0x0D);			// W
	cMacMain_Codes.Add(0x07);			// X
	cMacMain_Codes.Add(0x10);			// Y
	cMacMain_Codes.Add(0x06);			// Z
	cMacMain_Codes.Add(0x52);			// NumPad0
	cMacMain_Codes.Add(0x53);			// NumPad1
	cMacMain_Codes.Add(0x54);			// NumPad2
	cMacMain_Codes.Add(0x55);			// NumPad3
	cMacMain_Codes.Add(0x56);			// NumPad4
	cMacMain_Codes.Add(0x57);			// NumPad5
	cMacMain_Codes.Add(0x58);			// NumPad6
	cMacMain_Codes.Add(0x59);			// NumPad7
	cMacMain_Codes.Add(0x5B);			// NumPad8
	cMacMain_Codes.Add(0x5C);			// NumPad9
	cMacMain_Codes.Add(0x45);			// Add
	cMacMain_Codes.Add(0x4E);			// Subtract
	cMacMain_Codes.Add(0x43);			// Multiply
	cMacMain_Codes.Add(0x4B);			// Divide
	cMacMain_Codes.Add(0x41);			// Decimal
    
    // Create the autorelease pool
	NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
    
    // Since macOS Catalina we call "cMain_OnPreInit" before "Create the application object" below.
    // This is because "FServer" daemon (when NSApp has been created below) prevents macOS from restarting
    // and displays unwanted icon on the Dock.
    comms::cMain_OnPreInit();
    
    cMacMain_CreateApp();

    // Create pixel format
	NSOpenGLPixelFormatAttribute Attrs[] = {
        kCGLPFAOpenGLProfile, kCGLOGLPVersion_3_2_Core, // OpenGL 3.2 (#version 150)
        NSOpenGLPFAColorSize, 32,
		NSOpenGLPFADepthSize, 24,
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFAScreenMask, CGDisplayIDToOpenGLDisplayMask(kCGDirectMainDisplay),
		0
	};
	NSOpenGLPixelFormat *PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:Attrs];
	if(nil == PixelFormat) {
		comms::cLog::Error("Couldn't create pixel format");
		return -1;
	}
	// Create OpenGL context
	cMacMain_OglContext = [[NSOpenGLContext alloc] initWithFormat:PixelFormat shareContext:nil];
    [PixelFormat release];
	PixelFormat = nil;
	if(nil == cMacMain_OglContext) {
		comms::cLog::Error("Couldn't create context");
		return -1;
	}
    // Create Menu
    cMacMain_CreateMainMenu();
	// Create the window
	NSRect rc = NSMakeRect(0, 0, 640, 480);
	cMacMain_AllocWindowedWindow(rc);
	[cMacMain_WindowedWindow center];
	[cMacMain_WindowedWindow zoom:nil];
	comms::cMain_SetWindowTitle(comms::cMain_Title.ToCharPtr());
    // Create the view
	cMacMain_View = [[[MainView alloc] initWithFrame:rc] autorelease];
    // Enable high resolution drawing
    [cMacMain_View setWantsBestResolutionOpenGLSurface:YES];
	// Set window's view & delegate
	[cMacMain_WindowedWindow setContentView:cMacMain_View];
	[cMacMain_WindowedWindow setDelegate:(SET_DELEGATE_CAST)cMacMain_View];
    [cMacMain_WindowedWindow makeFirstResponder:cMacMain_View];
	// Display window
	if(!comms::cSettings::GetInstance()->FullScreen) {
		[cMacMain_WindowedWindow makeKeyAndOrderFront:nil];
	}
	// Attach OpenGL context to the view
	[cMacMain_OglContext setView:cMacMain_View];
	// Make the context be the current OpenGL context
	[cMacMain_OglContext makeCurrentContext];
	
	if(comms::cRender::Init()) {
#ifdef COMMS_3DCONNEXION
		cMacMain_TdxInitDevice();
#endif // COMMS_3DCONNEXION
		comms::cMain_OnInit();
		
		// Run the main event loop
		while(!cMacMain_PostQuitMessage) {
            cMacMain_OneIterationOfMessageLoop();
		}
        
        comms::cMain_OnFree();
#ifdef COMMS_3DCONNEXION
		cMacMain_TdxTerminateDevice();
#endif // COMMS_3DCONNEXION
		comms::cRender::Free();
	}
	
	// Free OpenGL
	[NSOpenGLContext clearCurrentContext];
	[cMacMain_OglContext clearDrawable];
	[cMacMain_OglContext release];
	cMacMain_OglContext = nil;

	comms::cMain_OnPostFree();
    
    [Pool release]; // Release the pool
	
	return 0;
}


class BaseWidget;
typedef bool fnCycleEnd(BaseWidget* W);
void ProcessRenderCycle(fnCycleEnd *WhenEnd,BaseWidget* W) {
    while(WhenEnd != nullptr && !WhenEnd(W) && !IsInExitState) {
        cMacMain_OneIterationOfMessageLoop();
    }
    if(IsInExitState) {
        [NSApp terminate:nil];
    }
}

#endif // COMMS_MACOS
