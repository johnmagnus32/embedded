/* init.c — a minimal declarative init + service supervisor (PID 1).
 *
 * Reads /etc/init/<name>.conf, then runs an event-driven loop: start services in `after` order once
 * their deps are up and their `ready` gate is satisfied, respawn dead daemons (with a crash-loop
 * guard), run `oneshot`s to completion, reap orphans, and serve initctl + shutdown — never exiting.
 *
 * The loop blocks in epoll on a signalfd (child exits / shutdown signals), a timerfd (next deadline),
 * and the initctl socket; steady-state it sleeps with zero wakeups. It REQUIRES signalfd/timerfd/epoll
 * + clock_gettime, so it's mainline-only and die()s loudly if they're missing rather than degrading —
 * the from-scratch kernel lacks them and runs INIT=shell until that gap closes (see PLAN.md).
 */
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/reboot.h>
#include <time.h>

#define CONFDIR "/etc/init"
#define MAX_SVC   64
#define MAX_ARGS  32
#define MAX_ENV   16
#define MAX_DEPS  8
#define LINE_CAP  512

/* Supervision timing (real milliseconds; the event loop measures with CLOCK_MONOTONIC). */
#define BACKOFF_MS       1000    /* wait before a dead daemon respawns */
#define READY_TIMEOUT_MS 6000    /* ready-gate timeout, then proceed anyway (never hang boot) */
#define READY_POLL_MS     100    /* how often to re-check a pending `ready` path (startup only) */
#define FAIL_WINDOW_MS  10000    /* crash-loop window */
#define MAX_FAILS           5    /* more than this many deaths within a window => give up (FAILED) */
#define STOP_GRACE_MS    3000    /* shutdown: SIGTERM -> wait this long -> SIGKILL survivors */

#define INITCTL_SOCK "/run/initctl.sock"    /* the control socket initctl talks to */
#define MAX_CONN            8    /* concurrent initctl connections (matches the listen backlog) */
#define CONN_TIMEOUT_MS  2000    /* close a client that hasn't sent a full command line in this long */

/* ---- logging + time ---- */

static void logmsg(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("[init] ", stdout);
	vprintf(fmt, ap);
	fputc('\n', stdout);
	va_end(ap);
	fflush(stdout);
}

/* Monotonic milliseconds — the clock for all backoff/timeout deadlines. */
static long long now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Unrecoverable failure: log and exit. PID 1 exiting halts the kernel — the honest outcome when it
 * can't run this init (missing event primitives); use INIT=shell on a kernel that lacks them. */
static void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("[init] FATAL: ", stdout); vprintf(fmt, ap); fputc('\n', stdout);
	va_end(ap);
	fflush(stdout);
	_exit(1);
}

/* ---- service + connection model ---- */

/* memset(0) => S_WAITING (a fresh svc is startable once deps are up). S_STOPPED = stopped via initctl. */
enum svc_state { S_WAITING, S_RUNNING, S_UP, S_DONE, S_FAILED, S_STOPPED };
static const char *state_str(enum svc_state s) {
	switch (s) {
	case S_WAITING: return "waiting"; case S_RUNNING: return "running"; case S_UP: return "up";
	case S_DONE:    return "done";    case S_FAILED:  return "failed";  case S_STOPPED: return "stopped";
	}
	return "?";
}

/* One service, parsed from <name>.conf. */
struct svc {
	char  name[64];
	char  desc[128];
	char *argv[MAX_ARGS + 1];   /* NULL-terminated; argv[0] set => valid */
	char *env[MAX_ENV + 1];     /* NULL-terminated KEY=VALUE list */
	int   oneshot;
	char  after[128];           /* raw value; split into after_names */
	char *after_names[MAX_DEPS];
	int   after_n;
	char  ready[256];           /* readiness path, or "" for up-on-spawn */
	pid_t pid;                  /* >0 while running */
	enum svc_state state;
	long long started_ms;       /* when the current run began (for the ready-gate timeout) */
	long long backoff_until_ms; /* earliest time a dead daemon may respawn */
	int   fail_count;           /* deaths in the current window */
	long long fail_win_start_ms;/* when the current crash-loop window opened */
	int   stop_req;             /* initctl stop requested -> settle S_STOPPED on exit, don't respawn */
};
static struct svc svcs[MAX_SVC];
static int nsvc;

