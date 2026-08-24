#import <Cocoa/Cocoa.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSString *AppPath = [NSString stringWithCString:argv[1] encoding:NSUTF8StringEncoding];
        pid_t PID = atoi(argv[2]);
        ProcessSerialNumber PSN;
        while(GetProcessForPID(PID, &PSN) != procNotFound) {
            sleep(1);
        }
        [[NSWorkspace sharedWorkspace] openFile:[AppPath stringByExpandingTildeInPath]];
    }
    return 0;
}