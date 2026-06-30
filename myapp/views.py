from urllib import request

from django.shortcuts import redirect, render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from myapp.models import DroneCommand, DroneStatus
from asgiref.sync import async_to_sync
from channels.layers import get_channel_layer
import json

def send_command(request, cmd_text):
    # Create a new command entry in the database
    DroneCommand.objects.create(command=cmd_text, status="PENDING")
    return redirect('dashboard')

def get_command(request):
    # Fetch the latest pending command from the database
    pending_cmd = DroneCommand.objects.filter(status="PENDING").order_by('-timestamp').first()
    
    if pending_cmd:
        response_data = {
            "command": pending_cmd.command,
            "status": "NEW_COMMAND"
        }
        # Change status to SENT 
        pending_cmd.status = "SENT"
        pending_cmd.save()
    else:
        response_data = {
            "command": "NONE",
            "status": "NO_NEW_COMMAND"
        }
        
    return JsonResponse(response_data)

def dashboard(request):
    drone_logs = DroneStatus.objects.all()[:50]
    last_command = DroneCommand.objects.order_by('-timestamp').first()
    return render(request, 'drone_dashboard.html', {
        'drone_logs': drone_logs,
        'last_command': last_command
    })

@csrf_exempt # allow ESP32 to send data without CSRF token
def receive_data(request):
    if request.method == 'POST':
        try:
            # read JSON from ESP32
            data = json.loads(request.body)
            
            # extract values from JSON
            flight_mode = data.get('flight_mode', 'DISARM') or 'DISARM'
            latitude = data.get('latitude')
            longitude = data.get('longitude')
            altitude = data.get('altitude') if data.get('altitude') is not None else 0.00
            speed = data.get('speed') if data.get('speed') is not None else 0.00
            roll = data.get('roll') if data.get('roll') is not None else 0.00
            pitch = data.get('pitch') if data.get('pitch') is not None else 0.00
            yaw = data.get('yaw') if data.get('yaw') is not None else 0.00
            battery_voltage = data.get('battery_voltage') if data.get('battery_voltage') is not None else 0.00
            battery_percentage = data.get('battery_percentage') if data.get('battery_percentage') is not None else 100

            # insert into PostgreSQL
            new_data = DroneStatus.objects.create(
                flight_mode=flight_mode,
                latitude=latitude,
                longitude=longitude,
                altitude=altitude,
                speed=speed,
                roll=roll,
                pitch=pitch,
                yaw=yaw,
                battery_voltage=battery_voltage,
                battery_percentage=battery_percentage
            )
            
            # sent data to WebSocket
            channel_layer = get_channel_layer()
            
            # send the telemetry data to dashboard
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
                        "roll": str(roll),
                        "pitch": str(pitch),
                        "yaw": str(yaw),
                        "battery_voltage": str(battery_voltage),
                        "battery_percentage": battery_percentage,
                        "timestamp": new_data.timestamp.strftime("%H:%M:%S")
                    }
                }
            )

            return JsonResponse({
                'status': 'success', 
                'message': 'Real-time telemetry data inserted successfully.'
            }, status=201)
        
        except Exception as e:
            return JsonResponse({
                'status': 'error', 
                'message': str(e)
            }, status=400)
        
    return JsonResponse({
        'status': 'failed', 
        'message': 'Only POST method is allowed.'
    }, status=405)
