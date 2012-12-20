/* Servidor de tiempos (modo concurrente).
 * Sincronización de relojes, siguiendo el algoritmo centralizado de Cristian. */

#include <string.h>

// Ficheros necesarios para el manejo de sockets, direcciones, etc.

#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>

#define TAMBUF 100

// Comandos y respuestas definidos en el protocolo.

char *comandos[] =
{
"OPEN",  // Solicitud de inicio de sesión.
"SYNC",	// Iniciar proceso de sincronización.
"TERM"	// Cierre de sesión.
};

int compara_comandos(char *s1, char *s2);
//Compara dos cadenas de exactamente 4 caracteres, identificando el comando.
int parser_comandos (char *comando);
//Identifica el comando de que se trata.
void limpiar_buffer (char *buffer, int inicio, int fin);
void traer (int s,struct sockaddr_in *info);

/***************************************************************************************************/

int main()
{
	int sock_escucha,sock_dialog;		// sockets de conexión y diálogo.
	struct sockaddr_in dir_serv,dir_cli;
	char buf[TAMBUF];			// buffer para lectura/escritura en el socket.
	int puerto, length;

	length=sizeof(struct sockaddr);

	system("clear");

	write(1,"ServidorCristian> connect",25);

	printf("\n\nPuerto (a partir de %d): ",IPPORT_RESERVED+1);

	scanf("%d",&puerto);

	while (!(puerto > IPPORT_RESERVED))
	{
		printf("\n\nEl puerto %d esta reservado. Intenta otra vez.",puerto);
		printf("\n\nPuerto (a partir de %d): ",IPPORT_RESERVED+1);
		scanf("%d",&puerto);
	}

	// Creamos el socket de escucha ...

	if ((sock_escucha=socket(PF_INET,SOCK_STREAM,0)) < 0)
	{
		perror("\nApertura de socket");
		exit(1);
	}

	dir_serv.sin_family=AF_INET;
	dir_serv.sin_port=htons(puerto);
	dir_serv.sin_addr.s_addr=htonl(INADDR_ANY);	// Cualquiera puede conectarse.

	if (bind(sock_escucha,(struct sockaddr *)&dir_serv,sizeof(dir_serv)) < 0)
	{
		perror("\nBind");
		exit(1);
	}

	// Ponemos el socket a la escucha ...

	if (listen(sock_escucha,7) < 0)
	{
		perror("\nListen");
		exit(1);
	}

	signal(SIGCHLD,SIG_IGN);	// evitar hijos zombies.

	printf("\n\nAPLICACION: Sincronizacion de relojes siguiendo el algoritmo centralizado\n");
	printf("            de Cristian.\n\n");
	printf("Servidor de tiempos, a la espera de peticiones ...\n");

	do
	{
		// Bloqueo a la espera de petición de conexión.

		sock_dialog=accept(sock_escucha,(struct sockaddr *)&dir_cli,(int *)&length);

		if (sock_dialog==-1)
		{
			// Tratamiento especial para interrupciones de hijos muertos.

			if (errno==EINTR) continue;
			perror("\nAccept");
			exit(1);
		}

		else
			switch(fork())
			{
				case -1:

					perror("\nCreando un hijo");
					break;

				case 0:

					close(sock_escucha);
					traer(sock_dialog,&dir_cli);
					close(sock_dialog);
					exit(0);

				default:

					close(sock_dialog);
					break;
			}
	} while (1);

	exit(0);
}

/***************************************************************************************************/

int compara_comandos(char *s1, char *s2)
//Compara dos cadenas de exactamente 4 caracteres, identificando el comando.

{
	int i;
	int result=1;

	for (i=0; i<4; i++)
		if (s1[i] != s2 [i])
			result=0;

	return (result);
}

int parser_comandos (char *comando)
// Identifica el comando de que se trata.

{
	if ((compara_comandos(comando, comandos[0])) == 1) return (0); // OPEN
	if ((compara_comandos(comando, comandos[1])) == 1) return (1); // SYNC
	if ((compara_comandos(comando, comandos[2])) == 1) return (2); // TERM

	return (-1);
}

void limpiar_buffer (char *buffer, int inicio, int fin)
{
	int i;

	for (i=inicio; i<=fin; i++) buffer[i]=' ';
}

void traer (int s,struct sockaddr_in *info)
{
	char *respuesta[] =
	{
	"OKEY",	// Puede llevar acompañado un parámetro indicando la hora (t(mt)) del servidor de de tiempos, o no llevar nada.
	"FINN"	// Puede ir acompañado de un string explicativo de porqué se ha producido la negativa.
	};

	// variables auxiliares.

	int k,lon,lon2;
	char buf[TAMBUF];

	struct timeval horaservidor;
	struct tm *tmp;

	// Espera el envío del comando OPEN.

	k=read(s,buf,TAMBUF);

	if (k<0)
	{
		perror("\nRecepcion Comando OPEN");
		return;
	}

	buf[k]='\0';

	if ((compara_comandos(comandos[0],buf)) == 0)
	{
		perror("\nComando desconocido");
		sprintf(buf,"%s: Comando OPEN esperado",respuesta[1]);
		write(s,buf,strlen(buf));
	}

	// Todo correcto. Devolvemos OKEY y esperamos a que nos manden un comando, que desde este momento
	// puede ser: SYNC o TERM

	sprintf(buf,"%s: Sesion abierta",respuesta[0]);
	write(s,buf,strlen(buf));
	printf("\nIP=%ld       Conectado\n",ntohl(info->sin_addr.s_addr));

	// Entramos en el bucle de recepción de comandos.

	do
	{
		k=read(s,buf,TAMBUF);

		if (k<0)
		{
			perror("\nRecepción de un comando");
			return;
		}

		buf[k]='\0';

		switch(parser_comandos(buf))
		{
			case 1:	// SYNC

				/* calcular hora del servidor */

				gettimeofday(&horaservidor,NULL);

				// el servidor le envia como respuesta:
				//	OK +
				//	segundos 	(el valor comienza en &buf[10]) +
				//	microsegundos 	(el valor comienza en &buf[35])

				sprintf(buf,"%s: seg:%ld\0",respuesta[0],horaservidor.tv_sec);
				lon=strlen(buf);
				limpiar_buffer(buf,lon+1,29);
				sprintf(&buf[30],"useg:%ld\0",horaservidor.tv_usec);
				lon2=strlen(&buf[30]);
				write(s,buf,30+lon2);

				tmp=(struct tm*)localtime(&(horaservidor.tv_sec));
				printf("\nIP=%ld       Sincronizando ...\n",ntohl(info->sin_addr.s_addr));
				printf("                    t(mt): %d hrs. %d mns. %d sgs. %ld usgs.\n",tmp->tm_hour,tmp->tm_min,tmp->tm_sec,horaservidor.tv_usec);
				break;

			case 2:	// TERM

				sprintf(buf,"%s: Cerrando sesion ...",respuesta[0]);
				write(s,buf,strlen(buf));
				close(s);
				printf("\nIP=%ld       Desconectado\n",ntohl(info->sin_addr.s_addr));
				return;

			case 0:	// OPEN

				sprintf(buf,"%s: Error de protocolo en el cliente",respuesta[1]);
				write(s,buf,strlen(buf));
				close(s);
				return;

			default:

				sprintf(buf,"%s: Comando desconocido",respuesta[1]);
				write(s,buf,strlen(buf));
				close(s);
				return;
		}
	} while (1);
}

