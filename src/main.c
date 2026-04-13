#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdbool.h>

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
#define PKG_DB_PATH "db"
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

typedef struct {
    char *pkgname;
    char *pkgver;
    char *source;
} PackageInfo;

/* Safe string functions (EE Ethos: Portable & Simple) */
/**
 * gpkg_strscpy - Copy a C-string into a sized buffer
 * @dst: Where to copy the string to
 * @src: Where to copy the string from
 * @siz: Size of destination buffer
 *
 * Copy the source string to the destination buffer, ensuring it's NUL-terminated
 * and does not overflow.
 *
 * Returns the number of characters copied (excluding NUL) or -E2BIG if truncated.
 */
#ifndef E2BIG
#define E2BIG 7
#endif

static ssize_t gpkg_strscpy(char *dst, const char *src, size_t siz) {
    if (siz == 0) return -E2BIG;

    size_t i;
    for (i = 0; i < siz; i++) {
        if ((dst[i] = src[i]) == '\0') {
            return i;
        }
    }

    dst[siz - 1] = '\0';
    return -E2BIG;
}

/* Helper for path concatenation using strscpy-like logic */
static ssize_t gpkg_path_join(char *dst, const char *p1, const char *p2, size_t siz) {
    ssize_t res = gpkg_strscpy(dst, p1, siz);
    if (res < 0) return res;

    size_t len = (size_t)res;
    if (len >= siz - 1) return -E2BIG;

    /* Add separator if needed */
    if (len > 0 && dst[len - 1] != '/' && p2[0] != '/') {
        dst[len++] = '/';
        dst[len] = '\0';
    }

    /* Append second part */
    ssize_t res2 = gpkg_strscpy(dst + len, p2, siz - len);
    if (res2 < 0) return res2;

    return (ssize_t)(len + (size_t)res2);
}

static char *get_repo_name(const char *pkgdir) {
    if (!pkgdir) return nullptr;
    char *p = strdup(pkgdir);
    char *sep = strchr(p, '/');
    if (sep) {
        *sep = '\0';
        return p;
    }
    free(p);
    return strdup("local");
}

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
    printf("  install, it, add <pkg>  - Install a package (.gpkg)\n");
    printf("  upgrade, up  <pkg>      - Upgrade a package (.gpkg)\n");
    printf("  remove, rm, del <pkg>   - Remove a package\n");
    printf("  list, ls                - List installed packages\n");
}

PackageInfo *get_pkg_info(const char *pkgdir) {
    char pkgbuild_path[4096];
    if (gpkg_path_join(pkgbuild_path, pkgdir, "PKGBUILD", sizeof(pkgbuild_path)) < 0) {
        return nullptr;
    }

    struct stat st;
    if (stat(pkgbuild_path, &st) != 0) {
        return nullptr;
    }

    /* Source PKGBUILD and echo variables */
    char cmd[8192];
    if (snprintf(cmd, sizeof(cmd), ". %s && echo \"$pkgname\" && echo \"$pkgver\" && echo \"$source\"", pkgbuild_path) >= (int)sizeof(cmd)) {
        fprintf(stderr, "Error: PKGBUILD path too long.\n");
        return nullptr;
    }

    FILE *fp = popen(cmd, "r");
    if (fp == nullptr) return nullptr;

    PackageInfo *info = calloc(1, sizeof(PackageInfo));
    char line[4096];

    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        info->pkgname = strdup(line);
    }
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        info->pkgver = strdup(line);
    }
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        info->source = strdup(line);
    }

    pclose(fp);
    return info;
}

void free_pkg_info(PackageInfo *info) {
    if (info) {
        free(info->pkgname);
        free(info->pkgver);
        free(info->source);
        free(info);
    }
}

int run_command(const char *file, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) return -1;
    if (pid == 0) {
        execvp(file, argv);
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
}

