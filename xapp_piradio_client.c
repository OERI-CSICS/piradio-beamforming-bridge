#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUFFER_SIZE 2048


/* ================================================================
 * Connect xApp-side C program to MATLAB TCP server
 * ================================================================ */
int connect_to_matlab(const char *ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET,
                  ip,
                  &server_addr.sin_addr) <= 0) {

        fprintf(stderr, "Invalid MATLAB IP address: %s\n", ip);

        close(sockfd);
        return -1;
    }

    printf("Connecting to MATLAB at %s:%d...\n",
           ip,
           port);

    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {

        perror("connect");

        close(sockfd);
        return -1;
    }

    printf("Connected to MATLAB successfully.\n");

    return sockfd;
}


/* ================================================================
 * Send all bytes
 * ================================================================ */
int send_all(int sockfd,
             const char *data,
             size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length) {

        ssize_t sent =
            send(sockfd,
                 data + total_sent,
                 length - total_sent,
                 0);

        if (sent < 0) {

            if (errno == EINTR)
                continue;

            perror("send");
            return -1;
        }

        if (sent == 0) {
            fprintf(stderr,
                    "MATLAB connection closed while sending.\n");

            return -1;
        }

        total_sent += (size_t)sent;
    }

    return 0;
}


/* ================================================================
 * Receive one newline-terminated response from MATLAB
 *
 * MATLAB uses:
 *
 * configureTerminator(server,"LF")
 *
 * so every JSON message ends with '\n'.
 * ================================================================ */
int recv_line(int sockfd,
              char *buffer,
              size_t max_length)
{
    size_t index = 0;

    while (index < max_length - 1) {

        char c;

        ssize_t n =
            recv(sockfd,
                 &c,
                 1,
                 0);

        if (n < 0) {

            if (errno == EINTR)
                continue;

            perror("recv");
            return -1;
        }

        if (n == 0) {

            fprintf(stderr,
                    "MATLAB closed the connection.\n");

            return -1;
        }

        if (c == '\n')
            break;

        buffer[index++] = c;
    }

    buffer[index] = '\0';

    return 0;
}


/* ================================================================
 * Send JSON command and wait for JSON response
 * ================================================================ */
int send_command(int sockfd,
                 const char *command)
{
    char response[BUFFER_SIZE];

    printf("\n----------------------------------------\n");
    printf("Sending to MATLAB:\n");
    printf("%s", command);
    printf("----------------------------------------\n");

    if (send_all(sockfd,
                 command,
                 strlen(command)) < 0) {

        fprintf(stderr,
                "Failed to send command to MATLAB.\n");

        return -1;
    }

    if (recv_line(sockfd,
                  response,
                  sizeof(response)) < 0) {

        fprintf(stderr,
                "Failed to receive MATLAB response.\n");

        return -1;
    }

    printf("MATLAB/Pi-Radio response:\n");
    printf("%s\n", response);

    return 0;
}


/* ================================================================
 * PING
 *
 * Sends:
 *
 * {"cmd":"PING"}
 * ================================================================ */
int send_ping(int sockfd)
{
    const char *command =
        "{\"cmd\":\"PING\"}\n";

    printf("\n=== PING TEST ===\n");

    return send_command(sockfd,
                        command);
}


/* ================================================================
 * SET BEAM
 *
 * Example:
 *
 * beam_deg = 20
 *
 * sends:
 *
 * {"cmd":"SET_BEAM","beam_deg":20.00}
 *
 * MATLAB then executes:
 *
 * sdr1.calc_w(20);
 * sdr1.configure_realtime(0);
 * ================================================================ */
int send_beam_command(int sockfd,
                      double beam_deg)
{
    char command[256];

    snprintf(command,
             sizeof(command),

             "{\"cmd\":\"SET_BEAM\","
             "\"beam_deg\":%.2f}\n",

             beam_deg);

    printf("\n=== SET BEAM ===\n");
    printf("Requested beam angle = %.2f deg\n",
           beam_deg);

    return send_command(sockfd,
                        command);
}


/* ================================================================
 * SET BEAM + NULL
 *
 * Example:
 *
 * beam_deg = 20
 * null_deg = -35
 *
 * sends:
 *
 * {"cmd":"SET_BEAM_NULL",
 *  "beam_deg":20.00,
 *  "null_deg":-35.00}
 *
 * MATLAB then executes:
 *
 * sdr1.calc_w(20,-35);
 * sdr1.configure_realtime(0);
 * ================================================================ */
int send_beam_null_command(int sockfd,
                           double beam_deg,
                           double null_deg)
{
    char command[256];

    snprintf(command,
             sizeof(command),

             "{\"cmd\":\"SET_BEAM_NULL\","
             "\"beam_deg\":%.2f,"
             "\"null_deg\":%.2f}\n",

             beam_deg,
             null_deg);

    printf("\n=== SET BEAM + NULL ===\n");

    printf("Beam angle = %.2f deg\n",
           beam_deg);

    printf("Null angle = %.2f deg\n",
           null_deg);

    return send_command(sockfd,
                        command);
}