/* An in-flight initctl connection: read non-blocking, accumulate a partial command line across epoll
 * wakes, dispatch on the first newline, close if it stalls past its deadline. */
struct conn {
	int       fd;               /* -1 = free slot */
	int       len;              /* bytes buffered in buf */
	long long deadline_ms;      /* close if still incomplete past this */
	char      buf[256];
};
static struct conn conns[MAX_CONN];

/* ---- config parsing ---- */

static char *skip_ws(char *s) { while (*s == ' ' || *s == '\t') s++; return s; }
static void  rstrip(char *s) {
	size_t n = strlen(s);
	while (n && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;
}

/* Split into argv on whitespace; single/double quotes group one arg (one layer stripped, like systemd
 * ExecStart). No expansion — use `/bin/sh -c` for pipes/vars/globs. Compacts in place (w never passes r). */
static void set_exec(struct svc *s, const char *val) {
	char *buf = strdup(val);
	if (!buf) return;
	int argc = 0; char *r = buf;
	while (argc < MAX_ARGS) {
		while (*r == ' ' || *r == '\t') r++;
		if (!*r) break;
		char *w = r;                              /* token start; compacted in place */
		s->argv[argc++] = w;
		while (*r && *r != ' ' && *r != '\t') {
			if (*r == '\'' || *r == '"') {        /* quoted run: copy contents, drop the quotes */
				char q = *r++;
				while (*r && *r != q) *w++ = *r++;
				if (*r == q) r++;
			} else {
				*w++ = *r++;
			}
		}
		int more = (*r != 0);                     /* capture before we NUL the delimiter */
		*w = 0;
		if (more) r++;
	}
	s->argv[argc] = NULL;
}

static void add_env(struct svc *s, const char *val) {
	int n = 0; while (s->env[n]) n++;
	if (n >= MAX_ENV) { logmsg("%s: too many env entries (max %d), ignoring '%s'", s->name, MAX_ENV, val); return; }
	s->env[n] = strdup(val);
}

/* Split `after` into dep names (whitespace-separated, no quoting). */
static void set_after(struct svc *s) {
	char *buf = strdup(s->after);
	if (!buf) return;
	char *r = buf;
	while (s->after_n < MAX_DEPS) {
		while (*r == ' ' || *r == '\t') r++;
		if (!*r) break;
		s->after_names[s->after_n++] = r;
		while (*r && *r != ' ' && *r != '\t') r++;
		if (*r) *r++ = 0;
	}
}

/* Parse one <name>.conf into svcs[nsvc]. Line-oriented `key value`; a leading '#' is a full-line
 * comment; the value runs verbatim to end-of-line (no inline comments). */
static void parse_conf(const char *dir, const char *fname) {
	if (nsvc >= MAX_SVC) { logmsg("too many services (max %d), skipping %s", MAX_SVC, fname); return; }
	char nm[64]; snprintf(nm, sizeof nm, "%.*s", (int)(strlen(fname) - 5), fname);   /* strip ".conf" */
	for (int i = 0; i < nsvc; i++) if (!strcmp(svcs[i].name, nm)) return;   /* already loaded (reload is additive) */
	char path[512];
	snprintf(path, sizeof path, "%s/%s", dir, fname);
	FILE *f = fopen(path, "r");
	if (!f) { logmsg("cannot open %s: %s", path, strerror(errno)); return; }

	struct svc *s = &svcs[nsvc];
	memset(s, 0, sizeof *s);
	snprintf(s->name, sizeof s->name, "%s", nm);

	char line[LINE_CAP];
	while (fgets(line, sizeof line, f)) {
		char *p = skip_ws(line);
		if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
		char *key = p;
		while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
		if (*p) *p++ = 0;
		char *val = skip_ws(p);
		rstrip(val);

		if      (!strcmp(key, "exec"))        set_exec(s, val);
		else if (!strcmp(key, "description")) snprintf(s->desc,  sizeof s->desc,  "%s", val);
		else if (!strcmp(key, "oneshot"))     s->oneshot = 1;
		else if (!strcmp(key, "after"))       snprintf(s->after, sizeof s->after, "%s", val);
		else if (!strcmp(key, "ready"))       snprintf(s->ready, sizeof s->ready, "%s", val);
		else if (!strcmp(key, "env"))         add_env(s, val);
		else logmsg("%s: unknown key '%s' (ignored)", s->name, key);
	}
	fclose(f);

	if (!s->argv[0]) { logmsg("%s: no 'exec' — skipping", s->name); return; }
	set_after(s);
	nsvc++;
}

static int is_conf(const char *n) { size_t l = strlen(n); return l > 5 && !strcmp(n + l - 5, ".conf"); }

/* Load every *.conf in filename order (alphasort => deterministic start order). */
static void load_configs(const char *dir) {
	struct dirent **names;
	int n = scandir(dir, &names, NULL, alphasort);
	if (n < 0) { logmsg("no config dir %s (%s) — nothing to supervise", dir, strerror(errno)); return; }
	for (int i = 0; i < n; i++) {
		if (is_conf(names[i]->d_name)) parse_conf(dir, names[i]->d_name);
		free(names[i]);
	}
	free(names);
}

/* ---- early setup (PID-1 boot) ---- */

/* Best-effort: a failure here must not abort PID 1 (exiting panics the kernel), so we log and press on
 * to a debuggable console — like BusyBox/systemd. EBUSY = already mounted (fine). On the custom kernel
 * mount is a no-op; re-opening /dev/console is an upgrade, not a requirement. */
static void mount_be(const char *src, const char *tgt, const char *fs, const char *data) {
	mkdir(tgt, 0755);
	if (mount(src, tgt, fs, 0, data) != 0 && errno != EBUSY)
		logmsg("mount %s on %s: %s (continuing)", fs, tgt, strerror(errno));
}

static void early_setup(void) {
	mount_be("proc", "/proc", "proc",     NULL);
	mount_be("sys",  "/sys",  "sysfs",    NULL);
	mount_be("dev",  "/dev",  "devtmpfs", "mode=0755");
	mount_be("run",  "/run",  "tmpfs",    "mode=0755,size=16m");
	mount_be("tmp",  "/tmp",  "tmpfs",    "mode=1777,size=32m");

	int fd = open("/dev/console", O_RDWR | O_NOCTTY);
	if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }

	setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1);
}

