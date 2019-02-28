// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#import <Cocoa/Cocoa.h>

@interface AssertDialogController : NSWindowController
{
    IBOutlet NSTextField *tfCondition;
    IBOutlet NSTextField *tfLine;
    IBOutlet NSTextField *tfFile;
    IBOutlet NSTextField *tfReason;
    
    
}

@property (assign, nonatomic) EDialogAction dialogAction;
// Assert settings


-(id)initWithDataAndNib:(NSString*) czCondition file:(NSString*) czFile
                   line:(int) nLine reason:(NSString*) czReason nib:(NSString*) nib;

// Button callbacks
-(IBAction)onContinue:(id)sender;
-(IBAction)onIgnore:(id)sender;
-(IBAction)onIgnoreAll:(id)sender;
-(IBAction)onBreak:(id)sender;
-(IBAction)onStop:(id)sender;
-(IBAction)onReportAsBug:(id)sender;
@end
