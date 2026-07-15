# ESP32-C6 WROOM-1 Telemetry & Command Gateway

This project acts as a bridge between a Drone (via Serial UART) and a Ground Control Station (via Wi-Fi/UDP/HTTP). It handles telemetry data transmission to a Django server and receives flight commands via UDP to forward them to the drone.

##  Hardware Specifications
* **Microcontroller**: ESP32-C6 WROOM-1
* **Flash Memory**: Configured to **8MB** (Customized via `menuconfig` to accommodate larger firmware overhead from networking, Wi-Fi, and JSON parsing components).
* **UART Interface** (Reference: [`config.h`](config.h) and [`uart.c`](uart.c)): Utilizing **`UART_NUM_0`** for the primary serial communication with the drone's flight controller.

##  System Configuration (FreeRTOS & CMake)
* **FreeRTOS Tick Rate**: Set to `1000 Hz` (`CONFIG_FREERTOS_HZ=1000`). This is crucial for precise millisecond delays and ensures compatibility with external libraries.
* **Task Stack Sizes** (Reference: [`main.c`](main.c)):: Network operations and JSON parsing consume significant memory. Stack sizes are optimized to prevent stack overflow:
  * http_sender_task`: 8192 Bytes
  * uart_tx_task`: 8192 Bytes
  * uart_rx_task`: 8192 Bytes
  * udp_receiver_task`: 4096 Bytes
* **CMake Dependencies** (Reference: [`CMakeLists.txt`](CMakeLists.txt)): The project explicitly requires `driver`, `esp_wifi`, `esp_event`, `nvs_flash`, `esp_http_client`, `esp_timer`, and `json`.

## Software Architecture & Data Flow (FreeRTOS Queues)
The system utilizes FreeRTOS Queues to pass data asynchronously between tasks without blocking the CPU.

### Task Communication Diagram
```text
[UDP Client] --(JSON String)--> [udp_receiver_task]
                                        |
                                (uart_tx_queue)
                                        v
                                [tx_task] --(24-Byte Binary)--> [UART_NUM_0 TX]

[UART_NUM_0 RX] --(22-Byte Binary)--> [rx_task]
                                        |
                                  (http_queue)
                                        v
                             [http_sender_task] --(HTTP POST)--> [Django Server]
```

## Queue Infrastructures
Detailed implementation can be found in [`main.c`](main.c) and [`drone_protocol.h`](drone_protocol.h).
1. **`uart_tx_queue`**: Holds up to 10 outbound command packets containing the `drone_command_t` structure (24 bytes fixed length).
2. **`http_queue`**: Holds up to 10 serialized telemetry JSON strings with a buffer allocation of 256 bytes per item.


## Network Protocols & Operation

### 1. UDP Command Receiver
Implementation details in [`HTTP.c`](HTTP.c) (`udp_receiver_task`).
* **Port Listener**: Binds and listens on local port `1234`.
* **Low-Power Idle Processing**: The task instantiates a standard network socket and calls the `recvfrom()` method. This execution halts the task entirely, putting it into a Blocked State which consumes 0% CPU cycles while waiting for network input.  
* **Command Ingestion**: The moment an external UDP client transmits a JSON packet, the task immediately wakes up within milliseconds. It passes the raw string to `convert2uart()` where it is parsed using cJSON. The values are converted into a packed `drone_command_t` binary structure, verified, and safely pushed to the `uart_tx_queue`.  

### 2. HTTP Telemetry POST Sender
Implementation details in [`HTTP.c`](HTTP.c) (`http_sender_task`) and [`uart.c`](uart.c) (`rx_task`).
* **Destination Endpoint**: Targets the remote Django API located at `localhost:8000/api/data/`.
* **Payload Format**: Enforces an explicit `application/json` header structure.  
* **Transmission Interval**: The HTTP client does not operate on a fixed timer loop; it is entirely event-driven.
  * The `uart_rx_task` continually listens for a valid telemetry stream.  
  * Once a complete 22-byte `monitor_packet_t` is found and passes the `calc_checksum` verification, it is converted to a JSON string and sent to the `http_queue`.  
  * The `http_sender_task` instantly pops the payload from the queue using `portMAX_DELAY` and triggers `send_http_post`.  
  * Consequently, the HTTP upload frequency perfectly mirrors the drone's transmission interval. If the drone transmits telemetry data over UART every 0.5 seconds (500ms), the gateway will perform an HTTP POST update every 0.5 seconds concurrently.