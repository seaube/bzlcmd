copts = select({
    ("@rules_cc//cc/compiler:clang"): [
        "-std=c++23",
        "-fexperimental-library",
        "-DBOOST_PROCESS_VERSION=1",
        "-DLIBDEFLATE_STATIC",
    ],
    "@rules_cc//cc/compiler:msvc-cl": [
        "/std:c++latest",
        "/permissive-",
        "/Zc:preprocessor",
        "/DBOOST_PROCESS_VERSION=1",
        "/DLIBDEFLATE_STATIC",
    ],
    "//conditions:default": [
        "-std=c++23",
        "-DBOOST_PROCESS_VERSION=1",
        "-DLIBDEFLATE_STATIC",
    ],
})

linkopts = select({
    "@platforms//os:windows": [
        "-lws2_32",
        "-lmswsock",
        "-liphlpapi",
        "-lbcrypt",
    ],
    "//conditions:default": [],
})

