#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shell.h"
#include "msg.h"
#include "net/ipv6/addr.h"
#include "thread.h"

#include "net/gnrc/ipv6.h"
#include "net/gnrc/icmpv6.h"
#include "xtimer.h"

#ifndef ADDR_IPV6
#define ADDR_IPV6 "2001:db8::1"
#endif

#define ICMPV6_ECHO_ID 12345

#define MSG_QUEUE (8U)
static msg_t server_queue[MSG_QUEUE];

void my_ping(ipv6_addr_t *addr)
{
    uint8_t payload[] = "RIOT";
    xtimer_ticks32_t start, end;
    int req;

    /* Send an ICMPv6 Echo Request and measure the time until we get a response */
    start = xtimer_now();
    gnrc_netif_t *netif = gnrc_netif_iter(NULL);
    uint16_t seq = 0;
    uint8_t ttl = 64;
    req = gnrc_icmpv6_echo_send(netif, addr, ICMPV6_ECHO_ID, seq, ttl, sizeof(payload));

    if (req < 0)
    {
        printf("Error sending ICMPv6 Echo Request\n");
        return;
    }

    printf("response %d\n", req);

    /* Wait for the ICMPv6 Echo Reply */
    /* Note: This is a placeholder - you will need to implement this part of the code to capture the reply */
    //  while (1)
    //{
    /* Check for ICMPv6 Echo Reply here */
    /* On receiving ICMPv6 Echo Reply */
    /* end = xtimer_now(); */
    /* break; */
    //}

    /* Compute and print latency */
    printf("Latency: %u ms\n", (unsigned int)xtimer_usec_from_ticks(xtimer_diff(end, start)) / 1000);
}

int cmd_my_ping(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("start ping");

    ipv6_addr_t ip;
    ipv6_addr_from_str(&ip, ADDR_IPV6);
    my_ping(&ip);

    return 0;
}

static const shell_command_t shell_commands[] = {
    {"my_ping", "my ping", cmd_my_ping},
    {NULL, NULL, NULL}};

int main(void)
{
    puts("my ping example\n");

    /* Set up the message queue for the server thread */
    msg_init_queue(server_queue, MSG_QUEUE);

    /* start shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
