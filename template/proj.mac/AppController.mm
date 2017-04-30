/****************************************************************************
 Copyright (c) 2010 cocos2d-x.org

 http://www.cocos2d-x.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#import "AppController.h"

#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <vector>

#include "AppDelegate.h"
#include "engine/CCDirector.h"
#include "engine/CCFileSystem.h"
#include "audio/SimpleAudioEngine.h"
#include "base/ProcessUtils.h"

#include "imgui/imgui_cocos2dx.h"
#include "core/platform/CCInput.h"

using namespace std;
using namespace cocos2d;

@implementation AppController

@synthesize menu;

-(void) dealloc
{
    CCDirector::sharedDirector()->end();
    [super dealloc];
}

#pragma mark -
#pragma delegates

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification
{
    isAlwaysOnTop = NO;

    [self parseCommandLineArgs];
    [self createWindowAndGLView];
    [self startup];

    [window orderFrontRegardless];
    [self windowDidLoad];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)theApplication
{
    return YES;
}

- (BOOL) applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    return NO;
}

- (void) windowDidLoad {
    NSEvent* (^handler)(NSEvent*) = ^(NSEvent *theEvent) {
        NSWindow *targetWindow = theEvent.window;
        if (targetWindow != window) {
            return theEvent;
        }

        NSEvent *result = theEvent;
        //NSLog(@"event monitor: %@", theEvent);
        int keyCode = theEvent.keyCode;
        if (keyCode == 96) { // F5
            [self relaunch];
        } else if (keyCode == 97) { // F6
            cocos2d::ImGuiCC::toggleVisible();
        } else if (keyCode == 122) { // F1
            cocos2d::CCDirector::sharedDirector()->getSubSystem<cocos2d::Input>()->onKeypadBack();
        } else if (keyCode == 120) { // F2
            cocos2d::CCDirector::sharedDirector()->getSubSystem<cocos2d::Input>()->onKeypadMenu();
        } else if (keyCode == 53) { // ESCAPE
            cocos2d::CCDirector::sharedDirector()->getSubSystem<cocos2d::Input>()->onKeypadBack();
        }

        return result;
    };
    eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask
                                                         handler:handler];
}

- (void) windowWillClose:(NSNotification *)notification
{
    [NSEvent removeMonitor:eventMonitor];
    eventMonitor = nil;
    [self saveLastState];
    CCDirector::sharedDirector()->end();
    [[NSApplication sharedApplication] terminate:self];
}

- (void) openConsoleWindow
{
    if (!consoleController)
    {
        consoleController = [[ConsoleWindowController alloc] initWithWindowNibName:@"ConsoleWindow"];
    }
    [consoleController.window orderFrontRegardless];

    NSDictionary *state = [[NSUserDefaults standardUserDefaults] objectForKey:@"last-state"];
    if (state)
    {
        NSNumber *x = [state objectForKey:@"console_x"];
        NSNumber *y = [state objectForKey:@"console_y"];
        if (x && y)
        {
            int posx = [x intValue];
            int posy = [y intValue];
            [consoleController.window setFrameOrigin:NSMakePoint(posx, posy)];
        }
    }

    //set console pipe
    pipe = [NSPipe pipe] ;
    pipeReadHandle = [pipe fileHandleForReading] ;

    int outfd = [[pipe fileHandleForWriting] fileDescriptor];
    if (dup2(outfd, fileno(stderr)) != fileno(stderr) || dup2(outfd, fileno(stdout)) != fileno(stdout))
    {
        perror("Unable to redirect output");
        [self showAlert:@"Unable to redirect output to console!" withTitle:@"quick player error"];
    }
    else
    {
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(handleNotification:) name: NSFileHandleReadCompletionNotification object: pipeReadHandle] ;
        [pipeReadHandle readInBackgroundAndNotify] ;
    }
}

#pragma mark -
#pragma mark functions

- (void) parseCommandLineArgs
{
    NSArray *nsargs = [[NSProcessInfo processInfo] arguments];
    std::string cmdLine([[nsargs firstObject] cStringUsingEncoding:NSUTF8StringEncoding]);
    for (int i = 1; i < [nsargs count]; ++i)
    {
        cmdLine += " \"";
        cmdLine += [[nsargs objectAtIndex:i] cStringUsingEncoding:NSUTF8StringEncoding];
        cmdLine += "\"";
    }
    parseArguments(cmdLine);
}

- (void) createWindowAndGLView
{
    int w = getIntArgument("w");
    int h = getIntArgument("h");
    const CCSize frameSize(w, h);
    float left = 10;
    float bottom = NSHeight([[NSScreen mainScreen] visibleFrame]) - frameSize.height;
    bottom -= [[[NSApplication sharedApplication] menu] menuBarHeight] + 10;

    int posx = 0, posy = 0;
    NSDictionary *state = [[NSUserDefaults standardUserDefaults] objectForKey:@"last-state"];
    if (state)
    {
        NSNumber *x = [state objectForKey:@"x"];
        NSNumber *y = [state objectForKey:@"y"];
        if (x && y)
        {
            posx = [x intValue];
            posy = [y intValue];
        }
    }

    // create the window
    // note that using NSResizableWindowMask causes the window to be a little
    // smaller and therefore ipad graphics are not loaded
    NSRect rect = NSMakeRect(left, bottom, frameSize.width, frameSize.height);
    NSInteger mask = NSClosableWindowMask | NSTitledWindowMask | NSMiniaturizableWindowMask;
    window = [[NSWindow alloc] initWithContentRect:rect
                                         styleMask:mask
                                           backing:NSBackingStoreBuffered
                                             defer:YES];
    window.delegate = self;

    // allocate our GL view
    // (isn't there already a shared EAGLView?)
    glView = [[EAGLView alloc] initWithFrame:rect];

    // set window parameters
    [window setContentView:glView];
    [window setTitle:@"QuickXPlayer"];
    [window center];

    [self setZoom:getFloatArgument("zoom")];
    if (posx != 0 && posy != 0)
    {
        [window setFrameOrigin:NSMakePoint(posx, posy)];
    }

    [window becomeFirstResponder];
    [window makeKeyAndOrderFront:self];
    [window setAcceptsMouseMovedEvents:NO];
}

- (void) startup
{
    std::string workdir = getArgument("workdir");
    if (workdir.empty())
    {
        workdir = [[NSBundle mainBundle] resourcePath].UTF8String;
    }
    else
    {
        workdir = [[[NSString stringWithCString:workdir.c_str() encoding:NSUTF8StringEncoding] stringByExpandingTildeInPath] stringByStandardizingPath].UTF8String;
    }

    [self openConsoleWindow];

    FileSystem::setResourceRoot(workdir);

    AppDelegate* app = new AppDelegate();
    app->run();
}

- (void) launch:(NSArray*)args
{
    NSURL *url = [NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
    NSMutableDictionary *configuration = [NSMutableDictionary dictionaryWithObject:args
                                                                            forKey:NSWorkspaceLaunchConfigurationArguments];
    NSError *error = [[[NSError alloc] init] autorelease];
    [[NSWorkspace sharedWorkspace] launchApplicationAtURL:url
                                                  options:NSWorkspaceLaunchNewInstance
                                            configuration:configuration error:&error];
}

- (void) relaunch:(NSArray*)args
{
    [self saveLastState];
    [self launch:args];
    [[NSApplication sharedApplication] terminate:self];

}

- (void) relaunch
{
    [self relaunch:[NSArray array]];
}

- (void) showAlertWithoutSheet:(NSString*)message withTitle:(NSString*)title
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert addButtonWithTitle:@"OK"];
    [alert setMessageText:message];
    [alert setInformativeText:title];
    [alert setAlertStyle:NSWarningAlertStyle];
    [alert runModal];
}

- (void) setZoom:(float)scale
{
    [glView setFrameZoomFactor:scale];
}

-(void) setAlwaysOnTop:(BOOL)alwaysOnTop
{
    NSMenuItem *windowMenu = [[window menu] itemWithTitle:@"Window"];
    NSMenuItem *menuItem = [[windowMenu submenu] itemWithTitle:@"Always On Top"];
    if (alwaysOnTop)
    {
        [window setLevel:NSFloatingWindowLevel];
        [menuItem setState:NSOnState];
    }
    else
    {
        [window setLevel:NSNormalWindowLevel];
        [menuItem setState:NSOffState];
    }
    isAlwaysOnTop = alwaysOnTop;
}

-(void) saveLastState
{
    NSMutableDictionary *state = [NSMutableDictionary dictionary];
    [state setObject:[NSNumber numberWithInt:window.frame.origin.x] forKey:@"x"];
    [state setObject:[NSNumber numberWithInt:window.frame.origin.y] forKey:@"y"];
    if (consoleController)
    {
        [state setObject:[NSNumber numberWithInt:consoleController.window.frame.origin.x] forKey:@"console_x"];
        [state setObject:[NSNumber numberWithInt:consoleController.window.frame.origin.y] forKey:@"console_y"];
    }
    [[NSUserDefaults standardUserDefaults] setObject:state forKey:@"last-state"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)handleNotification:(NSNotification *)note
{
    //NSLog(@"Received notification: %@", note);
    [pipeReadHandle readInBackgroundAndNotify] ;
    NSData *data = [[note userInfo] objectForKey:NSFileHandleNotificationDataItem];
    NSString *str = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
    //show log to console
    [consoleController trace:str];
}

#pragma mark -
#pragma mark IB Actions

- (IBAction) onFileRelaunch:(id)sender
{
    [self relaunch];
}

- (IBAction) onScreenChangeFrameSize:(id)sender
{
}

- (IBAction) onScreenPortait:(id)sender
{
    if ([sender state] == NSOnState) return;
    [sender setState:NSOnState];
    [[[[[window menu] itemWithTitle:@"Screen"] submenu] itemWithTitle:@"Landscape"] setState:NSOffState];
    [self relaunch];
}

- (IBAction) onScreenLandscape:(id)sender
{
    if ([sender state] == NSOnState) return;
    [sender setState:NSOnState];
    [[[[[window menu] itemWithTitle:@"Screen"] submenu] itemWithTitle:@"Portait"] setState:NSOffState];
    [self relaunch];
}

- (IBAction) onScreenZoomOut:(id)sender
{
    if ([sender state] == NSOnState) return;
    float scale = (float)[sender tag] / 100.0f;
    [self setZoom:scale];
}

-(IBAction) onWindowAlwaysOnTop:(id)sender
{
    [self setAlwaysOnTop:!isAlwaysOnTop];
}

-(void)keyDown:(NSEvent*) event
{
    printf("PRESS\n");
}
@end
