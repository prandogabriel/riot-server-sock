#include "net/gnrc.h"
#include "net/gnrc/icmpv6.h"
#include "net/ipv6.h"
#include "ztimer.h"
#include "bitfield.h"

#define DEFAULT_COUNT           (3U)
#define DEFAULT_DATALEN         (sizeof(uint32_t))
#define DEFAULT_ID              (0x53)
#define DEFAULT_INTERVAL_USEC   (1U * US_PER_SEC)
#define DEFAULT_TIMEOUT_USEC    (1U * US_PER_SEC)
#define CKTAB_SIZE              (64U * 8)   /* 64 byte * 8 bit/byte */

typedef struct {
    gnrc_netreg_entry_t netreg;
    ztimer_t sched_timer;
    msg_t sched_msg;
    ipv6_addr_t host;
    char *hostname;
    unsigned long num_sent, num_recv, num_rept;
    unsigned long long tsum;
    unsigned tmin, tmax;
    unsigned count;
    size_t datalen;
    BITFIELD(cktab, CKTAB_SIZE);
    uint32_t timeout;
    uint32_t interval;
    gnrc_netif_t *netif;
    uint16_t id;
    uint8_t hoplimit;
} _ping_data_t;

int gnrc_icmpv6_ping(ipv6_addr_t ip);
int get_latency(ipv6_addr_t ip);

