/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

// -----------------------------------------------------------------------
//                            MAIN
// -----------------------------------------------------------------------

job * ListBackground; // Creamos la lista para guardar los procesos en background.

void signal_handler(){
	int pid_wait,status,info;
	enum status status_res;

	job * aux = new_list("AuxList"); // Lista Auxiliar
	aux = ListBackground;

	block_SIGCHLD();
	/* Bloqueamos la signal SIGCHILD cuando accedemos o modificamos estructuras de datos
	para evitar riesgos de acceso a datos en estados no válido */

	/*
	Al activarse el manejador de la señal SIGCHLD habrá que revisar cada entrada de la lista para
	comprobar si algún proceso en segundo plano ha terminado o se ha suspendido.
	*/

	while(aux!=NULL){	// Buscamos en la lista de tareas.

	 // Procedemos a comprobar  si algun proceso ha cambiado de estado (waitpid), añadiendo la opcion WNOHANG para no bloquear la shell.
		pid_wait = waitpid(aux->pgid,&status,WUNTRACED | WNOHANG);

		// Actualizamos la lista de tareas.
		if(pid_wait == aux->pgid){  // Nos aseguramos que el proceso tiene el mismo pid que el que estamos tratando.
			status_res = analyze_status(status,&info); // Obtenemos el status e info.
			if(status_res == EXITED || status_res == SIGNALED){ // Si es el status es uno de esos 2 eliminamos la terea.
				delete_job(ListBackground,aux);
				free_job(aux);
			} else {	// En otro caso, SUSPENDED, la ponemos a Stopped.
				aux->state = STOPPED;
			}
		}
			aux = aux->next;	// Actualizamos el puntero para seguir iterando correctamente.
	}


	unblock_SIGCHLD(); // Desbloqueamos la signal SIGCHILD.
}

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */

	 ListBackground = new_list("Background List");


	signal(SIGCHLD,signal_handler);
	// Llamamos a nuestro manejador de signals, para tratar SIGCHILD, cuando un hijo cambie de estado.

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{
		ignore_terminal_signals();		// El Shell debe ignorar las signals.
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */

		if(args[0]==NULL) continue;   // if empty command

		else if(strcmp(args[0],"cd")==0){  // Implementacion del comando interno cd, para cambiar el directorio de trabajo.
			if(chdir(args[1]) == -1){
				printf("Error, not such directory found\n");
			}
			continue;			// Obliga a la siguiente iteración del bucle a tener lugar, saltando el código intermedio.
		}
		else if(strcmp(args[0],"jobs")==0){  // Implementacion del comando interno jobs, para comprobar el estado de la lista.
			if(empty_list(ListBackground)){
				printf("Empty List\n");
			} else {
				block_SIGCHLD();
				print_job_list(ListBackground);
				unblock_SIGCHLD();
			}
			continue;			// Obliga a la siguiente iteración del bucle a tener lugar, saltando el código intermedio.
		}

		else if(strcmp(args[0],"fg")==0){ // Implementacion del comando interno fg.
			int pos = 1;	// En caso por defecto la posicion sera 1.
			int wpid;
			enum status status_a;

			if(args[1] != NULL){
				pos = atoi(args[1]); // Usamos la funcion atoi para pasar de el string del argumento a int y conseguir la posicion.
			}

			if(pos<=0 || pos>list_size(ListBackground)){  // Hacemos la comprobacion del rango de la lista.
				printf("Posición erronea de la lista\n");
				continue;
			}

			int pid;
			job * ListAux;

			// Recuperamos el elemento de la lista mediante su posicion.
			ListAux = get_item_bypos(ListBackground,pos);
			pid = ListAux->pgid;  // Cogemos el pid.
		
			set_terminal(pid); // Devolvemos el terminal al proceso que recuperamos.
			
			if(ListAux->state == STOPPED){ // Manejo de trabajos suspendidos.
				killpg(pid,SIGCONT); // Le mandamos la señal para continuar.
			}
			
			ListAux->state = FOREGROUND; // Actualizamos el estado del proceso.
			 
			
			wpid = waitpid(pid,&status,WUNTRACED); // Vuelve si el hijo ha parado y dime que ha pasado.
			status_a = analyze_status(status,&info); // Obtenemos la informacion.
			
			// Avisamos al usuario mediante una actualizacion en pantalla.
			printf("Foreground PID: %d, Status: %s,info: %d\n", wpid, status_strings[status_a], info );
			
			set_terminal(getpid()); // Devolvemos el terminal al shell.
			
			if(status_a == SUSPENDED){ // Si se suspendio el proceso lo ponemos a stopped.
			ListAux->state = STOPPED;
			} else {						// Si no es así, lo podemos borrar de la lista.
				delete_job(ListBackground,ListAux);	// Eliminamos el nodo de la lista.
				free_job(ListAux);					//Liberamos memoria.
			}
			
			continue;
		} 
		else if(strcmp(args[0],"bg")==0){ // Implementacion interna del comando bg.
			int pos = 1;	// Posicion por defecto en la lista.
			
			if(args[1] != NULL){
				pos = atoi(args[1]);  // Pasamos el string a Integer.
			}

			if(pos<=0 || pos>list_size(ListBackground)){ // Comprobamos los rango de la lista.
				printf("Posición erronea de la lista\n");
				continue;
			}

			int pid;
			job * ListAux;

			// Recuperamos el elemento de la lista mediante su posicion.
			ListAux = get_item_bypos(ListBackground,pos);
			pid = ListAux->pgid; 	// Cogemos su PID
			
			// Actualizamos su estado.
			ListAux->state = BACKGROUND;
			killpg(pid,SIGCONT);
			
			
			continue;
		}

		else {
			 pid_fork = fork();
			 if(pid_fork == 0){				   // -> Entra el hijo
			 pid_fork = getpid();			  // Obtenemos su PID.
				restore_terminal_signals();   // Restauramos el comportamiento por defecto de las signals tras el fork.
				new_process_group(pid_fork); // Crea un nuevo grupo de procesos para el proceso creado.
				if(background == 0){		 // Si esta en foreground le cedemos el terminal.
				set_terminal(pid_fork);
				}
				execvp(args[0],args); 		// Reemplazamos la imagen del proceso padre, por la del hijo.
				printf("Error, command not found: %s \n",args[0]); // En caso de que llegue aquí es que hubo un error en el execvp
				exit (-1);					// Devolvemos el error.
				} else {					// -> Entra el Padre.
					if(background == 0){	// -> Foreground
					set_terminal(pid_fork);	// Le asignamos el terminal a la tarea en fg.
					pid_wait = waitpid(pid_fork,&status,WUNTRACED);		// Esperamos su finalizacion
					set_terminal(getpid()); 		// Devolvemos el control del terminal.
					status_res = analyze_status(status,&info); 			// Recogemos el estado de finalización.
					// Mostramos Actualizacion por pantalla al usuario.
					printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_wait,args[0], status_strings[status_res], info );
					if(status_res == SUSPENDED){	// Actualizamos su estado para guardarlo en la lista.
						block_SIGCHLD();
						add_job(ListBackground,new_job(pid_fork,args[0],STOPPED));
						unblock_SIGCHLD();
					}

				} else {							// -> Background.
					printf("Background job running... pid: %d, command: %s\n",pid_fork,args[0]);
					block_SIGCHLD();
					add_job(ListBackground,new_job(pid_fork,args[0],BACKGROUND));		// Guardamos el item en la lista.
					unblock_SIGCHLD();
				}
			}
		}
	} // end while
}
