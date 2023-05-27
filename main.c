#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shell.h"
#include "msg.h"
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
#include "xtimer.h"
#include "net/sock/udp.h"

#include "my_ping.h"
#ifndef ADDR_IPV6
#define ADDR_IPV6 "fec0:affe::1"
#endif
#ifndef SERVER_SOCK_ADDR_IPV6
#define SERVER_SOCK_ADDR_IPV6 "2001:db8::1"
#endif

#define BUF_SIZE (128U)
#define MSG_QUEUE (8U)

static msg_t server_queue[MSG_QUEUE];

static void client_request(void)
{
    sock_udp_t sock;
    sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
    sock_udp_ep_t remote = {.family = AF_INET6, .port = 1234};
    uint8_t buf[128];
    ssize_t res;

    // Parse the server IP address
    if (ipv6_addr_from_str((ipv6_addr_t *)remote.addr.ipv6, SERVER_SOCK_ADDR_IPV6) == NULL)
    {
        printf("Error: unable to parse server address\n");
        return;
    }

    if (sock_udp_create(&sock, &local, &remote, 0) < 0)
    {
        printf("Error: unable to create socket\n");
        return;
    }

    // Send a request (empty message)
    if (sock_udp_send(&sock, "", 1, &remote) < 0)
    {
        printf("Error: unable to send data\n");
        return;
    }

    // Receive the response
    if ((res = sock_udp_recv(&sock, buf, sizeof(buf), 5 * US_PER_SEC, NULL)) >= 0)
    {
        printf("Received data: %.*s\n", (int)res, buf);
    }
    else
    {
        printf("Error: unable to receive data\n");
    }

    sock_udp_close(&sock);
}

int cmd_udp_client_request(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\nnew udp request \n");

    client_request();

    return 0;
}

int cmd_measure_latency(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Measure latency to two different IP addresses */
    ipv6_addr_t ip1; //, ip2;
    ipv6_addr_from_str(&ip1, ADDR_IPV6);

    printf("\nlatency %d \n", (int)get_latency(ip1));

    return 0;
}

static const shell_command_t shell_commands[] = {
    {"measure_latency", "measure latency", cmd_measure_latency},
    {"udp_request", "cmd_udp_client_request", cmd_udp_client_request},
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
