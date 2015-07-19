#ifndef __TIFF_H__
#define __TIFF_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
	int little_endian;
	int width;
	int height;
	short n_components;
	short bps[3];
} tiff_image_t;

extern int read_tiff(const char *filepath, short **buf, tiff_image_t *image);

#ifdef __cplusplus
}
#endif

#endif