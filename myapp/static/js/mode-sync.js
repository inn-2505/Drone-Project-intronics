// หน้าที่:
// 1) เปิด WebSocket เดียวสำหรับหน้านั้นๆ (ใช้ร่วมกันทั้ง telemetry + mode sync
//    เพื่อไม่ให้แต่ละไฟล์เปิด socket ซ้ำกันเอง)
// 2) ควบคุมปุ่ม "Current Mode" ให้สถานะตรงกันทุกหน้าเสมอ ผ่านการถามค่าจาก
//    server ตอนโหลดหน้า + รับ broadcast ทุกครั้งที่มีการเปลี่ยนโหมด
// 3) กระจายข้อความ WebSocket ทุกชนิดต่อให้ไฟล์เฉพาะหน้า (เช่น dashboard.js)
//    ผ่าน CustomEvent 'droneSocketMessage' และแจ้งการเปลี่ยนโหมดผ่าน
//    CustomEvent 'flightModeChanged' (เช่น ให้ control.js รู้ว่าตอนนี้
//    manual control ใช้ได้ไหม)
// ============================================================
 
// ค่าที่ "รู้ล่าสุด" จาก server ไม่ใช่ตัวตัดสินใจ (source of truth คือ Django cache)
let currentFlightMode = 'AUTO_GPS';
 
// เปิด WebSocket เดียวต่อหน้า
const droneSocket = new WebSocket('ws://' + window.location.host + '/ws/drone/control/');
 
droneSocket.onmessage = function (e) {
    const msg = JSON.parse(e.data);
 
    if (msg.type === 'MODE_CHANGE') {
        updateModeUI(msg.data.mode === 'MANUAL');
    }
 
    // ส่งต่อทุกข้อความให้ไฟล์อื่นในหน้าเดียวกันฟัง (เช่น dashboard.js ฟัง TELEMETRY)
    // เพื่อไม่ให้แต่ละไฟล์ต้องเปิด socket ของตัวเอง (ซึ่งเป็นสาเหตุที่เคยพังมาก่อน)
    document.dispatchEvent(new CustomEvent('droneSocketMessage', { detail: msg }));
};
 
droneSocket.onclose = function () {
    console.warn('⚠️ WebSocket connection closed');
};
 
document.addEventListener('DOMContentLoaded', function () {
    const modeBtn = document.getElementById('btn-mode-toggle');
    if (modeBtn) {
        modeBtn.addEventListener('click', handleModeSwitch);
    }
 
    // ดึงสถานะปัจจุบันจาก server ทันทีที่โหลดหน้า ไม่ว่าจะเปิดหน้าไหนก่อน
    fetch('/api/switch-mode/')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                updateModeUI(data.current_mode === 'MANUAL');
            }
        })
        .catch(err => console.error('Error fetching current mode:', err));
});
 
// กดปุ่ม -> แค่ "ขอ" ให้ server สลับโหมด ไม่ไปแตะ UI ตรงๆ
// UI จะอัปเดตจริงตอนได้รับ broadcast กลับมาทาง WebSocket (onmessage ด้านบน)
function handleModeSwitch() {
    const targetMode = currentFlightMode !== 'MANUAL'; // สลับจากสถานะล่าสุดที่รู้จาก server
    sendModeChangeToBackend(targetMode);
}
 
// อัปเดตหน้าตาปุ่ม + แจ้งไฟล์อื่นในหน้าเดียวกันว่าโหมดเปลี่ยนแล้ว
// เรียกได้ทั้งตอนโหลดหน้า (fetch) และตอนรับ broadcast จาก WebSocket
function updateModeUI(isManual) {
    currentFlightMode = isManual ? 'MANUAL' : 'AUTO_GPS';
 
    const modeBtn = document.getElementById('btn-mode-toggle');
    if (modeBtn) {
        if (isManual) {
            modeBtn.innerText = '🟢 Current Mode: MANUAL';
            modeBtn.style.color = '#10b981';
            modeBtn.style.borderColor = '#10b981';
            modeBtn.style.backgroundColor = 'transparent';
            modeBtn.style.boxShadow = '0 0 12px rgba(16, 185, 129, 0.5)';
        } else {
            modeBtn.innerText = '🔴 Current Mode: AUTO (GPS)';
            modeBtn.style.color = '#ef4444';
            modeBtn.style.borderColor = '#ef4444';
            modeBtn.style.backgroundColor = 'transparent';
            modeBtn.style.boxShadow = '0 0 5px rgba(239, 68, 68, 0.2)';
        }
    }
 
    // 📣 แจ้งไฟล์อื่นในหน้าเดียวกัน (เช่น control.js ที่ต้องเช็คว่ารับคีย์บอร์ดได้ไหม)
    document.dispatchEvent(new CustomEvent('flightModeChanged', { detail: { isManual } }));
}
 
// ฟังก์ชันยิงสื่อสารกับ Django Cache
function sendModeChangeToBackend(activate) {
    const targetMode = activate ? 'MANUAL' : 'AUTO_GPS';
    fetch('/api/switch-mode/', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: targetMode })
    })
    .then(response => response.json())
    .then(data => {
        console.log('Server cache status updated:', data);
        // ไม่ต้องอัปเดต UI ตรงนี้ เพราะ backend จะ broadcast กลับมาทาง
        // WebSocket (msg.type === 'MODE_CHANGE') ซึ่งไปเรียก updateModeUI() ให้เองอัตโนมัติ
    })
    .catch(err => console.error('Error updating mode:', err));
}
 