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
    struct sockaddr_in direccion_origen;
    socklen_t len_origen = sizeof(direccion_origen);
    char tema[TOPIC_SIZE];
    char buffer_envio[BUFFER_SIZE];
    char buffer_recepcion[BUFFER_SIZE];

    // Crear el socket UDP (SOCK_DGRAM)
    socket_config = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_config < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la direccion IP y puerto del Broker
    memset(&direccion_server, 0, sizeof(direccion_server));
    direccion_server.sin_family = AF_INET;
    direccion_server.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &direccion_server.sin_addr) <= 0) {
        perror("Dirección IP inválida o no soportada");
        close(socket_config);
        exit(EXIT_FAILURE);
    }

    printf("=== Suscriptor UDP ===\n");
    printf("Ingrese el tema al que desea suscribirse (ej. futbol): ");
    fgets(tema, sizeof(tema), stdin);
    tema[strcspn(tema, "\n")] = '\0';

    // Construir el mensaje de suscripción: SUB:<tema>
    snprintf(buffer_envio, sizeof(buffer_envio), "SUB:%s", tema);

    // Enviar mensaje de suscripcion al Broker para registrar la direccion del suscriptor
    ssize_t bytes_enviados = sendto(socket_config, buffer_envio, strlen(buffer_envio), 0,
                                    (struct sockaddr *)&direccion_server, sizeof(direccion_server));

    if (bytes_enviados < 0) {
        perror("Error al enviar solicitud de suscripción");
        close(socket_config);
        exit(EXIT_FAILURE);
    }

    printf("[Suscriptor] Registrado en el Broker para el tema '%s'. Esperando noticias...\n\n", tema);

    while (1) {
        memset(buffer_recepcion, 0, BUFFER_SIZE);

        // recvfrom se bloquea hasta recibir un paquete UDP retransmitido por el Broker
        ssize_t bytes_recibidos = recvfrom(socket_config, buffer_recepcion, BUFFER_SIZE - 1, 0,
                                           (struct sockaddr *)&direccion_origen, &len_origen);

        if (bytes_recibidos < 0) {
            perror("Error al recibir mensaje");
            continue;
        }

        buffer_recepcion[bytes_recibidos] = '\0';
        printf("[Noticia Recibida]: %s\n", buffer_recepcion);
    }

    close(socket_config);
    return 0;
}