#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#define KO_MOSH_VERSION "0.1.0"

extern char **environ;

struct strvec {
    char **items;
    size_t len;
    size_t cap;
};

struct options {
    const char *client;
    const char *server;
    const char *ssh;
    const char *port_request;
    const char *ssh_port;
    const char *identity;
    const char *predict;
    const char *family;
    bool no_init;
    bool verbose;
    const char *userhost;
    int command_index;
};

struct startup_result {
    char *ip;
    char *port;
    char *key;
    int ssh_status;
};

static volatile sig_atomic_t ssh_child = -1;
static volatile sig_atomic_t interrupted_signal = 0;

static void die_usage(const char *argv0);

static void *xcalloc(size_t count, size_t size)
{
    void *ptr = calloc(count, size);
    if (!ptr) {
        perror("calloc");
        exit(125);
    }
    return ptr;
}

static char *xstrdup(const char *s)
{
    char *copy = strdup(s ? s : "");
    if (!copy) {
        perror("strdup");
        exit(125);
    }
    return copy;
}

static char *xasprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        perror("vsnprintf");
        exit(125);
    }
    char *buf = xcalloc((size_t)needed + 1, 1);
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);
    return buf;
}

static void strvec_push(struct strvec *vec, char *value)
{
    if (vec->len + 1 >= vec->cap) {
        size_t next = vec->cap ? vec->cap * 2 : 8;
        char **items = realloc(vec->items, next * sizeof(*items));
        if (!items) {
            perror("realloc");
            exit(125);
        }
        vec->items = items;
        vec->cap = next;
    }
    vec->items[vec->len++] = value;
    vec->items[vec->len] = NULL;
}

static void strvec_free(struct strvec *vec)
{
    if (!vec) {
        return;
    }
    for (size_t i = 0; i < vec->len; i++) {
        free(vec->items[i]);
    }
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static bool executable(const char *path)
{
    return path && access(path, X_OK) == 0;
}

static bool has_slash(const char *s)
{
    return s && strchr(s, '/') != NULL;
}

static char *path_dirname(const char *path)
{
    char *copy = xstrdup(path);
    char *slash = strrchr(copy, '/');
    if (!slash) {
        free(copy);
        return xstrdup(".");
    }
    if (slash == copy) {
        slash[1] = '\0';
        return copy;
    }
    *slash = '\0';
    return copy;
}

static char *path_join(const char *a, const char *b)
{
    if (!a || !*a) {
        return xstrdup(b);
    }
    size_t len = strlen(a);
    return xasprintf("%s%s%s", a, (len > 0 && a[len - 1] == '/') ? "" : "/", b);
}

static char *parent_dir(const char *path)
{
    char *dir = xstrdup(path);
    size_t len = strlen(dir);
    while (len > 1 && dir[len - 1] == '/') {
        dir[--len] = '\0';
    }
    char *slash = strrchr(dir, '/');
    if (!slash) {
        free(dir);
        return xstrdup(".");
    }
    if (slash == dir) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
    return dir;
}

static char *find_on_path(const char *name)
{
    const char *path = getenv("PATH");
    if (!path || has_slash(name)) {
        return executable(name) ? xstrdup(name) : NULL;
    }
    char *copy = xstrdup(path);
    char *save = NULL;
    for (char *entry = strtok_r(copy, ":", &save); entry; entry = strtok_r(NULL, ":", &save)) {
        char *candidate = path_join(*entry ? entry : ".", name);
        if (executable(candidate)) {
            free(copy);
            return candidate;
        }
        free(candidate);
    }
    free(copy);
    return NULL;
}

static char *self_path(const char *argv0)
{
#if defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return xstrdup(buf);
    }
#endif
    if (has_slash(argv0)) {
        char resolved[PATH_MAX];
        if (realpath(argv0, resolved)) {
            return xstrdup(resolved);
        }
        return xstrdup(argv0);
    }
    char *found = find_on_path(argv0);
    return found ? found : xstrdup(argv0);
}

