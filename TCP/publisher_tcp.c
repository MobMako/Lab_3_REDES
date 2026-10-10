#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 2048
#define TOPIC_SIZE 50

int main() {
    int socket_config;
    struct sockaddr_in direccion_server;
    char tema[TOPIC_SIZE];
    char mensaje[BUFFER_SIZE];
    char buffer_envio[BUFFER_SIZE + TOPIC_SIZE + 10];

    socket_config = socket(AF_INET, SOCK_STREAM, 0); //abro socket cli
    if (socket_config < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    memset(&direccion_server, 0, sizeof(direccion_server));
    direccion_server.sin_family = AF_INET;
    direccion_server.sin_port = htons(PORT); 

    if (inet_pton(AF_INET, SERVER_IP, &direccion_server.sin_addr) <= 0) {
        perror("Dirección IP inválida o no soportada");
        close(socket_config);
        exit(EXIT_FAILURE);
    }

    if (connect(socket_config, (struct sockaddr *)&direccion_server, sizeof(direccion_server)) < 0) { //conecto con broker
        perror("Error de conexion fallida con el Broker");
        close(socket_config);
        exit(EXIT_FAILURE);
    }
    printf("[Publicador TCP] Conectado exitosamente al Broker.\n");

    printf("Ingrese el tema de las noticias a publicar: ");
    fgets(tema, sizeof(tema), stdin);
    tema[strcspn(tema, "\n")] = '\0'; 

    while (1) {
        printf("\nIngrese la noticia para '%s' (o 'salir' para terminar): ", tema);
        fgets(mensaje, sizeof(mensaje), stdin);
        mensaje[strcspn(mensaje, "\n")] = '\0';

        if (strcmp(mensaje, "salir") == 0) {
            break;
        }
        
        // agrego \n como delimitador por si llegan pegados 
        snprintf(buffer_envio, sizeof(buffer_envio), "PUB:%s:%s\n", tema, mensaje); 

        ssize_t bytes_enviados = send(socket_config, buffer_envio, strlen(buffer_envio), 0);

        if (bytes_enviados < 0) {
            perror("Error al enviar la noticia");
            break;
        } else {
            printf("[Publicador] Noticia enviada al Broker con éxito.\n");
        }
    }

    close(socket_config);
    printf("[Publicador] Sesión finalizada.\n");
    return 0;
}