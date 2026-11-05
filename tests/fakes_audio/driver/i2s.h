#pragma once
#include <stddef.h>
#include <stdint.h>
using esp_err_t=int;
using i2s_mode_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_TIMEOUT=0x107;
constexpr int I2S_MODE_MASTER=1,I2S_MODE_TX=2,I2S_BITS_PER_SAMPLE_16BIT=16;
constexpr int I2S_CHANNEL_FMT_RIGHT_LEFT=0,I2S_COMM_FORMAT_STAND_I2S=1;
constexpr int ESP_INTR_FLAG_LEVEL1=1,I2S_NUM_0=0,I2S_PIN_NO_CHANGE=-1;
struct i2s_config_t {
    int mode=0,sample_rate=0,bits_per_sample=0,channel_format=0,communication_format=0;
    int intr_alloc_flags=0,dma_buf_count=0,dma_buf_len=0;
    bool tx_desc_auto_clear=false;
};
struct i2s_pin_config_t {int bck_io_num=0,ws_io_num=0,data_out_num=0,data_in_num=0,mck_io_num=0;};
esp_err_t i2s_driver_install(int,const i2s_config_t *,int,void *);
esp_err_t i2s_driver_uninstall(int);
esp_err_t i2s_set_pin(int,const i2s_pin_config_t *);
esp_err_t i2s_zero_dma_buffer(int);
esp_err_t i2s_write(int,const void *,size_t,size_t *,uint32_t);
