#include "Platform.h"

namespace platform
{
void setKeepAwake(bool)
{
}

bool lowPowerMode()
{
    return false;
}

QString powerSource()
{
    return QStringLiteral("?");
}
}