/* ---- service state machine ---- */

static struct svc *by_pid(pid_t p) {
	for (int i = 0; i < nsvc; i++) if (svcs[i].pid == p) return &svcs[i];
	return NULL;
}
static struct svc *svc_by_name(const char *name) {
	for (int i = 0; i < nsvc; i++) if (!strcmp(svcs[i].name, name)) return &svcs[i];
	return NULL;
}

/* A dep is "up" when a daemon is UP or a oneshot is DONE. An unknown dep name counts as up, so a typo
 * can't wedge the boot. */
static int dep_up(const char *name) {
	struct svc *d = svc_by_name(name);
	if (!d) return 1;
	return d->oneshot ? (d->state == S_DONE) : (d->state == S_UP);
}
static int deps_ready(struct svc *s) {
	for (int i = 0; i < s->after_n; i++) if (!dep_up(s->after_names[i])) return 0;
	return 1;
}
static int ready_present(struct svc *s) {
	return s->ready[0] ? (access(s->ready, F_OK) == 0) : 1;
}

static void start_service(struct svc *s, long long now) {
	logmsg("starting %s%s%s", s->name, s->desc[0] ? " — " : "", s->desc);
	pid_t pid = fork();
	if (pid < 0) { logmsg("%s: fork failed: %s", s->name, strerror(errno)); return; }  /* stays WAITING; retried */
	if (pid == 0) {
		sigset_t empty; sigemptyset(&empty);
		sigprocmask(SIG_SETMASK, &empty, NULL);   /* clear init's blocked SIGCHLD (inherited across exec) */
		signal(SIGPIPE, SIG_DFL);                 /* restore default SIGPIPE (init's SIG_IGN survives exec) */
		for (int i = 0; s->env[i]; i++) putenv(s->env[i]);
		execvp(s->argv[0], s->argv);
		fprintf(stderr, "[init] %s: exec %s: %s\n", s->name, s->argv[0], strerror(errno));
		_exit(127);
	}
	s->pid = pid;
	s->started_ms = now;
	s->state = S_RUNNING;
}

/* React to a child exit: a oneshot settles DONE (exit 0) or FAILED; a daemon respawns after a backoff
 * unless it has crash-looped (> MAX_FAILS deaths in FAIL_WINDOW_MS), in which case it's FAILED and left
 * alone so a broken binary can't pin the CPU. */
