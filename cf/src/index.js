/*
 * index.js - routes players to their room.
 *
 * Static assets (the game client) are served by the [assets] binding;
 * anything else falls through to here. /room/<name> is the WebSocket
 * endpoint, and the room name picks the Durable Object, so two people
 * who open the same URL land in the same game.
 */

export { Room } from './room.js';

/* Keep names tame: they become Durable Object names. */
const ROOM_RE = /^\/room\/([A-Za-z0-9_-]{1,32})$/;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const match = ROOM_RE.exec(url.pathname);
    if (!match) {
      return new Response('Not found', { status: 404 });
    }

    /* idFromName is deterministic, so the same room name is the same
     * object worldwide -- that is the whole trick. */
    const id = env.ROOMS.idFromName(match[1].toLowerCase());
    return env.ROOMS.get(id).fetch(request);
  },
};