static char *locate_client(const char *argv0, const char *explicit_client)
{
    if (explicit_client) {
        return xstrdup(explicit_client);
    }

    char *self = self_path(argv0);
    char *dir = path_dirname(self);
    char *candidate = path_join(dir, "mosh-client");
    free(self);
    free(dir);
    if (executable(candidate)) {
        return candidate;
    }
    free(candidate);

    candidate = find_on_path("mosh-client");
    if (candidate) {
        return candidate;
    }

    return xstrdup("mosh-client");
}

static char *locate_dbclient(const char *argv0, const char *explicit_ssh)
{
    const char *env_ssh = getenv("MOSH_SSH");
    const char *chosen = explicit_ssh ? explicit_ssh : env_ssh;
    if (chosen && *chosen) {
        if (strpbrk(chosen, " \t\r\n")) {
            fprintf(stderr, "MOSH_SSH/--ssh must be a dbclient path, not a shell command with arguments.\n");
            exit(2);
        }
        return xstrdup(chosen);
    }

    char *self = self_path(argv0);
    char *bin_dir = path_dirname(self);
    char *plugin_dir = parent_dir(bin_dir);
    char *plugins_dir = parent_dir(plugin_dir);
    char *koreader_dir = parent_dir(plugins_dir);
    char *candidate = path_join(koreader_dir, "dbclient");

    free(self);
    free(bin_dir);
    free(plugin_dir);
    free(plugins_dir);
    free(koreader_dir);

    if (executable(candidate)) {
        return candidate;
    }
    free(candidate);

    candidate = find_on_path("dbclient");
    if (candidate) {
        return candidate;
    }

    return xstrdup("dbclient");
}

static bool parse_u16(const char *s, int min, int *value)
{
    if (!s || !*s) {
        return false;
    }
    long n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (!isdigit(*p)) {
            return false;
        }
        n = n * 10 + (*p - '0');
        if (n > 65535) {
            return false;
        }
    }
    if (n < min) {
        return false;
    }
    *value = (int)n;
    return true;
}

static bool valid_port_range(const char *s)
{
    char *copy = xstrdup(s);
    char *colon = strchr(copy, ':');
    int low = 0;
    int high = 0;
    bool ok = false;

    if (colon) {
        *colon = '\0';
        ok = parse_u16(copy, 1, &low) && parse_u16(colon + 1, 1, &high) && low <= high;
    } else {
        ok = parse_u16(copy, 0, &low);
    }
    free(copy);
    return ok;
}

static bool valid_predict(const char *s)
{
    return strcmp(s, "adaptive") == 0 ||
           strcmp(s, "always") == 0 ||
           strcmp(s, "never") == 0 ||
           strcmp(s, "experimental") == 0;
}

static bool valid_userhost(const char *s)
{
    if (!s || !*s) {
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (iscntrl(*p) || isspace(*p)) {
            return false;
        }
        if (strchr(";&|<>()$`'\"\\", *p)) {
            return false;
        }
    }
    return true;
}

static char *shell_quote(const char *s)
{
    size_t extra = 2;
    for (const char *p = s; *p; p++) {
        extra += (*p == '\'') ? 4 : 1;
    }
    char *out = xcalloc(extra + 1, 1);
    char *q = out;
    *q++ = '\'';
    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            memcpy(q, "'\\''", 4);
            q += 4;
        } else {
            *q++ = *p;
        }
    }
    *q++ = '\'';
    *q = '\0';
    return out;
}

static bool env_is_utf8(const char *name)
{
    const char *value = getenv(name);
    if (!value) {
        return false;
    }
    for (const char *p = value; *p; p++) {
        if ((p[0] == 'U' || p[0] == 'u') &&
            (p[1] == 'T' || p[1] == 't') &&
            (p[2] == 'F' || p[2] == 'f') &&
            (p[3] == '-' || p[3] == '8') &&
            ((p[3] == '-' && p[4] == '8') || p[3] == '8')) {
            return true;
        }
    }
    return false;
}

