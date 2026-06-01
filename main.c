void spi_bus_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1, //No los uso
        .quadhd_io_num = -1
    }; 

    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1000000,
        .mode = 3,
        .spics_io_num = PIN_CS,
        .queue_size = 1
    };

    spi_bus_add_device(SPI2_HOST, &dev, &mcp4132);
}

esp_err_t mcp4132_write_register(uint8_t address, uint16_t data)
{
    uint8_t tx[2];

    data = data & 0x01FF;

    tx[0] = ((address & 0x0F) << 4) | ((MCP_WRITE & 0x03) << 2) | ((data >> 8) & 0x03);
    tx[1] = data & 0xFF;

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx
    };

    return spi_device_transmit(mcp4132, &t);
}

uint16_t mcp4132_read_register(uint8_t address)
{
    uint8_t tx[2] = {
        ((address & 0x0F) << 4) | ((MCP_READ & 0x03) << 2),
        0x00
    };

    uint8_t rx[2] = {0};

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx
    };

    spi_device_transmit(mcp4132, &t);

    return ((rx[0] & 0x01) << 8) | rx[1];
}

esp_err_t mcp4132_set_wiper(uint8_t N)
{
    if (N > 128) {
        return ESP_ERR_INVALID_ARG;
    }

    current_wiper = N;
    return mcp4132_write_register(MCP_WIPER0, N);
}

esp_err_t mcp4132_set_cutoff_frequency(float fc)
{
    if (fc <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    float rwb = 1.0f / (2.0f * 3.1416f * fc * C);
    int N = (int)(((rwb - RW) * 128.0f / RAB) + 0.5f);

    if (N < 0) {
        N = 0;
    }

    if (N > 128) {
        N = 128;
    }

    return mcp4132_set_wiper((uint8_t)N);
}

void app_main(void)
{
    char msg[100];

    spi_bus_init();

    mcp4132_set_wiper(64);

    while (1) {
        if (sample_flag) {
            sample_flag = false;

            int raw = 0;
            adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);

            int mv = (raw * 3300) / 4095;

            if (mv > 1400 && current_wiper != 95) {
                mcp4132_set_wiper(95);
                sprintf(msg, "Senal alta: %d mV, wiper N=95\r\n", mv);
                uart_write_bytes(UART_PORT, msg, strlen(msg));
            }

            if (mv < 800 && current_wiper != 42) {
                mcp4132_set_wiper(42);
                sprintf(msg, "Senal baja: %d mV, wiper N=42\r\n", mv);
                uart_write_bytes(UART_PORT, msg, strlen(msg));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
