#include "unity-gtk-action-group.h"
#include "unity-gtk-action.h"
#include <gio/gio.h>

static void unity_gtk_action_group_action_group_init (GActionGroupInterface *iface);

G_DEFINE_TYPE_WITH_CODE (UnityGtkActionGroup,
                         unity_gtk_action_group,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP,
                                                unity_gtk_action_group_action_group_init));

static void
unity_gtk_action_group_dispose (GObject *object)
{
  UnityGtkActionGroup *group;
  GHashTable *actions_by_name;
  GHashTable *names_by_radio_menu_item;

  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (object));

  group = UNITY_GTK_ACTION_GROUP (object);
  actions_by_name = group->actions_by_name;
  names_by_radio_menu_item = group->names_by_radio_menu_item;

  if (names_by_radio_menu_item != NULL)
    {
      group->names_by_radio_menu_item = NULL;
      g_hash_table_unref (names_by_radio_menu_item);
    }

  if (actions_by_name != NULL)
    {
      group->actions_by_name = NULL;
      g_hash_table_unref (actions_by_name);
    }

  G_OBJECT_CLASS (unity_gtk_action_group_parent_class)->dispose (object);
}

static gchar **
unity_gtk_action_group_list_actions (GActionGroup *action_group)
{
  UnityGtkActionGroup *group;
  GHashTable *actions_by_name;
  GHashTableIter iter;
  gchar **names;
  gpointer key;
  guint n;
  guint i;

  g_return_val_if_fail (UNITY_GTK_IS_ACTION_GROUP (action_group), NULL);

  group = UNITY_GTK_ACTION_GROUP (action_group);
  actions_by_name = group->actions_by_name;

  g_return_val_if_fail (actions_by_name != NULL, NULL);

  n = g_hash_table_size (actions_by_name);
  names = g_malloc_n (n + 1, sizeof (gchar *));

  g_hash_table_iter_init (&iter, actions_by_name);
  for (i = 0; i < n && g_hash_table_iter_next (&iter, &key, NULL); i++)
    names[i] = g_strdup (key);

  names[i] = NULL;

  return names;
}

static void
unity_gtk_action_group_really_change_action_state (GActionGroup *action_group,
                                                   const gchar  *name,
                                                   GVariant     *value)
{
  UnityGtkActionGroup *group;
  GHashTable *actions_by_name;
  UnityGtkAction *action;

  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (action_group));

  group = UNITY_GTK_ACTION_GROUP (action_group);
  actions_by_name = group->actions_by_name;

  g_return_if_fail (actions_by_name != NULL);

  action = g_hash_table_lookup (actions_by_name, name);

  g_return_if_fail (action != NULL);

  if (action->items_by_name != NULL)
    {
      const gchar *name;
      UnityGtkMenuItem *item;

      g_return_if_fail (value != NULL && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING));

      name = g_variant_get_string (value, NULL);
      item = g_hash_table_lookup (action->items_by_name, name);

      if (item != NULL && unity_gtk_menu_item_is_check (item))
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item->menu_item), TRUE);
    }
  else if (action->item != NULL && unity_gtk_menu_item_is_check (action->item))
    {
      g_return_if_fail (value != NULL && g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN));

      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (action->item->menu_item), g_variant_get_boolean (value));
    }
  else
    g_warn_if_fail (value == NULL);
}

static void
unity_gtk_action_group_change_action_state (GActionGroup *action_group,
                                            const gchar  *name,
                                            GVariant     *value)
{
  g_variant_ref_sink (value);
  unity_gtk_action_group_really_change_action_state (action_group, name, value);
  g_variant_unref (value);
}

static void
unity_gtk_action_group_activate_action (GActionGroup *action_group,
                                        const gchar  *name,
                                        GVariant     *parameter)
{
  UnityGtkActionGroup *group;
  GHashTable *actions_by_name;
  UnityGtkAction *action;

  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (action_group));

  group = UNITY_GTK_ACTION_GROUP (action_group);
  actions_by_name = group->actions_by_name;

  g_return_if_fail (actions_by_name != NULL);

  action = g_hash_table_lookup (actions_by_name, name);

  g_return_if_fail (action != NULL);

  if (action->items_by_name != NULL)
    {
      const gchar *name;
      UnityGtkMenuItem *item;

      g_return_if_fail (parameter != NULL && g_variant_is_of_type (parameter, G_VARIANT_TYPE_STRING));

      name = g_variant_get_string (parameter, NULL);
      item = g_hash_table_lookup (action->items_by_name, name);

      if (item != NULL && item->menu_item != NULL)
        gtk_menu_item_activate (item->menu_item);
    }
  else if (action->item != NULL)
    {
      g_warn_if_fail (parameter == NULL);

      if (action->item->menu_item != NULL)
        gtk_menu_item_activate (action->item->menu_item);
    }
}

