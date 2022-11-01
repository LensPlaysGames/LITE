#ifndef INTERNAL_LITE_GFX_H
#define INTERNAL_LITE_GFX_H

#include <gui.h>

typedef enum GUIPropertyID {
  GUI_PROP_ID_DEFAULT,
  GUI_PROP_ID_CURSOR,
  GUI_PROP_ID_REGION,
  /// When user creates new GUI properties, the ID should start here,
  /// so as to not stomp on built-in and required properties.
  GUI_PROP_ID_BEGIN_USER
} GUIPropertyID;

typedef struct GUIProperty {
  int id;
  GUIStringProperty property;
  struct GUIProperty *next;
} GUIProperty;

extern GUIProperty *gui_properties;

GUIProperty *gui_property_by_id(int id);

void create_gui_property(int id, GUIStringProperty *property);

#endif /* INTERNAL_LITE_GFX_H */
