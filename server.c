/**
 * File Transfer Server for Manufacturing Company
 * 
 * This server handles file transfers from multiple clients simultaneously
 * using multithreading. It ensures proper file ownership attribution and
 * enforces access controls based on user groups.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <pthread.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <pwd.h>
 #include <grp.h>
 #include <errno.h>
 #include <arpa/inet.h>
 
 #define PORT 8080
 #define MAX_CLIENTS 10
 #define BUFFER_SIZE 1024
 #define MAX_USERNAME_LENGTH 32
 #define MAX_PASSWORD_LENGTH 32
 #define MAX_FILEPATH_LENGTH 256
 #define MAX_DEPT_LENGTH 32
 
 // Base directory for file storage
 #define BASE_DIR "/tmp/fileserver"
 #define MANUFACTURING_DIR "/tmp/fileserver/Manufacturing"
 #define DISTRIBUTION_DIR "/tmp/fileserver/Distribution"
 
 // Structure to hold client connection information
 typedef struct {
     int socket;
     struct sockaddr_in address;
 } client_t;
 
 // Structure to hold authentication information
 typedef struct {
     char username[MAX_USERNAME_LENGTH];
     char department[MAX_DEPT_LENGTH];
     uid_t uid;
     gid_t gid;
 } auth_info_t;
 
 // Mutex for thread synchronization
 pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
 
 // Function prototypes
 void *handle_client(void *client_socket);
 int authenticate_user(int sock, auth_info_t *auth_info);
 int receive_file(int sock, const auth_info_t *auth_info);
 int check_access(const char *department, const auth_info_t *auth_info);
 void setup_directories();
 int is_user_in_group(const char *username, const char *groupname);
 
 /**
  * Checks if a user belongs to a specific group
  */
 int is_user_in_group(const char *username, const char *groupname) {
     struct group *grp = getgrnam(groupname);
     if (!grp) {
         return 0;
     }
     
     // Check if user is in gr_mem
     char **members = grp->gr_mem;
     while (*members) {
         if (strcmp(*members, username) == 0) {
             return 1;
         }
         members++;
     }
     
     // If not found in gr_mem, check primary group
     struct passwd *pwd = getpwnam(username);
     if (!pwd) {
         return 0;
     }
     
     if (pwd->pw_gid == grp->gr_gid) {
         return 1;
     }
     
     return 0;
 }
 
 int main() {
     int server_fd, client_sock;
     struct sockaddr_in address;
     int addrlen = sizeof(address);
     pthread_t thread_id;
     
     // Create socket
     if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
         perror("Socket creation failed");
         exit(EXIT_FAILURE);
     }
     
     // Set socket options to reuse address and port
     int opt = 1;
     if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
         perror("Setsockopt failed");
         exit(EXIT_FAILURE);
     }
     
     // Configure server address
     address.sin_family = AF_INET;
     address.sin_addr.s_addr = INADDR_ANY;
     address.sin_port = htons(PORT);
     
     // Bind socket to address and port
     if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
         perror("Bind failed");
         exit(EXIT_FAILURE);
     }
     
     // Listen for incoming connections
     if (listen(server_fd, MAX_CLIENTS) < 0) {
         perror("Listen failed");
         exit(EXIT_FAILURE);
     }
     
     // Create required directories if they don't exist
     setup_directories();
     
     printf("Server started on port %d\n", PORT);
     printf("Waiting for connections...\n");
     
     // Accept and handle incoming connections
     while (1) {
         // Accept new connection
         if ((client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
             perror("Accept failed");
             continue;
         }
         
         // Create client structure
         client_t *client = malloc(sizeof(client_t));
         if (client == NULL) {
             perror("Failed to allocate memory for client");
             close(client_sock);
             continue;
         }
         
         client->socket = client_sock;
         client->address = address;
         
         // Create new thread to handle client
         if (pthread_create(&thread_id, NULL, handle_client, (void*)client) < 0) {
             perror("Thread creation failed");
             free(client);
             close(client_sock);
             continue;
         }
         
         // Detach thread to allow resources to be freed automatically when thread exits
         pthread_detach(thread_id);
     }
     
     // Clean up
     close(server_fd);
     return 0;
 }
 
 /**
  * Creates required directories with proper permissions
  */
 void setup_directories() {
     // Create base directory
     mkdir(BASE_DIR, 0755);
     
     // Create department directories
     mkdir(MANUFACTURING_DIR, 0777);  
     mkdir(DISTRIBUTION_DIR, 0777);   
     
     // Set group ownership for the directories
     struct group *manufacturing_group = getgrnam("Manufacturing");
     struct group *distribution_group = getgrnam("Distribution");
     
     if (manufacturing_group) {
         printf("Found Manufacturing group with GID: %d\n", manufacturing_group->gr_gid);
         chown(MANUFACTURING_DIR, 0, manufacturing_group->gr_gid);
     } else {
         printf("WARNING: Manufacturing group not found, directory permissions may be incorrect\n");
     }
     
     if (distribution_group) {
         printf("Found Distribution group with GID: %d\n", distribution_group->gr_gid);
         chown(DISTRIBUTION_DIR, 0, distribution_group->gr_gid);
     } else {
         printf("WARNING: Distribution group not found, directory permissions may be incorrect\n");
     }
     
     printf("Directory setup complete\n");
 }
 
 /**
  * Thread function to handle client connection
  */
 void *handle_client(void *client_ptr) {
     client_t *client = (client_t *)client_ptr;
     int sock = client->socket;
     auth_info_t auth_info;
     char client_ip[INET_ADDRSTRLEN];
     
     // Store client IP for logging
     inet_ntop(AF_INET, &(client->address.sin_addr), client_ip, INET_ADDRSTRLEN);
     int client_port = ntohs(client->address.sin_port);
     
     printf("New connection from %s:%d\n", client_ip, client_port);
     
     // Authenticate user
     if (authenticate_user(sock, &auth_info) != 0) {
         printf("Authentication failed for client %s:%d\n", client_ip, client_port);
         close(sock);
         free(client);
         return NULL;
     }
     
     printf("User '%s' authenticated successfully from %s:%d\n", 
            auth_info.username, client_ip, client_port);
     
     // Receive and process file
     if (receive_file(sock, &auth_info) != 0) {
         printf("File transfer failed for user '%s' from %s:%d\n", 
                auth_info.username, client_ip, client_port);
     }
     
     // Clean up
     close(sock);
     free(client);
     printf("Connection closed with %s:%d\n", client_ip, client_port);
     return NULL;
 }
 
 /**
  * Authenticates a user based on username and password
  */
 int authenticate_user(int sock, auth_info_t *auth_info) {
     char username[MAX_USERNAME_LENGTH];
     char password[MAX_PASSWORD_LENGTH];
     char response[BUFFER_SIZE];
     int bytes_received;
     
     // Receive username
     memset(username, 0, sizeof(username));
     bytes_received = recv(sock, username, sizeof(username) - 1, 0);
     if (bytes_received <= 0) {
         return -1;
     }
     
     // Receive password
     memset(password, 0, sizeof(password));
     bytes_received = recv(sock, password, sizeof(password) - 1, 0);
     if (bytes_received <= 0) {
         return -1;
     }
     
     // Get user information
     struct passwd *pwd;
     if ((pwd = getpwnam(username)) == NULL) {
         strcpy(response, "Authentication failed: User not found");
         send(sock, response, strlen(response), 0);
         return -1;
     }
     
     // Check which department the user belongs to
     int is_manufacturing = is_user_in_group(username, "Manufacturing");
     int is_distribution = is_user_in_group(username, "Distribution");
     
     // Determine department
     if (is_manufacturing && !is_distribution) {
         strcpy(auth_info->department, "Manufacturing");
     } else if (!is_manufacturing && is_distribution) {
         strcpy(auth_info->department, "Distribution");
     } else if (is_manufacturing && is_distribution) {
         // User is in both groups, default to Manufacturing
         strcpy(auth_info->department, "Manufacturing");
     } else {
         // User is not in any required group
         strcpy(response, "Authentication failed: User not in required groups");
         send(sock, response, strlen(response), 0);
         return -1;
     }
     
     // Store user info
     strcpy(auth_info->username, username);
     auth_info->uid = pwd->pw_uid;
     auth_info->gid = pwd->pw_gid;
     
     // Send success response with department
     sprintf(response, "Authentication successful. Department: %s", auth_info->department);
     send(sock, response, strlen(response), 0);
     
     return 0;
 }
 
 /**
  * Checks if a user has access to a specific department
  */
 int check_access(const char *department, const auth_info_t *auth_info) {
     // User must be in the same department
     return strcmp(department, auth_info->department) == 0;
 }
 
 /**
  * Receives and processes a file from the client
  */
 int receive_file(int sock, const auth_info_t *auth_info) {
     char filepath[MAX_FILEPATH_LENGTH];
     char department[MAX_DEPT_LENGTH];
     char buffer[BUFFER_SIZE];
     char response[BUFFER_SIZE];
     int bytes_received;
     
     // Receive department
     memset(department, 0, sizeof(department));
     bytes_received = recv(sock, department, sizeof(department) - 1, 0);
     if (bytes_received <= 0) {
         return -1;
     }
     
     // Check if user has access to the department
     if (!check_access(department, auth_info)) {
         sprintf(response, "Error: You don't have access to the %s department", department);
         send(sock, response, strlen(response), 0);
         return -1;
     }
     
     // Receive filename
     memset(filepath, 0, sizeof(filepath));
     bytes_received = recv(sock, filepath, sizeof(filepath) - 1, 0);
     if (bytes_received <= 0) {
         return -1;
     }
     
     // Extract filename from path
     char *filename = strrchr(filepath, '/');
     if (filename == NULL) {
         filename = filepath;
     } else {
         filename++;  // Skip the '/'
     }
     
     // Create the complete destination path
     char dest_path[MAX_FILEPATH_LENGTH];
     if (strcmp(department, "Manufacturing") == 0) {
         snprintf(dest_path, sizeof(dest_path), "%s/%s", MANUFACTURING_DIR, filename);
     } else if (strcmp(department, "Distribution") == 0) {
         snprintf(dest_path, sizeof(dest_path), "%s/%s", DISTRIBUTION_DIR, filename);
     } else {
         strcpy(response, "Error: Invalid department");
         send(sock, response, strlen(response), 0);
         return -1;
     }
     
     // Receive file size
     uint32_t file_size;
     bytes_received = recv(sock, &file_size, sizeof(file_size), 0);
     if (bytes_received <= 0) {
         return -1;
     }
     file_size = ntohl(file_size);
     
     // Create file
     pthread_mutex_lock(&file_mutex);
     int file_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
     if (file_fd < 0) {
         pthread_mutex_unlock(&file_mutex);
         sprintf(response, "Error: Cannot create file: %s", strerror(errno));
         send(sock, response, strlen(response), 0);
         return -1;
     }
     
     // Receive and write file data
     uint32_t remaining = file_size;
     while (remaining > 0) {
         size_t to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
         bytes_received = recv(sock, buffer, to_read, 0);
         
         if (bytes_received <= 0) {
             close(file_fd);
             pthread_mutex_unlock(&file_mutex);
             return -1;
         }
         
         write(file_fd, buffer, bytes_received);
         remaining -= bytes_received;
     }
     
     // Close file
     close(file_fd);
     
     // Attempt to set file ownership, but don't fail if it doesn't work
     if (chown(dest_path, auth_info->uid, -1) < 0) {
         printf("Warning: Could not set file ownership: %s\n", strerror(errno));
     }
     
     // Create a file in the same directory with the owner's name for attribution
     char attribution_path[MAX_FILEPATH_LENGTH];
     snprintf(attribution_path, sizeof(attribution_path), "%s/%s.owner", 
              (strcmp(department, "Manufacturing") == 0) ? MANUFACTURING_DIR : DISTRIBUTION_DIR, 
              filename);
              
     int attr_fd = open(attribution_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
     if (attr_fd >= 0) {
         // Write the username to the attribution file
         write(attr_fd, auth_info->username, strlen(auth_info->username));
         close(attr_fd);
     }
     
     pthread_mutex_unlock(&file_mutex);
     
     // Send success response
     sprintf(response, "File '%s' successfully transferred to %s department", filename, department);
     send(sock, response, strlen(response), 0);
     
     printf("File '%s' transferred by user '%s' to %s department\n", 
            filename, auth_info->username, department);
     
     return 0;
 }