load("@with_cfg.bzl", "with_cfg")

alias_linux_x86_64, _alias_linux_x86_64_internal = with_cfg(native.alias).set(
    "platforms",
    [Label("@llvm//platforms:linux_x86_64")],
).build()

alias_windows_x86_64, _alias_windows_x86_64_internal = with_cfg(native.alias).set(
    "platforms",
    [Label("@llvm//platforms:windows_x86_64")],
).build()

alias_macos_x86_64, _alias_macos_x86_64_internal = with_cfg(native.alias).set(
    "platforms",
    [Label("@llvm//platforms:macos_x86_64")],
).build()

alias_macos_aarch64, _alias_macos_aarch64_internal = with_cfg(native.alias).set(
    "platforms",
    [Label("@llvm//platforms:macos_aarch64")],
).build()
