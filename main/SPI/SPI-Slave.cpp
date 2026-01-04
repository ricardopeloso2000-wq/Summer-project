#include "SPI-Slave.h"

SPI_Slave& SPI_Slave::VSPI_Instance()
{
    static SPI_Slave VSPI_Slave_Isnt(VSPI_HOST);
    return VSPI_Slave_Isnt;
}

SPI_Slave& SPI_Slave::HSPI_Instance()
{
    static SPI_Slave HSPI_Slave_Isnt(HSPI_HOST);
    return HSPI_Slave_Isnt;
}

SPI_Slave::SPI_Slave(spi_host_device_t Id) : RX_queue(SLAVE_RX_QUEUE_SIZE , Id , BUFFSIZE) , TX_queue(SLAVE_TX_QUEUE_SIZE , Id , BUFFSIZE)
{
    static bool gpio_isr_installed = false;
    if(!gpio_isr_installed) {
        gpio_install_isr_service(0);
        gpio_isr_installed = true;
    }

    Slave_Id = Id;
    if(Id == VSPI_HOST) VSPI_INIT();
    if(Id == HSPI_HOST) HSPI_INIT();

    Clear_Buffer.SetPointer((uint8_t*)spi_bus_dma_memory_alloc(Slave_Id , BUFFSIZE , 0));

    xTaskCreatePinnedToCore(
        SPI_Slave::TransmitThread,
        (Id == VSPI_HOST) ? "VSPI Thread" : "HSPI Thread",
        8192,
        this,
        5,
        &Thread,
        1
    );
}

