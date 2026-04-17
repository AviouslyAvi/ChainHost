#if __APPLE__
#import <Cocoa/Cocoa.h>

extern "C" void attachChildWindowToParent (void* childHandle, void* parentHandle);

void attachChildWindowToParent (void* childHandle, void* parentHandle)
{
    if (childHandle == nullptr || parentHandle == nullptr) return;
    NSWindow* child = (__bridge NSWindow*) childHandle;
    NSWindow* parent = (__bridge NSWindow*) parentHandle;
    [parent addChildWindow:child ordered:NSWindowAbove];
}

#endif
