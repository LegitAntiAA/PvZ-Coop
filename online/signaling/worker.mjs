// Cloudflare carries room discovery and bounded SDP/ICE signaling only.
// Gameplay always travels through the peers' encrypted WebRTC DataChannels.
const MAX_MESSAGE = 65536;
const MAX_ROOMS = 64;
const MAX_SOCKETS = 512;
const validText = (value, max) => typeof value === "string" && value.length > 0 &&
  [...value].length <= max && !/[\u0000-\u001f\u007f]/u.test(value);
const hex = (value, size) => typeof value === "string" && value.length === size && /^[0-9a-f]+$/u.test(value);
const sameProof = (left, right) => {
  if (left.length !== right.length) return false;
  let difference = 0;
  for (let i = 0; i < left.length; ++i) difference |= left.charCodeAt(i) ^ right.charCodeAt(i);
  return difference === 0;
};

export default {
  async fetch(request, env) {
    const path = new URL(request.url).pathname;
    if (path === "/health") return Response.json({ service: "pvz-coop-signaling", schema: 1 });
    if (path !== "/signal") return new Response("Not found", { status: 404 });
    if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket")
      return new Response("WebSocket required", { status: 426 });
    return env.DIRECTORY.get(env.DIRECTORY.idFromName("pvz-coop-v1")).fetch(request);
  }
};

export class RoomDirectory {
  constructor(ctx, env) {
    this.ctx = ctx;
    this.env = env;
    this.rate = new Map();
  }

  sockets() { return this.ctx.getWebSockets(); }
  attachment(ws) { return ws.deserializeAttachment() ?? {}; }
  save(ws, state) { ws.serializeAttachment(state); }
  send(ws, message) {
    try { ws.send(JSON.stringify(message)); } catch { /* close callback removes stale members */ }
  }
  error(ws, code) { this.send(ws, { kind: "error", code }); }
  hosts() { return this.sockets().filter(ws => this.attachment(ws).hosting); }
  host(room) { return this.hosts().find(ws => this.attachment(ws).room?.id === room); }
  members(room) { return this.sockets().filter(ws => this.attachment(ws).roomId === room); }
  byId(id) { return this.sockets().find(ws => this.attachment(ws).id === id); }

  list(ws) {
    const viewer = this.attachment(ws);
    const rooms = this.hosts().map(host => {
      const state = this.attachment(host), room = state.room;
      return { id: room.id, name: room.name, host: state.nickname, build: room.build,
        maxPlayers: room.maxPlayers, players: this.members(room.id).length,
        password: room.proof.length > 0, playing: room.playing };
    }).filter(room => room.build === viewer.build);
    this.send(ws, { kind: "rooms", rooms });
  }
  publish() {
    for (const ws of this.sockets()) if (this.attachment(ws).build) this.list(ws);
  }

  async fetch() {
    if (this.sockets().length >= MAX_SOCKETS) return new Response("Directory busy", { status: 503 });
    const [client, server] = Object.values(new WebSocketPair());
    this.ctx.acceptWebSocket(server);
    this.save(server, { id: crypto.randomUUID(), build: "", nickname: "", roomId: "", hosting: false,
      failedPasswords: 0, retryAt: 0 });
    return new Response(null, { status: 101, webSocket: client });
  }

  leave(ws, notification = "closed") {
    const state = this.attachment(ws);
    if (!state.roomId) return;
    const roomId = state.roomId;
    if (state.hosting) {
      for (const member of this.members(roomId)) {
        if (member === ws) continue;
        const memberState = this.attachment(member);
        memberState.roomId = ""; memberState.hosting = false;
        this.save(member, memberState);
        this.send(member, { kind: "closed" });
      }
    } else {
      const host = this.host(roomId);
      if (host) this.send(host, { kind: "peer-left", peer: state.id });
      this.send(ws, { kind: notification });
    }
    state.roomId = ""; state.hosting = false; delete state.room;
    this.save(ws, state);
  }

