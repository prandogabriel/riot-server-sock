#include "my_ping.h"
#include "bitfield.h"
#include "byteorder.h"
#include "msg.h"
#include "net/gnrc.h"
#include "net/gnrc/icmpv6.h"
#include "net/icmpv6.h"
#include "net/ipv6.h"
#include "net/utils.h"
#include "sched.h"
#include "shell.h"
#include "timex.h"
#include "unaligned.h"
#include "utlist.h"
#include "ztimer.h"
#include <limits.h>

#ifdef MODULE_LUID
#include "luid.h"
#endif
#ifdef MODULE_GNRC_IPV6_NIB
#include "net/gnrc/ipv6/nib/nc.h"
#endif

#define _SEND_NEXT_PING (0xEF48)
#define _PING_FINISH (0xEF49)

#define CKTAB_SIZE (64U * 8) /* 64 byte * 8 bit/byte */

// Funções e variáveis estáticas vão aqui...

static void _pinger(_ping_data_t *data);
static int _print_reply(gnrc_pktsnip_t *pkt, int corrupted, uint32_t rtt, void *ctx);
static int _finish(_ping_data_t *data);

static int measure_time = 0;

// Código de implementação restante aqui...
int get_latency(ipv6_addr_t ip)
{
  measure_time = 0;

  _ping_data_t data = {
      .netreg = GNRC_NETREG_ENTRY_INIT_PID(ICMPV6_ECHO_REP, thread_getpid()),
      .host = ip,
      .count = DEFAULT_COUNT,
      .tmin = UINT_MAX,
      .datalen = DEFAULT_DATALEN,
      .timeout = DEFAULT_TIMEOUT_USEC,
      .interval = DEFAULT_INTERVAL_USEC,
      .id = DEFAULT_ID,
  };
  int res;

  ztimer_acquire(ZTIMER_USEC);

  gnrc_netreg_register(GNRC_NETTYPE_ICMPV6, &data.netreg);

  _pinger(&data);
  do
  {
    msg_t msg;

    msg_receive(&msg);
    switch (msg.type)
    {
    case GNRC_NETAPI_MSG_TYPE_RCV:
    {
      gnrc_icmpv6_echo_rsp_handle(msg.content.ptr, data.datalen,
                                  _print_reply, &data);
      gnrc_pktbuf_release(msg.content.ptr);
      break;
    }
    case _SEND_NEXT_PING:
      _pinger(&data);
      break;
    case _PING_FINISH:
      goto finish;
    default:
      /* requeue wrong packets */
      msg_send(&msg, thread_getpid());
      break;
    }
  } while (data.num_recv < data.count);
finish:
  ztimer_remove(ZTIMER_USEC, &data.sched_timer);
  res = _finish(&data);
  gnrc_netreg_unregister(GNRC_NETTYPE_ICMPV6, &data.netreg);
  while (msg_avail() > 0)
  {
    msg_t msg;

    /* remove all remaining messages (likely caused by duplicates) */
    if ((msg_try_receive(&msg) > 0) &&
        (msg.type == GNRC_NETAPI_MSG_TYPE_RCV) &&
        (((gnrc_pktsnip_t *)msg.content.ptr)->type == GNRC_NETTYPE_ICMPV6))
    {
      gnrc_pktbuf_release(msg.content.ptr);
    }
    else
    {
      /* requeue other packets */
      msg_send(&msg, thread_getpid());
    }
  }
  ztimer_release(ZTIMER_USEC);

  if (res < 0)
  {
    return -1;
  }

  if (measure_time == 0)
  {
    return -1;
  }

  return measure_time / DEFAULT_COUNT;
}

