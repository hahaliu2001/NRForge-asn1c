# Revalidate the complete snapshot after compilation, before recording archive.
set(archive "${NRFORGE_SDK_ARCHIVE}")
unset(NRFORGE_SDK_ARCHIVE)
unset(NRFORGE_SDK_ARCHIVE CACHE)
include("${NRFORGE_SOURCE_ROOT}/cmake/VerifyNgapSdk.cmake")
file(SHA256 "${archive}" archive_sha256)
file(WRITE "${NRFORGE_SDK_RECEIPT}"
  "set(NRFORGE_BUILT_FINGERPRINT \"${NRFORGE_SDK_FINGERPRINT}\")\nset(NRFORGE_BUILT_ARCHIVE_SHA256 \"${archive_sha256}\")\n")
