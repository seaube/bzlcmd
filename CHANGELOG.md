# Changelog
All notable changes to this project will be documented in this file. See [conventional commits](https://www.conventionalcommits.org/) for commit guidelines.

- - -
## 0.2.6 - 2025-09-06
#### Bug Fixes
- handle modules without overlay or patch - (6e50176) - Ezekiel Warren
#### Features
- support wider range of archives + name fix (#107) - (1e3a11e) - Ezekiel Warren
- more flexible add archive (#76) - (3872836) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update dependency toolchains_llvm to v1.5.0 (#102) - (cc334fb) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.2.3 (#101) - (4398224) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.2.2 (#100) - (415c30b) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.2.1 (#99) - (87cc84d) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20250818.0 (#98) - (d834af3) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20250814 (#97) - (98b793c) - renovate[bot]
- **(deps)** update actions/checkout action to v5 (#96) - (d4279bd) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20250807.0 (#95) - (1cd44b4) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.2.0 (#94) - (19f14c2) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.1.4 (#93) - (b9e3f37) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.1.3 (#92) - (1d7a517) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.8.1 (#91) - (e9ca74c) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20250701.0 (#90) - (3fb95df) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.8.0 (#89) - (eebbdaa) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20250512.1 (#88) - (491f3cb) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.1.2 (#87) - (7ff25fa) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20250514.0 (#86) - (af16ccb) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20250512 (#85) - (e3d8e81) - renovate[bot]
- **(deps)** update dependency nlohmann_json to v3.12.0 (#84) - (2c767ff) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20250415.0 (#83) - (1cae7c1) - renovate[bot]
- **(deps)** update dependency toolchains_llvm to v1.4.0 (#82) - (3dd61ea) - renovate[bot]
- **(deps)** update dependency bazel to v7.6.1 (#81) - (ae2671c) - renovate[bot]
- **(deps)** update dependency bazel to v7.6.0 (#80) - (9155d0c) - renovate[bot]
- **(deps)** update jidicula/clang-format-action action to v4.15.0 (#79) - (1bb2516) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20250127.1 (#78) - (6e65a96) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20250311.0 (#77) - (be8bbf8) - renovate[bot]
- line endings - (05a2ef9) - Ezekiel Warren
- stop using deprecated bazelboost registry (#103) - (4fbdd88) - Ezekiel Warren

- - -

## 0.2.5 - 2025-02-17
#### Bug Fixes
- crash with EVP_MD_CTX_free - (75a07b9) - Ezekiel Warren
- add missing pragma once - (33305d7) - zaucy
#### Features
- allow running bazel cmd with BCR when not supplying --registry option - (2b54126) - Ezekiel Warren
- shutdown after doing bzlreg bazel commands - (61cd0a9) - Ezekiel Warren
- new bzlreg subcommand for running modules from registry - (345551d) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update dependency boringssl to v0.20250212.0 (#73) - (5570316) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.1.1 (#72) - (aeadbfb) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20250127 (#71) - (76e0b53) - renovate[bot]
- **(deps)** update dependency bazel to v7.5.0 (#70) - (a1d7990) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20240722.1 (#69) - (488e708) - renovate[bot]
- **(deps)** update dependency toolchains_llvm to v1.3.0 (#68) - (c310aac) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20250114.0 (#67) - (1be0961) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20241209.0 (#65) - (64a7cd3) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20240722.0.bcr.2 (#64) - (01af17f) - renovate[bot]
- **(deps)** update jidicula/clang-format-action action to v4.14.0 (#63) - (d6c3a7e) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20240722.0.bcr.1 (#62) - (9aac326) - renovate[bot]
- **(deps)** update dependency bazel to v7.4.1 (#61) - (02b62bd) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.1.0 (#60) - (faa5967) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.0.14 (#59) - (db0411f) - renovate[bot]
- fix some compile errors on linux - (65af576) - zaucy
- llvm update - (9d7a97e) - zaucy
- remove bazel lock from source control - (f4d8eed) - zaucy

- - -

## 0.2.4 - 2024-11-05
#### Features
- new update subcommand - (2a2bd94) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update dependency boringssl to v0.20241024.0 (#58) - (483e86f) - renovate[bot]
- **(deps)** update dependency bazel to v7.4.0 (#57) - (4c516b5) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.0.13 (#56) - (86f3b6d) - renovate[bot]
- **(deps)** update dependency nlohmann_json to v3.11.3.bcr.1 (#55) - (e7ea383) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.0.12 (#54) - (9781186) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.0.11 (#53) - (25a8289) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20240930.0 (#52) - (72f7985) - renovate[bot]
- **(deps)** update dependency bazel to v7.3.2 (#51) - (b1ae474) - renovate[bot]
- **(deps)** update dependency toolchains_llvm to v1.2.0 (#50) - (953ea03) - renovate[bot]
- **(deps)** update dependency boringssl to v0.20240913.0 (#49) - (d357a7b) - renovate[bot]
- **(deps)** update dependency rules_cc to v0.0.10 (#48) - (124a0f7) - renovate[bot]
- **(deps)** update dependency bazel to v7.3.1 (#47) - (b8c7e34) - renovate[bot]
- **(deps)** update dependency bazel to v7.3.0 (#46) - (382b404) - renovate[bot]
- update lock - (85ac9f5) - Ezekiel Warren

- - -

## 0.2.3 - 2024-08-09
#### Bug Fixes
- compatibility_level is optional (#45) - (d8f1730) - Ezekiel Warren
#### Features
- better error message with bad data (#44) - (9ab0f0f) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update dependency abseil-cpp to v20240722 (#42) - (e9e826b) - renovate[bot]
- **(deps)** update dependency bazel to v7.2.1 (#40) - (080cb6d) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20240116 (#23) - (c5c0d9a) - renovate[bot]
- **(deps)** update dependency toolchains_llvm to v1.1.2 (#41) - (9c47b5d) - renovate[bot]
- **(deps)** update dependency bazel to v7.2.0 (#39) - (5f24c10) - renovate[bot]
- **(deps)** update dependency boost.asio to v1.83.0.bzl.4 (#38) - (7ab2f28) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.7.1 (#37) - (b55affd) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.7.0 (#36) - (3baafc6) - renovate[bot]
- **(deps)** update jidicula/clang-format-action action to v4.13.0 (#35) - (32aec59) - renovate[bot]
- **(deps)** update jidicula/clang-format-action action to v4.12.0 (#34) - (d416806) - renovate[bot]
- **(deps)** update dependency bazel to v7.1.2 (#33) - (2935ef1) - renovate[bot]
- **(deps)** update dependency boringssl to v0.0.0-20240530-2db0eb3 (#32) - (7c5ebac) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.6.1 (#31) - (84d3c10) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.6.0 (#30) - (d3fbd80) - renovate[bot]
- formatting - (d0aacad) - Ezekiel Warren
- update clang format version - (6715fd6) - Ezekiel Warren
- ignore bazel lock files - (7e16a44) - Ezekiel Warren

- - -

## 0.2.2 - 2024-04-03
#### Miscellaneous Chores
- **(deps)** update dependency rules_cc to v0.0.9 (#28) - (746d302) - renovate[bot]
- **(deps)** update dependency boringssl to v0.0.0-20240126-22d349c (#24) - (3e150ec) - renovate[bot]
- **(deps)** update actions/cache action to v4 (#22) - (e623961) - renovate[bot]
- **(deps)** update dependency libdeflate to v1.19 (#17) - (10659b4) - renovate[bot]
- **(deps)** update dependency nlohmann_json to v3.11.3 (#21) - (69411b7) - renovate[bot]
- **(deps)** update dependency bazel_skylib to v1.5.0 (#18) - (1424775) - renovate[bot]
- **(deps)** update dependency boost.asio to v1.83.0.bzl.3 (#20) - (4cee416) - renovate[bot]
- **(deps)** update dependency abseil-cpp to v20230802.1 (#19) - (a5d1d67) - renovate[bot]
- **(deps)** update dependency bazel to v7.1.1 (#26) - (b671123) - renovate[bot]
- **(deps)** update dependency toolchains_llvm to v1 (#25) - (4cfe7e8) - renovate[bot]
- enable renovate automerge - (be4c70d) - Ezekiel Warren
- enable github merge queue - (cb69b9e) - Ezekiel Warren
- update test with problem archive (#27) - (199945b) - Ezekiel Warren
- add simple install script for windows - (6d27ee4) - Ezekiel Warren
- bzlmod + bazel updates - (3fe6a80) - Ezekiel Warren

- - -

## 0.2.1 - 2023-09-26
#### Bug Fixes
- handle optional 'repository' field - (03c4865) - Ezekiel Warren
- remove unused field - (a253a26) - Ezekiel Warren
#### Miscellaneous Chores
- **(deps)** update com_grail_bazel_toolchain digest to 2c25456 (#10) - (ea6ad20) - renovate[bot]
- **(deps)** update jidicula/clang-format-action action to v4.11.0 (#11) - (00eb00d) - renovate[bot]
- add disclaimer to README - (ee4f8a1) - Ezekiel Warren
- fix typo in README - (4ac35fe) - Ezekiel Warren
- add license - (e79593e) - Ezekiel Warren

- - -

## 0.2.0 - 2023-09-14
#### Bug Fixes
- actually get registries (#4) - (b373d70) - Ezekiel Warren
#### Features
- bzlmod CLI (#3) - (58fdcbb) - Ezekiel Warren
- new --strip-prefix flag - (b096367) - Ezekiel Warren
#### Miscellaneous Chores
- add release github script (#8) - (a5feb1c) - Ezekiel Warren
- add tests (#7) - (743a9da) - Ezekiel Warren
- add cache to CI + formatting (#6) - (3b1d509) - Ezekiel Warren
- rename to bzlcmd + readme update (#5) - (f8eab26) - Ezekiel Warren

- - -

## 0.1.0 - 2023-09-13
#### Features
- add-module command (#2) - (6f38c94) - Ezekiel Warren
#### Miscellaneous Chores
- add cog.toml - (47e18a8) - Ezekiel Warren
- wip - (4311708) - Ezekiel Warren

- - -

Changelog generated by [cocogitto](https://github.com/cocogitto/cocogitto).