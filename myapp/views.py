import json
import socket
from django.shortcuts import redirect, render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.core.cache import cache
from myapp.models import DroneCommand, DroneStatus

ESP32_UDP_PORT = 1234

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
            # ดึง IP ของ ESP32 ที่ส่ง POST เข้ามา และเก็บลง Cache 5 นาที (300 วินาที)
            esp_ip = request.META.get('REMOTE_ADDR')
            cache.set('esp32_ip', esp_ip, timeout=300)
            
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

@csrf_exempt  # receive command from dashboard and send to ESP32 via UDP
def send_udp_command(request):
    if request.method == 'POST':
        try:
            body = json.loads(request.body)
            command_text = body.get('command')
            
            if not command_text:
                return JsonResponse({'status': 'error', 'message': 'Missing command'}, status=400)
            
            # 1. บันทึกคำสั่งควบคุมลง PostgreSQL
            DroneCommand.objects.create(command=command_text, status="PENDING")
            
            # 2. ดึงหมายเลข IP ล่าสุดของ ESP32 ที่เราจำไว้จากใน Cache
            esp_ip = cache.get('esp32_ip')
            
            if not esp_ip:
                return JsonResponse({
                    'status': 'error', 
                    'message': 'Cannot send UDP. ESP32 IP not found in cache. Please wait for drone telemetry first.'
                }, status=400)
            
            # 3. เริ่มกระบวนการยิง UDP ข้ามระบบเน็ตเวิร์กหาตัวบอร์ด
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            payload = json.dumps({
                "command": command_text,
                "type": "LIVE_COMMAND"
            })
            
            sock.sendto(payload.encode('utf-8'), (esp_ip, ESP32_UDP_PORT))
            sock.close()
            
            return JsonResponse({
                'status': 'success', 
                'message': f'UDP Command [{command_text}] fired to {esp_ip}:{ESP32_UDP_PORT} successfully!'
            })
            
        except Exception as e:
            return JsonResponse({
                'status': 'error', 
                'message': str(e)
            }, status=500)
            
    return JsonResponse({'status': 'error', 'message': 'Only POST method is allowed.'}, status=405)