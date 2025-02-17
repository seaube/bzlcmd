copts = select({
    ("@rules_cc//cc/compiler:clang"): [
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
