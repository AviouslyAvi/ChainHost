#if __APPLE__
#import <Cocoa/Cocoa.h>

extern "C" void attachChildWindowToParent (void* childHandle, void* parentHandle);

void attachChildWindowToParent (void* childHandle, void* parentHandle)
{
    if (childHandle == nullptr || parentHandle == nullptr) return;
    NSView* childView = (__bridge NSView*) childHandle;
    NSView* parentView = (__bridge NSView*) parentHandle;
    NSWindow* child = [childView window];
    NSWindow* parent = [parentView window];
    if (child == nil || parent == nil || child == parent) return;
    [parent addChildWindow:child ordered:NSWindowAbove];
}

#endif
