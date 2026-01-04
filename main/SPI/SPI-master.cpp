#include "SPI-master.h"

SPI_master& SPI_master::VSPI_Instance()
{
    static SPI_master VSPI_inst(VSPI_HOST);
    return VSPI_inst;
}
SPI_master& SPI_master::HSPI_Instance()
{
    static SPI_master HSPI_inst(HSPI_HOST);
    return HSPI_inst;
}

SPI_master::SPI_master(spi_host_device_t Id) : Spi_Id(Id), RX_queue(MASTER_RX_QUEUE_SIZE , Id , BUFFSIZE) , TX_queue(MASTER_TX_QUEUE_SIZE , Id , BUFFSIZE)
{
    static bool gpio_isr_installed = false;
    if(!gpio_isr_installed) {
        gpio_install_isr_service(0);
        gpio_isr_installed = true;
    }

    if(Id == VSPI_HOST)VSPI_INIT();
    if(Id == HSPI_HOST)HSPI_INIT();

    rdysem = xSemaphoreCreateBinary();
    xSemaphoreGive(rdysem);

    xTaskCreatePinnedToCore(
        SPI_master::TransmitThread,
        (Id == VSPI_HOST) ? "VSPI Thread" : "HSPI Thread",
        8192,
        this,
        5,
        &Thread,
        1
    );

    Clear_Buffer.SetPointer((uint8_t*)spi_bus_dma_memory_alloc(Spi_Id , BUFFSIZE , 0));
}
//Initiates the VSPI device
void SPI_master::VSPI_INIT()
{
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = VSPI_MOSI;
    buscfg.miso_io_num = VSPI_MISO;
    buscfg.sclk_io_num = VSPI_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;
   

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = SPI_MASTER_FREQ_20M;
    devcfg.mode = 0;
    devcfg.spics_io_num = VSPI_CS;
    devcfg.duty_cycle_pos = 128;
    devcfg.cs_ena_posttrans = 3;
    devcfg.queue_size = 1;
    devcfg.flags = SPI_DEVICE_NO_DUMMY;

    switch(spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO))
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

    switch(spi_bus_add_device(VSPI_HOST, &devcfg, &SPI_Handle))
    {
        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(SPI_Tag , "elected clock source is unavailable or spi bus not initialized");
            break;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(SPI_Tag , "host doesn't have any free CS slots");
            break;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(SPI_Tag , "ESP out of memory");
            break;
        default:
            break;
    }

    //Sets up GPIO MISO line
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = BIT64(VSPI_HANDSHAKE_MISO_LINE);
    
    gpio_config(&io_conf);
    gpio_set_intr_type(gpio_num_t(VSPI_HANDSHAKE_MISO_LINE), GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(gpio_num_t(VSPI_HANDSHAKE_MISO_LINE), SPI_master::VSPI_GPIO_CALLBACK, this);

    //sets up GPIO MOSI line
    io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(VSPI_HANDSHAKE_MOSI_LINE);

    gpio_config(&io_conf);
}

//Initiates the HSPI device
void SPI_master::HSPI_INIT()
{
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = HSPI_MOSI;
    buscfg.miso_io_num = HSPI_MISO;
    buscfg.sclk_io_num = HSPI_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;
   

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = SPI_MASTER_FREQ_20M;
    devcfg.mode = 0;
    devcfg.spics_io_num = HSPI_CS;
    devcfg.duty_cycle_pos = 128;
    devcfg.cs_ena_posttrans = 3;
    devcfg.queue_size = 1;

    switch(spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO))
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

    switch(spi_bus_add_device(HSPI_HOST, &devcfg, &SPI_Handle))
    {
        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(SPI_Tag , "elected clock source is unavailable or spi bus not initialized");
            break;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(SPI_Tag , "host doesn't have any free CS slots");
            break;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(SPI_Tag , "ESP out of memory");
            break;
        default:
            break;
    }


    //Sets up GPIO MISO line
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = BIT64(HSPI_HANDSHAKE_MISO_LINE);
    
    gpio_config(&io_conf);
    gpio_set_intr_type(gpio_num_t(HSPI_HANDSHAKE_MISO_LINE), GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(gpio_num_t(HSPI_HANDSHAKE_MISO_LINE), SPI_master::HSPI_GPIO_CALLBACK, this);

    //sets up GPIO MOSI line
    io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(HSPI_HANDSHAKE_MOSI_LINE);

    gpio_config(&io_conf);
}

