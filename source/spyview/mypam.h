#ifndef __mypam_h__
#define __mypam_h__

extern "C"  {
#include <pam.h>
#include <pm.h>
  /* Stupid pam/pgm/pm defines these crappy macros */
  /* They cause severe namespace conflicts with algorithm */

#undef min
#undef max
#undef abs
#undef odd
}

#endif
