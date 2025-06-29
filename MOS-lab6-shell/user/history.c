#include <lib.h>
#include <args.h>

#define MAX_KEEP 20

void usage(void) {
    debugf("usage: history\n");
    exit(1);
}

int main(int argc, char **argv) {
    if (argc != 1) {
        usage();
    }

    // printf("history instruction:\n\n");

    int fd, r, total_lines = 0;
    char ch;

    // --- Pass 1: Count total number of lines ---
    if ((fd = open("/.mos_history", O_RDONLY)) < 0) {
        user_panic("history: %d", fd);
    }
    while ((r = read(fd, &ch, 1)) == 1) {
        if (ch == '\n') total_lines++;
    }
    close(fd);

    if (total_lines == 0) {
        printf("no history instruction.\n");
        exit(1);
    }

    // --- Pass 2: Reopen and skip to last MAX_KEEP lines ---
    if ((fd = open("/.mos_history", O_RDONLY)) < 0) {
        user_panic("history: %d", fd);
    }

    int current_line = 0;
    int start_line = (total_lines > MAX_KEEP) ? (total_lines - MAX_KEEP + 1) : 1;
    int print_line = 1;
    int printing = 0;

    while ((r = read(fd, &ch, 1)) == 1) {
        if (print_line == 1 && !printing && current_line + 1 >= start_line) {
            // printf(" %4d : ", start_line);
            printing = 1;
        }

        if (printing) {
            printf("%c", ch);
        }

        if (ch == '\n') {
            current_line++;
            if (printing && current_line >= start_line) {
                print_line++;
                if (current_line < total_lines) {
                    // printf(" %4d : ", start_line + print_line - 1);
                }
            }
        }
    }

    close(fd);
    // printf("\n\ntotal instruction: %d\nhistory finished.\n\n", total_lines);
    return 0;
}
