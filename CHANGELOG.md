# Changelog

## 1.0.0 (2026-10-16)

First release.

- `state_machine`: table-driven and non-blocking, with guards, actions and per-state timeouts
- `observable`: change notification with old and new values, optional mutex, locked read-modify-write
- `weekly_schedule`: time windows per weekday, overnight windows, merging, fixed storage
- `json_schema`: declarative ArduinoJson 7 validation with error paths and strict mode
- `debouncer` and `periodic`
- Host unit tests with sanitizers, and CI builds of two ESP32 examples
