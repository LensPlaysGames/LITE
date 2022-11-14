#include <gfx.h>

#include <gui.h>
#include <stdlib.h>

GUIProperty *gui_properties = NULL;

GUIProperty *gui_property_by_id(int id) {
  // First attempt to update property with matching ID.
  GUIProperty *it = gui_properties;
  while (it) {
    if (id == it->id) {
      return it;
    }
    it = it->next;
  }
  return NULL;
}

void create_gui_property(int id, GUIStringProperty *property) {
  // First attempt to update property with matching ID.
  GUIProperty *it = gui_properties;
  while (it) {
    if (id == it->id) {
      it->property = *property;
      return;
    }
    it = it->next;
  }
  // If no property with this ID exists, create a new one.
  GUIProperty *properties = gui_properties;
  gui_properties = malloc(sizeof(GUIProperty));
  gui_properties->id = id;
  gui_properties->property = *property;
  gui_properties->property.next = NULL;
  gui_properties->next = properties;
}
