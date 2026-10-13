/*
 * Unit tests for the Kensington endpoint-zero Tudor framing.
 *
 * Copyright (c) 2026 VeriMarkOnLinux contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gio/gio.h>
#include <glib.h>

#include "../libfprint/drivers/synatlsmoc/kensington_transport.h"

static void
assert_command_plan (gsize logical_length,
                     gsize expected_wire_length,
                     guint16 expected_value)
{
  g_autofree guint8 *logical = g_malloc (logical_length);
  g_autofree guint8 *wire = NULL;
  g_autoptr (GError) error = NULL;
  KensingtonControlPlan plan;

  for (gsize i = 0; i < logical_length; i++)
    logical[i] = (guint8) (i + 1);

  g_assert_true (kensington_transport_prepare_command (
      logical, logical_length, &wire, &plan, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (plan.request, ==, KENSINGTON_TUDOR_REQUEST_COMMAND);
  g_assert_cmpuint (plan.value, ==, expected_value);
  g_assert_cmpuint (plan.index, ==, 0);
  g_assert_cmpuint (plan.wire_length, ==, expected_wire_length);
  g_assert_cmpmem (wire, logical_length, logical, logical_length);

  for (gsize i = logical_length; i < plan.wire_length; i++)
    g_assert_cmpuint (wire[i], ==, 0);
}

static void
test_capture_confirmed_command_shapes (void)
{
  assert_command_plan (1, 8, 1);
  assert_command_plan (8, 8, 0);
  assert_command_plan (76, 80, 4);
  assert_command_plan (616, 616, 0);
  assert_command_plan (617, 624, 1);
}

static void
test_response_plan (void)
{
  g_autoptr (GError) error = NULL;
  KensingtonControlPlan plan;

  g_assert_true (kensington_transport_prepare_response (1671, &plan, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (plan.request, ==, KENSINGTON_TUDOR_REQUEST_RESPONSE);
  g_assert_cmpuint (plan.value, ==, 0);
  g_assert_cmpuint (plan.index, ==, 0);
  g_assert_cmpuint (plan.wire_length, ==, 1671);
}

static void
test_invalid_lengths (void)
{
  const guint8 byte = 0xaa;
  g_autofree guint8 *wire = NULL;
  g_autoptr (GError) error = NULL;
  KensingtonControlPlan plan;

  g_assert_false (kensington_transport_prepare_command (
      &byte, 0, &wire, &plan, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error (&error);

  g_assert_false (kensington_transport_prepare_command (
      &byte, G_MAXUINT16, &wire, &plan, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error (&error);

  g_assert_false (kensington_transport_prepare_response (0, &plan, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
}

static void
test_windows_event_mask_layout (void)
{
  guint8 encoded[KENSINGTON_EVENT_MASK_BYTES];
  const guint8 expected[KENSINGTON_EVENT_MASK_BYTES] = {
    0x04, 0x03, 0x02, 0x01, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0x04, 0x03, 0x02, 0x01, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
  };

  kensington_transport_encode_event_mask (encoded, 0x01020304);
  g_assert_cmpmem (encoded, sizeof (encoded), expected, sizeof (expected));
}

static void
test_captured_led_ex2_layout (void)
{
  guint8 waiting[KENSINGTON_LED_EX2_COMMAND_SIZE];
  guint8 rejected[KENSINGTON_LED_EX2_COMMAND_SIZE];
  guint8 off[KENSINGTON_LED_EX2_COMMAND_SIZE];
  const guint8 captured_prefix[] = {
    0x39, 0x00, 0x71, 0x02, 0x00, 0xff, 0xff, 0x00, 0x00, 0x05,
    0x7f, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x05,
    0x7f, 0x00, 0x20, 0x00, 0x00, 0x00,
  };

  const guint8 rejected_prefix[] = {
    0x39, 0xe8, 0x03, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x07,
    0x7f, 0x00, 0x20, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
  };

  kensington_transport_build_led_ex2 (waiting,
                                       KENSINGTON_LED_EVENT_WAITING);
  kensington_transport_build_led_ex2 (rejected,
                                       KENSINGTON_LED_EVENT_REJECTED);
  kensington_transport_build_led_ex2 (off, KENSINGTON_LED_EVENT_OFF);

  g_assert_cmpmem (waiting, sizeof (captured_prefix), captured_prefix,
                   sizeof (captured_prefix));
  g_assert_cmpmem (rejected, sizeof (rejected_prefix), rejected_prefix,
                   sizeof (rejected_prefix));
  for (guint i = 0; i < 6; i++)
    {
      gsize offset = 5 + i * 20;

      g_assert_cmpuint (off[offset + 7], ==, 0x20);
    }
  for (guint i = 3; i < 6; i++)
    {
      gsize offset = 5 + i * 20;

      g_assert_cmpuint (waiting[offset], ==, 0x00);
      g_assert_cmpuint (waiting[offset + 7], ==, 0x00);
    }
  for (guint i = 4; i < 6; i++)
    {
      gsize offset = 5 + i * 20;

      g_assert_cmpuint (rejected[offset], ==, 0x00);
      g_assert_cmpuint (rejected[offset + 7], ==, 0x00);
    }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/synatlsmoc/kensington/command-shapes",
                   test_capture_confirmed_command_shapes);
  g_test_add_func ("/synatlsmoc/kensington/response-plan",
                   test_response_plan);
  g_test_add_func ("/synatlsmoc/kensington/invalid-lengths",
                   test_invalid_lengths);
  g_test_add_func ("/synatlsmoc/kensington/windows-event-mask-layout",
                   test_windows_event_mask_layout);
  g_test_add_func ("/synatlsmoc/kensington/captured-led-ex2-layout",
                   test_captured_led_ex2_layout);
  return g_test_run ();
}
