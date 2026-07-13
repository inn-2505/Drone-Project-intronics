import json
import socket
import asyncio
from channels.generic.websocket import AsyncWebsocketConsumer
from channels.db import database_sync_to_async
from django.core.cache import cache
from myapp.models import DroneCommand

class DroneConsumer(AsyncWebsocketConsumer):
    async def connect(self):
        self.group_name = "drone_f722_control"
        await self.channel_layer.group_add(self.group_name, self.channel_name)
        await self.accept()

    async def disconnect(self, close_code):
        await self.channel_layer.group_discard(self.group_name, self.channel_name)

    # receive data from dashboard
    async def receive(self, text_data):
        data = json.loads(text_data)
        command = data.get('command')
        latitude = data.get('latitude')
        longitude = data.get('longitude')
        if command:
            # Save the command to the database
            await self.save_command(command)
            # send the command to ESP32 via UDP
            await self.send_udp_packet(latitude, longitude)

    # send UDP packet to ESP32
    async def send_udp_packet(self, latitude, longitude):
        ESP32_IP = cache.get('esp32_ip') or "192.168.1.198"
        ESP32_PORT = 1234    
        
        # send UDP message to ESP32
        def send_udp():
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # SOCK_DGRAM = UDP
            message = json.dumps({
                'latitude': latitude,
                'longitude': longitude
            }).encode('utf-8')
            sock.sendto(message, (ESP32_IP, ESP32_PORT))
            sock.close()
            
        loop = asyncio.get_event_loop()
        await loop.run_in_executor(None, send_udp)    

    async def drone_telemetry_message(self, event):
        data = event['data']
        await self.send(text_data=json.dumps({
            'type': 'TELEMETRY',
            'data': data
        }))    

    async def drone_mode_message(self, event):
        data = event['data']
        await self.send(text_data=json.dumps({
            'type': 'MODE_CHANGE',
            'data': data
        }))
        
    @database_sync_to_async
    def save_command(self, command_text):
        # บันทึกคำสั่งลงตาราง drone_f722_command โดยกำหนดสถานะเริ่มต้นเป็น PENDING (หรือ SENT ตามต้องการ)
        return DroneCommand.objects.create(command=command_text, status="PENDING")