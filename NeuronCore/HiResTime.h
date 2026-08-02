#pragma once

void InitialiseHighResTime();
double GetHighResTime();        // Return value in seconds

void SetFakeTimeMode();
void SetRealTimeMode();
void IncrementFakeTime(double _increment);


