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
    char buffer_envio[BUFFER_SIZE];
    char buffer_recepcion[BUFFER_SIZE];

    socket_config = socket(AF_INET, SOCK_STREAM, 0); //sock cli, stream porque tcp
    if (socket_config < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    memset(&direccion_server, 0, sizeof(direccion_server)); //config de ip y puerto del broker tcp
    direccion_server.sin_family = AF_INET;
    direccion_server.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &direccion_server.sin_addr) <= 0) { //formateo ip
        perror("Dirección IP inválida o no soportada");
        close(socket_config);
        exit(EXIT_FAILURE);
    }

    if (connect(socket_config, (struct sockaddr *)&direccion_server, sizeof(direccion_server)) < 0) { //conexión a broker
        perror("Error de conexión fallida con el Broker");
        close(socket_config);
        exit(EXIT_FAILURE);
    }
    printf("[Suscriptor TCP] Conectado exitosamente al Broker.\n");

    printf("Ingrese el tema al que desea suscribirse: ");
    fgets(tema, sizeof(tema), stdin);
    tema[strcspn(tema, "\n")] = '\0';

    snprintf(buffer_envio, sizeof(buffer_envio), "SUB:%s\n", tema);

    ssize_t bytes_enviados = send(socket_config, buffer_envio, strlen(buffer_envio), 0); //envío a broekr

    if (bytes_enviados < 0) {
        perror("Error al enviar solicitud de suscripción");
        close(socket_config);
        exit(EXIT_FAILURE);
    }

    printf("[Suscriptor] Registrado en el Broker para el tema '%s'. Esperando noticias...\n\n", tema);

    while (1) {
        memset(buffer_recepcion, 0, BUFFER_SIZE);
        ssize_t bytes_recibidos = recv(socket_config, buffer_recepcion, BUFFER_SIZE - 1, 0); //receoción dek mensaje como suscriptor

        if (bytes_recibidos < 0) {
            perror("Error al recibir mensaje");
            break;
        } else if (bytes_recibidos == 0) {
            //tcp avisa cuando el servidor (broker) se desconecta o se cierra
            printf("\n[Alerta] El Broker ha cerrado la conexión.\n");
            break;
        }

        buffer_recepcion[bytes_recibidos] = '\0';
        printf("[Noticia Recibida]: %s\n", buffer_recepcion);
    }

    close(socket_config);
    printf("[Suscriptor] Sesión finalizada.\n");
    return 0;
}
