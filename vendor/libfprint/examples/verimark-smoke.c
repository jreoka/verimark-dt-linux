/*
 * Private live smoke test for the Kensington VeriMark Desktop integration.
 *
 * This helper never deletes, formats, pairs, or prints template metadata. It
 * loads already-approved libfprint persistent pairing data,
 * opens the device, and performs bounded list/identify/verify/recovery tests.
 * Enrollment is available only with an exact, explicit approval token.
 */

#define FP_COMPONENT "verimark-smoke"
#define ENROLL_APPROVAL_TOKEN "CREATE_ONE_LIBFPRINT_TEST_ENROLLMENT"

#include <glib-unix.h>
#include <glib/gstdio.h>
#include <gusb.h>
#include <libfprint/fprint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

typedef enum
{
  OP_LIST,
  OP_IDENTIFY,
  OP_VERIFY,
  OP_CANCEL_RECOVER,
  OP_REMOVE,
  OP_ENROLL,
} SmokeOperation;

typedef struct
{
  FpContext *context;
  FpDevice *device;
  GMainLoop *loop;
  GCancellable *cancellable;
  GPtrArray *prints;

  gchar *persistent_path;
  gchar *operation_name;
  gchar *expect_name;
  gchar *approval_token;
  gchar *finger_name;
  gint print_index;
  guint cancel_after_ms;
  guint cancel_source;
  guint sigint_source;

  SmokeOperation operation;
  gboolean expect_match;
  gboolean action_match;
  gboolean cancel_requested;
  gboolean cancellation_observed;
  gboolean removal_observed;
  gboolean enrollment_committed;
  gboolean recovery_started;
  gboolean success;
  FpFinger selected_finger;
  const gchar *failure_stage;
} SmokeData;

static void
smoke_data_free (SmokeData *data)
{
  g_clear_handle_id (&data->cancel_source, g_source_remove);
  g_clear_handle_id (&data->sigint_source, g_source_remove);
  g_clear_pointer (&data->prints, g_ptr_array_unref);
  g_clear_object (&data->cancellable);
  g_clear_pointer (&data->loop, g_main_loop_unref);
  g_clear_object (&data->context);
  g_clear_pointer (&data->persistent_path, g_free);
  g_clear_pointer (&data->operation_name, g_free);
  g_clear_pointer (&data->expect_name, g_free);
  g_clear_pointer (&data->approval_token, g_free);
  g_clear_pointer (&data->finger_name, g_free);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SmokeData, smoke_data_free)

static void
print_report (SmokeData *data)
{
  g_print ("{\n");
  g_print ("  \"cancellation_observed\": %s,\n",
           data->cancellation_observed ? "true" : "false");
  g_print ("  \"enrollment_committed\": %s,\n",
           data->enrollment_committed ? "true" : "false");
  g_print ("  \"enrollment_write_approved\": %s,\n",
           data->operation == OP_ENROLL ? "true" : "false");
  if (data->failure_stage)
    g_print ("  \"failure_stage\": \"%s\",\n", data->failure_stage);
  else
    g_print ("  \"failure_stage\": null,\n");
  g_print ("  \"hid_interface_touched\": false,\n");
  g_print ("  \"operation\": \"%s\",\n", data->operation_name);
  g_print ("  \"pairing_material_logged\": false,\n");
  g_print ("  \"print_count\": %u,\n",
           data->prints ? data->prints->len : 0);
  g_print ("  \"reader_state_changed\": %s,\n",
           data->operation == OP_ENROLL ? "true" : "false");
  g_print ("  \"removal_observed\": %s,\n",
           data->removal_observed ? "true" : "false");
  g_print ("  \"result\": %s,\n",
           data->operation == OP_LIST || data->operation == OP_REMOVE ||
                   data->operation == OP_ENROLL
               ? "null"
               : (data->action_match ? "\"match\"" : "\"nonmatch\""));
  g_print ("  \"success\": %s,\n", data->success ? "true" : "false");
  g_print ("  \"template_identifiers_logged\": false\n");
  g_print ("}\n");
}

static gboolean
is_removal_error (GError *error)
{
  return error &&
         (g_error_matches (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_REMOVED) ||
          g_error_matches (error, G_USB_DEVICE_ERROR,
                           G_USB_DEVICE_ERROR_NO_DEVICE));
}

static void
on_device_closed (FpDevice     *device,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  SmokeData *data = user_data;
  g_autoptr (GError) error = NULL;

  if (!fp_device_close_finish (device, result, &error) &&
      !(data->operation == OP_REMOVE && data->removal_observed &&
        is_removal_error (error)))
    {
      data->success = FALSE;
      data->failure_stage = "close";
      g_warning ("Device close failed: %s", error->message);
    }

  print_report (data);
  g_main_loop_quit (data->loop);
}

