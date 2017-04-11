#ifndef REKALL_H
#define REKALL_H

uint64_t util_find_function_rva(const char *rekall_profile, const char *function);
uint64_t util_find_constant_rva(const char *rekall_profile, const char *constant);
uint64_t util_find_kernbase(vmi_instance_t vmi, const char *rekall_profile);

#endif
