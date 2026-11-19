#include "mesh_route.h"
#include <string.h>

namespace ml {
namespace {
bool sameId(const uint8_t a[8], const uint8_t b[8]) {
  return a && b && memcmp(a, b, 8) == 0;
}
bool zeroId(const uint8_t id[8]) {
  if (!id) return true;
  uint8_t value = 0;
  for (size_t i = 0; i < 8; ++i) value |= id[i];
  return value == 0;
}
bool contains(const uint8_t ids[][8], size_t count, const uint8_t id[8]) {
  for (size_t i = 0; i < count; ++i) if (sameId(ids[i], id)) return true;
  return false;
}
uint32_t metricCost(uint16_t latency, uint16_t loss) {
  // One millisecond is weighted like one percentage point of packet loss.
  return static_cast<uint32_t>(latency) * 1000U +
         static_cast<uint32_t>(loss) * 100U;
}
}

RoutePath::RoutePath() : nodeCount(0), visitedCount(0) {
  memset(sender, 0, sizeof(sender));
  memset(receiver, 0, sizeof(receiver));
  memset(nodes, 0, sizeof(nodes));
  memset(visited, 0, sizeof(visited));
}

bool RoutePath::hasVisited(const uint8_t id[8]) const {
  return !zeroId(id) && contains(visited, visitedCount, id);
}

bool RoutePath::markVisited(const uint8_t id[8]) {
  if (zeroId(id) || hasVisited(id) || visitedCount >= ROUTE_MAX_NODES) return false;
  memcpy(visited[visitedCount++], id, 8);
  return true;
}

bool RoutePath::isValid() const {
  if (zeroId(sender) || zeroId(receiver) || nodeCount < 2 ||
      nodeCount > ROUTE_MAX_NODES || visitedCount > ROUTE_MAX_NODES ||
      !sameId(nodes[0], sender) || !sameId(nodes[nodeCount - 1], receiver)) return false;
  for (size_t i = 0; i < nodeCount; ++i) {
    if (zeroId(nodes[i])) return false;
    for (size_t j = i + 1; j < nodeCount; ++j)
      if (sameId(nodes[i], nodes[j])) return false;
  }
  for (size_t i = 0; i < visitedCount; ++i) {
    if (zeroId(visited[i])) return false;
    for (size_t j = i + 1; j < visitedCount; ++j)
      if (sameId(visited[i], visited[j])) return false;
  }
  return true;
}

bool RoutePath::nextHop(const uint8_t current[8], uint8_t out[8]) const {
  if (!current || !out || !isValid() || !hasVisited(current)) return false;
  for (size_t i = 0; i + 1 < nodeCount; ++i) {
    if (sameId(nodes[i], current)) {
      if (hasVisited(nodes[i + 1])) return false;
      memcpy(out, nodes[i + 1], 8);
      return true;
    }
  }
  return false;
}

bool RoutePath::replaceRemaining(const uint8_t current[8],
                                 const uint8_t (*newNodes)[8], size_t count) {
  if (!current || !newNodes || count < 2 || count > ROUTE_MAX_NODES ||
      !isValid() || !hasVisited(current) || !sameId(newNodes[0], current)) return false;
  if (!sameId(newNodes[count - 1], receiver)) return false;
  for (size_t i = 0; i < count; ++i) {
    if (zeroId(newNodes[i])) return false;
    for (size_t j = i + 1; j < count; ++j)
      if (sameId(newNodes[i], newNodes[j])) return false;
    if (i > 0 && hasVisited(newNodes[i])) return false;
  }
  memset(nodes, 0, sizeof(nodes));
  for (size_t i = 0; i < count; ++i) memcpy(nodes[i], newNodes[i], 8);
  nodeCount = static_cast<uint8_t>(count);
  return isValid();
}

RouteTable::RouteTable() : linkCount_(0) {
  memset(links_, 0, sizeof(links_));
}

bool RouteTable::upsertLink(const uint8_t from[8], const uint8_t to[8],
                            uint16_t latency, uint16_t loss) {
  if (zeroId(from) || zeroId(to) || sameId(from, to) ||
      latency == 0 || loss > 1000) return false;
  for (size_t i = 0; i < linkCount_; ++i) {
    if ((sameId(links_[i].from, from) && sameId(links_[i].to, to)) ||
        (sameId(links_[i].from, to) && sameId(links_[i].to, from))) {
      links_[i].latencyMs = latency;
      links_[i].packetLossPermille = loss;
      links_[i].enabled = true;
      return true;
    }
  }
  if (linkCount_ >= ROUTE_MAX_LINKS) return false;
  LinkMetric& link = links_[linkCount_++];
  memcpy(link.from, from, 8);
  memcpy(link.to, to, 8);
  link.latencyMs = latency;
  link.packetLossPermille = loss;
  link.enabled = true;
  return true;
}

bool RouteTable::setLinkEnabled(const uint8_t from[8], const uint8_t to[8], bool enabled) {
  if (zeroId(from) || zeroId(to)) return false;
  for (size_t i = 0; i < linkCount_; ++i) {
    if ((sameId(links_[i].from, from) && sameId(links_[i].to, to)) ||
        (sameId(links_[i].from, to) && sameId(links_[i].to, from))) {
      links_[i].enabled = enabled;
      return true;
    }
  }
  return false;
}

bool RouteTable::calculate(const uint8_t senderId[8], const uint8_t receiverId[8],
                           const RoutePath& current, RoutePath& result) const {
  if (zeroId(senderId) || zeroId(receiverId) || sameId(senderId, receiverId)) return false;
  uint8_t ids[ROUTE_MAX_KNOWN_NODES][8] = {};
  size_t count = 0;
  for (size_t i = 0; i < linkCount_; ++i) {
    if (!contains(ids, count, links_[i].from) && count < ROUTE_MAX_KNOWN_NODES)
      memcpy(ids[count++], links_[i].from, 8);
    if (!contains(ids, count, links_[i].to) && count < ROUTE_MAX_KNOWN_NODES)
      memcpy(ids[count++], links_[i].to, 8);
  }
  if (!contains(ids, count, senderId) && count < ROUTE_MAX_KNOWN_NODES)
    memcpy(ids[count++], senderId, 8);
  if (!contains(ids, count, receiverId) && count < ROUTE_MAX_KNOWN_NODES)
    memcpy(ids[count++], receiverId, 8);
  if (!contains(ids, count, senderId) || !contains(ids, count, receiverId)) return false;

  uint32_t distance[ROUTE_MAX_KNOWN_NODES];
  int16_t previous[ROUTE_MAX_KNOWN_NODES];
  bool visited[ROUTE_MAX_KNOWN_NODES] = {};
  for (size_t i = 0; i < count; ++i) { distance[i] = UINT32_MAX; previous[i] = -1; }
  size_t sourceIndex = 0, targetIndex = 0;
  for (size_t i = 0; i < count; ++i) {
    if (sameId(ids[i], senderId)) sourceIndex = i;
    if (sameId(ids[i], receiverId)) targetIndex = i;
  }
  distance[sourceIndex] = 0;

  for (size_t step = 0; step < count; ++step) {
    size_t best = count;
    for (size_t i = 0; i < count; ++i)
      if (!visited[i] && (best == count || distance[i] < distance[best])) best = i;
    if (best == count || distance[best] == UINT32_MAX) break;
    if (best == targetIndex) break;
    visited[best] = true;
    for (size_t e = 0; e < linkCount_; ++e) {
      const LinkMetric& link = links_[e];
      if (!link.enabled) continue;
      const bool fromMatch = sameId(link.from, ids[best]);
      const bool toMatch = sameId(link.to, ids[best]);
      if (!fromMatch && !toMatch) continue;
      const uint8_t* neighbor = fromMatch ? link.to : link.from;
      size_t next = count;
      for (size_t i = 0; i < count; ++i) if (sameId(ids[i], neighbor)) { next = i; break; }
      if (next == count || visited[next] || (current.hasVisited(neighbor) && !sameId(neighbor, receiverId))) continue;
      const uint32_t cost = metricCost(link.latencyMs, link.packetLossPermille);
      if (distance[best] <= UINT32_MAX - cost && distance[best] + cost < distance[next]) {
        distance[next] = distance[best] + cost;
        previous[next] = static_cast<int16_t>(best);
      }
    }
  }
  if (distance[targetIndex] == UINT32_MAX) return false;
  uint8_t reverse[ROUTE_MAX_NODES][8] = {};
  size_t pathCount = 0;
  int16_t cursor = static_cast<int16_t>(targetIndex);
  while (cursor >= 0 && pathCount < ROUTE_MAX_NODES) {
    memcpy(reverse[pathCount++], ids[cursor], 8);
    if (static_cast<size_t>(cursor) == sourceIndex) break;
    cursor = previous[cursor];
  }
  if (pathCount < 2 || !sameId(reverse[pathCount - 1], senderId)) return false;
  RoutePath candidate;
  memcpy(candidate.sender, senderId, 8);
  memcpy(candidate.receiver, receiverId, 8);
  candidate.nodeCount = static_cast<uint8_t>(pathCount);
  for (size_t i = 0; i < pathCount; ++i)
    memcpy(candidate.nodes[i], reverse[pathCount - i - 1], 8);
  for (size_t i = 0; i < current.visitedCount; ++i)
    if (!candidate.markVisited(current.visited[i])) return false;
  if (!candidate.hasVisited(senderId) && !candidate.markVisited(senderId)) return false;
  result = candidate;
  return result.isValid();
}

NodeHealthMonitor::NodeHealthMonitor() { memset(entries_, 0, sizeof(entries_)); }

NodeHealthMonitor::Entry* NodeHealthMonitor::findOrCreate(const uint8_t id[8]) {
  if (zeroId(id)) return nullptr;
  for (size_t i = 0; i < ROUTE_MAX_KNOWN_NODES; ++i)
    if (entries_[i].occupied && sameId(entries_[i].nodeId, id)) return &entries_[i];
  for (size_t i = 0; i < ROUTE_MAX_KNOWN_NODES; ++i) if (!entries_[i].occupied) {
    Entry& entry = entries_[i];
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.nodeId, id, 8);
    entry.state = NodeHealth::Online;
    entry.occupied = true;
    return &entry;
  }
  return nullptr;
}

