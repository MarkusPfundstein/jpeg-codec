#ifndef __HELPER_H__
#define __HELPER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


	extern char *int2bin(int n, char *buf);
	extern char *uint2bin(uint8_t n, char *buf);

#ifdef __cplusplus
}
#endif

#endif