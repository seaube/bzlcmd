# bzlcmd - CLI tools for bazel projects

We use these tools daily, but they may contain bugs. Suggestions or contributions are welcome!

## bzlmod

Create a simple `MODULE.bazel` file and some other bazel files to get you started.

```sh
bzlmod init
```

Add a dependency to your bazel module file with ease! Correctly checks third party registries configured in your `.bazelrc` files as well.

```sh
bzlmod add rules_cc
```

`MODULE.bazel` after command:

```diff
module(name = "my_cool_module")

+bazel_dep(name = "rules_cc", version = "0.0.8")
```

## bzlreg

Create file and folder structure required for a [bazel registry](https://bazel.build/external/registry).

```sh
bzlreg init
```

Add a bazel module from it's archive to the registry.

```
bzlreg add-module http://example.com/some/targz/archive.tar.gz
```
