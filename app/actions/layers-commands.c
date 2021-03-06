/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "actions-types.h"

#include "config/gimpdialogconfig.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable-fill.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-merge.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-undo-push.h"
#include "core/gimpitemundo.h"
#include "core/gimplayer-floating-selection.h"
#include "core/gimplayer-new.h"
#include "core/gimppickable.h"
#include "core/gimppickable-auto-shrink.h"
#include "core/gimptoolinfo.h"
#include "core/gimpundostack.h"
#include "core/gimpprogress.h"

#include "text/gimptext.h"
#include "text/gimptext-vectors.h"
#include "text/gimptextlayer.h"

#include "vectors/gimpvectors-warp.h"

#include "widgets/gimpaction.h"
#include "widgets/gimpdock.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpprogressdialog.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpimagewindow.h"

#include "tools/gimptexttool.h"
#include "tools/tool_manager.h"

#include "dialogs/dialogs.h"
#include "dialogs/layer-add-mask-dialog.h"
#include "dialogs/layer-options-dialog.h"
#include "dialogs/resize-dialog.h"
#include "dialogs/scale-dialog.h"

#include "actions.h"
#include "layers-commands.h"

#include "gimp-intl.h"


static const GimpLayerModeEffects layer_modes[] =
{
  GIMP_NORMAL_MODE,
  GIMP_DISSOLVE_MODE,
  GIMP_MULTIPLY_MODE,
  GIMP_DIVIDE_MODE,
  GIMP_SCREEN_MODE,
  GIMP_NEW_OVERLAY_MODE,
  GIMP_DODGE_MODE,
  GIMP_BURN_MODE,
  GIMP_HARDLIGHT_MODE,
  GIMP_SOFTLIGHT_MODE,
  GIMP_GRAIN_EXTRACT_MODE,
  GIMP_GRAIN_MERGE_MODE,
  GIMP_DIFFERENCE_MODE,
  GIMP_ADDITION_MODE,
  GIMP_SUBTRACT_MODE,
  GIMP_DARKEN_ONLY_MODE,
  GIMP_LIGHTEN_ONLY_MODE,
  GIMP_HUE_MODE,
  GIMP_SATURATION_MODE,
  GIMP_COLOR_MODE,
  GIMP_VALUE_MODE,
  GIMP_LCH_HUE_MODE,
  GIMP_LCH_CHROMA_MODE,
  GIMP_LCH_COLOR_MODE,
  GIMP_LCH_LIGHTNESS_MODE
};


/*  local function prototypes  */

static void   layers_new_callback             (GtkWidget             *dialog,
                                               GimpImage             *image,
                                               GimpLayer             *layer,
                                               GimpContext           *context,
                                               const gchar           *layer_name,
                                               GimpFillType           layer_fill_type,
                                               gint                   layer_width,
                                               gint                   layer_height,
                                               gboolean               rename_text_layer,
                                               gpointer               user_data);
static void   layers_edit_attributes_callback (GtkWidget             *dialog,
                                               GimpImage             *image,
                                               GimpLayer             *layer,
                                               GimpContext           *context,
                                               const gchar           *layer_name,
                                               GimpFillType           layer_fill_type,
                                               gint                   layer_width,
                                               gint                   layer_height,
                                               gboolean               rename_text_layer,
                                               gpointer               user_data);
static void   layers_add_mask_callback        (GtkWidget             *dialog,
                                               GimpLayer             *layer,
                                               GimpAddMaskType        add_mask_type,
                                               GimpChannel           *channel,
                                               gboolean               invert,
                                               gpointer               user_data);
static void   layers_scale_callback           (GtkWidget             *dialog,
                                               GimpViewable          *viewable,
                                               gint                   width,
                                               gint                   height,
                                               GimpUnit               unit,
                                               GimpInterpolationType  interpolation,
                                               gdouble                xresolution,
                                               gdouble                yresolution,
                                               GimpUnit               resolution_unit,
                                               gpointer               user_data);
static void   layers_resize_callback          (GtkWidget             *dialog,
                                               GimpViewable          *viewable,
                                               GimpContext           *context,
                                               gint                   width,
                                               gint                   height,
                                               GimpUnit               unit,
                                               gint                   offset_x,
                                               gint                   offset_y,
                                               GimpItemSet            unused,
                                               gboolean               unused2,
                                               gpointer               data);

static gint   layers_mode_index               (GimpLayerModeEffects   layer_mode);


/*  private variables  */

