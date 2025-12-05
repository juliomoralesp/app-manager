#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <regex.h>

#define MAX_APP_NAME_LEN 256
#define MAX_CMD_LEN 1024
#define NUM_COLUMNS 5
#define UI_HEADER_FOOTER_ROWS 10
#define CONFIG_FILE "app_manager.conf"

struct termios orig_termios;

typedef struct {
    char* master_apps;
    char* displayed_apps;
    char* updatable_apps;
    int* selected_apps;
    int master_app_count;
    int displayed_app_count;
    int updatable_app_count;
    char search_term[MAX_APP_NAME_LEN];
} AppData;

typedef struct {
    char quit;
    char down;
    char up;
    char left;
    char right;
    char next_page;
    char prev_page;
    char select;
    char search;
    char remove;
    char update;
    char only_updatable;
    char select_all_updatable;
} KeyBindings;

void disable_raw_mode() {
    printf("\033[0m"); // Reset terminal colors
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void get_terminal_size(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        *rows = 24;
        *cols = 80;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
}

int execute_command(const char* command, char* output, size_t output_size) {
    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen failed");
        return -1;
    }

    size_t bytes_read = fread(output, 1, output_size - 1, fp);
    output[bytes_read] = '\0';

    return pclose(fp);
}

int compare_strings(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

int get_apps(const char* command, char** apps, int* count) {
    char output[32768];
    int status = execute_command(command, output, sizeof(output));

    if (status == -1) {
        return 0;
    }

    int app_count = 0;
    char* line = strtok(output, "\n");
    while (line != NULL && app_count < *count) {
        char app_name[MAX_APP_NAME_LEN];
        if (sscanf(line, "%s", app_name) == 1) {
            char* slash_pos = strchr(app_name, '/');
            if (slash_pos != NULL) {
                *slash_pos = '\0';
            }
            strcpy((*apps) + (app_count * MAX_APP_NAME_LEN), app_name);
            app_count++;
        }
        line = strtok(NULL, "\n");
    }
    *count = app_count;
    return *count;
}

void display_apps(AppData* app_data, int page, int cursor_pos, int terminal_cols, int apps_per_page) {
    // Clear screen and move cursor to top-left
    printf("\033[2J\033[H\033[44m");
    printf("--- Installed Applications (Page %d) ---\r\n", page + 1);

    int start = page * apps_per_page;
    int end = start + apps_per_page;
    if (end > app_data->displayed_app_count) {
        end = app_data->displayed_app_count;
    }

    int column_width = terminal_cols / NUM_COLUMNS;
    int name_width = column_width - 9;

    for (int i = start; i < end; i++) {
        char check = app_data->selected_apps[i] ? 'x' : ' ';
        char cursor = (i == cursor_pos) ? '>' : ' ';

        char* app_name = app_data->displayed_apps + (i * MAX_APP_NAME_LEN);
        char* is_upgradable = bsearch(app_name, app_data->updatable_apps, app_data->updatable_app_count, MAX_APP_NAME_LEN, compare_strings);
        char upgradable_marker = is_upgradable ? '*' : ' ';

        printf("%c [%c] %c %-*.*s", cursor, check, upgradable_marker, name_width, name_width, app_name);

        if ((i - start + 1) % NUM_COLUMNS == 0) {
            printf("\r\n");
        } else {
            printf("|");
        }
    }

    if ((end - start) % NUM_COLUMNS != 0) {
        printf("\r\n");
    }

    printf("--------------------------------------------------------------------------------\r\n");
    if (app_data->search_term[0] != '\0') {
        printf("Search: %s\r\n", app_data->search_term);
    }
    if (cursor_pos >= 0 && cursor_pos < app_data->displayed_app_count) {
        printf("Full name: %s\r\n", app_data->displayed_apps + (cursor_pos * MAX_APP_NAME_LEN));
    }
    printf("--------------------------------------------------------------------------------\r\n");
}

void manage_apps_batch(const char* action, char* app_names[], int num_apps) {
    disable_raw_mode();
    printf("Preparing to run: sudo apt-get %s -y", action);
    for (int i = 0; i < num_apps; i++) {
        printf(" %s", app_names[i]);
    }
    printf("\n");

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        return;
    } else if (pid == 0) {
        char** args = malloc(sizeof(char*) * (num_apps + 5));
        if (args == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        args[0] = "sudo";
        args[1] = "apt-get";
        args[2] = (char*)action;
        args[3] = "-y";
        for (int i = 0; i < num_apps; i++) {
            args[i + 4] = app_names[i];
        }
        args[num_apps + 4] = NULL;

        execvp("sudo", args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
        } else {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                printf("Batch operation completed successfully.\n");
            } else {
                fprintf(stderr, "Batch operation failed.\n");
            }
        }
    }

    printf("Press Enter to continue...");
    getchar();
    enable_raw_mode();
}

