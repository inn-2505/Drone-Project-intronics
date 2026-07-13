// ============================================
// Global State Variables (ประกาศตัวแปรส่วนกลาง)
// ============================================
let currentDroneLatLng = [13.7563, 100.5018]; // พิกัดเริ่มต้น (Bangkok)
let activeTargetLatLng = null; 
let displacementLine = null; 
let map;
let droneMarker;

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
    droneMarker = L.marker(currentDroneLatLng, { icon: droneIcon }).addTo(map)
        .bindPopup('🛸 Drone F722')
        .openPopup();

    // 3. ปักหมุดสถานที่ท่องเที่ยวต่างๆ พร้อมปุ่มกดส่งค่าพิกัด
    locations.forEach(loc => {
        const marker = L.marker([loc.lat, loc.lon]).addTo(map);
        
        const popupContent = document.createElement('div');
        popupContent.style.textAlign = 'center';
        popupContent.innerHTML = `
            <b style="color: #1a1a2e;">📍 ${loc.name}</b><br>
            <button class="btn btn-blue send-coord-btn" 
                    style="padding: 4px 8px; margin-top: 5px; font-size: 0.8rem; cursor: pointer;">
                Send Coordinates
            </button>
        `;
        
        popupContent.querySelector('.send-coord-btn').addEventListener('click', () => {
            triggerTargetCommand(loc.lat, loc.lon);
        });

        marker.bindPopup(popupContent);
    });
});

// ============================================
// Core Functions (ฟังก์ชันระบบควบคุมแผนที่และคำสั่ง)
// ============================================

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
function triggerTargetCommand(lat, lon) {
    activeTargetLatLng = [lat, lon];
    sendUdpCommand('GOTO', lat, lon);
    updateDisplacementLine(activeTargetLatLng);
    if (map) map.closePopup();
}

// ฟังก์ชันยิง HTTP POST คำสั่งไปยัง Django Backend
function sendUdpCommand(cmd, lat = null, lon = null) {
    const payload = { 'command': cmd };
    if (lat !== null && lon !== null) {
        payload['latitude'] = lat;
        payload['longitude'] = lon;
    }

    fetch('/api/command/', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',                
        },
        body: JSON.stringify({
            'command': cmd,
            'latitude': lat,  
            'longitude': lon
        })
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