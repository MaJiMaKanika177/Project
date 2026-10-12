#include "i2s_out.h"
#include "pins.h"
#include <Arduino.h>

#define I2S_PORT I2S_NUM_0

bool I2SOut::init(int sample_rate, int bits) {
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = sample_rate;
    cfg.bits_per_sample = (i2s_bits_per_sample_t)bits;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 1024;
    cfg.use_apll = true;
    cfg.tx_desc_auto_clear = true;

    if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) return false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = PIN_I2S_BCK;
    pins.ws_io_num = PIN_I2S_WS;
    pins.data_out_num = PIN_I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) return false;

    Serial.println("[I2S] Initialized");
    return true;
}

void I2SOut::write(const int16_t* samples, size_t count) {
    size_t written;
    i2s_write(I2S_PORT, samples, count * sizeof(int16_t), &written, portMAX_DELAY);
}

void I2SOut::stop() {
    i2s_driver_uninstall(I2S_PORT);
}