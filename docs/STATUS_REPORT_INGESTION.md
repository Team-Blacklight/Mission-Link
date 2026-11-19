# Authenticated status-report ingestion contract

This defines a deployment-neutral server-ingestion boundary for the public outage tracker. It does not create or deploy a backend; GitHub Pages remains static.

## Trust model

- Each enrolled reporter has a unique opaque reporter ID and distinct high-entropy secret provisioned out of band. Secrets stay server-side and never enter firmware defaults or the repository.
- The server maps the authenticated reporter ID to an enrolled node and server-maintained trust metadata. The body cannot choose its reporter identity.
- Reports are observations, not commands. They cannot cause transmission, route changes, or radio-setting changes.
- Only fresh authenticated reports from distinct enrolled reporters contribute to quorum. Repeated reports from one reporter are deduplicated.
- Correlation groups and topology dependencies come from trusted server inventory, never the request body. Different IDs sharing an upstream dependency are not independent evidence.
- Public IDs and labels are server-generated. Never publish Mesh Numbers, hardware IDs, keys, contact data, message metadata, precise locations, or raw reporter IDs.

## HTTP request

`POST /v1/reports`

Headers:
- `Content-Type: application/json`
- `X-ML-Reporter: <opaque enrolled reporter ID>`
- `X-ML-Timestamp: <Unix seconds>`
- `X-ML-Nonce: <base64url random nonce, 16–64 decoded bytes>`
- `X-ML-Signature: sha256=<lowercase hex HMAC-SHA256>`

The signature is HMAC-SHA256 over the exact request bytes, using the reporter secret and this byte sequence:

```
missionlink-status-v1\n<timestamp header>\n<nonce header>\n<raw request body>
```

Compare signatures in constant time. Reject unknown reporters, malformed headers, invalid signatures, timestamps outside a small clock-skew window (recommended maximum 5 minutes), and reused nonces. Store nonces durably with a uniqueness constraint scoped to reporter ID; in-memory replay tracking is insufficient across restarts or multiple server instances. Apply body-size limits and per-reporter/IP rate limits.

## Report body

The body follows `status-report.schema.json`. `targetPublicId` is an opaque public node ID assigned by the service. `observedState` is `reachable`, `unreachable`, or `maintenance`. Optional latency and packet-loss fields are bounded measurements, not proof of end-to-end message delivery.

The server validates reporter-to-target authorization using its own inventory. A reporter cannot declare another reporter, its own trust level, an incident, a quorum, or a public network state.

## Aggregation rules

1. Store accepted observations with server receive time, reporter, target, and server-side dependency/correlation metadata.
2. Deduplicate by reporter, target, and observation window; newer reports replace that reporter's older observation for the same window.
3. Count independent reporters only after excluding reporters in the same known dependency group. Different IDs alone do not prove independence.
4. One independent negative reporter may mark a target unstable; a configurable multi-reporter quorum may mark it suspected down. Never declare a definitive outage from one observation.
5. Maintenance is an operator-authorized server-side record with expiry. This endpoint cannot let a node self-declare maintenance.
6. Recovery requires fresh positive observations from the configured independent quorum. Expired observations become unknown, not down.
7. Generate incidents and public summaries server-side. Publish only allowlisted public IDs, safe labels, coarse state, and sanitized summaries.
8. Keep `/api/network-status.json` unknown until a real aggregator is deployed and has fresh accepted data.

## Responses

- `202 Accepted`: authenticated report accepted for aggregation.
- `400 Bad Request`: malformed body or headers.
- `401 Unauthorized`: unknown reporter or invalid signature; do not reveal whether a reporter ID exists.
- `403 Forbidden`: reporter is not authorized to observe the target.
- `409 Conflict`: nonce replay.
- `413 Payload Too Large`: body exceeds configured bound.
- `429 Too Many Requests`: rate limit exceeded.

Avoid detailed signature-validation errors.

## Deployment and key lifecycle

Use HTTPS, a managed secret store, durable transactional storage, backups, audit logs that omit secrets/signatures/raw bodies, and a separate read-only public feed. Support key rotation with a short overlap period and immediate revocation. Keep administrative maintenance controls separate from node credentials. Monitor clock drift, replay attempts, rate limiting, stale reports, and aggregator failures.

## Explicit non-goals

This contract does not implement a live server, cryptographic key provisioning, firmware reporting, radio transport, route discovery, or live outage detection. No RF behavior is changed; transmission remains disabled.