// Código de implementação restante aqui...
int gnrc_icmpv6_ping(ipv6_addr_t ip)
{
  _ping_data_t data = {
      .netreg = GNRC_NETREG_ENTRY_INIT_PID(ICMPV6_ECHO_REP, thread_getpid()),
      .host = ip,
      .count = DEFAULT_COUNT,
      .tmin = UINT_MAX,
      .datalen = DEFAULT_DATALEN,
      .timeout = DEFAULT_TIMEOUT_USEC,
      .interval = DEFAULT_INTERVAL_USEC,
      .id = DEFAULT_ID,
  };
  int res;

  ztimer_acquire(ZTIMER_USEC);

  gnrc_netreg_register(GNRC_NETTYPE_ICMPV6, &data.netreg);
  _pinger(&data);
  do
  {
    msg_t msg;

    msg_receive(&msg);
    switch (msg.type)
    {
    case GNRC_NETAPI_MSG_TYPE_RCV:
    {
      gnrc_icmpv6_echo_rsp_handle(msg.content.ptr, data.datalen,
                                  _print_reply, &data);
      gnrc_pktbuf_release(msg.content.ptr);
      break;
    }
    case _SEND_NEXT_PING:
      _pinger(&data);
      break;
    case _PING_FINISH:
      goto finish;
    default:
      /* requeue wrong packets */
      msg_send(&msg, thread_getpid());
      break;
    }
  } while (data.num_recv < data.count);
finish:
  ztimer_remove(ZTIMER_USEC, &data.sched_timer);
  res = _finish(&data);
  gnrc_netreg_unregister(GNRC_NETTYPE_ICMPV6, &data.netreg);
  while (msg_avail() > 0)
  {
    msg_t msg;

    /* remove all remaining messages (likely caused by duplicates) */
    if ((msg_try_receive(&msg) > 0) &&
        (msg.type == GNRC_NETAPI_MSG_TYPE_RCV) &&
        (((gnrc_pktsnip_t *)msg.content.ptr)->type == GNRC_NETTYPE_ICMPV6))
    {
      gnrc_pktbuf_release(msg.content.ptr);
    }
    else
    {
      /* requeue other packets */
      msg_send(&msg, thread_getpid());
    }
  }
  ztimer_release(ZTIMER_USEC);
  return res;
}

static void _pinger(_ping_data_t *data)
{
  uint32_t timer;
  int res;

  /* schedule next event (next ping or finish) ASAP */
  if ((data->num_sent + 1) < data->count)
  {
    /* didn't send all pings yet - schedule next in data->interval */
    data->sched_msg.type = _SEND_NEXT_PING;
    timer = data->interval;
  }
  else
  {
    /* Wait for the last ping to come back.
     * data->timeout: wait for a response in milliseconds.
     * Affects only timeout in absence of any responses,
     * otherwise ping waits for two max RTTs. */
    data->sched_msg.type = _PING_FINISH;
    timer = data->timeout;
    if (data->num_recv)
    {
      /* approx. 2*tmax, in seconds (2 RTT) */
      timer = (data->tmax / (512UL * 1024UL)) * US_PER_SEC;
      if (timer == 0)
      {
        timer = 1U * US_PER_SEC;
      }
    }
  }
  ztimer_set_msg(ZTIMER_USEC, &data->sched_timer, timer, &data->sched_msg,
                 thread_getpid());
  bf_unset(data->cktab, (size_t)data->num_sent % CKTAB_SIZE);

  res = gnrc_icmpv6_echo_send(data->netif, &data->host, data->id,
                              data->num_sent++, data->hoplimit, data->datalen);
  switch (-res)
  {
  case 0:
    break;
  case ENOMEM:
    printf("error: packet buffer full\n");
    break;
  default:
    printf("error: %d\n", res);
    break;
  }
}

