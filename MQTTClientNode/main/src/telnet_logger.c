#include "telnet_logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdarg.h>

static const char *TAG = "TELNET_LOGGER";

#define MAX_CLIENTS 4
#define TELNET_BUFFER_SIZE 512
#define TELNET_STACK_SIZE 4096

// Client connection structure
typedef struct {
    int socket;
    bool active;
} telnet_client_t;

// Server state
static struct {
    int server_socket;
    uint16_t port;
    telnet_client_t clients[MAX_CLIENTS];
    TaskHandle_t accept_task;
    bool running;
    vprintf_like_t original_log_func;
} telnet_server = {
    .server_socket = -1,
    .port = 0,
    .running = false,
    .original_log_func = NULL
};

// Forward declarations
static int telnet_vprintf(const char *fmt, va_list args);
static void telnet_accept_task(void *pvParameters);
static void close_client(int client_idx);

/**
 * @brief Custom vprintf that sends logs to both serial and Telnet clients
 */
static int telnet_vprintf(const char *fmt, va_list args)
{
    char buffer[TELNET_BUFFER_SIZE];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    
    if (len > 0) {
        // Send to all connected Telnet clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (telnet_server.clients[i].active) {
                int sent = send(telnet_server.clients[i].socket, buffer, len, 0);
                if (sent < 0) {
                    // Connection lost, close this client
                    close_client(i);
                }
            }
        }
        
        // Also send to original output (serial/USB)
        if (telnet_server.original_log_func) {
            va_list args_copy;
            va_copy(args_copy, args);
            telnet_server.original_log_func(fmt, args_copy);
            va_end(args_copy);
        }
    }
    
    return len;
}

/**
 * @brief Close a client connection
 */
static void close_client(int client_idx)
{
    if (client_idx >= 0 && client_idx < MAX_CLIENTS) {
        if (telnet_server.clients[client_idx].active) {
            close(telnet_server.clients[client_idx].socket);
            telnet_server.clients[client_idx].socket = -1;
            telnet_server.clients[client_idx].active = false;
            ESP_LOGI(TAG, "Client %d disconnected", client_idx);
        }
    }
}

/**
 * @brief Find free client slot
 */
static int find_free_slot(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!telnet_server.clients[i].active) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Task that accepts incoming Telnet connections
 */
static void telnet_accept_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Telnet accept task started on port %d", telnet_server.port);
    
    while (telnet_server.running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        // Accept new connection (blocking)
        int client_sock = accept(telnet_server.server_socket, 
                                 (struct sockaddr *)&client_addr, 
                                 &addr_len);
        
        if (client_sock < 0) {
            if (telnet_server.running) {
                ESP_LOGE(TAG, "Accept failed: errno %d", errno);
            }
            break;
        }
        
        // Find free slot for new client
        int slot = find_free_slot();
        if (slot == -1) {
            ESP_LOGW(TAG, "Max clients reached, rejecting connection");
            const char *msg = "Server full. Try again later.\r\n";
            send(client_sock, msg, strlen(msg), 0);
            close(client_sock);
            continue;
        }
        
        // Store client
        telnet_server.clients[slot].socket = client_sock;
        telnet_server.clients[slot].active = true;
        
        // Get client IP
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        
        ESP_LOGI(TAG, "Client %d connected from %s", slot, client_ip);
        
        // Send welcome message
        const char *welcome = "\r\n*** ESP32 Telnet Logger ***\r\n"
                             "Connected successfully. Logs will appear below.\r\n\r\n";
        send(client_sock, welcome, strlen(welcome), 0);
    }
    
    ESP_LOGI(TAG, "Telnet accept task stopped");
    vTaskDelete(NULL);
}

esp_err_t telnet_logger_init(uint16_t port)
{
    if (telnet_server.running) {
        ESP_LOGW(TAG, "Telnet logger already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    telnet_server.port = port;
    
    // Initialize client slots
    for (int i = 0; i < MAX_CLIENTS; i++) {
        telnet_server.clients[i].socket = -1;
        telnet_server.clients[i].active = false;
    }
    
    // Create server socket
    telnet_server.server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (telnet_server.server_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(telnet_server.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(port)
    };
    
    if (bind(telnet_server.server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: errno %d", errno);
        close(telnet_server.server_socket);
        return ESP_FAIL;
    }
    
    // Listen for connections
    if (listen(telnet_server.server_socket, 5) < 0) {
        ESP_LOGE(TAG, "Failed to listen: errno %d", errno);
        close(telnet_server.server_socket);
        return ESP_FAIL;
    }
    
    telnet_server.running = true;
    
    // Create accept task
    BaseType_t ret = xTaskCreate(telnet_accept_task, "telnet_accept", 
                                  TELNET_STACK_SIZE, NULL, 5, 
                                  &telnet_server.accept_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create accept task");
        close(telnet_server.server_socket);
        telnet_server.running = false;
        return ESP_FAIL;
    }
    
    // Redirect logs to Telnet (also keep serial output)
    telnet_server.original_log_func = esp_log_set_vprintf(telnet_vprintf);
    
    ESP_LOGI(TAG, "Telnet logger started on port %d", port);
    return ESP_OK;
}

esp_err_t telnet_logger_deinit(void)
{
    if (!telnet_server.running) {
        ESP_LOGW(TAG, "Telnet logger not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    telnet_server.running = false;
    
    // Restore original log function
    if (telnet_server.original_log_func) {
        esp_log_set_vprintf(telnet_server.original_log_func);
    }
    
    // Close all client connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        close_client(i);
    }
    
    // Close server socket (this will unblock accept())
    if (telnet_server.server_socket >= 0) {
        close(telnet_server.server_socket);
        telnet_server.server_socket = -1;
    }
    
    // Task will auto-delete after accept() fails
    // Give it time to cleanup
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Telnet logger stopped");
    return ESP_OK;
}

bool telnet_logger_has_clients(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (telnet_server.clients[i].active) {
            return true;
        }
    }
    return false;
}

int telnet_logger_get_client_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (telnet_server.clients[i].active) {
            count++;
        }
    }
    return count;
}
