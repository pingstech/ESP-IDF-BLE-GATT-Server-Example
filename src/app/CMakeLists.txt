set(sources
    src/app_manager.c
    src/ble_app.c
)

idf_component_register(
    SRCS ${sources}
    INCLUDE_DIRS . inc
    REQUIRES freertos
            esp_system
            esp_netif
            esp_event
            nvs_flash
            json
            bt
            esp_phy
            esp_coex
)