static void setup_local_environment(const char *argv0, const char *client)
{
    setenv("TERM", "vt52", 1);
    if (!env_is_utf8("LC_ALL") && !env_is_utf8("LC_CTYPE") && !env_is_utf8("LANG")) {
        setenv("LANG", "C.UTF-8", 1);
        setenv("LC_CTYPE", "C.UTF-8", 1);
    }

    char *self = (client && has_slash(client)) ? xstrdup(client) : self_path(argv0);
    char *bin_dir = path_dirname(self);
    char *plugin_dir = parent_dir(bin_dir);
    char *terminfo = path_join(plugin_dir, "share/terminfo");
    struct stat st;
    if (stat(terminfo, &st) == 0 && S_ISDIR(st.st_mode)) {
        setenv("TERMINFO", terminfo, 1);
        setenv("TERMINFO_DIRS", terminfo, 1);
    }
    free(self);
    free(bin_dir);
    free(plugin_dir);
    free(terminfo);
}

static void parse_args(int argc, char **argv, struct options *opts)
{
    opts->server = "mosh-server";
    opts->predict = "adaptive";
    opts->family = "auto";
    opts->command_index = argc;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = NULL;

        if (strcmp(arg, "--") == 0) {
            i++;
            if (i < argc && !opts->userhost) {
                opts->userhost = argv[i++];
            }
            opts->command_index = i;
            break;
        }
        if (opts->userhost) {
            opts->command_index = i;
            break;
        }
        if (strcmp(arg, "--help") == 0) {
            die_usage(argv[0]);
        } else if (strcmp(arg, "--version") == 0) {
            printf("koreader-mosh %s\n", KO_MOSH_VERSION);
            exit(0);
        } else if (strcmp(arg, "--verbose") == 0) {
            opts->verbose = true;
        } else if (strcmp(arg, "--no-init") == 0) {
            opts->no_init = true;
        } else if (strcmp(arg, "-4") == 0) {
            opts->family = "inet";
        } else if (strcmp(arg, "-6") == 0) {
            opts->family = "inet6";
        } else if (strncmp(arg, "--client=", 9) == 0) {
            opts->client = arg + 9;
        } else if (strcmp(arg, "--client") == 0 && i + 1 < argc) {
            opts->client = argv[++i];
        } else if (strncmp(arg, "--server=", 9) == 0) {
            opts->server = arg + 9;
        } else if (strcmp(arg, "--server") == 0 && i + 1 < argc) {
            opts->server = argv[++i];
        } else if (strncmp(arg, "--ssh=", 6) == 0) {
            opts->ssh = arg + 6;
        } else if (strcmp(arg, "--ssh") == 0 && i + 1 < argc) {
            opts->ssh = argv[++i];
        } else if (strncmp(arg, "--port=", 7) == 0) {
            opts->port_request = arg + 7;
        } else if (strcmp(arg, "--port") == 0 && i + 1 < argc) {
            opts->port_request = argv[++i];
        } else if (strncmp(arg, "-p", 2) == 0) {
            value = arg[2] ? arg + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (!value) {
                die_usage(argv[0]);
            }
            opts->port_request = value;
        } else if (strncmp(arg, "--ssh-port=", 11) == 0) {
            opts->ssh_port = arg + 11;
        } else if (strcmp(arg, "--ssh-port") == 0 && i + 1 < argc) {
            opts->ssh_port = argv[++i];
        } else if (strncmp(arg, "--identity=", 11) == 0) {
            opts->identity = arg + 11;
        } else if (strcmp(arg, "--identity") == 0 && i + 1 < argc) {
            opts->identity = argv[++i];
        } else if (strncmp(arg, "-i", 2) == 0) {
            value = arg[2] ? arg + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (!value) {
                die_usage(argv[0]);
            }
            opts->identity = value;
        } else if (strncmp(arg, "--predict=", 10) == 0) {
            opts->predict = arg + 10;
        } else if (strcmp(arg, "--predict") == 0 && i + 1 < argc) {
            opts->predict = argv[++i];
        } else if (arg[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            die_usage(argv[0]);
        } else {
            opts->userhost = arg;
            opts->command_index = i + 1;
        }
    }

    if (!opts->userhost) {
        die_usage(argv[0]);
    }
    if (!valid_userhost(opts->userhost)) {
        fprintf(stderr, "Invalid [user@]host: %s\n", opts->userhost);
        exit(2);
    }
    if (opts->port_request && !valid_port_range(opts->port_request)) {
        fprintf(stderr, "Invalid UDP port or range: %s\n", opts->port_request);
        exit(2);
    }
    if (opts->ssh_port) {
        int dummy = 0;
        if (!parse_u16(opts->ssh_port, 1, &dummy)) {
            fprintf(stderr, "Invalid SSH port: %s\n", opts->ssh_port);
            exit(2);
        }
    }
    if (!valid_predict(opts->predict)) {
        fprintf(stderr, "Invalid prediction mode: %s\n", opts->predict);
        exit(2);
    }
}

