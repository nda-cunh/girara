/* See LICENSE file for license and copyright information */

#include "shortcuts.h"
#include "datastructures.h"
#include "internal.h"
#include "session.h"
#include "settings.h"
#include "tabs.h"

#include <string.h>
#include <gtk/gtk.h>

static void girara_toggle_widget_visibility(GtkWidget* widget);

bool
girara_shortcut_add(girara_session_t* session, guint modifier, guint key, const char* buffer, girara_shortcut_function_t function, girara_mode_t mode, int argument_n, void* argument_data)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(buffer || key || modifier, false);
  g_return_val_if_fail(function != NULL, false);

  girara_argument_t argument = {argument_n, (argument_data != NULL) ?
    g_strdup(argument_data) : NULL};

  /* search for existing binding */
  bool found_existing_shortcut = false;
  GIRARA_LIST_FOREACH(session->bindings.shortcuts, girara_shortcut_t*, iter, shortcuts_it)
    if (((shortcuts_it->mask == modifier && shortcuts_it->key == key && (modifier != 0 || key != 0)) ||
       (buffer && shortcuts_it->buffered_command && !strcmp(shortcuts_it->buffered_command, buffer)))
        && ((shortcuts_it->mode == mode) || (mode == 0)))
    {
      shortcuts_it->function  = function;
      shortcuts_it->argument  = argument;
      found_existing_shortcut = true;

      if (mode != 0) {
        girara_list_iterator_free(iter);
        return true;
      }
    }
  GIRARA_LIST_FOREACH_END(session->bindings.shortcuts, girara_shortcut_t*, iter, shortcuts_it);

  if (found_existing_shortcut == true) {
    return true;
  }

  /* add new shortcut */
  girara_shortcut_t* shortcut = g_slice_new(girara_shortcut_t);

  shortcut->mask             = modifier;
  shortcut->key              = key;
  shortcut->buffered_command = buffer;
  shortcut->function         = function;
  shortcut->mode             = mode;
  shortcut->argument         = argument;
  girara_list_append(session->bindings.shortcuts, shortcut);

  return true;
}

bool
girara_shortcut_remove(girara_session_t* session, guint modifier, guint key, const char* buffer, girara_mode_t mode)
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(buffer || key || modifier, false);

  /* search for existing binding */
  GIRARA_LIST_FOREACH(session->bindings.shortcuts, girara_shortcut_t*, iter, shortcuts_it)
    if (((shortcuts_it->mask == modifier && shortcuts_it->key == key && (modifier != 0 || key != 0)) ||
       (buffer && shortcuts_it->buffered_command && !strcmp(shortcuts_it->buffered_command, buffer)))
        && shortcuts_it->mode == mode)
    {
      girara_list_remove(session->bindings.shortcuts, shortcuts_it);
      girara_list_iterator_free(iter);
      return true;
    }
  GIRARA_LIST_FOREACH_END(session->bindings.shortcuts, girara_shortcut_t*, iter, shortcuts_it);

  return false;
}

void
girara_shortcut_free(girara_shortcut_t* shortcut)
{
  g_return_if_fail(shortcut != NULL);
  g_free(shortcut->argument.data);
  g_slice_free(girara_shortcut_t, shortcut);
}

bool
girara_inputbar_shortcut_add(girara_session_t* session, guint modifier, guint key, girara_shortcut_function_t function, int argument_n, void* argument_data)
{
  g_return_val_if_fail(session  != NULL, false);
  g_return_val_if_fail(function != NULL, false);

  girara_argument_t argument = {argument_n, argument_data};

  /* search for existing special command */
  GIRARA_LIST_FOREACH(session->bindings.inputbar_shortcuts, girara_inputbar_shortcut_t*, iter, inp_sh_it)
    if (inp_sh_it->mask == modifier && inp_sh_it->key == key) {
      inp_sh_it->function = function;
      inp_sh_it->argument = argument;

      girara_list_iterator_free(iter);
      return true;
    }
  GIRARA_LIST_FOREACH_END(session->bindings.inputbar_shortcuts, girara_inputbar_shortcut_t*, iter, inp_sh_it);

  /* create new inputbar shortcut */
  girara_inputbar_shortcut_t* inputbar_shortcut = g_slice_new(girara_inputbar_shortcut_t);

  inputbar_shortcut->mask     = modifier;
  inputbar_shortcut->key      = key;
  inputbar_shortcut->function = function;
  inputbar_shortcut->argument = argument;

  girara_list_append(session->bindings.inputbar_shortcuts, inputbar_shortcut);
  return true;
}

