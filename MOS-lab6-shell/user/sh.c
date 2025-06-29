#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()#$"

int interactive;

int hisCount, curLine;
int hisBuf[1024];	// 第 i 条指令所占字节数

int next_run = 1;
int flag = 0;

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
	*p1 = 0;
	*p2 = 0;
	if (s == 0) {
		return 0;
	}

	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		return 0;
	}

	if (strchr(SYMBOLS, *s)) {
		int t = *s;
		if (t == '|' && *(s+1) == '|') {
			*p1 = s;
			*s = 0;
			s = s + 2;
			*p2 = s;
			return 'o';		// or <-> ||
		}
		if (t == '&' && *(s+1) == '&') {
			*p1 = s;
			*s = 0;
			s = s + 2;
			*p2 = s;
			return 'a';		// and <-> &&
		}
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}

	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
		s++;
	}
	*p2 = s;
	return 'w';
}

int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;

	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

#define MAXARGS 128

// 命令中是否有 cd
int parseCD(char *buf) {
	char *p = buf;
	if (strlen(buf) < 2) {
		return 0;
	}
	if (*p == 'c' && *(p+1) == 'd') {
		return 1;
	}
	for (int i = 0; i < strlen(buf) - 2; i++) {
		if (*(p+i) == ';' || *(p+i) == '&') {	// & 是什么情况
			i++;
			while (*(p+i) == ' ') {
				i++;
			}
			if (i <= strlen(buf) - 2 && *(p+i) == 'c' && *(p+i+1) == 'd') {
				return 1;
			}
		}
	}
	return 0;
}

int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;
	char new_p[1024];
	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		int child;

		if (c == 0) {
			next_run = 1;
			flag = 0;
			return argc;
		}
		
		if (!next_run) {
			if (c == 'a' && flag == 0) {
				next_run = 1;
			} else if (c == 'o' && flag == 1) {
				next_run = 1;
			}
			continue;
		}

		switch (c) {
		case 0:
		case '#':
			return argc;
		case 'a':
			child = fork();
			if (child == 0) {
				return argc;
			} else if (child > 0) {
				flag = wait(child);
				close(0);
				close(1);
				dup(opencons(), 1);
				dup(1, 0);
				if (flag == 0) {
					next_run = 1;
				} else {
					next_run = 0;
				}
				// debugf("flag: %d\n", flag);
				// debugf("next_run: %d\n", next_run);
				return parsecmd(argv, rightpipe);
			}
			break;
		case 'o':
			child = fork();
			if (child == 0) {
				return argc;
			} else if (child > 0) {
				flag = wait(child);
				close(0);
				close(1);
				dup(opencons(), 1);
				dup(1, 0);
				if (flag == 0) {
					next_run = 0;
				} else {
					next_run = 1;
				}
				// debugf("flag: %d\n", flag);
				// debugf("next_run: %d\n", next_run);
				return parsecmd(argv, rightpipe);
			}
			break;
		case '$':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: $ not followed by word\n");
				exit(1);
			}

			char *pbuf = t;
			char varname[128] = {0};
			char varbuf[1024];
			int i = 0;
			// 只读取变量名部分: 字母/数字/下划线
			while (*pbuf >= 'a' && *pbuf <= 'z' || *pbuf >= 'A' && *pbuf <= 'Z' 
					|| *pbuf >= '0' && *pbuf <= '9' || *pbuf == '_') {
				varname[i++] = *pbuf++;
			}
			varname[i] = '\0';
			// 获取变量值
			int r = syscall_get_shell_var(varname, varbuf);
			if (r < 0) {
				debugf("var: %s\n", varname);
				debugf("The var you need does not exist\n");
				exit(1);
			}
			// 拼接后续部分
			char fullbuf[1024];
			strcpy(fullbuf, varbuf); // 变量值
			char *pafter = fullbuf + strlen(varbuf);
			strcpy(pafter, pbuf);
			argv[argc++] = fullbuf;
			break;
		case 'w':
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit(1);
			}
			argv[argc++] = t;
			break;
		case ';':
			child = fork();
			if (child == 0) {
				return argc;
			} else if (child > 0) {
				wait(child);
				close(0);
				close(1);
				dup(opencons(), 1);
				dup(1, 0);
				return parsecmd(argv, rightpipe);
			}
			break;
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit(1);
			}
			parse_rpath(t, new_p);
			if ((fd = open(new_p, O_RDONLY)) < 0) {
				debugf("error opening\n");
				exit(1);
			}
			dup(fd, 0);
			close(fd);
			break;

			user_panic("< redirection not implemented");
			break;
		case '>':
			int tok = gettoken(0, &t);
			if (tok == '>') {
				if (gettoken(0, &t) != 'w') {
					debugf("syntax error: > not followed by word\n");
					exit(1);
				}
				parse_rpath(t, new_p);
				if ((fd = open(new_p, O_APPEND | O_WRONLY | O_CREAT)) < 0) {
					debugf("error opening\n");
					exit(1);
				}
				dup(fd, 1);
				close(fd);
				break;
			} else if (tok == 'w') {
				parse_rpath(t, new_p);
				if ((fd = open(new_p, O_WRONLY | O_CREAT)) < 0) {				
					debugf("error opening\n");
					exit(1);
				}
				dup(fd, 1);
				close(fd);
				break;
			} else {
				debugf("syntax error: > or >> not followed by word\n");
				exit(1);
			}
			user_panic("> redirection not implemented");
			break;
		case '|':;
			int p[2];
			pipe(p);
			*rightpipe = fork();
			if (*rightpipe == 0) {
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe);
			} else if (*rightpipe > 0) {
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}
			break;

			user_panic("| not implemented");

			break;
		}
	}

	return argc;
}