static void die_usage(const char *argv0)
{
    printf("Usage: %s [options] [--] [user@]host [command...]\n"
           "  -p, --port PORT[:PORT2]       server-side UDP port or range\n"
           "      --ssh-port PORT           dbclient TCP port\n"
           "  -i, --identity PATH           dbclient identity file\n"
           "      --server PATH             remote mosh-server path\n"
           "      --client PATH             local mosh-client path\n"
           "      --ssh PATH                dbclient path (or MOSH_SSH)\n"
           "  -4 | -6                       constrain SSH and DNS fallback family\n"
           "      --predict MODE            adaptive, always, never, experimental\n"
           "      --no-init                 set MOSH_NO_TERM_INIT=1\n"
           "      --verbose                 print startup diagnostics without keys\n"
           "      --version                 print version\n",
           argv0);
    exit(0);
}

static int run_color_count(const char *client)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        perror("pipe");
        exit(125);
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(125);
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execl(client, client, "-c", (char *)NULL);
        execlp(client, client, "-c", (char *)NULL);
        perror("mosh-client -c");
        _exit(127);
    }
    close(pipefd[1]);
    char buf[64] = {0};
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Cannot determine terminal color count with %s -c.\n", client);
        exit(1);
    }
    char *end = NULL;
    long colors = strtol(buf, &end, 10);
    if (end == buf || colors < 0 || colors > 16777216) {
        colors = 0;
    }
    return (int)colors;
}

static void append_script_word(char **script, const char *word)
{
    char *quoted = shell_quote(word);
    char *next = xasprintf("%s %s", *script, quoted);
    free(*script);
    free(quoted);
    *script = next;
}

static void append_locale_args(char **script)
{
    const char *names[] = {
        "LANG", "LANGUAGE", "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE",
        "LC_MONETARY", "LC_MESSAGES", "LC_PAPER", "LC_NAME", "LC_ADDRESS",
        "LC_TELEPHONE", "LC_MEASUREMENT", "LC_IDENTIFICATION", "LC_ALL", NULL
    };
    bool emitted_ctype = false;
    for (int i = 0; names[i]; i++) {
        const char *value = getenv(names[i]);
        if (!value || !*value) {
            continue;
        }
        if (strcmp(names[i], "LC_CTYPE") == 0 || strcmp(names[i], "LANG") == 0 || strcmp(names[i], "LC_ALL") == 0) {
            emitted_ctype = true;
        }
        char *assignment = xasprintf("%s=%s", names[i], value);
        append_script_word(script, "-l");
        append_script_word(script, assignment);
        free(assignment);
    }
    if (!emitted_ctype) {
        append_script_word(script, "-l");
        append_script_word(script, "LC_CTYPE=C.UTF-8");
    }
}

static char *build_remote_command(const struct options *opts, int colors, int argc, char **argv)
{
    char *script = xasprintf(
        "if [ -n \"${SSH_CONNECTION:-}\" ]; then printf '\\nMOSH SSH_CONNECTION %%s\\n' \"$SSH_CONNECTION\"; fi; exec %s new -c %d",
        shell_quote(opts->server), colors);

    if (opts->port_request) {
        append_script_word(&script, "-p");
        append_script_word(&script, opts->port_request);
    }
    append_locale_args(&script);
    if (opts->command_index < argc) {
        append_script_word(&script, "--");
        for (int i = opts->command_index; i < argc; i++) {
            append_script_word(&script, argv[i]);
        }
    }

    char *quoted_script = shell_quote(script);
    char *command = xasprintf("sh -c %s", quoted_script);
    free(script);
    free(quoted_script);
    return command;
}

