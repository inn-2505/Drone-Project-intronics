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

# def receive_data(request):
#     if request.method == 'GET':
#         try:
#             latest_status = DroneStatus.objects.first()
            
#             if latest_status:
#                 return JsonResponse({
#                     'status': 'success',
#                     'latitude': float(latest_status.latitude) if latest_status.latitude else None,
#                     'longitude': float(latest_status.longitude) if latest_status.longitude else None,
#                     'flight_mode': latest_status.flight_mode,
#                     'altitude': float(latest_status.altitude)
#                 }, status=200)
#             else:
#                 return JsonResponse({
#                     'status': 'empty',
#                     'message': 'No drone data available yet.'
#                 }, status=200)
                
#         except Exception as e:
#             return JsonResponse({'status': 'error',
#                                  'message': str(e)},
#                                  status=500)

#     elif request.method == 'POST':
#         try:
#             # store ESP32 IP in cache for 5 minutes
#             esp_ip = request.META.get('REMOTE_ADDR')
#             cache.set('esp32_ip', esp_ip, timeout=300)
            
#             # read JSON from ESP32
#             data = json.loads(request.body)
            
#             # extract values from JSON
#             flight_mode = data.get('flight_mode', 'DISARM') or 'DISARM'
#             latitude = data.get('latitude')
#             longitude = data.get('longitude')
#             altitude = data.get('altitude') if data.get('altitude') is not None else 0.00
#             speed = data.get('speed') if data.get('speed') is not None else 0.00
#             battery_voltage = data.get('battery_voltage') if data.get('battery_voltage') is not None else 0.00
#             battery_percentage = data.get('battery_percentage') if data.get('battery_percentage') is not None else 100

#             # insert into PostgreSQL
#             new_data = DroneStatus.objects.create(
#                 flight_mode=flight_mode,
#                 latitude=latitude,
#                 longitude=longitude,
#                 altitude=altitude,
#                 speed=speed,
#                 battery_voltage=battery_voltage,
#                 battery_percentage=battery_percentage
#             )

#             # send real-time telemetry data to WebSocket group            
#             channel_layer = get_channel_layer()
#             async_to_sync(channel_layer.group_send)(
#                 "drone_f722_control",
#                 {
#                     "type": "drone_telemetry_message",
#                     "data": {
#                         "flight_mode": flight_mode,
#                         "latitude": str(latitude) if latitude is not None else None,
#                         "longitude": str(longitude) if longitude is not None else None,
#                         "altitude": str(altitude),
#                         "speed": str(speed),
#                         "battery_voltage": str(battery_voltage),
#                         "battery_percentage": battery_percentage,
#                         "timestamp": new_data.timestamp.strftime("%d/%m/%Y %H:%M:%S")
#                     }
#                 }
#             )

#             return JsonResponse({
#                 'status': 'success', 
#                 'message': 'Real-time telemetry data inserted successfully.'
#             }, status=201)
        
#         except Exception as e:
#             print(traceback.format_exc())
#             return JsonResponse({
#                 'status': 'error', 
#                 'message': str(e)
#             }, status=400)
        
#     return JsonResponse({
#         'status': 'failed', 
#         'message': 'Only POST method is allowed.'
#     }, status=405)

@csrf_exempt  # receive command from dashboard/control and send to ESP32 via UDP
def send_udp_command(request):
    if request.method == 'POST':
        try:
            body = json.loads(request.body)
            command_text = body.get('command')
            
            lat1 = body.get('lat1')
            lon1 = body.get('lon1')
            lat2 = body.get('lat2')
            lon2 = body.get('lon2')
            alt = body.get('alt')
            spd = body.get('spd')
            throttle = body.get('throttle')
            yaw = body.get('yaw')
            pitch = body.get('pitch')
            roll = body.get('roll')

            if not command_text:
                return JsonResponse({'status': 'error', 'message': 'Missing command'}, status=400)

            # 🌟 ป้องกันบั๊ก: เช็กก่อนแปลงเป็น float เพราะถ้าส่งมาจากหน้า Control ค่าจะเป็น None
            db_lat1 = float(lat1) if lat1 is not None else None
            db_lon1 = float(lon1) if lon1 is not None else None
            db_lat2 = float(lat2) if lat2 is not None else None
            db_lon2 = float(lon2) if lon2 is not None else None
            
            # save command to PostgreSQL for logging (ใช้ค่าที่ปลอดภัยจากบั๊กแล้ว)
            DroneCommand.objects.create(
                command=command_text,
                lat1=db_lat1,
                lon1=db_lon1,
                lat2=db_lat2,
                lon2=db_lon2,
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
                "lat1": db_lat1,
                "lon1": db_lon1,
                "lat2": db_lat2,
                "lon2": db_lon2,
                "alt": float(alt) if alt is not None else None,
                "spd": float(spd) if spd is not None else None,
                "throttle": float(throttle) if throttle is not None else None,
                "yaw": float(yaw) if yaw is not None else None,
                "pitch": float(pitch) if pitch is not None else None,
                "roll": float(roll) if roll is not None else None
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
                
            data = json.loads(request.body)
            command = data.get('command', 'DISARM')
            lat1 = data.get('lat1', 0.0)
            lon1 = data.get('lon1', 0.0)
            lat2 = data.get('lat2', 0.0)
            lon2 = data.get('lon2', 0.0)
            alt = data.get('alt', 0.0)
            spd = data.get('spd', 0.0)
            throttle = data.get('throttle', 0)
            yaw = data.get('yaw', 0)
            pitch = data.get('pitch', 0)
            roll = data.get('roll', 0)
            print(f"[{command}] Lat1:{lat1} Lon1:{lon1} Lat2:{lat2} Lon2:{lon2} Alt:{alt} Spd:{spd} | Joy -> Throttle:{throttle} Yaw:{yaw}")
            # (โค้ดบันทึก PostgreSQL และส่ง UDP ของคุณตามปกติ...)
            
            esp_ip = cache.get('esp32_ip')
            if not esp_ip:
                return JsonResponse({'status': 'error', 'message': 'ESP32 IP not found'}, status=400)
                
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            payload = json.dumps(
                {
                    "command": command,
                    "lat1": float(lat1),
                    "lon1": float(lon1),
                    "lat2": float(lat2),
                    "lon2": float(lon2),
                    "alt": float(alt),
                    "spd": float(spd),
                    "throttle": int(throttle),
                    "yaw": int(yaw),
                    "pitch": int(pitch),
                    "roll": int(roll)
                }
            )
            sock.sendto(payload.encode('utf-8'), (esp_ip, ESP32_UDP_PORT))
            sock.close()
            
            return JsonResponse({'status': 'success'})
        except Exception as e:
            return JsonResponse({'status': 'error', 'message': str(e)}, status=500)