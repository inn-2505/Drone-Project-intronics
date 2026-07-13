import json
import socket
import traceback

from django.shortcuts import redirect, render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.core.cache import cache
from myapp.models import DroneCommand, DroneStatus
from asgiref.sync import async_to_sync
from channels.layers import get_channel_layer

ESP32_UDP_PORT = 1234

def dashboard(request):
    drone_logs = DroneStatus.objects.all()[:10]
    last_command = DroneCommand.objects.order_by('-timestamp').first()
    return render(request, 'dashboard.html', {
        'drone_logs': drone_logs,
        'last_command': last_command
    })

def control_view(request):
    return render(request, 'control.html')

@csrf_exempt # allow ESP32 to send data without CSRF token
def receive_data(request):
    if request.method == 'GET':
        try:
            latest_status = DroneStatus.objects.first()
            
            if latest_status:
                return JsonResponse({
                    'status': 'success',
                    'latitude': float(latest_status.latitude) if latest_status.latitude else None,
                    'longitude': float(latest_status.longitude) if latest_status.longitude else None,
                    'flight_mode': latest_status.flight_mode,
                    'altitude': float(latest_status.altitude)
                }, status=200)
            else:
                return JsonResponse({
                    'status': 'empty',
                    'message': 'No drone data available yet.'
                }, status=200)
                
        except Exception as e:
            return JsonResponse({'status': 'error',
                                 'message': str(e)},
                                 status=500)

    elif request.method == 'POST':
        try:
            # store ESP32 IP in cache for 5 minutes
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
            battery_voltage = data.get('battery_voltage') if data.get('battery_voltage') is not None else 0.00
            battery_percentage = data.get('battery_percentage') if data.get('battery_percentage') is not None else 100

            # insert into PostgreSQL
            new_data = DroneStatus.objects.create(
                flight_mode=flight_mode,
                latitude=latitude,
                longitude=longitude,
                altitude=altitude,
                speed=speed,
                battery_voltage=battery_voltage,
                battery_percentage=battery_percentage
            )

            # send real-time telemetry data to WebSocket group            
            channel_layer = get_channel_layer()
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

            return JsonResponse({
                'status': 'success', 
                'message': 'Real-time telemetry data inserted successfully.'
            }, status=201)
        
        except Exception as e:
            print(traceback.format_exc())
            return JsonResponse({
                'status': 'error', 
                'message': str(e)
            }, status=400)
        
    return JsonResponse({
        'status': 'failed', 
        'message': 'Only POST method is allowed.'
    }, status=405)

@csrf_exempt  # receive command from dashboard/control and send to ESP32 via UDP
def send_udp_command(request):
    if request.method == 'POST':
        try:
            body = json.loads(request.body)
            command_text = body.get('command')
            
            latitude = body.get('latitude')
            longitude = body.get('longitude')
            alt = body.get('altitude')
            spd = body.get('speed')

            if not command_text:
                return JsonResponse({'status': 'error', 'message': 'Missing command'}, status=400)

            # 🌟 ป้องกันบั๊ก: เช็กก่อนแปลงเป็น float เพราะถ้าส่งมาจากหน้า Control ค่าจะเป็น None
            db_lat = float(latitude) if latitude is not None else None
            db_lon = float(longitude) if longitude is not None else None
            
            # save command to PostgreSQL for logging (ใช้ค่าที่ปลอดภัยจากบั๊กแล้ว)
            DroneCommand.objects.create(
                command=command_text,
                latitude=db_lat,
                longitude=db_lon,
            )
            
            # pull ESP32 IP from cache
            esp_ip = cache.get('esp32_ip')
            
            # 🌟 ตัวช่วยจำลองสถานะ: ถ้าเทสระบบแล้วแคช IP ยังไม่มา ให้ใช้ IP สำรอง โค้ดจะได้ไม่เด้งพัง
            if not esp_ip:
                esp_ip = "192.168.1.198" 
            
            # send UDP command to ESP32
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            
            # 🌟 เติมสิ่งที่ขาดหาย: แนบ "command" เข้าไปใน Payload UDP เพื่อให้ ESP32 เอาไปใช้สั่งงานมอเตอร์ได้
            payload = json.dumps({
                "command": command_text,
                "latitude": db_lat,
                "longitude": db_lon,
                "altitude": float(alt) if alt is not None else None,
                "speed": float(spd) if spd is not None else None
            })
            
            sock.sendto(payload.encode('utf-8'), (esp_ip, ESP32_UDP_PORT))
            sock.close()
            
            return JsonResponse({
                'status': 'success', 
                'message': f'UDP Command [{command_text}] fired to {esp_ip}:{ESP32_UDP_PORT} successfully!'
            })
            
        except Exception as e:
            # พ่น Error ลึกๆ ออกมาดูทางหน้าต่าง Terminal ของ Django เพื่อไล่บั๊กได้ง่ายขึ้น
            print("=== DJANGO COMMAND API ERROR ===")
            traceback.print_exc() 
            return JsonResponse({
                'status': 'error', 
                'message': str(e)
            }, status=500)
            
    return JsonResponse({'status': 'error', 'message': 'Only POST method is allowed.'}, status=405)

