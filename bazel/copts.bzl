load("@aspect_bazel_lib//lib:transitions.bzl", "platform_transition_filegroup")
load("@bazel_skylib//rules:copy_file.bzl", "copy_file")

PLATFORMS = [
    "@zig_sdk//platform:windows_amd64",
    "@zig_sdk//platform:linux_amd64",
    "@zig_sdk//platform:linux_arm64",
    "@zig_sdk//platform:darwin_amd64",
    "@zig_sdk//platform:darwin_arm64",
]

PLATFORMS_EXT = {
    "windows_amd64": ".exe",
    "linux_amd64": "",
    "linux_arm64": "",
    "darwin_amd64": "",
    "darwin_arm64": "",
}

copts = select({
    "@rules_cc//cc/compiler:clang": [
        "-std=c++20",
        "-fexperimental-library",
    ],
    "@rules_cc//cc/compiler:msvc-cl": [
        "/std:c++20",
        "/permissive-",
        "/Zc:preprocessor",
    ],
    "//conditions:default": [
        "-std=c++20",
    ],
})

def for_all_platforms(name):
    for platform in PLATFORMS:
        platform_name = platform.split(":")[1]
        platform_transition_filegroup(
            name = "for-{}-{}".format(platform_name, name),
            srcs = [":{}".format(name)],
            target_platform = platform,
        )
        copy_file(
            name = "copy-{}-{}".format(name, platform_name),
            src = ":for-{}-{}".format(platform_name, name),
            out = "{}-{}{}".format(
                name,
                platform_name,
                PLATFORMS_EXT[platform_name],
            ),
            is_executable = True,
        )

    native.filegroup(
        name = "all_platforms",
        srcs = [":copy-{}-{}".format(name, platform.split(":")[1]) for platform in PLATFORMS],
    )
