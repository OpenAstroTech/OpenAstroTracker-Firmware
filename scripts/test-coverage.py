Import("env")

# Ensure coverage flags reach the linker so .gcda files are produced.
env.Append(LINKFLAGS=["--coverage"])
