#include <signal.h>
#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <inttypes.h>

/* Signal handler */
static struct sigaction act;
static int interrupted = 0;
static void close_handler(int sig) {
    interrupted = sig;
}

event_response_t cr3_callback(vmi_instance_t vmi, vmi_event_t *event) {
    //TODO 5 : find the PID of the current process being scheduled
    vmi_pid_t pid = vmi_dtb_to_pid(vmi, event->reg_event.value);
    printf("CR3 callback PID %"PRIi32"\n", pid);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <domain name>\n", argv[0]);
        return 1;
    }

    const char *name = argv[1];

    /* Signal handler */
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    //TODO 1 : initialize LibVMI
    vmi_instance_t vmi = NULL;
    if (VMI_FAILURE == vmi_init(&vmi, VMI_XEN | VMI_INIT_COMPLETE | VMI_INIT_EVENTS, name) == VMI_FAILURE) {
        fprintf(stderr, "Failed to init LibVMI library.\n");
        if (vmi) {
            vmi_destroy(vmi);
        }
        return 1;
    }

    //TODO 2 : initialize CR3 event and register it with LibVMI
    vmi_event_t cr3_event;
    SETUP_REG_EVENT(&cr3_event, CR3, VMI_REGACCESS_W, 0, cr3_callback);
    vmi_register_event(vmi, &cr3_event);

    while (!interrupted) {
        //TODO 3 : run LibVMI events loop
        vmi_events_listen(vmi, 500);
    }

    //TODO 4 : remove event and close LibVMI
    vmi_clear_event(vmi, &cr3_event, NULL);
    vmi_destroy(vmi);
    return 0;
}