const NodeHealthMonitor::Entry* NodeHealthMonitor::find(const uint8_t id[8]) const {
  if (zeroId(id)) return nullptr;
  for (size_t i = 0; i < ROUTE_MAX_KNOWN_NODES; ++i)
    if (entries_[i].occupied && sameId(entries_[i].nodeId, id)) return &entries_[i];
  return nullptr;
}

bool NodeHealthMonitor::setMaintenance(const uint8_t id[8], bool enabled) {
  Entry* entry = findOrCreate(id);
  if (!entry) return false;
  entry->state = enabled ? NodeHealth::Maintenance : NodeHealth::Recovering;
  entry->failureCount = entry->successCount = 0;
  memset(entry->failureReporters, 0, sizeof(entry->failureReporters));
  memset(entry->successReporters, 0, sizeof(entry->successReporters));
  return true;
}

bool NodeHealthMonitor::reportFailure(const uint8_t target[8], const uint8_t reporter[8], uint32_t now) {
  if (zeroId(target) || zeroId(reporter) || sameId(target, reporter)) return false;
  Entry* entry = findOrCreate(target);
  if (!entry || entry->state == NodeHealth::Maintenance) return false;
  if (contains(entry->failureReporters, entry->failureCount, reporter)) return true;
  if (entry->failureCount < ROUTE_MAX_HEALTH_REPORTERS)
    memcpy(entry->failureReporters[entry->failureCount++], reporter, 8);
  entry->lastReportMs = now;
  entry->successCount = 0;
  memset(entry->successReporters, 0, sizeof(entry->successReporters));
  entry->state = entry->failureCount >= DOWN_REPORT_QUORUM ? NodeHealth::SuspectedDown : NodeHealth::Unstable;
  return true;
}

