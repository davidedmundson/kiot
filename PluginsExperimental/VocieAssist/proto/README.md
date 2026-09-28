# Vendored ESPHome Protocol Buffers

These `.proto` files come from
[aioesphomeapi](https://github.com/esphome/aioesphomeapi), the Python ESPHome
API library. They define the wire format Home Assistant uses to talk to
ESPHome devices over TCP port 6053.

## Pinned version

- Source: `python-aioesphomeapi` Buildroot package, `PYTHON_AIOESPHOMEAPI_VERSION = v42.7.0`
- Upstream tag: https://github.com/esphome/aioesphomeapi/releases/tag/v42.7.0
- Path in upstream: `aioesphomeapi/api.proto`, `aioesphomeapi/api_options.proto`

We vendor these files (rather than depending on the Python package's
download dir) so the C++ package builds independently of which Python
LVA package is enabled in defconfig — and so we can pin the on-the-wire
contract to a known commit even when the Python package version moves.

## Files

- `api.proto` — message and service definitions (proto3, ~2,488 lines).
- `api_options.proto` — custom options that decorate messages/methods
  with their wire ID, source side, etc. (proto2, ~93 lines, only consumed
  by `protoc` at build time).

## When to bump

Bump these only when we deliberately want a newer ESPHome wire-protocol
revision. After any update:

1. Re-run the message-id table generator described in
   `src/protocol/proto_ids.h` to regenerate the hand-extracted ID
   constants.
2. Re-run the host-side framing tests.
3. Note the new upstream commit in this README.

## License

Apache-2.0. Same license as our project, no compatibility issues.
