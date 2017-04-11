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

#define POOLTAG_FILE "Fil\xe5"

static uint8_t trap = 0xCC;
static uint8_t backup_byte;
static uint8_t backup_ret_byte;
static uint8_t ret_trap_set = 0;
static int i;
char *syscall_name;
uint64_t syscall_va;
uint64_t ret_pa;

/* Signal handler */
static struct sigaction act;
static int interrupted = 0;
static void close_handler(int sig) {
    interrupted = sig;
}

event_response_t singlestep_cb(vmi_instance_t vmi, vmi_event_t *event) {
    uint64_t pa = (event->interrupt_event.gfn << 12) + event->interrupt_event.offset + event->interrupt_event.insn_length - 1;
    //if (pa == ret_pa)
    //    vmi_write_8_pa(vmi, ret_pa, &trap);
    //else
        vmi_write_8_va(vmi, syscall_va, 0, &trap);
    event_response_t rsp = 0;
    return rsp | VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
}

event_response_t trap_cb(vmi_instance_t vmi, vmi_event_t *event) {
    uint64_t pa = (event->interrupt_event.gfn << 12) + event->interrupt_event.offset + event->interrupt_event.insn_length - 1;
    //if (pa == ret_pa) {
        //printf("Heap Allocation Ret\n");
        //vmi_write_8_va(vmi, ret_pa, 0, &backup_ret_byte);
    //}
    //else {
        //Check if this is allocated for FILE
        //If it is, insert the trap at the return point of this function, if it is not yet set
        uint64_t tag = 0, size = 0;
        access_context_t ctx;
        ctx.translate_mechanism = VMI_TM_PROCESS_DTB;
        ctx.dtb = event->x86_regs->cr3;
        if (vmi_get_page_mode(vmi) == VMI_PM_IA32E) {
            size = event->x86_regs->rdx;
            tag = event->x86_regs->r8;
        }
        else {
            ctx.addr = event->x86_regs->rsp+8;
            if (VMI_FAILURE == vmi_read_32(vmi, &ctx, (uint32_t*)&size))
                return 0;
            ctx.addr = event->x86_regs->rsp+12;
            if (VMI_FAILURE == vmi_read_32(vmi, &ctx, (uint32_t*)&tag))
                return 0;
        }
        if (!memcmp(&tag, &POOLTAG_FILE, 4)) {
            if (!ret_trap_set) {
                printf("POOLTAG_FILE\n");
                uint64_t ret;
                ctx.addr = event->x86_regs->rsp;
                if (VMI_FAILURE == vmi_read_addr(vmi, &ctx, &ret))
                    return 0;
                ret_pa = vmi_pagetable_lookup(vmi, event->x86_regs->cr3, ret);
                ret_trap_set = 1;
                //Set breakpoint for HeapRetTrap
                //vmi_read_8_pa(vmi, ret_pa, &backup_ret_byte);
                //vmi_write_8_pa(vmi, ret_pa, &trap);
            }
        }
        vmi_write_8_va(vmi, syscall_va, 0, &backup_byte);
    //}
    event->interrupt_event.reinject = 0;
    event_response_t rsp = 0;
    return rsp | VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s <domain name>\n", argv[0]);
        return 1;
    }

    const char *name = argv[1];
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

    //STEP 2 : Add a trap to ExAllocatePoolWithTag function
    syscall_name = "ExAllocatePoolWithTag";
    uint64_t rva = util_find_function_rva(rekall_profile, syscall_name);
    uint64_t kernbase = util_find_kernbase(vmi, rekall_profile);
    syscall_va = kernbase + rva;
    if (syscall_va != 0) {
        vmi_read_8_va(vmi, syscall_va, 0, &backup_byte);
        vmi_write_8_va(vmi, syscall_va, 0, &trap);
    }

    //STEP 3 : register trap event and single step event (single step event is not enabled yet)
    vmi_event_t trap_event, singlestep_event;
    SETUP_INTERRUPT_EVENT(&trap_event, 0, trap_cb);
    SETUP_SINGLESTEP_EVENT(&singlestep_event, 1, singlestep_cb, 0);
    vmi_register_event(vmi, &trap_event);
    vmi_register_event(vmi, &singlestep_event);

    //STEP 4 : Listen to the event
    while (!interrupted) {
        vmi_events_listen(vmi, 500);
    }

    vmi_write_8_va(vmi, syscall_va, 0, &backup_byte);
    vmi_clear_event(vmi, &trap_event, NULL);
    vmi_clear_event(vmi, &singlestep_event, NULL);
    vmi_destroy(vmi);
    return 0;
}
