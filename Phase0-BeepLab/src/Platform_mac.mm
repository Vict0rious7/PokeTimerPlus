#include "Platform.h"

#import <Foundation/Foundation.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

namespace
{
id<NSObject> activity = nil;
}

namespace platform
{
void setKeepAwake(bool on)
{
    if (on && activity == nil)
    {
        activity = [[NSProcessInfo processInfo]
            beginActivityWithOptions:(NSActivityUserInitiated | NSActivityLatencyCritical)
                              reason:@"Timing beeps"];
        [activity retain];
    }
    else if (!on && activity != nil)
    {
        [[NSProcessInfo processInfo] endActivity:activity];
        [activity release];
        activity = nil;
    }
}

bool lowPowerMode()
{
    if (@available(macOS 12.0, *))
    {
        return [[NSProcessInfo processInfo] isLowPowerModeEnabled];
    }
    return false;
}

QString powerSource()
{
    CFStringRef type = IOPSGetProvidingPowerSourceType(nullptr);
    if (!type)
    {
        return QStringLiteral("?");
    }
    if (CFStringCompare(type, CFSTR(kIOPMACPowerKey), 0) == kCFCompareEqualTo)
    {
        return QStringLiteral("AC");
    }
    if (CFStringCompare(type, CFSTR(kIOPMBatteryPowerKey), 0) == kCFCompareEqualTo)
    {
        return QStringLiteral("Battery");
    }
    return QStringLiteral("?");
}
}