int download_source(const char *url) {
    mkdir("src-cache", 0755);
    const char *filename = strrchr(url, '/');
    if (filename) filename++; else filename = url;

    char dest_path[4096];
    if (gpkg_path_join(dest_path, "src-cache", filename, sizeof(dest_path)) < 0) {
        fprintf(stderr, "Error: Destination path too long for %s\n", filename);
        return -1;
    }

    struct stat st;
    if (stat(dest_path, &st) == 0) {
        printf("Source %s already exists, skipping download.\n", filename);
        return 0;
    }

    printf("Downloading %s...\n", url);
    
    /* Try aria2c first */
    char *aria_argv[] = {(char*)"aria2c", (char*)"-d", (char*)"src-cache", (char*)"-o", (char*)filename, (char*)"-x16", (char*)url, nullptr};
    if (run_command("aria2c", aria_argv) == 0) return 0;

    /* Fallback to curl */
    printf("Warning: aria2c failed or not found, falling back to curl...\n");
    char *curl_argv[] = {(char*)"curl", (char*)"-L", (char*)"-o", (char*)dest_path, (char*)url, nullptr};
    return run_command("curl", curl_argv);
}

int extract_source(const char *pkgdir, const char *url) {
    const char *filename = strrchr(url, '/');
    if (filename) filename++; else filename = url;

    char src_path[4096];
    if (gpkg_path_join(src_path, "src-cache", filename, sizeof(src_path)) < 0) {
        fprintf(stderr, "Error: Source path too long for %s\n", filename);
        return -1;
    }

    /* Determine src_base for cleanup (mirroring shell logic) */
    char src_base[4096];
    if (gpkg_strscpy(src_base, filename, sizeof(src_base)) < 0) {
        fprintf(stderr, "Error: Filename too long: %s\n", filename);
        return -1;
    }
    char *ext = strstr(src_base, ".tar.");
    if (!ext) ext = strstr(src_base, ".tgz");
    if (!ext) ext = strstr(src_base, ".zip");
    if (ext) *ext = 0;

    char cleanup_path[8192]; // Large enough for pkgdir + / + src_base
    if (gpkg_path_join(cleanup_path, pkgdir, src_base, sizeof(cleanup_path)) < 0) {
        fprintf(stderr, "Error: Cleanup path too long for %s in %s\n", src_base, pkgdir);
        return -1;
    }
    
    /* Remove old extraction */
    char *rm_argv[] = {(char*)"rm", (char*)"-rf", cleanup_path, nullptr};
    run_command("rm", rm_argv);

    printf("Extracting %s into %s...\n", filename, pkgdir);

    if (strstr(filename, ".tar.gz") || strstr(filename, ".tgz")) {
        char *tar_argv[] = {(char*)"tar", (char*)"-xzf", src_path, (char*)"-C", (char*)pkgdir, nullptr};
        return run_command("tar", tar_argv);
    } else if (strstr(filename, ".tar.xz")) {
        char *tar_argv[] = {(char*)"tar", (char*)"-xJf", src_path, (char*)"-C", (char*)pkgdir, nullptr};
        return run_command("tar", tar_argv);
    } else if (strstr(filename, ".tar.bz2")) {
        char *tar_argv[] = {(char*)"tar", (char*)"-xjf", src_path, (char*)"-C", (char*)pkgdir, nullptr};
        return run_command("tar", tar_argv);
    } else if (strstr(filename, ".zip")) {
        char *unzip_argv[] = {(char*)"unzip", (char*)"-q", src_path, (char*)"-d", (char*)pkgdir, nullptr};
        return run_command("unzip", unzip_argv);
    }

    fprintf(stderr, "Error: Unknown file type for %s\n", filename);
    return -1;
}