static int _print_reply(gnrc_pktsnip_t *pkt, int corrupted, uint32_t triptime, void *ctx)
{
  _ping_data_t *data = ctx;
  gnrc_pktsnip_t *netif = gnrc_pktsnip_search_type(pkt, GNRC_NETTYPE_NETIF);
  gnrc_pktsnip_t *ipv6 = gnrc_pktsnip_search_type(pkt, GNRC_NETTYPE_IPV6);
  gnrc_pktsnip_t *icmpv6 = gnrc_pktsnip_search_type(pkt, GNRC_NETTYPE_ICMPV6);

  ipv6_hdr_t *ipv6_hdr = ipv6->data;
  icmpv6_echo_t *icmpv6_hdr = icmpv6->data;

  kernel_pid_t if_pid = KERNEL_PID_UNDEF;
  int16_t rssi = GNRC_NETIF_HDR_NO_RSSI;
  int16_t truncated;

  if (netif)
  {
    gnrc_netif_hdr_t *netif_hdr = netif->data;
    if_pid = netif_hdr->if_pid;
    rssi = netif_hdr->rssi;
  }

  /* check if payload size matches expectation */
  truncated = (data->datalen + sizeof(icmpv6_echo_t)) - icmpv6->size;

  if (icmpv6_hdr->type != ICMPV6_ECHO_REP)
  {
    return -EINVAL;
  }

  char from_str[IPV6_ADDR_MAX_STR_LEN];
  const char *dupmsg = " (DUP!)";
  uint16_t recv_seq;

  /* not our ping */
  if (byteorder_ntohs(icmpv6_hdr->id) != data->id)
  {
    return -EINVAL;
  }
  if (!ipv6_addr_is_multicast(&data->host) &&
      !ipv6_addr_equal(&ipv6_hdr->src, &data->host))
  {
    return -EINVAL;
  }
  recv_seq = byteorder_ntohs(icmpv6_hdr->seq);
  ipv6_addr_to_str(&from_str[0], &ipv6_hdr->src, sizeof(from_str));

  if (gnrc_netif_highlander() || (if_pid == KERNEL_PID_UNDEF) ||
      !ipv6_addr_is_link_local(&ipv6_hdr->src))
  {
    printf("%u bytes from %s: icmp_seq=%u ttl=%u",
           (unsigned)icmpv6->size,
           from_str, recv_seq, ipv6_hdr->hl);
  }
  else
  {
    printf("%u bytes from %s%%%u: icmp_seq=%u ttl=%u",
           (unsigned)icmpv6->size,
           from_str, if_pid, recv_seq, ipv6_hdr->hl);
  }
  /* check if payload size matches */
  if (truncated)
  {
    printf(" truncated by %d byte", truncated);
  }
  /* check response for corruption */
  else if (corrupted >= 0)
  {
    printf(" corrupted at offset %u", corrupted);
  }
  if (rssi != GNRC_NETIF_HDR_NO_RSSI)
  {
    printf(" rssi=%" PRId16 " dBm", rssi);
  }
  /* we can only calculate RTT (triptime) if payload was large enough for
     a TX timestamp */
  if (triptime)
  {
    printf(" time=%lu.%03lu ms", (long unsigned)triptime / 1000,
           (long unsigned)triptime % 1000);

    measure_time += (long unsigned)triptime;

    data->tsum += triptime;
    if (triptime < data->tmin)
    {
      data->tmin = triptime;
    }
    if (triptime > data->tmax)
    {
      data->tmax = triptime;
    }
  }
  if (bf_isset(data->cktab, recv_seq % CKTAB_SIZE))
  {
    data->num_rept++;
  }
  else
  {
    bf_set(data->cktab, recv_seq % CKTAB_SIZE);
    data->num_recv++;
    dupmsg += 7;
  }

  puts(dupmsg);

  return 0;
}

static int _finish(_ping_data_t *data)
{
  unsigned long tmp, nrecv, ndup;

  tmp = data->num_sent;
  nrecv = data->num_recv;
  ndup = data->num_rept;
  printf("\n--- %s PING statistics ---\n"
         "%lu packets transmitted, "
         "%lu packets received, ",
         data->hostname, tmp, nrecv);
  if (ndup)
  {
    printf("%lu duplicates, ", ndup);
  }
  if (tmp > 0)
  {
    tmp = ((tmp - nrecv) * 100) / tmp;
  }
  printf("%lu%% packet loss\n", tmp);
  if (data->tmin != UINT_MAX)
  {
    unsigned tavg = data->tsum / (nrecv + ndup);
    printf("round-trip min/avg/max = %u.%03u/%u.%03u/%u.%03u ms\n",
           data->tmin / 1000, data->tmin % 1000,
           tavg / 1000, tavg % 1000,
           data->tmax / 1000, data->tmax % 1000);
  }
  /* if condition is true, exit with 1 -- 'failure' */
  return (nrecv == 0);
}

/** @} */
