#include <stdlib.h>
#include <stdint.h>
  uint16_t
fletcher16(const uint8_t *data, size_t len);

uint32_t
fletcher32(const uint16_t *data, size_t len);
