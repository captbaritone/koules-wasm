/*
 * server.cjs - run the Koules dedicated server under Node.
 *
 * The server is the game's own server.c, compiled to WebAssembly and
 * running its original 25Hz authoritative loop. This file is only the
 * transport: it accepts WebSocket connections and hands frames to and
 * from the wasm module through the small contract sock_ws.c expects.
 *
 * Everything here that matters is environment-specific, and that is the
 * point -- a Cloudflare Durable Object would replace this file and
 * nothing else.
 *
 *   node net/server.cjs [--port 8200] [-- <koules server args>]
 *
 * Extra arguments after `--` go to the game, e.g. `-K` for deathmatch
 * or `-L 3` to start on level 3.
 */
'use strict';

const { WebSocketServer } = require('ws');
const path = require('path');

const argv = process.argv.slice(2);
const sep = argv.indexOf('--');
const hostArgs = sep === -1 ? argv : argv.slice(0, sep);
const gameArgs = sep === -1 ? [] : argv.slice(sep + 1);

const portIdx = hostArgs.indexOf('--port');
const PORT = portIdx === -1 ? 8200 : Number(hostArgs[portIdx + 1]);

/* Peers are identified by a short string. The game copies it into
 * conn[].hostname and hands it back to us on every send, so it is the
 * only routing key we need. */
const peers = new Map();
let nextPeer = 1;

/* `ws` hands us a Buffer, an ArrayBuffer or a list of Buffers depending
 * on binaryType and whether the message arrived fragmented. Normalise to
 * a Uint8Array view over the actual bytes. */
function toBytes(data) {
  if (Array.isArray(data)) return toBytes(Buffer.concat(data));
  if (data instanceof ArrayBuffer) return new Uint8Array(data);
  return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
}

globalThis.KoulesTransport = {
  isServer: true,
  send(addr, bytes) {
    const sock = peers.get(addr);
    if (sock && sock.readyState === 1) sock.send(bytes);
  },
};

const wss = new WebSocketServer({ port: PORT });

wss.on('connection', (sock) => {
  const addr = 'p' + nextPeer++;
  peers.set(addr, sock);
  sock.binaryType = 'arraybuffer';
  console.log(`[net] ${addr} connected (${peers.size} online)`);

  sock.on('message', (data) => {
    /* The module creates KoulesNet on its first socket; until then
     * there is nothing listening and the client will retry. */
    const net = globalThis.KoulesNet;
    if (!net) return;
    net.deliver(addr, toBytes(data));
  });
  sock.on('close', () => {
    peers.delete(addr);
    console.log(`[net] ${addr} disconnected (${peers.size} online)`);
  });
  sock.on('error', () => {});
});

console.log(`[net] listening on ws://localhost:${PORT}`);

/* The game's main() parses argv, so -S is what makes it a server. */
const createKoules = require(path.join(__dirname, 'koules-server.js'));

createKoules({
  arguments: ['-S'].concat(gameArgs),
  print: (t) => console.log('[koules] ' + t),
  printErr: (t) => console.error('[koules] ' + t),
}).then(() => {
  /* Asyncify suspends inside the server loop and returns here; the
   * loop keeps running off the event loop from now on. */
  console.log('[net] koules server loop running');
}).catch((e) => {
  console.error('[net] failed to start:', e);
  process.exit(1);
});