static void on_child_exit(struct svc *s, int st, long long now) {
	if (WIFSIGNALED(st)) logmsg("%s killed (signal %d)", s->name, WTERMSIG(st));
	else                 logmsg("%s exited (status %d)", s->name, WIFEXITED(st) ? WEXITSTATUS(st) : -1);
	s->pid = 0;

	if (s->stop_req) { s->stop_req = 0; s->state = S_STOPPED; logmsg("%s stopped", s->name); return; }
	if (s->oneshot) {
		if (WIFEXITED(st) && WEXITSTATUS(st) == 0) s->state = S_DONE;
		else { s->state = S_FAILED; logmsg("%s: oneshot failed — not restarting", s->name); }
		return;
	}
	if (now - s->fail_win_start_ms >= FAIL_WINDOW_MS) { s->fail_win_start_ms = now; s->fail_count = 0; }
	s->fail_count++;
	if (s->fail_count > MAX_FAILS) {
		s->state = S_FAILED;
		logmsg("%s: %d deaths in ~%ds — giving up", s->name, s->fail_count, FAIL_WINDOW_MS / 1000);
	} else {
		s->state = S_WAITING;
		s->backoff_until_ms = now + BACKOFF_MS;
	}
}

/* One reconcile pass: start every service whose deps are up + backoff has elapsed, and promote a
 * started daemon to UP once its ready-gate is satisfied (or times out). Returns the number of state
 * changes, so the caller can iterate to a fixpoint (a dep coming up may unblock a dependent). */
static int reconcile(long long now) {
	int changed = 0;
	for (int i = 0; i < nsvc; i++) {
		struct svc *s = &svcs[i];
		if (s->state == S_WAITING && now >= s->backoff_until_ms && deps_ready(s)) {
			start_service(s, now);
			if (s->state == S_RUNNING) changed++;
		}
	}
	for (int i = 0; i < nsvc; i++) {
		struct svc *s = &svcs[i];
		if (s->state != S_RUNNING || s->oneshot) continue;   /* oneshots settle on exit, not readiness */
		if (ready_present(s)) {
			if (s->ready[0]) logmsg("%s: ready", s->name);
			s->state = S_UP; changed++;
		} else if (now - s->started_ms >= READY_TIMEOUT_MS) {
			logmsg("%s: ready gate '%s' timed out — proceeding", s->name, s->ready);
			s->state = S_UP; changed++;
		}
	}
	return changed;
}

/* Milliseconds until the next scheduled action (a backoff expiry, a ready re-check/timeout, or a
 * client's stall deadline), or -1 if nothing is pending — then epoll blocks purely on child exits. */
static int next_delay_ms(long long now) {
	long long best = -1;
	for (int i = 0; i < nsvc; i++) {
		struct svc *s = &svcs[i];
		if (s->state == S_WAITING && deps_ready(s)) {                 /* waiting out a backoff */
			long long d = s->backoff_until_ms - now; if (d < 0) d = 0;
			if (best < 0 || d < best) best = d;
		}
		if (s->state == S_RUNNING && !s->oneshot && s->ready[0]) {    /* polling a pending ready path */
			long long to = (s->started_ms + READY_TIMEOUT_MS) - now; if (to < 0) to = 0;
			if (best < 0 || to < best) best = to;
			if (READY_POLL_MS < best) best = READY_POLL_MS;
		}
	}
	for (int i = 0; i < MAX_CONN; i++)                               /* an initctl client's stall deadline */
		if (conns[i].fd >= 0) {
			long long d = conns[i].deadline_ms - now; if (d < 0) d = 0;
			if (best < 0 || d < best) best = d;
		}
	if (best < 0) return -1;
	if (best > 60000) best = 60000;   /* cap; a stray wake once a minute is harmless */
	return (int)best;
}

/* ---- shutdown ---- */

/* Orderly shutdown — never returns. SIGTERM every running service, reap for up to STOP_GRACE_MS,
 * SIGKILL survivors, then sync + reboot(). On the custom kernel (no reboot syscall) reboot() fails and
 * we _exit(0), which the kernel turns into its halt — so shutdown works everywhere. */
