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

// Initialize Leaflet map
const map = L.map('drone-map').setView([13.7563, 100.5018], 13);

// Add OpenStreetMap tile layer
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
}).addTo(map);

// Add a marker for the drone's initial position
const droneMarker = L.marker([13.7563, 100.5018]).addTo(map)
    .bindPopup('🛸 Drone F722')
    .openPopup();

const locations = [
    { name: "ฺBangkok", lat: 13.7563, lon: 100.5018 },
    { name: "Chiang Mai", lat: 18.7883, lon: 98.9853 },
    { name: "Phuket", lat: 7.8804, lon: 98.3923 },
    { name: "Pattaya", lat: 12.9236, lon: 100.8825 }
];

locations.forEach(loc => {
    const marker = L.marker([loc.lat, loc.lon]).addTo(map);

    // Add a popup with a button to send the GOTO command
    marker.bindPopup(`
        <div style="text-align: center;">
            <b style="color: #1a1a2e;">📍 ${loc.name}</b><br>
            <button onclick="sendUdpCommand('GOTO', ${loc.lat}, ${loc.lon})" 
                    class="btn btn-blue" 
                    style="padding: 4px 8px; margin-top: 5px; font-size: 0.8rem; cursor: pointer;">
                Send Coordinates to Drone
            </button>
        </div>
        `);
    });