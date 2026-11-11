import { test } from "node:test";
import assert from "node:assert/strict";
import { RoomDirectory } from "../../online/signaling/worker.mjs";

class Socket {
  constructor(id) { this.state = { id, build: "", nickname: "", roomId: "", hosting: false, failedPasswords: 0, retryAt: 0 }; this.messages = []; }
  deserializeAttachment() { return structuredClone(this.state); }
  serializeAttachment(value) { this.state = structuredClone(value); }
  send(text) { this.messages.push(JSON.parse(text)); }
  close() { this.closed = true; }
  last(kind) { return this.messages.findLast(message => message.kind === kind); }
}
function setup() {
  const sockets = [];
  const ctx = { getWebSockets: () => sockets.filter(socket => !socket.closed) };
  const directory = new RoomDirectory(ctx, {});
  const add = (id, build = "build-123") => {
    const socket = new Socket(id); sockets.push(socket);
    directory.webSocketMessage(socket, JSON.stringify({ kind: "hello", build, nickname: id }));
    return socket;
  };
  const send = (socket, message) => directory.webSocketMessage(socket, JSON.stringify(message));
  const create = (host, extras = {}) => {
    send(host, { kind: "create", name: "Моя комната", maxPlayers: 4, salt: "", proof: "", ...extras });
    return host.last("created").room;
  };
  return { directory, ctx, add, send, create };
}

test("directory filters exact build and never publishes password material", () => {
  const { add, create, send } = setup();
  const host = add("Host"), client = add("Client"), other = add("Other", "build-124");
  const room = create(host, { salt: "a".repeat(32), proof: "b".repeat(64) });
  assert.equal(client.last("rooms").rooms.length, 1);
  assert.equal(other.last("rooms").rooms.length, 0);
  const listed = client.last("rooms").rooms[0];
  assert.equal(listed.password, true);
  assert.equal("salt" in listed, false); assert.equal("proof" in listed, false);
  send(other, { kind: "join", room, proof: "b".repeat(64) });
  assert.equal(other.last("error").code, "version-mismatch");
  assert.equal(other.state.roomId, "");
});

test("password mismatch is rejected and retries are throttled", () => {
  const { add, create, send } = setup();
  const host = add("Host"), client = add("Client");
  const room = create(host, { salt: "a".repeat(32), proof: "b".repeat(64) });
  send(client, { kind: "challenge", room });
  assert.equal(client.last("challenge").salt, "a".repeat(32));
  send(client, { kind: "join", room, proof: "c".repeat(64) });
  assert.equal(client.last("error").code, "wrong-password");
  send(client, { kind: "join", room, proof: "b".repeat(64) });
  assert.equal(client.last("error").code, "password-retry-later");
  client.state.retryAt = 0;
  send(client, { kind: "join", room, proof: "b".repeat(64) });
  assert.equal(client.last("joined").host, "Host");
});

test("capacity includes host and pending ICE peers", () => {
  const { add, create, send } = setup();
  const host = add("Host"), first = add("First"), second = add("Second");
  const room = create(host, { maxPlayers: 2 });
  send(first, { kind: "join", room, proof: "" });
  send(second, { kind: "join", room, proof: "" });
  assert.equal(second.last("error").code, "room-full");
  assert.equal(host.last("rooms").rooms[0].players, 2);
});

test("signaling is host-centric and the server stamps sender identity", () => {
  const { add, create, send } = setup();
  const host = add("Host"), first = add("First"), second = add("Second"), outsider = add("Outsider");
  const room = create(host);
  send(first, { kind: "join", room, proof: "" }); send(second, { kind: "join", room, proof: "" });
  send(first, { kind: "signal", to: "Host", from: "Second", type: "sdp", sdpType: "offer", sdp: "v=0" });
  assert.equal(host.last("signal").from, "First");
  send(first, { kind: "signal", to: "Second", type: "candidate", candidate: "candidate:test", mid: "0" });
  assert.equal(first.last("error").code, "invalid-peer");
  send(outsider, { kind: "signal", to: "Host", type: "sdp", sdpType: "offer", sdp: "v=0" });
  assert.equal(outsider.last("error").code, "invalid-peer");
});

test("kick is host-only, frees capacity, and leaves the browser connected", () => {
  const { add, create, send } = setup();
  const host = add("Host"), client = add("Client");
  const room = create(host);
  send(client, { kind: "join", room, proof: "" });
  send(client, { kind: "kick", peer: "Host" }); assert.equal(client.last("error").code, "host-only");
  send(host, { kind: "kick", peer: "Client" });
  assert.ok(client.last("kicked")); assert.equal(client.state.roomId, ""); assert.equal(client.closed, undefined);
  assert.equal(host.last("rooms").rooms[0].players, 1);
});

test("hibernation reconstructs room data from live socket attachments", () => {
  const { ctx, add, create, send } = setup();
  const host = add("Host"), client = add("Client");
  create(host); send(host, { kind: "playing", playing: true });
  const resumed = new RoomDirectory(ctx, {});
  resumed.webSocketMessage(client, JSON.stringify({ kind: "list" }));
  assert.equal(client.last("rooms").rooms[0].playing, true);
  resumed.webSocketMessage(host, JSON.stringify({ kind: "leave" }));
  assert.equal(client.last("rooms").rooms.length, 0);
});

test("host loss closes memberships and removes stale room", () => {
  const { directory, add, create, send } = setup();
  const host = add("Host"), client = add("Client");
  const room = create(host); send(client, { kind: "join", room, proof: "" });
  directory.webSocketClose(host);
  assert.ok(client.last("closed")); assert.equal(client.state.roomId, "");
  assert.equal(client.last("rooms").rooms.length, 0);
});

test("malformed, oversized, game-data and unregistered messages are rejected", () => {
  const { directory, add, send } = setup();
  const client = add("Client");
  directory.webSocketMessage(client, "{"); assert.equal(client.last("error").code, "invalid-json");
  send(client, { kind: "snapshot", bytes: "not-relayed" }); assert.equal(client.last("error").code, "unknown-message");
  send(client, { kind: "create", name: "bad\nname", maxPlayers: 999, proof: "", salt: "" });
  assert.equal(client.last("error").code, "invalid-room");
  const fresh = new Socket("Fresh"); send(fresh, { kind: "list" }); assert.equal(fresh.last("error").code, "hello-required");
  directory.webSocketMessage(client, "x".repeat(65537)); assert.equal(client.closed, true);
});