int gpkg_install(const char *pkgfile) {
    struct stat st;
    if (stat(pkgfile, &st) != 0) {
        fprintf(stderr, "Error: Package file '%s' not found.\n", pkgfile);
        return -1;
    }

    /* Extract pkgname_ver from filename (remove .gpkg) */
    const char *base = strrchr(pkgfile, '/');
    if (base) base++; else base = pkgfile;
    
    char pkgname_ver[4096];
    gpkg_strscpy(pkgname_ver, base, sizeof(pkgname_ver));
    char *ext = strstr(pkgname_ver, ".gpkg");
    if (ext) *ext = '\0';

    printf("Installing %s...\n", pkgname_ver);

    const char *root = getenv("GPKG_ROOT");
    if (!root) root = "/";
    const char *repo = getenv("GPKG_REPO");
    if (!repo) repo = "local";

    /* Create database directory */
    char db_dir[4096];
    gpkg_path_join(db_dir, PKG_DB_PATH, repo, sizeof(db_dir));
    gpkg_path_join(db_dir, db_dir, "installed", sizeof(db_dir));
    gpkg_path_join(db_dir, db_dir, pkgname_ver, sizeof(db_dir));

    char mkdir_cmd[8192];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", db_dir);
    if (system(mkdir_cmd) != 0) {
        fprintf(stderr, "Error: Failed to create database directory %s\n", db_dir);
        return -1;
    }

    /* 1. Extract to root */
    char *tar_extract_argv[] = {(char*)"tar", (char*)"-xzf", (char*)pkgfile, (char*)"-C", (char*)root, nullptr};
    if (run_command("tar", tar_extract_argv) != 0) {
        fprintf(stderr, "Error: Failed to extract package to %s\n", root);
        return -1;
    }

    /* 2. Record file list */
    char files_path[4096];
    gpkg_path_join(files_path, db_dir, "files", sizeof(files_path));
    
    char tar_list_cmd[8192];
    snprintf(tar_list_cmd, sizeof(tar_list_cmd), "tar -tf %s > %s", pkgfile, files_path);
    if (system(tar_list_cmd) != 0) {
        fprintf(stderr, "Error: Failed to record file list.\n");
        return -1;
    }

    printf("%s installed successfully into %s.\n", pkgname_ver, root);
    return 0;
}

int gpkg_remove(const char *pkgname_ver) {
    const char *repo = getenv("GPKG_REPO");
    if (!repo) repo = "local";

    char db_dir[4096];
    gpkg_path_join(db_dir, PKG_DB_PATH, repo, sizeof(db_dir));
    gpkg_path_join(db_dir, db_dir, "installed", sizeof(db_dir));
    gpkg_path_join(db_dir, db_dir, pkgname_ver, sizeof(db_dir));

    struct stat st;
    if (stat(db_dir, &st) != 0) {
        /* Try searching all repos */
        DIR *d = opendir(PKG_DB_PATH);
        bool found = false;
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != nullptr) {
                if (ent->d_name[0] == '.') continue;
                char try_path[4096];
                gpkg_path_join(try_path, PKG_DB_PATH, ent->d_name, sizeof(try_path));
                gpkg_path_join(try_path, try_path, "installed", sizeof(try_path));
                gpkg_path_join(try_path, try_path, pkgname_ver, sizeof(try_path));
                if (stat(try_path, &st) == 0) {
                    gpkg_strscpy(db_dir, try_path, sizeof(db_dir));
                    found = true;
                    break;
                }
            }
            closedir(d);
        }
        if (!found) {
            fprintf(stderr, "Error: Package '%s' not found in database.\n", pkgname_ver);
            return -1;
        }
    }

    printf("Removing %s...\n", pkgname_ver);
    const char *root = getenv("GPKG_ROOT");
    if (!root) root = "/";

    char files_path[4096];
    gpkg_path_join(files_path, db_dir, "files", sizeof(files_path));

    /* 1. Remove files */
    char rm_files_cmd[8192];
    snprintf(rm_files_cmd, sizeof(rm_files_cmd), "while read -r f; do [ -f \"%s/$f\" ] || [ -L \"%s/$f\" ] && rm -f \"%s/$f\"; done < %s", root, root, root, files_path);
    system(rm_files_cmd);

    /* 2. Remove empty directories in reverse order */
    char rm_dirs_cmd[8192];
    snprintf(rm_dirs_cmd, sizeof(rm_dirs_cmd), "sort -r %s | while read -r f; do [ -d \"%s/$f\" ] && rmdir \"%s/$f\" 2>/dev/null || true; done", files_path, root, root);
    system(rm_dirs_cmd);

    /* 3. Remove database entry */
    char rm_db_cmd[8192];
    snprintf(rm_db_cmd, sizeof(rm_db_cmd), "rm -rf %s", db_dir);
    system(rm_db_cmd);

    printf("%s removed successfully.\n", pkgname_ver);
    return 0;
}

