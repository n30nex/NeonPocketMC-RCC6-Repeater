#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <helpers/CommonCLI.h>

class MyMesh;

class UITask {
  DisplayDriver* _display;
  MyMesh* _mesh;
  unsigned long _next_read, _next_refresh, _auto_off, _started_at;
  int _prevBtnState;
  bool _battery_low;
  NodePrefs* _node_prefs;
  char _version_info[12], _build_info[16];

  void renderCurrScreen();
  void renderCard(int x, int width, const char* label, const char* value, ColorVal value_color);

public:
  explicit UITask(DisplayDriver& display) :
      _display(&display), _mesh(nullptr), _next_read(0), _next_refresh(0),
      _auto_off(0), _started_at(0), _prevBtnState(HIGH), _battery_low(false),
      _node_prefs(nullptr) { }

  void begin(MyMesh* mesh, NodePrefs* node_prefs,
             const char* build_date, const char* firmware_version);
  void loop();
};
