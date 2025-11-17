# Modbus Communication Skills

## Overview
Real-time Modbus RTU communication monitoring and diagnostics for industrial sensor data acquisition.

## Capabilities

### Communication Statistics
- **Total Reads**: Number of Modbus read operations performed
- **Successful Reads**: Count of successful data acquisitions
- **Failed Reads**: Count of communication errors
- **Success Rate**: Percentage of successful communications
- **Last Read Time**: Timestamp of most recent sensor read
- **CRC Errors**: Count of data integrity failures

### Sensor Status
- **Active Sensors**: Number of configured and responding sensors
- **Sensor Health**: Individual sensor communication status
- **Data Quality**: Validity of received sensor values
- **Response Time**: Average Modbus response latency

### Error Diagnostics
- **Timeout Errors**: No response from slave device
- **CRC Failures**: Data corruption during transmission
- **Exception Responses**: Modbus protocol errors
- **Invalid Data**: Out-of-range or malformed values

## API Endpoints

### GET /api/modbus/status
Returns current Modbus communication statistics.

```json
{
  "total_reads": 1250,
  "successful_reads": 1245,
  "failed_reads": 5,
  "success_rate": 99.6,
  "last_read_time": "2025-11-17T19:30:00Z",
  "crc_errors": 2,
  "timeout_errors": 3,
  "sensors_active": 2,
  "sensors_configured": 2
}
```

### GET /api/modbus/sensors
Returns detailed status for each configured sensor.

```json
{
  "sensors": [
    {
      "id": 1,
      "name": "Flow Meter",
      "slave_id": 1,
      "status": "active",
      "last_value": 125.5,
      "unit": "L/min",
      "last_read": "2025-11-17T19:30:00Z",
      "error_count": 0
    }
  ]
}
```

## Real-time Monitoring
- Auto-refresh every 5 seconds
- Visual indicators for sensor health
- Historical trend data
- Alert notifications for failures

## Integration Points
- Web UI dashboard widgets
- System Monitor page
- Sensor configuration validation
- Diagnostic troubleshooting