// 这里的 argv 是没有去掉 ./.. 的
void useCD(int argc, char* argv) {
	int r;
	char cur[1024];
	struct Stat st = {0};

	if (argc == 1) {
		cur[0] = '/';
	} else {
		parse_rpath(argv, cur);
	}

	if ((r = stat(cur, &st)) < 0) { 
		printf("cd: The directory \'%s\' does not exist\n", argv);
		return;
	}
	if (!st.st_isdir) {
		printf("cd: \'%s\' is not a directory\n", argv);
		return;
	}
	if ((r = chdir(cur)) < 0) { 
		printf("6");
	}
	return;
}

int executeCommandAndCaptureOutput(char *cmd, char *output, int maxLen) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1;
    }

    int pid = fork();
    if (pid == -1) {
        return -1;
    }

    if (pid == 0) { 	// Child process
        dup(pipefd[1], 1);
        close(pipefd[1]);
        close(pipefd[0]);
		runcmd(cmd);
    } else { 	// Parent process
		// dup(pipefd[0], 0);
        close(pipefd[1]);
		int r;
		for (int i = 0; i < maxLen; i++) {
			if ((r = read(pipefd[0], output + i, 1)) != 1) {
				if (r < 0) {
					debugf("read error: %d\n", r);
				}
				break;
			}
		}
        close(pipefd[0]);
        wait(pid);
    }
    return 0;
}

int replaceBackquoteCommands(char *cmd) {
    char *begin, *end;
    char output[1024];
    
	while (1) {
		char *t = cmd;
		int in_quotes = 0;

		begin = 0;
		while(*t != '\0') {
			if (*t == '\"') {
				in_quotes = !in_quotes;
			}
			if (*t == '`' && !in_quotes) {
				begin = t;
				t++;
				break;
			}
			t++;
		}
		if (begin == NULL) {
			return 0; // No backquote found
		}
		end = 0;
		while(*t != '\0') {
			if (*t == '\"') {
				in_quotes = !in_quotes;
			}
			if (*t == '`' && !in_quotes) {
				end = t;
				t++;
				break;
			}
			t++;
		}
        if (end == NULL) {
            return -1; 	// Syntax error: unmatched backquote
        }

        *begin = '\0'; 	// Terminate the string at the start of the backquote command
        *end = '\0'; 	// Terminate the backquote command

		char temp_cmd[1024];
		strcpy(temp_cmd, end + 1);

        // Execute the command and capture its output
        if (executeCommandAndCaptureOutput(begin + 1, output, sizeof(output)) == -1) {
            return -1;
        }
        // Concatenate the parts
        strcat(cmd, output);
        strcat(cmd, temp_cmd);
    }
    return 0;
}

