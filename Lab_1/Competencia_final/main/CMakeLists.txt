idf_component_register(SRCS "Competencia_final.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp-dsp
                    PRIV_REQUIRES spiffs esp_timer driver)
spiffs_create_partition_image(storage ../partition FLASH_IN_PROJECT)
