# Azure IoT Hub Communication Skills

## Overview
Real-time Azure IoT Hub connectivity monitoring and telemetry tracking for cloud-based industrial data management.

## Capabilities

### Connection Status
- **Connection State**: Connected/Disconnected/Connecting
- **Connection Uptime**: Duration of current connection
- **Reconnection Attempts**: Count of connection retries
- **Last Connection Time**: Timestamp of successful connection
- **SAS Token Status**: Token validity and expiration

### Telemetry Statistics
- **Messages Sent**: Total telemetry messages transmitted
- **Messages Acknowledged**: Successfully delivered messages
- **Messages Failed**: Failed transmission attempts
- **Delivery Rate**: Percentage of successful deliveries
- **Last Telemetry Time**: Timestamp of most recent transmission
- **Data Volume**: Total bytes transmitted

### Cloud-to-Device
- **Commands Received**: Count of C2D messages
- **Last Command Time**: Timestamp of last received command
- **Command Queue**: Pending commands

### Error Diagnostics
- **Authentication Failures**: SAS token or certificate issues
- **Network Timeouts**: Connection timeout errors
- **MQTT Errors**: Protocol-level failures
- **Throttling Events**: Rate limit exceeded

## API Endpoints

### GET /api/azure/status
Returns current Azure IoT Hub connection status.

```json
{
  "connection_state": "connected",
  "connection_uptime": 3600,
  "messages_sent": 450,
  "messages_acked": 448,
  "messages_failed": 2,
  "delivery_rate": 99.6,
  "last_telemetry_time": "2025-11-17T19:30:00Z",
  "reconnect_attempts": 1,
  "sas_token_expires": "2025-11-17T20:30:00Z",
  "bytes_transmitted": 125000
}
```

### GET /api/azure/telemetry
Returns recent telemetry transmission history.

```json
{
  "recent_messages": [
    {
      "timestamp": "2025-11-17T19:30:00Z",
      "sensors_included": 2,
      "payload_size": 256,
      "status": "delivered",
      "latency_ms": 145
    }
  ],
  "hourly_stats": {
    "messages": 30,
    "success_rate": 100,
    "avg_latency_ms": 150
  }
}
```

### GET /api/azure/config
Returns current Azure IoT Hub configuration (masked sensitive data).

```json
{
  "hub_name": "fluxgen-testhub",
  "device_id": "testing_3",
  "telemetry_interval": 120,
  "qos_level": 1,
  "keep_alive": 60
}
```

## Real-time Monitoring
- Auto-refresh every 10 seconds
- Connection health indicators
- Telemetry transmission timeline
- Alert notifications for disconnections
- SAS token expiration warnings

## Security Features
- TLS 1.2 encrypted communication
- SAS token authentication
- Certificate validation
- Secure credential storage in NVS

## Integration Points
- Web UI dashboard widgets
- System Monitor page
- Azure configuration management
- Diagnostic troubleshooting
- Performance analytics
