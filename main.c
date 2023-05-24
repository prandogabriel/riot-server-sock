#include "msg.h"
#include "net/af.h"
#include "net/protnum.h"
#include "net/sock/udp.h"
#include "xtimer.h"

#ifndef ADDR_IPV6
#define ADDR_IPV6 "2001:db8::1"
#endif

#define BUF_SIZE    (128U)
#define MSG_QUEUE   (8U)
#define DEFAULT_PORT   (1885)

static msg_t server_queue[MSG_QUEUE];
static sock_udp_t sock;
static sock_udp_ep_t local = { .family = AF_INET6 };

void send_ping(sock_udp_t *sock, ipv6_addr_t *addr, uint16_t port) {
    uint8_t buf[1] = {0};
    sock_udp_ep_t remote = {.family = AF_INET6, .netif = SOCK_ADDR_ANY_NETIF, .port = port};
    memcpy(&remote.addr.ipv6, addr, sizeof(ipv6_addr_t));
    sock_udp_send(sock, buf, sizeof(buf), &remote);
}

void measure_latency(ipv6_addr_t *addr, uint16_t port) {
    xtimer_ticks32_t start, end;
    msg_t msg;
    char addr_str[IPV6_ADDR_MAX_STR_LEN];

    /* Send a ping and measure the time until we get a response */
    start = xtimer_now();
    send_ping(&sock, addr, port);

    while (1) {
        msg_receive(&msg);
        if (msg.type == GNRC_NETAPI_MSG_TYPE_RCV) {
            end = xtimer_now();
            printf("Latency to %s: %u ms\n", ipv6_addr_to_str(addr_str, addr, IPV6_ADDR_MAX_STR_LEN), (unsigned int) xtimer_usec_from_ticks(xtimer_diff(end, start)) / 1000);
            break;
        }
    }
}

int main(void) {
    /* Set up the message queue for the server thread */
    msg_init_queue(server_queue, MSG_QUEUE);

    /* Create the server socket */
    if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
        printf("Error creating server socket\n");
        return 1;
    }

    /* Measure latency to two different IP addresses */
    ipv6_addr_t ip1; //, ip2;
    ipv6_addr_from_str(&ip1, ADDR_IPV6);
    //ipv6_addr_from_str(&ip2, "2001:db8::2");
    measure_latency(&ip1, DEFAULT_PORT);
    //measure_latency(&ip2, 12345);

    /* Close the server socket */
    sock_udp_close(&sock);

    return 0;
}