void load_key_bindings(KeyBindings* keys) {
    // Default values
    keys->quit = 'q';
    keys->down = 'j';
    keys->up = 'm';
    keys->left = 'h';
    keys->right = 'l';
    keys->next_page = 'n';
    keys->prev_page = 'p';
    keys->select = ' ';
    keys->search = '/';
    keys->remove = 'r';
    keys->update = 'u';
    keys->only_updatable = 'o';
    keys->select_all_updatable = 'A';

    FILE* fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        return; // Config file not found, use defaults
    }

    char line[100];
    while (fgets(line, sizeof(line), fp)) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "quit") == 0) keys->quit = value[0];
            else if (strcmp(key, "down") == 0) keys->down = value[0];
            else if (strcmp(key, "up") == 0) keys->up = value[0];
            else if (strcmp(key, "left") == 0) keys->left = value[0];
            else if (strcmp(key, "right") == 0) keys->right = value[0];
            else if (strcmp(key, "next_page") == 0) keys->next_page = value[0];
            else if (strcmp(key, "prev_page") == 0) keys->prev_page = value[0];
            else if (strcmp(key, "select") == 0) keys->select = value[0];
            else if (strcmp(key, "search") == 0) keys->search = value[0];
            else if (strcmp(key, "remove") == 0) keys->remove = value[0];
            else if (strcmp(key, "update") == 0) keys->update = value[0];
            else if (strcmp(key, "only_updatable") == 0) keys->only_updatable = value[0];
            else if (strcmp(key, "select_all_updatable") == 0) keys->select_all_updatable = value[0];
        }
    }
    fclose(fp);
}

void init_data(AppData* app_data) {
    int max_apps = 4096;
    app_data->master_apps = malloc(max_apps * MAX_APP_NAME_LEN);
    app_data->displayed_apps = malloc(max_apps * MAX_APP_NAME_LEN);
    app_data->updatable_apps = malloc(max_apps * MAX_APP_NAME_LEN);
    app_data->selected_apps = malloc(max_apps * sizeof(int));
    app_data->search_term[0] = '\0';

    if (!app_data->master_apps || !app_data->displayed_apps || !app_data->updatable_apps || !app_data->selected_apps) {
        perror("Failed to allocate memory for apps");
        exit(EXIT_FAILURE);
    }
    
    printf("Loading installed applications...\n");
    app_data->master_app_count = max_apps;
    get_apps("dpkg --get-selections | grep -v deinstall", &app_data->master_apps, &app_data->master_app_count);
    if (app_data->master_app_count > 0) {
        qsort(app_data->master_apps, app_data->master_app_count, MAX_APP_NAME_LEN, compare_strings);
    }

    printf("Checking for updatable applications...\n");
    app_data->updatable_app_count = max_apps;
    get_apps("apt list --upgradable 2>/dev/null", &app_data->updatable_apps, &app_data->updatable_app_count);
    if (app_data->updatable_app_count > 0) {
        qsort(app_data->updatable_apps, app_data->updatable_app_count, MAX_APP_NAME_LEN, compare_strings);
    }
}

