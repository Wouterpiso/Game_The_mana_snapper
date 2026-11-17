#pragma once

enum class State {
    Idle,
    Display,
    Collection,
    Cam
};

void stateMachineInit();
void stateMachineRun();