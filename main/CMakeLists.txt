#idf_component_register(
#    SRCS led_strip.c
#    INCLUDE_DIRS .
#    REQUIRES driver log color esp_idf_lib_helpers
#)
set(COMPONENT_SRCDIRS ".")
set(COMPONENT_ADD_INCLUDEDIRS "include")
#set(COMPONENT_PRIV_REQUIRES driver log color esp_idf_lib_helpers)
#set(COMPONENT_REQUIRES "driver" "log" "color" "esp_idf_lib_helpers")
#set(COMPONENT_PRIV_REQUIRES  "klp"  "esp_adc" "spiffs" "nvs_flash" "gui" "lvgl" )
set(COMPONENT_EMBED_TXTFILES "${project_dir}/main/server_certs/ca_cert.pem")
register_component()
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)

#target_link_libraries(${COMPONENT_TARGET} "-u ld_include_highint_hdl_my")
