// ============================================
// Global State Variables (ประกาศตัวแปรส่วนกลาง)
// ============================================
let currentDroneLatLng = [13.7563, 100.5018]; // พิกัดเริ่มต้น (Bangkok)
let activeTargetLatLng = null; 
let displacementLine = null; 
let map;
let droneMarker;
let targetMarker = null; // เพิ่มตัวแปรเก็บหมุดเป้าหมายสีแดงที่ลากได้
// รายชื่อหมุดสถานที่ท่องเที่ยวเริ่มต้น
const locations = [
    { name: "Bangkok", lat: 13.7563, lon: 100.5018 },
    { name: "Chiang Mai", lat: 18.7883, lon: 98.9853 },
    { name: "Phuket", lat: 7.8804, lon: 98.3923 },
    { name: "Pattaya", lat: 12.9236, lon: 100.8825 }
];

// รูป Icon โดรน
const droneIcon = L.icon({
    iconUrl: 'https://cdn-icons-png.flaticon.com/128/4056/4056808.png', 
    iconSize: [40, 40],        
    iconAnchor: [20, 20],      
    popupAnchor: [0, -20]    
});

function createPinIcon(color, size = 36) {
    const svg = `
        <svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 24 24">
            <path fill="${color}" stroke="#7a0000" stroke-width="0.6"
                d="M12 0C7.6 0 4 3.6 4 8c0 5.4 7 15.6 7.3 16a1 1 0 0 0 1.4 0C13 23.6 20 13.4 20 8c0-4.4-3.6-8-8-8z"/>
            <circle cx="12" cy="8" r="3.2" fill="#ffffff"/>
        </svg>`;
    return L.divIcon({
        html: svg,
        className: '', // กันไม่ให้ Leaflet ใส่ background/border ของ divIcon ปกติมาทับ
        iconSize: [size, size],
        iconAnchor: [size / 2, size],
        popupAnchor: [0, -size]
    });
}
const targetIcon = createPinIcon('#e48787');  
const targetConfirmedIcon = createPinIcon('#c60000');   
// ============================================
// Initialize Leaflet Map (ทำงานเมื่อ HTML พร้อมโหลด)
// ============================================
document.addEventListener("DOMContentLoaded", function() {
    // 1. สร้างแผนที่และผูกเข้ากับ <div id="drone-map">
    map = L.map('drone-map').setView(currentDroneLatLng, 13);
    
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© OpenStreetMap contributors'
    }).addTo(map);

    // 2. ปักหมุดตัวโดรนเริ่มต้น
    // droneMarker = L.marker(currentDroneLatLng, { icon: droneIcon }).addTo(map)
    //     .bindPopup('🛸 Drone F722')
    //     .openPopup();
    droneMarker = L.marker(currentDroneLatLng, { icon: droneIcon }).addTo(map);
    // 🛠️ พอลากปล่อยมือ ให้อัปเดตตำแหน่งโดรนปัจจุบัน (currentDroneLatLng) ตามจุดใหม่ที่ลากไป
    // เพื่อให้คำสั่ง ARM/GOTO ครั้งต่อไปอ้างอิงตำแหน่งที่ลากไว้ ไม่ใช่ตำแหน่งเดิม
    droneMarker.on('dragend', function(event) {
        const position = event.target.getLatLng();
        currentDroneLatLng = [position.lat, position.lng];

        // อัปเดตเส้นประให้ลากตามตำแหน่งโดรนใหม่ ถ้ามีเป้าหมายตั้งไว้อยู่
        if (activeTargetLatLng) {
            updateDisplacementLine(activeTargetLatLng);
        }
    });
    
    // 3. ปักหมุดสถานที่ท่องเที่ยวต่างๆ พร้อมปุ่มกดส่งค่าพิกัด
    // locations.forEach(loc => {
    //     const marker = L.marker([loc.lat, loc.lon]).addTo(map);
        
    //     const popupContent = document.createElement('div');
    //     popupContent.style.textAlign = 'center';
    //     popupContent.innerHTML = `
    //         <b style="color: #1a1a2e;">📍 ${loc.name}</b><br>
    //         <button class="btn btn-blue send-coord-btn" 
    //                 style="padding: 4px 8px; margin-top: 5px; font-size: 0.8rem; cursor: pointer;">
    //             Send Coordinates
    //         </button>
    //     `;
        
    //     popupContent.querySelector('.send-coord-btn').addEventListener('click', () => {
    //         //triggerTargetCommand(loc.lat, loc.lon);
    //         moveOrActionTargetMarker(loc.lat, loc.lon);
    //         if (map) map.closePopup();
    //     });

    //     marker.bindPopup(popupContent);
    // });
    // แก้ไขเปลี่ยนมาผูกป๊อปอัป "Send Coordinates" ไว้ที่ตัวโดรนโดยตรง
    const dronePopupContent = document.createElement('div');
    dronePopupContent.style.textAlign = 'center';
    dronePopupContent.innerHTML = `
        <b style="color: #1a1a2e;">🛸 Drone F722</b><br>
        <button class="btn btn-blue spawn-target-btn" 
                style="padding: 6px 10px; margin-top: 5px; font-size: 0.8rem; cursor: pointer; background-color: #00f2fe; color: #1a1a2e; border: none; border-radius: 4px; font-weight: bold;">
            Send Coordinates
        </button>
    `;
    // เมื่อกดปุ่มที่ตัวโดรน ให้เสกหมุดเป้าหมายสีแดงขึ้นมาให้ลาก
    dronePopupContent.querySelector('.spawn-target-btn').addEventListener('click', () => {
        // เสกหมุดแดงขึ้นมาตรงตำแหน่งที่โดรนอยู่ปัจจุบัน ณ ตอนนั้น เพื่อเริ่มลาก
        moveOrActionTargetMarker(currentDroneLatLng[0], currentDroneLatLng[1]);
        if (droneMarker) droneMarker.closePopup();
    });
    droneMarker.bindPopup(dronePopupContent).openPopup();

    // 3. ปักหมุดสถานที่ท่องเที่ยวต่างๆ (คงไว้เป็นข้อมูลอ้างอิงเฉยๆ ตามโค้ดเดิม)
    locations.forEach(loc => {
        L.marker([loc.lat, loc.lon]).addTo(map).bindPopup(`<b>📍 ${loc.name}</b>`);
    });
});