static GimpUnit               layer_resize_unit   = GIMP_UNIT_PIXEL;
static GimpUnit               layer_scale_unit    = GIMP_UNIT_PIXEL;
static GimpInterpolationType  layer_scale_interp  = -1;


/*  public functions  */

void
layers_text_tool_cmd_callback (GtkAction *action,
                               gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *widget;
  GimpTool  *active_tool;
  return_if_no_layer (image, layer, data);
  return_if_no_widget (widget, data);

  if (! gimp_item_is_text_layer (GIMP_ITEM (layer)))
    {
      layers_edit_attributes_cmd_callback (action, data);
      return;
    }

  active_tool = tool_manager_get_active (image->gimp);

  if (! GIMP_IS_TEXT_TOOL (active_tool))
    {
      GimpToolInfo *tool_info = gimp_get_tool_info (image->gimp,
                                                    "gimp-text-tool");

      if (GIMP_IS_TOOL_INFO (tool_info))
        {
          gimp_context_set_tool (action_data_get_context (data), tool_info);
          active_tool = tool_manager_get_active (image->gimp);
        }
    }

  if (GIMP_IS_TEXT_TOOL (active_tool))
    gimp_text_tool_set_layer (GIMP_TEXT_TOOL (active_tool), layer);
}

void
layers_edit_attributes_cmd_callback (GtkAction *action,
                                     gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *widget;
  GtkWidget *dialog;
  return_if_no_layer (image, layer, data);
  return_if_no_widget (widget, data);

#define EDIT_DIALOG_KEY "gimp-layer-edit-attributes-dialog"

  dialog = dialogs_get_dialog (G_OBJECT (layer), EDIT_DIALOG_KEY);

  if (! dialog)
    {
      dialog = layer_options_dialog_new (gimp_item_get_image (GIMP_ITEM (layer)),
                                         layer,
                                         action_data_get_context (data),
                                         widget,
                                         _("Layer Attributes"),
                                         "gimp-layer-edit",
                                         "gtk-edit",
                                         _("Edit Layer Attributes"),
                                         GIMP_HELP_LAYER_EDIT,
                                         gimp_object_get_name (layer),
                                         0 /* unused */,
                                         layers_edit_attributes_callback,
                                         NULL);

      dialogs_attach_dialog (G_OBJECT (layer), EDIT_DIALOG_KEY, dialog);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

void
layers_new_cmd_callback (GtkAction *action,
                         gpointer   data)
{
  GimpImage *image;
  GtkWidget *widget;
  GimpLayer *floating_sel;
  GtkWidget *dialog;
  return_if_no_image (image, data);
  return_if_no_widget (widget, data);

  /*  If there is a floating selection, the new command transforms
   *  the current fs into a new layer
   */
  if ((floating_sel = gimp_image_get_floating_selection (image)))
    {
      GError *error = NULL;

      if (! floating_sel_to_layer (floating_sel, &error))
        {
          gimp_message_literal (image->gimp,
                                G_OBJECT (widget), GIMP_MESSAGE_WARNING,
                                error->message);
          g_clear_error (&error);
          return;
        }

      gimp_image_flush (image);
      return;
    }

#define NEW_DIALOG_KEY "gimp-layer-new-dialog"

  dialog = dialogs_get_dialog (G_OBJECT (image), NEW_DIALOG_KEY);

  if (! dialog)
    {
      GimpDialogConfig *config = GIMP_DIALOG_CONFIG (image->gimp->config);

      dialog = layer_options_dialog_new (image, NULL,
                                         action_data_get_context (data),
                                         widget,
                                         _("New Layer"),
                                         "gimp-layer-new",
                                         GIMP_STOCK_LAYER,
                                         _("Create a New Layer"),
                                         GIMP_HELP_LAYER_NEW,
                                         config->layer_new_name,
                                         config->layer_new_fill_type,
                                         layers_new_callback,
                                         NULL);

      dialogs_attach_dialog (G_OBJECT (image), NEW_DIALOG_KEY, dialog);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

void
layers_new_last_vals_cmd_callback (GtkAction *action,
                                   gpointer   data)
{
  GimpImage            *image;
  GtkWidget            *widget;
  GimpLayer            *floating_sel;
  GimpLayer            *new_layer;
  gint                  width, height;
  gint                  off_x, off_y;
  gdouble               opacity;
  GimpLayerModeEffects  mode;
  GimpDialogConfig     *config;
  return_if_no_image (image, data);
  return_if_no_widget (widget, data);

  config = GIMP_DIALOG_CONFIG (image->gimp->config);

  /*  If there is a floating selection, the new command transforms
   *  the current fs into a new layer
   */
  if ((floating_sel = gimp_image_get_floating_selection (image)))
    {
      layers_new_cmd_callback (action, data);
      return;
    }

  if (GIMP_IS_LAYER (GIMP_ACTION (action)->viewable))
    {
      GimpLayer *template = GIMP_LAYER (GIMP_ACTION (action)->viewable);

      gimp_item_get_offset (GIMP_ITEM (template), &off_x, &off_y);
      width   = gimp_item_get_width  (GIMP_ITEM (template));
      height  = gimp_item_get_height (GIMP_ITEM (template));
      opacity = gimp_layer_get_opacity (template);
      mode    = gimp_layer_get_mode (template);
    }
  else
    {
      width   = gimp_image_get_width (image);
      height  = gimp_image_get_height (image);
      off_x   = 0;
      off_y   = 0;
      opacity = 1.0;
      mode    = GIMP_NORMAL_MODE;
    }

  gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_EDIT_PASTE,
                               _("New Layer"));

  new_layer = gimp_layer_new (image, width, height,
                              gimp_image_get_layer_format (image, TRUE),
                              config->layer_new_name,
                              opacity, mode);

  gimp_drawable_fill (GIMP_DRAWABLE (new_layer),
                      action_data_get_context (data),
                      config->layer_new_fill_type);
  gimp_item_translate (GIMP_ITEM (new_layer), off_x, off_y, FALSE);

  gimp_image_add_layer (image, new_layer,
                        GIMP_IMAGE_ACTIVE_PARENT, -1, TRUE);

  gimp_image_undo_group_end (image);

  gimp_image_flush (image);
}

void
layers_new_from_visible_cmd_callback (GtkAction *action,
                                      gpointer   data)
{
  GimpImage        *image;
  GimpLayer        *layer;
  GimpPickable     *pickable;
  GimpColorProfile *profile;
  return_if_no_image (image, data);

  pickable = GIMP_PICKABLE (image);

  gimp_pickable_flush (pickable);

  profile = gimp_color_managed_get_color_profile (GIMP_COLOR_MANAGED (image));

  layer = gimp_layer_new_from_gegl_buffer (gimp_pickable_get_buffer (pickable),
                                           image,
                                           gimp_image_get_layer_format (image,
                                                                        TRUE),
                                           _("Visible"),
                                           GIMP_OPACITY_OPAQUE,
                                           GIMP_NORMAL_MODE,
                                           profile);

  gimp_image_add_layer (image, layer, GIMP_IMAGE_ACTIVE_PARENT, -1, TRUE);
  gimp_image_flush (image);
}

void
layers_new_group_cmd_callback (GtkAction *action,
                               gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_image (image, data);

  layer = gimp_group_layer_new (image);

  gimp_image_add_layer (image, layer, GIMP_IMAGE_ACTIVE_PARENT, -1, TRUE);
  gimp_image_flush (image);
}

void
layers_select_cmd_callback (GtkAction *action,
                            gint       value,
                            gpointer   data)
{
  GimpImage     *image;
  GimpLayer     *layer;
  GimpContainer *container;
  GimpLayer     *new_layer;
  return_if_no_image (image, data);

  layer = gimp_image_get_active_layer (image);

  if (layer)
    container = gimp_item_get_container (GIMP_ITEM (layer));
  else
    container = gimp_image_get_layers (image);

  new_layer = (GimpLayer *) action_select_object ((GimpActionSelectType) value,
                                                  container,
                                                  (GimpObject *) layer);

  if (new_layer && new_layer != layer)
    {
      gimp_image_set_active_layer (image, new_layer);
      gimp_image_flush (image);
    }
}

void
layers_raise_cmd_callback (GtkAction *action,
                           gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_image_raise_item (image, GIMP_ITEM (layer), NULL);
  gimp_image_flush (image);
}

void
layers_raise_to_top_cmd_callback (GtkAction *action,
                                  gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_image_raise_item_to_top (image, GIMP_ITEM (layer));
  gimp_image_flush (image);
}

void
layers_lower_cmd_callback (GtkAction *action,
                           gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_image_lower_item (image, GIMP_ITEM (layer), NULL);
  gimp_image_flush (image);
}

void
layers_lower_to_bottom_cmd_callback (GtkAction *action,
                                     gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_image_lower_item_to_bottom (image, GIMP_ITEM (layer));
  gimp_image_flush (image);
}

void
layers_duplicate_cmd_callback (GtkAction *action,
                               gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  GimpLayer *new_layer;
  return_if_no_layer (image, layer, data);

  new_layer = GIMP_LAYER (gimp_item_duplicate (GIMP_ITEM (layer),
                                               G_TYPE_FROM_INSTANCE (layer)));

  /*  use the actual parent here, not GIMP_IMAGE_ACTIVE_PARENT because
   *  the latter would add a duplicated group inside itself instead of
   *  above it
   */
  gimp_image_add_layer (image, new_layer,
                        gimp_layer_get_parent (layer), -1,
                        TRUE);
  gimp_image_flush (image);
}

void
layers_anchor_cmd_callback (GtkAction *action,
                            gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (gimp_layer_is_floating_sel (layer))
    {
      floating_sel_anchor (layer);
      gimp_image_flush (image);
    }
}

void
layers_merge_down_cmd_callback (GtkAction *action,
                                gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_image_merge_down (image, layer, action_data_get_context (data),
                         GIMP_EXPAND_AS_NECESSARY, NULL);
  gimp_image_flush (image);
}

void
layers_merge_group_cmd_callback (GtkAction *action,
                                 gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_image_merge_group_layer (image, GIMP_GROUP_LAYER (layer));
  gimp_image_flush (image);
}

void
layers_delete_cmd_callback (GtkAction *action,
                            gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_image_remove_layer (image, layer, TRUE, NULL);
  gimp_image_flush (image);
}

void
layers_text_discard_cmd_callback (GtkAction *action,
                                  gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (GIMP_IS_TEXT_LAYER (layer))
    gimp_text_layer_discard (GIMP_TEXT_LAYER (layer));
}

void
layers_text_to_vectors_cmd_callback (GtkAction *action,
                                     gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (GIMP_IS_TEXT_LAYER (layer))
    {
      GimpVectors *vectors;
      gint         x, y;

      vectors = gimp_text_vectors_new (image, GIMP_TEXT_LAYER (layer)->text);

      gimp_item_get_offset (GIMP_ITEM (layer), &x, &y);
      gimp_item_translate (GIMP_ITEM (vectors), x, y, FALSE);

      gimp_image_add_vectors (image, vectors,
                              GIMP_IMAGE_ACTIVE_PARENT, -1, TRUE);
      gimp_image_flush (image);
    }
}

void
layers_text_along_vectors_cmd_callback (GtkAction *action,
                                        gpointer   data)
{
  GimpImage   *image;
  GimpLayer   *layer;
  GimpVectors *vectors;
  return_if_no_layer (image, layer, data);
  return_if_no_vectors (image, vectors, data);

  if (GIMP_IS_TEXT_LAYER (layer))
    {
      GimpVectors *new_vectors;

      new_vectors = gimp_text_vectors_new (image, GIMP_TEXT_LAYER (layer)->text);

      gimp_vectors_warp_vectors (vectors, new_vectors,
                                 0.5 * gimp_item_get_height (GIMP_ITEM (layer)));

      gimp_item_set_visible (GIMP_ITEM (new_vectors), TRUE, FALSE);

      gimp_image_add_vectors (image, new_vectors,
                              GIMP_IMAGE_ACTIVE_PARENT, -1, TRUE);
      gimp_image_flush (image);
    }
}

void
layers_resize_cmd_callback (GtkAction *action,
                            gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *widget;
  GtkWidget *dialog;
  return_if_no_layer (image, layer, data);
  return_if_no_widget (widget, data);

#define RESIZE_DIALOG_KEY "gimp-resize-dialog"

  dialog = dialogs_get_dialog (G_OBJECT (layer), RESIZE_DIALOG_KEY);

  if (! dialog)
    {
      GimpDisplay *display = NULL;

      if (GIMP_IS_IMAGE_WINDOW (data))
        display = action_data_get_display (data);

      if (layer_resize_unit != GIMP_UNIT_PERCENT && display)
        layer_resize_unit = gimp_display_get_shell (display)->unit;

      dialog = resize_dialog_new (GIMP_VIEWABLE (layer),
                                  action_data_get_context (data),
                                  _("Set Layer Boundary Size"),
                                  "gimp-layer-resize",
                                  widget,
                                  gimp_standard_help_func,
                                  GIMP_HELP_LAYER_RESIZE,
                                  layer_resize_unit,
                                  layers_resize_callback,
                                  NULL);

      dialogs_attach_dialog (G_OBJECT (layer), RESIZE_DIALOG_KEY, dialog);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

void
layers_resize_to_image_cmd_callback (GtkAction *action,
                                     gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_layer_resize_to_image (layer, action_data_get_context (data));
  gimp_image_flush (image);
}

void
layers_scale_cmd_callback (GtkAction *action,
                           gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *widget;
  GtkWidget *dialog;
  return_if_no_layer (image, layer, data);
  return_if_no_widget (widget, data);

#define SCALE_DIALOG_KEY "gimp-scale-dialog"

  dialog = dialogs_get_dialog (G_OBJECT (layer), SCALE_DIALOG_KEY);

  if (! dialog)
    {
      GimpDisplay *display = NULL;

      if (GIMP_IS_IMAGE_WINDOW (data))
        display = action_data_get_display (data);

      if (layer_scale_unit != GIMP_UNIT_PERCENT && display)
        layer_scale_unit = gimp_display_get_shell (display)->unit;

      if (layer_scale_interp == -1)
        layer_scale_interp = image->gimp->config->interpolation_type;

      dialog = scale_dialog_new (GIMP_VIEWABLE (layer),
                                 action_data_get_context (data),
                                 _("Scale Layer"), "gimp-layer-scale",
                                 widget,
                                 gimp_standard_help_func, GIMP_HELP_LAYER_SCALE,
                                 layer_scale_unit,
                                 layer_scale_interp,
                                 layers_scale_callback,
                                 display);

      dialogs_attach_dialog (G_OBJECT (layer), SCALE_DIALOG_KEY, dialog);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

void
layers_crop_to_selection_cmd_callback (GtkAction *action,
                                       gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *widget;
  gint       x, y;
  gint       width, height;
  gint       off_x, off_y;
  return_if_no_layer (image, layer, data);
  return_if_no_widget (widget, data);

  if (! gimp_item_bounds (GIMP_ITEM (gimp_image_get_mask (image)),
                          &x, &y, &width, &height))
    {
      gimp_message_literal (image->gimp,
                            G_OBJECT (widget), GIMP_MESSAGE_WARNING,
                            _("Cannot crop because the current selection "
                              "is empty."));
      return;
    }

  gimp_item_get_offset (GIMP_ITEM (layer), &off_x, &off_y);
  off_x -= x;
  off_y -= y;

  gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_ITEM_RESIZE,
                               _("Crop Layer to Selection"));

  gimp_item_resize (GIMP_ITEM (layer), action_data_get_context (data),
                    width, height, off_x, off_y);

  gimp_image_undo_group_end (image);
  gimp_image_flush (image);
}

void
layers_crop_to_content_cmd_callback (GtkAction *action,
                                     gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *widget;
  gint       x, y;
  gint       width, height;
  return_if_no_layer (image, layer, data);
  return_if_no_widget (widget, data);

  switch (gimp_pickable_auto_shrink (GIMP_PICKABLE (layer),
                                     0, 0,
                                     gimp_item_get_width  (GIMP_ITEM (layer)),
                                     gimp_item_get_height (GIMP_ITEM (layer)),
                                     &x, &y, &width, &height))
    {
    case GIMP_AUTO_SHRINK_SHRINK:
      gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_ITEM_RESIZE,
                                   _("Crop Layer to Content"));

      gimp_item_resize (GIMP_ITEM (layer), action_data_get_context (data),
                        width, height, -x, -y);

      gimp_image_undo_group_end (image);
      gimp_image_flush (image);
      break;

    case GIMP_AUTO_SHRINK_EMPTY:
      gimp_message_literal (image->gimp,
                            G_OBJECT (widget), GIMP_MESSAGE_INFO,
                            _("Cannot crop because the active layer "
                              "has no content."));
      break;

    case GIMP_AUTO_SHRINK_UNSHRINKABLE:
      gimp_message_literal (image->gimp,
                            G_OBJECT (widget), GIMP_MESSAGE_INFO,
                            _("Cannot crop because the active layer "
                              "is already cropped to its content."));
      break;
    }
}

void
layers_mask_add_cmd_callback (GtkAction *action,
                              gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  GtkWidget *widget;
  GtkWidget *dialog;
  return_if_no_layer (image, layer, data);
  return_if_no_widget (widget, data);

  if (gimp_layer_get_mask (layer))
    return;

#define ADD_MASK_DIALOG_KEY "gimp-add-mask-dialog"

  dialog = dialogs_get_dialog (G_OBJECT (layer), ADD_MASK_DIALOG_KEY);

  if (! dialog)
    {
      GimpDialogConfig *config = GIMP_DIALOG_CONFIG (image->gimp->config);

      dialog = layer_add_mask_dialog_new (layer, action_data_get_context (data),
                                          widget,
                                          config->layer_add_mask_type,
                                          config->layer_add_mask_invert,
                                          layers_add_mask_callback,
                                          NULL);

      dialogs_attach_dialog (G_OBJECT (layer), ADD_MASK_DIALOG_KEY, dialog);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

void
layers_mask_add_last_vals_cmd_callback (GtkAction *action,
                                        gpointer   data)
{
  GimpImage        *image;
  GimpLayer        *layer;
  GtkWidget        *widget;
  GimpDialogConfig *config;
  GimpChannel      *channel = NULL;
  GimpLayerMask    *mask;
  return_if_no_layer (image, layer, data);
  return_if_no_widget (widget, data);

  if (gimp_layer_get_mask (layer))
    return;

  config = GIMP_DIALOG_CONFIG (image->gimp->config);

  if (config->layer_add_mask_type == GIMP_ADD_MASK_CHANNEL)
    {
      channel = gimp_image_get_active_channel (image);

      if (! channel)
        {
          GimpContainer *channels = gimp_image_get_channels (image);

          channel = GIMP_CHANNEL (gimp_container_get_first_child (channels));
        }

      if (! channel)
        {
          layers_mask_add_cmd_callback (action, data);
          return;
        }
    }

  mask = gimp_layer_create_mask (layer,
                                 config->layer_add_mask_type,
                                 channel);

  if (config->layer_add_mask_invert)
    gimp_channel_invert (GIMP_CHANNEL (mask), FALSE);

  gimp_layer_add_mask (layer, mask, TRUE, NULL);
  gimp_image_flush (image);
}

void
layers_mask_apply_cmd_callback (GtkAction *action,
                                gint       value,
                                gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (gimp_layer_get_mask (layer))
    {
      GimpMaskApplyMode mode = (GimpMaskApplyMode) value;

      gimp_layer_apply_mask (layer, mode, TRUE);
      gimp_image_flush (image);
    }
}

void
layers_mask_edit_cmd_callback (GtkAction *action,
                               gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (gimp_layer_get_mask (layer))
    {
      gboolean active;

      active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

      gimp_layer_set_edit_mask (layer, active);
      gimp_image_flush (image);
    }
}

void
layers_mask_show_cmd_callback (GtkAction *action,
                               gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (gimp_layer_get_mask (layer))
    {
      gboolean active;

      active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

      gimp_layer_set_show_mask (layer, active, TRUE);
      gimp_image_flush (image);
    }
}

void
layers_mask_disable_cmd_callback (GtkAction *action,
                                  gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (gimp_layer_get_mask (layer))
    {
      gboolean active;

      active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

      gimp_layer_set_apply_mask (layer, ! active, TRUE);
      gimp_image_flush (image);
    }
}

void
layers_mask_to_selection_cmd_callback (GtkAction *action,
                                       gint       value,
                                       gpointer   data)
{
  GimpImage     *image;
  GimpLayer     *layer;
  GimpLayerMask *mask;
  return_if_no_layer (image, layer, data);

  mask = gimp_layer_get_mask (layer);

  if (mask)
    {
      gimp_item_to_selection (GIMP_ITEM (mask),
                              (GimpChannelOps) value,
                              TRUE, FALSE, 0.0, 0.0);
      gimp_image_flush (image);
    }
}

void
layers_alpha_add_cmd_callback (GtkAction *action,
                               gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (! gimp_drawable_has_alpha (GIMP_DRAWABLE (layer)))
    {
      gimp_layer_add_alpha (layer);
      gimp_image_flush (image);
    }
}

void
layers_alpha_remove_cmd_callback (GtkAction *action,
                                  gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  if (gimp_drawable_has_alpha (GIMP_DRAWABLE (layer)))
    {
      gimp_layer_remove_alpha (layer, action_data_get_context (data));
      gimp_image_flush (image);
    }
}

void
layers_alpha_to_selection_cmd_callback (GtkAction *action,
                                        gint       value,
                                        gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  return_if_no_layer (image, layer, data);

  gimp_item_to_selection (GIMP_ITEM (layer),
                          (GimpChannelOps) value,
                          TRUE, FALSE, 0.0, 0.0);
  gimp_image_flush (image);
}

void
layers_opacity_cmd_callback (GtkAction *action,
                             gint       value,
                             gpointer   data)
{
  GimpImage      *image;
  GimpLayer      *layer;
  gdouble         opacity;
  GimpUndo       *undo;
  gboolean        push_undo = TRUE;
  return_if_no_layer (image, layer, data);

  undo = gimp_image_undo_can_compress (image, GIMP_TYPE_ITEM_UNDO,
                                       GIMP_UNDO_LAYER_OPACITY);

  if (undo && GIMP_ITEM_UNDO (undo)->item == GIMP_ITEM (layer))
    push_undo = FALSE;

  opacity = action_select_value ((GimpActionSelectType) value,
                                 gimp_layer_get_opacity (layer),
                                 0.0, 1.0, 1.0,
                                 1.0 / 255.0, 0.01, 0.1, 0.0, FALSE);
  gimp_layer_set_opacity (layer, opacity, push_undo);
  gimp_image_flush (image);
}

void
layers_mode_cmd_callback (GtkAction *action,
                          gint       value,
                          gpointer   data)
{
  GimpImage            *image;
  GimpLayer            *layer;
  GimpLayerModeEffects  layer_mode;
  gint                  index;
  GimpUndo             *undo;
  gboolean              push_undo = TRUE;
  return_if_no_layer (image, layer, data);

  undo = gimp_image_undo_can_compress (image, GIMP_TYPE_ITEM_UNDO,
                                       GIMP_UNDO_LAYER_MODE);

  if (undo && GIMP_ITEM_UNDO (undo)->item == GIMP_ITEM (layer))
    push_undo = FALSE;

  layer_mode = gimp_layer_get_mode (layer);

  index = action_select_value ((GimpActionSelectType) value,
                               layers_mode_index (layer_mode),
                               0, G_N_ELEMENTS (layer_modes) - 1, 0,
                               0.0, 1.0, 1.0, 0.0, FALSE);
  gimp_layer_set_mode (layer, layer_modes[index], push_undo);
  gimp_image_flush (image);
}

void
layers_lock_alpha_cmd_callback (GtkAction *action,
                                gpointer   data)
{
  GimpImage *image;
  GimpLayer *layer;
  gboolean   lock_alpha;
  return_if_no_layer (image, layer, data);

  lock_alpha = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

  if (lock_alpha != gimp_layer_get_lock_alpha (layer))
    {
      GimpUndo *undo;
      gboolean  push_undo = TRUE;

      undo = gimp_image_undo_can_compress (image, GIMP_TYPE_ITEM_UNDO,
                                           GIMP_UNDO_LAYER_LOCK_ALPHA);

      if (undo && GIMP_ITEM_UNDO (undo)->item == GIMP_ITEM (layer))
        push_undo = FALSE;

      gimp_layer_set_lock_alpha (layer, lock_alpha, push_undo);
      gimp_image_flush (image);
    }
}


/*  private functions  */

static void
layers_new_callback (GtkWidget    *dialog,
                     GimpImage    *image,
                     GimpLayer    *layer,
                     GimpContext  *context,
                     const gchar  *layer_name,
                     GimpFillType  layer_fill_type,
                     gint          layer_width,
                     gint          layer_height,
                     gboolean      rename_text_layer, /* unused */
                     gpointer      user_data)
{
  GimpDialogConfig *config = GIMP_DIALOG_CONFIG (image->gimp->config);

  g_object_set (config,
                "layer-new-name",      layer_name,
                "layer-new-fill-type", layer_fill_type,
                NULL);

  layer = gimp_layer_new (image, layer_width, layer_height,
                          gimp_image_get_layer_format (image, TRUE),
                          config->layer_new_name,
                          GIMP_OPACITY_OPAQUE, GIMP_NORMAL_MODE);

  if (layer)
    {
      gimp_drawable_fill (GIMP_DRAWABLE (layer), context,
                          config->layer_new_fill_type);
      gimp_image_add_layer (image, layer,
                            GIMP_IMAGE_ACTIVE_PARENT, -1, TRUE);
      gimp_image_flush (image);
    }
  else
    {
      g_warning ("%s: could not allocate new layer", G_STRFUNC);
    }

  gtk_widget_destroy (dialog);
}

static void
layers_edit_attributes_callback (GtkWidget    *dialog,
                                 GimpImage    *image,
                                 GimpLayer    *layer,
                                 GimpContext  *context,
                                 const gchar  *layer_name,
                                 GimpFillType  layer_fill_type, /* unused */
                                 gint          layer_width,     /* unused */
                                 gint          layer_height,    /* unused */
                                 gboolean      rename_text_layer,
                                 gpointer      user_data)
{
  if (strcmp (layer_name, gimp_object_get_name (layer)))
    {
      GError *error = NULL;

      if (gimp_item_rename (GIMP_ITEM (layer), layer_name, &error))
        {
          gimp_image_flush (image);
        }
      else
        {
          gimp_message_literal (image->gimp,
                                G_OBJECT (dialog), GIMP_MESSAGE_WARNING,
                                error->message);
          g_clear_error (&error);

          return;
        }
    }

  if (gimp_item_is_text_layer (GIMP_ITEM (layer)))
    {
      g_object_set (layer,
                    "auto-rename", rename_text_layer,
                    NULL);
    }

  gtk_widget_destroy (dialog);
}

static void
layers_add_mask_callback (GtkWidget       *dialog,
                          GimpLayer       *layer,
                          GimpAddMaskType  add_mask_type,
                          GimpChannel     *channel,
                          gboolean         invert,
                          gpointer         user_data)
{
  GimpImage        *image  = gimp_item_get_image (GIMP_ITEM (layer));
  GimpDialogConfig *config = GIMP_DIALOG_CONFIG (image->gimp->config);
  GimpLayerMask    *mask;
  GError           *error = NULL;

  g_object_set (config,
                "layer-add-mask-type",   add_mask_type,
                "layer-add-mask-invert", invert,
                NULL);

  mask = gimp_layer_create_mask (layer,
                                 config->layer_add_mask_type,
                                 channel);

  if (config->layer_add_mask_invert)
    gimp_channel_invert (GIMP_CHANNEL (mask), FALSE);

  if (! gimp_layer_add_mask (layer, mask, TRUE, &error))
    {
      gimp_message_literal (image->gimp,
                            G_OBJECT (dialog), GIMP_MESSAGE_WARNING,
                            error->message);
      g_object_unref (mask);
      g_clear_error (&error);
      return;
    }

  gimp_image_flush (image);

  gtk_widget_destroy (dialog);
}

static void
layers_scale_callback (GtkWidget             *dialog,
                       GimpViewable          *viewable,
                       gint                   width,
                       gint                   height,
                       GimpUnit               unit,
                       GimpInterpolationType  interpolation,
                       gdouble                xresolution,    /* unused */
                       gdouble                yresolution,    /* unused */
                       GimpUnit               resolution_unit,/* unused */
                       gpointer               user_data)
{
  GimpDisplay *display = GIMP_DISPLAY (user_data);

  layer_scale_unit   = unit;
  layer_scale_interp = interpolation;

  if (width > 0 && height > 0)
    {
      GimpItem     *item = GIMP_ITEM (viewable);
      GimpProgress *progress;
      GtkWidget    *progress_dialog = NULL;

      gtk_widget_destroy (dialog);

      if (width  == gimp_item_get_width  (item) &&
          height == gimp_item_get_height (item))
        return;

      if (display)
        {
          progress = GIMP_PROGRESS (display);
        }
      else
        {
          progress_dialog = gimp_progress_dialog_new ();
          progress = GIMP_PROGRESS (progress_dialog);
        }

      progress = gimp_progress_start (progress, FALSE, _("Scaling"));

      gimp_item_scale_by_origin (item,
                                 width, height, interpolation,
                                 progress, TRUE);

      if (progress)
        gimp_progress_end (progress);

      if (progress_dialog)
        gtk_widget_destroy (progress_dialog);

      gimp_image_flush (gimp_item_get_image (item));
    }
  else
    {
      g_warning ("Scale Error: "
                 "Both width and height must be greater than zero.");
    }
}

static void
layers_resize_callback (GtkWidget    *dialog,
                        GimpViewable *viewable,
                        GimpContext  *context,
                        gint          width,
                        gint          height,
                        GimpUnit      unit,
                        gint          offset_x,
                        gint          offset_y,
                        GimpItemSet   unused,
                        gboolean      unused2,
                        gpointer      user_data)
{
  layer_resize_unit = unit;

  if (width > 0 && height > 0)
    {
      GimpItem *item = GIMP_ITEM (viewable);

      gtk_widget_destroy (dialog);

      if (width  == gimp_item_get_width  (item) &&
          height == gimp_item_get_height (item))
        return;

      gimp_item_resize (item, context,
                        width, height, offset_x, offset_y);
      gimp_image_flush (gimp_item_get_image (item));
    }
  else
    {
      g_warning ("Resize Error: "
                 "Both width and height must be greater than zero.");
    }
}

static gint
layers_mode_index (GimpLayerModeEffects layer_mode)
{
  gint i = 0;

  while (i < (G_N_ELEMENTS (layer_modes) - 1) && layer_modes[i] != layer_mode)
    i++;

  return i;
}
