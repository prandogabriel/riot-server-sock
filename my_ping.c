#include "my_ping.h"
#include "bitfield.h"
#include "byteorder.h"
#include "msg.h"
#include "net/gnrc.h"
#include "net/gnrc/icmpv6.h"
#include "net/icmpv6.h"
#include "net/ipv6.h"
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
static int _finish(_ping_data_t *data);

// Código de implementação restante aqui...
uint32_t get_latency(ipv6_addr_t ip)
{
  uint32_t measure_time = 0;

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
      gnrc_pktsnip_t *ipv6, *icmpv6;
      uint32_t now = ztimer_now(ZTIMER_USEC);
      uint32_t triptime = 0;

      ipv6 = gnrc_pktsnip_search_type(msg.content.ptr, GNRC_NETTYPE_IPV6);
      icmpv6 = gnrc_pktsnip_search_type(msg.content.ptr, GNRC_NETTYPE_ICMPV6);
      if ((ipv6 == NULL) || (icmpv6 == NULL))
      {
       break;
      }
      //gnrc_icmpv6_echo_rsp_handle;
      //cmpv6_echo_t *icmpv6_hdr = icmpv6->data;
      icmpv6_echo_t *icmpv6_hdr = icmpv6->data;
      void *buf= icmpv6_hdr + 1;
      triptime = now - unaligned_get_u32(buf);
      //_check_payload(icmpv6_hdr + 1, data.datalen, now, &triptime, &corrupted);

      gnrc_pktbuf_release(msg.content.ptr);

      measure_time += triptime;
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
  if (msg_avail() > 0)
  {
    msg_t msg;

    /* remove all remaining messages (likely caused by duplicates) */
    if ((msg_try_receive(&msg) > 0) &&
        (msg.type == GNRC_NETAPI_MSG_TYPE_RCV) &&
        (((gnrc_pktsnip_t *)msg.content.ptr)->type == GNRC_NETTYPE_ICMPV6))
    {
      gnrc_pktbuf_release(msg.content.ptr);
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

static int _finish(_ping_data_t *data)
{
  unsigned long nrecv;

  nrecv = data->num_recv;
  /* if condition is true, exit with 1 -- 'failure' */
  return (nrecv == 0);
}

/** @} */
