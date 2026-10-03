/*
 * Test: intuition/display_alert
 * DisplayAlert() return values for recovery and dead-end alerts.
 *
 * On AmigaOS, DisplayAlert() blocks until the user presses a mouse button
 * (left = continue, right = reboot), so this program is not run unattended
 * on the reference (tests/ref_suite.yaml, interactive).  Headless lxa
 * answers a recovery alert with "continue" and a dead-end alert with FALSE.
 */

#include <exec/types.h>
#include <exec/alerts.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;
    Write(Output(), (CONST APTR)s, len);
}

static UBYTE alert_text[] = {
    0x00, 0x20, 0x10,   /* x (WORD), y (BYTE) */
    'T', 'e', 's', 't', ' ', 'a', 'l', 'e', 'r', 't', 0,
    0
};

int main(void)
{
    int errors = 0;

    print("Testing DisplayAlert()...\n");

    if (DisplayAlert(RECOVERY_ALERT, alert_text, 40) == TRUE) {
        print("  OK: DisplayAlert returns continue for recovery alerts\n");
    } else {
        print("  FAIL: DisplayAlert did not return continue for recovery alerts\n");
        errors++;
    }

    if (DisplayAlert(DEADEND_ALERT, alert_text, 40) == FALSE) {
        print("  OK: DisplayAlert returns FALSE for dead-end alerts\n");
    } else {
        print("  FAIL: DisplayAlert did not return FALSE for dead-end alerts\n");
        errors++;
    }

    if (errors == 0) {
        print("PASS: display_alert all tests completed\n");
        return 0;
    }
    print("FAIL: display_alert had errors\n");
    return 20;
}
