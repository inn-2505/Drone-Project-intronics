// ตัวแปรจำสถานะของแต่ละแกน (เริ่มต้นเป็น 0 คือปล่อยมือนิ่งทั้งหมด)
let isManualModeActive = false;

const keyBtnMap = {
    // 'w': 'btn-th-up',
    // 's': 'btn-th-down',
    'a': 'btn-yaw-left',
    'd': 'btn-yaw-right',
    'arrowup': 'btn-pitch-fwd',
    'arrowdown': 'btn-pitch-bak',
    'arrowleft': 'btn-roll-lft',
    'arrowright': 'btn-roll-rgt'
};

let isArmed = false;

let throttle = 1500;
let yaw = 1500;
let pitch = 1500;
let roll = 1500;

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
            addLogToHUD(`✅ Sent: ${commandType} `);
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

// ตัวแปรเก็บปุ่มคีย์บอร์ดที่ถูกกดค้างไว้
let keysPressed = {};
// ทำงานเมื่อโครงสร้างเว็บพร้อมใช้งาน (ผูกสไลเดอร์หน้าจอเข้ากับค่าตัวแปร)
document.addEventListener('DOMContentLoaded', function() {
    const throttleSlider = document.getElementById('throttleSlider');
    const throttleVal = document.getElementById('throttleVal');
    const throttleInput = document.getElementById('throttleInput');

    if (throttleSlider && throttleInput) {
        // กรณีเอามือลากสไลเดอร์บนหน้าจอตรงๆ
        throttleSlider.addEventListener('input', function() {
            throttle = parseInt(this.value);
            throttleInput.value = throttle;
            if (throttleInput) throttleInput.value = throttle;
            sendDroneStateToDjango();
        });

        // กรณีใส่ค่าด้วยมือใน input field
        throttleInput.addEventListener('change', function() {
            let val = parseInt(this.value);
            
            // ป้องกันกรอกค่าแปลกปลอม หรือค่านอกเหนือลิมิต 1300 - 1700
            if (isNaN(val)) val = 1500;
            if (val < 1300) val = 1300;
            if (val > 1700) val = 1700;
            
            throttle = val;
            this.value = throttle; // แสดงค่าที่จัดระเบียบใหม่ในช่องป้อน
            throttleSlider.value = throttle; // เลื่อนสไลเดอร์ตามจริง
            if (throttleVal) throttleVal.innerText = throttle;
            
            sendDroneStateToDjango();
        });
    }
});

// KeyDown 
document.addEventListener('keydown', function(event) {
    if (!isManualModeActive) {
        return; // ปิดการทำงานหากไม่ได้อยู่ในโหมด MANUAL
    }
    const keyLower = event.key.toLowerCase();
    keysPressed[keyLower] = true; // บันทึกว่าปุ่มนี้ถูกกดค้างไว้
    let changed = false;
    // สำหรับปุ่มทิศทางและปุ่มเลี้ยว (กดปุ๊บให้ไปสุดแกน 1300 หรือ 1700 ทันที)
    if (keyLower === 'a') { yaw = 1300; changed = true; }
    if (keyLower === 'd') { yaw = 1700; changed = true; }
    if (keyLower === 'arrowup') { pitch = 1700; changed = true; event.preventDefault(); }
    if (keyLower === 'arrowdown') { pitch = 1300; changed = true; event.preventDefault(); }
    if (keyLower === 'arrowleft') { roll = 1300; changed = true; event.preventDefault(); }
    if (keyLower === 'arrowright') { roll = 1700; changed = true; event.preventDefault(); }
    // เรืองแสงปุ่มควบคุมจำลองบนหน้าจอ
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

// KeyUp
document.addEventListener('keyup', function(event) {
    const keyLower = event.key.toLowerCase();
    delete keysPressed[keyLower]; // ลบปุ่มนี้ออกจากรายการกดค้าง
    let changed = false;
    // เมื่อปล่อยปุ่มให้สัญญาณเลี้ยวและปุ่มทิศทางดีดกลับมาตรงกลาง (1500)
    // (สังเกตว่าจะไม่มี 'w' หรือ 's' ในส่วนนี้ เพื่อปล่อยแล้วให้คันเร่งค้างไว้ที่เดิม)
    if (keyLower === 'a' || keyLower === 'd') { yaw = 1500; changed = true; }
    if (keyLower === 'arrowup' || keyLower === 'arrowdown') { pitch = 1500; changed = true; }
    if (keyLower === 'arrowleft' || keyLower === 'arrowright') { roll = 1500; changed = true; }
    // ดับแสงปุ่มควบคุมจำลองบนหน้าจอ
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

// Loop every 30ms to check if 'w' or 's' is pressed for throttle control
setInterval(function() {
    if (!isManualModeActive) return;
    let throttleChanged = false;
    
    // ขยับทีละ 5 หน่วย ทุก ๆ 30ms
    // ทำให้การเลื่อนจาก 1500 ไปหา 1700 ใช้เวลาประมาณ 1.2 วินาที
    const throttleStep = 5; 
    if (keysPressed['w']) {
        // เพิ่มคันเร่งทีละ 5 จนชนขอบบนที่ 1700
        throttle = Math.min(1700, throttle + throttleStep);
        throttleChanged = true;
    } else if (keysPressed['s']) {
        // ลดคันเร่งทีละ 5 จนชนขอบล่างที่ 1300
        throttle = Math.max(1300, throttle - throttleStep);
        throttleChanged = true;
    }
    if (throttleChanged) {
        // อัปเดตสไลเดอร์และตัวเลขบนหน้าจอให้ขยับเลื่อนตามแบบเรียลไทม์
        const throttleSlider = document.getElementById('throttleSlider');
        const throttleVal = document.getElementById('throttleVal');
        const throttleInput = document.getElementById('throttleInput');
        if (throttleSlider) throttleSlider.value = throttle;
        if (throttleInput) throttleInput.value = throttle;
        if (throttleVal) throttleVal.innerText = throttle;
        // ยิงคำสั่ง UDP ใหม่ไปบอก Django
        sendDroneStateToDjango();
    }
}, 30);

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