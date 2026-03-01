#ifndef SPIM_H
#define SPIM_H

#define INCLUDE_vTaskDelete 1

#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "Global_Functions/Global_Functions.h"
#include "Queues/DMAQueue.h"
#include "SmartPointers/DMASmartPointer.h"

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

#define MASTER_TX_QUEUE_SIZE 6
#define MASTER_RX_QUEUE_SIZE 6
#define BUFFSIZE 4096


class SPI_master
{
    public:
    //public members
    static SPI_master& VSPI_Instance();
    static SPI_master& HSPI_Instance();

    SPI_master(SPI_master&) = delete;
    SPI_master operator=(SPI_master&) = delete;

    void SPI_LockBus();
    void SPI_UnLockBus();
    
    bool PutMessageOnTXQueue(const DMASmartPointer<uint8_t>& TX_ptr , size_t size);
    bool GetLastRecivedMessage(DMASmartPointer<uint8_t>& smt_ptr);

    static void IRAM_ATTR VSPI_GPIO_CALLBACK(void* inst);
    static void IRAM_ATTR HSPI_GPIO_CALLBACK(void* inst);
    static void TransmitThread(void* pvParameters);
    
    private:
    //private members
    SPI_master(spi_host_device_t Id);
    ~SPI_master();

    void VSPI_INIT();
    void HSPI_INIT();

    void GPIO_routine();

    void TrasmitThread_routine();

    TaskHandle_t Thread;
    spi_host_device_t Spi_Id;
    const char* SPI_Tag = "SPI_Master";
    
    volatile bool stop_thread = false;
    volatile bool Slave_Sending = false;
    volatile bool transaction_ongoing = false;

    DMAQueue RX_queue;
    DMAQueue TX_queue;
    DMASmartPointer<uint8_t> Clear_Buffer;
    spi_device_handle_t SPI_Handle;
};


#endif