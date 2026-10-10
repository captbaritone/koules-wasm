/*
 * sock_ws.c - sock.c reimplemented on top of a WebSocket transport.
 *
 * Koules' netcode talks UDP: server.c and client.c exchange datagrams
 * through the dozen entry points in sock.h and touch nothing else. A
 * browser cannot open a UDP socket, so this file replaces sock.c and
 * leaves server.c, client.c and objectsio.c completely untouched.
 *
 * Datagrams are emulated over a single WebSocket per client. Each frame
 * is
 *
 *     [src port : u16 LE][dst port : u16 LE][payload]
 *
 * which is enough to keep the original two-socket handshake working:
 * the client contacts the server's well-known port, the server opens a
 * fresh "socket" for that client and replies with its port number, and
 * everything afterwards flows between those two virtual ports. The
 * server's one-UDP-socket-per-client model maps onto one WebSocket per
 * client rather neatly.
 *
 * The WebSocket itself is deliberately NOT created here. The host
 * supplies a transport object before the module starts:
 *
 *     globalThis.KoulesTransport = {
 *       isServer: <bool>,
 *       send: function (addr, bytes) { ... }
 *     }
 *
 * and pushes received frames in with
 *
 *     globalThis.KoulesNet.deliver(addr, bytes)
 *
 * where `addr` identifies the peer -- the string "server" for a client,
 * or a per-connection id on the server. That keeps the C side free of
 * any environment assumptions, so the same build runs against a Node
 * `ws` server today and a Cloudflare Durable Object later, with the
 * host object being the only thing that differs.
 *
 * Reliability: WebSocket is ordered and reliable, where UDP was
 * neither. The protocol copes fine -- position packets are whole-state
 * snapshots, and its own CRELIABLE/SRELIABLE layer simply becomes
 * redundant rather than wrong.
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>

#include "sock.h"
#include "wasm_compat.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define KW_MAXSOCK 16
#define KW_FDBASE 1000		/* clear of any real descriptor, so a stray
				 * close() on one of ours just fails */
#define KW_EPHEMERAL 40000

struct kw_sock
{
  int             used;
  int             port;
  int             connected;	/* DgramConnect was called */
  char            peer[256];
  int             peerport;
};

static struct kw_sock kw_socks[KW_MAXSOCK];
static int      kw_next_ephemeral = KW_EPHEMERAL;
static struct timeval kw_timeout;
static char     kw_lastaddr_buf[256];
static int      kw_lastport_val;
static int      kw_started;

/*--------------------------------------------------------------------
 * JS glue. All queueing lives in JS so the C side stays small.
 *------------------------------------------------------------------*/

EM_JS (void, kwjs_start, (void),
{
  if (globalThis.KoulesNet)
    return;
  var net = globalThis.KoulesNet = {
    queues: {},			/* dst port -> array of frames */
    lastAddr: String(),
    lastPort: 0,
    deliver: function (addr, bytes)
      {
	if (!bytes || bytes.length < 4)
	  return;
	var sp = bytes[0] | (bytes[1] << 8);
	var dp = bytes[2] | (bytes[3] << 8);
	var q = net.queues[dp] || (net.queues[dp] = []);
	q.push ({ addr: addr, port: sp, data: bytes.subarray (4) });
      }
  };
});

EM_JS (int, kwjs_readable, (int vport),
{
  var q = globalThis.KoulesNet && globalThis.KoulesNet.queues[vport];
  return (q && q.length) ? 1 : 0;
});

/* Pops one frame for `vport`, copies the payload out and records the
 * sender. Returns the payload length, or -1 when nothing is queued. */
EM_JS (int, kwjs_recv, (int vport, char *buf, int max),
{
  var net = globalThis.KoulesNet;
  var q = net && net.queues[vport];
  if (!q || !q.length)
    return -1;
  var f = q.shift ();
  net.lastAddr = f.addr;
  net.lastPort = f.port;
  var n = f.data.length;
  if (n > max)
    n = max;
  HEAPU8.set (f.data.subarray (0, n), buf);
  return n;
});

EM_JS (int, kwjs_send, (const char *host, int sport, int dport,
			const char *buf, int len),
{
  var t = globalThis.KoulesTransport;
  if (!t || !t.send)
    return -1;
  var frame = new Uint8Array (len + 4);
  frame[0] = sport & 0xff;
  frame[1] = (sport >> 8) & 0xff;
  frame[2] = dport & 0xff;
  frame[3] = (dport >> 8) & 0xff;
  frame.set (HEAPU8.subarray (buf, buf + len), 4);
  try
    {
      t.send (UTF8ToString (host), frame);
    }
  catch (e)
    {
      return -1;
    }
  return len;
});

