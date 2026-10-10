#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_SUBSCRIBERS 100
#define TOPIC_SIZE 50

// Estructura para almacenar cada suscriptor
typedef struct {
  struct sockaddr_in addr;
  char topic[TOPIC_SIZE];
} Subscriber;

Subscriber subscribers[MAX_SUBSCRIBERS];
int sub_count = 0;

// Registrar o actualizar la dirección IP:Puerto de un suscriptor
void registrar_suscriptor(struct sockaddr_in *cliaddr, const char *topic) {
  // Evitar duplicados comprobando IP y puerto
  for (int i = 0; i < sub_count; i++) {
   if (subscribers[i].addr.sin_addr.s_addr == cliaddr->sin_addr.s_addr &&
      subscribers[i].addr.sin_port == cliaddr->sin_port) {
      strncpy(subscribers[i].topic, topic, TOPIC_SIZE - 1);
      printf("[Broker] Suscriptor actualizado en %s:%d para el tema '%s'\n",
         inet_ntoa(cliaddr->sin_addr), ntohs(cliaddr->sin_port), topic);
      return;
    }
  }

  if (sub_count < MAX_SUBSCRIBERS) {
    subscribers[sub_count].addr = *cliaddr;
    strncpy(subscribers[sub_count].topic, topic, TOPIC_SIZE - 1);
    subscribers[sub_count].topic[TOPIC_SIZE - 1] = '\0';
    sub_count++;
    printf("[Broker] Nuevo suscriptor registrado (%s:%d) para el tema '%s'\n",
       inet_ntoa(cliaddr->sin_addr), ntohs(cliaddr->sin_port), topic);
  } else {
    printf("[Broker] Advertencia: Límite de suscriptores alcanzado.\n");
  }
}

// Reenviar mensaje UDP a todos los suscriptores registrados en el tema
void retransmitir_mensaje(int sockfd, const char *topic, const char *mensaje) {
  int enviados = 0;
  socklen_t len = sizeof(struct sockaddr_in);

  for (int i = 0; i < sub_count; i++) {
    if (strcmp(subscribers[i].topic, topic) == 0) {
      sendto(sockfd, mensaje, strlen(mensaje), 0,
         (struct sockaddr *)&subscribers[i].addr, len);
      enviados++;
    }
  }
  printf("[Broker] Noticia retransmitida a %d suscriptor(es) del tema '%s'\n", enviados, topic);
}

int main() {
  int socket_config;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in direccion_server; // IPv4
  struct sockaddr_in direccion_cliente;
  socklen_t len_cliente = sizeof(direccion_cliente);

  // 1. Crear el socket UDP (SOCK_DGRAM)
  socket_config = socket(AF_INET, SOCK_DGRAM, 0); // crea socket udp IPv4
 
  // limpia la memoria que podria estar ocupada en la estructura de direccion del servidor
  memset(&direccion_server, 0, sizeof(direccion_server));
 
  direccion_server.sin_family = AF_INET; // familia de direcciones IPV4
  direccion_server.sin_addr.s_addr = INADDR_ANY; //El servidor va a escuchar todas las peticiones entrantes desde direcciones internas (InAddressAny) 
  direccion_server.sin_port = htons(PORT); // htons es la funcion host to network short que convierte el numero de puerto de int 8080 a formato de red.

  struct sockaddr *direccion_generica = (struct sockaddr *)&direccion_server; // castea de IPv4 a direccion generica sockaddr
  bind(socket_config, direccion_generica, sizeof(direccion_server)); // hace el bind del socket con la configuracion de nuestra direccion de servidor
 
  printf("[Broker UDP] Servidor iniciado y escuchando en el puerto %d...\n", PORT);

 
 
  // 3. Bucle de recepción de datagramas
  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t n = recvfrom(socket_config, buffer, BUFFER_SIZE - 1, 0,
              (struct sockaddr *)&direccion_cliente, &len_cliente);
    if (n < 0) continue;

    buffer[n] = '\0';

    // Procesar orden de suscripción: "SUB:<tema>"
    if (strncmp(buffer, "SUB:", 4) == 0) {
      char *tema = buffer + 4;
      registrar_suscriptor(&direccion_cliente, tema);
    }
    // Procesar orden de publicación: "PUB:<tema>:<mensaje>"
    else if (strncmp(buffer, "PUB:", 4) == 0) {
      char *resto = buffer + 4;
      char *tema = strtok(resto, ":");
      char *contenido = strtok(NULL, "");

      if (tema && contenido) {
        retransmitir_mensaje(socket_config, tema, contenido);
      }
    }
  }

  close(socket_config);
  return 0;
}
