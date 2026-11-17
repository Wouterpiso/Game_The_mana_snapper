idf_component_register(
    SRCS
        "main.cpp"
        "../States/StateMachine.cpp"
        "../collection/Data.cpp"
        "../display/DisplayCommunication.cpp"
        "../image_reconition/CamCommunication.cpp"
    INCLUDE_DIRS
        "."
        "../States"
        "../collection"
        "../display"
        "../image_reconition"
    REQUIRES driver json
)