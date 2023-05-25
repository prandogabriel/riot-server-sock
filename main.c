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
#include "my_ping.h"
#ifndef ADDR_IPV6
#define ADDR_IPV6 "fec0:affe::1"
#endif

#define BUF_SIZE (128U)
#define MSG_QUEUE (8U)

static msg_t server_queue[MSG_QUEUE];

int cmd_measure_latency(int argc, char **argv)
{
    (void)argc;
    (void)argv;


    /* Measure latency to two different IP addresses */
    ipv6_addr_t ip1; //, ip2;
    ipv6_addr_from_str(&ip1, ADDR_IPV6);

    printf("\nlatency %d \n", get_latency(ip1));
   
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
