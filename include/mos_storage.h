#pragma once

#include <stddef.h>

namespace mos {

enum class StorageState {
  Absent,
  Ready,
  Error
};

enum class StorageTier {
  InternalFlash,
  Removable
};

// Removable storage is optional: a missing card is not a boot failure.
StorageState removableStorageState(bool cardDetected, bool mountSucceeded);

// Validate a path supplied to the storage API before passing it to a filesystem.
// Paths are relative to the mOS data root and use forward slashes only.
bool isSafeRelativePath(const char* path);

// Reject a file before reading it when its reported size exceeds the caller's
// explicit limit. A limit of zero intentionally rejects every non-empty file.
bool isWithinSizeLimit(size_t reportedSize, size_t maximumSize);

static constexpr size_t MAX_RELATIVE_PATH_BYTES = 96;

}  // namespace mos