static gboolean
unity_gtk_action_group_query_action (GActionGroup        *action_group,
                                     const gchar         *name,
                                     gboolean            *enabled,
                                     const GVariantType **parameter_type,
                                     const GVariantType **state_type,
                                     GVariant           **state_hint,
                                     GVariant           **state)
{
  UnityGtkActionGroup *group;
  GHashTable *actions_by_name;
  UnityGtkAction *action;

  g_return_val_if_fail (UNITY_GTK_IS_ACTION_GROUP (action_group), FALSE);

  group = UNITY_GTK_ACTION_GROUP (action_group);
  actions_by_name = group->actions_by_name;

  g_return_val_if_fail (actions_by_name != NULL, FALSE);

  action = g_hash_table_lookup (actions_by_name, name);

  if (action != NULL)
    {
      if (enabled != NULL)
        {
          if (action->items_by_name != NULL)
            {
              GHashTableIter iter;
              gpointer value;

              *enabled = FALSE;

              g_hash_table_iter_init (&iter, action->items_by_name);
              while (!*enabled && g_hash_table_iter_next (&iter, NULL, &value))
                *enabled = unity_gtk_menu_item_is_sensitive (value);
            }
          else
            *enabled = action->item != NULL && unity_gtk_menu_item_is_sensitive (action->item);
        }

      if (parameter_type != NULL)
        {
          if (action->items_by_name != NULL)
            *parameter_type = G_VARIANT_TYPE_STRING;
          else
            *parameter_type = NULL;
        }

      if (state_type != NULL)
        {
          if (action->items_by_name != NULL)
            *state_type = G_VARIANT_TYPE_STRING;
          else if (action->item != NULL && unity_gtk_menu_item_is_check (action->item))
            *state_type = G_VARIANT_TYPE_BOOLEAN;
          else
            *state_type = NULL;
        }

      if (state_hint != NULL)
        {
          if (action->items_by_name != NULL)
            {
              GVariantBuilder builder;
              GHashTableIter iter;
              gpointer key;

              g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

              g_hash_table_iter_init (&iter, action->items_by_name);
              while (g_hash_table_iter_next (&iter, &key, NULL))
                g_variant_builder_add (&builder, "s", key);

              *state_hint = g_variant_ref_sink (g_variant_builder_end (&builder));
            }
          else if (action->item != NULL && unity_gtk_menu_item_is_check (action->item))
            {
              GVariantBuilder builder;

              g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);

              g_variant_builder_add (&builder, "b", FALSE);
              g_variant_builder_add (&builder, "b", TRUE);

              *state_hint = g_variant_ref_sink (g_variant_builder_end (&builder));
            }
          else
            *state_hint = NULL;
        }

      if (state != NULL)
        {
          if (action->items_by_name != NULL)
            {
              GHashTableIter iter;
              gpointer key;
              gpointer value;

              *state = NULL;

              g_hash_table_iter_init (&iter, action->items_by_name);
              while (*state == NULL && g_hash_table_iter_next (&iter, &key, &value))
                if (unity_gtk_menu_item_is_active (value))
                  *state = g_variant_ref_sink (g_variant_new_string (key));
            }
          else if (action->item != NULL && unity_gtk_menu_item_is_check (action->item))
            *state = g_variant_ref_sink (g_variant_new_boolean (unity_gtk_menu_item_is_active (action->item)));
          else
            *state = NULL;
        }
    }

  return action != NULL;
}

static void
unity_gtk_action_group_class_init (UnityGtkActionGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = unity_gtk_action_group_dispose;
}

static void
unity_gtk_action_group_action_group_init (GActionGroupInterface *iface)
{
  iface->list_actions = unity_gtk_action_group_list_actions;
  iface->change_action_state = unity_gtk_action_group_change_action_state;
  iface->activate_action = unity_gtk_action_group_activate_action;
  iface->query_action = unity_gtk_action_group_query_action;
}

static void
unity_gtk_action_group_init (UnityGtkActionGroup *self)
{
  self->actions_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  self->names_by_radio_menu_item = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}

UnityGtkActionGroup *
unity_gtk_action_group_new (void)
{
  return g_object_new (UNITY_GTK_TYPE_ACTION_GROUP,
                       NULL);
}

