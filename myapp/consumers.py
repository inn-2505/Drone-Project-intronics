import json
from channels.generic.websocket import AsyncWebsocketConsumer
from channels.db import database_sync_to_async
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

        if command:
            # Save the command to the database
            await self.save_command(command)
        
        # send the command to the ESP32 via WebSocket
        await self.channel_layer.group_send(
            self.group_name,
            {
                'type': 'drone_command_message',
                'command': command
            }
        )

    # send data to ESP32
    async def drone_command_message(self, event):
        command = event['command']
        await self.send(text_data=json.dumps({
            'command': command,
            'type': 'LIVE_COMMAND'
        }))

    @database_sync_to_async
    def save_command(self, command_text):
        # บันทึกคำสั่งลงตาราง drone_f722_command โดยกำหนดสถานะเริ่มต้นเป็น PENDING (หรือ SENT ตามต้องการ)
        return DroneCommand.objects.create(command=command_text, status="PENDING")