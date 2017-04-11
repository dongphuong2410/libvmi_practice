#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <json-c/json.h>
#include <libvmi/libvmi.h>
#include "util.h"

uint64_t util_find_function_rva(const char *rekall_profile, const char *function)
{
    json_object *root = json_object_from_file(rekall_profile);
    if (!root) {
        fprintf(stderr, "Rekall profile couldn't be opened!\n");
        goto err_exit;
    }
    json_object *functions = NULL, *jsymbol = NULL;
    if (!json_object_object_get_ex(root, "$FUNCTIONS", &functions)) {
        fprintf(stderr, "Rekall profile: no $FUNCTIONS section found\n");
        goto err_exit;
    }
    if (!json_object_object_get_ex(functions, function, &jsymbol)) {
        fprintf(stderr, "Rekall profile: no '%s' found\n", function);
        goto err_exit;
    }
    uint64_t rva =  json_object_get_int64(jsymbol);
    json_object_put(root);
    return rva;
err_exit:
    if (root)
        json_object_put(root);
    return 0;
}

uint64_t util_find_constant_rva(const char *rekall_profile, const char *constant)
{
    json_object *root = json_object_from_file(rekall_profile);
    if (!root) {
        fprintf(stderr, "Rekall profile cound't be opened!\n");
        goto err_exit;
    }
    json_object *constants = NULL, *jsymbol = NULL;
    if (!json_object_object_get_ex(root, "$CONSTANTS", &constants)) {
        fprintf(stderr, "Rekall profile : no $CONSTANTS section found\n");
        goto err_exit;
    }
    if (!json_object_object_get_ex(constants, constant, &jsymbol)) {
        fprintf(stderr, "Rekall profile : no '%s' found\n", constant);
        goto err_exit;
    }
    uint64_t rva = json_object_get_int64(jsymbol);
    json_object_put(root);
    return rva;
err_exit:
    if (root)
        json_object_put(root);
    return 0;
}

uint64_t util_find_kernbase(vmi_instance_t vmi, const char *rekall_profile)
{
    uint64_t sysproc_rva;
    uint64_t sysproc = vmi_translate_ksym2v(vmi, "PsInitialSystemProcess");
    if (!sysproc) {
        printf("LibVMI failed to get the VA of PsInitialSystemProcess!\n");
        return 0;
    }
    sysproc_rva = util_find_constant_rva(rekall_profile, "PsInitialSystemProcess");
    if (!sysproc_rva) {
        fprintf(stderr, "Failed to get PsInitialSystemProcess RVA from Rekall profile~\n");
        return 0;
    }
    return sysproc - sysproc_rva;
}
