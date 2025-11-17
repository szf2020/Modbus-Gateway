# FluxGen IoT Gateway Skills

This folder contains documentation for the real-time monitoring capabilities integrated into the FluxGen ESP32 Modbus IoT Gateway web interface.

## Available Skills

### 1. Modbus Communication (`modbus.md`)
Real-time monitoring of RS485 Modbus RTU communication statistics including:
- Success/failure rates
- CRC and timeout errors
- Sensor health monitoring
- API endpoint: `/api/modbus/status`

### 2. Azure IoT Hub (`azure_iothub.md`)
Real-time monitoring of cloud connectivity and telemetry transmission:
- Connection state and uptime
- Message delivery statistics
- Device authentication status
- API endpoint: `/api/azure/status`

## Web UI Integration

The System Monitor page now includes live status updates for both Modbus and Azure IoT Hub. Status is automatically refreshed every 5 seconds.

### Color-coded Status Indicators
- **Green (status-good)**: Excellent performance (>95% success rate, connected)
- **Orange (status-warning)**: Degraded performance (80-95% success rate)
- **Red (status-error)**: Poor performance (<80% success rate, disconnected)

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/modbus/status` | GET | Modbus communication statistics |
| `/api/azure/status` | GET | Azure IoT Hub connection status |

## Implementation Details

- Statistics are tracked globally in main.c and modbus.c
- External variables are accessible via extern declarations
- JSON responses use cJSON for safe serialization
- Auto-refresh interval: 5 seconds (configurable in JavaScript)

## Future Enhancements

1. Historical data logging and trend analysis
2. Alert notifications via email/SMS
3. Export statistics to CSV/JSON
4. Integration with cloud dashboards
5. Predictive maintenance indicators
