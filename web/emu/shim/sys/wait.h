/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Blair (blairysbeatlab), FM-1 Groovebox */
/* wasi has no processes: hostsim.c's fork/pipe/waitpid paths are never reached in the browser build */
#pragma once
static inline int fork(void) { return -1; }
static inline int pipe(int *f) { (void)f; return -1; }
static inline int waitpid(int p, int *s, int o) { (void)p; (void)s; (void)o; return -1; }
#define WIFEXITED(x) 0
#define WEXITSTATUS(x) 0
