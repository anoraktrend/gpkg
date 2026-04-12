#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* 
 * GPKG - Minimalist Package Manager (C23/C2y) 
 * 
 * EE Ethos:
 * 1. Simple to use (no instruction required).
 * 2. Easy to compile & port (minimal dependencies).
 * 3. Minimal number of files.
 * 4. Useful functionality.
 *
 * NO GNU DEPENDENCIES.
 */

#ifndef PKG_DB_PATH
#define PKG_DB_PATH "db/installed"
#endif

#ifndef PKG_SCRIPTS_PATH
#define PKG_SCRIPTS_PATH "scripts"
#endif

typedef enum { 
    CMD_BUILD,
    CMD_INSTALL,
    CMD_UPGRADE,
    CMD_REMOVE,
    CMD_LIST,
    CMD_UNKNOWN
} Command;

Command parse_command(const char *cmd_str) {
    if (cmd_str == nullptr) return CMD_UNKNOWN;
    
    /* Build shortcuts: build, mk */
    if (strcmp(cmd_str, "build") == 0 || strcmp(cmd_str, "mk") == 0) return CMD_BUILD;
    
    /* Install shortcuts: install, it, add */
    if (strcmp(cmd_str, "install") == 0 || strcmp(cmd_str, "it") == 0 || strcmp(cmd_str, "add") == 0) return CMD_INSTALL;
    
    /* Upgrade shortcuts: upgrade, up */
    if (strcmp(cmd_str, "upgrade") == 0 || strcmp(cmd_str, "up") == 0) return CMD_UPGRADE;
    
    /* Remove shortcuts: remove, rm, del */
    if (strcmp(cmd_str, "remove") == 0 || strcmp(cmd_str, "rm") == 0 || strcmp(cmd_str, "del") == 0) return CMD_REMOVE;
    
    /* List shortcuts: list, ls */
    if (strcmp(cmd_str, "list") == 0 || strcmp(cmd_str, "ls") == 0) return CMD_LIST;
    
    return CMD_UNKNOWN;
}

void print_usage(const char *progname) {
    printf("Usage: %s <command> [args...]\n", progname);
    printf("Commands:\n");
    printf("  build, mk    <pkgdir>   - Build a package\n");
    printf("  install, it, add <pkg>  - Install a package\n");
    printf("  upgrade, up  <pkg>      - Upgrade a package\n");
    printf("  remove, rm, del <pkg>   - Remove a package\n");
    printf("  list, ls                - List installed packages\n");
}

int run_script(const char *script_name, int argc, char *argv[]) {
    char script_path[1024];
    snprintf(script_path, sizeof(script_path), "%s/%s", PKG_SCRIPTS_PATH, script_name);

    struct stat st;
    if (stat(script_path, &st) != 0) {
        perror("stat");
        fprintf(stderr, "Error: Script '%s' not found or inaccessible.\n", script_path);
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        char **child_argv = malloc((argc + 3) * sizeof(char *));
        if (child_argv == nullptr) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        child_argv[0] = strdup("/bin/sh");
        child_argv[1] = strdup(script_path);

        for (int i = 0; i < argc; i++) {
            child_argv[i + 2] = strdup(argv[i]);
        }
        child_argv[argc + 2] = nullptr;

        execv("/bin/sh", child_argv);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        /* Parent process */
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return -1;
        }
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    }
}

void list_packages(void) {
    DIR *d = opendir(PKG_DB_PATH);
    if (d == nullptr) {
        printf("No packages installed (database not found at %s).\n", PKG_DB_PATH);
        return;
    }

    struct dirent *dir;
    bool found = false;
    while ((dir = readdir(d)) != nullptr) {
        if (dir->d_name[0] == '.') continue;
        printf("- %s\n", dir->d_name);
        found = true;
    }

    if (!found) {
        printf("No packages installed.\n");
    }

    closedir(d);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    Command cmd = parse_command(argv[1]);

    switch (cmd) {
        case CMD_BUILD:
            if (argc < 3) {
                fprintf(stderr, "Error: 'build' requires a package directory.\n");
                return EXIT_FAILURE;
            }
            return run_script("build_pkg.sh", 1, &argv[2]);

        case CMD_INSTALL:
            if (argc < 3) {
                fprintf(stderr, "Error: 'install' requires a package file.\n");
                return EXIT_FAILURE;
            }
            return run_script("install_pkg.sh", 1, &argv[2]);

        case CMD_UPGRADE:
            if (argc < 3) {
                fprintf(stderr, "Error: 'upgrade' requires a package file.\n");
                return EXIT_FAILURE;
            }
            return run_script("upgrade_pkg.sh", 1, &argv[2]);

        case CMD_REMOVE:
            if (argc < 3) {
                fprintf(stderr, "Error: 'remove' requires a package name.\n");
                return EXIT_FAILURE;
            }
            return run_script("remove_pkg.sh", 1, &argv[2]);

        case CMD_LIST:
            list_packages();
            break;

        case CMD_UNKNOWN:
        default:
            fprintf(stderr, "Error: Unknown command '%s'.\n", argv[1]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
