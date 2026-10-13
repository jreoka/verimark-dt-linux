/* sensor-format: full on-chip fingerprint database format for the
 * Kensington VeriMark DT (047d:00f2) via the synatlsmoc libfprint driver.
 *
 * Calls the driver's clear_storage path, which sends VCSFW_CMD_DB2_FORMAT
 * (0xA5) — the Linux equivalent of the Windows vendor erase
 * (IOCTL_BIOMETRIC_SYNA_ERASE_DB). Wipes every template stored on the
 * sensor. Needs the pairing file (SYNA_TLSMOC_PERSISTENT_DATA_FILE) and
 * the patched libfprint (LD_LIBRARY_PATH) — both set by
 * scripts/sensor-format.sh. Run as root.
 *
 * Build: gcc sensor-format.c -o sensor-format \
 *   $(pkg-config --cflags glib-2.0 gobject-2.0 gio-2.0) \
 *   -I vendor/libfprint -I vendor/libfprint/.build/libfprint \
 *   -L vendor/libfprint/.build/libfprint -lfprint-2 \
 *   $(pkg-config --libs glib-2.0 gobject-2.0 gio-2.0)
 */
#include <stdio.h>

#include <libfprint/fprint.h>

int
main (void)
{
  g_autoptr (FpContext) ctx = fp_context_new ();
  GPtrArray *devices = fp_context_get_devices (ctx);
  FpDevice *target = NULL;

  for (guint i = 0; i < devices->len; i++)
    {
      FpDevice *dev = g_ptr_array_index (devices, i);
      const gchar *driver = fp_device_get_driver (dev);
      if (driver && g_strcmp0 (driver, "synatlsmoc") == 0)
        {
          target = dev;
          break;
        }
    }

  if (target == NULL)
    {
      fprintf (stderr, "no synatlsmoc (VeriMark DT) device found\n");
      return 1;
    }

  printf ("found: %s (driver %s)\n",
          fp_device_get_device_id (target),
          fp_device_get_driver (target));

  g_autoptr (GError) error = NULL;
  if (!fp_device_open_sync (target, NULL, &error))
    {
      fprintf (stderr, "open failed: %s\n", error->message);
      return 1;
    }

  printf ("sending DB2_FORMAT (full sensor database format)...\n");
  if (!fp_device_clear_storage_sync (target, NULL, &error))
    {
      fprintf (stderr, "format failed: %s\n", error->message);
      fp_device_close_sync (target, NULL, NULL);
      return 1;
    }

  printf ("sensor database formatted.\n");
  if (!fp_device_close_sync (target, NULL, &error))
    {
      fprintf (stderr, "close failed: %s\n", error->message);
      return 1;
    }

  return 0;
}
