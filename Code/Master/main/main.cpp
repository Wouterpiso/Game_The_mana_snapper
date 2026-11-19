#include "CamCommunication.hpp"
#include "Data.hpp"
#include "DisplayCommunication.hpp"
#include "ImageReconition.hpp"
#include "StateMachine.hpp"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

extern "C" void app_main(void)
{
    displayCommunicationInit();
    camCommunicationInit();
    imageRecognitionInit();
    const DataResult dataResult = dataInitializeDefault();
    if (dataResult != DataResult::Ok) {
        printf("Failed to initialize collection data\n");
    }
    stateMachineInit();
    printf("Master ready; display UART RX=IO16, TX=IO17; heap free %u, largest %u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    while (true) {
        stateMachineRun();
        vTaskDelay(1);
    }
}