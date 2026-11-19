#include "mos_storage.h"

namespace mos {

StorageState removableStorageState(bool cardDetected, bool mountSucceeded) {
  if (!cardDetected) {
    return StorageState::Absent;
  }
  return mountSucceeded ? StorageState::Ready : StorageState::Error;
}

bool isSafeRelativePath(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }

  size_t length = 0;
  bool segmentHasCharacters = false;
  char previous = '\0';

  for (const char* cursor = path; *cursor != '\0'; ++cursor) {
    const unsigned char ch = static_cast<unsigned char>(*cursor);
    ++length;
    if (length > MAX_RELATIVE_PATH_BYTES) {
      return false;
    }

    // Only printable ASCII and forward-slash separators are accepted. This
    // avoids platform-specific separators and control characters in paths.
    if (ch < 0x20 || ch > 0x7e || ch == '\\' || ch == ':') {
      return false;
    }
    if (cursor == path && ch == '/') {
      return false;
    }
    if (ch == '/') {
      if (!segmentHasCharacters || previous == '/') {
        return false;
      }
      segmentHasCharacters = false;
    } else {
      segmentHasCharacters = true;
    }
    previous = static_cast<char>(ch);
  }

  if (!segmentHasCharacters) {
    return false;
  }

  // Reject traversal segments exactly; names such as "..backup" remain valid.
  const char* segment = path;
  for (const char* cursor = path;; ++cursor) {
    if (*cursor == '/' || *cursor == '\0') {
      const size_t segmentLength = static_cast<size_t>(cursor - segment);
      if ((segmentLength == 1 && segment[0] == '.') ||
          (segmentLength == 2 && segment[0] == '.' && segment[1] == '.')) {
        return false;
      }
      if (*cursor == '\0') {
        break;
      }
      segment = cursor + 1;
    }
  }

  return true;
}

bool isWithinSizeLimit(size_t reportedSize, size_t maximumSize) {
  return reportedSize <= maximumSize;
}

}  // namespace mos
