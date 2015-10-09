#include <stdlib.h>
#include <uv.h>

#include "include/queue.h"
#include "neat_core.h"

#ifdef LINUX
    #include "neat_linux.h"
#endif

struct neat_ctx *neat_alloc_ctx()
{
    struct neat_ctx *nc = NULL;

#ifdef LINUX
    nc = (struct neat_ctx*) neat_alloc_ctx_linux();
#endif

    if (nc == NULL)
        return NULL;

    nc->loop = malloc(sizeof(uv_loop_t));

    if (nc->loop == NULL) {
        free(nc);
        return NULL;
    }

    uv_loop_init(nc->loop);
    LIST_INIT(&(nc->src_addrs));
    return nc;
}

void neat_start_event_loop(struct neat_ctx *nc)
{
    uv_run(nc->loop, UV_RUN_DEFAULT);
    uv_loop_close(nc->loop);
}

void neat_free_ctx(struct neat_ctx *nc)
{
    if (nc->cleanup)
        nc->cleanup(nc);

    free(nc->loop);
    free(nc);
}
