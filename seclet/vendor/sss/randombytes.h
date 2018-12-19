#ifndef sss_RANDOMBYTES_H
#define sss_RANDOMBYTES_H

#include "freertos/FreeRTOS.h"

/*
 * Write `n` bytes of high quality random bytes to `buf`
 */
int randombytes(void *buf, size_t n);


#endif /* sss_RANDOMBYTES_H */
