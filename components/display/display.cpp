#include "display.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "DISPLAY";
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t oled_dev = NULL;
static SemaphoreHandle_t oled_mutex = NULL;
static bool oled_ready = false;
static face_state_t current_face_state = FACE_IDLE;
static uint8_t face_buffer[128 * 64 / 8];

static void oled_send_cmd(uint8_t cmd) { if (!oled_dev) return; uint8_t b[2]={0x00,cmd}; i2c_master_transmit(oled_dev,b,sizeof(b),pdMS_TO_TICKS(100)); }
static void oled_clear(void) { if (!oled_dev) return; uint8_t tx[129]; tx[0]=0x40; memset(&tx[1],0,128); for(uint8_t p=0;p<8;p++){oled_send_cmd(0xB0+p);oled_send_cmd(0x00);oled_send_cmd(0x10);i2c_master_transmit(oled_dev,tx,sizeof(tx),pdMS_TO_TICKS(100));} }

void oled_init(void) {
    if(oled_ready) return;
    i2c_master_bus_config_t bus_config={}; bus_config.i2c_port=I2C_NUM_0; bus_config.sda_io_num=OLED_SDA_PIN; bus_config.scl_io_num=OLED_SCL_PIN; bus_config.clk_source=I2C_CLK_SRC_DEFAULT; bus_config.flags.enable_internal_pullup=true;
    esp_err_t err=i2c_new_master_bus(&bus_config,&i2c_bus); if(err!=ESP_OK){ESP_LOGE(TAG,"Gagal membuat I2C OLED: %s",esp_err_to_name(err));return;}
    i2c_device_config_t dev_config={}; dev_config.dev_addr_length=I2C_ADDR_BIT_LEN_7; dev_config.device_address=OLED_I2C_ADDR; dev_config.scl_speed_hz=100000;
    err=i2c_master_bus_add_device(i2c_bus,&dev_config,&oled_dev); if(err!=ESP_OK){ESP_LOGE(TAG,"Gagal menambahkan device OLED: %s",esp_err_to_name(err));oled_dev=NULL;return;}
    oled_mutex=xSemaphoreCreateMutex(); if(!oled_mutex){ESP_LOGE(TAG,"Gagal membuat mutex OLED");return;}
    vTaskDelay(pdMS_TO_TICKS(100));
    oled_send_cmd(0xAE);oled_send_cmd(0xD5);oled_send_cmd(0x80);oled_send_cmd(0xA8);oled_send_cmd(0x3F);oled_send_cmd(0xD3);oled_send_cmd(0x00);oled_send_cmd(0x40);oled_send_cmd(0x8D);oled_send_cmd(0x14);oled_send_cmd(0x20);oled_send_cmd(0x00);oled_send_cmd(0xA1);oled_send_cmd(0xC8);oled_send_cmd(0xDA);oled_send_cmd(0x12);oled_send_cmd(0x81);oled_send_cmd(0xCF);oled_send_cmd(0xD9);oled_send_cmd(0xF1);oled_send_cmd(0xDB);oled_send_cmd(0x40);oled_send_cmd(0xA4);oled_send_cmd(0xA6);oled_send_cmd(0xAF);oled_clear(); oled_ready=true;
    ESP_LOGI(TAG,"OLED SSD1306 siap: SDA=%d SCL=%d ADDR=0x%02X",OLED_SDA_PIN,OLED_SCL_PIN,OLED_I2C_ADDR);
}

void display_status(const char* text) { if(!oled_ready) oled_init(); ESP_LOGI(TAG,"[OLED STATUS]: %s",text?text:"(null)"); if(!oled_ready||!oled_mutex)return; xSemaphoreTake(oled_mutex,portMAX_DELAY); oled_clear(); xSemaphoreGive(oled_mutex); }
void face_set_state(face_state_t state){current_face_state=state;}
face_state_t face_get_state(void){return current_face_state;}

static void face_pixel(int x,int y){if(x<0||x>=128||y<0||y>=64)return;face_buffer[x+(y>>3)*128]|=(uint8_t)(1U<<(y&7));}
static void face_line(int x0,int y0,int x1,int y1){int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1,dy=y1>y0?y0-y1:y1-y0,sy=y0<y1?1:-1,err=dx+dy;for(;;){face_pixel(x0,y0);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}}
static void face_circle(int cx,int cy,int r){for(int y=-r;y<=r;y++)for(int x=-r;x<=r;x++)if(x*x+y*y<=r*r)face_pixel(cx+x,cy+y);}
static void face_ellipse(int cx,int cy,int rx,int ry){for(int y=-ry;y<=ry;y++)for(int x=-rx;x<=rx;x++)if(x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry)face_pixel(cx+x,cy+y);}

void face_render(void){if(!oled_ready)oled_init();if(!oled_ready||!oled_mutex||!oled_dev)return;memset(face_buffer,0,sizeof(face_buffer));if(current_face_state==FACE_SLEEP){face_line(24,32,48,32);face_line(80,32,104,32);}else{face_ellipse(38,30,14,20);face_ellipse(90,30,14,20);face_circle(38,30,2);face_circle(90,30,2);if(current_face_state==FACE_SPEAKING)face_ellipse(64,51,7,4);else if(current_face_state==FACE_HAPPY){face_line(53,51,59,54);face_line(59,54,69,54);face_line(69,54,75,51);}else if(current_face_state==FACE_SAD){face_line(55,55,64,51);face_line(64,51,73,55);}else if(current_face_state==FACE_ERROR){face_line(56,51,72,57);face_line(72,51,56,57);}}xSemaphoreTake(oled_mutex,portMAX_DELAY);for(uint8_t p=0;p<8;p++){uint8_t c1[2]={0,(uint8_t)(0xB0+p)},c2[2]={0,0},c3[2]={0,0x10},tx[129];tx[0]=0x40;memcpy(&tx[1],&face_buffer[p*128],128);i2c_master_transmit(oled_dev,c1,2,pdMS_TO_TICKS(100));i2c_master_transmit(oled_dev,c2,2,pdMS_TO_TICKS(100));i2c_master_transmit(oled_dev,c3,2,pdMS_TO_TICKS(100));i2c_master_transmit(oled_dev,tx,sizeof(tx),pdMS_TO_TICKS(100));}xSemaphoreGive(oled_mutex);}
