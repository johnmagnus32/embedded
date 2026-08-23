/* init.c — a minimal declarative init + service supervisor (PID 1).
 *
 * Reads /etc/init/<name>.conf, then runs an EVENT-DRIVEN supervision loop: start services in `after`
 * order once their deps are up and their `ready` gate is satisfied (bounded), respawn daemons that
 * exit (with a crash-loop guard), run `oneshot`s to completion, and reap orphans — forever, never
 * exiting. Best-effort early setup (mount the API filesystems, take /dev/console, set PATH) precedes it.
 * Shutdown + initctl are later phases (see PLAN.md).
 *
 * The loop blocks in epoll on a signalfd (SIGCHLD => a child exited) and a timerfd (next backoff /
 * ready-gate deadline), so when everything is steady it sleeps with ZERO wakeups until a child dies
 * — the systemd model. It REQUIRES signalfd/timerfd/epoll + clock_gettime, so it's MAINLINE-ONLY:
 * the from-scratch kernel lacks them (a deferred kernel gap — see PLAN.md) and runs INIT=shell until
 * that gap is closed. If those syscalls are missing, init fails loudly (die) rather than degrading
 * to a lesser supervisor — fix the kernel, don't work around it here.
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

#define CONFDIR_DEFAULT "/etc/init"
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

static void logmsg(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("[init] ", stdout);
	vprintf(fmt, ap);
	fputc('\n', stdout);
	va_end(ap);
	fflush(stdout);
}

/* Monotonic milliseconds — the supervisor's clock for backoff/timeout deadlines. */
static long long now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Unrecoverable setup failure — log loudly and exit (PID 1 exiting halts the kernel, which is the
 * honest outcome: this kernel can't run this init). Only reachable if the event primitives this
 * init REQUIRES are missing — i.e. a kernel that hasn't closed that gap; use INIT=shell there. */
static void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("[init] FATAL: ", stdout); vprintf(fmt, ap); fputc('\n', stdout);
	va_end(ap);
	fflush(stdout);
	_exit(1);
}

/* Service state. memset(0) => S_WAITING, so a fresh svc is eligible to start once its deps are up.
 * S_STOPPED = manually stopped via initctl (not eligible to start, not respawned). */
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
static const char *g_confdir = CONFDIR_DEFAULT;   /* set in main; reused by `initctl reload` */