  webSocketMessage(ws, raw) {
    if (typeof raw !== "string" || new TextEncoder().encode(raw).length > MAX_MESSAGE) {
      ws.close(1009, "Signaling message too large"); return;
    }
    const now = Date.now();
    const rate = this.rate.get(ws) ?? { time: now, count: 0 };
    if (now - rate.time > 1000) { rate.time = now; rate.count = 0; }
    if (++rate.count > 128) { this.error(ws, "rate-limit"); return; }
    this.rate.set(ws, rate);
    let message;
    try { message = JSON.parse(raw); } catch { this.error(ws, "invalid-json"); return; }
    if (!message || typeof message !== "object" || Array.isArray(message)) {
      this.error(ws, "invalid-message"); return;
    }
    const state = this.attachment(ws);
    if (message.kind === "hello") {
      if (state.build || !validText(message.build, 128) || !validText(message.nickname, 16)) {
        this.error(ws, "invalid-hello"); return;
      }
      state.build = message.build; state.nickname = message.nickname;
      this.save(ws, state); this.send(ws, { kind: "ready", peer: state.id }); return;
    }
    if (!state.build) { this.error(ws, "hello-required"); return; }
    switch (message.kind) {
      case "keepalive": this.send(ws, { kind: "alive" }); break;
      case "list": this.list(ws); break;
      case "create": {
        if (state.roomId) { this.error(ws, "already-in-room"); break; }
        if (this.hosts().length >= MAX_ROOMS) { this.error(ws, "directory-full"); break; }
        if (!validText(message.name, 48) || !Number.isInteger(message.maxPlayers) ||
            message.maxPlayers < 2 || message.maxPlayers > 4 ||
            !((message.salt === "" && message.proof === "") || (hex(message.salt, 32) && hex(message.proof, 64)))) {
          this.error(ws, "invalid-room"); break;
        }
        state.roomId = crypto.randomUUID(); state.hosting = true;
        state.room = { id: state.roomId, name: message.name, build: state.build,
          maxPlayers: message.maxPlayers, salt: message.salt, proof: message.proof, playing: false };
        this.save(ws, state); this.send(ws, { kind: "created", room: state.roomId }); this.publish(); break;
      }
      case "challenge": {
        const host = this.host(message.room), room = host && this.attachment(host).room;
        if (!room) { this.error(ws, "room-not-found"); break; }
        if (room.build !== state.build) { this.error(ws, "version-mismatch"); break; }
        this.send(ws, { kind: "challenge", room: room.id, password: room.proof.length > 0, salt: room.salt }); break;
      }
      case "join": {
        const host = this.host(message.room), hostState = host && this.attachment(host), room = hostState?.room;
        if (!room) { this.error(ws, "room-not-found"); break; }
        if (room.build !== state.build) { this.error(ws, "version-mismatch"); break; }
        if (state.roomId) { this.error(ws, "already-in-room"); break; }
        if (this.members(room.id).length >= room.maxPlayers) { this.error(ws, "room-full"); break; }
        if (state.retryAt > now) { this.error(ws, "password-retry-later"); break; }
        if (typeof message.proof !== "string" || !sameProof(room.proof, message.proof)) {
          state.failedPasswords = Math.min(10, state.failedPasswords + 1);
          state.retryAt = now + Math.min(30000, state.failedPasswords * 1000);
          this.save(ws, state); this.error(ws, "wrong-password"); break;
        }
        state.failedPasswords = 0; state.retryAt = 0;
        state.roomId = room.id; this.save(ws, state);
        this.send(ws, { kind: "joined", room: room.id, host: hostState.id, playing: room.playing });
        this.send(host, { kind: "peer-joined", peer: state.id, nickname: state.nickname });
        this.publish(); break;
      }
      case "playing": {
        if (!state.hosting || typeof message.playing !== "boolean") { this.error(ws, "host-only"); break; }
        state.room.playing = message.playing; this.save(ws, state); this.publish(); break;
      }
      case "signal": {
        const target = this.byId(message.to), targetState = target && this.attachment(target);
        if (!target || !state.roomId || targetState.roomId !== state.roomId ||
            targetState.build !== state.build || state.id === targetState.id ||
            (!state.hosting && !targetState.hosting)) { this.error(ws, "invalid-peer"); break; }
        if (message.type === "sdp" && typeof message.sdp === "string" && message.sdp.length <= 32768 &&
            (message.sdpType === "offer" || message.sdpType === "answer"))
          this.send(target, { kind: "signal", from: state.id, type: "sdp", sdpType: message.sdpType, sdp: message.sdp });
        else if (message.type === "candidate" && typeof message.candidate === "string" &&
            message.candidate.length <= 4096 && typeof message.mid === "string" && message.mid.length <= 64)
          this.send(target, { kind: "signal", from: state.id, type: "candidate", candidate: message.candidate, mid: message.mid });
        else this.error(ws, "invalid-signal");
        break;
      }
      case "drop":
      case "kick": {
        const target = this.byId(message.peer), targetState = target && this.attachment(target);
        if (!target || !state.roomId || targetState.roomId !== state.roomId || target === ws ||
            (!state.hosting && (message.kind === "kick" || !targetState.hosting))) {
          this.error(ws, "host-only"); break;
        }
        if (state.hosting) this.leave(target, message.kind === "kick" ? "kicked" : "closed");
        else this.leave(ws);
        this.publish(); break;
      }
      case "leave": this.leave(ws); this.publish(); break;
      default: this.error(ws, "unknown-message");
    }
  }

  webSocketClose(ws) {
    this.leave(ws); this.rate.delete(ws);
    try { ws.close(1000, "Closed"); } catch { /* already closed */ }
    this.publish();
  }
  webSocketError(ws) { this.webSocketClose(ws); }
}
