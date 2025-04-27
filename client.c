/**
 * File Transfer Client for Manufacturing Company
 * 
 * This client connects to the file transfer server and allows users to transfer
 * files to their department folders.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <arpa/inet.h>
 #include <sys/socket.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <netinet/in.h>
 #include <errno.h>
 
 #define SERVER_IP "127.0.0.1"
 #define PORT 8080
 #define BUFFER_SIZE 1024
 #define MAX_USERNAME_LENGTH 32
 #define MAX_PASSWORD_LENGTH 32
 #define MAX_FILEPATH_LENGTH 256
 #define MAX_DEPT_LENGTH 32
 
 // Function prototypes
 int authenticate(int sock);
 int transfer_file(int sock, const char *filepath, const char *department);
 
 int main() {
     int sock = 0;
     struct sockaddr_in serv_addr;
     char filepath[MAX_FILEPATH_LENGTH];
     char department[MAX_DEPT_LENGTH];
     int choice;
     
     // Create socket
     if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
         printf("Socket creation error\n");
         return -1;
     }
     
     // Set up server address
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_port = htons(PORT);
     
     // Convert IPv4 address from text to binary form
     if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
         printf("Invalid address or address not supported\n");
         return -1;
     }
     
     // Connect to server
     printf("Connecting to server at %s:%d...\n", SERVER_IP, PORT);
     if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
         printf("Connection failed: %s\n", strerror(errno));
         return -1;
     }
     
     printf("Connected to server.\n");
     
     // Authenticate user
     if (authenticate(sock) != 0) {
         printf("Authentication failed.\n");
         close(sock);
         return -1;
     }
     
     // Get file path from user
     printf("Enter the file path to transfer: ");
     fgets(filepath, sizeof(filepath), stdin);
     filepath[strcspn(filepath, "\n")] = 0; // Remove newline
     
     // Choose department
     do {
         printf("\nSelect destination department:\n");
         printf("1. Manufacturing\n");
         printf("2. Distribution\n");
         printf("Choice: ");
         scanf("%d", &choice);
         getchar(); // Consume newline
         
         if (choice == 1) {
             strcpy(department, "Manufacturing");
             break;
         } else if (choice == 2) {
             strcpy(department, "Distribution");
             break;
         } else {
             printf("Invalid choice. Please try again.\n");
         }
     } while (1);
     
     // Transfer file
     if (transfer_file(sock, filepath, department) != 0) {
         printf("File transfer failed.\n");
     } else {
         printf("File transfer completed successfully.\n");
     }
     
     // Close socket
     close(sock);
     return 0;
 }
 
 /**
  * Authenticate with the server
  */
 int authenticate(int sock) {
     char username[MAX_USERNAME_LENGTH];
     char password[MAX_PASSWORD_LENGTH];
     char response[BUFFER_SIZE];
     int bytes_received;
     
     // Get username
     printf("Username: ");
     fgets(username, sizeof(username), stdin);
     username[strcspn(username, "\n")] = 0; // Remove newline
     
     // Get password
     printf("Password: ");
     fgets(password, sizeof(password), stdin);
     password[strcspn(password, "\n")] = 0; // Remove newline
     
     // Send username to server
     if (send(sock, username, strlen(username), 0) < 0) {
         perror("Send username failed");
         return -1;
     }
     
     // Add a small delay to ensure messages are separated
     sleep(1);
     
     // Send password to server
     if (send(sock, password, strlen(password), 0) < 0) {
         perror("Send password failed");
         return -1;
     }
     
     // Receive authentication response
     memset(response, 0, sizeof(response));
     bytes_received = recv(sock, response, sizeof(response) - 1, 0);
     if (bytes_received <= 0) {
         printf("Error receiving response from server\n");
         return -1;
     }
     
     printf("Server response: %s\n", response);
     
     // Check if authentication was successful
     if (strstr(response, "Authentication successful") == NULL) {
         return -1;
     }
     
     return 0;
 }
 
 /**
  * Transfer a file to the server
  */
 int transfer_file(int sock, const char *filepath, const char *department) {
     char buffer[BUFFER_SIZE];
     char response[BUFFER_SIZE];
     int bytes_read, bytes_received;
     struct stat file_stat;
     
     // Check if file exists
     if (stat(filepath, &file_stat) != 0) {
         printf("Error: Cannot access file '%s': %s\n", filepath, strerror(errno));
         return -1;
     }
     
     // Send department to server
     send(sock, department, strlen(department), 0);
     
     // Add a small delay between messages
     sleep(1);
     
     // Send filepath to server
     send(sock, filepath, strlen(filepath), 0);
     
     // Add a small delay between messages
     sleep(1);
     
     // Send file size to server
     uint32_t file_size = htonl(file_stat.st_size);
     send(sock, &file_size, sizeof(file_size), 0);
     
     // Open file for reading
     int file_fd = open(filepath, O_RDONLY);
     if (file_fd < 0) {
         printf("Error opening file '%s': %s\n", filepath, strerror(errno));
         return -1;
     }
     
     // Send file data
     int total_sent = 0;
     while (total_sent < file_stat.st_size) {
         bytes_read = read(file_fd, buffer, sizeof(buffer));
         if (bytes_read <= 0) {
             if (bytes_read < 0) {
                 printf("Error reading file: %s\n", strerror(errno));
             }
             break;
         }
         
         int bytes_sent = send(sock, buffer, bytes_read, 0);
         if (bytes_sent < 0) {
             printf("Error sending file data: %s\n", strerror(errno));
             close(file_fd);
             return -1;
         }
         
         total_sent += bytes_sent;
         
         // Show progress
         float progress = (float)total_sent / file_stat.st_size * 100;
         printf("\rTransferring: %.2f%% complete", progress);
         fflush(stdout);
     }
     
     // Close file
     close(file_fd);
     printf("\n");
     
     // Receive transfer response
     memset(response, 0, sizeof(response));
     bytes_received = recv(sock, response, sizeof(response) - 1, 0);
     if (bytes_received <= 0) {
         printf("Error receiving response from server\n");
         return -1;
     }
     
     printf("Server response: %s\n", response);
     
     // Check if transfer was successful
     if (strstr(response, "successfully transferred") == NULL) {
         return -1;
     }
     
     return 0;
 }