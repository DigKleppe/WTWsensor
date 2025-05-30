# _ESP32 Template wifi project_

project contains all needed for a wifi project
tested with idf 5.4.1

To connect with your wifi:

Enable in menu "Connection information"  WPS or Smartconfig

in main:

	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
		ESP_LOGI(TAG, "nvs flash erased");
	}
	ESP_ERROR_CHECK(err);

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(init_spiffs());
    
    loadSettings(); // if settings are not found defau
	wifiConnect();


The wifi is started. If you provided the correct SSID and Pasword the connection is made.
When this is not the case WPS or smartconfig is started. 
If success the new SSID and Pasword is saved into the wifiSettings.
else the process is repeated continuous. 

A webserver is started 



