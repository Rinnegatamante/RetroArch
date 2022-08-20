#ifndef VITA_COMMON_H__
#define VITA_COMMON_H__

#include <vitaGL.h>

#define ATTR_VITA_WIDTH 960
#define ATTR_VITA_HEIGHT 544

typedef struct
{
   int native_window;
   bool resize;
   unsigned width, height;
   float refresh_rate;
} vita_ctx_data_t;

#endif