static gchar *
g_strdup_normalize (const gchar *str)
{
  gchar *string = g_strdup (str);
  guint i = 0;
  guint j;

  for (j = 0; str[j] != '\0'; j++)
    {
      if (g_ascii_isalnum (str[j]))
        string[i++] = str[j];
      else
        string[i++] = '-';
    }

  string[i] = '\0';

  return string;
}

static gchar *
unity_gtk_action_group_get_action_name (UnityGtkActionGroup *group,
                                        UnityGtkMenuItem    *item)
{
  GtkMenuItem *menu_item;
  GtkAction *action;
  const gchar *name;
  gchar *normalized_name;
  GHashTable *actions_by_name;

  g_return_val_if_fail (UNITY_GTK_IS_ACTION_GROUP (group), NULL);
  g_return_val_if_fail (UNITY_GTK_IS_MENU_ITEM (item), NULL);

  menu_item = item->menu_item;

  g_return_val_if_fail (menu_item != NULL, NULL);

  if (GTK_IS_RADIO_MENU_ITEM (menu_item))
    {
      GtkRadioMenuItem *radio_menu_item = GTK_RADIO_MENU_ITEM (menu_item);
      GSList *iter = g_slist_last (gtk_radio_menu_item_get_group (radio_menu_item));

      if (iter != NULL)
        menu_item = iter->data;
    }

  name = NULL;
  action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (menu_item));

  if (action != NULL)
    name = gtk_action_get_name (action);

  if (name == NULL || name[0] == '\0')
    name = gtk_menu_item_get_label (menu_item);

  g_return_val_if_fail (name != NULL && name[0] != '\0', NULL);

  normalized_name = g_strdup_normalize (name);
  actions_by_name = group->actions_by_name;

  if (actions_by_name != NULL && g_hash_table_contains (actions_by_name, normalized_name))
    {
      gchar *next_normalized_name = NULL;
      guint i = 0;

      do
        {
          g_free (next_normalized_name);
          next_normalized_name = g_strdup_printf ("%s-%u", normalized_name, i++);
        }
      while (g_hash_table_contains (actions_by_name, next_normalized_name));

      g_free (normalized_name);
      normalized_name = next_normalized_name;
    }

  return normalized_name;
}

static gchar *
unity_gtk_action_group_get_state_name (UnityGtkActionGroup *group,
                                       UnityGtkMenuItem    *item)
{
  gchar *name = NULL;

  g_return_val_if_fail (UNITY_GTK_IS_ACTION_GROUP (group), NULL);
  g_return_val_if_fail (UNITY_GTK_IS_MENU_ITEM (item), NULL);

  if (unity_gtk_menu_item_is_radio (item))
    {
      const gchar *label = unity_gtk_menu_item_get_label (item);

      if (label != NULL && label[0] != '\0')
        {
          gchar *normalized_label = g_strdup_normalize (label);
          UnityGtkAction *action = item->action;

          if (action != NULL)
            {
              if (action->items_by_name != NULL)
                {
                  if (g_hash_table_contains (action->items_by_name, normalized_label))
                    {
                      guint i = 0;

                      do
                        {
                          g_free (name);
                          name = g_strdup_printf ("%s-%u", normalized_label, i++);
                        }
                      while (g_hash_table_contains (action->items_by_name, name));

                      g_free (normalized_label);
                    }
                  else
                    name = normalized_label;
                }
              else
                {
                  g_warn_if_reached ();
                  name = normalized_label;
                }
            }
          else
            name = normalized_label;
        }
      else
        {
          GtkActivatable *activatable = GTK_ACTIVATABLE (item->menu_item);
          GtkAction *action = gtk_activatable_get_related_action (activatable);

          if (action != NULL)
            {
              GtkRadioAction *radio_action = GTK_RADIO_ACTION (action);
              const gchar *action_name = gtk_action_get_name (action);
              gchar *normalized_action_name = NULL;
              gint value;

              g_object_get (radio_action, "value", &value, NULL);

              if (action_name != NULL && action_name[0] != '\0')
                normalized_action_name = g_strdup_normalize (action_name);

              if (normalized_action_name != NULL)
                {
                  if (normalized_action_name[0] != '\0')
                    name = g_strdup_printf ("%s-%d", normalized_action_name, value);
                  else
                    name = g_strdup_printf ("%d", value);

                  g_free (normalized_action_name);
                }
              else
                name = g_strdup_printf ("%d", value);

              if (item->action != NULL)
                {
                  GHashTable *items_by_name = item->action->items_by_name;

                  if (items_by_name != NULL && g_hash_table_contains (items_by_name, name))
                    {
                      gchar *next_name = NULL;
                      guint i = 0;

                      do
                        {
                          g_free (next_name);
                          next_name = g_strdup_printf ("%s-%u", name, i++);
                        }
                      while (g_hash_table_contains (items_by_name, next_name));

                      g_free (name);
                      name = next_name;
                    }
                }
            }
        }
    }

  return name;
}

