#include <stdio.h>
#include <stdlib.h>

#include <xenctrl.h>
#include <libxl_utils.h>
#include <glib.h>

//src/xen_helper/xen_helper.h
typedef struct xen_interface {
    xc_interface *xc;
    libxl_ctx *xl_ctx;
    xentoollog_logger *xl_logger;
} xen_interface_t;

bool xen_init_interface(xen_interface_t **xen);
void xen_free_interface(xen_interface_t *xen);

int main(int argc, char **argvs)
{
    xen_interface_t *xen;
    int domId = atoi(argvs[1]);
    //src/libdrakvuf/drakvuf.c
    if (!xen_init_interface(&xen))
        goto err;
    int rc = xc_domain_pause(xen->xc, domId);
    sleep(15);
    //src/xen_helper/xen_helper.c:xen_force_resume
    do {
        xc_dominfo_t info = {0};
        if (1 == xc_domain_getinfo(xen->xc, domId, 1, &info) && info.domid == domId && info.paused)
            xc_domain_unpause(xen->xc, domId);
        else
            break;
    } while (1);
    return 0;
err:
    printf("ERROR\n");
}

//src/xen_helper/xen_helper.c
bool xen_init_interface(xen_interface_t **xen)
{
    *xen = g_malloc0(sizeof(xen_interface_t));
    (*xen)->xc = xc_interface_open(0,0,0);
    if ((*xen)->xc == NULL) {
        fprintf(stderr, "xc_interface_open() failed!\n");
        goto err;
    }
    (*xen)->xl_logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, XTL_PROGRESS, 0);
    if (!(*xen)->xl_logger)
    {
        goto err;
    }
    if (libxl_ctx_alloc(&(*xen)->xl_ctx, LIBXL_VERSION, 0, (*xen)->xl_logger)) {
        fprintf(stderr, "libxl_ctx_alloc() failed!\n");
        goto err;
    }
    return 1;
err:
    xen_free_interface(*xen);
    *xen = NULL;
    return 0;
}

void xen_free_interface(xen_interface_t *xen)
{
    if (xen) {
        if (xen->xl_ctx)
            libxl_ctx_free(xen->xl_ctx);
        if (xen->xl_logger)
            xtl_logger_destroy(xen->xl_logger);
        if (xen->xc)
            xc_interface_close(xen->xc);
        free(xen);
    }
}