EM_JS (void, kwjs_lastaddr, (char *out, int max),
{
  var a = (globalThis.KoulesNet && globalThis.KoulesNet.lastAddr) || String();
  stringToUTF8 (a, out, max);
});

EM_JS (int, kwjs_lastport, (void),
{
  return (globalThis.KoulesNet && globalThis.KoulesNet.lastPort) | 0;
});

/*--------------------------------------------------------------------
 * sock.h implementation
 *------------------------------------------------------------------*/

static struct kw_sock *
kw_get (int fd)
{
  int             i = fd - KW_FDBASE;

  if (i < 0 || i >= KW_MAXSOCK || !kw_socks[i].used)
    return NULL;
  return &kw_socks[i];
}

void
SetTimeout (int s, int us)
{
  kw_timeout.tv_sec = s;
  kw_timeout.tv_usec = us;
}

int
CreateDgramSocket (int port)
{
  int             i;

  if (!kw_started)
    {
      kwjs_start ();
      kw_started = 1;
    }

  for (i = 0; i < KW_MAXSOCK; i++)
    if (!kw_socks[i].used)
      {
	memset (&kw_socks[i], 0, sizeof (kw_socks[i]));
	kw_socks[i].used = 1;
	kw_socks[i].port = port ? port : kw_next_ephemeral++;
	return KW_FDBASE + i;
      }
  errno = EMFILE;
  return -1;
}

int
SocketClose (int fd)
{
  struct kw_sock *s = kw_get (fd);

  if (!s)
    return -1;
  s->used = 0;
  return 0;
}

int
GetPortNum (int fd)
{
  struct kw_sock *s = kw_get (fd);

  return s ? s->port : -1;
}

char           *
GetSockAddr (int fd)
{
  (void) fd;
  return "ws";
}

/* The original select()ed with a timeout. Honour it as a yield: that
 * keeps the host's event loop running, which is the only way a frame
 * can ever arrive, and it is what lets the game's blocking "wait for
 * the server to answer" loops work at all in a browser. A zero timeout
 * stays a pure poll, so the per-frame drain loops do not yield. */
int
SocketReadable (int fd)
{
  struct kw_sock *s = kw_get (fd);
  long            us;

  if (!s)
    return -1;
  if (kwjs_readable (s->port))
    return 1;

  us = (long) kw_timeout.tv_sec * 1000000 + kw_timeout.tv_usec;
  while (us > 0)
    {
      long            slice = us > 10000 ? 10000 : us;

      koules_yield_us (slice);
      if (kwjs_readable (s->port))
	return 1;
      us -= slice;
    }
  return 0;
}

int
DgramSend (int fd, char *host, int port, char *sbuf, int size)
{
  struct kw_sock *s = kw_get (fd);

  if (!s)
    return -1;
  return kwjs_send (host, s->port, port, sbuf, size);
}

int
DgramReceiveAny (int fd, char *rbuf, int size)
{
  struct kw_sock *s = kw_get (fd);
  int             n;

  if (!s)
    return -1;
  n = kwjs_recv (s->port, rbuf, size);
  if (n < 0)
    {
      errno = EWOULDBLOCK;
      return -1;
    }
  kwjs_lastaddr (kw_lastaddr_buf, sizeof (kw_lastaddr_buf));
  kw_lastport_val = kwjs_lastport ();
  return n;
}

int
DgramReceiveConnected (int fd, char *rbuf, int size)
{
  return DgramReceiveAny (fd, rbuf, size);
}

int
DgramConnect (int fd, char *host, int port)
{
  struct kw_sock *s = kw_get (fd);

  if (!s)
    return -1;
  strncpy (s->peer, host, sizeof (s->peer) - 1);
  s->peer[sizeof (s->peer) - 1] = '\0';
  s->peerport = port;
  s->connected = 1;
  return 0;
}

char           *
DgramLastaddr (void)
{
  return kw_lastaddr_buf;
}

int
DgramLastport (void)
{
  return kw_lastport_val;
}

/* Nothing to tune on a WebSocket; report success so the caller's
 * error paths stay out of the way. */
int
SetSocketReceiveBufferSize (int fd, int size)
{
  (void) fd, (void) size;
  return 0;
}

int
SetSocketSendBufferSize (int fd, int size)
{
  (void) fd, (void) size;
  return 0;
}

int
SetSocketNonBlocking (int fd, int flag)
{
  (void) fd, (void) flag;
  return 0;		       /* every receive here is already non-blocking */
}

int
GetSocketError (int fd)
{
  (void) fd;
  errno = 0;
  return 0;
}
