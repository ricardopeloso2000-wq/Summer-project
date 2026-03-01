#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#include "Global_Functions/Global_Functions.h"
#include "Queues/DMAQueue.h"
#include "SmartPointers/DMASmartPointer.h"

#include "driver/spi_slave.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define VSPI_MOSI 23
#define VSPI_MISO 19
#define VSPI_CLK  18
#define VSPI_CS   5

#define VSPI_HANDSHAKE_MOSI_LINE 34
#define VSPI_HANDSHAKE_MISO_LINE 35

#define HSPI_MOSI 13
#define HSPI_MISO 12
#define HSPI_CLK  14
#define HSPI_CS   15

#define HSPI_HANDSHAKE_MOSI_LINE 32
#define HSPI_HANDSHAKE_MISO_LINE 33

#define SLAVE_TX_QUEUE_SIZE 6
#define SLAVE_RX_QUEUE_SIZE 6
#define BUFFSIZE 4096

class SPI_Slave
{
    public:
    static SPI_Slave& VSPI_Instance();
    static SPI_Slave& HSPI_Instance();

    SPI_Slave(SPI_Slave&) = delete;
    SPI_Slave operator=(SPI_Slave&) = delete;

    bool PutMessageOnTXQueue(const DMASmartPointer<uint8_t>& TX_ptr , size_t size);
    bool GetMessageOnRXQueue(DMASmartPointer<uint8_t>& smt_ptr);

    static void IRAM_ATTR VSPI_GPIO_Callback(void* inst);
    static void IRAM_ATTR HSPI_GPIO_Callback(void* inst);
    static void IRAM_ATTR TransmitThread(void* pvParameters);

    spi_host_device_t Slave_Id;

    private:
    SPI_Slave(spi_host_device_t Id);
    ~SPI_Slave();

    void VSPI_INIT();
    void HSPI_INIT();

    void GPIO_routine();
    void TransmitThread_routine();

    const char* SPI_Tag = "SPI_slave";

    TaskHandle_t Thread;
    SemaphoreHandle_t rdysem;

    volatile bool stop_thread = false;
    volatile bool Master_Sending = false;
    volatile bool transaction_ongoing = false;

    DMAQueue RX_queue;
    DMAQueue TX_queue;
    DMASmartPointer<uint8_t> Clear_Buffer;
};

#endif