void SPI_Slave::VSPI_INIT()
{
    spi_bus_config_t buscfg;
    buscfg.mosi_io_num = VSPI_MOSI;
    buscfg.miso_io_num = VSPI_MISO;
    buscfg.sclk_io_num = VSPI_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;
   
    spi_slave_interface_config_t devcfg;
    devcfg.mode = 1;
    devcfg.queue_size = 1;
    devcfg.spics_io_num = VSPI_CS;

    switch(spi_slave_initialize(Slave_Id, &buscfg, &devcfg , SPI_DMA_CH_AUTO))
    {
        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(SPI_Tag , "host already is in use");
            break;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(SPI_Tag , "there is no available DMA channel");
            break;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(SPI_Tag , "ESP out of memory");
            break;
        default:
            break;
    }

    //sets up Slave MOSI hanshake line
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = BIT64(VSPI_HANDSHAKE_MOSI_LINE);
    
    gpio_config(&io_conf);
    gpio_set_intr_type(gpio_num_t(VSPI_HANDSHAKE_MOSI_LINE), GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(gpio_num_t(VSPI_HANDSHAKE_MOSI_LINE), SPI_Slave::VSPI_GPIO_Callback, this);

    //sets up GPIO MISO line
    io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(VSPI_HANDSHAKE_MISO_LINE);

    gpio_config(&io_conf);
}

void SPI_Slave::HSPI_INIT()
{
    spi_bus_config_t buscfg;
    buscfg.mosi_io_num = HSPI_MOSI;
    buscfg.miso_io_num = HSPI_MISO;
    buscfg.sclk_io_num = HSPI_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;
   
    spi_slave_interface_config_t devcfg;
    devcfg.mode = 1;
    devcfg.queue_size = 1;
    devcfg.spics_io_num = HSPI_CS;

    switch(spi_slave_initialize(Slave_Id, &buscfg, &devcfg , SPI_DMA_CH_AUTO))
    {
        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(SPI_Tag , "host already is in use");
            break;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(SPI_Tag , "there is no available DMA channel");
            break;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(SPI_Tag , "ESP out of memory");
            break;
        default:
            break;
    }

    //sets up Slave MOSI hanshake line
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = BIT64(HSPI_HANDSHAKE_MOSI_LINE);
    
    gpio_config(&io_conf);
    gpio_set_intr_type(gpio_num_t(HSPI_HANDSHAKE_MOSI_LINE), GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(gpio_num_t(HSPI_HANDSHAKE_MOSI_LINE), SPI_Slave::HSPI_GPIO_Callback, this);

    //sets up GPIO MISO line
    io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(HSPI_HANDSHAKE_MISO_LINE);

    gpio_config(&io_conf);
}

SPI_Slave::~SPI_Slave()
{
    spi_slave_free(Slave_Id);
    stop_thread = true;
    vTaskDelay(10 / portTICK_PERIOD_MS);
    vTaskDelete(Thread);
}

void SPI_Slave::VSPI_GPIO_Callback(void* inst)
{
    static uint32_t lasthandshaketime_us;
    uint32_t currtime_us = esp_timer_get_time();
    uint32_t diff = currtime_us - lasthandshaketime_us;
    if (diff < 1000) {
        return; //ignore everything <1ms after an earlier irq
    }
    lasthandshaketime_us = currtime_us;

    auto i = static_cast<SPI_Slave*>(inst);
    i->GPIO_routine();
}

void SPI_Slave::HSPI_GPIO_Callback(void* inst)
{
    static uint32_t lasthandshaketime_us;
    uint32_t currtime_us = esp_timer_get_time();
    uint32_t diff = currtime_us - lasthandshaketime_us;
    if (diff < 1000) {
        return; //ignore everything <1ms after an earlier irq
    }
    lasthandshaketime_us = currtime_us;

    auto i = static_cast<SPI_Slave*>(inst);
    i->GPIO_routine();
}

void SPI_Slave::GPIO_routine()
{
    if(TX_queue.empty()) Master_Sending= true;

    if(Slave_Id == VSPI_HOST) gpio_set_level((gpio_num_t)VSPI_HANDSHAKE_MISO_LINE , 1);
    if(Slave_Id == HSPI_HOST) gpio_set_level((gpio_num_t)HSPI_HANDSHAKE_MISO_LINE , 1);
}

void SPI_Slave::TransmitThread(void* pvParameters)
{
    auto inst = static_cast<SPI_Slave*>(pvParameters);
    inst->TransmitThread_routine();
}

void SPI_Slave::TransmitThread_routine()
{
    while(1)
    {
        while(!TX_queue.empty() || Master_Sending)
        {
            if(RX_queue.size() >= SLAVE_RX_QUEUE_SIZE)
            {
                RX_queue.pop();
                ESP_LOGW(SPI_Tag,"Dropping last RX_Buf from the queue");
            }

            spi_slave_transaction_t t = {};
            t.user = this;
            t.length = BUFFSIZE * 8;

            t.rx_buffer = RX_queue.back();
            RX_queue.push();

            if(Master_Sending)
            {
                t.tx_buffer = Clear_Buffer.GetPointer();
            }
            else
            {
                t.tx_buffer = TX_queue.front();
            }
 
            transaction_ongoing = true;
            spi_slave_transmit(Slave_Id , &t , portMAX_DELAY);
            transaction_ongoing = false;

            if(!Master_Sending) TX_queue.pop();
            Master_Sending = false;

            if(Slave_Id == VSPI_HOST) gpio_set_level((gpio_num_t)VSPI_HANDSHAKE_MISO_LINE , 0);
            if(Slave_Id == HSPI_HOST) gpio_set_level((gpio_num_t)HSPI_HANDSHAKE_MISO_LINE , 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

bool SPI_Slave::PutMessageOnTXQueue(const DMASmartPointer<uint8_t>& TX_ptr , size_t size)
{
    if(TX_ptr.GetPointer() == nullptr) return false;
    if(TX_queue.size() >= SLAVE_TX_QUEUE_SIZE) return false;

    TX_queue.push(TX_ptr.GetPointer() , size);
    return true;
}   

bool SPI_Slave::GetMessageOnRXQueue(DMASmartPointer<uint8_t>& smt_ptr)
{
    if(RX_queue.empty()) return false;
    if(RX_queue.size() == 1 && transaction_ongoing) return false;

    
    memcpy(smt_ptr.GetPointer() , RX_queue.front() , BUFFSIZE);
    RX_queue.pop();

    return true;
}


