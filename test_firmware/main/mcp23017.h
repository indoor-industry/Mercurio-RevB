#pragma once

#include "esp_err.h"
#include "shared_types.h"

/* Bring up I2C0 and the MCP23017 GPIO expander. */
esp_err_t init_i2c(void);
esp_err_t init_mcp(void);

/* Raw register access, used by every module that reads expander pins. */
esp_err_t mcp_write(uint8_t reg, uint8_t val);
esp_err_t mcp_read(uint8_t reg, uint8_t *out);
esp_err_t mcp_read2(uint8_t *gpa, uint8_t *gpb);

void test_mcp23017(test_entry_t *t);
