idf_component_register(
    SRCS
        "main.cpp"
        "../States/StateMachine.cpp"
        "../collection/Data.cpp"
        "../display/DisplayCommunication.cpp"
        "../image_reconition/CamCommunication.cpp"
        "../image_reconition/ImageReconition.cpp"
    INCLUDE_DIRS
        "."
        "../States"
        "../collection"
        "../display"
        "../image_reconition"
    REQUIRES driver json esp_http_client esp_wifi esp_netif nvs_flash esp_event cjson
)