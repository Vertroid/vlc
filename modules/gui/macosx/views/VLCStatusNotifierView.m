/*****************************************************************************
 * VLCStatusNotifierView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCStatusNotifierView.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryModel.h"

@interface VLCStatusNotifierView ()

@property NSUInteger remainingCount;
@property (nonatomic) BOOL permanentDiscoveryMessageActive;

@end

@implementation VLCStatusNotifierView

- (void)awakeFromNib
{
    self.remainingCount = 0;
    self.permanentDiscoveryMessageActive = NO;
    self.label.stringValue = _NS("Idle");

    NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
    [defaultCenter addObserver:self selector:@selector(updateStatus:) name:nil object:nil];
}

- (void)setPermanentDiscoveryMessageActive:(BOOL)permanentDiscoveryMessageActive
{
    if (_permanentDiscoveryMessageActive == permanentDiscoveryMessageActive) {
        return;
    }

    _permanentDiscoveryMessageActive = permanentDiscoveryMessageActive;
    if (permanentDiscoveryMessageActive) {
        self.remainingCount++;
        [self.progressIndicator startAnimation:self];
    } else {
        self.remainingCount--;
        [self.progressIndicator stopAnimation:self];
    }
}

- (void)updateStatus:(NSNotification *)notification
{
    if (![notification.name hasPrefix:@"VLC"]) {
        return;
    }

    if ([notification.name isEqualToString:VLCLibraryModelDiscoveryStarted]) {
        [self presentTransientMessage:_NS("Discovering media…")];
    } else if ([notification.name isEqualToString:VLCLibraryModelDiscoveryProgress]) {
        self.label.stringValue = _NS("Discovering media…");
        self.permanentDiscoveryMessageActive = YES;
    } else if ([notification.name isEqualToString:VLCLibraryModelDiscoveryCompleted]) {
        self.label.stringValue = _NS("Media discovery completed");
        self.permanentDiscoveryMessageActive = NO;
    } else if ([notification.name isEqualToString:VLCLibraryModelDiscoveryFailed]) {
        self.label.stringValue = _NS("Media discovery failed");
        self.permanentDiscoveryMessageActive = NO;
    }
}

- (void)presentTransientMessage:(NSString *)message
{
    self.label.stringValue = message;
    [self performSelector:@selector(clearTransientMessage) withObject:nil afterDelay:2.0];
    self.remainingCount++;
}

- (void)clearTransientMessage
{
    self.remainingCount--;
    if (self.remainingCount > 0) {
        return;
    }
    self.label.stringValue = _NS("Idle");
}

@end