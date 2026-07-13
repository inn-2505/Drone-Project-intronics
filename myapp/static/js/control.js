// ตัวแปรจำสถานะของแต่ละแกน (เริ่มต้นเป็น 0 คือปล่อยมือนิ่งทั้งหมด)
let isManualModeActive = false;

let droneState = {
    throttle: 0,
    yaw: 0,
    pitch: 0,
    roll: 0
};

const keyBtnMap = {
    'w': 'btn-th-up',
    's': 'btn-th-down',
    'a': 'btn-yaw-left',
    'd': 'btn-yaw-right',
    'arrowup': 'btn-pitch-fwd',
    'arrowdown': 'btn-pitch-bak',
    'arrowleft': 'btn-roll-lft',
    'arrowright': 'btn-roll-rgt'
};

function sendActionCommand(commandType) {
    let altValue = null;
    let speedValue = null;

    // 🌟 เช็กเงื่อนไข: ถ้าเป็นคำสั่ง TAKEOFF (หรือคำสั่งที่ต้องการระบุค่า) ค่อยไปดึงค่าจากสไลเดอร์
    if (commandType === 'TAKEOFF' || commandType === 'SET_PARAM') {
        const altSlider = document.querySelector('input[type="range"][max="120"]');
        const speedSlider = document.querySelector('input[type="range"][max="15"]');
        
        altValue = altSlider ? parseFloat(altSlider.value) : 30; // ถ้าหาบาร์ไม่เจอ ให้ Default ที่ 30 เมตร
        speedValue = speedSlider ? parseFloat(speedSlider.value) : 5;  // ถ้าหาบาร์ไม่เจอ ให้ Default ที่ 5 m/s
    }
    // 🌟 ถ้าเป็นคำสั่ง LAND, RTH, EMERGENCY ค่า altValue กับ speedValue จะถูกปล่อยให้เป็น null โดยอัตโนมัติ ป้องกันโดรนเอ๋อ

    // แพ็กข้อมูลส่งใน Format มาตรฐาน
    const payload = {
        'command': commandType,
        'latitude': null,
        'longitude': null,
        'altitude': altValue,
        'speed': speedValue
    };

    // ยิง Fetch API ไปหา Django ตามปกติ
    fetch('/api/command/', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(response => response.json())
    .then(data => {
        // บันทึก Log ลงหน้าจอตามประเภทคำสั่ง
        if (altValue !== null) {
            addLogToHUD(`✅ Sent: ${commandType} (Target Alt: ${altValue}m, Spd: ${speedValue}m/s)`);
        } else {
            addLogToHUD(`✅ Sent: ${commandType} (Direct Action)`);
        }
    })
    .catch(error => {
        addLogToHUD(`❌ Error: Failed to send ${commandType}`, true);
    });
}
// 📡 mode-sync.js ยิง event นี้ทุกครั้งที่โหมดเปลี่ยน ไม่ว่าจะกดปุ่มจากหน้าไหน
// ใช้ค่านี้แทนการเซ็ตเอง เพื่อไม่ให้สถานะเพี้ยนไปจากของจริงบน server
document.addEventListener('flightModeChanged', function (e) {
    isManualModeActive = e.detail.isManual;
});

// ฟังก์ชันสำหรับปุ่มลูกศรบนหน้าจอ (ตัว dpad ในหน้า control.html เรียกอันนี้)
// ⚠️ หมายเหตุ: ตอนนี้ยังไม่ได้ map ค่า cmd (เช่น 'THROTTLE_UP') ไปเป็นการ
// เปลี่ยนค่า droneState จริงๆ — แค่ log ไว้ แล้วยิงค่า droneState ปัจจุบัน
// (เหมือนโค้ดต้นฉบับก่อนหน้านี้) ถ้าต้องการให้ปุ่มเหล่านี้ขยับโดรนได้จริง
// บอกได้เลย จะช่วยเพิ่ม mousedown/mouseup ให้เหมือนคีย์บอร์ด
function sendDirectionCommand(cmd) {
    if (!isManualModeActive) {
        alert("⚠️ Manual control is disabled. Please switch to MANUAL mode first.");
        return;
    }
    console.log("📍 On-screen command:", cmd);
    sendDroneStateToDjango();
}

// ฟังก์ชันสำหรับยิงข้อมูล 4 แกนไปหา Django
function sendDroneStateToDjango() {
    // อัปเดตตัวหนังสือสถานะบนหน้าจอให้เราเห็น
    const logContainer = document.getElementById('actionLog');
    if (!isManualModeActive) {
        alert("⚠️ Manual control is disabled. Please switch to MANUAL mode first.");
        return;
    }
    if (logContainer) {
        logContainer.innerText = `T:${droneState.throttle} | Y:${droneState.yaw} | P:${droneState.pitch} | R:${droneState.roll}`;
    }

    fetch('/api/manual-control/', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(droneState) // 🚀 ส่งวัตถุ 4 แกนไปเลย!
    })
    .catch(error => console.error("Error sending state:", error));
}
// ตรวจจับตอนกดปุ่มคีย์บอร์ดลงค้างไว้ (KeyDown)
document.addEventListener('keydown', function(event) {
    let changed = false;
    const keyLower = event.key.toLowerCase();
    if (!isManualModeActive) {
        return;
    }
    switch(keyLower) {
        case 'w': droneState.throttle = 1; changed = true; break;
        case 's': droneState.throttle = -1; changed = true; break;
        case 'a': droneState.yaw = -1; changed = true; break;
        case 'd': droneState.yaw = 1; changed = true; break;
        case 'arrowup': droneState.pitch = 1; changed = true; event.preventDefault(); break;
        case 'arrowdown': droneState.pitch = -1; changed = true; event.preventDefault(); break;
        case 'arrowleft': droneState.roll = -1; changed = true; event.preventDefault(); break;
        case 'arrowright': droneState.roll = 1; changed = true; event.preventDefault(); break;
    }
    const btnId = keyBtnMap[keyLower];
    if (btnId) {
        const targetBtn = document.getElementById(btnId);
        if (targetBtn) {
            targetBtn.style.background = '#00f2fe';
            targetBtn.style.color = '#0f172a';
            targetBtn.style.boxShadow = '0 0 15px rgba(0, 242, 254, 0.8)';
        }
    }
    if (changed) sendDroneStateToDjango();
});

