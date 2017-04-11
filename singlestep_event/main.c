#include <stdio.h>
#include <stdlib.h>
#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <inttypes.h>
#include <signal.h>

static int interrupted = 0;
static void close_handler(int sig) {
    interrupted = sig;
}

event_response_t single_step_callback(vmi_instance_t vmi, vmi_event_t *event) {
    printf("Single-step event: VCPU:%u GFN %"PRIx64" GLA %016"PRIx64"\n",
            event->vcpu_id,
            event->ss_event.gfn,        //Page number at which event occured
            event->ss_event.gla);       //Specific virtual address at which event occurred
    reg_t rip;
    vmi_get_vcpureg(vmi, &rip, RIP, event->vcpu_id);    //Gets the current value of a VCPU register: RIP : Instruction Pointer
    printf("\tRIP: %"PRIx64"\n", rip);
}

vmi_event_t single_event;

int main(int argc, char **argvs)
{
    vmi_instance_t vmi;
    struct sigaction act;
    char *name = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: single_step <name of VM> \n");
        exit(1);
    }
    name = argvs[1];

    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    //Initialize the libvmi library
    if (vmi_init(&vmi, VMI_XEN | VMI_INIT_PARTIAL | VMI_INIT_EVENTS, name) == VMI_FAILURE) {
        printf("Failed to init LibVMI library.\n");
        return 1;
    }
    else {
        printf("LibVMI init succeeded!\n");
    }

    //Single step setup
    memset(&single_event, 0, sizeof(vmi_event_t));
    single_event.version = VMI_EVENTS_VERSION;
    single_event.type = VMI_EVENT_SINGLESTEP;
    single_event.callback = single_step_callback;
    single_event.ss_event.enable = 1;
    SET_VCPU_SINGLESTEP(single_event.ss_event, 0);
    vmi_register_event(vmi, &single_event);
    int count = 10;
    while (!interrupted && count--) {
        printf("Waiting for events ...\n");
        vmi_events_listen(vmi, 500);
    }
    printf("Finished with test.\n");

    //Clean up
    vmi_destroy(vmi);
    return 0;
}