void runcmd(char *s) {
	int exit_value = 0;
	gettoken(s, 0);		// 最最初始的解析

	char *argv[MAXARGS];
	int rightpipe = 0;
	int argc = parsecmd(argv, &rightpipe);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;

	if (strcmp("cd", argv[0]) == 0) {
		if (argc > 2) {
			debugf("Too many args for cd command\n");
			return;
		} else if (argc == 1) {		// 进入根目录 /
			chdir("/");
			return;
		} else {
			useCD(argc, argv[1]);
			return;
		}
	}

	int child = spawn(argv[0], argv);

	if (child < 0) {
		char new_cmd[1024] = {0};
		strcpy(new_cmd, argv[0]);
		int len = strlen(argv[0]);
		new_cmd[len] = '.';
		new_cmd[len + 1] = 'b';
		new_cmd[len + 2] = '\0'; // 将ls扩展成ls.b
		child = spawn(new_cmd, argv);
	}

	close_all();

	if (child >= 0) {
		exit_value = wait(child);
	} else {
		exit_value = 1;
		debugf("spawn %s: %d\n", argv[0], child);
	}
	if (rightpipe) {
		wait(rightpipe);
	}
	exit(exit_value);
}

int readPast(int target, char *code) {
	int r, fd, spot = 0;
	char buff[1024];
	if ((fd = open("/.mos_history", O_RDONLY)) < 0) {
		return fd;
	}
	for (int i = 0; i < target; i++) {
		spot += (hisBuf[i] + 1);
	}
	if ((r = readn(fd, buff, spot)) != spot) {
		return r;
	}
	if ((r = readn(fd, code, hisBuf[target])) != hisBuf[target]) {
		return r;
	}
	if ((r = close(fd)) < 0) {
		return r;
	}
	code[hisBuf[target]] = '\0';
	return 0;
}

