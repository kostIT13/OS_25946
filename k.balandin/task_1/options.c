#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/types.h>

extern char **environ;

typedef enum {
    ACT_I,   
    ACT_S,   
    ACT_P,   
    ACT_U, 
    ACT_U_SET, 
    ACT_C,  
    ACT_C_SET, 
    ACT_D,  
    ACT_V,   
    ACT_V_SET  
} action_type;

typedef struct {
    action_type type;
    char *arg;   
} action_t;

static action_t *actions = NULL;
static size_t actions_count = 0;
static size_t actions_cap = 0;

static void push_action(action_type t, const char *arg)
{
    if (actions_count == actions_cap) {
        actions_cap = actions_cap ? actions_cap * 2 : 16;
        actions = realloc(actions, actions_cap * sizeof(*actions));
        if (!actions) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
    actions[actions_count].type = t;
    actions[actions_count].arg = arg ? strdup(arg) : NULL;
    actions_count++;
}


static void do_i(void)
{
    printf("-i: real UID=%ld, effective UID=%ld, real GID=%ld, effective GID=%ld\n",
           (long)getuid(), (long)geteuid(),
           (long)getgid(), (long)getegid());
}

static void do_s(void)
{
    if (setpgid(0, 0) == -1) {
        perror("-s: setpgid");
    } else {
        printf("-s: process became group leader, PGID=%ld\n", (long)getpgrp());
    }
}

static void do_p(void)
{
    printf("-p: PID=%ld, PPID=%ld, PGID=%ld\n",
           (long)getpid(), (long)getppid(), (long)getpgrp());
}

static void do_u(void)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("-u: getrlimit");
        return;
    }
    printf("-u: ulimit (RLIMIT_NOFILE) soft=%llu hard=%llu\n",
           (unsigned long long)rl.rlim_cur,
           (unsigned long long)rl.rlim_max);
}

static void do_U(const char *arg)
{
    char *end = NULL;
    errno = 0;
    long val = strtol(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || val < 0) {
        fprintf(stderr, "-U: invalid value '%s'\n", arg);
        return;
    }
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("-U: getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)val;
    if (rl.rlim_max != RLIM_INFINITY && rl.rlim_cur > rl.rlim_max)
        rl.rlim_max = rl.rlim_cur;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("-U: setrlimit");
        return;
    }
    printf("-U: ulimit set to %ld\n", val);
}

static void do_c(void)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("-c: getrlimit");
        return;
    }
    printf("-c: core file size soft=%llu hard=%llu bytes\n",
           (unsigned long long)rl.rlim_cur,
           (unsigned long long)rl.rlim_max);
}

static void do_C(const char *arg)
{
    char *end = NULL;
    errno = 0;
    long val = strtol(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || val < 0) {
        fprintf(stderr, "-C: invalid value '%s'\n", arg);
        return;
    }
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("-C: getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)val;
    if (rl.rlim_max != RLIM_INFINITY && rl.rlim_cur > rl.rlim_max)
        rl.rlim_max = rl.rlim_cur;
    if (setrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("-C: setrlimit");
        return;
    }
    printf("-C: core file size set to %ld bytes\n", val);
}

static void do_d(void)
{
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("-d: getcwd");
        return;
    }
    printf("-d: cwd=%s\n", buf);
}

static void do_v(void)
{
    printf("-v: environment variables:\n");
    for (char **e = environ; *e; e++)
        printf("    %s\n", *e);
}

static void do_V(const char *arg)
{
    if (strchr(arg, '=') == NULL) {
        fprintf(stderr, "-V: argument must be NAME=value, got '%s'\n", arg);
        return;
    }
    if (putenv(strdup(arg)) != 0) {
        perror("-V: putenv");
        return;
    }
    printf("-V: set %s\n", arg);
}


static void run_actions(void)
{
    for (size_t i = actions_count; i-- > 0; ) {
        action_t *a = &actions[i];
        switch (a->type) {
        case ACT_I:     do_i();        break;
        case ACT_S:     do_s();        break;
        case ACT_P:     do_p();        break;
        case ACT_U:     do_u();        break;
        case ACT_U_SET: do_U(a->arg);  break;
        case ACT_C:     do_c();        break;
        case ACT_C_SET: do_C(a->arg);  break;
        case ACT_D:     do_d();        break;
        case ACT_V:     do_v();        break;
        case ACT_V_SET: do_V(a->arg);  break;
        }
    }
}


int main(int argc, char *argv[])
{
    const char *optstring = "ispuU:cC:dvV:";
    int c;

    while ((c = getopt(argc, argv, optstring)) != -1) {
        switch (c) {
        case 'i': push_action(ACT_I, NULL);      break;
        case 's': push_action(ACT_S, NULL);      break;
        case 'p': push_action(ACT_P, NULL);      break;
        case 'u': push_action(ACT_U, NULL);      break;
        case 'U': push_action(ACT_U_SET, optarg); break;
        case 'c': push_action(ACT_C, NULL);      break;
        case 'C': push_action(ACT_C_SET, optarg); break;
        case 'd': push_action(ACT_D, NULL);      break;
        case 'v': push_action(ACT_V, NULL);      break;
        case 'V': push_action(ACT_V_SET, optarg); break;
        case '?':
        default:
            if (optopt)
                fprintf(stderr, "Invalid option: -%c\n", optopt);
            else
                fprintf(stderr, "Invalid option: %s\n", argv[optind - 1]);
            break;
        }
    }

    run_actions();

    for (size_t i = 0; i < actions_count; i++)
        free(actions[i].arg);
    free(actions);

    return 0;
}
