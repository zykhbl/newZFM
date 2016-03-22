//
//  AppDelegate.m
//  BLFM
//
//  Created by zykhbl on 13-4-8.
//  Copyright (c) 2013年 zykhbl. All rights reserved.
//

#import "AppDelegate.h"
#import "AQPlayer.h"

@implementation AppDelegate

@synthesize window;
@synthesize mainVC;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    [AQPlayer playForeground];
    
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];    
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];
    
    self.mainVC = [[AudioPlayViewController alloc] init];
    self.mainVC.view.frame = self.window.bounds;
    self.window.rootViewController = self.mainVC;
    
    return YES;
}

@end
