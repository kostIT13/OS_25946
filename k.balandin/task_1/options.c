#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

// Структура для хранения опции и её аргумента
typedef struct {
    char opt;
    char *arg;
} Action;

int main(int argc, char *argv[]) {
    // Массив для хранения опций. Максимальный размер равен argc
    Action actions[argc];
    int action_count = 0;
    int c;
    
    // Строка опций для getopt: 
    // i, s, p, u, c, d, v - без аргументов
    // U:, C:, V: - с обязательным аргументом
    char *optstring = "ispuU:cC:dvV:";

    // Разрешаем getopt выводить сообщения об ошибках для неверных опций
    opterr = 1;

    // 1. Парсинг аргументов (getopt всегда идет слева направо)
    while ((c = getopt(argc, argv, optstring)) != -1) {
        actions[action_count].opt = c;
        actions[action_count].arg = optarg;
        action_count++;
    }

    // 2. Обработка опций СПРАВА НАЛЕВО (обратный цикл)
    for (int i = action_count - 1; i >= 0; i--) {
        switch (actions[i].opt) {
            case 'i': {
                printf("Real UID: %d, Effective UID: %d\n", getuid(), geteuid());
                printf("Real GID: %d, Effective GID: %d\n", getgid(), getegid());
                break;
            }
            case 's': {
                // 0, 0 означает: текущий процесс, и новый PGID равен текущему PID
                if (setpgid(0, 0) == -1) {
                    perror("Ошибка setpgid");
                } else {
                    printf("Процесс стал лидером группы процессов (PGID: %d)\n", getpgrp());
                }
                break;
            }
            case 'p': {
                printf("PID: %d, PPID: %d, PGID: %d\n", getpid(), getppid(), getpgrp());
                break;
            }
            case 'u': {
                struct rlimit rl;
                if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
                    printf("Текущий ulimit (RLIMIT_NOFILE): soft=%lld, hard=%lld\n", 
                           (long long)rl.rlim_cur, (long long)rl.rlim_max);
                } else {
                    perror("Ошибка getrlimit");
                }
                break;
            }
            case 'U': {
                char *endptr;
                errno = 0;
                long new_limit = strtol(actions[i].arg, &endptr, 10);
                
                // Проверка на ошибки преобразования: errno, остаток строки, отрицательное значение
                if (errno != 0 || *endptr != '\0' || new_limit < 0) {
                    fprintf(stderr, "Ошибка: некорректное значение для -U: '%s'\n", actions[i].arg);
                } else {
                    struct rlimit rl;
                    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
                        rl.rlim_cur = (rlim_t)new_limit;
                        if (new_limit > rl.rlim_max) {
                            rl.rlim_max = (rlim_t)new_limit;
                        }
                        if (setrlimit(RLIMIT_NOFILE, &rl) == 0) {
                            printf("ulimit успешно изменен на %ld\n", new_limit);
                        } else {
                            perror("Ошибка setrlimit");
                        }
                    }
                }
                break;
            }
            case 'c': {
                struct rlimit rl;
                if (getrlimit(RLIMIT_CORE, &rl) == 0) {
                    printf("Размер core-файла (soft): %lld байт, (hard): %lld байт\n", 
                           (long long)rl.rlim_cur, (long long)rl.rlim_max);
                } else {
                    perror("Ошибка getrlimit (core)");
                }
                break;
            }
            case 'C': {
                char *endptr;
                errno = 0;
                long new_core_size = strtol(actions[i].arg, &endptr, 10);
                
                if (errno != 0 || *endptr != '\0' || new_core_size < 0) {
                    fprintf(stderr, "Ошибка: некорректное значение для -C: '%s'\n", actions[i].arg);
                } else {
                    struct rlimit rl;
                    if (getrlimit(RLIMIT_CORE, &rl) == 0) {
                        rl.rlim_cur = (rlim_t)new_core_size;
                        // Не превышаем hard limit, если новое значение больше его
                        if (new_core_size > rl.rlim_max) {
                            rl.rlim_max = (rlim_t)new_core_size;
                        }
                        if (setrlimit(RLIMIT_CORE, &rl) == 0) {
                            printf("Размер core-файла успешно изменен на %ld байт\n", new_core_size);
                        } else {
                            perror("Ошибка setrlimit (core)");
                        }
                    }
                }
                break;
            }
            case 'd': {
                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                    printf("Текущая рабочая директория: %s\n", cwd);
                } else {
                    perror("Ошибка getcwd");
                }
                break;
            }
            case 'v': {
                extern char **environ;
                printf("--- Переменные окружения ---\n");
                for (char **env = environ; *env != NULL; env++) {
                    printf("%s\n", *env);
                }
                printf("----------------------------\n");
                break;
            }
            case 'V': {
                // Ожидаемый формат: "name=value"
                char *eq = strchr(actions[i].arg, '=');
                if (eq != NULL) {
                    *eq = '\0'; // Временно разрываем строку на имя и значение
                    char *name = actions[i].arg;
                    char *value = eq + 1;
                    
                    if (setenv(name, value, 1) == 0) {
                        printf("Переменная окружения установлена: %s=%s\n", name, value);
                    } else {
                        perror("Ошибка setenv");
                    }
                } else {
                    fprintf(stderr, "Ошибка: неверный формат для -V, ожидается 'name=value'\n");
                }
                break;
            }
            case '?':
                // getopt уже вывел сообщение об ошибке в stderr, 
                // здесь мы просто игнорируем дальнейшую обработку этой "опции"
                break;
        }
    }

    return 0;
}
