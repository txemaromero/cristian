//Cliente para sincronización de relojes mediante el algoritmo de Cristian, usando directamente la interfaz de sockets.

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>  			//para usar gethostbyname
#include <stdio.h>
#include <sys/timeb.h>
#include <time.h>

#define TAMBUF 100

char nom_serv[80];				//nombre de la máquina servidora
char *comandos[] = {"OPEN","SYNC","TERM"};	//comandos de comunicación con el servidor
char *respuestas[] = {"OKEY","FINN"};		//respuestas del servidor

struct timeval Dmin;

int compara_respuestas(char *s1,char *s2);
//Compara dos cadenas de exactamente 4 caracteres
int parser_respuestas (char *respuesta);
//Identifica el comando de que se trata
int calcular_desviacion_absoluta (long *sgs, long *usgs);
//Devuelve 1 sii el reloj local estuviese adelantado.
//        -1 sii el reloj local estuviese retrasado.
//         0 sii el reloj local estuviese ajustado.
int menorqueDmin(struct timeval *nuevaD);
//Devuelve 1 sii el tiempo D que se le pasa como argumento es menor que el que se tenía como mínimo hasta el momento.
//         0 e. o. c.
void sincronizar(int s);
void acabar(int s);

int main()
{
	int sock, k, puerto, i;
  	struct sockaddr_in server;
  	struct hostent *hp, *gethostbyname();

  	char buf[TAMBUF];
	char opcion[50];

	system("clear");

	printf("APLICACION: Sincronizacion de relojes siguiendo el algoritmo centralizado\n");
	printf("            de Cristian.\n\n");

 	 //pedir nombre del servidor.

  	printf("Introduce el nombre del servidor de la red local\n         (Ejemplo: acpt00.gi.ehu.es / localhost): ");
	scanf("%s", nom_serv);

	printf("\n\nPuerto (a partir de %d): ",IPPORT_RESERVED+1);
	scanf("%d", &puerto);

	while (puerto <= IPPORT_RESERVED)
	{
		printf("\n\nEl puerto %d esta reservado. Intenta otra vez.\n",puerto);
		printf("\nPuerto (a partir de %d): ",IPPORT_RESERVED+1);
		scanf("%d", &puerto);
	}

  	//Abrimos el socket para establecer la conexión.

  	sock = socket(PF_INET, SOCK_STREAM, 0);
  	if (sock <0)
	{
		perror("No se puede crear el socket");
	  	exit(1);
  	}

  	server.sin_family = AF_INET;
  	server.sin_port = htons(puerto);

  	//Extraer del DNS la información del servidor.

  	hp = gethostbyname(nom_serv);
  	if (hp == NULL)
  	{
		perror("gethostbyname");
	  	close(sock);
	  	exit(1);
  	}

  	memcpy(&server.sin_addr, hp->h_addr, hp->h_length);

  	if (connect(sock,(struct sockaddr*)&server, sizeof server) < 0)
	{
		perror("connect");
	  	close(sock);
	  	exit(1);
  	}

  	//Empieza el protocolo de aplicación con el envío de comando OPEN.

  	printf("\n\nConectando al puerto %d de la maquina, %s ...\n",puerto,nom_serv);
  	sprintf(buf,"%s", comandos[0]);
  	write(sock, buf, strlen(buf));

  	//si estamos aquí, deberemos recibir el OKEY de confirmación.

  	if ((k == read(sock, buf, TAMBUF)) < 0)
  	{
		perror("read");
	  	close(sock);
	  	exit(1);
  	}

  	switch(parser_respuestas(buf))
  	{
  		case 0:	//correcto inicio de sesión
	  		printf("\nInicio de sesion establecido ...\n");
	  		break;

  		case 1:	//inicio de sesión fallido.
	  		printf("\nInicio de sesion fallido ...\n");
	  		close(sock);
	         	exit(1);

  		default://no sabemos qué ha pasado ...
	  		printf("\nError de protocolo ... Abandono.\n");
	  		close(sock);
	  		exit(1);
  	}

	//Inicializar Dmin con un valor lo suficientemente grande.

	Dmin.tv_sec=1000;	//por ejemplo. (1000 sgs.)
	Dmin.tv_usec=1000000;

  	while(1)
  	{
		printf("\n--------------------------------------");
		printf("\n             Elija la opcion que desee:");
	  	printf("\n              1.- Sincronizar el reloj");
	  	printf("\n             2.- Salir");
		printf("\n--------------------------------------\n\n");
	  	scanf("%s",opcion);

	  	switch(opcion[0])
	     	{
	       		case '1':	sincronizar(sock); break;
	       		case '2':	acabar(sock); break;
               		default:	printf("\nOpcion desconocida. Vuelva a intentarlo.\n"); break;
	     	}
  	}
}

