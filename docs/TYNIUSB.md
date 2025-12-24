为了支持混合框架的Tinyusb，需要下面几步：

1. 在框架文件夹的Tinyusb的目录中修改Kconfig（引用/tinyusb.Kconfig)来增加menuconfig中的需要的编译参数配置，必须如此，而不能后续来改，因为会被工具自动覆盖；
2. 当前Arduino版本集成的tinyusb库有一个拼写bug错误，需要手动更改，到了出错，记得修改；
3. 为了让arduino可以正确被espif引用，需要申明下component, 在 `.platformio\packages\framework-arduinoespressif32\CMakeLists.txt` 中添加tinyusb，如下例：
    > set(requires spi_flash mbedtls mdns esp_adc_cal wifi_provisioning nghttp wpa_supplicant **tinyusb**)    
    > set(priv_requires fatfs nvs_flash app_update spiffs bootloader_support openssl bt esp_ipc esp_hid **tinyusb**)