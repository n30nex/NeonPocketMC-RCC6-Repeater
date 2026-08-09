# Releasing NeonPocketMC RCC6 server firmware

The repository workflows build and verify firmware; they do **not** create a GitHub Release automatically.

1. Merge the intended source to `main`.
2. Require successful exact-`main` runs of both `RCC6 MQTT Repeater Build` and `RCC6 Room Server Build`.
3. Download the repeater artifact plus all four Room Server artifacts from those runs.
4. Rename each application and merged recovery image to the public filenames documented in `README.md`.
5. Package the Windows/Linux configurator, exact source and retained license notices.
6. Create one aggregate `SHA256SUMS.txt` covering every attached asset.
7. Create a prerelease tag such as `v1.1.0-rc.1` at the exact `main` commit and upload the verified files.
8. Download every public asset anonymously and verify its size and SHA-256 before announcing the release.

Never publish generic `firmware.bin` names, mix artifacts from different commits, erase SPIFFS for a normal update, or imply that a tag-triggered release workflow exists.