static void
finish_and_close (SmokeData *data,
                  gboolean   success,
                  const char *failure_stage)
{
  data->success = success;
  data->failure_stage = failure_stage;
  g_clear_handle_id (&data->cancel_source, g_source_remove);

  if (fp_device_is_open (data->device))
    fp_device_close (data->device, NULL,
                     (GAsyncReadyCallback) on_device_closed, data);
  else
    {
      print_report (data);
      g_main_loop_quit (data->loop);
    }
}

static void start_action (SmokeData *data);

static void
on_enroll_progress (FpDevice *device,
                    gint      completed_stages,
                    FpPrint  *print,
                    gpointer  user_data,
                    GError   *error)
{
  if (error)
    g_print ("Sample rejected (%s). Lift the right little finger completely, "
             "then touch it again.\n", error->message);
  else
    g_print ("Sample accepted; sensor stage %d of %d. Lift the right little "
             "finger completely, then touch it again.\n",
             completed_stages, fp_device_get_nr_enroll_stages (device));
}

static void
on_enroll_finished (FpDevice     *device,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  SmokeData *data = user_data;
  g_autoptr (FpPrint) print = NULL;
  g_autoptr (GError) error = NULL;

  print = fp_device_enroll_finish (device, result, &error);
  if (!print)
    {
      g_warning ("Enrollment failed: %s", error->message);
      finish_and_close (data, FALSE, "enroll");
      return;
    }

  data->enrollment_committed = fp_print_get_device_stored (print);
  if (!data->enrollment_committed)
    {
      g_warning ("Enrollment completed without a device-stored print");
      finish_and_close (data, FALSE, "enroll-storage");
      return;
    }

  finish_and_close (data, TRUE, NULL);
}

static gboolean
cancel_action_cb (gpointer user_data)
{
  SmokeData *data = user_data;

  data->cancel_source = 0;
  data->cancel_requested = TRUE;
  g_print ("Cancelling the first identification; do not touch the reader yet.\n");
  g_cancellable_cancel (data->cancellable);
  return G_SOURCE_REMOVE;
}

static void
match_cb (FpDevice *device,
          FpPrint  *match,
          FpPrint  *print,
          gpointer  user_data,
          GError   *error)
{
  SmokeData *data = user_data;

  if (!error)
    data->action_match = match != NULL;
}

static void
on_identify_finished (FpDevice     *device,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  SmokeData *data = user_data;
  g_autoptr (FpPrint) match = NULL;
  g_autoptr (FpPrint) print = NULL;
  g_autoptr (GError) error = NULL;
  gboolean completed;

  completed = fp_device_identify_finish (device, result, &match, &print, &error);
  data->action_match = match != NULL;

  if (data->operation == OP_REMOVE)
    {
      if (!completed && is_removal_error (error))
        {
          data->removal_observed = TRUE;
          finish_and_close (data, TRUE, NULL);
          return;
        }

      g_warning ("Identification did not report physical device removal");
      finish_and_close (data, FALSE, "removal");
      return;
    }

  if (data->operation == OP_CANCEL_RECOVER && !data->recovery_started)
    {
      if (completed || !data->cancel_requested || error == NULL)
        {
          g_warning ("Identification did not report the requested cancellation");
          finish_and_close (data, FALSE, "cancel");
          return;
        }

      data->cancellation_observed = TRUE;
      data->recovery_started = TRUE;
      data->cancel_requested = FALSE;
      g_clear_object (&data->cancellable);
      data->cancellable = g_cancellable_new ();
      g_print ("Cancellation cleanup completed. Touch the enrolled finger now.\n");
      start_action (data);
      return;
    }

  if (!completed)
    {
      g_warning ("Identification failed: %s", error->message);
      finish_and_close (data, FALSE, "identify");
      return;
    }

  finish_and_close (data, data->action_match == data->expect_match,
                    data->action_match == data->expect_match ? NULL
                                                            : "expectation");
}

static void
on_verify_finished (FpDevice     *device,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  SmokeData *data = user_data;
  g_autoptr (FpPrint) print = NULL;
  g_autoptr (GError) error = NULL;
  gboolean match = FALSE;

  if (!fp_device_verify_finish (device, result, &match, &print, &error))
    {
      g_warning ("Verification failed: %s", error->message);
      finish_and_close (data, FALSE, "verify");
      return;
    }

  data->action_match = match;
  finish_and_close (data, match == data->expect_match,
                    match == data->expect_match ? NULL : "expectation");
}