int gpkg_patch(const char *pkgdir, const char *pkgname) {
    char patches_dir[4096];
    if (gpkg_path_join(patches_dir, pkgdir, "patches", sizeof(patches_dir)) < 0) return -1;

    struct stat st;
    if (stat(patches_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return 0; // No patches
    }

    printf("Applying patches for %s...\n", pkgname);

    /* Find source directory (heuristic) */
    char src_dir[4096];
    gpkg_strscpy(src_dir, pkgdir, sizeof(src_dir)); // Default to pkgdir

    DIR *d = opendir(pkgdir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != nullptr) {
            if (ent->d_name[0] == '.') continue;
            if (strcmp(ent->d_name, "patches") == 0) continue;
            
            char full_path[4096];
            gpkg_path_join(full_path, pkgdir, ent->d_name, sizeof(full_path));
            struct stat s;
            if (stat(full_path, &s) == 0 && S_ISDIR(s.st_mode)) {
                /* Found a subdirectory, likely the source */
                gpkg_strscpy(src_dir, full_path, sizeof(src_dir));
                break; 
            }
        }
        closedir(d);
    }

    DIR *pd = opendir(patches_dir);
    if (!pd) return -1;

    struct dirent *pent;
    while ((pent = readdir(pd)) != nullptr) {
        if (strstr(pent->d_name, ".patch")) {
            printf("Applying %s...\n", pent->d_name);
            char patch_path[4096];
            gpkg_path_join(patch_path, patches_dir, pent->d_name, sizeof(patch_path));

            char *patch_argv[] = {(char*)"patch", (char*)"-p1", (char*)"-d", (char*)src_dir, (char*)"-i", (char*)patch_path, nullptr};
            run_command("patch", patch_argv);
        }
    }
    closedir(pd);
    return 0;
}

int gpkg_package_archive(const char *pkgname, const char *pkgver, const char *destdir) {
    char pkgfile[4096];
    if (snprintf(pkgfile, sizeof(pkgfile), "%s-%s.gpkg", pkgname, pkgver) >= (int)sizeof(pkgfile)) {
        fprintf(stderr, "Error: Package filename too long.\n");
        return -1;
    }

    printf("Creating package %s...\n", pkgfile);
    char *tar_argv[] = {(char*)"tar", (char*)"-czf", pkgfile, (char*)"-C", (char*)destdir, (char*)".", nullptr};
    return run_command("tar", tar_argv);
}


