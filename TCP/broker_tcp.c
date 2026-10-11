#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define TOPIC_SIZE 50
#define BUFFER_SIZE 2048
#define MAX_SUBSCRIBERS 100 

typedef struct { //se registra el fd del socket sorrespondiente al sub
  int socket_fd;
  char topic[TOPIC_SIZE];
} Subscriber;

Subscriber subscribers[MAX_SUBSCRIBERS]; //meoria para almacenar los subs
int sub_count = 0;

//registrar o actualizar un suscriptor por su fd
void registrar_suscriptor(int sock_fd, const char *topic) {
  for (int i = 0; i < sub_count; i++) {
    if (subscribers[i].socket_fd == sock_fd) {
      strncpy(subscribers[i].topic, topic, TOPIC_SIZE - 1);
      subscribers[i].topic[TOPIC_SIZE - 1] = '\0';
      printf("[Broker TCP] Suscriptor en socket %d actualizo tema a '%s'\n", sock_fd, topic);
      return;
    }
  }
  if (sub_count < MAX_SUBSCRIBERS) {
    subscribers[sub_count].socket_fd = sock_fd;
    strncpy(subscribers[sub_count].topic, topic, TOPIC_SIZE - 1);
    subscribers[sub_count].topic[TOPIC_SIZE - 1] = '\0';
    sub_count++;
    printf("[Broker TCP] Nuevo suscriptor registrado (socket %d) para el tema '%s'\n", sock_fd, topic);
  } else {
    printf("[Broker TCP] Advertencia: Limite de suscriptores alcanzado.\n");
  }
}

//elim mediente fd
void eliminar_suscriptor(int sock_fd) {
  for (int i = 0; i < sub_count; i++) {
    if (subscribers[i].socket_fd == sock_fd) {
      printf("[Broker TCP] Eliminando suscripcion de socket %d (%s)\n", sock_fd, subscribers[i].topic);
      subscribers[i] = subscribers[sub_count - 1]; 
      sub_count--;
      break;
    }
  }
}

//enviar a todos suscritos
void retransmitir_mensaje(const char *topic, const char *mensaje) {
  int enviados = 0;
  
  // agrego \n al final para que el suscriptor tambien los reciba separados, esto es para evitar mensajes pegados
  char buffer_out[BUFFER_SIZE];
  snprintf(buffer_out, sizeof(buffer_out), "%s\n", mensaje);

  for (int i = 0; i < sub_count; i++) {
    if (strcmp(subscribers[i].topic, topic) == 0) {
      send(subscribers[i].socket_fd, buffer_out, strlen(buffer_out), 0);
      enviados++;
    }
  }
  printf("[Broker TCP] Noticia retransmitida a %d suscriptor(es) del tema '%s'\n", enviados, topic);
}

int main() {
    struct sockaddr_in dirv_client, dirv_server; //server es ip y puerto, client es ip y puerto del cliente
    int sock, confd; //sock es sock de escucha, confd essock el de conexión, 
    socklen_t n; //tam de dirv_client
    char r_buff[BUFFER_SIZE] = "", s_buff[BUFFER_SIZE] = ""; // bufs de receive y de send (respec. cliente, serv)

    int max_fd; //max sockets cli abiertos
    fd_set read_fds, master_fds; //master registra sockets vivos, read es copia de master

    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    //evito error de puerto ocupado si se reinicia rapido
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&dirv_server, 0, sizeof(dirv_server));
    dirv_server.sin_family = AF_INET;
    dirv_server.sin_port = htons(PORT); // htons para compatibilidad como se explica en el informe
    dirv_server.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr *)&dirv_server, sizeof dirv_server); //hago bind con el sock que ya tenía creado, 
    listen(sock, 10); //lo pongo a escuchar

    printf("[Broker TCP] Servidor iniciado y escuchando en el puerto %d...\n", PORT);

    FD_ZERO(&master_fds); //limpieza mapas
    FD_ZERO(&read_fds);
    FD_SET(sock, &master_fds); //abro obit correspondiente a sock en el mapa
    max_fd = sock; //el max es por ahora el unico sock que tengo abierto 

    while (1) {
        read_fds = master_fds; //hago la copia de mater dado que el select mata los que no tienen info rn el map
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) { //select para multiplexación
            perror("[Broker TCP] Error en select");
            break;
        }
        for (int fd = 0; fd <= max_fd; fd++) { //recorro todos sockets
            if (FD_ISSET(fd, &read_fds)) { //reviso si en el mapa read que copié ala pincipio tiene el sock fd abierto
                if (fd == sock) {//significa que el socket con actividad es el sock de escucha (cli hizo connect())
                    n = sizeof(dirv_client);
                    confd = accept(sock, (struct sockaddr *)&dirv_client, &n); //acepta al cliente y guarda su fd
                    if (confd >= 0) {
                        FD_SET(confd, &master_fds); // reg del nuevo socket activo
                        if (confd > max_fd) max_fd = confd; //actualizao el max fd
                        printf("[Broker TCP] Nueva conexion aceptada en socket %d (%s:%d)\n",
                               confd, inet_ntoa(dirv_client.sin_addr), ntohs(dirv_client.sin_port));
                    }
                }
                else { //si no es el socket que abrí entonces es un cliente ya existente
                    memset(r_buff, 0, sizeof(r_buff));
                    ssize_t bytes_leidos = recv(fd, r_buff, sizeof(r_buff) - 1, 0);
                    if (bytes_leidos <= 0) { //borro en caso de desconexión
                        printf("[Broker TCP] Cliente desconectado en socket %d\n", fd);
                        eliminar_suscriptor(fd);
                        close(fd);
                        FD_CLR(fd, &master_fds);
                    } else {
                        r_buff[bytes_leidos] = '\0';
                        
                        char *saveptr_linea;
                        char *mensaje_individual = strtok_r(r_buff, "\n", &saveptr_linea);
                        
                        while (mensaje_individual != NULL) {
                            if (strncmp(mensaje_individual, "SUB:", 4) == 0) { //veo si es sub,
                                char *tema = mensaje_individual + 4; //apunto al tema
                                registrar_suscriptor(fd, tema); // regis del suscriptor en el subscribers[]
                            } 
                            else if (strncmp(mensaje_individual, "PUB:", 4) == 0) { //veo si es pub
                                char *saveptr_pub;
                                char *resto = mensaje_individual + 4;
                                char *tema = strtok_r(resto, ":", &saveptr_pub); //aputnador al tema
                                char *contenido = strtok_r(NULL, "", &saveptr_pub);
                                if (tema && contenido) {
                                    retransmitir_mensaje(tema, contenido); //retransmito mensaje
                                }
                            }
                            mensaje_individual = strtok_r(NULL, "\n", &saveptr_linea);  
                        }
                    }
                }
            }
        }
    }

    close(sock);
    return 0;
}
