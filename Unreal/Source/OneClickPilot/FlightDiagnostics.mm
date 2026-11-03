// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>

#include "FlightDiagnostics.h"

bool WriteFlightDiagnostics(const char* Path, uint64_t Frames, const OCPFlightState& State)
{
    @autoreleasepool {
        NSDictionary* Snapshot = @{
            @"ready": @YES,
            @"renderer": @"unreal-engine-5",
            @"frames": @(Frames),
            @"rpm": @(State.rpm),
            @"latitude": @(State.latitude_deg),
            @"longitude": @(State.longitude_deg),
            @"altitude_ft": @(State.altitude_ft),
            @"airspeed_kt": @(State.airspeed_kts),
        };
        NSData* Data = [NSJSONSerialization dataWithJSONObject:Snapshot options:0 error:nullptr];
        NSString* Destination = [NSString stringWithUTF8String:Path];
        return [Data writeToFile:Destination options:NSDataWritingAtomic error:nullptr];
    }
}