void handle_input(char c, AppData* app_data, KeyBindings* keys, int* cursor_pos, int* current_page, int* only_updatable, int* needs_redraw, int apps_per_page) {
    *needs_redraw = 1;

    if (c == '\x1b') { // Arrow keys
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return;
        if (seq[0] == '[') {
            if (seq[1] == 'A') c = keys->up;
            if (seq[1] == 'B') c = keys->down;
            if (seq[1] == 'C') c = keys->right;
            if (seq[1] == 'D') c = keys->left;
        }
    }

    if (c == keys->quit) {
        exit(0);
    } else if (c == keys->down) {
        if (*cursor_pos + NUM_COLUMNS < app_data->displayed_app_count) *cursor_pos += NUM_COLUMNS;
    } else if (c == keys->up) {
        if (*cursor_pos >= NUM_COLUMNS) *cursor_pos -= NUM_COLUMNS;
    } else if (c == keys->left) {
        if (*cursor_pos > 0) (*cursor_pos)--;
    } else if (c == keys->right) {
        if (*cursor_pos < app_data->displayed_app_count - 1) (*cursor_pos)++;
    } else if (c == keys->select) {
        app_data->selected_apps[*cursor_pos] = !app_data->selected_apps[*cursor_pos];
    } else if (c == keys->next_page) {
        if ((*current_page + 1) * apps_per_page < app_data->displayed_app_count) {
            (*current_page)++;
            *cursor_pos = (*current_page) * apps_per_page;
        }
    } else if (c == keys->prev_page) {
        if (*current_page > 0) {
            (*current_page)--;
            *cursor_pos = (*current_page) * apps_per_page;
        }
    } else if (c == keys->only_updatable) {
        *only_updatable = !(*only_updatable);
        *cursor_pos = 0;
        *current_page = 0;
        memset(app_data->selected_apps, 0, app_data->master_app_count * sizeof(int));
    } else if (c == keys->select_all_updatable) {
        for (int i = 0; i < app_data->displayed_app_count; i++) {
            char* app_name = app_data->displayed_apps + (i * MAX_APP_NAME_LEN);
            char* is_upgradable = bsearch(app_name, app_data->updatable_apps, app_data->updatable_app_count, MAX_APP_NAME_LEN, compare_strings);
            if (is_upgradable) {
                app_data->selected_apps[i] = 1;
            }
        }
    } else if (c == keys->search) {
        disable_raw_mode();
        printf("\r\nSearch (regex): ");
        fflush(stdout);
        if (fgets(app_data->search_term, MAX_APP_NAME_LEN, stdin) == NULL) {
            app_data->search_term[0] = '\0';
        }
        app_data->search_term[strcspn(app_data->search_term, "\n")] = 0;
        enable_raw_mode();
        *only_updatable = 0;
        *cursor_pos = 0;
        *current_page = 0;
        memset(app_data->selected_apps, 0, app_data->master_app_count * sizeof(int));
    } else if (c == keys->remove || c == keys->update) {
        const char* action = (c == keys->remove) ? "remove" : "install";
        const char* action_desc = (c == keys->remove) ? "remove" : "update";

        int selected_count = 0;
        for (int i = 0; i < app_data->displayed_app_count; i++) {
            if (app_data->selected_apps[i]) {
                selected_count++;
            }
        }

        if (selected_count > 0) {
            disable_raw_mode();
            printf("\r\nAre you sure you want to %s %d selected application(s)? (y/N) ", action_desc, selected_count);
            char confirm_char[3];
            if (fgets(confirm_char, 3, stdin) != NULL && (confirm_char[0] == 'y' || confirm_char[0] == 'Y')) {
                char** selected_app_names = malloc(sizeof(char*) * selected_count);
                if (selected_app_names == NULL) {
                    perror("malloc failed");
                    return;
                }

                int current_app = 0;
                for (int i = 0; i < app_data->displayed_app_count; i++) {
                    if (app_data->selected_apps[i]) {
                        selected_app_names[current_app++] = app_data->displayed_apps + (i * MAX_APP_NAME_LEN);
                    }
                }
                manage_apps_batch(action, selected_app_names, selected_count);
                free(selected_app_names);
                init_data(app_data); // Reload data
            }
            enable_raw_mode();
        }
    } else {
        *needs_redraw = 0; // Unhandled key
    }
}

