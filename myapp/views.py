from urllib import request

from django.shortcuts import redirect, render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from myapp.models import DroneCommand, DroneStatus
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
            flight_mode = data.get('flight_mode')
            latitude = data.get('latitude')
            longitude = data.get('longitude')
            altitude = data.get('altitude')
            speed = data.get('speed')
            roll = data.get('roll')
            pitch = data.get('pitch')
            yaw = data.get('yaw')
            battery_voltage = data.get('battery_voltage')
            battery_percentage = data.get('battery_percentage')

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