bool
girara_inputbar_shortcut_remove(girara_session_t* session, guint modifier, guint key)
{
  g_return_val_if_fail(session  != NULL, false);

  /* search for existing special command */
  GIRARA_LIST_FOREACH(session->bindings.inputbar_shortcuts, girara_inputbar_shortcut_t*, iter, inp_sh_it)
    if (inp_sh_it->mask == modifier && inp_sh_it->key == key) {
      girara_list_remove(session->bindings.inputbar_shortcuts, inp_sh_it);
      girara_list_iterator_free(iter);
      return true;
    }
  GIRARA_LIST_FOREACH_END(session->bindings.inputbar_shortcuts, girara_inputbar_shortcut_t*, iter, inp_sh_it);

  return true;
}

void
girara_inputbar_shortcut_free(girara_inputbar_shortcut_t* inputbar_shortcut)
{
  g_slice_free(girara_inputbar_shortcut_t, inputbar_shortcut);
}
bool
girara_isc_abort(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  /* hide completion */
  girara_argument_t arg = { GIRARA_HIDE, NULL };
  girara_isc_completion(session, &arg, NULL, 0);

  /* clear inputbar */
  gtk_editable_delete_text(GTK_EDITABLE(session->gtk.inputbar_entry), 0, -1);

  /* grab view */
  gtk_widget_grab_focus(GTK_WIDGET(session->gtk.view));

  /* hide inputbar */
  gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar_dialog));
  gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));

  /* reset custom functions */
  session->signals.inputbar_custom_activate        = NULL;
  session->signals.inputbar_custom_key_press_event = NULL;
  gtk_entry_set_visibility(session->gtk.inputbar_entry, TRUE);

  return true;
}

bool
girara_isc_string_manipulation(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  gchar *separator = NULL;
  girara_setting_get(session, "word-separator", &separator);
  gchar *input  = gtk_editable_get_chars(GTK_EDITABLE(session->gtk.inputbar_entry), 0, -1);
  int    length = strlen(input);
  int pos       = gtk_editable_get_position(GTK_EDITABLE(session->gtk.inputbar_entry));
  int i;

  switch (argument->n) {
    case GIRARA_DELETE_LAST_WORD:
      i = pos - 1;

      if (!pos) {
        break;
      }

      /* remove trailing spaces */
      for (; i >= 0 && input[i] == ' '; i--);

      /* find the beginning of the word */
      while ((i == (pos - 1)) || ((i > 0) && !strchr(separator, input[i]))) {
        i--;
      }

      gtk_editable_delete_text(GTK_EDITABLE(session->gtk.inputbar_entry),  i + 1, pos);
      gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), i + 1);
      break;
    case GIRARA_DELETE_LAST_CHAR:
      if ((length - 1) <= 0) {
        girara_isc_abort(session, argument, NULL, 0);
      }
      gtk_editable_delete_text(GTK_EDITABLE(session->gtk.inputbar_entry), pos - 1, pos);
      break;
    case GIRARA_DELETE_TO_LINE_START:
      gtk_editable_delete_text(GTK_EDITABLE(session->gtk.inputbar_entry), 1, pos);
      break;
    case GIRARA_NEXT_CHAR:
      gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), pos + 1);
      break;
    case GIRARA_PREVIOUS_CHAR:
      gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), (pos == 0) ? 0 : pos - 1);
      break;
    case GIRARA_DELETE_CURR_CHAR:
      if((length - 1) <= 0) {
        girara_isc_abort(session, argument, NULL, 0);
      }
      gtk_editable_delete_text(GTK_EDITABLE(session->gtk.inputbar_entry), pos, pos + 1);
      break;
    case GIRARA_DELETE_TO_LINE_END:
      gtk_editable_delete_text(GTK_EDITABLE(session->gtk.inputbar_entry), pos, length);
      break;
    case GIRARA_GOTO_START:
      gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), 1);
      break;
    case GIRARA_GOTO_END:
      gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), -1);
      break;
  }

  g_free(separator);
  g_free(input);

  return false;
}