// ตรวจจับตอนปล่อยนิ้ว (KeyUp)
document.addEventListener('keyup', function(event) {
    let changed = false;

    const keyLower = event.key.toLowerCase();
    switch(keyLower) {
        // พอปล่อยปุ่มไหน แกะนั้นจะกลับมาเป็น 0 (นิ่ง) ทันที
        case 'w': case 's': droneState.throttle = 0; changed = true; break;
        case 'a': case 'd': droneState.yaw = 0; changed = true; break;
        case 'arrowup': case 'arrowdown': droneState.pitch = 0; changed = true; break;
        case 'arrowleft': case 'arrowright': droneState.roll = 0; changed = true; break;
    }

    const btnId = keyBtnMap[keyLower];
    if (btnId) {
        const targetBtn = document.getElementById(btnId);
        if (targetBtn) {
            targetBtn.style.background = '';
            targetBtn.style.color = '';
            targetBtn.style.boxShadow = '';
        }
    }

    if (changed) sendDroneStateToDjango();
});

function addLogToHUD(message, isError = false) {
    const logContainer = document.getElementById('control-logs-container');
    if (!logContainer) return;

    // ดึงเวลาปัจจุบันมาแสดง (เช่น 11:45:30)
    const now = new Date();
    const timeString = now.toTimeString().split(' ')[0]; 

    // สร้างข้อความบรรทัดใหม่
    const logLine = document.createElement('div');
    logLine.style.color = isError ? '#ef4444' : '#10b981'; // ตัวหนังสือแดงเมื่อเอ๋อ / เขียวเมื่อปกติ
    logLine.innerHTML = `[${timeString}] ${message}`;

    // ยัดบรรทัดใหม่ลงกล่อง และเลื่อนหน้าจอ Log ลงมาล่างสุดอัตโนมัติ (Auto-scroll)
    logContainer.appendChild(logLine);
    logContainer.scrollTop = logContainer.scrollHeight;
}