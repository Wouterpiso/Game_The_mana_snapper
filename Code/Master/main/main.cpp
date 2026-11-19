#include "CamCommunication.hpp"
#include "Data.hpp"
#include "DisplayCommunication.hpp"
#include "ImageReconition.hpp"
#include "StateMachine.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    displayCommunicationInit();
    camCommunicationInit();
    imageRecognitionInit();
    dataInitializeDefault();
    stateMachineInit();

    while (true) {
        stateMachineRun();
        vTaskDelay(1);
    }
}