/* default shortcut implementation */
bool
girara_sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(session->gtk.inputbar_entry != NULL, false);

  if (gtk_widget_get_visible(GTK_WIDGET(session->gtk.inputbar)) == false) {
    gtk_widget_show(GTK_WIDGET(session->gtk.inputbar));
  }

  if (gtk_widget_get_visible(GTK_WIDGET(session->gtk.notification_area)) == true) {
    gtk_widget_hide(GTK_WIDGET(session->gtk.notification_area));
  }

  gtk_widget_grab_focus(GTK_WIDGET(session->gtk.inputbar_entry));

  if (argument != NULL && argument->data != NULL) {
    gtk_entry_set_text(session->gtk.inputbar_entry, (char*) argument->data);

    /* we save the X clipboard that will be clear by "grab_focus" */
    gchar* x_clipboard_text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY));

    gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), -1);

    if (x_clipboard_text != NULL) {
      /* we reset the X clipboard with saved text */
      gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), x_clipboard_text, -1);
      g_free(x_clipboard_text);
    }
  }

  return true;
}

bool
girara_sc_abort(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_isc_abort(session, NULL, NULL, 0);
  gtk_widget_hide(GTK_WIDGET(session->gtk.notification_area));

  return false;
}

bool
girara_sc_quit(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_argument_t arg = { GIRARA_HIDE, NULL };
  girara_isc_completion(session, &arg, NULL, 0);

  gtk_main_quit();

  return false;
}

bool
girara_sc_tab_close(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_tab_t* tab = girara_tab_current_get(session);

  if (tab != NULL) {
    girara_tab_remove(session, tab);
  }

  return false;
}

bool
girara_sc_tab_navigate(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int t)
{
  g_return_val_if_fail(session != NULL, false);

  unsigned int number_of_tabs = girara_get_number_of_tabs(session);
  unsigned int current_tab    = girara_tab_position_get(session, girara_tab_current_get(session));
  unsigned int step           = (argument->n == GIRARA_PREVIOUS) ? -1 : 1;
  unsigned int new_tab        = (current_tab + step) % number_of_tabs;

  if (t != 0 && t <= number_of_tabs) {
    new_tab = t - 1;
  }

  girara_tab_t* tab = girara_tab_get(session, new_tab);

  if (tab != NULL) {
    girara_tab_current_set(session, tab);
  }

  girara_tab_update(session);

  return false;
}

static void
girara_toggle_widget_visibility(GtkWidget* widget)
{
  if (widget == NULL) {
    return;
  }

  if (gtk_widget_get_visible(widget)) {
    gtk_widget_hide(widget);
  } else {
    gtk_widget_show(widget);
  }
}

bool
girara_sc_toggle_inputbar(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_toggle_widget_visibility(GTK_WIDGET(session->gtk.inputbar));

  return true;
}

bool
girara_sc_toggle_statusbar(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_toggle_widget_visibility(GTK_WIDGET(session->gtk.statusbar));

  return true;
}

bool
girara_sc_toggle_tabbar(girara_session_t* session, girara_argument_t* UNUSED(argument), girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session != NULL, false);

  girara_toggle_widget_visibility(GTK_WIDGET(session->gtk.tabbar));

  return true;
}

bool
girara_sc_set(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event), unsigned int UNUSED(t))
{
  g_return_val_if_fail(session  != NULL, false);

  if (argument == NULL || argument->data == NULL) {
    return false;
  }

  /* create argument list */
  girara_list_t* argument_list = girara_list_new();
  if (argument_list == NULL) {
    return false;
  }

  gchar** argv = NULL;
  gint argc    = 0;

  girara_list_set_free_function(argument_list, g_free);
  if (g_shell_parse_argv((const gchar*) argument->data, &argc, &argv, NULL) != FALSE) {
    for(int i = 0; i < argc; i++) {
      char* argument = g_strdup(argv[i]);
      if (argument != NULL) {
        girara_list_append(argument_list, (void*) argument);
      } else {
        girara_list_free(argument_list);
        return false;
      }
    }
  } else {
    girara_list_free(argument_list);
    return false;
  }

  /* call set */
  girara_cmd_set(session, argument_list);

  /* cleanup */
  girara_list_free(argument_list);

  return false;
}

