
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "updateTask.h"

#include "autoCalTask.h"
#include "sensirionTask.h"
#include "settings.h"
#include "wifiConnect.h"
#include "clockTask.h"

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <led_strip.h>
#include <stdio.h>

#define SDA_PIN 21 // 1			//21
#define SCL_PIN 22 // 2 		// 22
#define I2C_CLK 50000

#define I2C_MASTER_SCL_IO SCL_PIN	/*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO SDA_PIN	/*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0	/*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ I2C_CLK	/*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

#define LED_TYPE LED_STRIP_WS2812
#define LED_GPIO GPIO_NUM_4 //  GPIO_NUM_48
#define CONFIG_LED_STRIP_LEN 1   

extern const char server_root_cert_pem_start[] asm("_binary_ca_cert_pem_start");  // dummy, to pull in for linker
const char * dummy;
int moduleNr;

const char firmWareVersion[] = { "0.91"} ; // just for info , set this in firmWareVersion.txt for update

const char * getFirmWareVersion () {
	return firmWareVersion;
}
esp_err_t init_spiffs(void);

uint32_t timeStamp = 1; // global timestamp for logging
/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void) {
	i2c_port_t i2c_master_port = I2C_MASTER_NUM;

	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_MASTER_SDA_IO,
		.scl_io_num = I2C_MASTER_SCL_IO,
		.sda_pullup_en = GPIO_PULLUP_DISABLE, // no pullups, externally switched
		.scl_pullup_en = GPIO_PULLUP_DISABLE,
		.master = I2C_MASTER_FREQ_HZ,
		.clk_flags = 0,
	};

	i2c_param_config(i2c_master_port, &conf);
	return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

static const char *TAG = "main";

static const rgb_t colors[] = {
	{.r = 0x0f, .g = 0x0f, .b = 0x0f}, // wit
	{.r = 0x00, .g = 0x00, .b = 0x2f}, // blauw
	{.r = 0x00, .g = 0x2f, .b = 0x00}, // groen
	{.r = 0x2f, .g = 0x00, .b = 0x00}, // rood
	{.r = 0x2f, .g = 0x2f, .b = 0x00}, // geel
	{.r = 0x2f, .g = 0x00, .b = 0x2f}, // paars
	{.r = 0x00, .g = 0x2f, .b = 0x2f}, // cyaan
	{.r = 0x00, .g = 0x00, .b = 0x00}, // uit

};

#define COLORS_TOTAL (sizeof(colors) / sizeof(rgb_t))

void LEDtask(void *pvParameters) {
	bool flash = false;

	led_strip_t strip = {
		.type = LED_TYPE,
		.is_rgbw = false,
#ifdef LED_STRIP_BRIGHTNESS
		.brightness = 55,
#endif
		.length = CONFIG_LED_STRIP_LEN,
		.gpio = LED_GPIO,
		.channel = RMT_CHANNEL_0,
		.buf = NULL,
	};

	ESP_ERROR_CHECK(led_strip_init(&strip));

	int c = 0;
	while (1) {
		if (sensirionError) {
			c = 3; // rood
			flash = true;
		} else {
			switch ((int)connectStatus) {
			case CONNECTING:
				ESP_LOGI(TAG, "CONNECTING");
				c = 4; 
				break;

			case WPS_ACTIVE:
				ESP_LOGI(TAG, "WPS_ACTIVE");
				flash = true;
				c = 1; // blauw   
				break;

			case IP_RECEIVED:
				//     ESP_LOGI(TAG, "IP_RECEIVED");
				flash = false;
				c = 2; // groen
				break;

			case CONNECTED:
				ESP_LOGI(TAG, "CONNECTED");
				c = 4; // geel
				break;

			default:
				ESP_LOGI(TAG, "default");
				break;
			}
		}
		led_strip_fill(&strip, 0, strip.length, colors[c]);
		led_strip_flush(&strip);

		if ( flash) {
			vTaskDelay (pdMS_TO_TICKS(300));
			led_strip_fill(&strip, 0, strip.length, colors[7]);
			led_strip_flush(&strip);
			vTaskDelay (pdMS_TO_TICKS(300));
		}
		else
			vTaskDelay(pdMS_TO_TICKS(1000));

		// if (++c >= COLORS_TOTAL)
		//     c = 0;
	}
}

extern "C" void app_main() {
	time_t now = 0;
	esp_err_t err;
	struct tm timeinfo;
	int lastSecond = -1;
// jumpers
	ESP_ERROR_CHECK( gpio_input_enable(GPIO_NUM_25)); 
	ESP_ERROR_CHECK(gpio_input_enable(GPIO_NUM_26));
	ESP_ERROR_CHECK(gpio_input_enable(GPIO_NUM_27)); 
	ESP_ERROR_CHECK(gpio_pullup_en(GPIO_NUM_25));
	ESP_ERROR_CHECK(gpio_pullup_en(GPIO_NUM_26));
	ESP_ERROR_CHECK(gpio_pullup_en(GPIO_NUM_27));

	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
		ESP_LOGI(TAG, "nvs flash erased");
	}
	ESP_ERROR_CHECK(err);

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	err = init_spiffs();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(err));
		return;
	}

	err = loadSettings();
	vTaskDelay(10);
	int n = 1;
	if (gpio_get_level (GPIO_NUM_25) == 0) 
		n+= 1;
	if (gpio_get_level (GPIO_NUM_26)== 0) 
		n+= 2;
	
	sprintf (userSettings.moduleName, "S%d", n );  // make modulenname "S1 .. S4"
	ESP_LOGI(TAG, "moduleName:%s" , userSettings.moduleName);
	moduleNr = n; 
	
	wifiConnect();

	i2c_master_init();
	xTaskCreate(sensirionTask, "sensirionTask", 4 * 1024, (void *)I2C_MASTER_NUM, 0, NULL);

	led_strip_install();
	xTaskCreate(LEDtask, "LEDtask", configMINIMAL_STACK_SIZE * 5, NULL, 5, NULL);

	xTaskCreate(&updateTask, "updateTask",2* 8192, NULL, 5, NULL);

	xTaskCreate(&autoCalTask, "autoCalTask",8192, NULL, 5, NULL);

	do {
		vTaskDelay(100);
	} while (connectStatus != IP_RECEIVED);
	
	xTaskCreate(clockTask, "clock", 4 * 1024, NULL, 0, NULL);

	while (1) {
		//	int rssi = getRssi();
		//	ESP_LOGI(TAG, "RSSI: %d", rssi);
		vTaskDelay(pdMS_TO_TICKS(200)); //
		time(&now);
		localtime_r(&now, &timeinfo);
		if (lastSecond != timeinfo.tm_sec) {
			lastSecond = timeinfo.tm_sec; // every second
			timeStamp++;
			if ( timeStamp == 0)
				timeStamp++;
		}
	}
}