/* ================================================================
 * Print usage
 * ================================================================ */
void print_usage(const char *program)
{
    printf("\nUsage:\n\n");

    printf("PING:\n");
    printf("  %s <MATLAB_IP> <PORT> ping\n\n",
           program);

    printf("SET BEAM:\n");
    printf("  %s <MATLAB_IP> <PORT> beam <BEAM_DEG>\n\n",
           program);

    printf("SET BEAM + NULL:\n");
    printf("  %s <MATLAB_IP> <PORT> beam-null "
           "<BEAM_DEG> <NULL_DEG>\n\n",
           program);


    printf("Examples:\n\n");

    printf("  %s 192.168.1.100 5000 ping\n",
           program);

    printf("  %s 192.168.1.100 5000 beam 20\n",
           program);

    printf("  %s 192.168.1.100 5000 "
           "beam-null 20 -35\n\n",
           program);
}


/* ================================================================
 * MAIN
 * ================================================================ */
int main(int argc,
         char *argv[])
{
    if (argc < 4) {

        print_usage(argv[0]);

        return EXIT_FAILURE;
    }


    /*
     * ------------------------------------------------------------
     * MATLAB server information
     * ------------------------------------------------------------
     */

    const char *matlab_ip =
        argv[1];

    int matlab_port =
        atoi(argv[2]);

    const char *operation =
        argv[3];


    /*
     * ------------------------------------------------------------
     * Connect to MATLAB
     * ------------------------------------------------------------
     */

    int matlab_socket =
        connect_to_matlab(matlab_ip,
                          matlab_port);

    if (matlab_socket < 0) {

        fprintf(stderr,
                "Could not connect to MATLAB.\n");

        return EXIT_FAILURE;
    }


    /*
     * ------------------------------------------------------------
     * PING
     * ------------------------------------------------------------
     */

    if (strcmp(operation, "ping") == 0) {

        if (send_ping(matlab_socket) < 0) {

            close(matlab_socket);

            return EXIT_FAILURE;
        }
    }


    /*
     * ------------------------------------------------------------
     * SET BEAM
     *
     * Example:
     *
     * ./xapp_piradio_client 192.168.1.100 5000 beam 20
     * ------------------------------------------------------------
     */

    else if (strcmp(operation, "beam") == 0) {

        if (argc != 5) {

            fprintf(stderr,
                    "beam requires BEAM_DEG\n");

            print_usage(argv[0]);

            close(matlab_socket);

            return EXIT_FAILURE;
        }

        double beam_deg =
            atof(argv[4]);


        /*
         * Optional angle validation.
         *
         * Change these limits if your physical
         * Pi-Radio array supports a different range.
         */

        if (beam_deg < -90.0 ||
            beam_deg > 90.0) {

            fprintf(stderr,
                    "Beam angle must be between "
                    "-90 and +90 degrees.\n");

            close(matlab_socket);

            return EXIT_FAILURE;
        }


        if (send_beam_command(matlab_socket,
                              beam_deg) < 0) {

            close(matlab_socket);

            return EXIT_FAILURE;
        }
    }


    /*
     * ------------------------------------------------------------
     * SET BEAM + NULL
     *
     * Example:
     *
     * ./xapp_piradio_client \
     *     192.168.1.100 \
     *     5000 \
     *     beam-null \
     *     20 \
     *     -35
     *
     * ------------------------------------------------------------
     */

    else if (strcmp(operation,
                    "beam-null") == 0) {

        if (argc != 6) {

            fprintf(stderr,
                    "beam-null requires "
                    "BEAM_DEG and NULL_DEG\n");

            print_usage(argv[0]);

            close(matlab_socket);

            return EXIT_FAILURE;
        }


        double beam_deg =
            atof(argv[4]);

        double null_deg =
            atof(argv[5]);


        /*
         * Angle validation
         */

        if (beam_deg < -90.0 ||
            beam_deg > 90.0) {

            fprintf(stderr,
                    "Beam angle must be between "
                    "-90 and +90 degrees.\n");

            close(matlab_socket);

            return EXIT_FAILURE;
        }


        if (null_deg < -90.0 ||
            null_deg > 90.0) {

            fprintf(stderr,
                    "Null angle must be between "
                    "-90 and +90 degrees.\n");

            close(matlab_socket);

            return EXIT_FAILURE;
        }


        if (send_beam_null_command(matlab_socket,
                                   beam_deg,
                                   null_deg) < 0) {

            close(matlab_socket);

            return EXIT_FAILURE;
        }
    }


    /*
     * ------------------------------------------------------------
     * Unknown command
     * ------------------------------------------------------------
     */

    else {

        fprintf(stderr,
                "Unknown operation: %s\n",
                operation);

        print_usage(argv[0]);

        close(matlab_socket);

        return EXIT_FAILURE;
    }


    /*
     * ------------------------------------------------------------
     * Cleanup
     * ------------------------------------------------------------
     */

    close(matlab_socket);

    printf("\nMATLAB connection closed successfully.\n");

    return EXIT_SUCCESS;
}