/* ---- parsing helpers ---- */
static char *skip_ws(char *s) { while (*s == ' ' || *s == '\t') s++; return s; }
static void  rstrip(char *s) {
	size_t n = strlen(s);
	while (n && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;
}

/* Split a command line into argv on whitespace, honoring single/double quotes for grouping
 * (one layer stripped, like systemd's ExecStart) — so `exec /bin/sh -c 'echo hi | tr a-z A-Z'`
 * passes the whole command as one arg. NO variable/glob expansion; that's what /bin/sh -c is for.
 * Compacts in place into a persistent strdup (write ptr never overtakes read ptr). */
static void set_exec(struct svc *s, const char *val) {
	char *buf = strdup(val);
	if (!buf) return;
	int argc = 0; char *r = buf;
	while (argc < MAX_ARGS) {
		while (*r == ' ' || *r == '\t') r++;      /* skip whitespace between tokens */
		if (!*r) break;
		char *w = r;                              /* token start; compact into it */
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

/* Split the `after` value into dep names (service names — whitespace-separated, no quoting). */
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

/* Parse one <name>.conf into svcs[nsvc]. Line-oriented `key value`; a leading '#' is a
 * full-line comment; the value runs verbatim to end-of-line (no inline comments). */
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
		if (*p) *p++ = 0;                 /* terminate key */
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

/* Scan the config dir in filename order (deterministic). */
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

/* ---- early setup ----
 * Best-effort by design: a failure here must NOT abort PID 1 (exiting panics the kernel), so we
 * log and press on far enough to still get a debuggable console — same as BusyBox/systemd.
 *   - EBUSY: already mounted (kernel / earlier stage / the initramfs cpio) — goal achieved, ignore.
 *   - a real error (ENODEV = kernel lacks the fs, EINVAL, ENOENT): a genuine misconfig — logged
 *     loudly, but we continue rather than panic; the downstream failure will surface with a breadcrumb.
 *   - EPERM only happens unprivileged (a host-side test, or a container without CAP_SYS_ADMIN);
 *     it cannot occur in a real root PID-1 boot.
 * On the custom kernel `mount` is a no-op returning 0, so it can't fail there. If /dev/console can't
 * be opened we keep the stdio the kernel already gave PID 1 — the re-open is an upgrade, not a need. */
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

/* ---- start + supervise ---- */
static void start_service(struct svc *s, long long now) {
	logmsg("starting %s%s%s", s->name, s->desc[0] ? " — " : "", s->desc);
	pid_t pid = fork();
	if (pid < 0) { logmsg("%s: fork failed: %s", s->name, strerror(errno)); return; }  /* stays WAITING; retried */
	if (pid == 0) {
		sigset_t empty; sigemptyset(&empty);
		sigprocmask(SIG_SETMASK, &empty, NULL);   /* clear init's blocked SIGCHLD (inherited across exec) */
		for (int i = 0; s->env[i]; i++) putenv(s->env[i]);
		execvp(s->argv[0], s->argv);
		fprintf(stderr, "[init] %s: exec %s: %s\n", s->name, s->argv[0], strerror(errno));
		_exit(127);
	}
	s->pid = pid;
	s->started_ms = now;
	s->state = S_RUNNING;
}

static struct svc *by_pid(pid_t p) {
	for (int i = 0; i < nsvc; i++) if (svcs[i].pid == p) return &svcs[i];
	return NULL;
}
static struct svc *svc_by_name(const char *name) {
	for (int i = 0; i < nsvc; i++) if (!strcmp(svcs[i].name, name)) return &svcs[i];
	return NULL;
}

/* A dep is "up" when a daemon is UP or a oneshot is DONE. An unknown dep name counts as up (it's
 * validated + warned once at startup) so a typo can't wedge the boot. */
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

/* React to a child exit: a oneshot settles DONE (exit 0) or FAILED; a daemon respawns after a
 * backoff unless it has crash-looped (more than MAX_FAILS deaths within FAIL_WINDOW_MS), in which
 * case it's FAILED and left alone so a broken binary can't pin the CPU. */
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

/* Milliseconds until the next scheduled action (a backoff expiry, or a ready-path re-check/timeout),
 * or -1 if nothing is pending — then epoll blocks purely on child exits, zero idle wakeups. */
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
	if (best < 0) return -1;
	if (best > 60000) best = 60000;   /* cap; a stray wake once a minute is harmless */
	return (int)best;
}

/* Orderly shutdown — never returns. SIGTERM every running service, reap for up to STOP_GRACE_MS,
 * SIGKILL survivors, then sync + reboot(). On the custom kernel (no reboot syscall) reboot() fails
 * and we _exit(0), which the kernel turns into its halt — so shutdown works everywhere; only the
 * final instruction differs. */
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
		struct timespec ts = { 0, 50 * 1000000L }; nanosleep(&ts, NULL);   /* 50ms poll */
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

/* Handle one initctl connection: read a "verb [arg]" line, act, reply, close. */
static void handle_control(int cfd) {
	char buf[256];
	ssize_t n = read(cfd, buf, sizeof buf - 1);
	if (n <= 0) { close(cfd); return; }
	buf[n] = 0;
	buf[strcspn(buf, "\r\n")] = 0;                          /* trim newline */
	char *verb = buf, *arg = strchr(buf, ' ');
	if (arg) { *arg++ = 0; while (*arg == ' ') arg++; }
	struct svc *s = (arg && *arg) ? svc_by_name(arg) : NULL;
	#define REPLY(str) (void)write(cfd, (str), strlen(str))

	if (!strcmp(verb, "poweroff")) { REPLY("ok: powering off\n"); close(cfd); shutdown_system(RB_POWER_OFF,   "poweroff"); }
	if (!strcmp(verb, "reboot"))   { REPLY("ok: rebooting\n");    close(cfd); shutdown_system(RB_AUTOBOOT,    "reboot");   }
	if (!strcmp(verb, "halt"))     { REPLY("ok: halting\n");      close(cfd); shutdown_system(RB_HALT_SYSTEM, "halt");     }

	if (!strcmp(verb, "status")) {
		char line[160];
		for (int i = 0; i < nsvc; i++) {
			if (arg && *arg && strcmp(arg, svcs[i].name)) continue;
			snprintf(line, sizeof line, "%-16.40s %-7.7s pid=%d\n", svcs[i].name, state_str(svcs[i].state), (int)svcs[i].pid);
			REPLY(line);
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
		load_configs(g_confdir); REPLY("ok: reloaded\n");
	} else {
		REPLY("unknown verb (poweroff|reboot|halt|status|start|stop|restart|reload)\n");
	}
	#undef REPLY
	close(cfd);
}

/* The event-driven supervision loop — never returns. Blocks in epoll on a signalfd (a child exited)
 * and a timerfd (the next backoff / ready-gate deadline); with nothing pending it sleeps with zero
 * wakeups until a child dies. */
static void supervise(void) {
	sigset_t mask; sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);                           /* a child exited -> reap */
	sigaddset(&mask, SIGTERM);                           /* -> poweroff */
	sigaddset(&mask, SIGINT);                            /* ctrl-alt-del -> reboot */
	sigprocmask(SIG_BLOCK, &mask, NULL);                 /* deliver them via signalfd, not handlers */
	int sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	int efd = epoll_create1(EPOLL_CLOEXEC);
	int lfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);   /* initctl control socket */
	if (sfd < 0 || tfd < 0 || efd < 0 || lfd < 0)
		die("signalfd/timerfd/epoll/socket unavailable — this init requires a modern Linux kernel");

	/* bind + listen the control socket (real boot: /run is a tmpfs from early_setup, so this works). */
	struct sockaddr_un sa; memset(&sa, 0, sizeof sa); sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, INITCTL_SOCK, sizeof sa.sun_path - 1);
	unlink(INITCTL_SOCK);
	if (bind(lfd, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(lfd, 8) < 0)
		die("initctl socket %s: %s", INITCTL_SOCK, strerror(errno));

	struct epoll_event ev = { .events = EPOLLIN };
	ev.data.fd = sfd; epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &ev);
	ev.data.fd = tfd; epoll_ctl(efd, EPOLL_CTL_ADD, tfd, &ev);
	ev.data.fd = lfd; epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &ev);   /* lfd is always valid — we die above otherwise */

	for (;;) {
		long long now = now_ms();
		while (reconcile(now)) now = now_ms();           /* settle cascading starts/promotions */

		int d = next_delay_ms(now);
		struct itimerspec its; memset(&its, 0, sizeof its);
		if (d >= 0) {
			its.it_value.tv_sec  = d / 1000;
			its.it_value.tv_nsec = (d % 1000) * 1000000L;
			if (d == 0) its.it_value.tv_nsec = 1;        /* an all-zero itimerspec DISARMs, not fires */
		}
		timerfd_settime(tfd, 0, &its, NULL);             /* d < 0 => disarmed => block on child exits only */

		struct epoll_event evs[4];
		int n = epoll_wait(efd, evs, 4, -1);
		if (n < 0) { if (errno == EINTR) continue; die("epoll_wait: %s", strerror(errno)); }
		for (int i = 0; i < n; i++) {
			if (evs[i].data.fd == sfd) {
				struct signalfd_siginfo si; int got_chld = 0;
				while (read(sfd, &si, sizeof si) > 0) {
					if      (si.ssi_signo == SIGCHLD) got_chld = 1;
					else if (si.ssi_signo == SIGTERM) shutdown_system(RB_POWER_OFF, "SIGTERM");   /* never returns */
					else if (si.ssi_signo == SIGINT)  shutdown_system(RB_AUTOBOOT, "SIGINT/ctrl-alt-del");
				}
				if (got_chld) {
					long long tnow = now_ms(); int st; pid_t pid;
					while ((pid = waitpid(-1, &st, WNOHANG)) > 0) {
						struct svc *s = by_pid(pid);
						if (s) on_child_exit(s, st, tnow);   /* s == NULL => orphan reaped */
					}
				}
			} else if (evs[i].data.fd == tfd) {
				uint64_t x; while (read(tfd, &x, sizeof x) > 0) { }  /* drain expirations */
			} else if (evs[i].data.fd == lfd) {
				int cfd; while ((cfd = accept(lfd, NULL, NULL)) >= 0) handle_control(cfd);
			}
		}
	}
}

int main(void) {
	logmsg("booting");
	if (getpid() != 1) logmsg("warning: not PID 1 — running degraded (test mode)");

	early_setup();

	const char *confdir = getenv("INIT_CONFDIR");
	if (confdir && *confdir) g_confdir = confdir;
	load_configs(g_confdir);
	logmsg("loaded %d service(s) from %s", nsvc, g_confdir);

	/* Validate `after` deps once — unknown names are tolerated at runtime (treated as up), but a
	 * typo should be visible rather than silently wedging a dependent. */
	for (int i = 0; i < nsvc; i++)
		for (int j = 0; j < svcs[i].after_n; j++)
			if (!svc_by_name(svcs[i].after_names[j]))
				logmsg("%s: after '%s' — no such service (ignored)", svcs[i].name, svcs[i].after_names[j]);

	supervise();
	return 0;  /* not reached */
}
