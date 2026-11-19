/**
 * @file StateMachine.hpp
 * @brief Request handling loop of the Master.
 */
#pragma once

/**
 * @brief State used while a Screen request is being handled.
 */
enum class State {
  Idle,          /**< No request is being handled. */
  Display,       /**< Reserved menu command. */
  Collection,    /**< Sending the collection list. */
  Cam,           /**< Camera capture is active. */
  AddCard,       /**< Looking up and adding a card. */
  OcrDemo,       /**< Running the OCR demo. */
  CardInfo,      /**< Sending card details. */
  CardImage,     /**< Sending a card image. */
  CardRemove,    /**< Removing every copy of a card. */
  CardRemoveOne, /**< Removing one copy of a card. */
  Import,        /**< Importing the collection. */
  Export         /**< Exporting the collection. */
};

/**
 * @brief Puts the state machine in its initial state.
 */
void stateMachineInit();

/**
 * @brief Reads and handles waiting Screen requests.
 *
 * Call this repeatedly from the main loop; it does not block on input.
 */
void stateMachineRun();
