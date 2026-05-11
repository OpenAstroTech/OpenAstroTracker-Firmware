Import("env")

# Forward --coverage to the linker (PIO build_flags only feed compile flags
# reliably; on native/clang the gcov runtime is pulled in via the link flag).
env.Append(LINKFLAGS=["--coverage"])

# Disable SCons' build cache for this env. Cached .o files lack the companion
# .gcno notes files that gcov/gcovr need, which makes coverage reports empty.
env.CacheDir(None)
