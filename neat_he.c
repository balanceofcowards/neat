#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <assert.h>

#include "neat.h"
#include "neat_internal.h"
#include "neat_property_helpers.h"

static void he_print_results(struct neat_resolver_results *results)
{
    struct neat_resolver_res *result;
    char addr_name[INET6_ADDRSTRLEN];
    char serv_name[6];

    fprintf(stderr, "Results:\n");
    LIST_FOREACH(result, results, next_res) {
        switch (result->ai_protocol) {
        case IPPROTO_UDP:
            fprintf(stderr, "UDP/");
            break;
        case IPPROTO_TCP:
            fprintf(stderr, "TCP/");
            break;
        case IPPROTO_SCTP:
            fprintf(stderr, "SCTP/");
            break;
        default:
            fprintf(stderr, "proto%d/", result->ai_protocol);
            break;
        }
        switch (result->ai_family) {
        case AF_INET:
            fprintf(stderr, "IPv4");
            break;
        case AF_INET6:
            fprintf(stderr, "IPv6");
            break;
        default:
            fprintf(stderr, "family%d", result->ai_family);
            break;
        }
        getnameinfo((struct sockaddr *)&result->src_addr, result->src_addr_len,
                    addr_name, sizeof(addr_name),
                    serv_name, sizeof(serv_name),
                    NI_NUMERICHOST | NI_NUMERICSERV);
        fprintf(stderr, ": %s:%s->", addr_name, serv_name);
        getnameinfo((struct sockaddr *)&result->dst_addr, result->dst_addr_len,
                    addr_name, sizeof(addr_name),
                    serv_name, sizeof(serv_name),
                    NI_NUMERICHOST | NI_NUMERICSERV);
        fprintf(stderr, "%s:%s\n", addr_name, serv_name);
    }
}

static void
he_resolve_cb(struct neat_resolver *resolver, struct neat_resolver_results *results, uint8_t code)
{
    neat_flow *flow = (neat_flow *)resolver->userData1;
    neat_he_callback_fx callback_fx;
    callback_fx = (neat_he_callback_fx) (neat_flow *)resolver->userData2;

    if (code != NEAT_RESOLVER_OK) {
        callback_fx(resolver->nc, (neat_flow *)resolver->userData1, code,
                    0, 0, 0, -1);
        return;
    }

    assert (results->lh_first);
    assert (!flow->resolver_results);

    //he_print_results(results);

    // right now we're just going to use the first address. Todo by HE folks
    flow->family = results->lh_first->ai_family;
    flow->sockType = results->lh_first->ai_socktype;
    flow->sockProtocol = results->lh_first->ai_protocol;
    flow->resolver_results = results;
    flow->sockAddr = (struct sockaddr *) &(results->lh_first->dst_addr);

    printf("Protocol %u\n", flow->sockProtocol);

    callback_fx(resolver->nc, (neat_flow *)resolver->userData1, NEAT_OK,
                flow->family, flow->sockType, flow->sockProtocol, -1);
}

neat_error_code neat_he_lookup(neat_ctx *ctx, neat_flow *flow, neat_he_callback_fx callback_fx)
{
    int protocols[NEAT_MAX_NUM_PROTO]; /* We only support SCTP, TCP, UDP, and UDPLite */
    uint8_t nr_of_protocols;
    uint8_t family;

    if ((flow->propertyMask & NEAT_PROPERTY_IPV4_REQUIRED) &&
        (flow->propertyMask & NEAT_PROPERTY_IPV4_BANNED))
        return NEAT_ERROR_UNABLE;
    if ((flow->propertyMask & NEAT_PROPERTY_IPV6_REQUIRED) &&
        (flow->propertyMask & NEAT_PROPERTY_IPV6_BANNED))
        return NEAT_ERROR_UNABLE;
    if ((flow->propertyMask & NEAT_PROPERTY_IPV4_BANNED) &&
        (flow->propertyMask & NEAT_PROPERTY_IPV6_BANNED))
        return NEAT_ERROR_UNABLE;
    if ((flow->propertyMask & NEAT_PROPERTY_IPV4_REQUIRED) &&
        (flow->propertyMask & NEAT_PROPERTY_IPV6_BANNED))
        family = AF_INET;
    else if ((flow->propertyMask & NEAT_PROPERTY_IPV6_REQUIRED) &&
             (flow->propertyMask & NEAT_PROPERTY_IPV4_BANNED))
        family = AF_INET6;
    else
        family = AF_UNSPEC; /* AF_INET and AF_INET6 */

    nr_of_protocols = neat_property_translate_protocols(flow->propertyMask,
            protocols);
    if (nr_of_protocols == 0)
        return NEAT_ERROR_UNABLE;

    if (!ctx->resolver) {
        ctx->resolver = neat_resolver_init(ctx, he_resolve_cb, NULL);
    }
    ctx->resolver->userData1 = (void *)flow; // todo this doesn't allow multiple sockets
    ctx->resolver->userData2 = callback_fx;

    /* FIXME: derivation of the socket type is wrong.
     * FIXME: Make use of the array of protocols
     */
    neat_getaddrinfo(ctx->resolver, family, flow->name, flow->port,
            protocols, nr_of_protocols);

    return NEAT_OK;
}