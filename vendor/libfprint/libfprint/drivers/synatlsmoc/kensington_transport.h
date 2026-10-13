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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define KENSINGTON_TUDOR_REQUEST_COMMAND  0x16
#define KENSINGTON_TUDOR_REQUEST_RESPONSE 0x17
#define KENSINGTON_EVENT_MASK_BYTES       32
#define KENSINGTON_LED_EX2_COMMAND_SIZE   125

typedef enum
{
  KENSINGTON_LED_EVENT_OFF = 0,
  KENSINGTON_LED_EVENT_WAITING = 1,
  KENSINGTON_LED_EVENT_REJECTED = 5,
} KensingtonLedEvent;

typedef struct
{
  guint8  request;
  guint16 value;
  guint16 index;
  gsize   wire_length;
} KensingtonControlPlan;

gboolean kensington_transport_prepare_command (const guint8          *logical,
                                                gsize                  logical_length,
                                                guint8               **wire,
                                                KensingtonControlPlan *plan,
                                                GError               **error);

gboolean kensington_transport_prepare_response (gsize                  expected_length,
                                                 KensingtonControlPlan *plan,
                                                 GError               **error);

void kensington_transport_encode_event_mask (
    guint8 event_masks[KENSINGTON_EVENT_MASK_BYTES],
    guint32 mask);

void kensington_transport_build_led_ex2 (
    guint8 command[KENSINGTON_LED_EX2_COMMAND_SIZE],
    KensingtonLedEvent event);

G_END_DECLS