static void signal_handler(int sig)
{
    interrupted_signal = sig;
    if (ssh_child > 0) {
        if (kill(-ssh_child, sig) != 0) {
            kill(ssh_child, sig);
        }
    }
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

static bool parse_key(const char *key)
{
    if (strlen(key) != 22) {
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        if (!(isalnum(*p) || *p == '/' || *p == '+')) {
            return false;
        }
    }
    return true;
}

static void parse_connect_line(const char *line, struct startup_result *result)
{
    char port[16] = {0};
    char key[64] = {0};
    char tail[4] = {0};
    if (sscanf(line, "MOSH CONNECT %15[0-9] %63[A-Za-z0-9/+]%3s", port, key, tail) != 2) {
        fprintf(stderr, "Bad MOSH CONNECT string.\n");
        exit(1);
    }
    int port_value = 0;
    if (!parse_u16(port, 1, &port_value) || !parse_key(key)) {
        fprintf(stderr, "Bad MOSH CONNECT string.\n");
        exit(1);
    }
    if (result->port || result->key) {
        fprintf(stderr, "Duplicate MOSH CONNECT string.\n");
        exit(1);
    }
    result->port = xstrdup(port);
    result->key = xstrdup(key);
}

static void parse_ssh_connection_line(const char *line, struct startup_result *result)
{
    const char *prefix = "MOSH SSH_CONNECTION ";
    const char *value = line + strlen(prefix);
    char *copy = xstrdup(value);
    char *save = NULL;
    char *fields[4] = {0};
    int count = 0;
    for (char *tok = strtok_r(copy, " \t", &save); tok && count < 4; tok = strtok_r(NULL, " \t", &save)) {
        fields[count++] = tok;
    }
    if (count != 4 || strtok_r(NULL, " \t", &save)) {
        free(copy);
        fprintf(stderr, "Bad MOSH SSH_CONNECTION string.\n");
        exit(1);
    }
    if (result->ip) {
        free(copy);
        fprintf(stderr, "Duplicate MOSH remote IP string.\n");
        exit(1);
    }
    result->ip = xstrdup(fields[2]);
    free(copy);
}

static int status_to_exit(int status)
{
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

static struct startup_result run_ssh(const char *dbclient, const struct options *opts, const char *remote_command)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        perror("pipe");
        exit(125);
    }

    struct strvec args = {0};
    strvec_push(&args, xstrdup(dbclient));
    if (strcmp(opts->family, "inet") == 0) {
        strvec_push(&args, xstrdup("-4"));
    } else if (strcmp(opts->family, "inet6") == 0) {
        strvec_push(&args, xstrdup("-6"));
    }
    if (opts->ssh_port) {
        strvec_push(&args, xstrdup("-p"));
        strvec_push(&args, xstrdup(opts->ssh_port));
    }
    if (opts->identity) {
        strvec_push(&args, xstrdup("-i"));
        strvec_push(&args, xstrdup(opts->identity));
    }
    strvec_push(&args, xstrdup(opts->userhost));
    strvec_push(&args, xstrdup(remote_command));

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(125);
    }
    if (pid == 0) {
        setpgid(0, 0);
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execv(dbclient, args.items);
        execvp(dbclient, args.items);
        perror("dbclient");
        _exit(127);
    }

    ssh_child = pid;
    setpgid(pid, pid);
    close(pipefd[1]);

    FILE *stream = fdopen(pipefd[0], "r");
    if (!stream) {
        perror("fdopen");
        exit(125);
    }

    struct startup_result result = {0};
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, stream) != -1) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "MOSH CONNECT ", 13) == 0) {
            parse_connect_line(line, &result);
        } else if (strncmp(line, "MOSH SSH_CONNECTION ", 20) == 0) {
            parse_ssh_connection_line(line, &result);
        } else if (strncmp(line, "MOSH ", 5) == 0) {
            fprintf(stderr, "Unexpected MOSH control message.\n");
            exit(1);
        } else {
            printf("%s\n", line);
            fflush(stdout);
        }
    }
    free(line);
    fclose(stream);

    int status = 0;
    waitpid(pid, &status, 0);
    ssh_child = -1;
    result.ssh_status = status_to_exit(status);
    strvec_free(&args);
    return result;
}