static void
start_action (SmokeData *data)
{
  data->action_match = FALSE;

  if (data->operation == OP_ENROLL)
    {
      FpPrint *template = fp_print_new (data->device);

      fp_print_set_finger (template, FP_FINGER_RIGHT_LITTLE);
      fp_print_set_username (template, "verimark-step14-test");
      g_print ("Enrollment will write one new right-little-finger template.\n");
      g_print ("Lift the right little finger completely, then touch it.\n");
      fp_device_enroll (data->device, template, data->cancellable,
                        on_enroll_progress, data, NULL,
                        (GAsyncReadyCallback) on_enroll_finished, data);
      return;
    }

  if (data->operation == OP_VERIFY)
    {
      guint index;

      if (data->selected_finger != FP_FINGER_UNKNOWN)
        {
          guint matches = 0;

          index = 0;
          for (guint i = 0; i < data->prints->len; i++)
            {
              FpPrint *print = g_ptr_array_index (data->prints, i);

              if (fp_print_get_finger (print) == data->selected_finger)
                {
                  index = i;
                  matches++;
                }
            }
          if (matches != 1)
            {
              g_warning ("Expected exactly one stored print for --finger");
              finish_and_close (data, FALSE, "finger-selection");
              return;
            }
        }
      else if (data->print_index < 0 && data->prints->len == 1)
        index = 0;
      else if (data->print_index >= 0 &&
               (guint) data->print_index < data->prints->len)
        index = data->print_index;
      else
        {
          g_warning ("Use --print-index when more than one print is stored");
          finish_and_close (data, FALSE, "print-selection");
          return;
        }

      g_print ("Touch the fingerprint reader once.\n");
      fp_device_verify (data->device, g_ptr_array_index (data->prints, index),
                        data->cancellable, match_cb, data, NULL,
                        (GAsyncReadyCallback) on_verify_finished, data);
      return;
    }

  if (data->operation == OP_CANCEL_RECOVER && !data->recovery_started)
    {
      g_print ("Starting identification; keep your finger away.\n");
      data->cancel_source = g_timeout_add (data->cancel_after_ms,
                                           cancel_action_cb, data);
    }
  else if (data->operation == OP_REMOVE)
    g_print ("Unplug the fingerprint reader now; do not touch it.\n");
  else
    g_print ("Touch the fingerprint reader once.\n");

  fp_device_identify (data->device, data->prints, data->cancellable,
                      match_cb, data, NULL,
                      (GAsyncReadyCallback) on_identify_finished, data);
}

static void
on_prints_listed (FpDevice     *device,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  SmokeData *data = user_data;
  g_autoptr (GError) error = NULL;

  data->prints = fp_device_list_prints_finish (device, result, &error);
  if (!data->prints)
    {
      g_warning ("Print listing failed: %s", error->message);
      finish_and_close (data, FALSE, "list");
      return;
    }

  g_print ("Sensor reports %u stored print(s); identifiers are not displayed.\n",
           data->prints->len);

  if (data->operation == OP_LIST)
    {
      finish_and_close (data, TRUE, NULL);
      return;
    }
  if (data->prints->len == 0 && data->operation != OP_ENROLL)
    {
      finish_and_close (data, FALSE, "empty-storage");
      return;
    }

  start_action (data);
}

static void
on_device_opened (FpDevice     *device,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  SmokeData *data = user_data;
  g_autoptr (GError) error = NULL;

  if (!fp_device_open_finish (device, result, &error))
    {
      g_warning ("Device open failed: %s", error->message);
      finish_and_close (data, FALSE, "open");
      return;
    }

  fp_device_list_prints (device, data->cancellable,
                         (GAsyncReadyCallback) on_prints_listed, data);
}

static gboolean
sigint_cb (gpointer user_data)
{
  SmokeData *data = user_data;

  g_cancellable_cancel (data->cancellable);
  return G_SOURCE_CONTINUE;
}

static gboolean
parse_operation (SmokeData *data)
{
  if (g_strcmp0 (data->operation_name, "list") == 0)
    data->operation = OP_LIST;
  else if (g_strcmp0 (data->operation_name, "identify") == 0)
    data->operation = OP_IDENTIFY;
  else if (g_strcmp0 (data->operation_name, "verify") == 0)
    data->operation = OP_VERIFY;
  else if (g_strcmp0 (data->operation_name, "cancel-recover") == 0)
    data->operation = OP_CANCEL_RECOVER;
  else if (g_strcmp0 (data->operation_name, "remove") == 0)
    data->operation = OP_REMOVE;
  else if (g_strcmp0 (data->operation_name, "enroll") == 0)
    data->operation = OP_ENROLL;
  else
    return FALSE;

  if (g_strcmp0 (data->expect_name, "match") == 0)
    data->expect_match = TRUE;
  else if (g_strcmp0 (data->expect_name, "nonmatch") == 0)
    data->expect_match = FALSE;
  else
    return FALSE;

  return TRUE;
}