int compara_respuestas(char *s1,char *s2)
//Compara dos cadenas de exactamente 4 caracteres
{
	int i;
	int result = 1;
	for (i=0; i<4; i++)
		if(s1[i] != s2[i]) result = 0;
	return(result);
}

int parser_respuestas (char *respuesta)
//Identifica el comando de que se trata
{
	if ((compara_respuestas(respuesta,respuestas[0])) == 1) return(0); //OKEY
	if ((compara_respuestas(respuesta,respuestas[1])) == 1) return(1); //FINN
	return(-1);
}

int calcular_desviacion_absoluta (long *sgs, long *usgs)
//Devuelve 1 sii el reloj local estuviese adelantado.
//        -1 sii el reloj local estuviese retrasado.
//         0 sii el reloj local estuviese ajustado.
{
	if (*sgs > 0)
	{
		printf("adelantado.");
		if (*usgs < 0)
		{
			*usgs=(*usgs) + 1000000;
			*sgs--;
		}
		return(1);
	}
	else if (*sgs == 0)
	{
		if (*usgs > 0)
		{
			printf("adelantado.");
			return(1);
		}
		else if (*usgs == 0)
		{
			printf("ajustado.");
			return(0);
		}
		else
		{
			printf("retrasado.");
			*usgs=(*usgs) * (-1);
			return(-1);
		}
	}
	else
	{
		*sgs=(*sgs) * (-1);
		printf("retrasado.");
		if (*usgs > 0)
		{
			*usgs=(*usgs) * (-1) + 1000000;
			*sgs=(*sgs) - 1;
		}
		else if (*usgs < 0) *usgs=(*usgs) * (-1);
		return(-1);
	}
}

int menorqueDmin(struct timeval *nuevaD)
//Devuelve 1 sii el tiempo D que se le pasa como argumento es menor que el que se tenía como mínimo hasta el momento.
//         0 e. o. c.
{
	if (nuevaD->tv_sec < Dmin.tv_sec) return(1);
	else if (nuevaD->tv_sec > Dmin.tv_sec) return(0);
	else
	{
		if (nuevaD->tv_usec < Dmin.tv_usec) return(1);
		else return(0);
	}
}

