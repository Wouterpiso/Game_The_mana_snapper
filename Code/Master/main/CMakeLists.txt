idf_component_register(
    SRCS
        "main.cpp"
        "../States/StateMachine.cpp"
        "../API/API.cpp"
        "../collection/Data.cpp"
        "../display/DisplayCommunication.cpp"
        "../image_reconition/CamCommunication.cpp"
        "../image_reconition/ImageReconition.cpp"
    INCLUDE_DIRS
        "."
        "../States"
        "../API"
        "../collection"
        "../display"
        "../image_reconition"
    REQUIRES driver json esp_http_client esp_wifi esp_netif nvs_flash esp_event esp-tls mbedtls spiffs
)

# The OCR demo cards (test1.jpeg .. test9.jpeg) are packed into the storage
# partition instead of the app, which is nearly full. Flashing the storage
# partition replaces the saved collection.
spiffs_create_partition_image(storage ../test_Image FLASH_IN_PROJECT)
