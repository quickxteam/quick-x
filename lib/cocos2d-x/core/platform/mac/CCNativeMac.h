
#if CC_LUA_ENGINE_ENABLED > 0
#include "CCLuaEngine.h"
#endif

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

USING_NS_CC;

@interface CCNativeMac : NSObject
{
    NSAlert *alertView_;
}

+ (CCNativeMac *)sharedInstance;


#pragma mark -
#pragma mark activity indicator

- (void)showActivityIndicator;
- (void)hideActivityIndicator;


#pragma mark -
#pragma mark alert view

- (void)createAlertView:(NSString *)title
             andMessage:(NSString *)message
   andCancelButtonTitle:(NSString *)cancelButtonTitle;
- (NSInteger)addAlertButton:(NSString *)buttonTitle;
- (void)showAlertView;
- (void)removeAlertView;
- (void)cancelAlertView;

#if CC_LUA_ENGINE_ENABLED > 0
- (void)showAlertViewWithLuaListener:(LUA_FUNCTION)listener;
- (void)removeAlertViewLuaListener;
#endif


#pragma mark -
#pragma mark misc

- (NSString*)getInputText:(NSString*)title message:(NSString*)message defaultValue:(NSString*)defaultValue;

@end