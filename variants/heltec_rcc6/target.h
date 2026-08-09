#pragma once

#define RADIOLIB_STATIC_ONLY 1

#include "heltec_rcc6.h"

#include <RadioLib.h>
#include <helpers/SensorManager.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/radiolib/RadioLibWrappers.h>

extern HeltecRCC6Board board;
extern WRAPPER_CLASS radio_driver;
extern ESP32RTCClock rtc_clock;
extern SensorManager sensors;

bool radio_init();
mesh::LocalIdentity radio_new_identity();
