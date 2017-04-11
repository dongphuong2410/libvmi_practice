#include <stdio.h>
#include <stdlib.h>
#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <signal.h>
#include <inttypes.h>

reg_t cr3, rip;
vmi_pid_t pid;
vmi_event_t cr3_event;
vmi_event_t mm_event;

addr_t rip_pa;
int mm_enabled;

static int interrupted = 0;
static void close_handler(int sig) {
    interrupted = sig;
}

event_response_t cr3_callback(vmi_instance_t vmi, vmi_event_t *event) {
    vmi_pid_t current_pid = vmi_dtb_to_pid(vmi, event->reg_event.value);
    printf("PID %i with CR3=%"PRIx64" executing on vcpu %u.\n", current_pid, event->reg_event.value, event->vcpu_id);

    if (current_pid == pid) {
        if (!mm_enabled) {
            mm_enabled = 1;
            printf("-- Enabling mem-event\n");
            vmi_register_event(vmi, &mm_event);
        }
        else {
            if (mm_enabled) {
                mm_enabled = 0;
                printf(" -- Disabling mem-event\n");
                vmi_clear_event(vmi, &mm_event, NULL);
            }
        }
    }
    return 0;
}

event_response_t mm_callback(vmi_instance_t vmi, vmi_event_t *event) {
    vmi_get_vcpureg(vmi, &cr3, CR3, 0);
    vmi_pid_t current_pid = vmi_dtb_to_pid(vmi, cr3);

    reg_t rip_test;
    vmi_get_vcpureg(vmi, &rip_test, RIP, 0);
    printf("Memevent: {\n\tPID %d. RIP 0x%lx:\n", current_pid, rip_test);
}

int main(int argc, char **argvs)
{
    vmi_instance_t vmi = NULL;
    status_t status  = VMI_SUCCESS;
    struct sigaction act;

    mm_enabled = 0;
    char *name = NULL;

    //Read the VM name
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <name of VM>\n", argvs[0]);
        exit(1);
    }
    name = argvs[1];

    //Handle signal
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    //Initialize the libvmi library
    if (vmi_init(&vmi, VMI_XEN | VMI_INIT_COMPLETE | VMI_INIT_EVENTS, name) == VMI_FAILURE) {
        printf("Failed to init LibVMI library.\n");
        if (vmi != NULL) {
            vmi_destroy(vmi);
        }
        return 1;
    }
    else {
        printf("LibVMI init succeed!\n");
    }
    vmi_pause_vm(vmi);

    //VMI_REGACCESS_W : type of access,  W maybe means Write
    //0 : reg_event.equal : Event filtter : callback triggers IFF register == value
    //CR3 : page directory base register (PDBR)
    SETUP_REG_EVENT(&cr3_event, CR3, VMI_REGACCESS_W, 0, cr3_callback);
    vmi_register_event(vmi, &cr3_event);

    //Setup a mem event for tracking memory at the current instruction's page
    //But don't install it; that will be done by the cr3 handler
    vmi_get_vcpureg(vmi, &rip, RIP, 0); //vmi_get_vcpureg get the current value of a VCPU register, this currently only supports control registers)
    vmi_get_vcpureg(vmi, &cr3, CR3, 0);

    printf("Current value of RIP is 0x%lx\n", rip);
    rip -= 0x1;     //??? get the instruction befor or after this instruction ??

    pid = vmi_dtb_to_pid(vmi, cr3);
    printf("current pid %d\n", pid);
    if (pid == 4) {
        rip_pa = vmi_translate_uv2p(vmi, rip, pid);
    }
    else {
        rip_pa = vmi_translate_kv2p(vmi, rip);
    }

    printf("Preparing memory event to catch next RIP 0x%lx, PA 0x%lx, page 0x%lx for PID %u\n",
            rip, rip_pa, rip_pa >> 12, pid);
    SETUP_MEM_EVENT(&mm_event, rip_pa, VMI_MEMACCESS_X, mm_callback, 0);

    vmi_resume_vm(vmi);

    int count = 20;
    while (!interrupted && count--) {
        status = vmi_events_listen(vmi, 500);
        if (status != VMI_SUCCESS) {
            printf("Error waiting for events, quitting ...\n");
            interrupted = -1;
        }
    }
    printf("Finished with test.\n");

    //Clean up
    vmi_destroy(vmi);

    return 0;
}