int
main (int argc, char **argv)
{
  g_autoptr (SmokeData) data = g_new0 (SmokeData, 1);
  g_autoptr (GOptionContext) options = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *persistent = NULL;
  gsize persistent_size = 0;
  GPtrArray *devices;
  struct stat persistent_stat;
  guint found = 0;
  gint exit_status;
  GOptionEntry entries[] = {
    { "persistent", 0, 0, G_OPTION_ARG_FILENAME, &data->persistent_path,
      "Mode-0600 libfprint persistent pairing file", "PATH" },
    { "operation", 0, 0, G_OPTION_ARG_STRING, &data->operation_name,
      "list, identify, verify, cancel-recover, remove, or enroll", "NAME" },
    { "expect", 0, 0, G_OPTION_ARG_STRING, &data->expect_name,
      "Expected biometric result: match or nonmatch", "RESULT" },
    { "print-index", 0, 0, G_OPTION_ARG_INT, &data->print_index,
      "Zero-based stored print index for verify", "N" },
    { "finger", 0, 0, G_OPTION_ARG_STRING, &data->finger_name,
      "Stored finger label for verify (right-little)", "NAME" },
    { "cancel-after-ms", 0, 0, G_OPTION_ARG_INT, &data->cancel_after_ms,
      "Cancellation delay for cancel-recover", "MS" },
    { "approval-token", 0, 0, G_OPTION_ARG_STRING, &data->approval_token,
      "Exact approval token required for enrollment", "TOKEN" },
    { NULL }
  };

  data->operation_name = g_strdup ("list");
  data->expect_name = g_strdup ("match");
  data->print_index = -1;
  data->cancel_after_ms = 1200;
  options = g_option_context_new ("- private VeriMark libfprint smoke test");
  g_option_context_add_main_entries (options, entries, NULL);
  if (!g_option_context_parse (options, &argc, &argv, &error) ||
      !data->persistent_path || !parse_operation (data))
    {
      g_printerr ("Invalid arguments: %s\n",
                  error ? error->message : "check --help");
      return EXIT_FAILURE;
    }
  if (data->operation == OP_ENROLL &&
      g_strcmp0 (data->approval_token, ENROLL_APPROVAL_TOKEN) != 0)
    {
      g_printerr ("Enrollment requires the exact approval token.\n");
      return EXIT_FAILURE;
    }
  if (data->finger_name)
    {
      if (data->operation != OP_VERIFY || data->print_index >= 0 ||
          g_strcmp0 (data->finger_name, "right-little") != 0)
        {
          g_printerr ("--finger right-little is valid only for verify and "
                      "cannot be combined with --print-index.\n");
          return EXIT_FAILURE;
        }
      data->selected_finger = FP_FINGER_RIGHT_LITTLE;
    }

  if (g_stat (data->persistent_path, &persistent_stat) != 0 ||
      !S_ISREG (persistent_stat.st_mode) ||
      (persistent_stat.st_mode & 0777) != 0600)
    {
      g_printerr ("Persistent pairing data must be a regular mode-0600 file.\n");
      return EXIT_FAILURE;
    }
  if (!g_file_get_contents (data->persistent_path, &persistent,
                            &persistent_size, &error))
    {
      g_printerr ("Cannot read persistent pairing data: %s\n", error->message);
      return EXIT_FAILURE;
    }

  data->context = fp_context_new ();
  devices = fp_context_get_devices (data->context);
  for (guint i = 0; devices && i < devices->len; i++)
    {
      FpDevice *candidate = g_ptr_array_index (devices, i);

      if (g_strcmp0 (fp_device_get_driver (candidate), "synatlsmoc") == 0)
        {
          data->device = candidate;
          found++;
        }
    }
  if (found != 1)
    {
      g_printerr ("Expected exactly one synatlsmoc device, found %u.\n", found);
      return EXIT_FAILURE;
    }
  if (!fp_device_set_persistent_data (data->device, (guchar *) persistent,
                                      persistent_size, &error))
    {
      g_printerr ("Persistent pairing data was rejected: %s\n", error->message);
      return EXIT_FAILURE;
    }

  data->loop = g_main_loop_new (NULL, FALSE);
  data->cancellable = g_cancellable_new ();
  data->sigint_source = g_unix_signal_add_full (G_PRIORITY_HIGH, SIGINT,
                                                sigint_cb, data, NULL);
  fp_device_open (data->device, data->cancellable,
                  (GAsyncReadyCallback) on_device_opened, data);
  g_main_loop_run (data->loop);

  exit_status = data->success ? EXIT_SUCCESS : EXIT_FAILURE;
  return exit_status;
}
