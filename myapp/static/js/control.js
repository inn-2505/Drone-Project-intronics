// ตัวแปรจำสถานะของแต่ละแกน (เริ่มต้นเป็น 0 คือปล่อยมือนิ่งทั้งหมด)
let isManualModeActive = false;

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

let isArmed = false;

let throttle = 0;
let yaw = 0;
let pitch = 0;
let roll = 0;

let currentDroneCommand = 'DISARM';
//ฟังก์ชันสร้างชุดข้อมูลตามฟอร์แมตมาตรฐาน
function getDroneDataFormat(customCommand = null, joystickData = null) {
    // ใช้คำสั่งที่ส่งเข้ามา หรือถ้าไม่มีให้ใช้สถานะคำสั่งล่าสุดบนหน้าเว็บ
    const activeCommand = customCommand || currentDroneCommand;

    // กำหนดค่าเริ่มต้นของจอยสติ๊ก (ถ้าไม่มีการส่งมา ให้เป็น 0)
    const lat1 = 0.0;
    const lon1 = 0.0;
    const lat2 = 0.0;
    const lon2 = 0.0;

   // 1.3 ค้นหา Slider ความสูงและความเร็วตามสเปกของหน้าเว็บ
    const altSlider = document.querySelector('input[type="range"][max="120"]');
    const speedSlider = document.querySelector('input[type="range"][max="15"]');

    // ดึงค่าเริ่มต้นจากสไลเดอร์
    let alt = altSlider ? parseFloat(altSlider.value) : 30;
    let spd = speedSlider ? parseFloat(speedSlider.value) : 5;

    // 🔥 บังคับใช้ Logic ค่า Default/ค่าสไลเดอร์ เมื่อเป็น ARM หรือ SET_PARAM
    if (activeCommand === 'ARM' || activeCommand === 'SET_PARAM') {
        alt = altSlider ? parseFloat(altSlider.value) : 30;
        spd = speedSlider ? parseFloat(speedSlider.value) : 5;
    }
    
    // 1.4 ดึงค่าปุ่มควบคุมทิศทาง (ถ้าไม่มีการขยับให้เป็น 0)
    const outThrottle = joystickData ? joystickData.throttle : throttle;
    const outYaw = joystickData ? joystickData.yaw : yaw;
    const outPitch = joystickData ? joystickData.pitch : pitch;
    const outRoll = joystickData ? joystickData.roll : roll;

    // ส่งกลับออกไปตามฟอร์แมตที่คุณต้องการเป๊ะ ๆ
    return {
        command: activeCommand,
        lat1: lat1,
        lon1: lon1,
        lat2: lat2,
        lon2: lon2,
        alt: alt,
        spd: spd,
        throttle: outThrottle,
        yaw: outYaw,
        pitch: outPitch,
        roll: outRoll
    };
}

function toggleArmDisarm() {
    const btn = document.getElementById('btn-arm-toggle');
    const icon = document.getElementById('icon-arm-toggle');
    const text = document.getElementById('text-arm-toggle');

    if (!isArmed) {
        // ---- จังหวะที่ 1: กดเพื่อ ARM ----
        currentDroneCommand = 'ARM';
        sendActionCommand(currentDroneCommand);
        isArmed = true;
        text.innerText = 'Disarm Drone'; // สลับตัวอักษร
        btn.style.background = '#ef3c1d'; // (แถม) เปลี่ยนสีปุ่มเป็นสีแดงให้ดูเตือนภัยขึ้น
        
    } else {
        // ---- จังหวะที่ 2: กดเพื่อ DISARM ----
        currentDroneCommand = 'DISARM';
        sendActionCommand(currentDroneCommand); // ส่งคำสั่งไปที่ Django backend
        
        isArmed = false;
        text.innerText = 'Arm Drone'; // สลับตัวอักษรกลับ
        btn.style.background = '#00fe048e'; // สลับสีปุ่มกลับเป็นสีฟ้า/เขียวเดิม
    }
}
function sendActionCommand(commandType) {
    const payload = getDroneDataFormat(commandType, null);

    // ยิง Fetch API ไปหา Django ตามปกติ
    fetch('/api/command/', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(response => response.json())
    .then(data => {
        // บันทึก Log ลงหน้าจอตามประเภทคำสั่ง
        if (payload.alt!== null) {
            addLogToHUD(`✅ Sent: ${commandType} (Target Alt: ${payload.alt}m, Spd: ${payload.spd}m/s)`);
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
    const payload = getDroneDataFormat(null, {
        throttle: throttle,
        yaw: yaw,
        pitch: pitch,
        roll: roll
    });
    // อัปเดตตัวหนังสือสถานะบนหน้าจอให้เราเห็น
    const logContainer = document.getElementById('actionLog');
    if (logContainer) {
        logContainer.innerText = `T:${payload.throttle} | Y:${payload.yaw} | P:${payload.pitch} | R:${payload.roll}`;
    }
    
    fetch('/api/manual-control/', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(payload) // 🚀 ส่งวัตถุ 4 แกนไปเลย!
    })
    .catch(error => console.error("Error sending state:", error));
}
// ตรวจจับตอนกดปุ่มคีย์บอร์ดลงค้างไว้ (KeyDown)
document.addEventListener('keydown', function(event) {
    let changed = false;
    const keyLower = event.key.toLowerCase();
    if (!isManualModeActive) {
        alert("⚠️ Manual control is disabled. Please switch to MANUAL mode first.");
        return;
    }
    switch(keyLower) {
        case 'w': throttle = 1; changed = true; break;
        case 's': throttle = -1; changed = true; break;
        case 'a': yaw = -1; changed = true; break;
        case 'd': yaw = 1; changed = true; break;
        case 'arrowup': pitch = 1; changed = true; event.preventDefault(); break;
        case 'arrowdown': pitch = -1; changed = true; event.preventDefault(); break;
        case 'arrowleft': roll = -1; changed = true; event.preventDefault(); break;
        case 'arrowright': roll = 1; changed = true; event.preventDefault(); break;
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
        case 'w': case 's': throttle = 0; changed = true; break;
        case 'a': case 'd': yaw = 0; changed = true; break;
        case 'arrowup': case 'arrowdown': pitch = 0; changed = true; break;
        case 'arrowleft': case 'arrowright': roll = 0; changed = true; break;
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