bool NodeHealthMonitor::reportSuccess(const uint8_t target[8], const uint8_t reporter[8], uint32_t now) {
  if (zeroId(target) || zeroId(reporter) || sameId(target, reporter)) return false;
  Entry* entry = findOrCreate(target);
  if (!entry) return false;
  if (entry->state == NodeHealth::Maintenance) return false;
  if (!contains(entry->successReporters, entry->successCount, reporter) &&
      entry->successCount < ROUTE_MAX_HEALTH_REPORTERS)
    memcpy(entry->successReporters[entry->successCount++], reporter, 8);
  entry->lastReportMs = now;
  entry->failureCount = 0;
  memset(entry->failureReporters, 0, sizeof(entry->failureReporters));
  entry->state = entry->successCount >= RECOVERY_SUCCESS_QUORUM ? NodeHealth::Online : NodeHealth::Recovering;
  return true;
}

NodeHealth NodeHealthMonitor::state(const uint8_t id[8]) const {
  const Entry* entry = find(id);
  return entry ? entry->state : NodeHealth::Online;
}

LinkRetryPolicy::LinkRetryPolicy() { memset(entries_, 0, sizeof(entries_)); }

LinkRetryPolicy::Entry* LinkRetryPolicy::findOrCreate(const uint8_t from[8], const uint8_t to[8]) {
  if (zeroId(from) || zeroId(to) || sameId(from, to)) return nullptr;
  for (size_t i = 0; i < CAPACITY; ++i)
    if (entries_[i].occupied && sameId(entries_[i].from, from) && sameId(entries_[i].to, to)) return &entries_[i];
  for (size_t i = 0; i < CAPACITY; ++i) if (!entries_[i].occupied) {
    Entry& entry = entries_[i];
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.from, from, 8);
    memcpy(entry.to, to, 8);
    entry.occupied = true;
    return &entry;
  }
  return nullptr;
}

