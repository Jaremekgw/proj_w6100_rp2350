/**
 * An Over-The-Air (OTA) software update mechanism
 *
 * ~/project/pico2/pico-examples$ vim pico_w/wifi/ota_update/README.md
 *
 */
#define TCP_EFU_SOCKET      1
#define TCP_EFU_PORT        4243    // OTA port=4242
#define EFU_BUF_SIZE        2048
