// send UDP command to Django backend 
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
        document.getElementById('lastCmdText').innerText = cmd;
    })
    .catch(error => {
        console.error('Error:', error);
        alert('Failed to send command: ' + error.message);
    });
}
    
// ============================================
// Drone Custom Icon  
// ============================================
const droneIcon = L.icon({
    iconUrl: 'https://cdn-icons-png.flaticon.com/128/4056/4056808.png', // Drone icon URL 
    iconSize: [40, 40],        
    iconAnchor: [20, 20],      
    popupAnchor: [0, -20]    
});

// set initial drone position
let currentDroneLatLng = [13.7563, 100.5018];
let activeTargetLatLng = null; 
let displacementLine = null; 
    
// ============================================
// Initialize Leaflet Map
// ============================================
const map = L.map('drone-map').setView(currentDroneLatLng, 13);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '© OpenStreetMap contributors'
}).addTo(map);

// Add a marker for the drone's initial position
const droneMarker = L.marker(currentDroneLatLng, { icon: droneIcon }).addTo(map)
    .bindPopup('🛸 Drone F722')
    .openPopup();

// ============================================
// function to recenter the map on the drone's current position
// ============================================
function recenterMap() {
    map.setView(currentDroneLatLng, map.getZoom());
}

// ============================================
// function to update the displacement line from the drone to the target
// ============================================
function updateDisplacementLine(targetLatLng) {
    // delete the previous line if it exists
    if (displacementLine) {
        map.removeLayer(displacementLine);
    }
    
    // draw a new line from the drone to the target
    displacementLine = L.polyline([currentDroneLatLng, targetLatLng], {
        color: '#00f2fe',
        weight: 3,
        dashArray: '10, 10',
        opacity: 0.8
    }).addTo(map);
}

// ============================================
// adjust the map view to fit both the drone and the target
// ============================================
const locations = [
    { name: "Bangkok", lat: 13.7563, lon: 100.5018 },
    { name: "Chiang Mai", lat: 18.7883, lon: 98.9853 },
    { name: "Phuket", lat: 7.8804, lon: 98.3923 },
    { name: "Pattaya", lat: 12.9236, lon: 100.8825 }
];
locations.forEach(loc => {
    const marker = L.marker([loc.lat, loc.lon]).addTo(map);
    
    // Add a popup with a button to send to esp32 and update the displacement line
    const popupContent = document.createElement('div');
    popupContent.style.textAlign = 'center';
    popupContent.innerHTML = `
        <b style="color: #1a1a2e;">📍 ${loc.name}</b><br>
        <button class="btn btn-blue send-coord-btn" 
                style="padding: 4px 8px; margin-top: 5px; font-size: 0.8rem; cursor: pointer;">
            Send Coordinates
        </button>
    `;
    // ผูก Event ตัวจริงเมื่อคลิกปุ่มใน Popup ของหมุดนั้นๆ
    popupContent.querySelector('.send-coord-btn').addEventListener('click', () => {
        triggerTargetCommand(loc.lat, loc.lon);
    });

    marker.bindPopup(popupContent);
});

// function to trigger the target command and update the displacement line
function triggerTargetCommand(lat, lon) {
    activeTargetLatLng = [lat, lon];
    sendUdpCommand('GOTO', lat, lon);
    updateDisplacementLine(activeTargetLatLng);
}

// ============================================
// connect to WebSocket for real-time telemetry updates
// ============================================
const socketUrl = 'ws://' + window.location.host + '/ws/drone/control/';
const droneSocket = new WebSocket(socketUrl);

droneSocket.onmessage = function(e) {
    const msg = JSON.parse(e.data);
    
    if (msg.type === 'TELEMETRY') {
        const data = msg.data;
        
        // update the drone's current position on the map
        const lat = parseFloat(data.latitude);
        const lon = parseFloat(data.longitude);
        
        if (!isNaN(lat) && !isNaN(lon)) {
            currentDroneLatLng = [lat, lon];
            droneMarker.setLatLng(currentDroneLatLng); // move the drone marker to the new position
            
            // update the displacement line if a target is active
            if (activeTargetLatLng) {
                updateDisplacementLine(activeTargetLatLng);
            }
        }
        
        // update the telemetry log table with the new data
        updateLogTable(data);
    }
};

function updateLogTable(data) {
    const tbody = document.querySelector('table tbody');
    
    const tr = document.createElement('tr');
    tr.innerHTML = `
        <td>${data.timestamp}</td>
        <td><span class="badge ${data.flight_mode}">${data.flight_mode}</span></td>
        <td style="color: #00f2fe; font-weight: bold;">${data.altitude} m</td>
        <td>${data.speed} m/s</td>
        <td>${data.latitude}, ${data.longitude}</td>
        <td>${data.battery_percentage}% (${data.battery_voltage}V)</td>
    `;
    
    // insert the new row at the top of the table
    tbody.insertBefore(tr, tbody.firstChild);
    
    // delete the last row if there are more than 10 rows in the table
    while (tbody.rows.length > 10) {
        tbody.deleteRow(tbody.rows.length - 1);
    }
}