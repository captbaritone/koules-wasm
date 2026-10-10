/*
 * room.js - one Durable Object per game room.
 *
 * The DO owns a single instance of the game's own dedicated server
 * (server.c, compiled to WebAssembly against the headless backend in
 * null/) and runs its original 25Hz authoritative loop. Players arrive
 * over WebSocket; this file is only the transport, the same job
 * net/server.cjs does under Node.
 *
 * The DO is what makes the room a real place: it is single-threaded,
 * globally unique for its name, and holds the game state in memory, so
 * there is exactly one simulation per room and no coordination to do.
 */

import createKoules from '../koules-server.mjs';
import wasmModule from '../koules-server.wasm';

/* Once the last player leaves there is nobody to simulate for. The
 * server notices too and returns from its loop, which ends the Asyncify
 * timer chain -- so this is only about how long we hold the finished
 * instance before starting a fresh game for whoever shows up next. */
const IDLE_RESET_MS = 30_000;

export class Room {
  constructor(state, env) {
    this.state = state;
    this.env = env;
    this.peers = new Map();
    this.nextPeer = 1;
    this.koules = null;
    this.booting = null;
    this.idleTimer = null;
  }

  async fetch(request) {
    if (request.headers.get('Upgrade') !== 'websocket') {
      return new Response('expected a websocket upgrade', { status: 426 });
    }

    const pair = new WebSocketPair();
    const [clientSide, serverSide] = [pair[0], pair[1]];

    /* Deliberately not the hibernation API: hibernation is for rooms
     * that sit idle between messages, and this one is running a
     * simulation 25 times a second for as long as anyone is here. */
    serverSide.accept();

    const addr = 'p' + this.nextPeer++;
    this.peers.set(addr, serverSide);
    if (this.idleTimer !== null) {
      clearTimeout(this.idleTimer);
      this.idleTimer = null;
    }

    serverSide.addEventListener('message', (event) => {
      const net = this.koules && this.koules.KoulesNet;
      if (!net) return; /* not booted yet; the client retries */
      net.deliver(addr, toBytes(event.data));
    });

    const drop = () => {
      if (!this.peers.delete(addr)) return;   /* close and error both fire */
      /* Tell the game, so it can free that player's slot instead of
       * waiting on a timeout that used to take the whole server down. */
      const net = this.koules && this.koules.KoulesNet;
      if (net) net.peerGone(addr);
      if (this.peers.size === 0) this.scheduleReset();
    };
    serverSide.addEventListener('close', drop);
    serverSide.addEventListener('error', drop);

    await this.boot();

    return new Response(null, { status: 101, webSocket: clientSide });
  }

  /* Start the game server once, on the first player to arrive. */
  boot() {
    if (this.booting) return this.booting;

    this.booting = createKoules({
      arguments: ['-S'],

      /* Routing key is the peer id; the game copies it into
       * conn[].hostname and hands it back on every send. */
      KoulesTransport: {
        send: (addr, bytes) => {
          const sock = this.peers.get(addr);
          if (!sock) return;
          try {
            sock.send(bytes);
          } catch {
            this.peers.delete(addr);
          }
        },
      },

      /* Workers allow no I/O during startup, so the module must not go
       * and fetch its own .wasm. We already have it as a compiled
       * WebAssembly.Module from the import above, and instantiating
       * from that is synchronous and allowed. */
      instantiateWasm: (imports, successCallback) => {
        const instance = new WebAssembly.Instance(wasmModule, imports);
        successCallback(instance);
        return instance.exports;
      },

      print: (t) => console.log('[koules] ' + t),
      printErr: (t) => console.error('[koules] ' + t),
    }).then((mod) => {
      /* Resolves once main() suspends inside the server loop's first
       * sleep; the loop keeps running off the event loop from here. */
      this.koules = mod;
      return mod;
    });

    return this.booting;
  }

  /* Drop the finished server so the next arrival starts a new game in
   * this same room. server.c returns from server_loop once every client
   * has gone, so the instance we are letting go of is already idle --
   * nothing is left ticking. */
  scheduleReset() {
    if (this.idleTimer !== null) clearTimeout(this.idleTimer);
    this.idleTimer = setTimeout(() => {
      if (this.peers.size > 0) return;
      this.koules = null;
      this.booting = null;
      this.nextPeer = 1;
      console.log('[room] empty; next player gets a fresh game');
    }, IDLE_RESET_MS);
  }
}

/* A WebSocket message is an ArrayBuffer here, but be liberal: the
 * equivalent Node host sees Buffers and fragment arrays. */
function toBytes(data) {
  if (data instanceof ArrayBuffer) return new Uint8Array(data);
  if (ArrayBuffer.isView(data)) {
    return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  }
  /* A text frame means something is talking to us that is not the game. */
  return new Uint8Array(0);
}
