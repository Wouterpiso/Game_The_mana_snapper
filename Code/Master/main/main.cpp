/**
 * @file main.cpp
 * @brief Entry point of the Master: initialises the modules and runs the loop.
 */
#include "CamCommunication.hpp"
#include "Data.hpp"
#include "DisplayCommunication.hpp"
#include "ImageRecognition.hpp"
#include "StateMachine.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Starts the Master and handles Screen requests forever.
 */
extern "C" void app_main(void) {
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
