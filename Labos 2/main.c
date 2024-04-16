#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <termios.h>

struct sigaction prije;

void obradi_dogadjaj(int sig)
{
	printf("\n[signal SIGINT] proces %d primio signal %d\n", (int) getpid(), sig);
	//proslijedi ga ako se program izvodi u prvom planu
}

void obradi_signal_zavrsio_neki_proces_dijete(int id)
{
	//ako je već dole pozvan waitpid, onda na ovaj signal waitpid ne daje informaciju (ponovo)
	pid_t pid_zavrsio = waitpid(-1, NULL, WNOHANG); //ne čeka
	if (pid_zavrsio > 0)
		if (kill(pid_zavrsio, 0) == -1) //možda je samo promijenio stanje ili je bas završio
			printf("\n[roditelj %d - SIGCHLD + waitpid] dijete %d zavrsilo s radom\n", (int) getpid(), pid_zavrsio);
	//else
		//printf("\n[roditelj %d - SIGCHLD + waitpid] waitpid ne daje informaciju\n", (int) getpid());
}


//primjer stvaranja procesa i u njemu pokretanja programa
pid_t pokreni_program(char *naredba[], int u_pozadini)
{
	pid_t pid_novi;
	if ((pid_novi = fork()) == 0) {
		sigaction(SIGINT, &prije, NULL); //resetiraj signale
		setpgid(pid_novi, pid_novi); //stvori novu grupu za ovaj proces
		if (!u_pozadini)
			tcsetpgrp(STDIN_FILENO, getpgid(pid_novi)); //dodijeli terminal

		execvp(naredba[0], naredba);
		perror("Nisam pokrenuo program!");
		exit(1);
	}

	return pid_novi; //roditelj samo dolazi do tuda
}

int main()
{
	struct sigaction act;
	pid_t pid_novi;

	printf("[roditelj %d] krenuo s radom\n", (int) getpid());

	//postavi signale SIGINT i SIGCHLD
	act.sa_handler = obradi_dogadjaj;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	//sigaction(SIGINT, &act, &prije);
	act.sa_handler = obradi_signal_zavrsio_neki_proces_dijete;
	sigaction(SIGCHLD, &act, NULL);
	act.sa_handler = SIG_IGN;
	sigaction(SIGTTOU, &act, NULL); //zbog tcsetpgrp

	struct termios shell_term_settings;
	tcgetattr(STDIN_FILENO, &shell_term_settings);

	//uzmi natrag kontrolu nad terminalom
	//tcsetpgrp(STDIN_FILENO, getpgid(0));

	size_t vel_buf = 128;
	char buffer[vel_buf];

	do {
		char cwd[1024];
		getcwd(cwd, sizeof(cwd));
		printf("root%s$ ", cwd);

		if (fgets(buffer, vel_buf, stdin) != NULL) {
			#define MAXARGS 5
			char *argv[MAXARGS];
			int argc = 0;
			argv[argc] = strtok(buffer, " \t\n");
			while (argv[argc] != NULL) {
				argc++;
				argv[argc] = strtok(NULL, " \t\n");
			}

			//if argv = cd
			if (strcmp(argv[0], "cd") == 0) {
				if (argc == 1) {
					chdir(getenv("HOME"));
				} else if (argc == 2) {
					chdir(argv[1]);
				}
				else if (argc > 2) {
					char *path = (char *) malloc(vel_buf);
					strcpy(path, argv[1]);
					for (int i = 2; i < argc; i++) {
						strcat(path, " ");
						strcat(path, argv[i]);
					}
					chdir(path);
					free(path);
				}
				continue;
			}

			//if argv = exit
			if (strcmp(argv[0], "exit") == 0) {
				break;
			}

			//if argv = kill
			if (!strcmp(argv[0], "kill") && argc == 3)
			{
				kill(atoi(argv[2]), atoi(argv[1]));
			}

			//if argv = ps
			if (!strcmp(argv[0], "ps") && argc == 1)
			{
				system("ps");
			}

			pid_novi = pokreni_program(argv, 0);

			pid_t pid_zavrsio;
			do {
				pid_zavrsio = waitpid(pid_novi, NULL, 0); //čekaj
				if (pid_zavrsio > 0) {
					if (kill(pid_novi, 0) == -1) { //nema ga više? ili samo mijenja stanje
						printf("[roditelj] dijete %d zavrsilo s radom\n", pid_zavrsio);

						//vraćam terminal ljusci
						tcsetpgrp(STDIN_FILENO, getpgid(0));
						tcsetattr(STDIN_FILENO, 0, &shell_term_settings);
					}
					else {
						pid_novi = (pid_t) 0; //nije gotov
					}
				}
				else {
					printf("[roditelj] waitpid gotov ali ne daje informaciju\n");
					break;
				}
			}
			while(pid_zavrsio <= 0);
		}
		else {
			//printf("[roditelj] neka greska pri unosu, vjerojatno dobio signal\n");
		}
	}
	while(strncmp(buffer, "exit", 4) != 0);

	return 0;
}