bool girara_shortcut_mapping_add(girara_session_t* session, const char* identifier, girara_shortcut_function_t function)
{
  g_return_val_if_fail(session  != NULL, false);

  if (function == NULL || identifier == NULL) {
    return false;
  }

  GIRARA_LIST_FOREACH(session->config.shortcut_mappings, girara_shortcut_mapping_t*, iter, data)
    if (strcmp(data->identifier, identifier) == 0) {
      data->function = function;
      girara_list_iterator_free(iter);
      return true;
    }
  GIRARA_LIST_FOREACH_END(session->config.shortcut_mappings, girara_shortcut_mapping_t*, iter, data);

  /* add new config handle */
  girara_shortcut_mapping_t* mapping = g_slice_new(girara_shortcut_mapping_t);

  mapping->identifier = g_strdup(identifier);
  mapping->function   = function;
  girara_list_append(session->config.shortcut_mappings, mapping);

  return true;
}

void
girara_shortcut_mapping_free(girara_shortcut_mapping_t* mapping)
{
  if (mapping == NULL) {
    return;
  }

  g_free(mapping->identifier);
  g_slice_free(girara_shortcut_mapping_t, mapping);
}

bool girara_argument_mapping_add(girara_session_t* session, const char* identifier, int value)
{
  g_return_val_if_fail(session  != NULL, false);

  if (identifier == NULL) {
    return false;
  }

  GIRARA_LIST_FOREACH(session->config.argument_mappings, girara_argument_mapping_t*, iter, mapping);
    if (g_strcmp0(mapping->identifier, identifier) == 0) {
      mapping->value = value;
      girara_list_iterator_free(iter);
      return true;
    }
  GIRARA_LIST_FOREACH_END(session->config.argument_mappings, girara_argument_mapping_t*, iter, mapping);

  /* add new config handle */
  girara_argument_mapping_t* mapping = g_slice_new(girara_argument_mapping_t);

  mapping->identifier = g_strdup(identifier);
  mapping->value      = value;
  girara_list_append(session->config.argument_mappings, mapping);

  return true;
}

void
girara_argument_mapping_free(girara_argument_mapping_t* argument_mapping)
{
  if (argument_mapping == NULL) {
    return;
  }

  g_free(argument_mapping->identifier);
  g_slice_free(girara_argument_mapping_t, argument_mapping);
}

bool
girara_mouse_event_add(girara_session_t* session, guint mask, guint button,
    girara_shortcut_function_t function, girara_mode_t mode, girara_event_type_t
    event_type, int argument_n, void* argument_data)
{
  g_return_val_if_fail(session  != NULL, false);
  g_return_val_if_fail(function != NULL, false);

  girara_argument_t argument = {argument_n, argument_data};

  /* search for existing binding */
  GIRARA_LIST_FOREACH(session->bindings.mouse_events, girara_mouse_event_t*, iter, me_it)
    if (me_it->mask == mask && me_it->button == button &&
       me_it->mode == mode && me_it->event_type == event_type)
    {
      me_it->function = function;
      me_it->argument = argument;
      girara_list_iterator_free(iter);
      return true;
    }
  GIRARA_LIST_FOREACH_END(session->bindings.mouse_events, girara_mouse_event_t*, iter, me_it);

  /* add new mouse event */
  girara_mouse_event_t* mouse_event = g_slice_new(girara_mouse_event_t);

  mouse_event->mask       = mask;
  mouse_event->button     = button;
  mouse_event->function   = function;
  mouse_event->mode       = mode;
  mouse_event->event_type = event_type;
  mouse_event->argument   = argument;
  girara_list_append(session->bindings.mouse_events, mouse_event);

  return true;
}

bool
girara_mouse_event_remove(girara_session_t* session, guint mask, guint button, girara_mode_t mode)
{
  g_return_val_if_fail(session  != NULL, false);

  /* search for existing binding */
  GIRARA_LIST_FOREACH(session->bindings.mouse_events, girara_mouse_event_t*, iter, me_it)
    if (me_it->mask == mask && me_it->button == button &&
       me_it->mode == mode)
    {
      girara_list_remove(session->bindings.mouse_events, me_it);
      girara_list_iterator_free(iter);
      return true;
    }
  GIRARA_LIST_FOREACH_END(session->bindings.mouse_events, girara_mouse_event_t*, iter, me_it);

  return false;
}

void
girara_mouse_event_free(girara_mouse_event_t* mouse_event)
{
  if (mouse_event == NULL) {
    return;
  }
  g_slice_free(girara_mouse_event_t, mouse_event);
}