static void shutdown_system(int reboot_cmd, const char *what) {
	logmsg("shutting down (%s) — stopping services", what);
	for (int i = 0; i < nsvc; i++) if (svcs[i].pid > 0) kill(svcs[i].pid, SIGTERM);

	long long deadline = now_ms() + STOP_GRACE_MS;
	for (;;) {
		int st; pid_t pid;
		while ((pid = waitpid(-1, &st, WNOHANG)) > 0) { struct svc *s = by_pid(pid); if (s) s->pid = 0; }
		int alive = 0;
		for (int i = 0; i < nsvc; i++) if (svcs[i].pid > 0) alive++;
		if (!alive || now_ms() >= deadline) break;
		struct timespec ts = { 0, 50 * 1000000L }; nanosleep(&ts, NULL);
	}
	for (int i = 0; i < nsvc; i++)
		if (svcs[i].pid > 0) { logmsg("%s did not stop — SIGKILL", svcs[i].name); kill(svcs[i].pid, SIGKILL); }
	{ int st; while (waitpid(-1, &st, WNOHANG) > 0) { } }   /* final reap sweep */

	logmsg("sync + %s", what);
	sync();
	reboot(reboot_cmd);                                     /* mainline: never returns */
	logmsg("reboot() unavailable (custom kernel) — exiting PID 1 to halt");
	_exit(0);                                               /* kernel: last process exits -> halt */
}

/* ---- initctl control socket ---- */

static struct conn *conn_alloc(int fd, long long now) {
	for (int i = 0; i < MAX_CONN; i++)
		if (conns[i].fd < 0) {
			conns[i].fd = fd; conns[i].len = 0; conns[i].deadline_ms = now + CONN_TIMEOUT_MS;
			return &conns[i];
		}
	return NULL;   /* pool full */
}
static struct conn *conn_by_fd(int fd) {
	for (int i = 0; i < MAX_CONN; i++) if (conns[i].fd == fd) return &conns[i];
	return NULL;
}
static void conn_close(int efd, struct conn *c) {
	epoll_ctl(efd, EPOLL_CTL_DEL, c->fd, NULL);
	close(c->fd);
	c->fd = -1;
}

/* Close any connection still incomplete past its deadline — a stalled client is dropped, not left to
 * block the loop (the whole point of buffering the read instead of reading inside accept). */
static void close_stale_conns(int efd, long long now) {
	for (int i = 0; i < MAX_CONN; i++)
		if (conns[i].fd >= 0 && now >= conns[i].deadline_ms) {
			logmsg("initctl: client stalled — closing");
			conn_close(efd, &conns[i]);
		}
}

/* Act on one initctl command line. Writes the reply to fd; does NOT close (handle_client owns that).
 * poweroff/reboot/halt call shutdown_system, which never returns. */
static void handle_command(int fd, char *line) {
	line[strcspn(line, "\r\n")] = 0;                        /* tolerate a trailing CR */
	char *verb = line, *arg = strchr(line, ' ');
	if (arg) { *arg++ = 0; while (*arg == ' ') arg++; }
	struct svc *s = (arg && *arg) ? svc_by_name(arg) : NULL;
	#define REPLY(str) (void)write(fd, (str), strlen(str))

	if (!strcmp(verb, "poweroff")) { REPLY("ok: powering off\n"); shutdown_system(RB_POWER_OFF,   "poweroff"); }
	if (!strcmp(verb, "reboot"))   { REPLY("ok: rebooting\n");    shutdown_system(RB_AUTOBOOT,    "reboot");   }
	if (!strcmp(verb, "halt"))     { REPLY("ok: halting\n");      shutdown_system(RB_HALT_SYSTEM, "halt");     }

	if (!strcmp(verb, "status")) {
		char l[160];
		for (int i = 0; i < nsvc; i++) {
			if (arg && *arg && strcmp(arg, svcs[i].name)) continue;
			snprintf(l, sizeof l, "%-16.40s %-7.7s pid=%d\n", svcs[i].name, state_str(svcs[i].state), (int)svcs[i].pid);
			REPLY(l);
		}
	} else if (!strcmp(verb, "start")) {
		if (!s) REPLY("no such service\n");
		else if (s->pid > 0) REPLY("already running\n");
		else { s->state = S_WAITING; s->backoff_until_ms = 0; s->stop_req = 0; s->fail_count = 0; REPLY("ok: starting\n"); }
	} else if (!strcmp(verb, "stop")) {
		if (!s) REPLY("no such service\n");
		else if (s->pid > 0) { s->stop_req = 1; kill(s->pid, SIGTERM); REPLY("ok: stopping\n"); }
		else { s->state = S_STOPPED; REPLY("ok: stopped\n"); }
	} else if (!strcmp(verb, "restart")) {
		if (!s) REPLY("no such service\n");
		else if (s->pid > 0) { kill(s->pid, SIGTERM); REPLY("ok: restarting\n"); }   /* respawns via on_child_exit */
		else { s->state = S_WAITING; s->backoff_until_ms = 0; s->fail_count = 0; REPLY("ok: starting\n"); }
	} else if (!strcmp(verb, "reload")) {
		load_configs(CONFDIR); REPLY("ok: reloaded\n");
	} else {
		REPLY("unknown verb (poweroff|reboot|halt|status|start|stop|restart|reload)\n");
	}
	#undef REPLY
}

