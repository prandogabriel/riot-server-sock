#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shell.h"
#include "msg.h"
#include "net/emcute.h"
#include "net/ipv6/addr.h"
#include "thread.h"
#include <net/gnrc/ipv6.h>
#include <net/gnrc/icmpv6.h>
#include "net/gnrc/netif.h"
#include "net/gnrc/ipv6/nib.h"
#include "net/gnrc/rpl.h"
#include "net/ipv6/addr.h"
#include "net/af.h"
#include "net/protnum.h"
#include "net/sock/udp.h"
#include "xtimer.h"

#ifndef ADDR_IPV6
#define ADDR_IPV6 "fec0:affe::1"
#endif

#define BUF_SIZE (128U)
#define MSG_QUEUE (8U)

static msg_t server_queue[MSG_QUEUE];

void measure_latency(ipv6_addr_t *addr)
{
    char addr_str[IPV6_ADDR_MAX_STR_LEN]; // Adicionada a variável addr_str.

    gnrc_netif_t *netif = gnrc_netif_iter(NULL);
    xtimer_ticks32_t start, end;
    uint16_t id = 0;
    uint16_t seq = 0;
    uint8_t ttl = 64;
    size_t len = BUF_SIZE;

    start = xtimer_now();
    int res = gnrc_icmpv6_echo_send(netif, addr, id, seq, ttl, len);
    end = xtimer_now();

    if (res == 0) {
        printf("Latency to %s: %" PRIu32 " ms\n",
               ipv6_addr_to_str(addr_str, addr, IPV6_ADDR_MAX_STR_LEN),
               xtimer_usec_from_ticks(xtimer_diff(end, start)) / 1000);
    } else {
        printf("Failed to send ICMPv6 Echo Request to %s\n",
               ipv6_addr_to_str(addr_str, addr, IPV6_ADDR_MAX_STR_LEN));
    }
}

int cmd_measure_latency(int argc, char **argv)
{
    (void)argc;
    (void)argv;


    /* Measure latency to two different IP addresses */
    ipv6_addr_t ip1; //, ip2;
    ipv6_addr_from_str(&ip1, ADDR_IPV6);
    // ipv6_addr_from_str(&ip2, "2001:db8::2");
    // measure_latency(&ip1, DEFAULT_PORT);
    // measure_latency(&ip2, 12345);

    printf("start new measure\n");

    measure_latency(&ip1);

    return 0;
}

static const shell_command_t shell_commands[] = {
    {"measure_latency", "measure latency", cmd_measure_latency},
    {NULL, NULL, NULL}};

int main(void)
{
    puts("measure latency example\n");

    /* Set up the message queue for the server thread */
    msg_init_queue(server_queue, MSG_QUEUE);

    /* start shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
