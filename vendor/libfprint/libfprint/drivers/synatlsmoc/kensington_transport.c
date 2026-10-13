/*
 * Kensington endpoint-zero transport for Synaptics Tudor Match-In-Sensor.
 *
 * Copyright (c) 2026 VeriMarkOnLinux contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "kensington_transport.h"

#include <gio/gio.h>
#include <string.h>

#define KENSINGTON_TUDOR_ALIGNMENT 8

static gboolean
validate_control_length (gsize length,
                         GError **error)
{
  if (length == 0 || length > G_MAXUINT16)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   "Kensington control length %" G_GSIZE_FORMAT
                   " is outside the USB control-transfer range",
                   length);
      return FALSE;
    }

  return TRUE;
}

gboolean
kensington_transport_prepare_command (const guint8          *logical,
                                       gsize                  logical_length,
                                       guint8               **wire,
                                       KensingtonControlPlan *plan,
                                       GError               **error)
{
  gsize wire_length;

  g_return_val_if_fail (wire != NULL, FALSE);
  g_return_val_if_fail (plan != NULL, FALSE);

  *wire = NULL;
  *plan = (KensingtonControlPlan) { 0 };

  if (logical == NULL || logical_length == 0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           "Kensington command must not be empty");
      return FALSE;
    }

  if (logical_length > G_MAXSIZE - (KENSINGTON_TUDOR_ALIGNMENT - 1))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           "Kensington command length overflows alignment");
      return FALSE;
    }

  wire_length = (logical_length + (KENSINGTON_TUDOR_ALIGNMENT - 1)) &
                ~((gsize) KENSINGTON_TUDOR_ALIGNMENT - 1);
  if (!validate_control_length (wire_length, error))
    return FALSE;

  *wire = g_malloc0 (wire_length);
  memcpy (*wire, logical, logical_length);

  plan->request = KENSINGTON_TUDOR_REQUEST_COMMAND;
  plan->value = logical_length % KENSINGTON_TUDOR_ALIGNMENT;
  plan->index = 0;
  plan->wire_length = wire_length;
  return TRUE;
}

gboolean
kensington_transport_prepare_response (gsize                  expected_length,
                                        KensingtonControlPlan *plan,
                                        GError               **error)
{
  g_return_val_if_fail (plan != NULL, FALSE);

  *plan = (KensingtonControlPlan) { 0 };
  if (!validate_control_length (expected_length, error))
    return FALSE;

  plan->request = KENSINGTON_TUDOR_REQUEST_RESPONSE;
  plan->value = 0;
  plan->index = 0;
  plan->wire_length = expected_length;
  return TRUE;
}

void
kensington_transport_encode_event_mask (
    guint8 event_masks[KENSINGTON_EVENT_MASK_BYTES],
    guint32 mask)
{
  guint32 mask_le = GUINT32_TO_LE (mask);

  g_return_if_fail (event_masks != NULL);

  /* Windows writes the active mask into event groups 0 and 4 only.  Repeating
   * it into all eight groups works for basic capture, but changes the capture
   * indication behaviour on 047d:00f2. */
  memset (event_masks, 0, KENSINGTON_EVENT_MASK_BYTES);
  memcpy (event_masks, &mask_le, sizeof (mask_le));
  memcpy (event_masks + 4 * sizeof (mask_le), &mask_le, sizeof (mask_le));
}

void
kensington_transport_build_led_ex2 (
    guint8 command[KENSINGTON_LED_EX2_COMMAND_SIZE],
    KensingtonLedEvent event)
{
  static const guint8 waiting_block[20] = {
    0xff, 0xff, 0x00, 0x00, 0x05, 0x7f, 0x00, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  static const guint8 rejected_frames[4][20] = {
    {
      0x4b, 0x00, 0x00, 0x00, 0x07, 0x7f, 0x00, 0x20, 0x7f, 0x7f,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
      0x4b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
      0x4b, 0x00, 0x00, 0x00, 0x01, 0x7f, 0x00, 0x20, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
      0x4b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
  };

  g_return_if_fail (command != NULL);
  g_return_if_fail (event == KENSINGTON_LED_EVENT_OFF ||
                    event == KENSINGTON_LED_EVENT_WAITING ||
                    event == KENSINGTON_LED_EVENT_REJECTED);

  memset (command, 0, KENSINGTON_LED_EX2_COMMAND_SIZE);
  command[0] = 0x39;

  switch (event)
    {
    case KENSINGTON_LED_EVENT_OFF:
      for (guint i = 0; i < 6; i++)
        command[5 + i * 20 + 7] = 0x20;
      break;

    case KENSINGTON_LED_EVENT_WAITING:
      command[2] = 0x71;
      command[3] = 0x02;
      for (guint i = 0; i < 3; i++)
        memcpy (command + 5 + i * sizeof (waiting_block), waiting_block,
                sizeof (waiting_block));
      break;

    case KENSINGTON_LED_EVENT_REJECTED:
      command[1] = 0xe8;
      command[2] = 0x03;
      memcpy (command + 5, rejected_frames, sizeof (rejected_frames));
      break;
    }
}