/* ---- event loop ---- */

/* Register fd for read-readiness (data.fd = fd, so the loop dispatches by fd). */
static void epoll_watch(int efd, int fd) {
	struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
	epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
}

/* Block SIGCHLD/SIGTERM/SIGINT and deliver them synchronously via a signalfd (not async handlers). */
static int open_signalfd(void) {
	sigset_t mask; sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);                           /* a child exited -> reap  */
	sigaddset(&mask, SIGTERM);                           /* -> poweroff             */
	sigaddset(&mask, SIGINT);                            /* ctrl-alt-del -> reboot  */
	sigprocmask(SIG_BLOCK, &mask, NULL);
	return signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
}

/* Create + bind + listen the initctl control socket. /run is a tmpfs from early_setup, so bind works.
 * socket() failure returns -1 (reported by supervise's collective check); bind/listen failure dies. */
static int open_control_socket(void) {
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0) return -1;
	struct sockaddr_un sa; memset(&sa, 0, sizeof sa); sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, INITCTL_SOCK, sizeof sa.sun_path - 1);
	unlink(INITCTL_SOCK);
	if (bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(fd, 8) < 0)
		die("initctl socket %s: %s", INITCTL_SOCK, strerror(errno));
	return fd;
}

/* Reap every child that has exited and advance its service's state (orphans are reaped and ignored). */
static void reap_children(void) {
	long long now = now_ms();
	int st; pid_t pid;
	while ((pid = waitpid(-1, &st, WNOHANG)) > 0) {
		struct svc *s = by_pid(pid);
		if (s) on_child_exit(s, st, now);            /* s == NULL => an orphan we inherited */
	}
}

/* signalfd readable: drain the whole queue first, then act. SIGTERM/SIGINT shut down (never return);
 * SIGCHLD only flags that one reap sweep is due once the queue is fully drained. */
static void handle_signal(int sfd) {
	struct signalfd_siginfo si; int got_chld = 0;
	while (read(sfd, &si, sizeof si) > 0) {
		if      (si.ssi_signo == SIGCHLD) got_chld = 1;
		else if (si.ssi_signo == SIGTERM) shutdown_system(RB_POWER_OFF, "SIGTERM");                /* never returns */
		else if (si.ssi_signo == SIGINT)  shutdown_system(RB_AUTOBOOT, "SIGINT/ctrl-alt-del");     /* never returns */
	}
	if (got_chld) reap_children();
}

/* timerfd readable: consume the expiration count. The deadline it marks (backoff / ready-gate / conn
 * timeout) is acted on by reconcile()/close_stale_conns() at the top of the next loop iteration. */
static void handle_timer(int tfd) {
	uint64_t x; while (read(tfd, &x, sizeof x) > 0) { }
}

/* Arm the timerfd to fire at the next pending deadline, or disarm it when nothing is pending — then
 * the loop blocks purely on child exits and clients. */
static void arm_timer(int tfd, long long now) {
	int d = next_delay_ms(now);
	struct itimerspec its; memset(&its, 0, sizeof its);
	if (d >= 0) {
		its.it_value.tv_sec  = d / 1000;
		its.it_value.tv_nsec = (d % 1000) * 1000000L;
		if (d == 0) its.it_value.tv_nsec = 1;        /* an all-zero itimerspec DISARMs, not fires */
	}
	timerfd_settime(tfd, 0, &its, NULL);
}

