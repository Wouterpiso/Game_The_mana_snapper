#include "CamCommunication.hpp"
#include "Data.hpp"
#include "DisplayCommunication.hpp"
#include "StateMachine.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

extern "C" void app_main(void)
{
    displayCommunicationInit();
    camCommunicationInit();
    const DataResult dataResult = dataInitializeDefault();
    if (dataResult != DataResult::Ok) {
        printf("Failed to initialize collection data\n");
    }
    stateMachineInit();
    printf("Master ready; display UART RX=IO16, TX=IO17\n");

    while (true) {
        stateMachineRun();
        vTaskDelay(1);
    }
}