int main() {
    AppData app_data;
    KeyBindings keys;
    int cursor_pos = 0;
    int current_page = 0;
    int only_updatable = 0;
    int needs_redraw = 1;
    int terminal_rows, terminal_cols;
    int apps_per_page;

    load_key_bindings(&keys);
    init_data(&app_data);

    if (app_data.master_app_count == 0) {
        fprintf(stderr, "Could not retrieve any installed applications. Exiting.\n");
        return 1;
    }

    printf("%d applications loaded (%d updatable). Press any key to continue.\n", app_data.master_app_count, app_data.updatable_app_count);
    getchar();

    enable_raw_mode();

    while (1) {
        if (needs_redraw) {
            get_terminal_size(&terminal_rows, &terminal_cols);
            apps_per_page = (terminal_rows - UI_HEADER_FOOTER_ROWS) * NUM_COLUMNS;
            
            if (only_updatable) {
                memcpy(app_data.displayed_apps, app_data.updatable_apps, app_data.updatable_app_count * MAX_APP_NAME_LEN);
                app_data.displayed_app_count = app_data.updatable_app_count;
            } else if (app_data.search_term[0] != '\0') {
                regex_t regex;
                if (regcomp(&regex, app_data.search_term, REG_EXTENDED | REG_ICASE) == 0) {
                    app_data.displayed_app_count = 0;
                    for (int i = 0; i < app_data.master_app_count; i++) {
                        char* app_name = app_data.master_apps + (i * MAX_APP_NAME_LEN);
                        if (regexec(&regex, app_name, 0, NULL, 0) == 0) {
                             strcpy(app_data.displayed_apps + (app_data.displayed_app_count * MAX_APP_NAME_LEN), app_name);
                             app_data.displayed_app_count++;
                        }
                    }
                    regfree(&regex);
                }
            } else {
                memcpy(app_data.displayed_apps, app_data.master_apps, app_data.master_app_count * MAX_APP_NAME_LEN);
                app_data.displayed_app_count = app_data.master_app_count;
            }

            display_apps(&app_data, current_page, cursor_pos, terminal_cols, apps_per_page);
            
            printf("\r\nMenu:\r\n");
            printf("  - Arrows or '%c'/'%c'/'%c'/'%c' to move.\r\n", keys.up, keys.down, keys.left, keys.right);
            printf("  - '%c' to select/deselect, '%c' to search.\r\n", keys.select, keys.search);
            printf("  - '%c'/'%c' for next/previous page.\r\n", keys.next_page, keys.prev_page);
            printf("  - '%c' to remove selected, '%c' to update selected.\r\n", keys.remove, keys.update);
            printf("  - '%c' to show only updatable apps, '%c' to select all updatable.\r\n", keys.only_updatable, keys.select_all_updatable);
            printf("  - '%c' to quit.\r\n", keys.quit);
            printf("Your choice: ");
            fflush(stdout);
        }

        needs_redraw = 0;
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) != 1) continue;
        
        handle_input(c, &app_data, &keys, &cursor_pos, &current_page, &only_updatable, &needs_redraw, apps_per_page);

        if (cursor_pos >= (current_page + 1) * apps_per_page) {
            if((current_page + 1) * apps_per_page < app_data.displayed_app_count) {
                current_page++;
            }
        }
        if (cursor_pos < current_page * apps_per_page) {
            if(current_page > 0) {
                current_page--;
            }
        }
    }

    free(app_data.master_apps);
    free(app_data.displayed_apps);
    free(app_data.updatable_apps);
    free(app_data.selected_apps);
    
    disable_raw_mode();
    printf("\r\n");
    return 0;
}