/* listen socket readable: accept every pending initctl client and register each as its own epoll
 * source (non-blocking + cloexec). Pool full => reject the client (backpressure), never block. */
static void accept_clients(int efd, int lfd) {
	int cfd;
	while ((cfd = accept4(lfd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC)) >= 0) {
		struct conn *c = conn_alloc(cfd, now_ms());
		if (!c) { close(cfd); continue; }        /* pool full -> reject (backpressure) */
		epoll_watch(efd, cfd);
	}
}

/* An accepted client is readable: drain it (non-blocking) into its buffer; on the first newline run
 * the command and close; on EOF / error / an over-long line, close. A partial line stays buffered
 * (EAGAIN) until the next wake — so a slow or stalled client never blocks the loop. */
static void handle_client(int efd, int fd) {
	struct conn *c = conn_by_fd(fd);
	if (!c) return;
	for (;;) {
		ssize_t r = read(c->fd, c->buf + c->len, sizeof c->buf - 1 - (size_t)c->len);
		if (r > 0) {
			c->len += (int)r;
			char *nl = memchr(c->buf, '\n', (size_t)c->len);
			if (nl) { *nl = 0; handle_command(c->fd, c->buf); conn_close(efd, c); return; }
			if ((size_t)c->len >= sizeof c->buf - 1) { conn_close(efd, c); return; }   /* line too long */
		} else if (r == 0) {
			conn_close(efd, c); return;                                                /* client closed */
		} else {
			if (errno == EINTR)  continue;
			if (errno == EAGAIN) return;                                               /* wait for more */
			conn_close(efd, c); return;                                                /* real error */
		}
	}
}

/* The event-driven supervision loop — never returns. Sets up the fds it waits on, then blocks in epoll
 * on a signalfd (child exit / shutdown signal), a timerfd (next deadline), the initctl listen socket,
 * and any in-flight control connections; with nothing pending it sleeps with zero wakeups. */
static void supervise(void) {
	int sfd = open_signalfd();
	int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	int efd = epoll_create1(EPOLL_CLOEXEC);
	int lfd = open_control_socket();
	if (sfd < 0 || tfd < 0 || efd < 0 || lfd < 0)
		die("signalfd/timerfd/epoll/socket unavailable — this init requires a modern Linux kernel");

	epoll_watch(efd, sfd);                               /* child exits / shutdown signals */
	epoll_watch(efd, tfd);                               /* the next armed deadline        */
	epoll_watch(efd, lfd);                               /* new initctl connections        */

	for (int i = 0; i < MAX_CONN; i++) conns[i].fd = -1; /* mark all control-connection slots free */

	for (;;) {
		long long now = now_ms();
		while (reconcile(now)) now = now_ms();           /* settle cascading starts/promotions */
		close_stale_conns(efd, now);                     /* drop any stalled initctl client */

		arm_timer(tfd, now);                             /* wake at the next deadline, else block on exits/clients */

		struct epoll_event evs[MAX_CONN + 4];
		int n = epoll_wait(efd, evs, MAX_CONN + 4, -1);
		if (n < 0) { if (errno == EINTR) continue; die("epoll_wait: %s", strerror(errno)); }
		for (int i = 0; i < n; i++) {
			int fd = evs[i].data.fd;
			if      (fd == sfd) handle_signal(sfd);         /* SIGCHLD -> reap;  SIGTERM/SIGINT -> shutdown */
			else if (fd == tfd) handle_timer(tfd);          /* a backoff / ready-gate / conn deadline fired */
			else if (fd == lfd) accept_clients(efd, lfd);   /* a new initctl client connected              */
			else                handle_client(efd, fd);     /* an existing client sent a command           */
		}
	}
}

int main(void) {
	logmsg("booting");
	if (getpid() != 1) logmsg("warning: not PID 1 — running degraded (test mode)");

	/* Ignore SIGPIPE: if an initctl client hangs up before we send its reply, the reply write() fails
	 * with EPIPE instead of killing us. Spawned services reset it to the default (see start_service). */
	signal(SIGPIPE, SIG_IGN);

	early_setup();

	load_configs(CONFDIR);
	logmsg("loaded %d service(s) from %s", nsvc, CONFDIR);

	supervise();
	return 0;  /* not reached */
}
