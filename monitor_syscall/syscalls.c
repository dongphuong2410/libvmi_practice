#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <libvmi/libvmi.h>
#include <libvmi/events.h>

#include "util.h"

/*
 * In this demo, the goal is to write a software breakpoint instruction
 * into the beginning a Windows system call handler. Software breakpoints
 * can be configured to rap to the hypervisor and thus can be hidden from
 * the executing guest.
 * When we receive the breakpoint event, we can remove it, place back the
 * original instruction we overwrote so the guest continues as if nothing
 * happened. Further enabling singlestepping will allow you to re-trap the
 * system call handler
 */

static uint8_t trap = 0xCC;
static uint8_t backup_byte;
static int i;
char *syscall_name;
uint64_t syscall_addr;

/* Signal handler */
static struct sigaction act;
static int interrupted = 0;
static void close_handler(int sig) {
    interrupted = sig;
}

event_response_t singlestep_cb(vmi_instance_t vmi, vmi_event_t *event) {
    //STEP 9: write trap again and disable singlestepping
    vmi_write_8_va(vmi, syscall_addr, 0, &trap);
    event_response_t rsp = 0;
    return rsp | VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
}

event_response_t trap_cb(vmi_instance_t vmi, vmi_event_t *event) {
    printf("Received a trap event for syscall %s!\n", syscall_name);

    //STEP 7 : replace trap with backup byte and specify reinjection behavior
    vmi_write_8_va(vmi, syscall_addr, 0, &backup_byte);
    event->interrupt_event.reinject = 0;
    //STEP 8 : turn on single stepping
    event_response_t rsp = 0;
    return rsp | VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: %s <domain name> <syscall_name>\n", argv[0]);
        return 1;
    }

    const char *name = argv[1];
    syscall_name = argv[2];
    char *rekall_profile = "/root/windows.json";
    uint64_t syscall_va;

    /* Signal handler to catch CTRL+C, etc .. */
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    //STEP 1: initialize LibVMI
    vmi_instance_t vmi = NULL;
    if (VMI_FAILURE == vmi_init(&vmi, VMI_XEN | VMI_INIT_COMPLETE | VMI_INIT_EVENTS, name)) {
        fprintf(stderr, "Failed to init LibVMI library.\n");
        if (vmi) {
            vmi_destroy(vmi);
        }
        return 1;
    }

    //STEP 2 : backup first byte of a syscall and write trap
    // You can back up the first byte into backup_byte.
    uint64_t rva = util_find_function_rva(rekall_profile, syscall_name);
    uint64_t kernbase = util_find_kernbase(vmi, rekall_profile);
    syscall_addr = kernbase + rva;
    if (syscall_addr != 0) {
        printf("Syscall %s is located at VA 0x%lx\n", syscall_name, syscall_addr);
        vmi_read_8_va(vmi, syscall_addr, 0, &backup_byte);
        vmi_write_8_va(vmi, syscall_addr, 0, &trap);
    }

    vmi_event_t trap_event, singlestep_event;
    //STEP 3 : enable listening for interrupt events
    SETUP_INTERRUPT_EVENT(&trap_event, 0, trap_cb);
    //STEP 4 : enable listening for singlestep events BUT don't yet turn on singlestepping
    SETUP_SINGLESTEP_EVENT(&singlestep_event, 1, singlestep_cb, 0);
    vmi_register_event(vmi, &trap_event);
    vmi_register_event(vmi, &singlestep_event);

    //STEP 5 : start events loop
    while (!interrupted) {
        vmi_events_listen(vmi, 500);
    }

    //STEP 6 : remove traps and close LibVMI
    vmi_write_8_va(vmi, syscall_addr, 0, &backup_byte);
    vmi_clear_event(vmi, &trap_event, NULL);
    vmi_clear_event(vmi, &singlestep_event, NULL);
    vmi_destroy(vmi);
    return 0;
}