#  API สำหรับเปลี่ยนโหมดการบิน (สลับโหมดแมนนวล / อัตโนมัติ)
@csrf_exempt
def switch_mode_view(request):
    if request.method == 'GET':
        # ให้ค่าเริ่มต้นเป็น AUTO_GPS ถ้ายังไม่เคยตั้งค่าอะไรเลย (โหมดปลอดภัยสุด)
        current_mode = cache.get('current_flight_mode', 'AUTO_GPS')
        return JsonResponse({'status': 'success', 'current_mode': current_mode})
 
    if request.method == 'POST':
        try:
            body = json.loads(request.body)
            target_mode = body.get('mode') # รับค่า 'MANUAL' หรือ 'AUTO_GPS'
            
            if target_mode not in ['MANUAL', 'AUTO_GPS']:
                return JsonResponse({'status': 'error', 'message': 'Invalid mode'}, status=400)
            
            # บันทึกโหมดปัจจุบันลง Cache ค้างไว้ (นี่คือ single source of truth ของทุกหน้า)
            cache.set('current_flight_mode', target_mode, timeout=None)
            print(f"🔄 [FLIGHT MODE CHANGED TO]: {target_mode}")
            
            # 🚀 (ออปชันเสริม) ส่ง UDP ไปบอก ESP32 ด้วยว่าคนสั่งเปลี่ยนโหมดแล้วนะ
            esp_ip = cache.get('esp32_ip')
            if esp_ip:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                payload = json.dumps({"flight_mode": target_mode})
                sock.sendto(payload.encode('utf-8'), (esp_ip, ESP32_UDP_PORT))
                sock.close()
 
            # 📡 broadcast สถานะโหมดใหม่ไปยังทุกหน้าที่เปิด WebSocket อยู่
            # (ใช้ group เดียวกับ telemetry เพื่อไม่ต้องเปิด socket ซ้ำ)
            channel_layer = get_channel_layer()
            async_to_sync(channel_layer.group_send)(
                "drone_f722_control",
                {
                    "type": "drone_mode_message",
                    "data": {
                        "mode": target_mode
                    }
                }
            )
 
            return JsonResponse({'status': 'success', 'current_mode': target_mode})
        except Exception as e:
            return JsonResponse({'status': 'error', 'message': str(e)}, status=500)
 
    return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
 

# อัปเดตตัวควบคุมแมนนวลให้ "ฉลาดขึ้น" (เช็กโหมดก่อนส่งข้อมูล)
@csrf_exempt
def manual_control_view(request):
    if request.method == 'POST':
        try:
            # 🛑 เช็กความปลอดภัยอันดับแรก: ตอนนี้โดรนอยู่ในโหมด MANUAL หรือเปล่า?
            current_mode = cache.get('current_flight_mode', 'AUTO_GPS') # ถ้าไม่มีค่าในแคช ให้ล็อกเป็นโหมดปลอดภัย (AUTO) ไว้ก่อน
            if current_mode != 'MANUAL':
                return JsonResponse({
                    'status': 'blocked', 
                    'message': 'Manual control is disabled because the drone is not in MANUAL mode. Please switch to MANUAL mode first.'
                }, status=403)
                
            body = json.loads(request.body)
            throttle = body.get('throttle', 0)
            yaw = body.get('yaw', 0)
            pitch = body.get('pitch', 0)
            roll = body.get('roll', 0)
            #print(f"🎮 Manual RC Input -> T: {throttle}, Y: {yaw}, P: {pitch}, R: {roll}")
            # (โค้ดบันทึก PostgreSQL และส่ง UDP ของคุณตามปกติ...)
            
            esp_ip = cache.get('esp32_ip')
            if not esp_ip:
                return JsonResponse({'status': 'error', 'message': 'ESP32 IP not found'}, status=400)
                
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            payload = json.dumps({"throttle": throttle, "yaw": yaw, "pitch": pitch, "roll": roll})
            sock.sendto(payload.encode('utf-8'), (esp_ip, ESP32_UDP_PORT))
            sock.close()
            
            return JsonResponse({'status': 'success'})
        except Exception as e:
            return JsonResponse({'status': 'error', 'message': str(e)}, status=500)