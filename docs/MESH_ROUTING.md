# Mesh routing and node health foundation

This is a hardware-independent foundation. It is not yet wired into the radio packet format or firmware receive loop.

## Route metadata

A route records sender, receiver, ordered node path, and a unique visited-node list. Each node is marked once when it accepts the message for forwarding. A repeated visit is rejected, and a node may only forward to the next unvisited node in the selected path.

The bounded route holds at most 16 nodes. Rerouting preserves visited-node history and excludes already visited intermediate nodes. The sender is marked visited when a fresh route is calculated.

## Route selection

RouteTable stores up to 64 bidirectional link metrics. Dijkstra's algorithm selects the route with the lowest combined metric:

- 1 ms of latency is weighted equally to 1 percentage point of packet loss.
- The current metric is latencyMs * 1000 + packetLossPermille * 100.
- Disabled links and previously visited intermediate nodes are excluded.

This weighting is an initial tunable policy, not a claim that it is optimal for every radio environment. Link metrics need to come from measured observations.

## Retries and cooldown

A link may be retried twice after failed delivery attempts. The next failure marks it temporarily unavailable for 30 seconds. A successful delivery clears the failure history. Cooldown uses wrap-safe millisecond comparisons.

## Node health

Failure reports are deduplicated by reporter identity. Two distinct reporters move a node to SuspectedDown; one reporter only marks it Unstable. A maintenance flag suppresses failure alerts. Two distinct successful reporters restore an unmaintained node to Online.

This is a quorum signal, not cryptographic proof. Reports must be authenticated and correlated against independent paths before they can drive production routing or operator alerts. The module does not yet emit a notification or persist node health.

## Limits and safety

- No radio transmission is enabled or added.
- No wire-format change is made in this first milestone.
- Route metadata authentication, message ACK/delivery states, route advertisement/discovery, and a live mOS dashboard remain follow-up work.
- All tables are fixed-size; no dynamic allocation is used in this module.
- Native tests cover route selection, visited-node handling, rerouting, health quorum, maintenance suppression, and retry cooldown.
