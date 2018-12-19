#include "esp_system.h"

int randombytes(void *buf, size_t n) {
        esp_fill_random(buf, n);
        return 0;
}