// ============================================
// Core Functions (ฟังก์ชันระบบควบคุมแผนที่และคำสั่ง)
// ============================================

//  "หมุดเป้าหมายสีแดงที่ลากได้" (สร้างใหม่ขึ้นมาคู่สาย)
function moveOrActionTargetMarker(lat, lon) {
    if (!map) return;

    activeTargetLatLng = [lat, lon];
    updateDisplacementLine(activeTargetLatLng);

    // สร้างกล่องข้อความที่มีปุ่มกดยืนยันการบินไป (Confirm Fly To)
    const popupDiv = document.createElement('div');
    popupDiv.style.textAlign = 'center';
    popupDiv.innerHTML = `
        <b style="color: #ff4a4a;">🎯 Target Position</b><br>
        <span style="font-size: 0.75rem; color:#666;">Lat: ${lat.toFixed(6)}<br>Lng: ${lon.toFixed(6)}</span><br>
        <button class="btn btn-red confirm-fly-btn" 
                style="padding: 6px 10px; margin-top: 6px; font-size: 0.8rem; background-color: #ff4a4a; color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold;">
            Confirm Target
        </button>
    `;

    popupDiv.querySelector('.confirm-fly-btn').addEventListener('click', () => {
        triggerTargetCommand(currentDroneLatLng[0], currentDroneLatLng[1], activeTargetLatLng[0], activeTargetLatLng[1]);
    });

    if (!targetMarker) {
        targetMarker = L.marker(activeTargetLatLng, { icon: targetIcon, draggable: true }).addTo(map);

        // ดักฟังตอน "เริ่มลาก" ให้ปิดป๊อปอัปก่อน
        targetMarker.on('dragstart', () => {
            targetMarker.closePopup();
        });

        // ดักฟังตอน "ลากเสร็จปล่อยมือ" (dragend) เพื่อคำนวณเส้นประกับพิกัดใหม่
        targetMarker.on('dragend', function(event) {
            const marker = event.target;
            const position = marker.getLatLng();
            
            // 🛠️ ลากไปตำแหน่งใหม่ = ยังไม่ confirm ส่ง จึงรีเซ็ต icon กลับเป็นสีแดงปกติ (ยังไม่ยืนยัน)
            marker.setIcon(targetIcon);
            
            // เรียกฟังก์ชันตัวเองวนกลับมาลูปซ้ำเพื่ออัปเดตเส้นและผูกปุ่มป๊อปอัปพิกัดใหม่
            moveOrActionTargetMarker(position.lat, position.lng);
        });
        } else {
            // ถ้ามีหมุดอยู่แล้วบนแผนที่ ให้ย้ายพิกัดมันไปจุดล่าสุด
            targetMarker.setLatLng(activeTargetLatLng);
            // 🛠️ เป้าหมายใหม่ = ยังไม่ confirm ส่ง จึงรีเซ็ต icon กลับเป็นสีแดงปกติ
            targetMarker.setIcon(targetIcon);
            // ผูกป๊อปอัปใหม่เข้าไปแล้วสั่งเปิดค้างไว้ นักบินจะได้กดสั่งบินได้สะดวก
        }
        targetMarker.bindPopup(popupDiv).openPopup();
    }
