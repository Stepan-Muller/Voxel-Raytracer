#include <math.h>

#define PI 3.1415926535

/* Zastavi radiany na 2PI a 0 */
static float capRad(float a)
{
    if (a > 2 * PI)
        a -= 2 * PI;
    else if (a < 0)
        a += 2 * PI;

    return a;
}