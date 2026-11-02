#pragma once
#include <stddef.h>
#include <stdint.h>

namespace ml {

static constexpr size_t ROUTE_MAX_NODES = 16;
static constexpr size_t ROUTE_MAX_KNOWN_NODES = 32;
static constexpr size_t ROUTE_MAX_LINKS = 64;
static constexpr size_t ROUTE_MAX_HEALTH_REPORTERS = 16;
static constexpr uint8_t ROUTE_MAX_RETRIES = 2;
static constexpr uint32_t ROUTE_UNAVAILABLE_TTL_MS = 30000;

struct RoutePath {
  uint8_t sender[8];
  uint8_t receiver[8];
  uint8_t nodes[ROUTE_MAX_NODES][8];
  uint8_t nodeCount;
  uint8_t visited[ROUTE_MAX_NODES][8];
  uint8_t visitedCount;

  RoutePath();
  bool markVisited(const uint8_t nodeId[8]);
  bool hasVisited(const uint8_t nodeId[8]) const;
  bool nextHop(const uint8_t currentNode[8], uint8_t outNextHop[8]) const;
  bool replaceRemaining(const uint8_t currentNode[8],
                        const uint8_t (*newNodes)[8], size_t count);
  bool isValid() const;
};

struct LinkMetric {
  uint8_t from[8];
  uint8_t to[8];
  uint16_t latencyMs;
  uint16_t packetLossPermille;
  bool enabled;
};

class RouteTable {
 public:
  RouteTable();
  bool upsertLink(const uint8_t from[8], const uint8_t to[8],
                  uint16_t latencyMs, uint16_t packetLossPermille);
  bool setLinkEnabled(const uint8_t from[8], const uint8_t to[8], bool enabled);
  bool calculate(const uint8_t sender[8], const uint8_t receiver[8],
                 const RoutePath& current, RoutePath& result) const;
  size_t linkCount() const { return linkCount_; }

 private:
  LinkMetric links_[ROUTE_MAX_LINKS];
  size_t linkCount_;
};

enum class NodeHealth : uint8_t {
  Online, Unstable, SuspectedDown, Maintenance, Down, Recovering
};

class NodeHealthMonitor {
 public:
  static constexpr uint8_t DOWN_REPORT_QUORUM = 2;
  static constexpr uint8_t RECOVERY_SUCCESS_QUORUM = 2;
  NodeHealthMonitor();
  bool setMaintenance(const uint8_t nodeId[8], bool enabled);
  bool reportFailure(const uint8_t target[8], const uint8_t reporter[8], uint32_t nowMs);
  bool reportSuccess(const uint8_t target[8], const uint8_t reporter[8], uint32_t nowMs);
  NodeHealth state(const uint8_t nodeId[8]) const;

 private:
  struct Entry {
    uint8_t nodeId[8];
    uint8_t failureReporters[ROUTE_MAX_HEALTH_REPORTERS][8];
    uint8_t successReporters[ROUTE_MAX_HEALTH_REPORTERS][8];
    uint8_t failureCount;
    uint8_t successCount;
    uint32_t lastReportMs;
    NodeHealth state;
    bool occupied;
  };
  Entry entries_[ROUTE_MAX_KNOWN_NODES];
  Entry* findOrCreate(const uint8_t nodeId[8]);
  const Entry* find(const uint8_t nodeId[8]) const;
};

class LinkRetryPolicy {
 public:
  LinkRetryPolicy();
  bool recordFailure(const uint8_t from[8], const uint8_t to[8], uint32_t nowMs);
  void markSuccess(const uint8_t from[8], const uint8_t to[8]);
  void expire(uint32_t nowMs);
  bool isTemporarilyUnavailable(const uint8_t from[8], const uint8_t to[8],
                                uint32_t nowMs) const;

 private:
  struct Entry {
    uint8_t from[8];
    uint8_t to[8];
    uint8_t failures;
    uint32_t unavailableUntil;
    bool occupied;
  };
  static constexpr size_t CAPACITY = ROUTE_MAX_LINKS;
  Entry entries_[CAPACITY];
  Entry* findOrCreate(const uint8_t from[8], const uint8_t to[8]);
  const Entry* find(const uint8_t from[8], const uint8_t to[8]) const;
};

}  // namespace ml
