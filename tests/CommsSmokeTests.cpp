#include "comms/comms.h"

#include <cmath>

int main()
{
    const comms::cVec3 vector(3.0f, 4.0f, 0.0f);
    return std::fabs(vector.Length() - 5.0f) < 0.0001f ? 0 : 1;
}