void SPI_master::TransmitThread(void* pvParameters)
{
    auto inst = static_cast<SPI_master*>(pvParameters);
    inst->TrasmitThread_routine();
}

void SPI_master::TrasmitThread_routine()
{
    while(!stop_thread)
    {
        while(!TX_queue.empty() || Slave_Sending)
        {
            if(Spi_Id == VSPI_HOST) gpio_set_level((gpio_num_t)VSPI_HANDSHAKE_MOSI_LINE , 1);  
            if(Spi_Id == HSPI_HOST) gpio_set_level((gpio_num_t)HSPI_HANDSHAKE_MOSI_LINE , 1);

            if(RX_queue.size() >= MASTER_RX_QUEUE_SIZE)
            {
                RX_queue.pop();
                ESP_LOGW(SPI_Tag,"Dropping last RX_Buf from the queue");
            }

            spi_transaction_t t = {};
            t.user = this;
            t.length = BUFFSIZE * 8;

            t.rx_buffer = RX_queue.back();
            RX_queue.push();

            if(Slave_Sending)
            {
                t.tx_buffer = Clear_Buffer.GetPointer();
            }
            else
            {
                t.tx_buffer = TX_queue.front();
            }

            
            transaction_ongoing = true;
            xSemaphoreTake(rdysem, portMAX_DELAY); //Wait for Slave to be ready
            spi_device_transmit(SPI_Handle , &t);
            transaction_ongoing = false;

            if(!Slave_Sending) TX_queue.pop();
            Slave_Sending = false;

            if(Spi_Id == VSPI_HOST) gpio_set_level((gpio_num_t)VSPI_HANDSHAKE_MOSI_LINE , 0);  
            if(Spi_Id == HSPI_HOST) gpio_set_level((gpio_num_t)HSPI_HANDSHAKE_MOSI_LINE , 0);
        }
        
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

SPI_master::~SPI_master()
{
    if(spi_bus_remove_device(SPI_Handle) != ESP_OK)
    {
        ESP_LOGE(SPI_Tag , "Unable to Free Master SPI device");
    }
    spi_bus_free(Spi_Id);
    stop_thread = true;
    vTaskDelay(10 / portTICK_PERIOD_MS);
    vTaskDelete(Thread);
}

void SPI_master::VSPI_GPIO_CALLBACK(void* arg)
{
    static uint32_t lasthandshaketimeVSPI_us;
    uint32_t currtime_us = esp_timer_get_time();
    uint32_t diff = currtime_us - lasthandshaketimeVSPI_us;
    if (diff < 1000) {
        return; //ignore everything <1ms after an earlier irq
    }
    lasthandshaketimeVSPI_us = currtime_us;

    auto inst = static_cast<SPI_master*>(arg);
    inst->GPIO_routine();
}

void SPI_master::HSPI_GPIO_CALLBACK(void* arg)
{
    static uint32_t lasthandshaketimeHSPI_us;
    uint32_t currtime_us = esp_timer_get_time();
    uint32_t diff = currtime_us - lasthandshaketimeHSPI_us;
    if (diff < 1000) {
        return; //ignore everything <1ms after an earlier irq
    }
    lasthandshaketimeHSPI_us = currtime_us;

    auto inst = static_cast<SPI_master*>(arg);
    inst->GPIO_routine();
}

void SPI_master::GPIO_routine()
{
    if(TX_queue.empty()) Slave_Sending = true;

    BaseType_t mustYield = false;
    xSemaphoreGiveFromISR(rdysem, &mustYield);
    if (mustYield) {
        portYIELD_FROM_ISR();
    }
}

void SPI_master::SPI_LockBus()
{
    spi_device_acquire_bus(SPI_Handle , portMAX_DELAY);
}

void SPI_master::SPI_UnLockBus()
{
    spi_device_release_bus(SPI_Handle);
}

bool SPI_master::GetLastRecivedMessage(DMASmartPointer<uint8_t>& smt_ptr)
{
    if(RX_queue.empty()) return false;
    if(transaction_ongoing && RX_queue.size() == 1) return false;

    memcpy(smt_ptr.GetPointer() , RX_queue.front() , BUFFSIZE);
    RX_queue.pop();

    return true;
}

bool SPI_master::PutMessageOnTXQueue(const DMASmartPointer<uint8_t>& smt_ptr , size_t size)
{
    if(smt_ptr.GetPointer() == nullptr) return false;
    if(TX_queue.size() >= MASTER_TX_QUEUE_SIZE) return false;

    TX_queue.push(smt_ptr.GetPointer() , size);
    
    return true;
}

