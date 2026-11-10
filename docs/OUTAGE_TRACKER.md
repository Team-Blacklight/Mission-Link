# MissionLink outage tracker

The public web client includes outages.html, a read-only network status page. It polls ./api/network-status.json every 30 seconds and deliberately displays an unknown/awaiting state when no feed is available. It never fabricates node counts or outage records.

## Status feed contract (schema version 1)

The future authenticated status service should publish a JSON document with this shape:

~~~json
{
  "schemaVersion": 1,
  "generatedAt": "2026-11-10T12:00:00Z",
  "networkState": "online",
  "summary": "All monitored services are operational.",
  "nodes": [
    { "publicId": "node-public-id", "label": "Vancouver relay", "state": "online" }
  ],
  "incidents": [
    { "title": "Relay connectivity degraded", "status": "investigating", "summary": "Multiple independent reports indicate degraded reachability.", "startedAt": "2026-11-10T11:45:00Z", "resolvedAt": null }
  ]
}
~~~

Allowed network states: online, degraded, outage, maintenance, unknown. Allowed node states: online, unstable, suspected_down, down, maintenance, recovering. An incident with status resolved is treated as resolved; other incident statuses count as active.

## Production requirements

- Generate the feed server-side from authenticated node reports, not directly from arbitrary browser clients.
- Require distinct reporters for outage quorum and deduplicate repeated reports from the same reporter. Correlate reports to avoid counting nodes that share one failed upstream path as independent evidence.
- Maintenance must be explicitly authorized and have an expiry.
- Publish only privacy-safe node labels/IDs and incident summaries; never publish message content, contact-book data, credentials, or exact private node location.
- Authenticate the feed between the status service and clients, and apply rate limits.
- The current static GitHub Pages deployment does not provide this backend. Until a status service is deployed, the page correctly remains in the unknown state.