void readline(char *buf, u_int n) {
	char curIn[1024];
	int r, len = 0;
	char temp = 0;
	for (int i = 0; len < n;) {
		if ((r = read(0, &temp, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit(1);
		}
		switch (temp) {
			// Ctrl-A: Move cursor to beginning
			case 0x01:  // Ctrl-A
				if (interactive && i > 0) {
					printf("\033[%dD", i);
					i = 0;
				}
				break;

			// Ctrl-E: Move cursor to end
			case 0x05:  // Ctrl-E
				if (interactive && i < len) {
					printf("\033[%dC", len - i);
					i = len;
				}
				break;

			// Ctrl-K: Delete to end of line
			case 0x0B:  // Ctrl-K
				if (interactive && i < len) {
					int deleted = len - i;
					for (int j = i; j < len; j++) {
						buf[j] = 0;
					}
					buf[i] = '\0';
					len = i;
					for (int j = 0; j < deleted; j++) {
						printf(" ");
					}
					printf("\033[%dD", deleted);
				}
				break;

			// Ctrl-U: Delete to beginning of line
			case 0x15:  // Ctrl-U
				if (interactive && i > 0) {
					int orig_i = i;
					for (int j = 0; j < len - i; j++) {
						buf[j] = buf[i + j];
					}
					len = len - i;
					i = 0;
					buf[len] = 0;
					printf("\033[%dD%s", orig_i, buf);
					for (int j = 0; j < orig_i; j++) {
						printf(" ");
					}
					printf("\033[%dD", orig_i);
				}
				break;

			// Ctrl-W: Delete previous word
			case 0x17:  // Ctrl-W
				if (interactive && i > 0) {
					int orig = i;
					// First skip spaces
					while (i > 0 && (buf[i - 1] == ' ' || buf[i - 1] == '\t'))
						i--;
					// Then skip non-spaces
					while (i > 0 && buf[i - 1] != ' ' && buf[i - 1] != '\t')
						i--;
					int toDel = orig - i;
					for (int j = i; j <= len - toDel; j++) {
						buf[j] = buf[j + toDel];
					}
					len -= toDel;
					buf[len] = '\0';
					printf("\033[%dD%s", orig, buf);
					int toBack = len - i;
					for (int k = 0; k < toDel; k++) printf(" ");
					printf("\033[%dD", toBack + toDel);
				}
				break;

			case '\b':
			case 0x7f:
				if (i <= 0) { break; } // cursor at left bottom, ignore backspace

				for (int j = (--i); j <= len - 1; j++) {
					buf[j] = buf[j + 1];
				}
				buf[--len] = 0;
				printf("\033[%dD%s \033[%dD", (i + 1), buf, (len - i + 1));
				break;

			case '\033':
				read(0, &temp, 1);
				if (temp == '[') {
					read(0, &temp, 1);
					if (temp == 'D') { // have space for left
						if (i > 0) {
							i -= 1;
						} else {
							printf("\033[C");
						}
					} else if (temp == 'C') {
						if (i < len) {
							i += 1;
						} else {
							printf("\033[D");
						}
					} else if (temp == 'A') { // up
						printf("\033[B");
						if (curLine != 0) {
							buf[len] = '\0';
							if (curLine == hisCount) {
								strcpy(curIn, buf);
							}
							if (i != 0) { printf("\033[%dD", i); }
							for (int j = 0; j < len; j++) { printf(" "); }
							if (len != 0) { printf("\033[%dD", len); }

							if ((r = readPast(--curLine, buf)) < 0) { printf("G");exit(1); }
							printf("%s", buf);
							i = strlen(buf);
							len = i;
							// redirect cursor
						}
					} else if (temp == 'B') {
						buf[len] = '\0';
						if (i != 0) { printf("\033[%dD", i); }
						for (int j = 0; j < len; j++) { printf(" "); }
						if (len != 0) { printf("\033[%dD", len); }
						if (1 + curLine < hisCount) {
							if ((r = readPast(++curLine, buf)) < 0) { printf("G");exit(1); }
						} else {
							strcpy(buf, curIn);
							curLine = hisCount;
						}
						printf("%s", buf);
						i = strlen(buf);
						len = i;
						// redirect cursor	
					}
				}
				break;

			case '\r':
			case '\n':
				buf[len] = '\0';
				int hisFd;
				if ((hisFd = open("/.mos_history", O_APPEND | O_WRONLY)) < 0) { 
					printf("err2\n"); 
					exit(1); 
				}
				if ((r = write(hisFd, buf, len)) != len) { 
					printf("err3\n"); 
					exit(1); 
				}
				if ((r = write(hisFd, "\n", 1)) != 1) { 
					printf("err4\n"); 
					exit(1); 
				}
				if ((r = close(hisFd)) < 0) { 
					printf("err5\n"); 
					exit(1); 
				}
				hisBuf[hisCount++] = len;
				curLine = hisCount; // cannot 'curLine++', otherwise usable instrctions will be [0, curLine + 1]
				memset(curIn, '\0', sizeof(curIn));
				return;
			default:
				buf[len + 1] = 0;
				for (int j = len; j >= i + 1; j--) {
					buf[j] = buf[j - 1];
				}
				buf[i++] = temp;
				if (interactive != 0) {
					printf("\033[%dD%s", i, buf);
					if ((r = len++ + 1 - i) != 0) {
						printf("\033[%dD", r);
					}
				} else {
					len++;
				}
			break;
		}
	}
	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit(1);
}

int is_exit(char *buf) {
	if (strcmp(buf, "exit") == 0) {
		return 1;
	}
	return 0;
}

void init_history() {
    int fd, r, offset = 0;
    char c;
    hisCount = 0;
    if ((fd = open("/.mos_history", O_RDONLY)) < 0) {
        return;
    }
    int len = 0;
    while ((r = read(fd, &c, 1)) == 1) {
        if (c == '\n') {
            hisBuf[hisCount++] = len;
            len = 0;
        } else {
            len++;
        }
    }
    close(fd);
}

void changeEq2Space(char *wbuf) {
	if (wbuf == 0) {
		return;
	}
	while (*wbuf != '\0') {
		if (*wbuf == '=') {
			*wbuf = ' ';
		}
		wbuf++;
	}
}

int main(int argc, char **argv) {
	int r;
	int echocmds = 0;
	interactive = iscons(0);
	
	syscall_shellid_alloc();
	syscall_extend_vars();
	if ((r = open("/.mos_history", O_CREAT)) < 0) {
		printf("err1\n");
		exit(1);
	}
	init_history();

	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2024                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}
	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
		readline(buf, sizeof buf);

		replaceBackquoteCommands(buf);
		changeEq2Space(buf);

		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}

		if (parseCD(buf) == 1) {
			runcmd(buf);
			continue;
		}

		if (is_exit(buf)) {
			exit(0);
		}

		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}
		if (r == 0) {
			runcmd(buf);
			exit(0);
		} else {
			wait(r);
		}
	}
	return 0;
}