void sincronizar(int s)
{
  	char buf[TAMBUF];
	int k, adelantado;

	struct timeval t0, t1, D, desviacion, desviacionabsoluta, tmt, t, ajuste;
	struct tm *tmp, *tmp2, *tmp3;

	system("clear");

	//Anotamos la hora actual justo antes de la petición de la hora al servidor.

	gettimeofday(&t0,NULL);

	//Enviamos el comando SYNC para empezar la sincronización.

	sprintf(buf, "%s", comandos[1]);
	if (write(s, buf, strlen(buf)) < strlen(buf))
	{
		perror("Error al enviar el comando SYNC. Abandono..");
		close(s);
		exit(0);
	}

	//Recibimos la hora proporcionada por el servidor.

	if ((k = read(s, buf, TAMBUF)) <0)
	{
		perror("Leyendo la hora recibida por el servidor.");
		exit(1);
	}

	//Anotamos la hora actual, tras haber recibido la hora del servidor.

	gettimeofday(&t1,NULL);

	//Calculo de D. D=t1-t0

	D.tv_sec=t1.tv_sec-t0.tv_sec;
	D.tv_usec=t1.tv_usec-t0.tv_usec;

	//He restado por un lado segundos y por otro lado microsegundos. La diferencia (t1-t0) puede no ser real.
	//t1 > t0

	if (D.tv_usec < 0)
	{
		D.tv_usec=D.tv_usec + 1000000;
		D.tv_sec--;
	}

	printf("Intento. D igual a, %ld sgs. y %ld usgs.\n",D.tv_sec,D.tv_usec);

	//Manipular la hora del servidor recibida.

	tmt.tv_sec=atol(&buf[10]);			//segundos y microsegundos de t(mt).
	tmt.tv_usec=atol(&buf[35]);
	tmp=(struct tm*)localtime(&(tmt.tv_sec));

	t.tv_sec=t1.tv_sec-(D.tv_sec/2);
	t.tv_usec=t1.tv_usec-(D.tv_usec/2);

	//He restado por un lado segundos y por otro lado microsegundos. La diferencia puede no ser real.

	if (t.tv_usec < 0)
	{
		t.tv_usec=t.tv_usec + 1000000;
		t.tv_sec--;
	}

	tmp2=(struct tm*)localtime(&(t.tv_sec));

	printf("\n               t: %d hrs. %d mns. %d sgs. %ld usgs.",tmp2->tm_hour,tmp2->tm_min,tmp2->tm_sec,t.tv_usec);
	printf("\n           t(mt): %d hrs. %d mns. %d sgs. %ld usgs. (Servidor de tiempos)\n",tmp->tm_hour,tmp->tm_min,tmp->tm_sec,tmt.tv_usec);

	//Calculamos la diferencia de hora entre nuestro reloj y el del servidor, y lo ajustamos.

	//La desviación actual del reloj local vendrá dada por: desviacion=t-t(mt)

	printf("\nLuego, tendrias el reloj ");

	desviacionabsoluta.tv_sec=t.tv_sec-tmt.tv_sec;
	desviacionabsoluta.tv_usec=t.tv_usec-tmt.tv_usec;

	//He restado por un lado segundos y por otro lado microsegundos.

	adelantado=calcular_desviacion_absoluta(&desviacionabsoluta.tv_sec,&desviacionabsoluta.tv_usec);

	//adelantado toma el valor:  1 sii el reloj local está adelantado.
	//                          -1 sii el reloj local está retrasado.
	//                           0 sii el reloj local está ajustado.


	tmp3=(struct tm*)localtime(&(desviacionabsoluta.tv_sec));
	printf("\nDesviacion |t- t(mt)|: %d hrs. %d mns. %d sgs. %ld usgs.\n\n",tmp3->tm_hour-1,tmp3->tm_min,tmp3->tm_sec,desviacionabsoluta.tv_usec);

	printf("El valor de D obtenido en este intento,\n");

	if (menorqueDmin(&D) == 0)
	{
		printf("NO es MENOR que el minimo obtenido hasta el momento.\n");
		printf("Minimo D: %ld sgs. y %ld usgs.\n\n\nSe opta por NO ajustar. No hay garantias de una mejor precision.\n\n",Dmin.tv_sec,Dmin.tv_usec);
	}
	else
	{
		printf("SI es MENOR que el minimo obtenido hasta el momento.\n");
		Dmin.tv_sec=D.tv_sec;
		Dmin.tv_usec=D.tv_usec;
		printf("Nuevo minimo D: %ld sgs. y %ld usgs.\n\n\n",Dmin.tv_sec,Dmin.tv_usec);

		gettimeofday(&ajuste,NULL);

		if (adelantado==1) 		//restar diferencia
		{
			ajuste.tv_usec=ajuste.tv_usec-desviacionabsoluta.tv_usec;

			if (ajuste.tv_usec < 0)
			{
				ajuste.tv_usec=ajuste.tv_usec + 1000000;
				ajuste.tv_sec--;
			}
			ajuste.tv_sec=ajuste.tv_sec-desviacionabsoluta.tv_sec;

			if ((settimeofday(&ajuste,NULL)) <0) perror("Ajustando reloj ...");
			else
			{
				tmp3=(struct tm*)localtime(&(ajuste.tv_sec));
				printf("               Sincronizado reloj: %d hrs. %d mns. %d sgs. %ld usgs.\n\n",tmp3->tm_hour,tmp3->tm_min,tmp3->tm_sec,ajuste.tv_usec);
			}
		}
		else if (adelantado==-1)	//sumar diferencia
		{
			ajuste.tv_usec=ajuste.tv_usec+desviacionabsoluta.tv_usec;

			if (ajuste.tv_usec >= 1000000)
			{
				ajuste.tv_usec=ajuste.tv_usec - 1000000;
				ajuste.tv_sec++;
			}
			ajuste.tv_sec=ajuste.tv_sec+desviacionabsoluta.tv_sec;

			if ((settimeofday(&ajuste,NULL)) <0) perror("Ajustando reloj ...");
			else
			{
				tmp3=(struct tm*)localtime(&(ajuste.tv_sec));
				printf("               Sincronizado reloj: %d hrs. %d mns. %d sgs. %ld usgs.\n\n",tmp3->tm_hour,tmp3->tm_min,tmp3->tm_sec,ajuste.tv_usec);
			}
		}
		else				//se deja tal cual
		{
			tmp3=(struct tm*)localtime(&(ajuste.tv_sec));
			printf("               Sincronizado reloj: %d hrs. %d mns. %d sgs. %ld usgs.\n\n",tmp3->tm_hour,tmp3->tm_min,tmp3->tm_sec,ajuste.tv_usec);
		}
	}
}

void acabar(int s)
{
	char buf[TAMBUF];
	int k;

	//Enviamos el comando TERM

	sprintf(buf,"%s",comandos[2]);

	if (write(s, buf, strlen(buf)) < strlen(buf))
	{
		perror("Error al enviar el comando TERM, Abandono..");
		close(s);
		exit(0);
	}

	//Esperamos la recepción de la cadena OKEY.

	if ((k = read(s, buf, 4)) < 0)
	{
		perror("Leyendo respuesta al comando TERM");
		exit(1);
	}

	//Verificamos que hemos leído un OKEY.

	switch(parser_respuestas(buf))
	{
		case 0:	//Correcto, cerramos la conexión.
			close(s);
			exit(0);
		case 1:	//Algo anda mal.
			printf("Rechazado el comando TERM\n");
			close(s);
			return;
		default://No sabemos qué ha pasado ...
			close(s);
			exit(1);
	}
}