// ฟังก์ชันดึงหน้าจอกลับมาโฟกัสที่โดรน
function recenterMap() {
    if (!map) return; 
    map.setView(currentDroneLatLng, map.getZoom());
}

// ฟังก์ชันวาดเส้นประเชื่อมโยงจากโดรนไปยังจุดเป้าหมาย
function updateDisplacementLine(targetLatLng) {
    if (!map) return; 
    if (displacementLine) {
        map.removeLayer(displacementLine);
    }
    
    displacementLine = L.polyline([currentDroneLatLng, targetLatLng], {
        color: '#00f2fe',
        weight: 3,
        dashArray: '10, 10',
        opacity: 0.8
    }).addTo(map);
}

// ฟังก์ชันเมื่อกดปุ่มส่งพิกัดบนแผนที่
const DEFAULT_SAFE_ALT = 30; // เมตร
const DEFAULT_SAFE_SPD = 5;  // m/s

function triggerTargetCommand(droneLat, droneLon, targetLat, targetLon) {
    sendUdpCommand('ARM', droneLat, droneLon, targetLat, targetLon);
    if (targetMarker) {
        targetMarker.setIcon(targetConfirmedIcon);
        targetMarker.closePopup();
    }
}

// ฟังก์ชันยิง HTTP POST คำสั่งไปยัง Django Backend
function sendUdpCommand(cmd, lat1 = null, lon1 = null, lat2 = null, lon2 = null) {
    const payload = {
        command: cmd,
        lat1: lat1,
        lon1: lon1,
        lat2: lat2,
        lon2: lon2,
        alt: DEFAULT_SAFE_ALT,
        spd: DEFAULT_SAFE_SPD,
        throttle: 0,
        yaw: 0,
        pitch: 0,
        roll: 0
    };

    fetch('/api/command/', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',                
        },
        body: JSON.stringify(payload)
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.message); });
        }
        return response.json();
    })
    .then(data => {
        console.log('Success:', data);
        const lastCmdEl = document.getElementById('lastCmdText');
        if (lastCmdEl) lastCmdEl.innerText = cmd;
    })
    .catch(error => {
        console.error('Error:', error);
        alert('Failed to send command: ' + error.message);
    });
}

// ฟังก์ชันสร้างตารางสรุปสถานะโดรนแบบ Real-time
function updateLogTable(data) {
    const tbody = document.querySelector('table tbody');
    if (!tbody) return;
    
    const tr = document.createElement('tr');
    tr.innerHTML = `
        <td>${data.timestamp}</td>
        <td><span class="badge ${data.flight_mode}">${data.flight_mode}</span></td>
        <td style="color: #00f2fe; font-weight: bold;">${data.altitude} m</td>
        <td>${data.speed} m/s</td>
        <td>${data.latitude}, ${data.longitude}</td>
        <td>${data.battery_percentage}% (${data.battery_voltage}V)</td>
    `;
    
    tbody.insertBefore(tr, tbody.firstChild);
    
    while (tbody.rows.length > 10) {
        tbody.deleteRow(tbody.rows.length - 1);
    }
}

// ============================================
// Event Listener (รับสตรีมข้อมูล Telemetry จาก WebSocket)
// ============================================
document.addEventListener('droneSocketMessage', function (e) {
    const msg = e.detail;
 
    if (msg.type === 'TELEMETRY') {
        const data = msg.data;
 
        const lat = parseFloat(data.latitude);
        const lon = parseFloat(data.longitude);
 
        if (!isNaN(lat) && !isNaN(lon)) {
            currentDroneLatLng = [lat, lon];
            
            // ขยับหมุดโดรนบนหน้าจอตามค่าจริง
            if (droneMarker) {
                droneMarker.setLatLng(currentDroneLatLng);
            }
            
            // อัปเดตเส้นประขยับตามตัวโดรนไปหาเป้าหมาย
            if (activeTargetLatLng) {
                updateDisplacementLine(activeTargetLatLng);
            }
        }
 
        // เพิ่มข้อมูลลงในตาราง Log
        updateLogTable(data);
    }
});