void list_packages(void) {
    DIR *db_dir = opendir(PKG_DB_PATH);
    if (db_dir == nullptr) {
        printf("No packages installed (database not found at %s).\n", PKG_DB_PATH);
        return;
    }

    struct dirent *repo_ent;
    bool found = false;
    while ((repo_ent = readdir(db_dir)) != nullptr) {
        if (repo_ent->d_name[0] == '.') continue;

        char repo_path[4096];
        if (gpkg_path_join(repo_path, PKG_DB_PATH, repo_ent->d_name, sizeof(repo_path)) < 0) continue;
        
        char installed_path[4096];
        if (gpkg_path_join(installed_path, repo_path, "installed", sizeof(installed_path)) < 0) continue;

        DIR *inst_dir = opendir(installed_path);
        if (!inst_dir) continue;

        struct dirent *pkg_ent;
        while ((pkg_ent = readdir(inst_dir)) != nullptr) {
            if (pkg_ent->d_name[0] == '.') continue;
            printf("[%s] %s\n", repo_ent->d_name, pkg_ent->d_name);
            found = true;
        }
        closedir(inst_dir);
    }

    if (!found) {
        printf("No packages installed.\n");
    }

    closedir(db_dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    Command cmd = parse_command(argv[1]);

    switch (cmd) {
        case CMD_BUILD: {
            if (argc < 3) {
                fprintf(stderr, "Error: 'build' requires a package directory.\n");
                return EXIT_FAILURE;
            }
            const char *pkgdir = argv[2];
            PackageInfo *info = get_pkg_info(pkgdir);
            if (!info) {
                fprintf(stderr, "Error: Could not read PKGBUILD in %s\n", pkgdir);
                return EXIT_FAILURE;
            }

            char *repo = get_repo_name(pkgdir);
            setenv("GPKG_REPO", repo, 1);

            if (info->source && strlen(info->source) > 0) {
                printf("Fetching sources for %s...\n", info->pkgname);
                char *src_dup = strdup(info->source);
                char *url = strtok(src_dup, ";");
                while (url) {
                    if (download_source(url) != 0) {
                        fprintf(stderr, "Error: Failed to download %s\n", url);
                        free(src_dup);
                        free_pkg_info(info);
                        free(repo);
                        return EXIT_FAILURE;
                    }
                    if (extract_source(pkgdir, url) != 0) {
                        fprintf(stderr, "Error: Failed to extract %s\n", url);
                        free(src_dup);
                        free_pkg_info(info);
                        free(repo);
                        return EXIT_FAILURE;
                    }
                    url = strtok(nullptr, ";");
                }
                free(src_dup);
            }

            /* Apply patches */
            if (gpkg_patch(pkgdir, info->pkgname) != 0) {
                fprintf(stderr, "Error: Failed to apply patches.\n");
                free_pkg_info(info);
                free(repo);
                return EXIT_FAILURE;
            }

            /* Setup destination directory */
            char destdir[4096];
            snprintf(destdir, sizeof(destdir), "pkg-%s", info->pkgname);
            char setup_cmd[8192];
            snprintf(setup_cmd, sizeof(setup_cmd), "rm -rf %s && mkdir -p %s", destdir, destdir);
            system(setup_cmd);

            /* Run build/package via shell */
            char abs_destdir[4096];
            char cwd[4096];
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                snprintf(abs_destdir, sizeof(abs_destdir), "%s/%s", cwd, destdir);
            } else {
                gpkg_strscpy(abs_destdir, destdir, sizeof(abs_destdir));
            }
            
            printf("Building %s-%s...\n", info->pkgname, info->pkgver);
            char shell_cmd[16384];
            snprintf(shell_cmd, sizeof(shell_cmd), 
                "export DESTDIR=\"%s\"; . %s/PKGBUILD; "
                "if type build >/dev/null 2>&1; then (cd %s && build); fi; "
                "if type package >/dev/null 2>&1; then (cd %s && package); fi",
                abs_destdir, pkgdir, pkgdir, pkgdir);
            
            int res = system(shell_cmd);
            if (res != 0) {
                fprintf(stderr, "Error: Build failed for %s\n", info->pkgname);
                free_pkg_info(info);
                free(repo);
                return EXIT_FAILURE;
            }

            /* Create the package archive */
            if (gpkg_package_archive(info->pkgname, info->pkgver, destdir) != 0) {
                fprintf(stderr, "Error: Failed to create package archive.\n");
                free_pkg_info(info);
                free(repo);
                return EXIT_FAILURE;
            }

            free_pkg_info(info);
            free(repo);
            return EXIT_SUCCESS;
        }

        case CMD_INSTALL: {
            if (argc < 3) {
                fprintf(stderr, "Error: 'install' requires a package file.\n");
                return EXIT_FAILURE;
            }
            /* Try to guess repo from environment or default to 'local' */
            if (!getenv("GPKG_REPO")) setenv("GPKG_REPO", "local", 0);
            return gpkg_install(argv[2]) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        case CMD_UPGRADE: {
            if (argc < 3) {
                fprintf(stderr, "Error: 'upgrade' requires a package file.\n");
                return EXIT_FAILURE;
            }
            if (!getenv("GPKG_REPO")) setenv("GPKG_REPO", "local", 0);
            /* Upgrade is currently just a re-install */
            return gpkg_install(argv[2]) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        case CMD_REMOVE: {
            if (argc < 3) {
                fprintf(stderr, "Error: 'remove' requires a package name.\n");
                return EXIT_FAILURE;
            }
            if (!getenv("GPKG_REPO")) setenv("GPKG_REPO", "local", 0);
            return gpkg_remove(argv[2]) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

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