const LinkRetryPolicy::Entry* LinkRetryPolicy::find(const uint8_t from[8], const uint8_t to[8]) const {
  for (size_t i = 0; i < CAPACITY; ++i)
    if (entries_[i].occupied && sameId(entries_[i].from, from) && sameId(entries_[i].to, to)) return &entries_[i];
  return nullptr;
}

bool LinkRetryPolicy::recordFailure(const uint8_t from[8], const uint8_t to[8], uint32_t now) {
  Entry* entry = findOrCreate(from, to);
  if (!entry || entry->unavailableUntil != 0) return false;
  if (entry->failures < ROUTE_MAX_RETRIES) {
    ++entry->failures;
    return true;
  }
  entry->unavailableUntil = now + ROUTE_UNAVAILABLE_TTL_MS;
  return false;
}

void LinkRetryPolicy::markSuccess(const uint8_t from[8], const uint8_t to[8]) {
  Entry* entry = findOrCreate(from, to);
  if (!entry) return;
  entry->failures = 0;
  entry->unavailableUntil = 0;
}

void LinkRetryPolicy::expire(uint32_t now) {
  for (size_t i = 0; i < CAPACITY; ++i) {
    Entry& entry = entries_[i];
    if (entry.occupied && entry.unavailableUntil != 0 &&
        static_cast<int32_t>(now - entry.unavailableUntil) >= 0) {
      entry.unavailableUntil = 0;
      entry.failures = 0;
    }
  }
}

bool LinkRetryPolicy::isTemporarilyUnavailable(const uint8_t from[8], const uint8_t to[8],
                                                uint32_t now) const {
  const Entry* entry = find(from, to);
  return entry && entry->unavailableUntil != 0 &&
         static_cast<int32_t>(now - entry->unavailableUntil) < 0;
}

}  // namespace ml
