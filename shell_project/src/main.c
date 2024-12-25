---------------------------------------------------------------------------------------------
6. COMMİT ARKAPLANDA ÇALIŞMA 
-------------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CMD_LEN 1024

// Arka planda çalışan süreçlerin PID'lerini tutacak bir dizi
pid_t background_pids[MAX_CMD_LEN];
int bg_pid_count = 0;

void execute_command(char *command) {
    char *args[MAX_CMD_LEN];
    char *input_file = NULL;
    char *output_file = NULL;
    int arg_count = 0;
    int is_background = 0; // Arka planda çalışıp çalışmadığını belirler

    // Komutu parçala
    char *token = strtok(command, " ");
    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            // Giriş yönlendirme işareti buldu
            token = strtok(NULL, " ");
            if (token == NULL) {
                fprintf(stderr, "Hata: '<' işaretinden sonra dosya adı gelmelidir.\n");
                return;
            }
            input_file = token;
        } else if (strcmp(token, ">") == 0) {
            // Çıkış yönlendirme işareti buldu
            token = strtok(NULL, " ");
            if (token == NULL) {
                fprintf(stderr, "Hata: '>' işaretinden sonra dosya adı gelmelidir.\n");
                return;
            }
            output_file = token;
        } else if (strcmp(token, "&") == 0) {
            // Arka planda çalıştırma işareti
            is_background = 1;
        } else {
            // Normal argüman
            args[arg_count++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL; // Argüman sonlandırıcı

    // Giriş dosyası varsa işleme devam et
    if (input_file) {
        int fd = open(input_file, O_RDONLY);
        if (fd < 0) {
            perror("Giriş dosyası bulunamadı");
            return;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Çocuk süreç: Dosyayı standart girişe yönlendir
            dup2(fd, STDIN_FILENO);
            close(fd);
            execvp(args[0], args); // Komutu çalıştır
            perror("Komut çalıştırılamadı");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Ebeveyn süreç: Çocuk sürecin bitmesini bekle
            close(fd);
            int status;
            waitpid(pid, &status, 0);
        } else {
            perror("Fork başarısız");
        }
    } else if (output_file) {
        // Çıkış dosyası varsa işleme devam et
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("Çıkış dosyası açılamadı");
            return;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Çocuk süreç: Dosyayı standart çıkışa yönlendir
            dup2(fd, STDOUT_FILENO);
            close(fd);
            execvp(args[0], args); // Komutu çalıştır
            perror("Komut çalıştırılamadı");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Ebeveyn süreç: Çocuk sürecin bitmesini bekle
            close(fd);
            int status;
            waitpid(pid, &status, 0);
        } else {
            perror("Fork başarısız");
        }
    } else if (is_background) {
        // Arka planda çalıştırma
        pid_t pid = fork();
        if (pid == 0) {
            // Çocuk süreç: Arka planda komutu çalıştır
            execvp(args[0], args);
            perror("Komut çalıştırılamadı");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Ebeveyn süreç: PID'yi kaydet
            background_pids[bg_pid_count++] = pid;
            printf("Arka planda çalışan proses ID: %d\n", pid);
        } else {
            perror("Fork başarısız");
        }
    } else {
        // Normal çalıştırma (giriş veya çıkış yönlendirme yoksa)
        pid_t pid = fork();
        if (pid == 0) {
            execvp(args[0], args);
            perror("Komut çalıştırılamadı");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
        } else {
            perror("Fork başarısız");
        }
    }
}

// Arka planda çalışan işlemlerin durumu kontrol edilir
void check_background_processes() {
    int status;
    for (int i = 0; i < bg_pid_count; i++) {
        pid_t pid = background_pids[i];
        pid_t result = waitpid(pid, &status, WNOHANG); // Durum sorgulaması
        if (result == pid) {
            if (WIFEXITED(status)) {
                printf("[%d] retval: %d\n", pid, WEXITSTATUS(status));
            }
        }
    }
}

int main() {
    char command[MAX_CMD_LEN];

    while (1) {
        printf("> ");
        fflush(stdout);

        // Kullanıcıdan komut al
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break; // EOF
        }

        // Yeni satır karakterini kaldır
        command[strcspn(command, "\n")] = '\0';

        // quit komutuyla çıkış
        if (strcmp(command, "quit") == 0) {
            // quit komutuyla çıkış yapmadan önce arka planda çalışan tüm işlemleri kontrol et
            while (bg_pid_count > 0) {
                check_background_processes();
            }
            break;
        }

        // Komutu çalıştır
        execute_command(command);

        // Arka planda çalışan süreçleri kontrol et
        check_background_processes();
    }

    return 0;
}



