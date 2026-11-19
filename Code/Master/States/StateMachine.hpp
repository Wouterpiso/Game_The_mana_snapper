#pragma once

enum class State {
    Idle,
    Display,
    Collection,
    Cam,
    AddCard,
    OcrDemo,
    CardInfo,
    CardImage,
    CardRemove,
    CardRemoveOne,
    Import,
    Export
};

void stateMachineInit();
void stateMachineRun();
