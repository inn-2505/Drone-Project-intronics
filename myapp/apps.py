import os
import threading
from django.apps import AppConfig

class MyappConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'myapp'

    def ready(self):
        if os.environ.get('RUN_MAIN') == 'true' or not os.environ.get('DJANGO_SETTINGS_MODULE'):
            threading.Thread(target=self.start_udp_listener, daemon=True).start()

    def start_udp_listener(self):
        import socket
        import json
        from django.core.cache import cache
        from django.db import connection
        from myapp.models import DroneStatus
        from channels.layers import get_channel_layer
        from asgiref.sync import async_to_sync

        UDP_IP = "0.0.0.0"
        UDP_PORT = 5002  # Port for receiving telemetry data from the ESP32

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((UDP_IP, UDP_PORT))
        print(f"📡 [THREAD] UDP Telemetry Listener running on port {UDP_PORT}...")

        while True:
            try:
                data_bytes, addr = sock.recvfrom(1024)
                sender_ip = addr[0]
                cache.set('esp32_ip', sender_ip, timeout=300)

                data = json.loads(data_bytes.decode('utf-8'))
                
                flight_mode = data.get('flight_mode', 'DISARM') or 'DISARM'
                latitude = data.get('latitude')
                longitude = data.get('longitude')
                altitude = data.get('altitude') if data.get('altitude') is not None else 0.00
                speed = data.get('speed') if data.get('speed') is not None else 0.00
                battery_voltage = data.get('battery_voltage') if data.get('battery_voltage') is not None else 0.00
                battery_percentage = data.get('battery_percentage') if data.get('battery_percentage') is not None else 100

                new_data = DroneStatus.objects.create(
                    flight_mode=flight_mode,
                    latitude=latitude,
                    longitude=longitude,
                    altitude=altitude,
                    speed=speed,
                    battery_voltage=battery_voltage,
                    battery_percentage=battery_percentage
                )

                latest_ids = DroneStatus.objects.order_by('-timestamp', '-id')[:10].values_list('id', flat=True)
                DroneStatus.objects.exclude(id__in=list(latest_ids)).delete()

                channel_layer = get_channel_layer()
                if channel_layer:
                    async_to_sync(channel_layer.group_send)(
                        "drone_f722_control",
                        {
                            "type": "drone_telemetry_message",
                            "data": {
                                "flight_mode": flight_mode,
                                "latitude": str(latitude) if latitude is not None else None,
                                "longitude": str(longitude) if longitude is not None else None,
                                "altitude": str(altitude),
                                "speed": str(speed),
                                "battery_voltage": str(battery_voltage),
                                "battery_percentage": battery_percentage,
                                "timestamp": new_data.timestamp.strftime("%d/%m/%Y %H:%M:%S")
                            }
                        }
                    )
            except Exception as e:
                print(f"❌ [UDP Listener Thread Error]: {e}")
            finally:
                connection.close()