void
unity_gtk_action_group_connect_item (UnityGtkActionGroup *group,
                                     UnityGtkMenuItem    *item)
{
  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (group));
  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (item));

  if (item->parent_shell != NULL && (item->parent_shell->action_group != group || item->action == NULL))
    {
      UnityGtkAction *new_action = NULL;
      UnityGtkAction *action = NULL;

      if (item->action != NULL)
        {
          if (item->parent_shell->action_group != NULL)
            unity_gtk_action_group_disconnect_item (item->parent_shell->action_group, item);
          else
            unity_gtk_menu_item_set_action (item, NULL);
        }

      if (unity_gtk_menu_item_is_radio (item))
        {
          GtkRadioMenuItem *radio_menu_item = GTK_RADIO_MENU_ITEM (item->menu_item);
          const gchar *action_name;
          gchar *state_name;

          g_return_if_fail (group->actions_by_name != NULL);
          g_return_if_fail (group->names_by_radio_menu_item != NULL);

          action_name = g_hash_table_lookup (group->names_by_radio_menu_item, radio_menu_item);

          if (action_name == NULL)
            {
              GtkRadioMenuItem *last_radio_menu_item = NULL;
              GSList *iter = gtk_radio_menu_item_get_group (radio_menu_item);

              while (action_name == NULL && iter != NULL)
                {
                  last_radio_menu_item = iter->data;
                  action_name = g_hash_table_lookup (group->names_by_radio_menu_item, last_radio_menu_item);
                  iter = g_slist_next (iter);
                }

              if (action_name == NULL)
                {
                  gchar *new_action_name = unity_gtk_action_group_get_action_name (group, item);

                  g_hash_table_insert (group->names_by_radio_menu_item, radio_menu_item, new_action_name);

                  if (last_radio_menu_item != NULL)
                    g_hash_table_insert (group->names_by_radio_menu_item, last_radio_menu_item, g_strdup (new_action_name));

                  action_name = new_action_name;
                }
              else
                g_hash_table_insert (group->names_by_radio_menu_item, radio_menu_item, g_strdup (action_name));
            }

          action = g_hash_table_lookup (group->actions_by_name, action_name);

          if (action == NULL)
            action = new_action = unity_gtk_action_new_radio (action_name);

          state_name = unity_gtk_action_group_get_state_name (group, item);
          g_hash_table_insert (action->items_by_name, state_name, item);
        }
      else if (!unity_gtk_menu_item_is_separator (item))
        {
          gchar *name = unity_gtk_action_group_get_action_name (group, item);
          action = new_action = unity_gtk_action_new (name, item);
          g_free (name);
        }

      unity_gtk_menu_item_set_action (item, action);

      if (new_action != NULL)
        {
          if (group->actions_by_name != NULL)
            g_hash_table_insert (group->actions_by_name, new_action->name, new_action);
          else
            g_warn_if_reached ();

          g_action_group_action_added (G_ACTION_GROUP (group), new_action->name);
        }
    }
}

void
unity_gtk_action_group_disconnect_item (UnityGtkActionGroup *group,
                                        UnityGtkMenuItem    *item)
{
  UnityGtkAction *action;

  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (group));
  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (item));
  g_warn_if_fail (item->parent_shell != NULL && item->parent_shell->action_group == group);

  action = item->action;

  if (action != NULL)
    {
      if (action->items_by_name != NULL)
        {
          const gchar *name = NULL;

          if (group->names_by_radio_menu_item == NULL)
            {
              GHashTableIter iter;
              gpointer key;
              gpointer value;

              g_warn_if_reached ();

              g_hash_table_iter_init (&iter, action->items_by_name);
              while (name == NULL && g_hash_table_iter_next (&iter, &key, &value))
                if (value == item)
                  name = key;
            }
          else
            name = g_hash_table_lookup (group->names_by_radio_menu_item, item->menu_item);

          if (name != NULL)
            {
              g_hash_table_remove (action->items_by_name, name);

              if (group->names_by_radio_menu_item != NULL)
                g_hash_table_remove (group->names_by_radio_menu_item, item->menu_item);
              else
                g_warn_if_reached ();

              if (g_hash_table_size (action->items_by_name) == 0)
                {
                  if (group->actions_by_name != NULL)
                    g_hash_table_remove (group->actions_by_name, action->name);
                  else
                    g_warn_if_reached ();

                  g_action_group_action_removed (G_ACTION_GROUP (group), action->name);
                }
            }
          else
            g_warn_if_reached ();
        }
      else
        {
          if (group->actions_by_name != NULL)
            g_hash_table_remove (group->actions_by_name, action->name);
          else
            g_warn_if_reached ();

          g_action_group_action_removed (G_ACTION_GROUP (group), action->name);
        }
    }

  unity_gtk_menu_item_set_action (item, NULL);
}

