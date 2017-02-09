#include <stdint.h>
#include <stdlib.h>


#include "mongoose/mongoose.h"
#include "esp_system.h"

int mg_ssl_if_mbed_random(void *ctx, unsigned char *buf, size_t len)
{
    while (len > 0) {
      uint32_t r = esp_random(); /* Uses hardware RNG. */
      for (int i = 0; i < 4 && len > 0; i++, len--) {
        *buf++ = (uint8_t) r;
        r >>= 8;
      }
    }
    (void) ctx;
    return 0;
}
