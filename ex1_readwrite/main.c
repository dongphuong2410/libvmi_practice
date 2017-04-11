#include <stdio.h>
#include <stdlib.h>
#include <libvmi/libvmi.h>

int main(int argc, char **argvs)
{
    char *name = NULL;
    int PID;
    int stack_address;
    int heap_address = 0, test_value = 0;
    //Read the VM name
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <name of VM> <PID> <Stack Address>\n", argvs[0]);
        exit(1);
    }
    name = argvs[1];
    PID = atoi(argvs[2]);
    stack_address = (int)strtol(argvs[3], NULL, 0);

    vmi_instance_t vmi = NULL;
    //Initialize the libvmi library
    if (vmi_init(&vmi, VMI_XEN | VMI_INIT_COMPLETE, name) == VMI_FAILURE) {
        fprintf(stderr, "Failed to init LibVMI library.\n");
        if (vmi) {
            vmi_destroy(vmi);
        }
        return 1;
    }

    //Read the heap address and test value
    vmi_read_addr_va(vmi, stack_address, PID, (addr_t *)&heap_address);
    vmi_read_32_va(vmi, heap_address, PID, &test_value);

    printf("Heap address %#08x\n", heap_address);
    printf("Test value %d\n", test_value);
    printf("Modify test value to 15\n");

    test_value = 15;
    vmi_write_32_va(vmi, heap_address, PID, &test_value);
    vmi_destroy(vmi);
    return 0;
}