static char *extract_host(const char *userhost)
{
    const char *host = strrchr(userhost, '@');
    host = host ? host + 1 : userhost;
    if (*host == '[') {
        const char *end = strchr(host, ']');
        if (!end || end == host + 1) {
            return NULL;
        }
        return strndup(host + 1, (size_t)(end - host - 1));
    }
    if (!*host) {
        return NULL;
    }
    return xstrdup(host);
}

static char *resolve_fallback_ip(const char *userhost, const char *family)
{
    const char *disable_dns = getenv("KO_MOSH_DISABLE_DNS");
    if (disable_dns && *disable_dns) {
        return NULL;
    }
    char *host = extract_host(userhost);
    if (!host) {
        return NULL;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    if (strcmp(family, "inet") == 0) {
        hints.ai_family = AF_INET;
    } else if (strcmp(family, "inet6") == 0) {
        hints.ai_family = AF_INET6;
    } else {
        hints.ai_family = AF_UNSPEC;
    }

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, NULL, &hints, &res);
    free(host);
    if (rc != 0 || !res) {
        return NULL;
    }

    char ip[NI_MAXHOST];
    rc = getnameinfo(res->ai_addr, res->ai_addrlen, ip, sizeof(ip), NULL, 0, NI_NUMERICHOST);
    freeaddrinfo(res);
    if (rc != 0) {
        return NULL;
    }
    return xstrdup(ip);
}

static void exec_client(const char *client, const struct options *opts, struct startup_result *result)
{
    setenv("MOSH_KEY", result->key, 1);
    setenv("MOSH_PREDICTION_DISPLAY", opts->predict, 1);
    if (opts->no_init) {
        setenv("MOSH_NO_TERM_INIT", "1", 1);
    }

    char *original = xasprintf("koreader-mosh %s |", opts->userhost);
    char *argv_exec[] = {
        (char *)client,
        "-#",
        original,
        result->ip,
        result->port,
        NULL,
    };
    execv(client, argv_exec);
    execvp(client, argv_exec);
    unsetenv("MOSH_KEY");
    perror("mosh-client");
    free(original);
    exit(127);
}

int main(int argc, char **argv)
{
    struct options opts = {0};
    parse_args(argc, argv, &opts);

    char *client = locate_client(argv[0], opts.client);
    setup_local_environment(argv[0], client);
    install_signal_handlers();

    char *dbclient = locate_dbclient(argv[0], opts.ssh);
    int colors = run_color_count(client);
    char *remote_command = build_remote_command(&opts, colors, argc, argv);

    if (opts.verbose) {
        fprintf(stderr, "koreader-mosh: client=%s\n", client);
        fprintf(stderr, "koreader-mosh: ssh=%s\n", dbclient);
        fprintf(stderr, "koreader-mosh: remote=%s\n", opts.userhost);
    }

    struct startup_result result = run_ssh(dbclient, &opts, remote_command);
    free(remote_command);
    free(dbclient);

    if (interrupted_signal) {
        return 128 + interrupted_signal;
    }
    if (!result.key || !result.port) {
        if (result.ssh_status != 0) {
            fprintf(stderr, "SSH startup failed with exit status %d.\n", result.ssh_status);
            return result.ssh_status;
        }
        fprintf(stderr, "Did not find mosh server startup message. Is mosh-server installed on the remote host?\n");
        return 1;
    }
    if (!result.ip) {
        result.ip = resolve_fallback_ip(opts.userhost, opts.family);
    }
    if (!result.ip) {
        fprintf(stderr, "Did not find remote IP address from SSH_CONNECTION or DNS fallback.\n");
        return 1;
    }

    exec_client(client, &opts, &result);
    return 127;
}
