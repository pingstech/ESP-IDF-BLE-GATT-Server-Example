# BLE Demo Application

## Overview

The **BLE Demo** application demonstrates the use of Bluetooth Low Energy (BLE) in an embedded system. This project uses the ESP32 platform with FreeRTOS and the ESP-IDF framework. The application includes a basic BLE GATT server setup and advertising features. It enables the creation of a BLE service, advertisement of that service, and supports communication via BLE characteristics.

## Features

- **BLE GATT Server**: The app configures a BLE service with characteristics that can be read, written, and notified.
- **JSON Parsing**: The application parses incoming JSON messages and responds with acknowledgments.
- **FreeRTOS Integration**: The app is designed to run in a FreeRTOS environment.
- **BLE Advertising**: It advertises a BLE service for device discovery.
  
## Project Setup

This project uses the ESP-IDF framework and requires a compatible ESP32 device.

### Prerequisites

- **ESP-IDF**: Version 4.0 or later
- **ESP32 Development Board**
- **CMake**: Version 3.16 or later

### Setting Up the Development Environment

1. Install **ESP-IDF** by following the official installation guide: [ESP-IDF Setup Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/).
2. Set up your **ESP32 development environment** as per your operating system.

### Directory Structure

```
├── main
│   ├── CMakeLists.txt
│   └── main.c
├── src
│   └── app
│       ├── app_manager.h
│       └── ble_app.h
│   └── src
│       ├── app_manager.c
│       ├── ble_app.c
│       └── CMakeLists.txt
├── CMakeLists.txt
├── .gitignore
├── BLE_TUTORIAL_TR.md (Turkish)
├── README.md <- This is the file you are currently reading
└── sdkconfig.defaults
```

## More Information and Documentation

For a detailed tutorial and usage examples, please check out the [BLE TUTORIAL_TR.md](./BLE_TUTORIAL_TR.md) file.


## Building the Project

1. **Configure the project**:
   Open a terminal in your project directory and run:

   ```
   idf.py menuconfig
   ```

2. **Build the project**:    

   In the same terminal window, run:
   ```
   idf.py build
   ```

3. **Flash the firmware**:    

   After a successful build, flash the firmware onto your ESP32 board using:
   ```
   idf.py flash
   ```

4. **Flash the firmware**:    

   To monitor the output of your ESP32, use::
   ```
   idf.py monitor
   ```

## Contributing
Feel free to fork this project and contribute by submitting pull requests.

## License
This project is licensed under the MIT License.

## Contact
For any inquiries, you can reach the project author at:

Furkan YAYLA [Linkedin](https://www.linkedin.com/in/yaylafurkan/).