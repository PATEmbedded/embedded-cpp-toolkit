// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Pourya Afshintabar
//
// Single include for the toolkit. The JSON schema module is included when
// ArduinoJson is available.

#pragma once

#include "ect/debouncer.hpp"
#include "ect/observable.hpp"
#include "ect/periodic.hpp"
#include "ect/state_machine.hpp"
#include "ect/weekly_schedule.hpp"

#if __has_include(<ArduinoJson.h>)
#include "ect/json_schema.hpp"
#endif
