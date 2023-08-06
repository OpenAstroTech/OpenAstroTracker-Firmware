#ifndef MACROS_HPP
#define MACROS_HPP

#define DO_PRAGMA_(x) _Pragma(#x)
#define DO_PRAGMA(x)  DO_PRAGMA_(x)

#define PUSH_NO_WARNINGS                                                                                                                   \
    DO_PRAGMA(GCC diagnostic push);                                                                                                        \
    DO_PRAGMA(GCC diagnostic ignored "-Wconversion");                                                                                      \
    DO_PRAGMA(GCC diagnostic ignored "-Wdouble-promotion");                                                                                \
    DO_PRAGMA(GCC diagnostic ignored "-Wshadow");                                                                                          \
    DO_PRAGMA(GCC diagnostic ignored "-Wsign-conversion");                                                                                 \
    DO_PRAGMA(GCC diagnostic ignored "-Wsign-compare");                                                                                    \
    DO_PRAGMA(GCC diagnostic ignored "-Wignored-qualifiers");                                                                              \
    DO_PRAGMA(GCC diagnostic ignored "-Wuseless-cast");                                                                                    \
    DO_PRAGMA(GCC diagnostic ignored "-Wunknown-pragmas");                                                                                 \
    DO_PRAGMA(GCC diagnostic ignored "-Wall");                                                                                             \
    DO_PRAGMA(GCC diagnostic ignored "-Wextra");                                                                                           \
    DO_PRAGMA(GCC diagnostic ignored "-Wpedantic");

#define POP_NO_WARNINGS DO_PRAGMA(GCC diagnostic pop);

#endif  // MACROS_HPP