void
unity_gtk_action_group_connect_shell (UnityGtkActionGroup *group,
                                      UnityGtkMenuShell   *shell)
{
  GSequence *visible_indices;

  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (group));
  g_return_if_fail (UNITY_GTK_IS_MENU_SHELL (shell));

  visible_indices = shell->visible_indices;

  if (shell->action_group != NULL && shell->action_group != group)
    unity_gtk_action_group_disconnect_shell (shell->action_group, shell);

  if (visible_indices != NULL)
    {
      GSequenceIter *iter = g_sequence_get_begin_iter (visible_indices);

      while (!g_sequence_iter_is_end (iter))
        {
          guint i = GPOINTER_TO_UINT (g_sequence_get (iter));
          UnityGtkMenuItem *item = g_ptr_array_index (shell->items, i);

          unity_gtk_action_group_connect_item (group, item);

          if (item->child_shell != NULL)
            {
              if (item->child_shell_valid)
                unity_gtk_action_group_connect_shell (group, item->child_shell);
              else
                g_warn_if_reached ();
            }

          iter = g_sequence_iter_next (iter);
        }
    }

  if (shell->action_group == NULL)
    shell->action_group = g_object_ref (group);
}

void
unity_gtk_action_group_disconnect_shell (UnityGtkActionGroup *group,
                                         UnityGtkMenuShell   *shell)
{
  UnityGtkActionGroup *action_group;
  GSequence *visible_indices;

  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (group));
  g_return_if_fail (UNITY_GTK_IS_MENU_SHELL (shell));
  g_warn_if_fail (shell->action_group == group);

  visible_indices = shell->visible_indices;

  if (visible_indices != NULL)
    {
      GSequenceIter *iter = g_sequence_get_begin_iter (visible_indices);

      while (!g_sequence_iter_is_end (iter))
        {
          guint i = GPOINTER_TO_UINT (g_sequence_get (iter));
          UnityGtkMenuItem *item = g_ptr_array_index (shell->items, i);

          unity_gtk_action_group_disconnect_item (group, item);

          if (item->child_shell != NULL)
            {
              if (item->child_shell_valid)
                unity_gtk_action_group_disconnect_shell (group, item->child_shell);
              else
                g_warn_if_reached ();
            }

          iter = g_sequence_iter_next (iter);
        }
    }

  action_group = shell->action_group;

  if (action_group != NULL)
    {
      shell->action_group = NULL;
      g_object_unref (action_group);
    }
}

void
unity_gtk_action_group_print (UnityGtkActionGroup *group,
                              guint                indent)
{
  gchar *space;

  g_return_if_fail (group == NULL || UNITY_GTK_IS_ACTION_GROUP (group));

  space = g_strnfill (indent, ' ');

  if (group != NULL)
    {
      g_print ("%s(%s *) %p\n", space, G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (group)), group);

      if (group->actions_by_name != NULL)
        {
          GHashTableIter iter;
          gpointer key;
          gpointer value;

          g_hash_table_iter_init (&iter, group->actions_by_name);
          while (g_hash_table_iter_next (&iter, &key, &value))
            {
              g_print ("%s  \"%s\" ->\n", space, (const gchar *) key);
              unity_gtk_action_print (value, indent + 4);
            }
        }

      if (group->names_by_radio_menu_item != NULL)
        {
          GHashTableIter iter;
          gpointer key;
          gpointer value;

          g_hash_table_iter_init (&iter, group->names_by_radio_menu_item);
          while (g_hash_table_iter_next (&iter, &key, &value))
            g_print ("%s  (%s *) %p -> \"%s\"\n", space, G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (key)), key, (const gchar *) value);
        }
    }
  else
    g_print ("%sNULL\n", space);

  g_free (space);
}
